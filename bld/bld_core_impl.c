/* bld/bld_core_impl.c — arena, str, path, log, hash, fs implementations */
#pragma once

#include "bld_core.h"

#define XXH_INLINE_ALL
#include "xxhash.h"

/* ================================================================
 *  Arena
 * ================================================================ */

static Bld_Arena bld__global_arena;

Bld_Arena* bld_arena_get(void) {
    if (!bld__global_arena.base) {
        void* mem = mmap(NULL, BLD_ARENA_SIZE,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) {
            fprintf(stderr, "bld: fatal: mmap arena failed: %s\n", strerror(errno));
            exit(1);
        }
        bld__global_arena.base = mem;
        bld__global_arena.offset = 0;
        bld__global_arena.capacity = BLD_ARENA_SIZE;
        bld__global_arena.last_ptr = NULL;
        bld__global_arena.last_size = 0;
        pthread_mutex_init(&bld__global_arena.mutex, NULL);
    }
    return &bld__global_arena;
}

void* bld_arena_alloc(size_t size) {
    Bld_Arena* a = bld_arena_get();
    pthread_mutex_lock(&a->mutex);
    size_t aligned = (size + 7) & ~(size_t)7;
    if (a->offset + aligned > a->capacity) {
        fprintf(stderr, "bld: fatal: arena OOM (%zu used, %zu requested)\n", a->offset, size);
        exit(1);
    }
    void* ptr = a->base + a->offset;
    a->offset += aligned;
    a->last_ptr = ptr;
    a->last_size = aligned;
    pthread_mutex_unlock(&a->mutex);
    return ptr;
}

void* bld_arena_realloc(void* old_ptr, size_t old_size, size_t new_size) {
    Bld_Arena* a = bld_arena_get();
    pthread_mutex_lock(&a->mutex);
    if (old_ptr && old_ptr == a->last_ptr) {
        size_t new_aligned = (new_size + 7) & ~(size_t)7;
        if ((char*)old_ptr + new_aligned <= a->base + a->capacity) {
            a->offset = (size_t)((char*)old_ptr - a->base) + new_aligned;
            a->last_size = new_aligned;
            pthread_mutex_unlock(&a->mutex);
            return old_ptr;
        }
    }
    /* can't realloc in place — allocate new (unlocks/relocks inside) */
    pthread_mutex_unlock(&a->mutex);
    void* new_ptr = bld_arena_alloc(new_size);
    if (old_ptr && old_size > 0)
        memcpy(new_ptr, old_ptr, old_size < new_size ? old_size : new_size);
    return new_ptr;
}

/* ================================================================
 *  Str
 * ================================================================ */

const char* bld_str_fmt(const char* fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return ""; }
    char* buf = bld_arena_alloc((size_t)n + 1);
    vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

const char* bld_str_dup(const char* s) {
    if (!s) return "";
    size_t len = strlen(s);
    char* buf = bld_arena_alloc(len + 1);
    memcpy(buf, s, len + 1);
    return buf;
}

const char* bld_str_cat(const char* first, ...) {
    size_t total = 0;
    if (first) total = strlen(first);
    va_list ap;
    va_start(ap, first);
    const char* s;
    while ((s = va_arg(ap, const char*)) != NULL) total += strlen(s);
    va_end(ap);

    char* buf = bld_arena_alloc(total + 1);
    size_t off = 0;
    if (first) { size_t n = strlen(first); memcpy(buf + off, first, n); off += n; }
    va_start(ap, first);
    while ((s = va_arg(ap, const char*)) != NULL) {
        size_t n = strlen(s);
        memcpy(buf + off, s, n);
        off += n;
    }
    va_end(ap);
    buf[off] = '\0';
    return buf;
}

Bld_Strings bld_str_lines(const char* s) {
    Bld_Strings lines = {0};
    while (*s) {
        const char* nl = strchr(s, '\n');
        size_t n = nl ? (size_t)(nl - s) : strlen(s);
        char* line = bld_arena_alloc(n + 1);
        memcpy(line, s, n);
        line[n] = '\0';
        bld_da_push(&lines, (const char*)line);
        s += n + (nl ? 1 : 0);
    }
    return lines;
}

const char* bld_str_join(const Bld_Strings* parts, const char* sep) {
    if (!parts->count) return "";
    size_t sep_len = strlen(sep);
    /* compute lengths in one pass */
    size_t* lens = bld_arena_alloc(parts->count * sizeof(size_t));
    size_t total = 0;
    for (size_t i = 0; i < parts->count; i++) {
        lens[i] = strlen(parts->items[i]);
        total += lens[i] + (i > 0 ? sep_len : 0);
    }
    char* buf = bld_arena_alloc(total + 1);
    size_t off = 0;
    for (size_t i = 0; i < parts->count; i++) {
        if (i > 0) { memcpy(buf + off, sep, sep_len); off += sep_len; }
        memcpy(buf + off, parts->items[i], lens[i]);
        off += lens[i];
    }
    buf[off] = '\0';
    return buf;
}

/* ================================================================
 *  Path
 * ================================================================ */

#define BLD_SEP '/'
#define BLD_SEP_STR "/"

Bld_Path bld_path_join(Bld_Path a, Bld_Path b) {
    if (!a.s || !a.s[0]) return (Bld_Path){bld_str_dup(b.s)};
    if (!b.s || !b.s[0]) return (Bld_Path){bld_str_dup(a.s)};
    size_t alen = strlen(a.s);
    if (a.s[alen - 1] == BLD_SEP)
        return (Bld_Path){bld_str_cat(a.s, b.s, NULL)};
    return (Bld_Path){bld_str_cat(a.s, BLD_SEP_STR, b.s, NULL)};
}

const char* bld_path_filename(Bld_Path p) {
    const char* last = strrchr(p.s, BLD_SEP);
    return last ? last + 1 : p.s;
}

Bld_Path bld_path_parent(Bld_Path p) {
    const char* last = strrchr(p.s, BLD_SEP);
    if (!last) return (Bld_Path){"."};
    if (last == p.s) return (Bld_Path){"/"};
    size_t len = (size_t)(last - p.s);
    char* buf = bld_arena_alloc(len + 1);
    memcpy(buf, p.s, len);
    buf[len] = '\0';
    return (Bld_Path){buf};
}

const char* bld_path_ext(Bld_Path p) {
    const char* fn = bld_path_filename(p);
    const char* dot = strrchr(fn, '.');
    return dot ? dot : "";
}

Bld_Path bld_path_replace_ext(Bld_Path p, const char* ext) {
    const char* fn = bld_path_filename(p);
    const char* dot = strrchr(fn, '.');
    size_t base_len = dot ? (size_t)(dot - p.s) : strlen(p.s);
    size_t ext_len = strlen(ext);
    char* buf = bld_arena_alloc(base_len + ext_len + 1);
    memcpy(buf, p.s, base_len);
    memcpy(buf + base_len, ext, ext_len + 1);
    return (Bld_Path){buf};
}

Bld_Path bld_path_fmt(const char* fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return (Bld_Path){""}; }
    char* buf = bld_arena_alloc((size_t)n + 1);
    vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return (Bld_Path){buf};
}

/* ================================================================
 *  Log
 * ================================================================ */

static pthread_mutex_t bld__log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int bld__color = -1;

static void bld__init_color(void) {
    bld__color = isatty(STDOUT_FILENO);
}

#define BLD_C_RESET  "\033[0m"
#define BLD_C_BOLD   "\033[1m"
#define BLD_C_DIM    "\033[2m"
#define BLD_C_GREEN  "\033[32m"
#define BLD_C_RED    "\033[31m"
#define BLD_C_YELLOW "\033[33m"

/* returns code if color enabled, empty string otherwise */
static const char* bld__c(const char* code) {
    if (bld__color < 0) bld__init_color();
    return bld__color ? code : "";
}

void bld_log(const char* fmt, ...) {
    pthread_mutex_lock(&bld__log_mutex);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fflush(stdout);
    pthread_mutex_unlock(&bld__log_mutex);
}

void bld_panic(const char* fmt, ...) {
    pthread_mutex_lock(&bld__log_mutex);
    fprintf(stderr, "%sbld: error:%s ", bld__c(BLD_C_RED), bld__c(BLD_C_RESET));
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
    pthread_mutex_unlock(&bld__log_mutex);
    exit(1);
}

void bld_log_progress(uint64_t current, uint64_t total, const char* name) {
    pthread_mutex_lock(&bld__log_mutex);
    fprintf(stdout, "%s[%" PRIu64 "/%" PRIu64 "]%s %s\n",
            bld__c(BLD_C_GREEN), current, total, bld__c(BLD_C_RESET), name);
    fflush(stdout);
    pthread_mutex_unlock(&bld__log_mutex);
}

void bld_log_cached(const char* name) {
    pthread_mutex_lock(&bld__log_mutex);
    fprintf(stdout, "%s[cached]%s %s\n", bld__c(BLD_C_DIM), bld__c(BLD_C_RESET), name);
    fflush(stdout);
    pthread_mutex_unlock(&bld__log_mutex);
}

static const char* bld__fmt_size(size_t bytes) {
    if (bytes < 1024) return bld_str_fmt("%zuB", bytes);
    if (bytes < 1024 * 1024) return bld_str_fmt("%.1fKB", (double)bytes / 1024);
    return bld_str_fmt("%.1fMB", (double)bytes / (1024 * 1024));
}

void bld_log_done(uint64_t executed, uint64_t cached, uint64_t failed, uint64_t skipped, double elapsed, size_t arena_used) {
    pthread_mutex_lock(&bld__log_mutex);
    const char* mem = bld__fmt_size(arena_used);
    const char* color = failed > 0 ? bld__c(BLD_C_RED) : bld__c(BLD_C_GREEN);
    const char* label = failed > 0 ? "bld failed:" : "bld done:";
    fprintf(stdout, "%s%s%s", color, label, bld__c(BLD_C_RESET));
    if (executed > 0) fprintf(stdout, " %" PRIu64 " built,", executed);
    if (cached > 0)   fprintf(stdout, " %" PRIu64 " cached,", cached);
    if (failed > 0)   fprintf(stdout, " %s%" PRIu64 " failed%s,", bld__c(BLD_C_RED), failed, bld__c(BLD_C_RESET));
    if (skipped > 0)  fprintf(stdout, " %" PRIu64 " skipped,", skipped);
    fprintf(stdout, " %.2fs, %s arena\n", elapsed, mem);
    fflush(stdout);
    pthread_mutex_unlock(&bld__log_mutex);
}

void bld_log_action(const char* fmt, ...) {
    pthread_mutex_lock(&bld__log_mutex);
    fprintf(stdout, "%s", bld__c(BLD_C_DIM));
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fprintf(stdout, "%s", bld__c(BLD_C_RESET));
    fflush(stdout);
    pthread_mutex_unlock(&bld__log_mutex);
}

void bld_log_info(const char* fmt, ...) {
    pthread_mutex_lock(&bld__log_mutex);
    fprintf(stdout, "%s", bld__c(BLD_C_YELLOW));
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fprintf(stdout, "%s", bld__c(BLD_C_RESET));
    fflush(stdout);
    pthread_mutex_unlock(&bld__log_mutex);
}

/* ================================================================
 *  Hash
 * ================================================================ */

Bld_Hash bld_hash_combine(Bld_Hash a, Bld_Hash b) {
    uint64_t buf[2] = {a.value, b.value};
    return (Bld_Hash){XXH3_64bits(buf, sizeof(buf))};
}

Bld_Hash bld_hash_combine_unordered(Bld_Hash a, Bld_Hash b) {
    return (Bld_Hash){a.value + b.value};
}

Bld_Hash bld_hash_str(const char* s) {
    return (Bld_Hash){XXH3_64bits(s, strlen(s))};
}

Bld_Hash bld_hash_file(Bld_Path p) {
    size_t len;
    const char* data = bld_fs_read_file(p, &len);
    return (Bld_Hash){XXH3_64bits(data, len)};
}

Bld_Hash bld_hash_dir(Bld_Path dir) {
    Bld_PathList files = bld_fs_list_files_r(dir);
    Bld_Hash h = {0};
    size_t dir_len = strlen(dir.s);
    for (size_t i = 0; i < files.count; i++) {
        const char* full = files.items[i].s;
        const char* rel = full;
        if (strncmp(rel, dir.s, dir_len) == 0 && rel[dir_len] == '/')
            rel += dir_len + 1;
        h = bld_hash_combine_unordered(h,
            bld_hash_combine(bld_hash_str(rel), bld_hash_file(files.items[i])));
    }
    return h;
}

/* ================================================================
 *  Filesystem
 * ================================================================ */

int bld_fs_exists(Bld_Path p) {
    struct stat st;
    return stat(p.s, &st) == 0;
}

int bld_fs_is_dir(Bld_Path p) {
    struct stat st;
    return stat(p.s, &st) == 0 && S_ISDIR(st.st_mode);
}

int bld_fs_is_file(Bld_Path p) {
    struct stat st;
    return stat(p.s, &st) == 0 && S_ISREG(st.st_mode);
}

void bld_fs_mkdir_p(Bld_Path p) {
    if (!p.s || !p.s[0]) return;
    size_t len = strlen(p.s);
    char* tmp = bld_arena_alloc(len + 1);
    memcpy(tmp, p.s, len + 1);
    for (char* s = tmp + 1; *s; s++) {
        if (*s == '/') {
            *s = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                bld_panic("mkdir_p: failed to create %s: %s\n", tmp, strerror(errno));
            *s = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        bld_panic("mkdir_p: failed to create %s: %s\n", p.s, strerror(errno));
}

void bld_fs_remove(Bld_Path p) {
    if (remove(p.s) != 0 && errno != ENOENT)
        bld_panic("remove: %s: %s\n", p.s, strerror(errno));
}

static void bld__fs_remove_all_impl(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return;
    if (!S_ISDIR(st.st_mode)) { remove(path); return; }
    DIR* d = opendir(path);
    if (!d) bld_panic("remove_all: opendir %s: %s\n", path, strerror(errno));
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        bld__fs_remove_all_impl(bld_str_fmt("%s/%s", path, ent->d_name));
    }
    closedir(d);
    if (rmdir(path) != 0)
        bld_panic("remove_all: rmdir %s: %s\n", path, strerror(errno));
}

void bld_fs_remove_all(Bld_Path p) { bld__fs_remove_all_impl(p.s); }

void bld_fs_rename(Bld_Path from, Bld_Path to) {
    if (rename(from.s, to.s) == 0) return;
    if (errno == EXDEV) {
        /* cross-filesystem: copy + delete */
        struct stat st;
        if (stat(from.s, &st) != 0)
            bld_panic("rename (cross-fs) stat %s: %s\n", from.s, strerror(errno));
        if (S_ISDIR(st.st_mode)) bld_fs_copy_r(from, to);
        else bld_fs_copy_file(from, to);
        bld__fs_remove_all_impl(from.s);
        return;
    }
    bld_panic("rename %s -> %s: %s\n", from.s, to.s, strerror(errno));
}

void bld_fs_copy_file(Bld_Path from, Bld_Path to) {
    FILE* fin = fopen(from.s, "rb");
    if (!fin) bld_panic("copy_file: open %s: %s\n", from.s, strerror(errno));
    FILE* fout = fopen(to.s, "wb");
    if (!fout) { fclose(fin); bld_panic("copy_file: open %s: %s\n", to.s, strerror(errno)); }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fin)) > 0) {
        if (fwrite(buf, 1, n, fout) != n) {
            fclose(fin); fclose(fout);
            bld_panic("copy_file: write %s: %s\n", to.s, strerror(errno));
        }
    }
    if (ferror(fin)) { fclose(fin); fclose(fout); bld_panic("copy_file: read %s: %s\n", from.s, strerror(errno)); }
    fclose(fin);
    if (fclose(fout) != 0) bld_panic("copy_file: close %s: %s\n", to.s, strerror(errno));
    /* preserve permissions */
    struct stat st;
    if (stat(from.s, &st) == 0) chmod(to.s, st.st_mode);
}

static void bld__fs_copy_r_impl(const char* from, const char* to) {
    struct stat st;
    if (stat(from, &st) != 0) bld_panic("copy_r: stat %s: %s\n", from, strerror(errno));
    if (S_ISREG(st.st_mode)) { bld_fs_copy_file((Bld_Path){from}, (Bld_Path){to}); return; }
    if (!S_ISDIR(st.st_mode)) return;
    bld_fs_mkdir_p((Bld_Path){to});
    DIR* d = opendir(from);
    if (!d) bld_panic("copy_r: opendir %s: %s\n", from, strerror(errno));
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        bld__fs_copy_r_impl(bld_str_fmt("%s/%s", from, ent->d_name),
                            bld_str_fmt("%s/%s", to, ent->d_name));
    }
    closedir(d);
}

void bld_fs_copy_r(Bld_Path from, Bld_Path to) { bld__fs_copy_r_impl(from.s, to.s); }

Bld_Path bld_fs_realpath(Bld_Path p) {
    char* resolved = realpath(p.s, NULL);
    if (!resolved) bld_panic("realpath %s: %s\n", p.s, strerror(errno));
    const char* dup = bld_str_dup(resolved);
    free(resolved);
    return (Bld_Path){dup};
}

Bld_Path bld_fs_getcwd(void) {
    char* cwd = getcwd(NULL, 0);
    if (!cwd) bld_panic("getcwd: %s\n", strerror(errno));
    const char* dup = bld_str_dup(cwd);
    free(cwd);
    return (Bld_Path){dup};
}

static void bld__fs_list_files_r_impl(const char* dir, Bld_PathList* out) {
    DIR* d = opendir(dir);
    if (!d) bld_panic("list_files_r: opendir %s: %s\n", dir, strerror(errno));
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        const char* full = bld_str_fmt("%s/%s", dir, ent->d_name);
        Bld_Path fp = {full};
        if (bld_fs_is_dir(fp)) bld__fs_list_files_r_impl(full, out);
        else if (bld_fs_is_file(fp)) bld_da_push(out, fp);
    }
    closedir(d);
}

Bld_PathList bld_fs_list_files_r(Bld_Path dir) {
    Bld_PathList list = {0};
    bld__fs_list_files_r_impl(dir.s, &list);
    return list;
}

const char* bld_fs_read_file(Bld_Path p, size_t* out_len) {
    FILE* f = fopen(p.s, "rb");
    if (!f) bld_panic("read_file: open %s: %s\n", p.s, strerror(errno));
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); bld_panic("read_file: seek %s: %s\n", p.s, strerror(errno)); }
    long size = ftell(f);
    if (size < 0) { fclose(f); bld_panic("read_file: ftell %s: %s\n", p.s, strerror(errno)); }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); bld_panic("read_file: seek %s: %s\n", p.s, strerror(errno)); }
    char* buf = bld_arena_alloc((size_t)size + 1);
    size_t rd = fread(buf, 1, (size_t)size, f);
    if (rd != (size_t)size) { fclose(f); bld_panic("read_file: %s: short read\n", p.s); }
    fclose(f);
    buf[size] = '\0';
    if (out_len) *out_len = (size_t)size;
    return buf;
}

void bld_fs_write_file(Bld_Path p, const char* data, size_t len) {
    FILE* f = fopen(p.s, "wb");
    if (!f) bld_panic("write_file: open %s: %s\n", p.s, strerror(errno));
    if (fwrite(data, 1, len, f) != len) { fclose(f); bld_panic("write_file: write %s: %s\n", p.s, strerror(errno)); }
    if (fclose(f) != 0) bld_panic("write_file: close %s: %s\n", p.s, strerror(errno));
}

/* ---- Glob ---- */

static int bld__strcmp_indirect(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

const char** bld_files_glob(const char* pattern) {
    /* find first wildcard */
    const char* wild = strpbrk(pattern, "*?[");
    if (!wild) {
        /* no wildcards — return single file if it exists */
        const char** r = bld_arena_alloc(2 * sizeof(const char*));
        r[0] = bld_str_dup(pattern);
        r[1] = NULL;
        return r;
    }

    /* split into base dir + file pattern at last / before wildcard */
    const char* last_sep = NULL;
    for (const char* p = pattern; p < wild; p++)
        if (*p == '/') last_sep = p;

    const char* base_dir;
    const char* file_pat;
    if (last_sep) {
        size_t len = (size_t)(last_sep - pattern);
        char* d = bld_arena_alloc(len + 1);
        memcpy(d, pattern, len);
        d[len] = '\0';
        base_dir = d;
        file_pat = last_sep + 1;
    } else {
        base_dir = ".";
        file_pat = pattern;
    }

    int recursive = (strstr(file_pat, "**") != NULL);
    /* for recursive globs, strip leading double-star-slash */
    const char* match_pat = file_pat;
    if (recursive && strncmp(match_pat, "**/", 3) == 0)
        match_pat = match_pat + 3;

    Bld_Strings result = {0};

    if (recursive) {
        Bld_PathList files = bld_fs_list_files_r(bld_path(base_dir));
        for (size_t i = 0; i < files.count; i++) {
            const char* fname = bld_path_filename(files.items[i]);
            if (fnmatch(match_pat, fname, 0) == 0)
                bld_da_push(&result, files.items[i].s);
        }
    } else {
        DIR* d = opendir(base_dir);
        if (!d) bld_panic("files_glob: opendir %s: %s\n", base_dir, strerror(errno));
        struct dirent* ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            if (!bld_fs_is_file(bld_path(bld_str_fmt("%s/%s",
                    strcmp(base_dir, ".") == 0 ? "" : base_dir, ent->d_name)))) continue;
            if (fnmatch(match_pat, ent->d_name, 0) == 0) {
                const char* path = (strcmp(base_dir, ".") == 0)
                    ? bld_str_dup(ent->d_name)
                    : bld_str_fmt("%s/%s", base_dir, ent->d_name);
                bld_da_push(&result, path);
            }
        }
        closedir(d);
    }

    /* sort for deterministic order across runs */
    if (result.count > 1)
        qsort(result.items, result.count, sizeof(const char*), bld__strcmp_indirect);

    bld_da_push(&result, (const char*)NULL);
    return result.items;
}

const char** bld_files_exclude(const char** files, const char** exclude) {
    if (!files) return files;
    Bld_Strings result = {0};
    for (const char** f = files; *f; f++) {
        int skip = 0;
        if (exclude) {
            for (const char** e = exclude; *e; e++) {
                if (strcmp(*f, *e) == 0) { skip = 1; break; }
            }
        }
        if (!skip) bld_da_push(&result, *f);
    }
    bld_da_push(&result, (const char*)NULL);
    return result.items;
}

const char** bld_files_merge(const char** a, const char** b) {
    size_t na = 0, nb = 0;
    if (a) for (const char** p = a; *p; p++) na++;
    if (b) for (const char** p = b; *p; p++) nb++;
    const char** r = bld_arena_alloc((na + nb + 1) * sizeof(const char*));
    for (size_t i = 0; i < na; i++) r[i] = a[i];
    for (size_t i = 0; i < nb; i++) r[na + i] = b[i];
    r[na + nb] = NULL;
    return r;
}

/* ================================================================
 *  Tool detection
 * ================================================================ */

/* capture first line of "driver --version" output, return hash */
static Bld_Hash bld__compiler_version_hash(const char* driver) {
    const char* cmd = bld_str_fmt("%s --version 2>/dev/null", driver);
    FILE* f = popen(cmd, "r");
    if (!f) return (Bld_Hash){0};
    char buf[256] = {0};
    if (fgets(buf, sizeof(buf), f)) {
        /* strip trailing newline */
        size_t n = strlen(buf);
        if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
    }
    pclose(f);
    if (!buf[0]) return (Bld_Hash){0};
    return bld_hash_str(buf);
}

static Bld_Hash bld__make_identity_hash(const char* driver) {
    Bld_Hash h = bld_hash_str(driver);
    Bld_Hash ver = bld__compiler_version_hash(driver);
    if (ver.value) h = bld_hash_combine(h, ver);
    return h;
}

static int bld__has_in_path(const char* name) {
    const char* path_env = getenv("PATH");
    if (!path_env) return 0;
    const char* p = path_env;
    while (*p) {
        const char* sep = strchr(p, ':');
        size_t len = sep ? (size_t)(sep - p) : strlen(p);
        if (bld_fs_is_file(bld_path(bld_str_fmt("%.*s/%s", (int)len, p, name)))) return 1;
        p += len + (sep ? 1 : 0);
    }
    return 0;
}

