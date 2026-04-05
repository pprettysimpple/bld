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

Bld_Strs bld_str_lines(const char* s) {
    Bld_Strs lines = {0};
    while (*s) {
        const char* nl = strchr(s, '\n');
        size_t n = nl ? (size_t)(nl - s) : strlen(s);
        char* line = bld_arena_alloc(n + 1);
        memcpy(line, s, n);
        line[n] = '\0';
        bld_strs_push(&lines, (const char*)line);
        s += n + (nl ? 1 : 0);
    }
    return lines;
}

const char* bld_str_join(const Bld_Strs* parts, const char* sep) {
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

const char** bld_dup_strarray(const char** arr) {
    if (!arr) return NULL;
    size_t n = 0;
    while (arr[n]) n++;
    const char** copy = bld_arena_alloc((n + 1) * sizeof(const char*));
    for (size_t i = 0; i < n; i++) copy[i] = bld_str_dup(arr[i]);
    copy[n] = NULL;
    return copy;
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
 *  Cmd — growable string buffer
 * ================================================================ */

void bld_cmd_appendf(Bld_Cmd* cmd, const char* fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n <= 0) { va_end(ap2); return; }
    size_t needed = cmd->count + (size_t)n + 1;
    if (needed > cmd->cap) {
        size_t new_cap = cmd->cap ? cmd->cap : 64;
        while (new_cap < needed) new_cap *= 2;
        cmd->items = bld_arena_realloc(cmd->items, cmd->cap, new_cap);
        cmd->cap = new_cap;
    }
    vsnprintf(cmd->items + cmd->count, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    cmd->count += (size_t)n;
}

void bld_cmd_append_sq(Bld_Cmd* cmd, const char* s) {
    if (!strchr(s, '\'')) {
        bld_cmd_appendf(cmd, "'%s'", s);
    } else {
        bld_cmd_appendf(cmd, "'");
        for (const char* c = s; *c; c++) {
            if (*c == '\'') bld_cmd_appendf(cmd, "'\\''");
            else { char tmp[2] = {*c, 0}; bld_cmd_appendf(cmd, "%s", tmp); }
        }
        bld_cmd_appendf(cmd, "'");
    }
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
    struct stat st;
    if (stat(p.s, &st) != 0) bld_panic("hash_file: stat %s: %s\n", p.s, strerror(errno));
    size_t len = (size_t)st.st_size;
    if (len == 0) return (Bld_Hash){0};
    int fd = open(p.s, O_RDONLY);
    if (fd < 0) bld_panic("hash_file: open %s: %s\n", p.s, strerror(errno));
    if (len <= 4096) {
        char buf[4096];
        ssize_t n = read(fd, buf, len);
        close(fd);
        if (n < 0 || (size_t)n != len) bld_panic("hash_file: read %s: %s\n", p.s, strerror(errno));
        return (Bld_Hash){XXH3_64bits(buf, len)};
    }
    void* data = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { close(fd); bld_panic("hash_file: mmap %s: %s\n", p.s, strerror(errno)); }
    Bld_Hash h = {XXH3_64bits(data, len)};
    munmap(data, len);
    close(fd);
    return h;
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

bool bld_fs_exists(Bld_Path p) {
    struct stat st;
    return stat(p.s, &st) == 0;
}

bool bld_fs_is_dir(Bld_Path p) {
    struct stat st;
    return stat(p.s, &st) == 0 && S_ISDIR(st.st_mode);
}

bool bld_fs_is_file(Bld_Path p) {
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

/* ---- Bld_Strs / Bld_Paths literal constructors ---- */

Bld_Strs bld__strs_lit(const char** items, size_t len) {
    const char** copy = bld_arena_alloc(len * sizeof(const char*));
    memcpy(copy, items, len * sizeof(const char*));
    return (Bld_Strs){copy, len, len};
}

Bld_Paths bld__paths_lit(const char** items, size_t len) {
    const char** copy = bld_arena_alloc(len * sizeof(const char*));
    memcpy(copy, items, len * sizeof(const char*));
    return (Bld_Paths){copy, len, len};
}

/* ---- Bld_Strs / Bld_Paths push & merge ---- */

void bld_strs_push(Bld_Strs* s, const char* item) {
    if (s->count >= s->cap) {
        size_t new_cap = s->cap ? s->cap * 2 : 8;
        if (s->cap == 0 && s->items && s->count > 0) {
            /* first dynamic use of a static slice — copy from compound literal */
            const char** old = s->items;
            s->items = bld_arena_alloc(new_cap * sizeof(const char*));
            memcpy(s->items, old, s->count * sizeof(const char*));
        } else {
            s->items = bld_arena_realloc(
                s->items, s->cap * sizeof(const char*), new_cap * sizeof(const char*));
        }
        s->cap = new_cap;
    }
    s->items[s->count++] = item;
}

void bld_paths_push(Bld_Paths* s, const char* item) {
    if (s->count >= s->cap) {
        size_t new_cap = s->cap ? s->cap * 2 : 8;
        if (s->cap == 0 && s->items && s->count > 0) {
            const char** old = s->items;
            s->items = bld_arena_alloc(new_cap * sizeof(const char*));
            memcpy(s->items, old, s->count * sizeof(const char*));
        } else {
            s->items = bld_arena_realloc(
                s->items, s->cap * sizeof(const char*), new_cap * sizeof(const char*));
        }
        s->cap = new_cap;
    }
    s->items[s->count++] = item;
}

Bld_Strs bld_strs_merge(Bld_Strs a, Bld_Strs b) {
    size_t total = a.count + b.count;
    if (total == 0) return (Bld_Strs){0};
    const char** r = bld_arena_alloc(total * sizeof(const char*));
    for (size_t i = 0; i < a.count; i++) r[i] = a.items[i];
    for (size_t i = 0; i < b.count; i++) r[a.count + i] = b.items[i];
    return (Bld_Strs){r, total, total};
}

Bld_Paths bld_paths_merge(Bld_Paths a, Bld_Paths b) {
    size_t total = a.count + b.count;
    if (total == 0) return (Bld_Paths){0};
    const char** r = bld_arena_alloc(total * sizeof(const char*));
    for (size_t i = 0; i < a.count; i++) r[i] = a.items[i];
    for (size_t i = 0; i < b.count; i++) r[a.count + i] = b.items[i];
    return (Bld_Paths){r, total, total};
}

Bld_Paths bld_files_glob(const char* pattern) {
    /* find first wildcard */
    const char* wild = strpbrk(pattern, "*?[");
    if (!wild) {
        /* no wildcards — return single file if it exists */
        const char** r = bld_arena_alloc(1 * sizeof(const char*));
        r[0] = bld_str_dup(pattern);
        return (Bld_Paths){r, 1, 0};
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

    Bld_Paths result = {0};

    if (recursive) {
        Bld_PathList files = bld_fs_list_files_r(bld_path(base_dir));
        for (size_t i = 0; i < files.count; i++) {
            const char* fname = bld_path_filename(files.items[i]);
            if (fnmatch(match_pat, fname, 0) == 0)
                bld_paths_push(&result, files.items[i].s);
        }
    } else {
        DIR* d = opendir(base_dir);
        if (!d) bld_panic("files_glob: opendir %s: %s\n", base_dir, strerror(errno));
        struct dirent* ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            const char* path = (strcmp(base_dir, ".") == 0)
                ? bld_str_dup(ent->d_name)
                : bld_str_fmt("%s/%s", base_dir, ent->d_name);
            if (!bld_fs_is_file(bld_path(path))) continue;
            if (fnmatch(match_pat, ent->d_name, 0) == 0) {
                bld_paths_push(&result, path);
            }
        }
        closedir(d);
    }

    /* sort for deterministic order across runs */
    if (result.count > 1)
        qsort(result.items, result.count, sizeof(const char*), bld__strcmp_indirect);

    return result;
}

Bld_Paths bld_files_exclude(Bld_Paths files, Bld_Paths exclude) {
    if (!files.items || files.count == 0) return files;
    Bld_Paths result = {0};
    for (size_t i = 0; i < files.count; i++) {
        bool skip = false;
        for (size_t j = 0; j < exclude.count; j++) {
            const char* pat = exclude.items[j];
            bool is_glob = (strpbrk(pat, "*?[") != NULL);
            if (is_glob) {
                /* match pattern against basename */
                const char* base = strrchr(files.items[i], '/');
                base = base ? base + 1 : files.items[i];
                if (fnmatch(pat, base, 0) == 0) { skip = true; break; }
            } else if (strcmp(files.items[i], pat) == 0) {
                /* exact match */
                skip = true; break;
            } else {
                /* directory prefix: "src/win" matches "src/win/core.c" */
                size_t plen = strlen(pat);
                if (plen > 0 && strncmp(files.items[i], pat, plen) == 0
                    && files.items[i][plen] == '/') {
                    skip = true; break;
                }
            }
        }
        if (!skip) bld_paths_push(&result, files.items[i]);
    }
    return result;
}

Bld_Paths bld_files_merge(Bld_Paths a, Bld_Paths b) {
    return bld_paths_merge(a, b);
}

/* ================================================================
 *  Tool detection
 * ================================================================ */

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

static Bld_OsTarget bld__detect_os_from_triple(const char* triple) {
    if (strstr(triple, "darwin")) return BLD_OS_MACOS;
    if (strstr(triple, "mingw") || strstr(triple, "windows")) return BLD_OS_WINDOWS;
    if (strstr(triple, "freebsd")) return BLD_OS_FREEBSD;
    return BLD_OS_LINUX;
}

/* ================================================================
 *  Flag resolution (shared by toolchain renderers and legacy path)
 * ================================================================ */

static const char* bld__resolve_optimize(Bld_Optimize o) {
    switch (o) {
        case BLD_OPT_DEFAULT: return "";
        case BLD_OPT_O0: return "-O0"; case BLD_OPT_O1: return "-O1";
        case BLD_OPT_O2: return "-O2"; case BLD_OPT_O3: return "-O3";
        case BLD_OPT_Os: return "-Os"; case BLD_OPT_OFAST: return "-Ofast";
    }
    return "";
}

static const char* bld__resolve_c_standard(Bld_CStd s) {
    switch (s) {
        case BLD_C_DEFAULT: return "";
        case BLD_C_90: return "-std=c90";
        case BLD_C_99: return "-std=c99"; case BLD_C_11: return "-std=c11";
        case BLD_C_17: return "-std=c17"; case BLD_C_23: return "-std=c23";
        case BLD_C_GNU90: return "-std=gnu90"; case BLD_C_GNU99: return "-std=gnu99";
        case BLD_C_GNU11: return "-std=gnu11"; case BLD_C_GNU17: return "-std=gnu17";
        case BLD_C_GNU23: return "-std=gnu23";
    }
    return "";
}

static const char* bld__resolve_cxx_standard(Bld_CxxStd s) {
    switch (s) {
        case BLD_CXX_DEFAULT: return "";
        case BLD_CXX_11: return "-std=c++11"; case BLD_CXX_14: return "-std=c++14";
        case BLD_CXX_17: return "-std=c++17"; case BLD_CXX_20: return "-std=c++20";
        case BLD_CXX_23: return "-std=c++23";
        case BLD_CXX_GNU11: return "-std=gnu++11"; case BLD_CXX_GNU14: return "-std=gnu++14";
        case BLD_CXX_GNU17: return "-std=gnu++17"; case BLD_CXX_GNU20: return "-std=gnu++20";
        case BLD_CXX_GNU23: return "-std=gnu++23";
    }
    return "";
}

/* ================================================================
 *  GCC toolchain factory
 * ================================================================ */

/*
 * GCC render_compile: produces the FULL compile command including -c -o and -MMD -MF.
 *
 * Flag order:
 *   driver, -O, -std=, -Wall/-w, extra_flags, -D defines, -I compile_flags dirs,
 *   -isystem sys dirs, -g, -fsanitize=address, -flto,
 *   -fPIC, extra_cflags (target include dirs + ext_dep flags),
 *   "source" (quoted), -c -o output, -MMD -MF depfile
 */
static void bld__gcc_render_compile(Bld_Cmd* cmd, Bld_CompileCmd c) {
    bld_cmd_appendf(cmd, "%s", c.driver);

    /* optimization */
    const char* os = bld__resolve_optimize(c.optimize);
    if (os[0]) bld_cmd_appendf(cmd, " %s", os);

    /* language standard */
    const char* ss = "";
    if (c.lang == BLD_LANG_C) ss = bld__resolve_c_standard(c.c_std);
    else if (c.lang == BLD_LANG_CXX) ss = bld__resolve_cxx_standard(c.cxx_std);
    if (ss[0]) bld_cmd_appendf(cmd, " %s", ss);

    /* warnings */
    if (!c.warnings) bld_cmd_appendf(cmd, " -w");
    if (c.warnings) bld_cmd_appendf(cmd, " -Wall");

    /* extra compile flags (from CompileFlags.extra_flags) */
    if (c.extra_flags && c.extra_flags[0]) bld_cmd_appendf(cmd, " %s", c.extra_flags);

    /* defines */
    for (size_t i = 0; i < c.defines.count; i++) {
        bld_cmd_appendf(cmd, " -D");
        bld_cmd_append_sq(cmd, c.defines.items[i]);
    }

    /* include dirs from CompileFlags */
    for (size_t i = 0; i < c.include_dirs.count; i++)
        bld_cmd_appendf(cmd, " -I%s", c.include_dirs.items[i]);

    /* system include dirs from CompileFlags */
    for (size_t i = 0; i < c.sys_include_dirs.count; i++)
        bld_cmd_appendf(cmd, " -isystem %s", c.sys_include_dirs.items[i]);

    /* debug/sanitizer/lto */
    if (c.debug_info) bld_cmd_appendf(cmd, " -g");
    if (c.asan) bld_cmd_appendf(cmd, " -fsanitize=address");
    if (c.lto) bld_cmd_appendf(cmd, " -flto");

    /* PIC */
    if (c.pic) bld_cmd_appendf(cmd, " -fPIC");

    /* extra cflags (merged from ext_deps) — includes target include dirs (quoted),
       ext_dep -I/-isystem/extra_cflags already flattened into this field */
    if (c.extra_cflags && c.extra_cflags[0]) bld_cmd_appendf(cmd, " %s", c.extra_cflags);

    /* source file (quoted) */
    bld_cmd_appendf(cmd, " \"%s\"", c.source);

    /* -c -o output */
    bld_cmd_appendf(cmd, " -c -o %s", c.output);

    /* dependency file */
    if (c.depfile && c.depfile[0])
        bld_cmd_appendf(cmd, " -MMD -MF %s", c.depfile);
}

/*
 * GCC render_link: produces the FULL link command.
 *
 * For exe (shared==0):
 *   driver, -g/-fsanitize/-flto, obj_paths (quoted), -L lib_dirs (quoted), -l lib_names,
 *   -Wl,-rpath,<rpath> for each rpath, -o output, extra_ldflags
 *
 * For shared lib (shared==1):
 *   driver, -g/-fsanitize/-flto, -shared, OS-dependent soname flag,
 *   obj_paths (quoted), -o output, extra_ldflags
 */
static void bld__gcc_render_link(Bld_Cmd* cmd, Bld_LinkCmd c) {
    bld_cmd_appendf(cmd, "%s", c.driver);

    /* debug/sanitizer/lto */
    if (c.debug_info) bld_cmd_appendf(cmd, " -g");
    if (c.asan) bld_cmd_appendf(cmd, " -fsanitize=address");
    if (c.lto) bld_cmd_appendf(cmd, " -flto");

    if (c.shared) {
        bld_cmd_appendf(cmd, " -shared");
        if (c.soname && c.soname[0])
            bld_cmd_appendf(cmd, " -Wl,-soname,%s", c.soname);

        /* obj paths */
        for (size_t i = 0; i < c.obj_paths.count; i++)
            bld_cmd_appendf(cmd, " \"%s\"", c.obj_paths.items[i]);

        bld_cmd_appendf(cmd, " -o %s", c.output);
    } else {
        /* obj paths */
        for (size_t i = 0; i < c.obj_paths.count; i++)
            bld_cmd_appendf(cmd, " \"%s\"", c.obj_paths.items[i]);

        /* lib dirs */
        for (size_t i = 0; i < c.lib_dirs.count; i++)
            bld_cmd_appendf(cmd, " -L\"%s\"", c.lib_dirs.items[i]);

        /* lib names */
        for (size_t i = 0; i < c.lib_names.count; i++)
            bld_cmd_appendf(cmd, " -l%s", c.lib_names.items[i]);

        /* rpaths */
        for (size_t i = 0; i < c.rpaths.count; i++)
            bld_cmd_appendf(cmd, " -Wl,-rpath,%s", c.rpaths.items[i]);

        bld_cmd_appendf(cmd, " -o %s", c.output);
    }

    /* extra link flags */
    if (c.extra_ldflags && c.extra_ldflags[0])
        bld_cmd_appendf(cmd, " %s", c.extra_ldflags);
}

/*
 * GCC render_archive: produces "tool rcs output obj1 obj2 ..."
 */
static void bld__gcc_render_archive(Bld_Cmd* cmd, const char* tool, const char* output, Bld_Paths obj_paths) {
    bld_cmd_appendf(cmd, "%s rcs %s", tool, output);
    for (size_t i = 0; i < obj_paths.count; i++)
        bld_cmd_appendf(cmd, " \"%s\"", obj_paths.items[i]);
}

Bld_Toolchain* bld_toolchain_gcc(Bld_OsTarget os) {
    Bld_Toolchain* tc = bld_arena_alloc(sizeof(Bld_Toolchain));
    memset(tc, 0, sizeof(*tc));
    tc->name = "gcc";
    tc->os = os;

    /* extensions by OS */
    tc->obj_ext = "o";
    tc->static_lib_prefix = "lib";
    tc->static_lib_ext = "a";

    switch (os) {
        case BLD_OS_WINDOWS:
            tc->exe_ext = ".exe";
            tc->shared_lib_prefix = "";
            tc->shared_lib_ext = "dll";
            break;
        case BLD_OS_MACOS:
            tc->exe_ext = "";
            tc->shared_lib_prefix = "lib";
            tc->shared_lib_ext = "dylib";
            break;
        case BLD_OS_FREEBSD:
        case BLD_OS_LINUX:
        default:
            tc->exe_ext = "";
            tc->shared_lib_prefix = "lib";
            tc->shared_lib_ext = "so";
            break;
    }

    /* detect archiver */
    if (bld__has_in_path("llvm-ar")) {
        tc->archiver.driver = "llvm-ar";
        tc->archiver.identity_hash = bld__make_identity_hash("llvm-ar");
        tc->archiver.available = true;
    } else if (bld__has_in_path("ar")) {
        tc->archiver.driver = "ar";
        tc->archiver.identity_hash = bld__make_identity_hash("ar");
        tc->archiver.available = true;
    }

    /* render functions */
    tc->render_compile = bld__gcc_render_compile;
    tc->render_link    = bld__gcc_render_link;
    tc->render_archive = bld__gcc_render_archive;

    return tc;
}

