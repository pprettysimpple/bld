/* bld/bld_core.h — all declarations, types, and API for bld */
#pragma once

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <direct.h>
  #include <io.h>
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <process.h>

  /* POSIX compat types/macros for MSVC */
  typedef ptrdiff_t ssize_t;
  #ifndef STDOUT_FILENO
    #define STDOUT_FILENO 1
  #endif
  #ifndef STDERR_FILENO
    #define STDERR_FILENO 2
  #endif
  #define stat    _stat64
  #define fstat   _fstat64
  #define open    _open
  #define read    _read
  #define write   _write
  #define close   _close
  #define unlink  _unlink
  #define popen   _popen
  #define pclose  _pclose
  #define isatty  _isatty
  #define fileno  _fileno
  #define getcwd  _getcwd
  #define chdir   _chdir
  #define rmdir   _rmdir
  #define chmod(p,m) ((void)0) /* no-op on Windows */
  #define O_RDONLY _O_RDONLY
  #define O_WRONLY _O_WRONLY
  #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
  #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
  #ifndef EXDEV
    #define EXDEV 18
  #endif

  /* Thread / sync abstraction */
  typedef CRITICAL_SECTION     Bld_Mutex;
  typedef CONDITION_VARIABLE   Bld_Cond;
#else
  #include <dirent.h>
  #include <fcntl.h>
  #include <fnmatch.h>
  #include <pthread.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <sys/wait.h>
  #include <unistd.h>

  /* Thread / sync abstraction */
  typedef pthread_mutex_t      Bld_Mutex;
  typedef pthread_cond_t       Bld_Cond;
#endif

/* ===== Platform abstraction: atomics ===== */

#ifdef _MSC_VER
  #define bld__atomic_fetch_add64(ptr, val)  InterlockedExchangeAdd64((volatile LONG64*)(ptr), (LONG64)(val))
  #define bld__atomic_store32(ptr, val)      InterlockedExchange((volatile LONG*)(ptr), (LONG)(val))
#else
  #define bld__atomic_fetch_add64(ptr, val)  __atomic_fetch_add((ptr), (val), __ATOMIC_RELAXED)
  #define bld__atomic_store32(ptr, val)      __atomic_store_n((ptr), (val), __ATOMIC_RELAXED)
#endif

/* ===== Platform abstraction: mutex / cond helpers ===== */

#ifdef _WIN32
  static inline void bld__mutex_init(Bld_Mutex* m)  { InitializeCriticalSection(m); }
  static inline void bld__mutex_lock(Bld_Mutex* m)  { EnterCriticalSection(m); }
  static inline void bld__mutex_unlock(Bld_Mutex* m){ LeaveCriticalSection(m); }
  static inline void bld__cond_init(Bld_Cond* c)    { InitializeConditionVariable(c); }
  static inline void bld__cond_wait(Bld_Cond* c, Bld_Mutex* m) { SleepConditionVariableCS(c, m, INFINITE); }
  static inline void bld__cond_broadcast(Bld_Cond* c){ WakeAllConditionVariable(c); }
#else
  static inline void bld__mutex_init(Bld_Mutex* m)  { pthread_mutex_init(m, NULL); }
  static inline void bld__mutex_lock(Bld_Mutex* m)  { pthread_mutex_lock(m); }
  static inline void bld__mutex_unlock(Bld_Mutex* m){ pthread_mutex_unlock(m); }
  static inline void bld__cond_init(Bld_Cond* c)    { pthread_cond_init(c, NULL); }
  static inline void bld__cond_wait(Bld_Cond* c, Bld_Mutex* m) { pthread_cond_wait(c, m); }
  static inline void bld__cond_broadcast(Bld_Cond* c){ pthread_cond_broadcast(c); }
#endif

/* ===== Arena ===== */

#ifndef BLD_ARENA_SIZE
  #define BLD_ARENA_SIZE ((size_t)10 * 1024 * 1024 * 1024) /* 10 GB virtual */
#endif

typedef struct {
    char*  base;
    size_t offset;
    size_t capacity;
    char*  last_ptr;
    size_t last_size;
    Bld_Mutex mutex;
} Bld_Arena;

Bld_Arena* bld_arena_get(void);
void*      bld_arena_alloc(size_t size);
void*      bld_arena_realloc(void* old_ptr, size_t old_size, size_t new_size);

/* ===== Dynamic array (generic via macros, arena-backed) ===== */

#define bld_da_push(da, item) do {                                            \
    if ((da)->count >= (da)->cap) {                                           \
        size_t _new_cap = (da)->cap ? (da)->cap * 2 : 8;                     \
        (da)->items = bld_arena_realloc(                                      \
            (da)->items,                                                      \
            (da)->cap * sizeof(*(da)->items),                                 \
            _new_cap * sizeof(*(da)->items));                                 \
        (da)->cap = _new_cap;                                                 \
    }                                                                         \
    (da)->items[(da)->count++] = (item);                                      \
} while (0)

#define bld_da_last(da) ((da)->items[(da)->count - 1])
#define BLD_DA(T) struct { T* items; size_t count, cap; }

/* ===== Str — arena-allocated null-terminated strings ===== */

const char* bld_str_fmt(const char* fmt, ...);
const char* bld_str_dup(const char* s);
const char* bld_str_cat(const char* first, ...); /* NULL sentinel */

/* ===== Path — typed wrapper over const char* ===== */

typedef struct { const char* s; } Bld_Path;

#define bld_filepath(literal) ((Bld_Path){(literal)})
#define bld_path(literal) bld_filepath(literal)  /* compat alias */

Bld_Path    bld_path_join(Bld_Path a, Bld_Path b);
Bld_Path    bld_path_parent(Bld_Path p);
const char* bld_path_filename(Bld_Path p);
const char* bld_path_ext(Bld_Path p);
Bld_Path    bld_path_replace_ext(Bld_Path p, const char* ext);
Bld_Path    bld_path_fmt(const char* fmt, ...);

typedef BLD_DA(Bld_Path)       Bld_PathList;

/* ===== Typed slices for user-facing API ===== */

typedef struct { const char** items; size_t count, cap; } Bld_Strs;   /* defines, lib names */
typedef struct { const char** items; size_t count, cap; } Bld_Paths;  /* source files, dir paths */

Bld_Strs    bld_str_lines(const char* s);
const char* bld_str_join(const Bld_Strs* parts, const char* sep);
const char** bld_dup_strarray(const char** arr); /* deep-copy NULL-terminated string array (internal) */

/* ===== Cmd — growable string buffer (arena-backed) ===== */

typedef struct { char* items; size_t count, cap; } Bld_Cmd;

void bld_cmd_appendf(Bld_Cmd* cmd, const char* fmt, ...);
void bld_cmd_append_sq(Bld_Cmd* cmd, const char* s); /* shell-safe single-quote */

/* ===== Hash ===== */

typedef struct { uint64_t value; } Bld_Hash;

Bld_Hash bld_hash_combine(Bld_Hash a, Bld_Hash b);
Bld_Hash bld_hash_combine_unordered(Bld_Hash a, Bld_Hash b);
Bld_Hash bld_hash_str(const char* s);
Bld_Hash bld_hash_file(Bld_Path p);
Bld_Hash bld_hash_dir(Bld_Path dir);

Bld_Strs  bld__strs_lit(const char** items, size_t len);
Bld_Paths bld__paths_lit(const char** items, size_t len);

#define BLD_STRS(...)  bld__strs_lit( \
    (const char*[]){__VA_ARGS__}, \
    sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*))

#define BLD_PATHS(...) bld__paths_lit( \
    (const char*[]){__VA_ARGS__}, \
    sizeof((const char*[]){__VA_ARGS__}) / sizeof(const char*))

void      bld_strs_push(Bld_Strs* s, const char* item);
void      bld_paths_push(Bld_Paths* s, const char* item);
Bld_Strs  bld_strs_merge(Bld_Strs a, Bld_Strs b);
Bld_Paths bld_paths_merge(Bld_Paths a, Bld_Paths b);

/* ===== Log ===== */

void bld_log(const char* fmt, ...);
#ifdef _MSC_VER
  #define BLD_NORETURN __declspec(noreturn)
#else
  #define BLD_NORETURN __attribute__((noreturn))
#endif
BLD_NORETURN void bld_panic(const char* fmt, ...);

/* structured log functions (color-aware, thread-safe) */
void bld_log_progress(uint64_t current, uint64_t total, const char* name);
void bld_log_cached(const char* name);
void bld_log_done(uint64_t executed, uint64_t cached, uint64_t failed, uint64_t skipped, double elapsed, size_t arena_used);
void bld_log_action(const char* fmt, ...);  /* compile/link commands (-v) */
void bld_log_info(const char* fmt, ...);    /* [*] info messages */

/* ===== Filesystem ===== */

bool     bld_fs_exists(Bld_Path p);
bool     bld_fs_is_dir(Bld_Path p);
bool     bld_fs_is_file(Bld_Path p);
void     bld_fs_mkdir_p(Bld_Path p);
void     bld_fs_remove(Bld_Path p);
void     bld_fs_remove_all(Bld_Path p);
void     bld_fs_rename(Bld_Path from, Bld_Path to);
void     bld_fs_copy_file(Bld_Path from, Bld_Path to);
void     bld_fs_copy_r(Bld_Path from, Bld_Path to);
Bld_Path bld_fs_realpath(Bld_Path p);
Bld_Path bld_fs_getcwd(void);

Bld_PathList bld_fs_list_files_r(Bld_Path dir);

const char* bld_fs_read_file(Bld_Path p, size_t* out_len);
void        bld_fs_write_file(Bld_Path p, const char* data, size_t len);
static inline void bld_fs_write_str(Bld_Path p, const char* s) { bld_fs_write_file(p, s, strlen(s)); }

/* glob: returns Bld_Paths with sized .items */
Bld_Paths bld_files_glob(const char* pattern);
/* exclude paths from a Bld_Paths, returns new Bld_Paths */
Bld_Paths bld_files_exclude(Bld_Paths files, Bld_Paths exclude);
/* merge two Bld_Paths, returns new Bld_Paths */
Bld_Paths bld_files_merge(Bld_Paths a, Bld_Paths b);

/* ===== Enums ===== */

typedef enum {
    BLD_OPT_DEFAULT = 0,
    BLD_OPT_O0, BLD_OPT_O1, BLD_OPT_O2, BLD_OPT_O3, BLD_OPT_Os, BLD_OPT_OFAST,
} Bld_Optimize;

typedef enum {
    BLD_LANG_AUTO = 0, BLD_LANG_C, BLD_LANG_CXX, BLD_LANG_ASM, BLD_LANG__COUNT
} Bld_Lang;

typedef enum {
    BLD_C_DEFAULT = 0,
    BLD_C_90, BLD_C_99, BLD_C_11, BLD_C_17, BLD_C_23,
    BLD_C_GNU90, BLD_C_GNU99, BLD_C_GNU11, BLD_C_GNU17, BLD_C_GNU23,
} Bld_CStd;

typedef enum {
    BLD_CXX_DEFAULT = 0,
    BLD_CXX_11, BLD_CXX_14, BLD_CXX_17, BLD_CXX_20, BLD_CXX_23,
    BLD_CXX_GNU11, BLD_CXX_GNU14, BLD_CXX_GNU17, BLD_CXX_GNU20, BLD_CXX_GNU23,
} Bld_CxxStd;

typedef struct {
    Bld_Lang    lang;
    const char* driver;
    Bld_Hash    identity_hash;
    bool        available;
    union {
        struct { Bld_CStd standard; } c;
        struct { Bld_CxxStd standard; } cxx;
        struct { int _pad; } as;
    };
} Bld_Compiler;

typedef enum { BLD_UNSET = 0, BLD_ON, BLD_OFF } Bld_Toggle;

/* ===== Toolchain ===== */

typedef enum {
    BLD_OS_LINUX = 0, BLD_OS_MACOS, BLD_OS_WINDOWS, BLD_OS_FREEBSD,
} Bld_OsTarget;

typedef struct {
    const char* driver;
    Bld_Hash    identity_hash;
    bool        available;
} Bld_Tool;

typedef struct {
    const char*  driver;
    Bld_Lang     lang;
    Bld_CStd     c_std;
    Bld_CxxStd   cxx_std;
    Bld_Optimize optimize;
    bool         warnings;
    bool         pic;
    bool         debug_info;
    bool         asan;
    bool         lto;
    const char*  extra_flags;
    Bld_Strs     defines;
    Bld_Paths    include_dirs;
    Bld_Paths    sys_include_dirs;
    const char*  extra_cflags;
    const char*  source;
    const char*  output;
    const char*  depfile;
} Bld_CompileCmd;

typedef struct {
    const char*  driver;
    bool         shared;
    bool         debug_info;
    bool         asan;
    bool         lto;
    const char*  soname;
    Bld_Paths    obj_paths;
    Bld_Paths    lib_dirs;
    Bld_Strs     lib_names;
    Bld_Paths    rpaths;
    const char*  extra_ldflags;
    const char*  output;
} Bld_LinkCmd;

typedef struct Bld_Toolchain Bld_Toolchain;
struct Bld_Toolchain {
    const char* name;
    Bld_OsTarget os;

    Bld_Compiler compilers[BLD_LANG__COUNT - 1];
    Bld_Tool     archiver;

    const char*  obj_ext;
    const char*  exe_ext;
    const char*  static_lib_prefix;
    const char*  static_lib_ext;
    const char*  shared_lib_prefix;
    const char*  shared_lib_ext;

    void (*render_compile)(Bld_Cmd* cmd, Bld_CompileCmd args);
    void (*render_link)   (Bld_Cmd* cmd, Bld_LinkCmd args);
    void (*render_archive)(Bld_Cmd* cmd, const char* tool, const char* output, Bld_Paths obj_paths);

    Bld_Hash     sysinclude_hash;  /* hash of system include paths + mtimes */

    void* user_data;
};

Bld_Toolchain* bld_toolchain_gcc(Bld_OsTarget os);

/* ===== Step — low-level graph node ===== */

typedef struct Bld_Step Bld_Step;
typedef struct Bld_Target Bld_Target;

typedef struct {
    int      status;        /* 0 = success, non-zero = failure */
    Bld_Path output_file;   /* captured error output (empty if none) */
} Bld_ActionResult;

#define BLD_ACTION_OK   ((Bld_ActionResult){0, {0}})
#define BLD_ACTION_FAIL ((Bld_ActionResult){1, {0}})

typedef Bld_ActionResult (*Bld_ActionFn)(void* ctx, Bld_Path output, Bld_Path depfile);
typedef Bld_Hash (*Bld_HashFn)(void* ctx, Bld_Hash current);

typedef enum {
    BLD_STEP_PENDING = 0,
    BLD_STEP_RUNNING,
    BLD_STEP_OK,
    BLD_STEP_FAILED,
    BLD_STEP_SKIPPED,
} Bld_StepState;

/* named DA types for struct fields */
typedef BLD_DA(Bld_Step*)   Bld_StepList;
typedef BLD_DA(Bld_Target*) Bld_TargetList;

struct Bld_Step {
    const char*  name;
    bool         silent;
    bool         phony;      /* always execute, never cache */

    Bld_StepList deps;      /* ordering dependencies */
    Bld_StepList inputs;    /* data dependencies (step outputs used by action) */

    Bld_ActionFn     action;
    void*            action_ctx;
    Bld_HashFn hash_fn;
    void*            hash_fn_ctx;

    bool has_depfile;
    bool content_hash;  /* use output content hash instead of recipe hash for downstream */

    /* computed at build time */
    size_t        idx;          /* index in b->all_steps (set at creation) */
    Bld_Hash input_hash;
    Bld_Hash cache_key;
    bool          hash_valid;
    Bld_StepState state;

    Bld_Mutex mutex;
    Bld_Cond  cond;
};

/* ===== LazyPath ===== */

typedef struct {
    Bld_Target* source;
    Bld_Path    path;
} Bld_LazyPath;

/* ===== External dependency bundle (internal, being phased out) ===== */

/* ===== Build / compiler / linker flags ===== */

typedef struct {
    Bld_Toggle asan;
    Bld_Toggle lto;
} Bld_BuildFlags;

typedef struct {
    Bld_Optimize   optimize;
    Bld_Toggle     warnings;
    Bld_Toggle     debug_info;
    const char*    extra_flags;
    Bld_Strs       defines;
    Bld_Paths      include_dirs;        /* -I */
    Bld_Paths      system_include_dirs; /* -isystem */
} Bld_CompileFlags;

typedef struct {
    const char* extra_flags;   /* appended after objects, e.g. "-lm -ldl" */
    Bld_Strs    libs;          /* -l names */
    Bld_Paths   lib_dirs;      /* -L paths */
} Bld_LinkFlags;

typedef BLD_DA(Bld_LazyPath) Bld_LazyPathList;

/* ===== Target — high-level subgraph ===== */

typedef enum {
    BLD_TGT_EXE,
    BLD_TGT_LIB,
    BLD_TGT_CUSTOM,
    BLD_TGT_PKG,
} Bld_TargetKind;

typedef struct {
    const char*      file;   /* source filename (matched by suffix) */
    Bld_CompileFlags flags;  /* non-zero fields override target defaults */
} Bld_FileOverride;

typedef BLD_DA(Bld_FileOverride) Bld_FileOverrideList;

typedef struct {
    const char*  name;
    Bld_Target*  exe;
    const char*  working_dir;
    Bld_Strs     args;
    int          timeout;  /* seconds, 0 = no timeout */
} Bld_TestEntry;

typedef BLD_DA(Bld_TestEntry) Bld_TestList;

struct Bld_Target {
    Bld_TargetKind kind;
    const char*    name;
    const char*    desc;
    bool           found;   /* true for internal targets, false if find_pkg failed */
    Bld_Step*      entry;   /* no-op input port */
    Bld_Step*      exit;    /* output port (final artifact) */
    Bld_LazyPathList     include_dirs;
    Bld_TargetList       link_deps;        /* public transitive link dependencies */
    Bld_TargetList       resolved_link_deps; /* filled by resolve, used by render */
    Bld_FileOverrideList file_overrides;   /* per-file compile flag overrides */
    Bld_LazyPathList     lazy_sources;     /* generated sources (added via bld_add_source) */
};

/* ===== Opts structs (for compound-literal macros) ===== */

/* BLD_PATHS and BLD_STRS macros defined above in typed wrappers section */

typedef struct {
    const char*      name;
    const char*      desc;
    const char*      output_name; /* override output filename (default: name) */
    Bld_Paths        sources;
    Bld_Lang         lang;       /* BLD_LANG_AUTO (0): per-file by extension */
    Bld_CompileFlags compile;
    Bld_CompileFlags compile_pub; /* public: propagated to link_with consumers */
    Bld_LinkFlags    link;
    Bld_LinkFlags    link_pub;    /* public: propagated to link_with consumers */
    Bld_Toolchain*   toolchain;  /* NULL = use Bld default */
} Bld_ExeOpts;

typedef struct {
    const char*      name;
    const char*      desc;
    const char*      lib_basename; /* override lib filename base (default: name), toolchain adds prefix/suffix */
    Bld_Paths        sources;
    Bld_Lang         lang;       /* BLD_LANG_AUTO (0): per-file by extension */
    Bld_CompileFlags compile;           /* private: for compiling this lib's sources */
    Bld_CompileFlags compile_pub; /* public: propagated to targets that link_with this lib */
    Bld_LinkFlags    link;
    Bld_LinkFlags    link_pub;    /* public: propagated to consumers via link_with */
    bool             shared;
    Bld_Toolchain*   toolchain;  /* NULL = use Bld default */
} Bld_LibOpts;

typedef struct {
    const char*    name;
    const char*    desc;
    const char*    working_dir;
    Bld_Strs       args;
} Bld_RunOpts;

typedef struct {
    const char*    name;
    const char*    desc;
    Bld_ActionFn   action;
    void*          action_ctx;
    Bld_HashFn     hash_fn;
    void*          hash_fn_ctx;
    bool           has_depfile;
    bool           content_hash; /* use output content hash for early cutoff (default for steps) */
    Bld_Paths      watch;        /* files to hash for cache invalidation */
} Bld_StepOpts;

typedef struct {
    const char*    name;
    const char*    desc;
    const char*    cmd;           /* shell command to execute */
    Bld_Paths      watch;        /* files to hash for cache */
} Bld_CmdOpts;

/* ===== Exe, Lib — "derived" from Target ===== */

typedef struct {
    Bld_Target        target;     /* MUST be first */
    Bld_LibOpts       opts;
    Bld_StepList      obj_steps;
    Bld_Step*         publish_step;
    Bld_Toolchain*    toolchain;  /* resolved: opts.toolchain ?: b->toolchain */
} Bld_Lib;

typedef BLD_DA(Bld_Lib*) Bld_LibList;

typedef struct {
    Bld_Target          target;     /* MUST be first */
    Bld_ExeOpts         opts;
    Bld_StepList        obj_steps;
    Bld_LibList         shared_libs;
    Bld_Toolchain*      toolchain;  /* resolved: opts.toolchain ?: b->toolchain */
} Bld_Exe;

/* ===== Package target (external dependency) ===== */

typedef struct {
    Bld_Target       target;       /* MUST be first */
    Bld_CompileFlags compile_pub;
    Bld_LinkFlags    link_pub;
} Bld_Pkg;

typedef struct {
    const char*      name;
    Bld_Paths        include_dirs;
    Bld_Paths        system_include_dirs;
    Bld_Strs         libs;
    Bld_Paths        lib_dirs;
    const char*      extra_cflags;
    const char*      extra_ldflags;
} Bld_PkgOpts;

/* ===== Bld context ===== */

typedef enum { BLD_MODE_DEFAULT = 0, BLD_MODE_DEBUG, BLD_MODE_RELEASE } Bld_BuildMode;

typedef struct {
    const char* key;
    const char* value;   /* NULL for flag-only (-Dfoo with no =) */
    bool        used;
} Bld_UserOption;

typedef struct {
    const char* name;
    const char* description;
    const char* default_val;  /* "on"/"off" for bool, string value for string */
    enum { BLD_OPT_TYPE_BOOL, BLD_OPT_TYPE_STRING } type;
} Bld_AvailableOption;

typedef BLD_DA(Bld_UserOption)      Bld_UserOptionList;
typedef BLD_DA(Bld_AvailableOption) Bld_AvailableOptionList;

typedef struct {
    bool          verbose;
    bool          silent;
    bool          show_cached;
    bool          show_help;
    bool          keep_going;
    int           max_jobs;
    Bld_BuildMode mode;
    const char*   install_prefix;
    Bld_Strs      targets;       /* requested targets */
    Bld_Strs      passthrough;   /* args after -- */
} Bld_Settings;

typedef struct {
    int          argc;
    char**       argv;
    Bld_Settings settings;

    Bld_Path root;
    Bld_Path cache;
    Bld_Path out;

    Bld_Toolchain* toolchain;
    Bld_Optimize   global_optimize;
    bool           global_warnings;
    Bld_Strs       global_defines;    /* applied to all targets and checks */
    Bld_BuildFlags build_flags;

    Bld_Target*  target_default;
    Bld_StepList        all_steps;
    Bld_TargetList      all_targets;

    /* user options (-D) */
    Bld_UserOptionList      user_options;
    Bld_AvailableOptionList avail_options;

    /* tests */
    Bld_TestList tests;

    /* build stats (filled by bld__run_build) */
    uint64_t steps_executed;
    uint64_t steps_cached;
    uint64_t steps_failed;
    uint64_t steps_skipped;
    uint64_t progress_current;
    uint64_t progress_total;
} Bld;

static inline Bld_Compiler* bld_compiler(Bld* b, Bld_Lang lang) {
    assert(lang >= BLD_LANG_C && lang < BLD_LANG__COUNT);
    return &b->toolchain->compilers[lang - 1];
}


/* ===== Compiler setter API ===== */

typedef struct { const char* driver; Bld_CStd standard; } Bld_CCompilerOpts;
typedef struct { const char* driver; Bld_CxxStd standard; } Bld_CxxCompilerOpts;
typedef struct { const char* driver; } Bld_AsmCompilerOpts;

void bld__set_compiler_c(Bld* b, const Bld_CCompilerOpts* opts);
void bld__set_compiler_cxx(Bld* b, const Bld_CxxCompilerOpts* opts);
void bld__set_compiler_asm(Bld* b, const Bld_AsmCompilerOpts* opts);

#define bld_set_compiler_c(b, ...)   bld__set_compiler_c((b), &(Bld_CCompilerOpts){__VA_ARGS__})
#define bld_set_compiler_cxx(b, ...) bld__set_compiler_cxx((b), &(Bld_CxxCompilerOpts){__VA_ARGS__})
#define bld_set_compiler_asm(b, ...) bld__set_compiler_asm((b), &(Bld_AsmCompilerOpts){__VA_ARGS__})

/* ===== API ===== */

/* create targets — all return Bld_Target* */
Bld_Target* bld__add_exe(Bld* b, const Bld_ExeOpts* opts);
Bld_Target* bld__add_lib(Bld* b, const Bld_LibOpts* opts);
Bld_Target* bld__add_step(Bld* b, const Bld_StepOpts* opts);
Bld_Target* bld__add_cmd(Bld* b, const Bld_CmdOpts* opts);
Bld_Target* bld__add_run(Bld* b, Bld_Target* target, const Bld_RunOpts* opts);

#define bld_add_exe(b, ...)       bld__add_exe((b), &(Bld_ExeOpts){__VA_ARGS__})
#define bld_add_lib(b, ...)       bld__add_lib((b), &(Bld_LibOpts){__VA_ARGS__})
#define bld_add_step(b, ...)      bld__add_step((b), &(Bld_StepOpts){__VA_ARGS__})
#define bld_add_cmd(b, ...)       bld__add_cmd((b), &(Bld_CmdOpts){__VA_ARGS__})
#define bld_add_run(b, tgt, ...)  bld__add_run((b), (tgt), &(Bld_RunOpts){__VA_ARGS__})

/* dependency: pure ordering */
void bld_depends_on(Bld_Target* a, Bld_Target* b);

/* link: ordering + pass artifact to linker */
void bld_link_with(Bld_Target* a, Bld_Target* b);

/* lazy path from target output */
Bld_LazyPath bld_output(Bld_Target* t);
Bld_LazyPath bld_output_sub(Bld_Target* t, const char* subpath);

/* add include dir (LazyPath for generated paths) */
void bld_add_include_dir(Bld_Target* t, Bld_LazyPath dir);

/* add generated source (from codegen step output) */
void bld_add_source(Bld_Target* t, Bld_LazyPath src);

/* install — dst paths are relative to --prefix (default: build/)
 *   bld_install_exe   → <prefix>/bin/<name>
 *   bld_install_lib   → <prefix>/lib/<libname>
 *   bld_install       → <prefix>/<dst>
 *   bld_install_files → copies each file into <prefix>/<dst>/
 *   bld_install_dir   → copies directory tree into <prefix>/<dst>/
 */
Bld_Target* bld_install_exe(Bld* b, Bld_Target* exe);
Bld_Target* bld_install_lib(Bld* b, Bld_Target* lib);
Bld_Target* bld_install(Bld* b, Bld_Target* target, Bld_Path dst);
Bld_Target* bld_install_files(Bld* b, Bld_Paths files, Bld_Path dst);
Bld_Target* bld_install_dir(Bld* b, const char* src_dir, Bld_Path dst);

/* package target (manual external dependency) */
Bld_Target* bld__add_pkg(Bld* b, const Bld_PkgOpts* opts);
#define bld_pkg(b, ...) bld__add_pkg((b), &(Bld_PkgOpts){__VA_ARGS__})

/* external dependencies — bld_find_pkg declared in bld_dep.h */

/* per-file compile flag override (non-zero fields override target defaults) */
void bld__override_file(Bld_Target* t, const char* file, const Bld_CompileFlags* flags);
#define bld_override_file(t, file, ...) bld__override_file((t), (file), &(Bld_CompileFlags){__VA_ARGS__})

/* deep clone (arena-allocated, safe to modify fields independently) */
Bld_CompileFlags bld_clone_compile_flags(Bld_CompileFlags f);

/* build mode defaults */
Bld_CompileFlags bld_default_compile_flags(Bld* b);
/* default_link_flags removed — link flags no longer carry build-global toggles */

/* child build context (shares cache with parent, for feature checks / subbuilds) */
Bld* bld_new(Bld* parent);
void bld_execute(Bld* b);
bool bld_target_ok(Bld_Target* t);
Bld_Path bld_target_artifact(Bld* b, Bld_Target* t);

/* feature detection checks — declared in bld_checks.h */

/* test registration */
Bld_Target* bld__add_test(Bld* b, Bld_Target* exe, const Bld_RunOpts* opts);
#define bld_add_test(b, tgt, ...) bld__add_test((b), (tgt), &(Bld_RunOpts){__VA_ARGS__})

/* user-defined build options (-D) */
bool        bld_option_bool(Bld* b, const char* name, const char* desc, bool default_val);
const char* bld_option_str(Bld* b, const char* name, const char* desc, const char* default_val);

/*
 * bld_recompile_cmd — command to recompile this build script.
 * Define in your build.c via BLD_RECOMPILE_CMD macro:
 *
 *   BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")
 *
 * bld appends " -o <output>" automatically. If you link against libbld.a:
 *
 *   BLD_RECOMPILE_CMD("cc -I/prefix/include build.c -L/prefix/lib -lbld -lpthread")
 */
extern const char* bld_recompile_cmd;
#define BLD_RECOMPILE_CMD(cmd) const char* bld_recompile_cmd = (cmd);

/* user must define this */
void configure(Bld* b);
