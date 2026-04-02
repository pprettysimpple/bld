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
  #error "Windows support is not yet implemented"
#else
  #include <dirent.h>
  #include <fcntl.h>
  #include <fnmatch.h>
  #include <pthread.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <sys/wait.h>
  #include <unistd.h>
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
    pthread_mutex_t mutex;
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

#define bld_path(literal) ((Bld_Path){(literal)})

Bld_Path    bld_path_join(Bld_Path a, Bld_Path b);
Bld_Path    bld_path_parent(Bld_Path p);
const char* bld_path_filename(Bld_Path p);
const char* bld_path_ext(Bld_Path p);
Bld_Path    bld_path_replace_ext(Bld_Path p, const char* ext);
Bld_Path    bld_path_fmt(const char* fmt, ...);

typedef BLD_DA(Bld_Path)       Bld_PathList;
typedef BLD_DA(const char*)    Bld_Strings;

Bld_Strings bld_str_lines(const char* s);
const char* bld_str_join(const Bld_Strings* parts, const char* sep);

/* ===== Hash ===== */

typedef struct { uint64_t value; } Bld_Hash;

Bld_Hash bld_hash_combine(Bld_Hash a, Bld_Hash b);
Bld_Hash bld_hash_combine_unordered(Bld_Hash a, Bld_Hash b);
Bld_Hash bld_hash_str(const char* s);
Bld_Hash bld_hash_file(Bld_Path p);
Bld_Hash bld_hash_dir(Bld_Path dir);

/* ===== Log ===== */

void bld_log(const char* fmt, ...);
void bld_panic(const char* fmt, ...) __attribute__((noreturn));

/* structured log functions (color-aware, thread-safe) */
void bld_log_progress(uint64_t current, uint64_t total, const char* name);
void bld_log_cached(const char* name);
void bld_log_done(uint64_t executed, uint64_t cached, uint64_t failed, uint64_t skipped, double elapsed, size_t arena_used);
void bld_log_action(const char* fmt, ...);  /* compile/link commands (-v) */
void bld_log_info(const char* fmt, ...);    /* [*] info messages */

/* ===== Filesystem ===== */

int      bld_fs_exists(Bld_Path p);
int      bld_fs_is_dir(Bld_Path p);
int      bld_fs_is_file(Bld_Path p);
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

/* glob: returns NULL-terminated array of matching file paths */
const char** bld_files_glob(const char* pattern);
/* exclude paths from a NULL-terminated array, returns new array */
const char** bld_files_exclude(const char** files, const char** exclude);
/* merge two NULL-terminated arrays, returns new array */
const char** bld_files_merge(const char** a, const char** b);

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
    int         available;
    union {
        struct { Bld_CStd standard; } c;
        struct { Bld_CxxStd standard; } cxx;
        struct { int _pad; } as;
    };
} Bld_Compiler;

typedef enum { BLD_UNSET = 0, BLD_ON, BLD_OFF } Bld_Toggle;

/* ===== Step — low-level graph node ===== */

typedef struct Bld_Step Bld_Step;
typedef struct Bld_Target Bld_Target;

typedef enum { BLD_ACTION_OK = 0, BLD_ACTION_FAILED = 1 } Bld_ActionResult;
typedef Bld_ActionResult (*Bld_ActionFn)(void* ctx, Bld_Path output, Bld_Path depfile);
typedef Bld_Hash (*Bld_RecipeHashFn)(void* ctx, Bld_Hash current);

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
    int          silent;
    int          phony;      /* always execute, never cache */

    Bld_StepList deps;      /* ordering dependencies */
    Bld_StepList inputs;    /* data dependencies (step outputs used by action) */

    Bld_ActionFn     action;
    void*            action_ctx;
    Bld_RecipeHashFn hash_fn;
    void*            hash_fn_ctx;

    int has_depfile;
    int content_hash;  /* use output content hash instead of recipe hash for downstream */

    /* computed at build time */
    Bld_Hash input_hash;
    Bld_Hash cache_key;
    int           hash_valid;
    Bld_StepState state;

    pthread_mutex_t mutex;
    pthread_cond_t  cond;
};

/* ===== LazyPath ===== */

typedef struct {
    Bld_Target* source;
    Bld_Path    path;
} Bld_LazyPath;

/* ===== External dependency bundle ===== */

typedef struct {
    const char*  name;                /* package name (for error messages) */
    int          found;               /* 1 = found, 0 = not found */
    const char** include_dirs;        /* -I, NULL-terminated */
    const char** system_include_dirs; /* -isystem, NULL-terminated */
    const char** libs;                /* -l names ("ssl", "crypto"), NULL-terminated */
    const char** lib_dirs;            /* -L paths, NULL-terminated */
    const char*  extra_cflags;        /* raw extra compile flags */
    const char*  extra_ldflags;       /* raw extra link flags */
} Bld_Dep;

/* ===== Compiler/linker flags ===== */

typedef struct {
    Bld_Optimize   optimize;
    Bld_Toggle     warnings;
    const char*    extra_flags;
    const char**   defines;             /* NULL-terminated */
    const char**   include_dirs;        /* NULL-terminated, -I */
    const char**   system_include_dirs; /* NULL-terminated, -isystem */
} Bld_CompileFlags;

typedef struct {
    Bld_Toggle asan;
    Bld_Toggle debug_info;
    Bld_Toggle lto;
    const char* extra_flags;   /* appended after objects, e.g. "-lm -ldl" */
} Bld_LinkFlags;

typedef BLD_DA(Bld_LazyPath) Bld_LazyPathList;
typedef BLD_DA(Bld_Dep*)    Bld_DepList;

/* ===== Target — high-level subgraph ===== */

typedef enum {
    BLD_TGT_EXE,
    BLD_TGT_LIB,
    BLD_TGT_CUSTOM,
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
    const char** args;
    int          timeout;  /* seconds, 0 = no timeout */
} Bld_TestEntry;

typedef BLD_DA(Bld_TestEntry) Bld_TestList;

struct Bld_Target {
    Bld_TargetKind kind;
    const char*    name;
    const char*    desc;
    Bld_Step*      entry;   /* no-op input port */
    Bld_Step*      exit;    /* output port (final artifact) */
    Bld_LazyPathList     include_dirs;
    Bld_TargetList       link_deps;        /* public transitive link dependencies */
    Bld_DepList          ext_deps;         /* external deps (compile: private, link: transitive) */
    Bld_FileOverrideList file_overrides;   /* per-file compile flag overrides */
    Bld_LazyPathList     lazy_sources;     /* generated sources (added via bld_add_source) */
};

/* ===== Opts structs (for compound-literal macros) ===== */

#define BLD_PATHS(...) ((const char*[]){__VA_ARGS__, NULL})
#define BLD_DEFS(...)  ((const char*[]){__VA_ARGS__, NULL})

typedef struct {
    const char*      name;
    const char*      desc;
    const char*      output_name; /* override output filename (default: name) */
    const char**     sources;
    Bld_Lang         lang;       /* BLD_LANG_AUTO (0): per-file by extension */
    Bld_CompileFlags compile;
    Bld_LinkFlags    link;
} Bld_ExeOpts;

typedef struct {
    const char*      name;
    const char*      desc;
    const char*      output_name; /* override lib filename base (default: name) */
    const char**     sources;
    Bld_Lang         lang;       /* BLD_LANG_AUTO (0): per-file by extension */
    Bld_CompileFlags compile;
    Bld_LinkFlags    link;
    int              shared;  /* 0 = static (default), 1 = shared */
} Bld_LibOpts;

typedef struct {
    const char*    name;
    const char*    desc;
    const char*    working_dir;
    const char**   args;
} Bld_RunOpts;

typedef struct {
    const char*    name;
    const char*    desc;
    Bld_ActionFn   action;
    void*          action_ctx;
    int            has_depfile;
    const char**   watch;      /* NULL-terminated list of files to hash for cache invalidation */
} Bld_StepOpts;

/* ===== Exe, Lib — "derived" from Target ===== */

typedef struct {
    Bld_Target        target;     /* MUST be first */
    Bld_LibOpts       opts;
    Bld_StepList      obj_steps;
    Bld_Step*         publish_step; /* copies .so to out/lib/ for exe linking */
} Bld_Lib;

typedef BLD_DA(Bld_Lib*) Bld_LibList;

typedef struct {
    Bld_Target          target;     /* MUST be first */
    Bld_ExeOpts         opts;
    Bld_StepList        obj_steps;
    Bld_LibList         shared_libs;
    Bld_DepList         resolved_ext_deps;
} Bld_Exe;

/* ===== Bld context ===== */

typedef enum { BLD_MODE_DEFAULT = 0, BLD_MODE_DEBUG, BLD_MODE_RELEASE } Bld_BuildMode;

typedef struct {
    const char* key;
    const char* value;   /* NULL for flag-only (-Dfoo with no =) */
    int         used;
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
    int           verbose;
    int           silent;
    int           show_cached;
    int           show_help;
    int           keep_going;
    int           max_jobs;
    Bld_BuildMode mode;
    const char*   install_prefix;
    const char**  targets;       /* NULL-terminated requested targets */
    const char**  passthrough;   /* NULL-terminated args after -- */
} Bld_Settings;

typedef struct {
    int          argc;
    char**       argv;
    Bld_Settings settings;

    Bld_Path root;
    Bld_Path cache;
    Bld_Path out;

    Bld_Compiler compilers[BLD_LANG__COUNT - 1]; /* [0]=C, [1]=CXX, [2]=ASM */
    Bld_Optimize   global_optimize;
    int            global_warnings;
    Bld_LinkFlags  global_link;

    const char* static_link_tool;

    Bld_Target*  install_target;
    Bld_Target*  build_all_target;
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
    return &b->compilers[lang - 1];
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
Bld_Target* bld__add_run(Bld* b, Bld_Target* target, const Bld_RunOpts* opts);

#define bld_add_exe(b, ...)       bld__add_exe((b), &(Bld_ExeOpts){__VA_ARGS__})
#define bld_add_lib(b, ...)       bld__add_lib((b), &(Bld_LibOpts){__VA_ARGS__})
#define bld_add_step(b, ...)      bld__add_step((b), &(Bld_StepOpts){__VA_ARGS__})
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

/* install */
Bld_Target* bld_install_exe(Bld* b, Bld_Target* exe);
Bld_Target* bld_install_lib(Bld* b, Bld_Target* lib);
Bld_Target* bld_install(Bld* b, Bld_Target* target, Bld_Path dst);

/* external dependencies */
Bld_Dep* bld__dep(const Bld_Dep* d);
Bld_Dep* bld_find_pkg(const char* name);
void     bld_use_dep(Bld_Target* t, Bld_Dep* dep);

#define bld_dep(...) bld__dep(&(Bld_Dep){__VA_ARGS__})

/* per-file compile flag override (non-zero fields override target defaults) */
void bld__override_file(Bld_Target* t, const char* file, const Bld_CompileFlags* flags);
#define bld_override_file(t, file, ...) bld__override_file((t), (file), &(Bld_CompileFlags){__VA_ARGS__})

/* deep clone (arena-allocated, safe to modify fields independently) */
Bld_CompileFlags bld_clone_compile_flags(Bld_CompileFlags f);

/* build mode defaults */
Bld_CompileFlags bld_default_compile_flags(Bld* b);
Bld_LinkFlags    bld_default_link_flags(Bld* b);

/* child build context (shares cache with parent, for feature checks / subbuilds) */
Bld* bld_new(Bld* parent);
void bld_execute(Bld* b);
int  bld_target_ok(Bld_Target* t);
Bld_Path bld_target_artifact(Bld* b, Bld_Target* t);

/* feature detection checks */
typedef struct Bld_Checks Bld_Checks;
Bld_Checks* bld_checks_new(Bld* parent);
bool* bld_checks_header(Bld_Checks* c, const char* define_name, const char* header);
bool* bld_checks_func(Bld_Checks* c, const char* define_name, const char* func, const char* header);
int*  bld_checks_sizeof(Bld_Checks* c, const char* define_name, const char* type);
bool* bld_checks_compile(Bld_Checks* c, const char* define_name, const char* source);
void  bld_checks_run(Bld_Checks* c);
void  bld_checks_write(Bld_Checks* c, const char* path);

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

