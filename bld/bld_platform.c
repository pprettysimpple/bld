/* bld/bld_platform.c — platform-specific implementations */
#pragma once

#include "bld_platform.h"

/* ================================================================
 *  mkdir (single directory)
 * ================================================================ */

#ifdef _WIN32
static int bld_plat_mkdir(const char* path) { return _mkdir(path); }
#else
static int bld_plat_mkdir(const char* path) { return mkdir(path, 0755); }
#endif

/* ================================================================
 *  Rename (atomic on POSIX, retry on Windows for file locking)
 * ================================================================ */

#ifdef _WIN32
static int bld_plat_rename(const char* from, const char* to) {
    /* MoveFileEx with REPLACE_EXISTING handles dest-exists and is more
     * robust than C rename() on Windows. Retry briefly for antivirus locks. */
    for (int attempt = 0; attempt < 5; attempt++) {
        if (MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING)) return 0;
        Sleep(10); /* 10ms between retries */
    }
    return -1;
}
#else
static int bld_plat_rename(const char* from, const char* to) {
    return rename(from, to);
}
#endif

/* ================================================================
 *  Directory iteration
 * ================================================================ */

#ifdef _WIN32

struct Bld_Dir {
    HANDLE           h;
    WIN32_FIND_DATAA data;
    int              first;
    int              valid;
};

static Bld_Dir* bld_plat_dir_open(const char* path) {
    Bld_Dir* d = bld_arena_alloc(sizeof(Bld_Dir));
    d->first = 1;
    d->valid = 0;
    char pat[MAX_PATH];
    snprintf(pat, sizeof(pat), "%s\\*", path);
    d->h = FindFirstFileA(pat, &d->data);
    d->valid = (d->h != INVALID_HANDLE_VALUE);
    return d;
}

static const char* bld_plat_dir_next(Bld_Dir* d) {
    while (1) {
        if (!d->valid) return NULL;
        if (d->first) { d->first = 0; }
        else { d->valid = FindNextFileA(d->h, &d->data); if (!d->valid) return NULL; }
        if (strcmp(d->data.cFileName, ".") == 0 || strcmp(d->data.cFileName, "..") == 0) continue;
        return d->data.cFileName;
    }
}

static void bld_plat_dir_close(Bld_Dir* d) {
    if (d->h != INVALID_HANDLE_VALUE) FindClose(d->h);
}

#else

struct Bld_Dir { DIR* d; };

static Bld_Dir* bld_plat_dir_open(const char* path) {
    Bld_Dir* d = bld_arena_alloc(sizeof(Bld_Dir));
    d->d = opendir(path);
    return d;
}

static const char* bld_plat_dir_next(Bld_Dir* d) {
    struct dirent* ent;
    while ((ent = readdir(d->d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        return ent->d_name;
    }
    return NULL;
}

static void bld_plat_dir_close(Bld_Dir* d) { if (d->d) closedir(d->d); }

#endif

/* ================================================================
 *  Glob pattern matching
 * ================================================================ */

#ifdef _WIN32
/* simple glob matching: supports *, ?, [chars] */
static int bld_plat_glob(const char* pat, const char* str) {
    while (*pat) {
        if (*pat == '*') {
            pat++;
            if (!*pat) return 0;
            for (; *str; str++) { if (bld_plat_glob(pat, str) == 0) return 0; }
            return 1;
        } else if (*pat == '?') {
            if (!*str) return 1;
            pat++; str++;
        } else if (*pat == '[') {
            pat++;
            int inv = 0, matched = 0;
            if (*pat == '!' || *pat == '^') { inv = 1; pat++; }
            while (*pat && *pat != ']') {
                if (pat[1] == '-' && pat[2] && pat[2] != ']') {
                    if (*str >= pat[0] && *str <= pat[2]) matched = 1;
                    pat += 3;
                } else {
                    if (*str == *pat) matched = 1;
                    pat++;
                }
            }
            if (*pat == ']') pat++;
            if (inv) matched = !matched;
            if (!matched || !*str) return 1;
            str++;
        } else {
            if (*pat != *str) return 1;
            pat++; str++;
        }
    }
    return *str ? 1 : 0;
}
#else
static int bld_plat_glob(const char* pat, const char* str) { return fnmatch(pat, str, 0); }
#endif

/* ================================================================
 *  realpath
 * ================================================================ */

#ifdef _WIN32
static char* bld_plat_realpath(const char* path) {
    char* buf = bld_arena_alloc(MAX_PATH);
    DWORD n = GetFullPathNameA(path, MAX_PATH, buf, NULL);
    if (n == 0 || n >= MAX_PATH) return NULL;
    /* convert backslashes to forward slashes */
    for (char* p = buf; *p; p++) if (*p == '\\') *p = '/';
    return buf;
}
#else
static char* bld_plat_realpath(const char* path) {
    char* resolved = realpath(path, NULL);
    if (!resolved) return NULL;
    char* dup = bld_arena_alloc(strlen(resolved) + 1);
    strcpy(dup, resolved);
    free(resolved);
    return dup;
}
#endif

/* ================================================================
 *  Virtual memory for arena
 * ================================================================ */

#ifdef _WIN32
static void* bld_plat_vmem(size_t size) {
    void* mem = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!mem) {
        fprintf(stderr, "bld: fatal: VirtualAlloc arena failed\n");
        exit(1);
    }
    return mem;
}
#else
static void* bld_plat_vmem(size_t size) {
    void* mem = mmap(NULL, size,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        fprintf(stderr, "bld: fatal: mmap arena failed: %s\n", strerror(errno));
        exit(1);
    }
    return mem;
}
#endif

/* ================================================================
 *  Hash large file (> 4096 bytes)
 * ================================================================ */

static uint64_t bld__fnv1a(const void* data, size_t len); /* forward decl from bld_core_impl.c */

#ifdef _WIN32
static Bld_Hash bld_plat_hash_large_file(int fd, size_t len) {
    (void)len;
    uint64_t h = 0xcbf29ce484222325ull;
    char buf[8192];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        const uint8_t* bp = (const uint8_t*)buf;
        for (ssize_t i = 0; i < n; i++) { h ^= bp[i]; h *= 0x100000001b3ull; }
    }
    close(fd);
    return (Bld_Hash){h};
}
#else
static Bld_Hash bld_plat_hash_large_file(int fd, size_t len) {
    void* data = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { close(fd); bld_panic("hash_file: mmap failed: %s\n", strerror(errno)); }
    Bld_Hash h = {bld__fnv1a(data, len)};
    munmap(data, len);
    close(fd);
    return h;
}
#endif

/* ================================================================
 *  Subprocess execution
 * ================================================================ */

#ifdef _WIN32
static Bld_ProcResult bld_plat_run(const char* cmd, const char* workdir, int flags) {
    Bld_ProcResult res = {.exit_code = -1, .output_file = bld_path("")};

    HANDLE hOutFile = INVALID_HANDLE_VALUE;
    if (!(flags & (BLD_PROC_SILENT | BLD_PROC_PASSTHRU))) {
        char tmppath[MAX_PATH], tmpfile[MAX_PATH];
        GetTempPathA(MAX_PATH, tmppath);
        GetTempFileNameA(tmppath, "bld", 0, tmpfile);
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
        hOutFile = CreateFileA(tmpfile, GENERIC_WRITE, FILE_SHARE_READ, &sa,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hOutFile == INVALID_HANDLE_VALUE) return res;
        /* convert backslashes in path for consistency */
        char* dup = bld_arena_alloc(strlen(tmpfile) + 1);
        strcpy(dup, tmpfile);
        for (char* p = dup; *p; p++) if (*p == '\\') *p = '/';
        res.output_file = bld_path(dup);
    }

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (flags & BLD_PROC_SILENT) {
        si.dwFlags = STARTF_USESTDHANDLES;
        HANDLE nul = CreateFileA("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        si.hStdOutput = nul;
        si.hStdError  = nul;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    } else if (hOutFile != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hOutFile;
        si.hStdError  = hOutFile;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    }

    /* pass command directly to CreateProcess — no shell needed for compiler commands */
    char* mut_cmd = bld_arena_alloc(strlen(cmd) + 1);
    strcpy(mut_cmd, cmd);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    BOOL ok = CreateProcessA(NULL, mut_cmd, NULL, NULL, TRUE, 0, NULL, workdir, &si, &pi);
    if (hOutFile != INVALID_HANDLE_VALUE) CloseHandle(hOutFile);

    if (!ok) {
        if (res.output_file.s[0]) unlink(res.output_file.s);
        res.output_file = bld_path("");
        return res;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitcode = 0;
    GetExitCodeProcess(pi.hProcess, &exitcode);
    res.exit_code = (int)exitcode;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return res;
}
#else
static Bld_ProcResult bld_plat_run(const char* cmd, const char* workdir, int flags) {
    Bld_ProcResult res = {.exit_code = -1, .output_file = bld_path("")};

    int outfd = -1;
    if (!(flags & (BLD_PROC_SILENT | BLD_PROC_PASSTHRU))) {
        char tpl[] = "/tmp/bld_XXXXXX";
        outfd = mkstemp(tpl);
        if (outfd < 0) return res;
        res.output_file = bld_path(bld_str_dup(tpl));
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (outfd >= 0) { close(outfd); unlink(res.output_file.s); }
        return res;
    }

    if (pid == 0) {
        if (workdir && workdir[0]) chdir(workdir);
        if (flags & BLD_PROC_SILENT) {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) { dup2(devnull, STDOUT_FILENO); dup2(devnull, STDERR_FILENO); close(devnull); }
        } else if (outfd >= 0) {
            dup2(outfd, STDOUT_FILENO); dup2(outfd, STDERR_FILENO); close(outfd);
        }
        execvp("sh", (char*[]){"sh", "-c", (char*)cmd, NULL});
        _exit(127);
    }

    if (outfd >= 0) close(outfd);
    int status;
    waitpid(pid, &status, 0);
    res.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return res;
}
#endif

/* ================================================================
 *  Output helpers (not platform-specific, but used by subprocess callers)
 * ================================================================ */

/* stream file contents to stderr */
static void bld__dump_to_stderr(Bld_Path path) {
    if (!path.s || !path.s[0]) return;
    FILE* f = fopen(path.s, "rb");
    if (!f) return;
    char buf[4096]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        fwrite(buf, 1, n, stderr);
    fclose(f);
}

/* discard captured output */
static void bld__proc_discard_output(Bld_ProcResult* r) {
    if (!r->output_file.s || !r->output_file.s[0]) return;
    unlink(r->output_file.s);
    r->output_file = bld_path("");
}

/* ================================================================
 *  PATH separator
 * ================================================================ */

#ifdef _WIN32
static char bld_plat_path_list_sep(void) { return ';'; }
#else
static char bld_plat_path_list_sep(void) { return ':'; }
#endif

/* ================================================================
 *  PATH lookup
 * ================================================================ */

/* find exact name in PATH — no extension magic */
static const char* bld_plat_find_in_path(const char* name) {
    const char* path_env = getenv("PATH");
    if (!path_env) return NULL;
    char sep = bld_plat_path_list_sep();
    const char* p = path_env;
    while (*p) {
        const char* s = strchr(p, sep);
        size_t len = s ? (size_t)(s - p) : strlen(p);
        Bld_Path full = bld_path(bld_str_fmt("%.*s/%s", (int)len, p, name));
        if (bld_fs_is_file(full)) return full.s;
        p += len + (s ? 1 : 0);
    }
    return NULL;
}

/* find executable binary in PATH — adds .exe on Windows */
static const char* bld_plat_find_binary(const char* name) {
#ifdef _WIN32
    return bld_plat_find_in_path(bld_str_fmt("%s.exe", name));
#else
    return bld_plat_find_in_path(name);
#endif
}

/* ================================================================
 *  Timer
 * ================================================================ */

#ifdef _WIN32
static double bld_plat_time(void) {
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart / (double)freq.QuadPart;
}
#else
static double bld_plat_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
#endif

/* ================================================================
 *  Thread workers
 * ================================================================ */

#ifdef _WIN32

typedef struct {
    Bld_WorkerFn fn;
    void* ctx;
} Bld__PlatWorkerCtx;

static DWORD WINAPI bld__plat_worker_trampoline(void* arg) {
    Bld__PlatWorkerCtx* c = arg;
    c->fn(c->ctx);
    return 0;
}

static void bld_plat_run_workers(Bld_WorkerFn fn, void* ctx, int count) {
    HANDLE* th = bld_arena_alloc(sizeof(HANDLE) * (size_t)count);
    Bld__PlatWorkerCtx* wctx = bld_arena_alloc(sizeof(Bld__PlatWorkerCtx));
    wctx->fn = fn;
    wctx->ctx = ctx;
    for (int t = 0; t < count; t++)
        th[t] = CreateThread(NULL, 0, bld__plat_worker_trampoline, wctx, 0, NULL);
    WaitForMultipleObjects((DWORD)count, th, TRUE, INFINITE);
    for (int t = 0; t < count; t++) CloseHandle(th[t]);
}

#else

typedef struct {
    Bld_WorkerFn fn;
    void* ctx;
} Bld__PlatWorkerCtx;

static void* bld__plat_worker_trampoline(void* arg) {
    Bld__PlatWorkerCtx* c = arg;
    c->fn(c->ctx);
    return NULL;
}

static void bld_plat_run_workers(Bld_WorkerFn fn, void* ctx, int count) {
    pthread_t* th = bld_arena_alloc(sizeof(pthread_t) * (size_t)count);
    Bld__PlatWorkerCtx* wctx = bld_arena_alloc(sizeof(Bld__PlatWorkerCtx));
    wctx->fn = fn;
    wctx->ctx = ctx;
    for (int t = 0; t < count; t++) pthread_create(&th[t], NULL, bld__plat_worker_trampoline, wctx);
    for (int t = 0; t < count; t++) pthread_join(th[t], NULL);
}

#endif

/* ================================================================
 *  CPU count
 * ================================================================ */

#ifdef _WIN32
static int bld_plat_cpus(void) {
    SYSTEM_INFO si; GetSystemInfo(&si);
    int n = (int)si.dwNumberOfProcessors;
    return n > 0 ? n : 1;
}
#else
static int bld_plat_cpus(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
}
#endif

/* ================================================================
 *  Self-recompile + re-exec
 * ================================================================ */

#ifdef _WIN32
/* Windows: can't overwrite running exe. rename it first, compile new one, re-exec. */
static void bld_plat_self_recompile(const char* cmd, const char* exe, char** argv) {
    const char* old = bld_str_fmt("%s.old", exe);
    unlink(old);          /* remove leftover from previous recompile */
    rename(exe, old);     /* rename running exe (Windows allows this) */
    Bld_ProcResult r = bld_plat_run(cmd, NULL, BLD_PROC_DEFAULT);
    if (r.exit_code != 0) {
        bld__dump_to_stderr(r.output_file);
        rename(old, exe); /* restore on failure */
        bld_panic("failed to recompile build tool\n");
    }
    bld__proc_discard_output(&r);
    intptr_t rc = _spawnv(_P_WAIT, exe, (const char* const*)argv);
    exit((int)rc);
}
#else
static void bld_plat_self_recompile(const char* cmd, const char* exe, char** argv) {
    (void)exe;
    Bld_ProcResult r = bld_plat_run(cmd, NULL, BLD_PROC_DEFAULT);
    if (r.exit_code != 0) {
        bld__dump_to_stderr(r.output_file);
        bld_panic("failed to recompile build tool\n");
    }
    bld__proc_discard_output(&r);
    execv(argv[0], argv);
    bld_panic("execv failed: %s\n", strerror(errno));
}
#endif

/* ================================================================
 *  getcwd with backslash conversion
 * ================================================================ */

#ifdef _WIN32
static char* bld_plat_getcwd(char* buf, size_t size) {
    if (!getcwd(buf, (int)size)) return NULL;
    for (char* p = buf; *p; p++) if (*p == '\\') *p = '/';
    return buf;
}
#else
static char* bld_plat_getcwd(char* buf, size_t size) {
    (void)buf; (void)size;
    return getcwd(NULL, 0);
}
#endif
