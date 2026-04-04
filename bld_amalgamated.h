/*
 * # bld.h API Reference
 * 
 * > Single-header C build system. Write build.c, include bld.h, implement configure().
 * 
 * ## Quick Start
 * 
 * Every build script follows this pattern:
 * 
 * ```c
 * #define BLD_IMPLEMENTATION
 * #define BLD_STRIP_PREFIX   // optional: use add_exe instead of bld_add_exe
 * #include "bld.h"
 * 
 * BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")
 * 
 * void configure(Bld* b) {
 *     // define targets here
 * }
 * ```
 * 
 * Bootstrap and run:
 * 
 *     cc -std=c11 -w build.c -o b -lpthread
 *     ./b build              # build and install all targets
 *     ./b test               # run registered tests
 *     ./b build --prefix /usr/local  # custom install prefix
 *     ./b build -v           # verbose: show compiler commands
 *     ./b build -Dfoo=on     # pass user option
 *     ./b --help             # list targets and options
 * 
 * bld auto-recompiles itself when build.c changes.
 * 
 * ## Targets
 * 
 * ### Executable
 * 
 * ```c
 * Bld_Target* exe = bld_add_exe(b,
 *     .name    = "myapp",
 *     .desc    = "My application",
 *     .sources = BLD_PATHS("src/main.c", "src/util.c"),
 *     .compile = { .optimize = BLD_OPT_O2, .warnings = BLD_ON },
 *     .link    = { .libs = BLD_STRS("m", "pthread") });
 * ```
 * 
 * ### Static Library
 * 
 * ```c
 * Bld_Target* lib = bld_add_lib(b,
 *     .name    = "mylib",
 *     .sources = BLD_PATHS("src/foo.c", "src/bar.c"),
 *     .compile           = { .optimize = BLD_OPT_O2 },
 *     .compile_propagate = { .include_dirs = BLD_PATHS("include") });
 * ```
 * 
 * `compile` flags apply when compiling this lib's sources.
 * `compile_propagate` flags propagate to any target that calls `bld_link_with(target, lib)`.
 * 
 * ### Shared Library
 * 
 * ```c
 * Bld_Target* so = bld_add_lib(b,
 *     .name    = "mylib",
 *     .shared  = true,
 *     .sources = BLD_PATHS("src/foo.c", "src/bar.c"));
 * ```
 * 
 * ### Exe Opts Fields
 * 
 * - `.name` (required) — target identifier, used in CLI and progress output
 * - `.desc` — shown in `--help`
 * - `.output_name` — override output filename (default: `.name`)
 * - `.sources` — `Bld_Paths` of source files
 * - `.lang` — force language: `BLD_LANG_C`, `BLD_LANG_CXX`, `BLD_LANG_ASM`, or `BLD_LANG_AUTO` (default, detect by extension)
 * - `.compile` — `Bld_CompileFlags` (private compile flags)
 * - `.compile_propagate` — `Bld_CompileFlags` (public, propagated via `link_with`)
 * - `.link` — `Bld_LinkFlags` (private link flags)
 * - `.link_propagate` — `Bld_LinkFlags` (public, propagated via `link_with`)
 * - `.toolchain` — override per-target toolchain (default: `b->toolchain`)
 * 
 * ### Lib Opts Fields
 * 
 * Same as Exe, except:
 * - `.lib_basename` — override library filename base (default: `.name`).
 *   Toolchain adds prefix/suffix automatically (`lib` + `.a`/`.so`/`.dylib`).
 *   Example: `.lib_basename = "z"` produces `libz.a`.
 * - `.compile_propagate` — public compile flags, propagated to consumers via `link_with`
 * - `.link_propagate` — public link flags, propagated to consumers via `link_with`
 * - `.shared` — `true` for shared library, `false` (default) for static
 * 
 * ## Source Files
 * 
 * ### Literal Lists
 * 
 * ```c
 * .sources = BLD_PATHS("src/main.c", "src/util.c")
 * ```
 * 
 * `BLD_PATHS` and `BLD_STRS` are arena-allocated — safe to store and use later.
 * 
 * ### Globbing
 * 
 * ```c
 * Bld_Paths all = bld_files_glob("src/*.c");
 * Bld_Paths filtered = bld_files_exclude(all, BLD_PATHS("src/test_main.c"));
 * Bld_Paths combined = bld_files_merge(lib_srcs, extra_srcs);
 * ```
 * 
 * `bld_files_glob` supports `*` wildcards (not recursive `**`).
 * 
 * ### Dynamic Construction
 * 
 * ```c
 * Bld_Paths srcs = {0};
 * bld_paths_push(&srcs, "src/main.c");
 * bld_paths_push(&srcs, "src/util.c");
 * if (use_ssl) bld_paths_push(&srcs, "src/ssl.c");
 * ```
 * 
 * ## Typed Slices: Bld_Strs and Bld_Paths
 * 
 * Two sized slice types for strings and paths:
 * 
 * ```c
 * Bld_Strs  — for defines, library names, arguments
 * Bld_Paths — for file paths, directory paths
 * 
 * // literal (arena-allocated, safe to store)
 * BLD_STRS("FOO=1", "BAR=2")
 * BLD_PATHS("src/a.c", "src/b.c")
 * 
 * // dynamic (arena-backed, growable)
 * Bld_Strs defs = {0};
 * bld_strs_push(&defs, "FOO=1");
 * 
 * Bld_Paths dirs = {0};
 * bld_paths_push(&dirs, "/usr/include");
 * 
 * // merge two slices
 * Bld_Strs  all_defs  = bld_strs_merge(a, b);
 * Bld_Paths all_paths = bld_paths_merge(a, b);
 * ```
 * 
 * All slices are arena-backed with `{ .items, .len, .cap }`.
 * 
 * ## Compile Flags
 * 
 * ```c
 * Bld_CompileFlags flags = {
 *     .optimize    = BLD_OPT_O2,       // O0, O1, O2, O3, Os, OFAST
 *     .warnings    = BLD_ON,           // ON, OFF, or UNSET (inherit global)
 *     .debug_info  = BLD_ON,           // -g
 *     .extra_flags = "-fno-strict-aliasing",
 *     .defines          = BLD_STRS("VERSION=2", "DEBUG"),
 *     .include_dirs     = BLD_PATHS("src", "vendor"),
 *     .system_include_dirs = BLD_PATHS("/opt/sdk/include"),
 * };
 * ```
 * 
 * ### Reusing and Modifying Flags
 * 
 * ```c
 * Bld_CompileFlags base = bld_default_compile_flags(b);  // mode-aware defaults
 * base.optimize = BLD_OPT_O3;
 * 
 * Bld_CompileFlags copy = bld_clone_compile_flags(base);  // deep copy
 * copy.include_dirs = BLD_PATHS("extra");
 * ```
 * 
 * `bld_default_compile_flags` returns sensible defaults based on build mode:
 * - `--release`: O2, warnings on
 * - `--debug`: O0, debug_info on, warnings on
 * - default: O0, warnings on
 * 
 * ## Link Flags
 * 
 * ```c
 * Bld_LinkFlags lflags = {
 *     .libs        = BLD_STRS("ssl", "crypto"),   // -l names
 *     .lib_dirs    = BLD_PATHS("/opt/openssl/lib"), // -L paths
 *     .extra_flags = "-Wl,--as-needed",
 * };
 * ```
 * 
 * ## Build Flags (Global)
 * 
 * ```c
 * b->build_flags.asan = BLD_ON;   // -fsanitize=address for all targets
 * b->build_flags.lto  = BLD_ON;   // -flto for all targets
 * ```
 * 
 * These apply globally to all compile and link commands.
 * 
 * ## Compiler Setup
 * 
 * ```c
 * bld_set_compiler_c(b,   .driver = "gcc",   .standard = BLD_C_11);
 * bld_set_compiler_cxx(b, .driver = "g++",   .standard = BLD_CXX_17);
 * bld_set_compiler_asm(b, .driver = "nasm");
 * ```
 * 
 * If `.driver` is omitted, bld auto-detects from PATH.
 * Standards: `BLD_C_90/99/11/17/23`, `BLD_C_GNU90/99/11/17/23`,
 * `BLD_CXX_11/14/17/20/23`, `BLD_CXX_GNU11/14/17/20/23`.
 * 
 * ## Linking Targets Together
 * 
 * ```c
 * bld_link_with(exe, lib);       // link lib into exe + propagate compile/link_propagate
 * bld_depends_on(gen, codegen);  // ordering only, no artifact passing
 * ```
 * 
 * `bld_link_with` is transitive: if A links B and B links C, A gets C's propagated flags.
 * 
 * ## External Dependencies
 * 
 * ### pkg-config
 * 
 * ```c
 * Bld_Dep* ssl = bld_find_pkg("openssl");
 * if (ssl->found) {
 *     bld_use_dep(lib, ssl);   // adds include dirs, -l flags, -L flags
 * }
 * ```
 * 
 * ### Manual
 * 
 * ```c
 * Bld_Dep* my = bld_dep(
 *     .name         = "mylib",
 *     .include_dirs = BLD_PATHS("/opt/mylib/include"),
 *     .libs         = BLD_STRS("mylib"),
 *     .lib_dirs     = BLD_PATHS("/opt/mylib/lib"));
 * bld_use_dep(target, my);
 * ```
 * 
 * `bld_use_dep` applies compile flags (include dirs) privately and link flags transitively.
 * 
 * ## Feature Detection
 * 
 * ```c
 * Bld_Checks* chk = bld_checks_new(b);
 * 
 * // header existence
 * bool* has_unistd  = bld_checks_header(chk, "HAVE_UNISTD_H", "unistd.h");
 * bool* has_windows = bld_checks_header(chk, "HAVE_WINDOWS_H", "windows.h");
 * 
 * // function existence (provide header that declares it)
 * bool* has_strtoll = bld_checks_func(chk, "HAVE_STRTOLL", "strtoll", "stdlib.h");
 * 
 * // type sizes (cross-compilation safe: binary scan, no execution)
 * int* sz_long = bld_checks_sizeof(chk, "SIZEOF_LONG", "long");
 * int* sz_ptr  = bld_checks_sizeof(chk, "SIZEOF_VOID_P", "void*");
 * 
 * // arbitrary compile test
 * bool* has_builtin = bld_checks_compile(chk, "HAS_BUILTIN_EXPECT",
 *     "int main(){return __builtin_expect(0,0);}");
 * 
 * // execute all checks in parallel
 * bld_checks_run(chk);
 * 
 * // now pointers are valid:
 * if (*has_unistd) { // ... }
 * int long_size = *sz_long;  // e.g. 8
 * 
 * // write results as #define / #undef to a header
 * bld_checks_write(chk, "generated/config.h");
 * // produces:
 * //   #define HAVE_UNISTD_H 1
 * //   #undef  HAVE_WINDOWS_H
 * //   #define SIZEOF_LONG 8
 * ```
 * 
 * ## Platform Detection
 * 
 * Use `b->toolchain->os` to branch on the target platform:
 * 
 * ```c
 * if (b->toolchain->os == BLD_OS_LINUX) {
 *     bld_paths_push(&srcs, "src/platform_linux.c");
 * } else if (b->toolchain->os == BLD_OS_MACOS) {
 *     bld_paths_push(&srcs, "src/platform_macos.c");
 * }
 * ```
 * 
 * Available: `BLD_OS_LINUX`, `BLD_OS_MACOS`, `BLD_OS_WINDOWS`, `BLD_OS_FREEBSD`.
 * 
 * ## Installation
 * 
 * ```c
 * bld_install_exe(b, exe);     // installs to <prefix>/bin/
 * bld_install_lib(b, lib);     // installs to <prefix>/lib/
 * 
 * // install specific files to a subpath under prefix
 * bld_install_files(b, BLD_PATHS("include/mylib.h"), bld_path("include"));
 * 
 * // install entire directory tree
 * bld_install_dir(b, "doc", bld_path("share/doc/mylib"));
 * 
 * // install any target artifact to custom path
 * bld_install(b, codegen_target, bld_path("share/myapp"));
 * ```
 * 
 * `./b build` compiles, links, and installs everything.
 * Default install prefix is `./build/` (relative to project root, not `/usr/local`).
 * Override with `--prefix`: `./b build --prefix /usr/local`
 * 
 * ## Tests
 * 
 * ```c
 * bld_add_test(b, exe_target,
 *     .name = "smoke-test",
 *     .desc = "Verify basic functionality",
 *     .args = BLD_STRS("--self-test"),
 *     .timeout = 30);  // seconds, 0 = no timeout
 * ```
 * 
 * CLI: `./b test`
 * 
 * ## User Options
 * 
 * ```c
 * bool use_ssl = bld_option_bool(b, "ssl", "Enable SSL support", true);
 * const char* backend = bld_option_str(b, "backend", "TLS backend", "openssl");
 * ```
 * 
 * CLI: `./b build -Dssl=off -Dbackend=wolfssl`
 * Options appear in `./b --help`.
 * 
 * ## Per-file Compile Flag Overrides
 * 
 * ```c
 * bld_override_file(lib, "third_party/noisy.c", .warnings = BLD_OFF);
 * bld_override_file(lib, "hot_path.c", .optimize = BLD_OPT_O3);
 * ```
 * 
 * Non-zero fields override the target's compile flags for that specific file.
 * 
 * ## Custom Steps
 * 
 * ### Action Function
 * 
 * ```c
 * static Bld_ActionResult my_codegen(void* ctx, Bld_Path output, Bld_Path depfile) {
 *     (void)depfile;
 *     Bld* b = ctx;
 *     // generate code, write to output directory
 *     bld_fs_mkdir_p(output);
 *     const char* code = "#include <stdio.h>\nint gen(void){return 42;}\n";
 *     bld_fs_write_file(bld_path_join(output, bld_path("gen.c")), code, strlen(code));
 *     return BLD_ACTION_OK;
 * }
 * 
 * void configure(Bld* b) {
 *     Bld_Target* gen = bld_add_step(b,
 *         .name   = "codegen",
 *         .desc   = "Generate source files",
 *         .action = my_codegen,
 *         .action_ctx = b,
 *         .watch  = BLD_PATHS("schema.json"));  // re-run when these change
 * 
 *     Bld_Target* exe = bld_add_exe(b, .name = "app", .sources = BLD_PATHS("main.c"));
 *     bld_add_source(exe, bld_output_sub(gen, "gen.c"));  // add generated source
 *     bld_add_include_dir(exe, bld_output(gen));           // add generated dir to -I
 * }
 * ```
 * 
 * ### Shell Command Step
 * 
 * ```c
 * Bld_Target* gen = bld_add_cmd(b,
 *     .name  = "protoc",
 *     .desc  = "Generate protobuf sources",
 *     .cmd   = "protoc --c_out=generated/ schema.proto",
 *     .watch = BLD_PATHS("schema.proto"));
 * ```
 * 
 * ### Running a Built Target
 * 
 * ```c
 * Bld_Target* tool = bld_add_exe(b, .name = "tool", .sources = BLD_PATHS("tool.c"));
 * Bld_Target* run  = bld_add_run(b, tool,
 *     .name = "run-tool",
 *     .desc = "Run the tool",
 *     .args = BLD_STRS("--generate", "output.c"));
 * ```
 * 
 * ## Toolchain
 * 
 * ```c
 * Bld_Toolchain* tc = bld_toolchain_gcc(BLD_OS_LINUX);
 * b->toolchain = tc;  // set as default for all targets
 * ```
 * 
 * OS targets: `BLD_OS_LINUX`, `BLD_OS_MACOS`, `BLD_OS_WINDOWS`, `BLD_OS_FREEBSD`.
 * 
 * Per-target override:
 * 
 * ```c
 * bld_add_exe(b, .name = "cross", .sources = ..., .toolchain = my_cross_tc);
 * ```
 * 
 * ## Filesystem Helpers
 * 
 * ```c
 * bld_fs_exists(bld_path("file.c"))     // returns bool
 * bld_fs_is_dir(bld_path("src"))
 * bld_fs_is_file(bld_path("main.c"))
 * bld_fs_mkdir_p(bld_path("out/gen"))   // recursive mkdir
 * bld_fs_copy_file(from, to)
 * bld_fs_copy_r(from, to)              // recursive copy
 * bld_fs_remove(path)
 * bld_fs_remove_all(path)              // recursive remove
 * bld_fs_rename(from, to)
 * bld_fs_read_file(path, &len)         // returns arena-allocated content
 * bld_fs_write_file(path, data, len)
 * bld_fs_realpath(path)
 * bld_fs_getcwd()
 * ```
 * 
 * ## String Helpers
 * 
 * ```c
 * bld_str_fmt("hello %s", name)   // arena-allocated sprintf
 * bld_str_dup("copy me")          // arena-allocated strdup
 * bld_str_cat("a", "b", "c", NULL)  // concatenate (NULL sentinel)
 * bld_str_lines(text)             // split into Bld_Strs by newline
 * bld_str_join(&strs, ", ")      // join Bld_Strs with separator
 * ```
 * 
 * ## Complete Example: Library + Executable + Tests
 * 
 * ```c
 * #define BLD_IMPLEMENTATION
 * #define BLD_STRIP_PREFIX
 * #include "bld.h"
 * 
 * BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")
 * 
 * void configure(Bld* b) {
 *     bld_set_compiler_c(b, .standard = BLD_C_11);
 * 
 *     Bld_CompileFlags cflags = bld_default_compile_flags(b);
 *     cflags.optimize = BLD_OPT_O2;
 * 
 *     // static library
 *     Bld_Paths lib_srcs = bld_files_glob("src/lib/*.c");
 *     Bld_Target* lib = bld_add_lib(b,
 *         .name              = "mylib",
 *         .sources           = lib_srcs,
 *         .compile           = cflags,
 *         .compile_propagate = { .include_dirs = BLD_PATHS("include") });
 * 
 *     // executable
 *     Bld_Target* app = bld_add_exe(b,
 *         .name    = "myapp",
 *         .sources = BLD_PATHS("src/main.c"),
 *         .compile = cflags,
 *         .link    = { .libs = BLD_STRS("m") });
 *     bld_link_with(app, lib);
 * 
 *     // test
 *     Bld_Target* test_exe = bld_add_exe(b,
 *         .name    = "test-runner",
 *         .sources = BLD_PATHS("tests/test_main.c"),
 *         .compile = cflags);
 *     bld_link_with(test_exe, lib);
 *     bld_add_test(b, test_exe, .name = "unit-tests");
 * 
 *     // install
 *     bld_install_exe(b, app);
 *     bld_install_lib(b, lib);
 *     bld_install_files(b, BLD_PATHS("include/mylib.h"), bld_path("include"));
 * }
 * ```
 * 
 * ## Complete Example: Feature Detection + External Deps
 * 
 * ```c
 * #define BLD_IMPLEMENTATION
 * #define BLD_STRIP_PREFIX
 * #include "bld.h"
 * 
 * BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")
 * 
 * void configure(Bld* b) {
 *     bld_set_compiler_c(b, .standard = BLD_C_99);
 * 
 *     // feature detection
 *     Bld_Checks* chk = bld_checks_new(b);
 *     bld_checks_header(chk, "HAVE_UNISTD_H", "unistd.h");
 *     bld_checks_header(chk, "HAVE_SYS_SOCKET_H", "sys/socket.h");
 *     bld_checks_func(chk, "HAVE_STRLCPY", "strlcpy", "string.h");
 *     int* sz_long = bld_checks_sizeof(chk, "SIZEOF_LONG", "long");
 *     bld_checks_run(chk);
 *     bld_checks_write(chk, "generated/config.h");
 * 
 *     // external deps
 *     Bld_Dep* ssl  = bld_find_pkg("openssl");
 *     Bld_Dep* zlib = bld_find_pkg("zlib");
 * 
 *     // library
 *     Bld_Paths srcs = bld_files_glob("lib/*.c");
 *     Bld_CompileFlags cf = bld_default_compile_flags(b);
 *     cf.defines = BLD_STRS("HAVE_CONFIG_H");
 *     cf.include_dirs = BLD_PATHS("include", "lib", "generated");
 * 
 *     Bld_Target* lib = bld_add_lib(b,
 *         .name    = "mynet",
 *         .sources = srcs,
 *         .compile = cf);
 *     if (ssl->found)  bld_use_dep(lib, ssl);
 *     if (zlib->found) bld_use_dep(lib, zlib);
 * 
 *     // executable
 *     Bld_Target* exe = bld_add_exe(b,
 *         .name    = "mynet-cli",
 *         .sources = BLD_PATHS("src/main.c"),
 *         .compile = cf,
 *         .link    = { .libs = BLD_STRS("m", "pthread") });
 *     bld_link_with(exe, lib);
 * }
 * ```
 */

/*  bld.h — single-header C build system
 *
 *  #define BLD_IMPLEMENTATION   — include implementation (do this in exactly one .c file)
 *  #define BLD_STRIP_PREFIX     — strip bld_/Bld_/BLD_ prefixes for convenience
 */
#pragma once

/* --- bld/bld_core.h --- */
/* bld/bld_core.h — all declarations, types, and API for bld */

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

/* ===== Typed slices for user-facing API ===== */

typedef struct { const char** items; size_t len, cap; } Bld_Strs;   /* defines, lib names */
typedef struct { const char** items; size_t len, cap; } Bld_Paths;  /* source files, dir paths */

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
void bld_panic(const char* fmt, ...) __attribute__((noreturn));

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

    void* user_data;
};

Bld_Toolchain* bld_toolchain_gcc(Bld_OsTarget os);

/* ===== Step — low-level graph node ===== */

typedef struct Bld_Step Bld_Step;
typedef struct Bld_Target Bld_Target;

typedef enum { BLD_ACTION_OK = 0, BLD_ACTION_FAILED = 1 } Bld_ActionResult;
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
    Bld_Hash input_hash;
    Bld_Hash cache_key;
    bool          hash_valid;
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
    bool         found;
    Bld_Paths    include_dirs;        /* -I */
    Bld_Paths    system_include_dirs; /* -isystem */
    Bld_Strs     libs;                /* -l names ("ssl", "crypto") */
    Bld_Paths    lib_dirs;            /* -L paths */
    const char*  extra_cflags;        /* raw extra compile flags */
    const char*  extra_ldflags;       /* raw extra link flags */
} Bld_Dep;

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
    Bld_Strs     args;
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

/* BLD_PATHS and BLD_STRS macros defined above in typed wrappers section */

typedef struct {
    const char*      name;
    const char*      desc;
    const char*      output_name; /* override output filename (default: name) */
    Bld_Paths        sources;
    Bld_Lang         lang;       /* BLD_LANG_AUTO (0): per-file by extension */
    Bld_CompileFlags compile;
    Bld_CompileFlags compile_propagate; /* public: propagated to link_with consumers */
    Bld_LinkFlags    link;
    Bld_LinkFlags    link_propagate;    /* public: propagated to link_with consumers */
    Bld_Toolchain*   toolchain;  /* NULL = use Bld default */
} Bld_ExeOpts;

typedef struct {
    const char*      name;
    const char*      desc;
    const char*      lib_basename; /* override lib filename base (default: name), toolchain adds prefix/suffix */
    Bld_Paths        sources;
    Bld_Lang         lang;       /* BLD_LANG_AUTO (0): per-file by extension */
    Bld_CompileFlags compile;           /* private: for compiling this lib's sources */
    Bld_CompileFlags compile_propagate; /* public: propagated to targets that link_with this lib */
    Bld_LinkFlags    link;
    Bld_LinkFlags    link_propagate;    /* public: propagated to consumers via link_with */
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
    Bld_DepList         resolved_ext_deps;
    Bld_Toolchain*      toolchain;  /* resolved: opts.toolchain ?: b->toolchain */
} Bld_Exe;

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

/* install */
Bld_Target* bld_install_exe(Bld* b, Bld_Target* exe);
Bld_Target* bld_install_lib(Bld* b, Bld_Target* lib);
Bld_Target* bld_install(Bld* b, Bld_Target* target, Bld_Path dst);
Bld_Target* bld_install_files(Bld* b, Bld_Paths files, Bld_Path dst);
Bld_Target* bld_install_dir(Bld* b, const char* src_dir, Bld_Path dst);

/* external dependencies — bld__dep, bld_find_pkg declared in bld_dep.h */
void     bld_use_dep(Bld_Target* t, Bld_Dep* dep);

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
/* --- bld/bld_cache.h --- */
/* bld/bld_cache.h — artifact cache interface */


/* ---- Path utilities (used by build actions, install, public API) ---- */

Bld_Path bld__step_artifact(Bld* b, Bld_Step* s);
Bld_Path bld__target_artifact(Bld* b, Bld_Target* t);
Bld_Path bld__cache_tmp(Bld* b);

/* ---- Cache operations ---- */

/* Compute cache_key from input_hash (+ depfile), check if cached and valid.
   Caller must set step->input_hash before calling. Sets step->cache_key + hash_valid. */
int bld__cache_has(Bld* b, Bld_Step* step);

/* Store action results in cache. Handles depfile, artifact, early cutoff, meta. */
void bld__cache_store(Bld* b, Bld_Step* step, Bld_Path tmp_out, Bld_Path tmp_dep);
/* --- bld/bld_dep.h --- */
/* bld/bld_dep.h — external dependency discovery */


/* deep-copy a dependency from compound literal (sets found=1) */
Bld_Dep* bld__dep(const Bld_Dep* d);
#define bld_dep(...) bld__dep(&(Bld_Dep){__VA_ARGS__})

/* discover dependency via pkg-config (found=0 if not installed) */
Bld_Dep* bld_find_pkg(const char* name);
/* --- bld/bld_checks.h --- */
/* bld/bld_checks.h — feature detection interface */


typedef struct Bld_Checks Bld_Checks;

Bld_Checks* bld_checks_new(Bld* parent);
bool* bld_checks_header(Bld_Checks* c, const char* define_name, const char* header);
bool* bld_checks_func(Bld_Checks* c, const char* define_name, const char* func, const char* header);
int*  bld_checks_sizeof(Bld_Checks* c, const char* define_name, const char* type);
bool* bld_checks_compile(Bld_Checks* c, const char* define_name, const char* source);
void  bld_checks_run(Bld_Checks* c);
void  bld_checks_write(Bld_Checks* c, const char* path);

/* implementation */
#ifdef BLD_IMPLEMENTATION
/* --- bld/bld_core_impl.c --- */
/* bld/bld_core_impl.c — arena, str, path, log, hash, fs implementations */


#define XXH_INLINE_ALL
/* --- bld/xxhash.h --- */
/*
 * xxHash - Extremely Fast Hash algorithm
 * Header File
 * Copyright (C) 2012-2023 Yann Collet
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - xxHash homepage: https://www.xxhash.com
 *   - xxHash source repository: https://github.com/Cyan4973/xxHash
 */

/*!
 * @mainpage xxHash
 *
 * xxHash is an extremely fast non-cryptographic hash algorithm, working at RAM speed
 * limits.
 *
 * It is proposed in four flavors, in three families:
 * 1. @ref XXH32_family
 *   - Classic 32-bit hash function. Simple, compact, and runs on almost all
 *     32-bit and 64-bit systems.
 * 2. @ref XXH64_family
 *   - Classic 64-bit adaptation of XXH32. Just as simple, and runs well on most
 *     64-bit systems (but _not_ 32-bit systems).
 * 3. @ref XXH3_family
 *   - Modern 64-bit and 128-bit hash function family which features improved
 *     strength and performance across the board, especially on smaller data.
 *     It benefits greatly from SIMD and 64-bit without requiring it.
 *
 * Benchmarks
 * ---
 * The reference system uses an Intel i7-9700K CPU, and runs Ubuntu x64 20.04.
 * The open source benchmark program is compiled with clang v10.0 using -O3 flag.
 *
 * | Hash Name            | ISA ext | Width | Large Data Speed | Small Data Velocity |
 * | -------------------- | ------- | ----: | ---------------: | ------------------: |
 * | XXH3_64bits()        | @b AVX2 |    64 |        59.4 GB/s |               133.1 |
 * | MeowHash             | AES-NI  |   128 |        58.2 GB/s |                52.5 |
 * | XXH3_128bits()       | @b AVX2 |   128 |        57.9 GB/s |               118.1 |
 * | CLHash               | PCLMUL  |    64 |        37.1 GB/s |                58.1 |
 * | XXH3_64bits()        | @b SSE2 |    64 |        31.5 GB/s |               133.1 |
 * | XXH3_128bits()       | @b SSE2 |   128 |        29.6 GB/s |               118.1 |
 * | RAM sequential read  |         |   N/A |        28.0 GB/s |                 N/A |
 * | ahash                | AES-NI  |    64 |        22.5 GB/s |               107.2 |
 * | City64               |         |    64 |        22.0 GB/s |                76.6 |
 * | T1ha2                |         |    64 |        22.0 GB/s |                99.0 |
 * | City128              |         |   128 |        21.7 GB/s |                57.7 |
 * | FarmHash             | AES-NI  |    64 |        21.3 GB/s |                71.9 |
 * | XXH64()              |         |    64 |        19.4 GB/s |                71.0 |
 * | SpookyHash           |         |    64 |        19.3 GB/s |                53.2 |
 * | Mum                  |         |    64 |        18.0 GB/s |                67.0 |
 * | CRC32C               | SSE4.2  |    32 |        13.0 GB/s |                57.9 |
 * | XXH32()              |         |    32 |         9.7 GB/s |                71.9 |
 * | City32               |         |    32 |         9.1 GB/s |                66.0 |
 * | Blake3*              | @b AVX2 |   256 |         4.4 GB/s |                 8.1 |
 * | Murmur3              |         |    32 |         3.9 GB/s |                56.1 |
 * | SipHash*             |         |    64 |         3.0 GB/s |                43.2 |
 * | Blake3*              | @b SSE2 |   256 |         2.4 GB/s |                 8.1 |
 * | HighwayHash          |         |    64 |         1.4 GB/s |                 6.0 |
 * | FNV64                |         |    64 |         1.2 GB/s |                62.7 |
 * | Blake2*              |         |   256 |         1.1 GB/s |                 5.1 |
 * | SHA1*                |         |   160 |         0.8 GB/s |                 5.6 |
 * | MD5*                 |         |   128 |         0.6 GB/s |                 7.8 |
 * @note
 *   - Hashes which require a specific ISA extension are noted. SSE2 is also noted,
 *     even though it is mandatory on x64.
 *   - Hashes with an asterisk are cryptographic. Note that MD5 is non-cryptographic
 *     by modern standards.
 *   - Small data velocity is a rough average of algorithm's efficiency for small
 *     data. For more accurate information, see the wiki.
 *   - More benchmarks and strength tests are found on the wiki:
 *         https://github.com/Cyan4973/xxHash/wiki
 *
 * Usage
 * ------
 * All xxHash variants use a similar API. Changing the algorithm is a trivial
 * substitution.
 *
 * @pre
 *    For functions which take an input and length parameter, the following
 *    requirements are assumed:
 *    - The range from [`input`, `input + length`) is valid, readable memory.
 *      - The only exception is if the `length` is `0`, `input` may be `NULL`.
 *    - For C++, the objects must have the *TriviallyCopyable* property, as the
 *      functions access bytes directly as if it was an array of `unsigned char`.
 *
 * @anchor single_shot_example
 * **Single Shot**
 *
 * These functions are stateless functions which hash a contiguous block of memory,
 * immediately returning the result. They are the easiest and usually the fastest
 * option.
 *
 * XXH32(), XXH64(), XXH3_64bits(), XXH3_128bits()
 *
 * @code{.c}
 *   #include <string.h>
 *   #include "xxhash.h"
 *
 *   // Example for a function which hashes a null terminated string with XXH32().
 *   XXH32_hash_t hash_string(const char* string, XXH32_hash_t seed)
 *   {
 *       // NULL pointers are only valid if the length is zero
 *       size_t length = (string == NULL) ? 0 : strlen(string);
 *       return XXH32(string, length, seed);
 *   }
 * @endcode
 *
 *
 * @anchor streaming_example
 * **Streaming**
 *
 * These groups of functions allow incremental hashing of unknown size, even
 * more than what would fit in a size_t.
 *
 * XXH32_reset(), XXH64_reset(), XXH3_64bits_reset(), XXH3_128bits_reset()
 *
 * @code{.c}
 *   #include <stdio.h>
 *   #include <assert.h>
 *   #include "xxhash.h"
 *   // Example for a function which hashes a FILE incrementally with XXH3_64bits().
 *   XXH64_hash_t hashFile(FILE* f)
 *   {
 *       // Allocate a state struct. Do not just use malloc() or new.
 *       XXH3_state_t* state = XXH3_createState();
 *       assert(state != NULL && "Out of memory!");
 *       // Reset the state to start a new hashing session.
 *       XXH3_64bits_reset(state);
 *       char buffer[4096];
 *       size_t count;
 *       // Read the file in chunks
 *       while ((count = fread(buffer, 1, sizeof(buffer), f)) != 0) {
 *           // Run update() as many times as necessary to process the data
 *           XXH3_64bits_update(state, buffer, count);
 *       }
 *       // Retrieve the finalized hash. This will not change the state.
 *       XXH64_hash_t result = XXH3_64bits_digest(state);
 *       // Free the state. Do not use free().
 *       XXH3_freeState(state);
 *       return result;
 *   }
 * @endcode
 *
 * Streaming functions generate the xxHash value from an incremental input.
 * This method is slower than single-call functions, due to state management.
 * For small inputs, prefer `XXH32()` and `XXH64()`, which are better optimized.
 *
 * An XXH state must first be allocated using `XXH*_createState()`.
 *
 * Start a new hash by initializing the state with a seed using `XXH*_reset()`.
 *
 * Then, feed the hash state by calling `XXH*_update()` as many times as necessary.
 *
 * The function returns an error code, with 0 meaning OK, and any other value
 * meaning there is an error.
 *
 * Finally, a hash value can be produced anytime, by using `XXH*_digest()`.
 * This function returns the nn-bits hash as an int or long long.
 *
 * It's still possible to continue inserting input into the hash state after a
 * digest, and generate new hash values later on by invoking `XXH*_digest()`.
 *
 * When done, release the state using `XXH*_freeState()`.
 *
 *
 * @anchor canonical_representation_example
 * **Canonical Representation**
 *
 * The default return values from XXH functions are unsigned 32, 64 and 128 bit
 * integers.
 * This the simplest and fastest format for further post-processing.
 *
 * However, this leaves open the question of what is the order on the byte level,
 * since little and big endian conventions will store the same number differently.
 *
 * The canonical representation settles this issue by mandating big-endian
 * convention, the same convention as human-readable numbers (large digits first).
 *
 * When writing hash values to storage, sending them over a network, or printing
 * them, it's highly recommended to use the canonical representation to ensure
 * portability across a wider range of systems, present and future.
 *
 * The following functions allow transformation of hash values to and from
 * canonical format.
 *
 * XXH32_canonicalFromHash(), XXH32_hashFromCanonical(),
 * XXH64_canonicalFromHash(), XXH64_hashFromCanonical(),
 * XXH128_canonicalFromHash(), XXH128_hashFromCanonical(),
 *
 * @code{.c}
 *   #include <stdio.h>
 *   #include "xxhash.h"
 *
 *   // Example for a function which prints XXH32_hash_t in human readable format
 *   void printXxh32(XXH32_hash_t hash)
 *   {
 *       XXH32_canonical_t cano;
 *       XXH32_canonicalFromHash(&cano, hash);
 *       size_t i;
 *       for(i = 0; i < sizeof(cano.digest); ++i) {
 *           printf("%02x", cano.digest[i]);
 *       }
 *       printf("\n");
 *   }
 *
 *   // Example for a function which converts XXH32_canonical_t to XXH32_hash_t
 *   XXH32_hash_t convertCanonicalToXxh32(XXH32_canonical_t cano)
 *   {
 *       XXH32_hash_t hash = XXH32_hashFromCanonical(&cano);
 *       return hash;
 *   }
 * @endcode
 *
 *
 * @file xxhash.h
 * xxHash prototypes and implementation
 */

#if defined (__cplusplus)
extern "C" {
#endif

/* ****************************
 *  INLINE mode
 ******************************/
/*!
 * @defgroup public Public API
 * Contains details on the public xxHash functions.
 * @{
 */
#ifdef XXH_DOXYGEN
/*!
 * @brief Gives access to internal state declaration, required for static allocation.
 *
 * Incompatible with dynamic linking, due to risks of ABI changes.
 *
 * Usage:
 * @code{.c}
 *     #define XXH_STATIC_LINKING_ONLY
 *     #include "xxhash.h"
 * @endcode
 */
#  define XXH_STATIC_LINKING_ONLY
/* Do not undef XXH_STATIC_LINKING_ONLY for Doxygen */

/*!
 * @brief Gives access to internal definitions.
 *
 * Usage:
 * @code{.c}
 *     #define XXH_STATIC_LINKING_ONLY
 *     #define XXH_IMPLEMENTATION
 *     #include "xxhash.h"
 * @endcode
 */
#  define XXH_IMPLEMENTATION
/* Do not undef XXH_IMPLEMENTATION for Doxygen */

/*!
 * @brief Exposes the implementation and marks all functions as `inline`.
 *
 * Use these build macros to inline xxhash into the target unit.
 * Inlining improves performance on small inputs, especially when the length is
 * expressed as a compile-time constant:
 *
 *  https://fastcompression.blogspot.com/2018/03/xxhash-for-small-keys-impressive-power.html
 *
 * It also keeps xxHash symbols private to the unit, so they are not exported.
 *
 * Usage:
 * @code{.c}
 *     #define XXH_INLINE_ALL
 *     #include "xxhash.h"
 * @endcode
 * Do not compile and link xxhash.o as a separate object, as it is not useful.
 */
#  define XXH_INLINE_ALL
#  undef XXH_INLINE_ALL
/*!
 * @brief Exposes the implementation without marking functions as inline.
 */
#  define XXH_PRIVATE_API
#  undef XXH_PRIVATE_API
/*!
 * @brief Emulate a namespace by transparently prefixing all symbols.
 *
 * If you want to include _and expose_ xxHash functions from within your own
 * library, but also want to avoid symbol collisions with other libraries which
 * may also include xxHash, you can use @ref XXH_NAMESPACE to automatically prefix
 * any public symbol from xxhash library with the value of @ref XXH_NAMESPACE
 * (therefore, avoid empty or numeric values).
 *
 * Note that no change is required within the calling program as long as it
 * includes `xxhash.h`: Regular symbol names will be automatically translated
 * by this header.
 */
#  define XXH_NAMESPACE /* YOUR NAME HERE */
#  undef XXH_NAMESPACE
#endif

#if (defined(XXH_INLINE_ALL) || defined(XXH_PRIVATE_API)) \
    && !defined(XXH_INLINE_ALL_31684351384)
   /* this section should be traversed only once */
#  define XXH_INLINE_ALL_31684351384
   /* give access to the advanced API, required to compile implementations */
#  undef XXH_STATIC_LINKING_ONLY   /* avoid macro redef */
#  define XXH_STATIC_LINKING_ONLY
   /* make all functions private */
#  undef XXH_PUBLIC_API
#  if defined(__GNUC__)
#    define XXH_PUBLIC_API static __inline __attribute__((__unused__))
#  elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#    define XXH_PUBLIC_API static inline
#  elif defined(_MSC_VER)
#    define XXH_PUBLIC_API static __inline
#  else
     /* note: this version may generate warnings for unused static functions */
#    define XXH_PUBLIC_API static
#  endif

   /*
    * This part deals with the special case where a unit wants to inline xxHash,
    * but "xxhash.h" has previously been included without XXH_INLINE_ALL,
    * such as part of some previously included *.h header file.
    * Without further action, the new include would just be ignored,
    * and functions would effectively _not_ be inlined (silent failure).
    * The following macros solve this situation by prefixing all inlined names,
    * avoiding naming collision with previous inclusions.
    */
   /* Before that, we unconditionally #undef all symbols,
    * in case they were already defined with XXH_NAMESPACE.
    * They will then be redefined for XXH_INLINE_ALL
    */
#  undef XXH_versionNumber
    /* XXH32 */
#  undef XXH32
#  undef XXH32_createState
#  undef XXH32_freeState
#  undef XXH32_reset
#  undef XXH32_update
#  undef XXH32_digest
#  undef XXH32_copyState
#  undef XXH32_canonicalFromHash
#  undef XXH32_hashFromCanonical
    /* XXH64 */
#  undef XXH64
#  undef XXH64_createState
#  undef XXH64_freeState
#  undef XXH64_reset
#  undef XXH64_update
#  undef XXH64_digest
#  undef XXH64_copyState
#  undef XXH64_canonicalFromHash
#  undef XXH64_hashFromCanonical
    /* XXH3_64bits */
#  undef XXH3_64bits
#  undef XXH3_64bits_withSecret
#  undef XXH3_64bits_withSeed
#  undef XXH3_64bits_withSecretandSeed
#  undef XXH3_createState
#  undef XXH3_freeState
#  undef XXH3_copyState
#  undef XXH3_64bits_reset
#  undef XXH3_64bits_reset_withSeed
#  undef XXH3_64bits_reset_withSecret
#  undef XXH3_64bits_update
#  undef XXH3_64bits_digest
#  undef XXH3_generateSecret
    /* XXH3_128bits */
#  undef XXH128
#  undef XXH3_128bits
#  undef XXH3_128bits_withSeed
#  undef XXH3_128bits_withSecret
#  undef XXH3_128bits_reset
#  undef XXH3_128bits_reset_withSeed
#  undef XXH3_128bits_reset_withSecret
#  undef XXH3_128bits_reset_withSecretandSeed
#  undef XXH3_128bits_update
#  undef XXH3_128bits_digest
#  undef XXH128_isEqual
#  undef XXH128_cmp
#  undef XXH128_canonicalFromHash
#  undef XXH128_hashFromCanonical
    /* Finally, free the namespace itself */
#  undef XXH_NAMESPACE

    /* employ the namespace for XXH_INLINE_ALL */
#  define XXH_NAMESPACE XXH_INLINE_
   /*
    * Some identifiers (enums, type names) are not symbols,
    * but they must nonetheless be renamed to avoid redeclaration.
    * Alternative solution: do not redeclare them.
    * However, this requires some #ifdefs, and has a more dispersed impact.
    * Meanwhile, renaming can be achieved in a single place.
    */
#  define XXH_IPREF(Id)   XXH_NAMESPACE ## Id
#  define XXH_OK XXH_IPREF(XXH_OK)
#  define XXH_ERROR XXH_IPREF(XXH_ERROR)
#  define XXH_errorcode XXH_IPREF(XXH_errorcode)
#  define XXH32_canonical_t  XXH_IPREF(XXH32_canonical_t)
#  define XXH64_canonical_t  XXH_IPREF(XXH64_canonical_t)
#  define XXH128_canonical_t XXH_IPREF(XXH128_canonical_t)
#  define XXH32_state_s XXH_IPREF(XXH32_state_s)
#  define XXH32_state_t XXH_IPREF(XXH32_state_t)
#  define XXH64_state_s XXH_IPREF(XXH64_state_s)
#  define XXH64_state_t XXH_IPREF(XXH64_state_t)
#  define XXH3_state_s  XXH_IPREF(XXH3_state_s)
#  define XXH3_state_t  XXH_IPREF(XXH3_state_t)
#  define XXH128_hash_t XXH_IPREF(XXH128_hash_t)
   /* Ensure the header is parsed again, even if it was previously included */
#  undef XXHASH_H_5627135585666179
#  undef XXHASH_H_STATIC_13879238742
#endif /* XXH_INLINE_ALL || XXH_PRIVATE_API */

/* ****************************************************************
 *  Stable API
 *****************************************************************/
#ifndef XXHASH_H_5627135585666179
#define XXHASH_H_5627135585666179 1

/*! @brief Marks a global symbol. */
#if !defined(XXH_INLINE_ALL) && !defined(XXH_PRIVATE_API)
#  if defined(_WIN32) && defined(_MSC_VER) && (defined(XXH_IMPORT) || defined(XXH_EXPORT))
#    ifdef XXH_EXPORT
#      define XXH_PUBLIC_API __declspec(dllexport)
#    elif XXH_IMPORT
#      define XXH_PUBLIC_API __declspec(dllimport)
#    endif
#  else
#    define XXH_PUBLIC_API   /* do nothing */
#  endif
#endif

#ifdef XXH_NAMESPACE
#  define XXH_CAT(A,B) A##B
#  define XXH_NAME2(A,B) XXH_CAT(A,B)
#  define XXH_versionNumber XXH_NAME2(XXH_NAMESPACE, XXH_versionNumber)
/* XXH32 */
#  define XXH32 XXH_NAME2(XXH_NAMESPACE, XXH32)
#  define XXH32_createState XXH_NAME2(XXH_NAMESPACE, XXH32_createState)
#  define XXH32_freeState XXH_NAME2(XXH_NAMESPACE, XXH32_freeState)
#  define XXH32_reset XXH_NAME2(XXH_NAMESPACE, XXH32_reset)
#  define XXH32_update XXH_NAME2(XXH_NAMESPACE, XXH32_update)
#  define XXH32_digest XXH_NAME2(XXH_NAMESPACE, XXH32_digest)
#  define XXH32_copyState XXH_NAME2(XXH_NAMESPACE, XXH32_copyState)
#  define XXH32_canonicalFromHash XXH_NAME2(XXH_NAMESPACE, XXH32_canonicalFromHash)
#  define XXH32_hashFromCanonical XXH_NAME2(XXH_NAMESPACE, XXH32_hashFromCanonical)
/* XXH64 */
#  define XXH64 XXH_NAME2(XXH_NAMESPACE, XXH64)
#  define XXH64_createState XXH_NAME2(XXH_NAMESPACE, XXH64_createState)
#  define XXH64_freeState XXH_NAME2(XXH_NAMESPACE, XXH64_freeState)
#  define XXH64_reset XXH_NAME2(XXH_NAMESPACE, XXH64_reset)
#  define XXH64_update XXH_NAME2(XXH_NAMESPACE, XXH64_update)
#  define XXH64_digest XXH_NAME2(XXH_NAMESPACE, XXH64_digest)
#  define XXH64_copyState XXH_NAME2(XXH_NAMESPACE, XXH64_copyState)
#  define XXH64_canonicalFromHash XXH_NAME2(XXH_NAMESPACE, XXH64_canonicalFromHash)
#  define XXH64_hashFromCanonical XXH_NAME2(XXH_NAMESPACE, XXH64_hashFromCanonical)
/* XXH3_64bits */
#  define XXH3_64bits XXH_NAME2(XXH_NAMESPACE, XXH3_64bits)
#  define XXH3_64bits_withSecret XXH_NAME2(XXH_NAMESPACE, XXH3_64bits_withSecret)
#  define XXH3_64bits_withSeed XXH_NAME2(XXH_NAMESPACE, XXH3_64bits_withSeed)
#  define XXH3_64bits_withSecretandSeed XXH_NAME2(XXH_NAMESPACE, XXH3_64bits_withSecretandSeed)
#  define XXH3_createState XXH_NAME2(XXH_NAMESPACE, XXH3_createState)
#  define XXH3_freeState XXH_NAME2(XXH_NAMESPACE, XXH3_freeState)
#  define XXH3_copyState XXH_NAME2(XXH_NAMESPACE, XXH3_copyState)
#  define XXH3_64bits_reset XXH_NAME2(XXH_NAMESPACE, XXH3_64bits_reset)
#  define XXH3_64bits_reset_withSeed XXH_NAME2(XXH_NAMESPACE, XXH3_64bits_reset_withSeed)
#  define XXH3_64bits_reset_withSecret XXH_NAME2(XXH_NAMESPACE, XXH3_64bits_reset_withSecret)
#  define XXH3_64bits_reset_withSecretandSeed XXH_NAME2(XXH_NAMESPACE, XXH3_64bits_reset_withSecretandSeed)
#  define XXH3_64bits_update XXH_NAME2(XXH_NAMESPACE, XXH3_64bits_update)
#  define XXH3_64bits_digest XXH_NAME2(XXH_NAMESPACE, XXH3_64bits_digest)
#  define XXH3_generateSecret XXH_NAME2(XXH_NAMESPACE, XXH3_generateSecret)
#  define XXH3_generateSecret_fromSeed XXH_NAME2(XXH_NAMESPACE, XXH3_generateSecret_fromSeed)
/* XXH3_128bits */
#  define XXH128 XXH_NAME2(XXH_NAMESPACE, XXH128)
#  define XXH3_128bits XXH_NAME2(XXH_NAMESPACE, XXH3_128bits)
#  define XXH3_128bits_withSeed XXH_NAME2(XXH_NAMESPACE, XXH3_128bits_withSeed)
#  define XXH3_128bits_withSecret XXH_NAME2(XXH_NAMESPACE, XXH3_128bits_withSecret)
#  define XXH3_128bits_withSecretandSeed XXH_NAME2(XXH_NAMESPACE, XXH3_128bits_withSecretandSeed)
#  define XXH3_128bits_reset XXH_NAME2(XXH_NAMESPACE, XXH3_128bits_reset)
#  define XXH3_128bits_reset_withSeed XXH_NAME2(XXH_NAMESPACE, XXH3_128bits_reset_withSeed)
#  define XXH3_128bits_reset_withSecret XXH_NAME2(XXH_NAMESPACE, XXH3_128bits_reset_withSecret)
#  define XXH3_128bits_reset_withSecretandSeed XXH_NAME2(XXH_NAMESPACE, XXH3_128bits_reset_withSecretandSeed)
#  define XXH3_128bits_update XXH_NAME2(XXH_NAMESPACE, XXH3_128bits_update)
#  define XXH3_128bits_digest XXH_NAME2(XXH_NAMESPACE, XXH3_128bits_digest)
#  define XXH128_isEqual XXH_NAME2(XXH_NAMESPACE, XXH128_isEqual)
#  define XXH128_cmp     XXH_NAME2(XXH_NAMESPACE, XXH128_cmp)
#  define XXH128_canonicalFromHash XXH_NAME2(XXH_NAMESPACE, XXH128_canonicalFromHash)
#  define XXH128_hashFromCanonical XXH_NAME2(XXH_NAMESPACE, XXH128_hashFromCanonical)
#endif


/* *************************************
*  Compiler specifics
***************************************/

/* specific declaration modes for Windows */
#if !defined(XXH_INLINE_ALL) && !defined(XXH_PRIVATE_API)
#  if defined(_WIN32) && defined(_MSC_VER) && (defined(XXH_IMPORT) || defined(XXH_EXPORT))
#    ifdef XXH_EXPORT
#      define XXH_PUBLIC_API __declspec(dllexport)
#    elif XXH_IMPORT
#      define XXH_PUBLIC_API __declspec(dllimport)
#    endif
#  else
#    define XXH_PUBLIC_API   /* do nothing */
#  endif
#endif

#if defined (__GNUC__)
# define XXH_CONSTF  __attribute__((__const__))
# define XXH_PUREF   __attribute__((__pure__))
# define XXH_MALLOCF __attribute__((__malloc__))
#else
# define XXH_CONSTF  /* disable */
# define XXH_PUREF
# define XXH_MALLOCF
#endif

/* *************************************
*  Version
***************************************/
#define XXH_VERSION_MAJOR    0
#define XXH_VERSION_MINOR    8
#define XXH_VERSION_RELEASE  3
/*! @brief Version number, encoded as two digits each */
#define XXH_VERSION_NUMBER  (XXH_VERSION_MAJOR *100*100 + XXH_VERSION_MINOR *100 + XXH_VERSION_RELEASE)

/*!
 * @brief Obtains the xxHash version.
 *
 * This is mostly useful when xxHash is compiled as a shared library,
 * since the returned value comes from the library, as opposed to header file.
 *
 * @return @ref XXH_VERSION_NUMBER of the invoked library.
 */
XXH_PUBLIC_API XXH_CONSTF unsigned XXH_versionNumber (void);


/* ****************************
*  Common basic types
******************************/
#include <stddef.h>   /* size_t */
/*!
 * @brief Exit code for the streaming API.
 */
typedef enum {
    XXH_OK = 0, /*!< OK */
    XXH_ERROR   /*!< Error */
} XXH_errorcode;


/*-**********************************************************************
*  32-bit hash
************************************************************************/
#if defined(XXH_DOXYGEN) /* Don't show <stdint.h> include */
/*!
 * @brief An unsigned 32-bit integer.
 *
 * Not necessarily defined to `uint32_t` but functionally equivalent.
 */
typedef uint32_t XXH32_hash_t;

#elif !defined (__VMS) \
  && (defined (__cplusplus) \
  || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
#   ifdef _AIX
#     include <inttypes.h>
#   else
#     include <stdint.h>
#   endif
    typedef uint32_t XXH32_hash_t;

#else
#   include <limits.h>
#   if UINT_MAX == 0xFFFFFFFFUL
      typedef unsigned int XXH32_hash_t;
#   elif ULONG_MAX == 0xFFFFFFFFUL
      typedef unsigned long XXH32_hash_t;
#   else
#     error "unsupported platform: need a 32-bit type"
#   endif
#endif

/*!
 * @}
 *
 * @defgroup XXH32_family XXH32 family
 * @ingroup public
 * Contains functions used in the classic 32-bit xxHash algorithm.
 *
 * @note
 *   XXH32 is useful for older platforms, with no or poor 64-bit performance.
 *   Note that the @ref XXH3_family provides competitive speed for both 32-bit
 *   and 64-bit systems, and offers true 64/128 bit hash results.
 *
 * @see @ref XXH64_family, @ref XXH3_family : Other xxHash families
 * @see @ref XXH32_impl for implementation details
 * @{
 */

/*!
 * @brief Calculates the 32-bit hash of @p input using xxHash32.
 *
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 * @param seed The 32-bit seed to alter the hash's output predictably.
 *
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return The calculated 32-bit xxHash32 value.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH32_hash_t XXH32 (const void* input, size_t length, XXH32_hash_t seed);

#ifndef XXH_NO_STREAM
/*!
 * @typedef struct XXH32_state_s XXH32_state_t
 * @brief The opaque state struct for the XXH32 streaming API.
 *
 * @see XXH32_state_s for details.
 * @see @ref streaming_example "Streaming Example"
 */
typedef struct XXH32_state_s XXH32_state_t;

/*!
 * @brief Allocates an @ref XXH32_state_t.
 *
 * @return An allocated pointer of @ref XXH32_state_t on success.
 * @return `NULL` on failure.
 *
 * @note Must be freed with XXH32_freeState().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_MALLOCF XXH32_state_t* XXH32_createState(void);
/*!
 * @brief Frees an @ref XXH32_state_t.
 *
 * @param statePtr A pointer to an @ref XXH32_state_t allocated with @ref XXH32_createState().
 *
 * @return @ref XXH_OK.
 *
 * @note @p statePtr must be allocated with XXH32_createState().
 *
 * @see @ref streaming_example "Streaming Example"
 *
 */
XXH_PUBLIC_API XXH_errorcode  XXH32_freeState(XXH32_state_t* statePtr);
/*!
 * @brief Copies one @ref XXH32_state_t to another.
 *
 * @param dst_state The state to copy to.
 * @param src_state The state to copy from.
 * @pre
 *   @p dst_state and @p src_state must not be `NULL` and must not overlap.
 */
XXH_PUBLIC_API void XXH32_copyState(XXH32_state_t* dst_state, const XXH32_state_t* src_state);

/*!
 * @brief Resets an @ref XXH32_state_t to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 * @param seed The 32-bit seed to alter the hash result predictably.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note This function resets and seeds a state. Call it before @ref XXH32_update().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH32_reset  (XXH32_state_t* statePtr, XXH32_hash_t seed);

/*!
 * @brief Consumes a block of @p input to an @ref XXH32_state_t.
 *
 * @param statePtr The state struct to update.
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note Call this to incrementally consume blocks of data.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH32_update (XXH32_state_t* statePtr, const void* input, size_t length);

/*!
 * @brief Returns the calculated hash value from an @ref XXH32_state_t.
 *
 * @param statePtr The state struct to calculate the hash from.
 *
 * @pre
 *  @p statePtr must not be `NULL`.
 *
 * @return The calculated 32-bit xxHash32 value from that state.
 *
 * @note
 *   Calling XXH32_digest() will not affect @p statePtr, so you can update,
 *   digest, and update again.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH32_hash_t XXH32_digest (const XXH32_state_t* statePtr);
#endif /* !XXH_NO_STREAM */

/*******   Canonical representation   *******/

/*!
 * @brief Canonical (big endian) representation of @ref XXH32_hash_t.
 */
typedef struct {
    unsigned char digest[4]; /*!< Hash bytes, big endian */
} XXH32_canonical_t;

/*!
 * @brief Converts an @ref XXH32_hash_t to a big endian @ref XXH32_canonical_t.
 *
 * @param dst  The @ref XXH32_canonical_t pointer to be stored to.
 * @param hash The @ref XXH32_hash_t to be converted.
 *
 * @pre
 *   @p dst must not be `NULL`.
 *
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API void XXH32_canonicalFromHash(XXH32_canonical_t* dst, XXH32_hash_t hash);

/*!
 * @brief Converts an @ref XXH32_canonical_t to a native @ref XXH32_hash_t.
 *
 * @param src The @ref XXH32_canonical_t to convert.
 *
 * @pre
 *   @p src must not be `NULL`.
 *
 * @return The converted hash.
 *
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH32_hash_t XXH32_hashFromCanonical(const XXH32_canonical_t* src);


/*! @cond Doxygen ignores this part */
#ifdef __has_attribute
# define XXH_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
# define XXH_HAS_ATTRIBUTE(x) 0
#endif
/*! @endcond */

/*! @cond Doxygen ignores this part */
/*
 * C23 __STDC_VERSION__ number hasn't been specified yet. For now
 * leave as `201711L` (C17 + 1).
 * TODO: Update to correct value when its been specified.
 */
#define XXH_C23_VN 201711L
/*! @endcond */

/*! @cond Doxygen ignores this part */
/* C-language Attributes are added in C23. */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= XXH_C23_VN) && defined(__has_c_attribute)
# define XXH_HAS_C_ATTRIBUTE(x) __has_c_attribute(x)
#else
# define XXH_HAS_C_ATTRIBUTE(x) 0
#endif
/*! @endcond */

/*! @cond Doxygen ignores this part */
#if defined(__cplusplus) && defined(__has_cpp_attribute)
# define XXH_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
# define XXH_HAS_CPP_ATTRIBUTE(x) 0
#endif
/*! @endcond */

/*! @cond Doxygen ignores this part */
/*
 * Define XXH_FALLTHROUGH macro for annotating switch case with the 'fallthrough' attribute
 * introduced in CPP17 and C23.
 * CPP17 : https://en.cppreference.com/w/cpp/language/attributes/fallthrough
 * C23   : https://en.cppreference.com/w/c/language/attributes/fallthrough
 */
#if XXH_HAS_C_ATTRIBUTE(fallthrough) || XXH_HAS_CPP_ATTRIBUTE(fallthrough)
# define XXH_FALLTHROUGH [[fallthrough]]
#elif XXH_HAS_ATTRIBUTE(__fallthrough__)
# define XXH_FALLTHROUGH __attribute__ ((__fallthrough__))
#else
# define XXH_FALLTHROUGH /* fallthrough */
#endif
/*! @endcond */

/*! @cond Doxygen ignores this part */
/*
 * Define XXH_NOESCAPE for annotated pointers in public API.
 * https://clang.llvm.org/docs/AttributeReference.html#noescape
 * As of writing this, only supported by clang.
 */
#if XXH_HAS_ATTRIBUTE(noescape)
# define XXH_NOESCAPE __attribute__((__noescape__))
#else
# define XXH_NOESCAPE
#endif
/*! @endcond */


/*!
 * @}
 * @ingroup public
 * @{
 */

#ifndef XXH_NO_LONG_LONG
/*-**********************************************************************
*  64-bit hash
************************************************************************/
#if defined(XXH_DOXYGEN) /* don't include <stdint.h> */
/*!
 * @brief An unsigned 64-bit integer.
 *
 * Not necessarily defined to `uint64_t` but functionally equivalent.
 */
typedef uint64_t XXH64_hash_t;
#elif !defined (__VMS) \
  && (defined (__cplusplus) \
  || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
#   ifdef _AIX
#     include <inttypes.h>
#   else
#     include <stdint.h>
#   endif
   typedef uint64_t XXH64_hash_t;
#else
#  include <limits.h>
#  if defined(__LP64__) && ULONG_MAX == 0xFFFFFFFFFFFFFFFFULL
     /* LP64 ABI says uint64_t is unsigned long */
     typedef unsigned long XXH64_hash_t;
#  else
     /* the following type must have a width of 64-bit */
     typedef unsigned long long XXH64_hash_t;
#  endif
#endif

/*!
 * @}
 *
 * @defgroup XXH64_family XXH64 family
 * @ingroup public
 * @{
 * Contains functions used in the classic 64-bit xxHash algorithm.
 *
 * @note
 *   XXH3 provides competitive speed for both 32-bit and 64-bit systems,
 *   and offers true 64/128 bit hash results.
 *   It provides better speed for systems with vector processing capabilities.
 */

/*!
 * @brief Calculates the 64-bit hash of @p input using xxHash64.
 *
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 * @param seed The 64-bit seed to alter the hash's output predictably.
 *
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return The calculated 64-bit xxHash64 value.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH64(XXH_NOESCAPE const void* input, size_t length, XXH64_hash_t seed);

/*******   Streaming   *******/
#ifndef XXH_NO_STREAM
/*!
 * @brief The opaque state struct for the XXH64 streaming API.
 *
 * @see XXH64_state_s for details.
 * @see @ref streaming_example "Streaming Example"
 */
typedef struct XXH64_state_s XXH64_state_t;   /* incomplete type */

/*!
 * @brief Allocates an @ref XXH64_state_t.
 *
 * @return An allocated pointer of @ref XXH64_state_t on success.
 * @return `NULL` on failure.
 *
 * @note Must be freed with XXH64_freeState().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_MALLOCF XXH64_state_t* XXH64_createState(void);

/*!
 * @brief Frees an @ref XXH64_state_t.
 *
 * @param statePtr A pointer to an @ref XXH64_state_t allocated with @ref XXH64_createState().
 *
 * @return @ref XXH_OK.
 *
 * @note @p statePtr must be allocated with XXH64_createState().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode  XXH64_freeState(XXH64_state_t* statePtr);

/*!
 * @brief Copies one @ref XXH64_state_t to another.
 *
 * @param dst_state The state to copy to.
 * @param src_state The state to copy from.
 * @pre
 *   @p dst_state and @p src_state must not be `NULL` and must not overlap.
 */
XXH_PUBLIC_API void XXH64_copyState(XXH_NOESCAPE XXH64_state_t* dst_state, const XXH64_state_t* src_state);

/*!
 * @brief Resets an @ref XXH64_state_t to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 * @param seed The 64-bit seed to alter the hash result predictably.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note This function resets and seeds a state. Call it before @ref XXH64_update().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH64_reset  (XXH_NOESCAPE XXH64_state_t* statePtr, XXH64_hash_t seed);

/*!
 * @brief Consumes a block of @p input to an @ref XXH64_state_t.
 *
 * @param statePtr The state struct to update.
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note Call this to incrementally consume blocks of data.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH64_update (XXH_NOESCAPE XXH64_state_t* statePtr, XXH_NOESCAPE const void* input, size_t length);

/*!
 * @brief Returns the calculated hash value from an @ref XXH64_state_t.
 *
 * @param statePtr The state struct to calculate the hash from.
 *
 * @pre
 *  @p statePtr must not be `NULL`.
 *
 * @return The calculated 64-bit xxHash64 value from that state.
 *
 * @note
 *   Calling XXH64_digest() will not affect @p statePtr, so you can update,
 *   digest, and update again.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH64_digest (XXH_NOESCAPE const XXH64_state_t* statePtr);
#endif /* !XXH_NO_STREAM */
/*******   Canonical representation   *******/

/*!
 * @brief Canonical (big endian) representation of @ref XXH64_hash_t.
 */
typedef struct { unsigned char digest[sizeof(XXH64_hash_t)]; } XXH64_canonical_t;

/*!
 * @brief Converts an @ref XXH64_hash_t to a big endian @ref XXH64_canonical_t.
 *
 * @param dst The @ref XXH64_canonical_t pointer to be stored to.
 * @param hash The @ref XXH64_hash_t to be converted.
 *
 * @pre
 *   @p dst must not be `NULL`.
 *
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API void XXH64_canonicalFromHash(XXH_NOESCAPE XXH64_canonical_t* dst, XXH64_hash_t hash);

/*!
 * @brief Converts an @ref XXH64_canonical_t to a native @ref XXH64_hash_t.
 *
 * @param src The @ref XXH64_canonical_t to convert.
 *
 * @pre
 *   @p src must not be `NULL`.
 *
 * @return The converted hash.
 *
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH64_hashFromCanonical(XXH_NOESCAPE const XXH64_canonical_t* src);

#ifndef XXH_NO_XXH3

/*!
 * @}
 * ************************************************************************
 * @defgroup XXH3_family XXH3 family
 * @ingroup public
 * @{
 *
 * XXH3 is a more recent hash algorithm featuring:
 *  - Improved speed for both small and large inputs
 *  - True 64-bit and 128-bit outputs
 *  - SIMD acceleration
 *  - Improved 32-bit viability
 *
 * Speed analysis methodology is explained here:
 *
 *    https://fastcompression.blogspot.com/2019/03/presenting-xxh3.html
 *
 * Compared to XXH64, expect XXH3 to run approximately
 * ~2x faster on large inputs and >3x faster on small ones,
 * exact differences vary depending on platform.
 *
 * XXH3's speed benefits greatly from SIMD and 64-bit arithmetic,
 * but does not require it.
 * Most 32-bit and 64-bit targets that can run XXH32 smoothly can run XXH3
 * at competitive speeds, even without vector support. Further details are
 * explained in the implementation.
 *
 * XXH3 has a fast scalar implementation, but it also includes accelerated SIMD
 * implementations for many common platforms:
 *   - AVX512
 *   - AVX2
 *   - SSE2
 *   - ARM NEON
 *   - WebAssembly SIMD128
 *   - POWER8 VSX
 *   - s390x ZVector
 * This can be controlled via the @ref XXH_VECTOR macro, but it automatically
 * selects the best version according to predefined macros. For the x86 family, an
 * automatic runtime dispatcher is included separately in @ref xxh_x86dispatch.c.
 *
 * XXH3 implementation is portable:
 * it has a generic C90 formulation that can be compiled on any platform,
 * all implementations generate exactly the same hash value on all platforms.
 * Starting from v0.8.0, it's also labelled "stable", meaning that
 * any future version will also generate the same hash value.
 *
 * XXH3 offers 2 variants, _64bits and _128bits.
 *
 * When only 64 bits are needed, prefer invoking the _64bits variant, as it
 * reduces the amount of mixing, resulting in faster speed on small inputs.
 * It's also generally simpler to manipulate a scalar return type than a struct.
 *
 * The API supports one-shot hashing, streaming mode, and custom secrets.
 */

/*!
 * @ingroup tuning
 * @brief Possible values for @ref XXH_VECTOR.
 *
 * Unless set explicitly, determined automatically.
 */
#  define XXH_SCALAR 0 /*!< Portable scalar version */
#  define XXH_SSE2   1 /*!< SSE2 for Pentium 4, Opteron, all x86_64. */
#  define XXH_AVX2   2 /*!< AVX2 for Haswell and Bulldozer */
#  define XXH_AVX512 3 /*!< AVX512 for Skylake and Icelake */
#  define XXH_NEON   4 /*!< NEON for most ARMv7-A, all AArch64, and WASM SIMD128 */
#  define XXH_VSX    5 /*!< VSX and ZVector for POWER8/z13 (64-bit) */
#  define XXH_SVE    6 /*!< SVE for some ARMv8-A and ARMv9-A */
#  define XXH_LSX    7 /*!< LSX (128-bit SIMD) for LoongArch64 */


/*-**********************************************************************
*  XXH3 64-bit variant
************************************************************************/

/*!
 * @brief Calculates 64-bit unseeded variant of XXH3 hash of @p input.
 *
 * @param input  The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 *
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return The calculated 64-bit XXH3 hash value.
 *
 * @note
 *   This is equivalent to @ref XXH3_64bits_withSeed() with a seed of `0`, however
 *   it may have slightly better performance due to constant propagation of the
 *   defaults.
 *
 * @see
 *    XXH3_64bits_withSeed(), XXH3_64bits_withSecret(): other seeding variants
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH3_64bits(XXH_NOESCAPE const void* input, size_t length);

/*!
 * @brief Calculates 64-bit seeded variant of XXH3 hash of @p input.
 *
 * @param input  The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 * @param seed   The 64-bit seed to alter the hash result predictably.
 *
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return The calculated 64-bit XXH3 hash value.
 *
 * @note
 *    seed == 0 produces the same results as @ref XXH3_64bits().
 *
 * This variant generates a custom secret on the fly based on default secret
 * altered using the @p seed value.
 *
 * While this operation is decently fast, note that it's not completely free.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH3_64bits_withSeed(XXH_NOESCAPE const void* input, size_t length, XXH64_hash_t seed);

/*!
 * The bare minimum size for a custom secret.
 *
 * @see
 *  XXH3_64bits_withSecret(), XXH3_64bits_reset_withSecret(),
 *  XXH3_128bits_withSecret(), XXH3_128bits_reset_withSecret().
 */
#define XXH3_SECRET_SIZE_MIN 136

/*!
 * @brief Calculates 64-bit variant of XXH3 with a custom "secret".
 *
 * @param data       The block of data to be hashed, at least @p len bytes in size.
 * @param len        The length of @p data, in bytes.
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 *
 * @return The calculated 64-bit XXH3 hash value.
 *
 * @pre
 *   The memory between @p data and @p data + @p len must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p data may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * It's possible to provide any blob of bytes as a "secret" to generate the hash.
 * This makes it more difficult for an external actor to prepare an intentional collision.
 * The main condition is that @p secretSize *must* be large enough (>= @ref XXH3_SECRET_SIZE_MIN).
 * However, the quality of the secret impacts the dispersion of the hash algorithm.
 * Therefore, the secret _must_ look like a bunch of random bytes.
 * Avoid "trivial" or structured data such as repeated sequences or a text document.
 * Whenever in doubt about the "randomness" of the blob of bytes,
 * consider employing @ref XXH3_generateSecret() instead (see below).
 * It will generate a proper high entropy secret derived from the blob of bytes.
 * Another advantage of using XXH3_generateSecret() is that
 * it guarantees that all bits within the initial blob of bytes
 * will impact every bit of the output.
 * This is not necessarily the case when using the blob of bytes directly
 * because, when hashing _small_ inputs, only a portion of the secret is employed.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t XXH3_64bits_withSecret(XXH_NOESCAPE const void* data, size_t len, XXH_NOESCAPE const void* secret, size_t secretSize);


/*******   Streaming   *******/
#ifndef XXH_NO_STREAM
/*
 * Streaming requires state maintenance.
 * This operation costs memory and CPU.
 * As a consequence, streaming is slower than one-shot hashing.
 * For better performance, prefer one-shot functions whenever applicable.
 */

/*!
 * @brief The opaque state struct for the XXH3 streaming API.
 *
 * @see XXH3_state_s for details.
 * @see @ref streaming_example "Streaming Example"
 */
typedef struct XXH3_state_s XXH3_state_t;
XXH_PUBLIC_API XXH_MALLOCF XXH3_state_t* XXH3_createState(void);
XXH_PUBLIC_API XXH_errorcode XXH3_freeState(XXH3_state_t* statePtr);

/*!
 * @brief Copies one @ref XXH3_state_t to another.
 *
 * @param dst_state The state to copy to.
 * @param src_state The state to copy from.
 * @pre
 *   @p dst_state and @p src_state must not be `NULL` and must not overlap.
 */
XXH_PUBLIC_API void XXH3_copyState(XXH_NOESCAPE XXH3_state_t* dst_state, XXH_NOESCAPE const XXH3_state_t* src_state);

/*!
 * @brief Resets an @ref XXH3_state_t to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   - This function resets `statePtr` and generate a secret with default parameters.
 *   - Call this function before @ref XXH3_64bits_update().
 *   - Digest will be equivalent to `XXH3_64bits()`.
 *
 * @see @ref streaming_example "Streaming Example"
 *
 */
XXH_PUBLIC_API XXH_errorcode XXH3_64bits_reset(XXH_NOESCAPE XXH3_state_t* statePtr);

/*!
 * @brief Resets an @ref XXH3_state_t with 64-bit seed to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 * @param seed     The 64-bit seed to alter the hash result predictably.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   - This function resets `statePtr` and generate a secret from `seed`.
 *   - Call this function before @ref XXH3_64bits_update().
 *   - Digest will be equivalent to `XXH3_64bits_withSeed()`.
 *
 * @see @ref streaming_example "Streaming Example"
 *
 */
XXH_PUBLIC_API XXH_errorcode XXH3_64bits_reset_withSeed(XXH_NOESCAPE XXH3_state_t* statePtr, XXH64_hash_t seed);

/*!
 * @brief Resets an @ref XXH3_state_t with secret data to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   `secret` is referenced, it _must outlive_ the hash streaming session.
 *
 * Similar to one-shot API, `secretSize` must be >= @ref XXH3_SECRET_SIZE_MIN,
 * and the quality of produced hash values depends on secret's entropy
 * (secret's content should look like a bunch of random bytes).
 * When in doubt about the randomness of a candidate `secret`,
 * consider employing `XXH3_generateSecret()` instead (see below).
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH3_64bits_reset_withSecret(XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* secret, size_t secretSize);

/*!
 * @brief Consumes a block of @p input to an @ref XXH3_state_t.
 *
 * @param statePtr The state struct to update.
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 * @pre
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note Call this to incrementally consume blocks of data.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH3_64bits_update (XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* input, size_t length);

/*!
 * @brief Returns the calculated XXH3 64-bit hash value from an @ref XXH3_state_t.
 *
 * @param statePtr The state struct to calculate the hash from.
 *
 * @pre
 *  @p statePtr must not be `NULL`.
 *
 * @return The calculated XXH3 64-bit hash value from that state.
 *
 * @note
 *   Calling XXH3_64bits_digest() will not affect @p statePtr, so you can update,
 *   digest, and update again.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t  XXH3_64bits_digest (XXH_NOESCAPE const XXH3_state_t* statePtr);
#endif /* !XXH_NO_STREAM */

/* note : canonical representation of XXH3 is the same as XXH64
 * since they both produce XXH64_hash_t values */


/*-**********************************************************************
*  XXH3 128-bit variant
************************************************************************/

/*!
 * @brief The return value from 128-bit hashes.
 *
 * Stored in little endian order, although the fields themselves are in native
 * endianness.
 */
typedef struct {
    XXH64_hash_t low64;   /*!< `value & 0xFFFFFFFFFFFFFFFF` */
    XXH64_hash_t high64;  /*!< `value >> 64` */
} XXH128_hash_t;

/*!
 * @brief Calculates 128-bit unseeded variant of XXH3 of @p data.
 *
 * @param data The block of data to be hashed, at least @p length bytes in size.
 * @param len  The length of @p data, in bytes.
 *
 * @return The calculated 128-bit variant of XXH3 value.
 *
 * The 128-bit variant of XXH3 has more strength, but it has a bit of overhead
 * for shorter inputs.
 *
 * This is equivalent to @ref XXH3_128bits_withSeed() with a seed of `0`, however
 * it may have slightly better performance due to constant propagation of the
 * defaults.
 *
 * @see XXH3_128bits_withSeed(), XXH3_128bits_withSecret(): other seeding variants
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH3_128bits(XXH_NOESCAPE const void* data, size_t len);
/*! @brief Calculates 128-bit seeded variant of XXH3 hash of @p data.
 *
 * @param data The block of data to be hashed, at least @p length bytes in size.
 * @param len  The length of @p data, in bytes.
 * @param seed The 64-bit seed to alter the hash result predictably.
 *
 * @return The calculated 128-bit variant of XXH3 value.
 *
 * @note
 *    seed == 0 produces the same results as @ref XXH3_64bits().
 *
 * This variant generates a custom secret on the fly based on default secret
 * altered using the @p seed value.
 *
 * While this operation is decently fast, note that it's not completely free.
 *
 * @see XXH3_128bits(), XXH3_128bits_withSecret(): other seeding variants
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH3_128bits_withSeed(XXH_NOESCAPE const void* data, size_t len, XXH64_hash_t seed);
/*!
 * @brief Calculates 128-bit variant of XXH3 with a custom "secret".
 *
 * @param data       The block of data to be hashed, at least @p len bytes in size.
 * @param len        The length of @p data, in bytes.
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 *
 * @return The calculated 128-bit variant of XXH3 value.
 *
 * It's possible to provide any blob of bytes as a "secret" to generate the hash.
 * This makes it more difficult for an external actor to prepare an intentional collision.
 * The main condition is that @p secretSize *must* be large enough (>= @ref XXH3_SECRET_SIZE_MIN).
 * However, the quality of the secret impacts the dispersion of the hash algorithm.
 * Therefore, the secret _must_ look like a bunch of random bytes.
 * Avoid "trivial" or structured data such as repeated sequences or a text document.
 * Whenever in doubt about the "randomness" of the blob of bytes,
 * consider employing @ref XXH3_generateSecret() instead (see below).
 * It will generate a proper high entropy secret derived from the blob of bytes.
 * Another advantage of using XXH3_generateSecret() is that
 * it guarantees that all bits within the initial blob of bytes
 * will impact every bit of the output.
 * This is not necessarily the case when using the blob of bytes directly
 * because, when hashing _small_ inputs, only a portion of the secret is employed.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH3_128bits_withSecret(XXH_NOESCAPE const void* data, size_t len, XXH_NOESCAPE const void* secret, size_t secretSize);

/*******   Streaming   *******/
#ifndef XXH_NO_STREAM
/*
 * Streaming requires state maintenance.
 * This operation costs memory and CPU.
 * As a consequence, streaming is slower than one-shot hashing.
 * For better performance, prefer one-shot functions whenever applicable.
 *
 * XXH3_128bits uses the same XXH3_state_t as XXH3_64bits().
 * Use already declared XXH3_createState() and XXH3_freeState().
 *
 * All reset and streaming functions have same meaning as their 64-bit counterpart.
 */

/*!
 * @brief Resets an @ref XXH3_state_t to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   - This function resets `statePtr` and generate a secret with default parameters.
 *   - Call it before @ref XXH3_128bits_update().
 *   - Digest will be equivalent to `XXH3_128bits()`.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH3_128bits_reset(XXH_NOESCAPE XXH3_state_t* statePtr);

/*!
 * @brief Resets an @ref XXH3_state_t with 64-bit seed to begin a new hash.
 *
 * @param statePtr The state struct to reset.
 * @param seed     The 64-bit seed to alter the hash result predictably.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   - This function resets `statePtr` and generate a secret from `seed`.
 *   - Call it before @ref XXH3_128bits_update().
 *   - Digest will be equivalent to `XXH3_128bits_withSeed()`.
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH3_128bits_reset_withSeed(XXH_NOESCAPE XXH3_state_t* statePtr, XXH64_hash_t seed);
/*!
 * @brief Resets an @ref XXH3_state_t with secret data to begin a new hash.
 *
 * @param statePtr   The state struct to reset.
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * `secret` is referenced, it _must outlive_ the hash streaming session.
 * Similar to one-shot API, `secretSize` must be >= @ref XXH3_SECRET_SIZE_MIN,
 * and the quality of produced hash values depends on secret's entropy
 * (secret's content should look like a bunch of random bytes).
 * When in doubt about the randomness of a candidate `secret`,
 * consider employing `XXH3_generateSecret()` instead (see below).
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH3_128bits_reset_withSecret(XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* secret, size_t secretSize);

/*!
 * @brief Consumes a block of @p input to an @ref XXH3_state_t.
 *
 * Call this to incrementally consume blocks of data.
 *
 * @param statePtr The state struct to update.
 * @param input The block of data to be hashed, at least @p length bytes in size.
 * @param length The length of @p input, in bytes.
 *
 * @pre
 *   @p statePtr must not be `NULL`.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @note
 *   The memory between @p input and @p input + @p length must be valid,
 *   readable, contiguous memory. However, if @p length is `0`, @p input may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 */
XXH_PUBLIC_API XXH_errorcode XXH3_128bits_update (XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* input, size_t length);

/*!
 * @brief Returns the calculated XXH3 128-bit hash value from an @ref XXH3_state_t.
 *
 * @param statePtr The state struct to calculate the hash from.
 *
 * @pre
 *  @p statePtr must not be `NULL`.
 *
 * @return The calculated XXH3 128-bit hash value from that state.
 *
 * @note
 *   Calling XXH3_128bits_digest() will not affect @p statePtr, so you can update,
 *   digest, and update again.
 *
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH3_128bits_digest (XXH_NOESCAPE const XXH3_state_t* statePtr);
#endif /* !XXH_NO_STREAM */

/* Following helper functions make it possible to compare XXH128_hast_t values.
 * Since XXH128_hash_t is a structure, this capability is not offered by the language.
 * Note: For better performance, these functions can be inlined using XXH_INLINE_ALL */

/*!
 * @brief Check equality of two XXH128_hash_t values
 *
 * @param h1 The 128-bit hash value.
 * @param h2 Another 128-bit hash value.
 *
 * @return `1` if `h1` and `h2` are equal.
 * @return `0` if they are not.
 */
XXH_PUBLIC_API XXH_PUREF int XXH128_isEqual(XXH128_hash_t h1, XXH128_hash_t h2);

/*!
 * @brief Compares two @ref XXH128_hash_t
 *
 * This comparator is compatible with stdlib's `qsort()`/`bsearch()`.
 *
 * @param h128_1 Left-hand side value
 * @param h128_2 Right-hand side value
 *
 * @return >0 if @p h128_1  > @p h128_2
 * @return =0 if @p h128_1 == @p h128_2
 * @return <0 if @p h128_1  < @p h128_2
 */
XXH_PUBLIC_API XXH_PUREF int XXH128_cmp(XXH_NOESCAPE const void* h128_1, XXH_NOESCAPE const void* h128_2);


/*******   Canonical representation   *******/
typedef struct { unsigned char digest[sizeof(XXH128_hash_t)]; } XXH128_canonical_t;


/*!
 * @brief Converts an @ref XXH128_hash_t to a big endian @ref XXH128_canonical_t.
 *
 * @param dst  The @ref XXH128_canonical_t pointer to be stored to.
 * @param hash The @ref XXH128_hash_t to be converted.
 *
 * @pre
 *   @p dst must not be `NULL`.
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API void XXH128_canonicalFromHash(XXH_NOESCAPE XXH128_canonical_t* dst, XXH128_hash_t hash);

/*!
 * @brief Converts an @ref XXH128_canonical_t to a native @ref XXH128_hash_t.
 *
 * @param src The @ref XXH128_canonical_t to convert.
 *
 * @pre
 *   @p src must not be `NULL`.
 *
 * @return The converted hash.
 * @see @ref canonical_representation_example "Canonical Representation Example"
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH128_hashFromCanonical(XXH_NOESCAPE const XXH128_canonical_t* src);


#endif  /* !XXH_NO_XXH3 */
#endif  /* XXH_NO_LONG_LONG */

/*!
 * @}
 */
#endif /* XXHASH_H_5627135585666179 */



#if defined(XXH_STATIC_LINKING_ONLY) && !defined(XXHASH_H_STATIC_13879238742)
#define XXHASH_H_STATIC_13879238742
/* ****************************************************************************
 * This section contains declarations which are not guaranteed to remain stable.
 * They may change in future versions, becoming incompatible with a different
 * version of the library.
 * These declarations should only be used with static linking.
 * Never use them in association with dynamic linking!
 ***************************************************************************** */

/*
 * These definitions are only present to allow static allocation
 * of XXH states, on stack or in a struct, for example.
 * Never **ever** access their members directly.
 */

/*!
 * @internal
 * @brief Structure for XXH32 streaming API.
 *
 * @note This is only defined when @ref XXH_STATIC_LINKING_ONLY,
 * @ref XXH_INLINE_ALL, or @ref XXH_IMPLEMENTATION is defined. Otherwise it is
 * an opaque type. This allows fields to safely be changed.
 *
 * Typedef'd to @ref XXH32_state_t.
 * Do not access the members of this struct directly.
 * @see XXH64_state_s, XXH3_state_s
 */
struct XXH32_state_s {
   XXH32_hash_t total_len_32; /*!< Total length hashed, modulo 2^32 */
   XXH32_hash_t large_len;    /*!< Whether the hash is >= 16 (handles @ref total_len_32 overflow) */
   XXH32_hash_t acc[4];       /*!< Accumulator lanes */
   unsigned char buffer[16];  /*!< Internal buffer for partial reads. */
   XXH32_hash_t bufferedSize; /*!< Amount of data in @ref buffer */
   XXH32_hash_t reserved;     /*!< Reserved field. Do not read nor write to it. */
};   /* typedef'd to XXH32_state_t */


#ifndef XXH_NO_LONG_LONG  /* defined when there is no 64-bit support */

/*!
 * @internal
 * @brief Structure for XXH64 streaming API.
 *
 * @note This is only defined when @ref XXH_STATIC_LINKING_ONLY,
 * @ref XXH_INLINE_ALL, or @ref XXH_IMPLEMENTATION is defined. Otherwise it is
 * an opaque type. This allows fields to safely be changed.
 *
 * Typedef'd to @ref XXH64_state_t.
 * Do not access the members of this struct directly.
 * @see XXH32_state_s, XXH3_state_s
 */
struct XXH64_state_s {
   XXH64_hash_t total_len;    /*!< Total length hashed. This is always 64-bit. */
   XXH64_hash_t acc[4];       /*!< Accumulator lanes */
   unsigned char buffer[32];  /*!< Internal buffer for partial reads.. */
   XXH32_hash_t bufferedSize; /*!< Amount of data in @ref buffer */
   XXH32_hash_t reserved32;   /*!< Reserved field, needed for padding anyways*/
   XXH64_hash_t reserved64;   /*!< Reserved field. Do not read or write to it. */
};   /* typedef'd to XXH64_state_t */

#ifndef XXH_NO_XXH3

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) /* >= C11 */
#  define XXH_ALIGN(n)      _Alignas(n)
#elif defined(__cplusplus) && (__cplusplus >= 201103L) /* >= C++11 */
/* In C++ alignas() is a keyword */
#  define XXH_ALIGN(n)      alignas(n)
#elif defined(__GNUC__)
#  define XXH_ALIGN(n)      __attribute__ ((aligned(n)))
#elif defined(_MSC_VER)
#  define XXH_ALIGN(n)      __declspec(align(n))
#else
#  define XXH_ALIGN(n)   /* disabled */
#endif

/* Old GCC versions only accept the attribute after the type in structures. */
#if !(defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L))   /* C11+ */ \
    && ! (defined(__cplusplus) && (__cplusplus >= 201103L)) /* >= C++11 */ \
    && defined(__GNUC__)
#   define XXH_ALIGN_MEMBER(align, type) type XXH_ALIGN(align)
#else
#   define XXH_ALIGN_MEMBER(align, type) XXH_ALIGN(align) type
#endif

/*!
 * @brief The size of the internal XXH3 buffer.
 *
 * This is the optimal update size for incremental hashing.
 *
 * @see XXH3_64b_update(), XXH3_128b_update().
 */
#define XXH3_INTERNALBUFFER_SIZE 256

/*!
 * @internal
 * @brief Default size of the secret buffer (and @ref XXH3_kSecret).
 *
 * This is the size used in @ref XXH3_kSecret and the seeded functions.
 *
 * Not to be confused with @ref XXH3_SECRET_SIZE_MIN.
 */
#define XXH3_SECRET_DEFAULT_SIZE 192

/*!
 * @internal
 * @brief Structure for XXH3 streaming API.
 *
 * @note This is only defined when @ref XXH_STATIC_LINKING_ONLY,
 * @ref XXH_INLINE_ALL, or @ref XXH_IMPLEMENTATION is defined.
 * Otherwise it is an opaque type.
 * Never use this definition in combination with dynamic library.
 * This allows fields to safely be changed in the future.
 *
 * @note ** This structure has a strict alignment requirement of 64 bytes!! **
 * Do not allocate this with `malloc()` or `new`,
 * it will not be sufficiently aligned.
 * Use @ref XXH3_createState() and @ref XXH3_freeState(), or stack allocation.
 *
 * Typedef'd to @ref XXH3_state_t.
 * Do never access the members of this struct directly.
 *
 * @see XXH3_INITSTATE() for stack initialization.
 * @see XXH3_createState(), XXH3_freeState().
 * @see XXH32_state_s, XXH64_state_s
 */
struct XXH3_state_s {
   XXH_ALIGN_MEMBER(64, XXH64_hash_t acc[8]);
       /*!< The 8 accumulators. See @ref XXH32_state_s::v and @ref XXH64_state_s::v */
   XXH_ALIGN_MEMBER(64, unsigned char customSecret[XXH3_SECRET_DEFAULT_SIZE]);
       /*!< Used to store a custom secret generated from a seed. */
   XXH_ALIGN_MEMBER(64, unsigned char buffer[XXH3_INTERNALBUFFER_SIZE]);
       /*!< The internal buffer. @see XXH32_state_s::mem32 */
   XXH32_hash_t bufferedSize;
       /*!< The amount of memory in @ref buffer, @see XXH32_state_s::memsize */
   XXH32_hash_t useSeed;
       /*!< Reserved field. Needed for padding on 64-bit. */
   size_t nbStripesSoFar;
       /*!< Number or stripes processed. */
   XXH64_hash_t totalLen;
       /*!< Total length hashed. 64-bit even on 32-bit targets. */
   size_t nbStripesPerBlock;
       /*!< Number of stripes per block. */
   size_t secretLimit;
       /*!< Size of @ref customSecret or @ref extSecret */
   XXH64_hash_t seed;
       /*!< Seed for _withSeed variants. Must be zero otherwise, @see XXH3_INITSTATE() */
   XXH64_hash_t reserved64;
       /*!< Reserved field. */
   const unsigned char* extSecret;
       /*!< Reference to an external secret for the _withSecret variants, NULL
        *   for other variants. */
   /* note: there may be some padding at the end due to alignment on 64 bytes */
}; /* typedef'd to XXH3_state_t */

#undef XXH_ALIGN_MEMBER

/*!
 * @brief Initializes a stack-allocated `XXH3_state_s`.
 *
 * When the @ref XXH3_state_t structure is merely emplaced on stack,
 * it should be initialized with XXH3_INITSTATE() or a memset()
 * in case its first reset uses XXH3_NNbits_reset_withSeed().
 * This init can be omitted if the first reset uses default or _withSecret mode.
 * This operation isn't necessary when the state is created with XXH3_createState().
 * Note that this doesn't prepare the state for a streaming operation,
 * it's still necessary to use XXH3_NNbits_reset*() afterwards.
 */
#define XXH3_INITSTATE(XXH3_state_ptr)                       \
    do {                                                     \
        XXH3_state_t* tmp_xxh3_state_ptr = (XXH3_state_ptr); \
        tmp_xxh3_state_ptr->seed = 0;                        \
        tmp_xxh3_state_ptr->extSecret = NULL;                \
    } while(0)


/*!
 * @brief Calculates the 128-bit hash of @p data using XXH3.
 *
 * @param data The block of data to be hashed, at least @p len bytes in size.
 * @param len  The length of @p data, in bytes.
 * @param seed The 64-bit seed to alter the hash's output predictably.
 *
 * @pre
 *   The memory between @p data and @p data + @p len must be valid,
 *   readable, contiguous memory. However, if @p len is `0`, @p data may be
 *   `NULL`. In C++, this also must be *TriviallyCopyable*.
 *
 * @return The calculated 128-bit XXH3 value.
 *
 * @see @ref single_shot_example "Single Shot Example" for an example.
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t XXH128(XXH_NOESCAPE const void* data, size_t len, XXH64_hash_t seed);


/* ===   Experimental API   === */
/* Symbols defined below must be considered tied to a specific library version. */

/*!
 * @brief Derive a high-entropy secret from any user-defined content, named customSeed.
 *
 * @param secretBuffer    A writable buffer for derived high-entropy secret data.
 * @param secretSize      Size of secretBuffer, in bytes.  Must be >= XXH3_SECRET_SIZE_MIN.
 * @param customSeed      A user-defined content.
 * @param customSeedSize  Size of customSeed, in bytes.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * The generated secret can be used in combination with `*_withSecret()` functions.
 * The `_withSecret()` variants are useful to provide a higher level of protection
 * than 64-bit seed, as it becomes much more difficult for an external actor to
 * guess how to impact the calculation logic.
 *
 * The function accepts as input a custom seed of any length and any content,
 * and derives from it a high-entropy secret of length @p secretSize into an
 * already allocated buffer @p secretBuffer.
 *
 * The generated secret can then be used with any `*_withSecret()` variant.
 * The functions @ref XXH3_128bits_withSecret(), @ref XXH3_64bits_withSecret(),
 * @ref XXH3_128bits_reset_withSecret() and @ref XXH3_64bits_reset_withSecret()
 * are part of this list. They all accept a `secret` parameter
 * which must be large enough for implementation reasons (>= @ref XXH3_SECRET_SIZE_MIN)
 * _and_ feature very high entropy (consist of random-looking bytes).
 * These conditions can be a high bar to meet, so @ref XXH3_generateSecret() can
 * be employed to ensure proper quality.
 *
 * @p customSeed can be anything. It can have any size, even small ones,
 * and its content can be anything, even "poor entropy" sources such as a bunch
 * of zeroes. The resulting `secret` will nonetheless provide all required qualities.
 *
 * @pre
 *   - @p secretSize must be >= @ref XXH3_SECRET_SIZE_MIN
 *   - When @p customSeedSize > 0, supplying NULL as customSeed is undefined behavior.
 *
 * Example code:
 * @code{.c}
 *    #include <stdio.h>
 *    #include <stdlib.h>
 *    #include <string.h>
 *    #define XXH_STATIC_LINKING_ONLY // expose unstable API
 *    #include "xxhash.h"
 *    // Hashes argv[2] using the entropy from argv[1].
 *    int main(int argc, char* argv[])
 *    {
 *        char secret[XXH3_SECRET_SIZE_MIN];
 *        if (argv != 3) { return 1; }
 *        XXH3_generateSecret(secret, sizeof(secret), argv[1], strlen(argv[1]));
 *        XXH64_hash_t h = XXH3_64bits_withSecret(
 *             argv[2], strlen(argv[2]),
 *             secret, sizeof(secret)
 *        );
 *        printf("%016llx\n", (unsigned long long) h);
 *    }
 * @endcode
 */
XXH_PUBLIC_API XXH_errorcode XXH3_generateSecret(XXH_NOESCAPE void* secretBuffer, size_t secretSize, XXH_NOESCAPE const void* customSeed, size_t customSeedSize);

/*!
 * @brief Generate the same secret as the _withSeed() variants.
 *
 * @param secretBuffer A writable buffer of @ref XXH3_SECRET_DEFAULT_SIZE bytes
 * @param seed         The 64-bit seed to alter the hash result predictably.
 *
 * The generated secret can be used in combination with
 *`*_withSecret()` and `_withSecretandSeed()` variants.
 *
 * Example C++ `std::string` hash class:
 * @code{.cpp}
 *    #include <string>
 *    #define XXH_STATIC_LINKING_ONLY // expose unstable API
 *    #include "xxhash.h"
 *    // Slow, seeds each time
 *    class HashSlow {
 *        XXH64_hash_t seed;
 *    public:
 *        HashSlow(XXH64_hash_t s) : seed{s} {}
 *        size_t operator()(const std::string& x) const {
 *            return size_t{XXH3_64bits_withSeed(x.c_str(), x.length(), seed)};
 *        }
 *    };
 *    // Fast, caches the seeded secret for future uses.
 *    class HashFast {
 *        unsigned char secret[XXH3_SECRET_DEFAULT_SIZE];
 *    public:
 *        HashFast(XXH64_hash_t s) {
 *            XXH3_generateSecret_fromSeed(secret, seed);
 *        }
 *        size_t operator()(const std::string& x) const {
 *            return size_t{
 *                XXH3_64bits_withSecret(x.c_str(), x.length(), secret, sizeof(secret))
 *            };
 *        }
 *    };
 * @endcode
 */
XXH_PUBLIC_API void XXH3_generateSecret_fromSeed(XXH_NOESCAPE void* secretBuffer, XXH64_hash_t seed);

/*!
 * @brief Maximum size of "short" key in bytes.
 */
#define XXH3_MIDSIZE_MAX 240

/*!
 * @brief Calculates 64/128-bit seeded variant of XXH3 hash of @p data.
 *
 * @param data       The block of data to be hashed, at least @p len bytes in size.
 * @param len        The length of @p data, in bytes.
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 * @param seed       The 64-bit seed to alter the hash result predictably.
 *
 * These variants generate hash values using either:
 * - @p seed for "short" keys (< @ref XXH3_MIDSIZE_MAX = 240 bytes)
 * - @p secret for "large" keys (>= @ref XXH3_MIDSIZE_MAX).
 *
 * This generally benefits speed, compared to `_withSeed()` or `_withSecret()`.
 * `_withSeed()` has to generate the secret on the fly for "large" keys.
 * It's fast, but can be perceptible for "not so large" keys (< 1 KB).
 * `_withSecret()` has to generate the masks on the fly for "small" keys,
 * which requires more instructions than _withSeed() variants.
 * Therefore, _withSecretandSeed variant combines the best of both worlds.
 *
 * When @p secret has been generated by XXH3_generateSecret_fromSeed(),
 * this variant produces *exactly* the same results as `_withSeed()` variant,
 * hence offering only a pure speed benefit on "large" input,
 * by skipping the need to regenerate the secret for every large input.
 *
 * Another usage scenario is to hash the secret to a 64-bit hash value,
 * for example with XXH3_64bits(), which then becomes the seed,
 * and then employ both the seed and the secret in _withSecretandSeed().
 * On top of speed, an added benefit is that each bit in the secret
 * has a 50% chance to swap each bit in the output, via its impact to the seed.
 *
 * This is not guaranteed when using the secret directly in "small data" scenarios,
 * because only portions of the secret are employed for small data.
 */
XXH_PUBLIC_API XXH_PUREF XXH64_hash_t
XXH3_64bits_withSecretandSeed(XXH_NOESCAPE const void* data, size_t len,
                              XXH_NOESCAPE const void* secret, size_t secretSize,
                              XXH64_hash_t seed);

/*!
 * @brief Calculates 128-bit seeded variant of XXH3 hash of @p data.
 *
 * @param data       The memory segment to be hashed, at least @p len bytes in size.
 * @param length     The length of @p data, in bytes.
 * @param secret     The secret used to alter hash result predictably.
 * @param secretSize The length of @p secret, in bytes (must be >= XXH3_SECRET_SIZE_MIN)
 * @param seed64     The 64-bit seed to alter the hash result predictably.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @see XXH3_64bits_withSecretandSeed(): contract is the same.
 */
XXH_PUBLIC_API XXH_PUREF XXH128_hash_t
XXH3_128bits_withSecretandSeed(XXH_NOESCAPE const void* input, size_t length,
                               XXH_NOESCAPE const void* secret, size_t secretSize,
                               XXH64_hash_t seed64);

#ifndef XXH_NO_STREAM
/*!
 * @brief Resets an @ref XXH3_state_t with secret data to begin a new hash.
 *
 * @param statePtr   A pointer to an @ref XXH3_state_t allocated with @ref XXH3_createState().
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 * @param seed64     The 64-bit seed to alter the hash result predictably.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @see XXH3_64bits_withSecretandSeed(). Contract is identical.
 */
XXH_PUBLIC_API XXH_errorcode
XXH3_64bits_reset_withSecretandSeed(XXH_NOESCAPE XXH3_state_t* statePtr,
                                    XXH_NOESCAPE const void* secret, size_t secretSize,
                                    XXH64_hash_t seed64);

/*!
 * @brief Resets an @ref XXH3_state_t with secret data to begin a new hash.
 *
 * @param statePtr   A pointer to an @ref XXH3_state_t allocated with @ref XXH3_createState().
 * @param secret     The secret data.
 * @param secretSize The length of @p secret, in bytes.
 * @param seed64     The 64-bit seed to alter the hash result predictably.
 *
 * @return @ref XXH_OK on success.
 * @return @ref XXH_ERROR on failure.
 *
 * @see XXH3_64bits_withSecretandSeed(). Contract is identical.
 *
 * Note: there was a bug in an earlier version of this function (<= v0.8.2)
 * that would make it generate an incorrect hash value
 * when @p seed == 0 and @p length < XXH3_MIDSIZE_MAX
 * and @p secret is different from XXH3_generateSecret_fromSeed().
 * As stated in the contract, the correct hash result must be
 * the same as XXH3_128bits_withSeed() when @p length <= XXH3_MIDSIZE_MAX.
 * Results generated by this older version are wrong, hence not comparable.
 */
XXH_PUBLIC_API XXH_errorcode
XXH3_128bits_reset_withSecretandSeed(XXH_NOESCAPE XXH3_state_t* statePtr,
                                     XXH_NOESCAPE const void* secret, size_t secretSize,
                                     XXH64_hash_t seed64);

#endif /* !XXH_NO_STREAM */

#endif  /* !XXH_NO_XXH3 */
#endif  /* XXH_NO_LONG_LONG */
#if defined(XXH_INLINE_ALL) || defined(XXH_PRIVATE_API)
#  define XXH_IMPLEMENTATION
#endif

#endif  /* defined(XXH_STATIC_LINKING_ONLY) && !defined(XXHASH_H_STATIC_13879238742) */


/* ======================================================================== */
/* ======================================================================== */
/* ======================================================================== */


/*-**********************************************************************
 * xxHash implementation
 *-**********************************************************************
 * xxHash's implementation used to be hosted inside xxhash.c.
 *
 * However, inlining requires implementation to be visible to the compiler,
 * hence be included alongside the header.
 * Previously, implementation was hosted inside xxhash.c,
 * which was then #included when inlining was activated.
 * This construction created issues with a few build and install systems,
 * as it required xxhash.c to be stored in /include directory.
 *
 * xxHash implementation is now directly integrated within xxhash.h.
 * As a consequence, xxhash.c is no longer needed in /include.
 *
 * xxhash.c is still available and is still useful.
 * In a "normal" setup, when xxhash is not inlined,
 * xxhash.h only exposes the prototypes and public symbols,
 * while xxhash.c can be built into an object file xxhash.o
 * which can then be linked into the final binary.
 ************************************************************************/

#if ( defined(XXH_INLINE_ALL) || defined(XXH_PRIVATE_API) \
   || defined(XXH_IMPLEMENTATION) ) && !defined(XXH_IMPLEM_13a8737387)
#  define XXH_IMPLEM_13a8737387

/* *************************************
*  Tuning parameters
***************************************/

/*!
 * @defgroup tuning Tuning parameters
 * @{
 *
 * Various macros to control xxHash's behavior.
 */
#ifdef XXH_DOXYGEN
/*!
 * @brief Define this to disable 64-bit code.
 *
 * Useful if only using the @ref XXH32_family and you have a strict C90 compiler.
 */
#  define XXH_NO_LONG_LONG
#  undef XXH_NO_LONG_LONG /* don't actually */
/*!
 * @brief Controls how unaligned memory is accessed.
 *
 * By default, access to unaligned memory is controlled by `memcpy()`, which is
 * safe and portable.
 *
 * Unfortunately, on some target/compiler combinations, the generated assembly
 * is sub-optimal.
 *
 * The below switch allow selection of a different access method
 * in the search for improved performance.
 *
 * @par Possible options:
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=0` (default): `memcpy`
 *   @par
 *     Use `memcpy()`. Safe and portable. Note that most modern compilers will
 *     eliminate the function call and treat it as an unaligned access.
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=1`: `__attribute__((aligned(1)))`
 *   @par
 *     Depends on compiler extensions and is therefore not portable.
 *     This method is safe _if_ your compiler supports it,
 *     and *generally* as fast or faster than `memcpy`.
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=2`: Direct cast
 *  @par
 *     Casts directly and dereferences. This method doesn't depend on the
 *     compiler, but it violates the C standard as it directly dereferences an
 *     unaligned pointer. It can generate buggy code on targets which do not
 *     support unaligned memory accesses, but in some circumstances, it's the
 *     only known way to get the most performance.
 *
 *  - `XXH_FORCE_MEMORY_ACCESS=3`: Byteshift
 *  @par
 *     Also portable. This can generate the best code on old compilers which don't
 *     inline small `memcpy()` calls, and it might also be faster on big-endian
 *     systems which lack a native byteswap instruction. However, some compilers
 *     will emit literal byteshifts even if the target supports unaligned access.
 *
 *
 * @warning
 *   Methods 1 and 2 rely on implementation-defined behavior. Use these with
 *   care, as what works on one compiler/platform/optimization level may cause
 *   another to read garbage data or even crash.
 *
 * See https://fastcompression.blogspot.com/2015/08/accessing-unaligned-memory.html for details.
 *
 * Prefer these methods in priority order (0 > 3 > 1 > 2)
 */
#  define XXH_FORCE_MEMORY_ACCESS 0

/*!
 * @def XXH_SIZE_OPT
 * @brief Controls how much xxHash optimizes for size.
 *
 * xxHash, when compiled, tends to result in a rather large binary size. This
 * is mostly due to heavy usage to forced inlining and constant folding of the
 * @ref XXH3_family to increase performance.
 *
 * However, some developers prefer size over speed. This option can
 * significantly reduce the size of the generated code. When using the `-Os`
 * or `-Oz` options on GCC or Clang, this is defined to 1 by default,
 * otherwise it is defined to 0.
 *
 * Most of these size optimizations can be controlled manually.
 *
 * This is a number from 0-2.
 *  - `XXH_SIZE_OPT` == 0: Default. xxHash makes no size optimizations. Speed
 *    comes first.
 *  - `XXH_SIZE_OPT` == 1: Default for `-Os` and `-Oz`. xxHash is more
 *    conservative and disables hacks that increase code size. It implies the
 *    options @ref XXH_NO_INLINE_HINTS == 1, @ref XXH_FORCE_ALIGN_CHECK == 0,
 *    and @ref XXH3_NEON_LANES == 8 if they are not already defined.
 *  - `XXH_SIZE_OPT` == 2: xxHash tries to make itself as small as possible.
 *    Performance may cry. For example, the single shot functions just use the
 *    streaming API.
 */
#  define XXH_SIZE_OPT 0

/*!
 * @def XXH_FORCE_ALIGN_CHECK
 * @brief If defined to non-zero, adds a special path for aligned inputs (XXH32()
 * and XXH64() only).
 *
 * This is an important performance trick for architectures without decent
 * unaligned memory access performance.
 *
 * It checks for input alignment, and when conditions are met, uses a "fast
 * path" employing direct 32-bit/64-bit reads, resulting in _dramatically
 * faster_ read speed.
 *
 * The check costs one initial branch per hash, which is generally negligible,
 * but not zero.
 *
 * Moreover, it's not useful to generate an additional code path if memory
 * access uses the same instruction for both aligned and unaligned
 * addresses (e.g. x86 and aarch64).
 *
 * In these cases, the alignment check can be removed by setting this macro to 0.
 * Then the code will always use unaligned memory access.
 * Align check is automatically disabled on x86, x64, ARM64, and some ARM chips
 * which are platforms known to offer good unaligned memory accesses performance.
 *
 * It is also disabled by default when @ref XXH_SIZE_OPT >= 1.
 *
 * This option does not affect XXH3 (only XXH32 and XXH64).
 */
#  define XXH_FORCE_ALIGN_CHECK 0

/*!
 * @def XXH_NO_INLINE_HINTS
 * @brief When non-zero, sets all functions to `static`.
 *
 * By default, xxHash tries to force the compiler to inline almost all internal
 * functions.
 *
 * This can usually improve performance due to reduced jumping and improved
 * constant folding, but significantly increases the size of the binary which
 * might not be favorable.
 *
 * Additionally, sometimes the forced inlining can be detrimental to performance,
 * depending on the architecture.
 *
 * XXH_NO_INLINE_HINTS marks all internal functions as static, giving the
 * compiler full control on whether to inline or not.
 *
 * When not optimizing (-O0), using `-fno-inline` with GCC or Clang, or if
 * @ref XXH_SIZE_OPT >= 1, this will automatically be defined.
 */
#  define XXH_NO_INLINE_HINTS 0

/*!
 * @def XXH3_INLINE_SECRET
 * @brief Determines whether to inline the XXH3 withSecret code.
 *
 * When the secret size is known, the compiler can improve the performance
 * of XXH3_64bits_withSecret() and XXH3_128bits_withSecret().
 *
 * However, if the secret size is not known, it doesn't have any benefit. This
 * happens when xxHash is compiled into a global symbol. Therefore, if
 * @ref XXH_INLINE_ALL is *not* defined, this will be defined to 0.
 *
 * Additionally, this defaults to 0 on GCC 12+, which has an issue with function pointers
 * that are *sometimes* force inline on -Og, and it is impossible to automatically
 * detect this optimization level.
 */
#  define XXH3_INLINE_SECRET 0

/*!
 * @def XXH32_ENDJMP
 * @brief Whether to use a jump for `XXH32_finalize`.
 *
 * For performance, `XXH32_finalize` uses multiple branches in the finalizer.
 * This is generally preferable for performance,
 * but depending on exact architecture, a jmp may be preferable.
 *
 * This setting is only possibly making a difference for very small inputs.
 */
#  define XXH32_ENDJMP 0

/*!
 * @internal
 * @brief Redefines old internal names.
 *
 * For compatibility with code that uses xxHash's internals before the names
 * were changed to improve namespacing. There is no other reason to use this.
 */
#  define XXH_OLD_NAMES
#  undef XXH_OLD_NAMES /* don't actually use, it is ugly. */

/*!
 * @def XXH_NO_STREAM
 * @brief Disables the streaming API.
 *
 * When xxHash is not inlined and the streaming functions are not used, disabling
 * the streaming functions can improve code size significantly, especially with
 * the @ref XXH3_family which tends to make constant folded copies of itself.
 */
#  define XXH_NO_STREAM
#  undef XXH_NO_STREAM /* don't actually */
#endif /* XXH_DOXYGEN */
/*!
 * @}
 */

#ifndef XXH_FORCE_MEMORY_ACCESS   /* can be defined externally, on command line for example */
   /* prefer __packed__ structures (method 1) for GCC
    * < ARMv7 with unaligned access (e.g. Raspbian armhf) still uses byte shifting, so we use memcpy
    * which for some reason does unaligned loads. */
#  if defined(__GNUC__) && !(defined(__ARM_ARCH) && __ARM_ARCH < 7 && defined(__ARM_FEATURE_UNALIGNED))
#    define XXH_FORCE_MEMORY_ACCESS 1
#  endif
#endif

#ifndef XXH_SIZE_OPT
   /* default to 1 for -Os or -Oz */
#  if (defined(__GNUC__) || defined(__clang__)) && defined(__OPTIMIZE_SIZE__)
#    define XXH_SIZE_OPT 1
#  else
#    define XXH_SIZE_OPT 0
#  endif
#endif

#ifndef XXH_FORCE_ALIGN_CHECK  /* can be defined externally */
   /* don't check on sizeopt, x86, aarch64, or arm when unaligned access is available */
#  if XXH_SIZE_OPT >= 1 || \
      defined(__i386)  || defined(__x86_64__) || defined(__aarch64__) || defined(__ARM_FEATURE_UNALIGNED) \
   || defined(_M_IX86) || defined(_M_X64)     || defined(_M_ARM64)    || defined(_M_ARM) /* visual */
#    define XXH_FORCE_ALIGN_CHECK 0
#  else
#    define XXH_FORCE_ALIGN_CHECK 1
#  endif
#endif

#ifndef XXH_NO_INLINE_HINTS
#  if XXH_SIZE_OPT >= 1 || defined(__NO_INLINE__)  /* -O0, -fno-inline */
#    define XXH_NO_INLINE_HINTS 1
#  else
#    define XXH_NO_INLINE_HINTS 0
#  endif
#endif

#ifndef XXH3_INLINE_SECRET
#  if (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12) \
     || !defined(XXH_INLINE_ALL)
#    define XXH3_INLINE_SECRET 0
#  else
#    define XXH3_INLINE_SECRET 1
#  endif
#endif

#ifndef XXH32_ENDJMP
/* generally preferable for performance */
#  define XXH32_ENDJMP 0
#endif

/*!
 * @defgroup impl Implementation
 * @{
 */


/* *************************************
*  Includes & Memory related functions
***************************************/
#if defined(XXH_NO_STREAM)
/* nothing */
#elif defined(XXH_NO_STDLIB)

/* When requesting to disable any mention of stdlib,
 * the library loses the ability to invoked malloc / free.
 * In practice, it means that functions like `XXH*_createState()`
 * will always fail, and return NULL.
 * This flag is useful in situations where
 * xxhash.h is integrated into some kernel, embedded or limited environment
 * without access to dynamic allocation.
 */

static XXH_CONSTF void* XXH_malloc(size_t s) { (void)s; return NULL; }
static void XXH_free(void* p) { (void)p; }

#else

/*
 * Modify the local functions below should you wish to use
 * different memory routines for malloc() and free()
 */
#include <stdlib.h>

/*!
 * @internal
 * @brief Modify this function to use a different routine than malloc().
 */
static XXH_MALLOCF void* XXH_malloc(size_t s) { return malloc(s); }

/*!
 * @internal
 * @brief Modify this function to use a different routine than free().
 */
static void XXH_free(void* p) { free(p); }

#endif  /* XXH_NO_STDLIB */

#include <string.h>

/*!
 * @internal
 * @brief Modify this function to use a different routine than memcpy().
 */
static void* XXH_memcpy(void* dest, const void* src, size_t size)
{
    return memcpy(dest,src,size);
}

#include <limits.h>   /* ULLONG_MAX */


/* *************************************
*  Compiler Specific Options
***************************************/
#ifdef _MSC_VER /* Visual Studio warning fix */
#  pragma warning(disable : 4127) /* disable: C4127: conditional expression is constant */
#endif

#if XXH_NO_INLINE_HINTS  /* disable inlining hints */
#  if defined(__GNUC__) || defined(__clang__)
#    define XXH_FORCE_INLINE static __attribute__((__unused__))
#  else
#    define XXH_FORCE_INLINE static
#  endif
#  define XXH_NO_INLINE static
/* enable inlining hints */
#elif defined(__GNUC__) || defined(__clang__)
#  define XXH_FORCE_INLINE static __inline__ __attribute__((__always_inline__, __unused__))
#  define XXH_NO_INLINE static __attribute__((__noinline__))
#elif defined(_MSC_VER)  /* Visual Studio */
#  define XXH_FORCE_INLINE static __forceinline
#  define XXH_NO_INLINE static __declspec(noinline)
#elif defined (__cplusplus) \
  || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L))   /* C99 */
#  define XXH_FORCE_INLINE static inline
#  define XXH_NO_INLINE static
#else
#  define XXH_FORCE_INLINE static
#  define XXH_NO_INLINE static
#endif

#if defined(XXH_INLINE_ALL)
#  define XXH_STATIC XXH_FORCE_INLINE
#else
#  define XXH_STATIC static
#endif

#if XXH3_INLINE_SECRET
#  define XXH3_WITH_SECRET_INLINE XXH_FORCE_INLINE
#else
#  define XXH3_WITH_SECRET_INLINE XXH_NO_INLINE
#endif

#if ((defined(sun) || defined(__sun)) && __cplusplus) /* Solaris includes __STDC_VERSION__ with C++. Tested with GCC 5.5 */
#  define XXH_RESTRICT   /* disable */
#elif defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* >= C99 */
#  define XXH_RESTRICT   restrict
#elif (defined (__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))) \
   || (defined (__clang__)) \
   || (defined (_MSC_VER) && (_MSC_VER >= 1400)) \
   || (defined (__INTEL_COMPILER) && (__INTEL_COMPILER >= 1300))
/*
 * There are a LOT more compilers that recognize __restrict but this
 * covers the major ones.
 */
#  define XXH_RESTRICT   __restrict
#else
#  define XXH_RESTRICT   /* disable */
#endif

/* *************************************
*  Debug
***************************************/
/*!
 * @ingroup tuning
 * @def XXH_DEBUGLEVEL
 * @brief Sets the debugging level.
 *
 * XXH_DEBUGLEVEL is expected to be defined externally, typically via the
 * compiler's command line options. The value must be a number.
 */
#ifndef XXH_DEBUGLEVEL
#  ifdef DEBUGLEVEL /* backwards compat */
#    define XXH_DEBUGLEVEL DEBUGLEVEL
#  else
#    define XXH_DEBUGLEVEL 0
#  endif
#endif

#if (XXH_DEBUGLEVEL>=1)
#  include <assert.h>   /* note: can still be disabled with NDEBUG */
#  define XXH_ASSERT(c)   assert(c)
#else
#  if defined(__INTEL_COMPILER)
#    define XXH_ASSERT(c)   XXH_ASSUME((unsigned char) (c))
#  else
#    define XXH_ASSERT(c)   XXH_ASSUME(c)
#  endif
#endif

/* note: use after variable declarations */
#ifndef XXH_STATIC_ASSERT
#  if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)    /* C11 */
#    define XXH_STATIC_ASSERT_WITH_MESSAGE(c,m) do { _Static_assert((c),m); } while(0)
#  elif defined(__cplusplus) && (__cplusplus >= 201103L)            /* C++11 */
#    define XXH_STATIC_ASSERT_WITH_MESSAGE(c,m) do { static_assert((c),m); } while(0)
#  else
#    define XXH_STATIC_ASSERT_WITH_MESSAGE(c,m) do { struct xxh_sa { char x[(c) ? 1 : -1]; }; } while(0)
#  endif
#  define XXH_STATIC_ASSERT(c) XXH_STATIC_ASSERT_WITH_MESSAGE((c),#c)
#endif

/*!
 * @internal
 * @def XXH_COMPILER_GUARD(var)
 * @brief Used to prevent unwanted optimizations for @p var.
 *
 * It uses an empty GCC inline assembly statement with a register constraint
 * which forces @p var into a general purpose register (eg eax, ebx, ecx
 * on x86) and marks it as modified.
 *
 * This is used in a few places to avoid unwanted autovectorization (e.g.
 * XXH32_round()). All vectorization we want is explicit via intrinsics,
 * and _usually_ isn't wanted elsewhere.
 *
 * We also use it to prevent unwanted constant folding for AArch64 in
 * XXH3_initCustomSecret_scalar().
 */
#if defined(__GNUC__) || defined(__clang__)
#  define XXH_COMPILER_GUARD(var) __asm__("" : "+r" (var))
#else
#  define XXH_COMPILER_GUARD(var) ((void)0)
#endif

/* Specifically for NEON vectors which use the "w" constraint, on
 * Clang. */
#if defined(__clang__) && defined(__ARM_ARCH) && !defined(__wasm__)
#  define XXH_COMPILER_GUARD_CLANG_NEON(var) __asm__("" : "+w" (var))
#else
#  define XXH_COMPILER_GUARD_CLANG_NEON(var) ((void)0)
#endif

/* *************************************
*  Basic Types
***************************************/
#if !defined (__VMS) \
 && (defined (__cplusplus) \
 || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
#   ifdef _AIX
#     include <inttypes.h>
#   else
#     include <stdint.h>
#   endif
    typedef uint8_t xxh_u8;
#else
    typedef unsigned char xxh_u8;
#endif
typedef XXH32_hash_t xxh_u32;

#ifdef XXH_OLD_NAMES
#  warning "XXH_OLD_NAMES is planned to be removed starting v0.9. If the program depends on it, consider moving away from it by employing newer type names directly"
#  define BYTE xxh_u8
#  define U8   xxh_u8
#  define U32  xxh_u32
#endif

/* ***   Memory access   *** */

/*!
 * @internal
 * @fn xxh_u32 XXH_read32(const void* ptr)
 * @brief Reads an unaligned 32-bit integer from @p ptr in native endianness.
 *
 * Affected by @ref XXH_FORCE_MEMORY_ACCESS.
 *
 * @param ptr The pointer to read from.
 * @return The 32-bit native endian integer from the bytes at @p ptr.
 */

/*!
 * @internal
 * @fn xxh_u32 XXH_readLE32(const void* ptr)
 * @brief Reads an unaligned 32-bit little endian integer from @p ptr.
 *
 * Affected by @ref XXH_FORCE_MEMORY_ACCESS.
 *
 * @param ptr The pointer to read from.
 * @return The 32-bit little endian integer from the bytes at @p ptr.
 */

/*!
 * @internal
 * @fn xxh_u32 XXH_readBE32(const void* ptr)
 * @brief Reads an unaligned 32-bit big endian integer from @p ptr.
 *
 * Affected by @ref XXH_FORCE_MEMORY_ACCESS.
 *
 * @param ptr The pointer to read from.
 * @return The 32-bit big endian integer from the bytes at @p ptr.
 */

/*!
 * @internal
 * @fn xxh_u32 XXH_readLE32_align(const void* ptr, XXH_alignment align)
 * @brief Like @ref XXH_readLE32(), but has an option for aligned reads.
 *
 * Affected by @ref XXH_FORCE_MEMORY_ACCESS.
 * Note that when @ref XXH_FORCE_ALIGN_CHECK == 0, the @p align parameter is
 * always @ref XXH_alignment::XXH_unaligned.
 *
 * @param ptr The pointer to read from.
 * @param align Whether @p ptr is aligned.
 * @pre
 *   If @p align == @ref XXH_alignment::XXH_aligned, @p ptr must be 4 byte
 *   aligned.
 * @return The 32-bit little endian integer from the bytes at @p ptr.
 */

#if (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==3))
/*
 * Manual byteshift. Best for old compilers which don't inline memcpy.
 * We actually directly use XXH_readLE32 and XXH_readBE32.
 */
#elif (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==2))

/*
 * Force direct memory access. Only works on CPU which support unaligned memory
 * access in hardware.
 */
static xxh_u32 XXH_read32(const void* memPtr) { return *(const xxh_u32*) memPtr; }

#elif (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==1))

/*
 * __attribute__((aligned(1))) is supported by gcc and clang. Originally the
 * documentation claimed that it only increased the alignment, but actually it
 * can decrease it on gcc, clang, and icc:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69502,
 * https://gcc.godbolt.org/z/xYez1j67Y.
 */
#ifdef XXH_OLD_NAMES
typedef union { xxh_u32 u32; } __attribute__((__packed__)) unalign;
#endif
static xxh_u32 XXH_read32(const void* ptr)
{
    typedef __attribute__((__aligned__(1))) xxh_u32 xxh_unalign32;
    return *((const xxh_unalign32*)ptr);
}

#else

/*
 * Portable and safe solution. Generally efficient.
 * see: https://fastcompression.blogspot.com/2015/08/accessing-unaligned-memory.html
 */
static xxh_u32 XXH_read32(const void* memPtr)
{
    xxh_u32 val;
    XXH_memcpy(&val, memPtr, sizeof(val));
    return val;
}

#endif   /* XXH_FORCE_DIRECT_MEMORY_ACCESS */


/* ***   Endianness   *** */

/*!
 * @ingroup tuning
 * @def XXH_CPU_LITTLE_ENDIAN
 * @brief Whether the target is little endian.
 *
 * Defined to 1 if the target is little endian, or 0 if it is big endian.
 * It can be defined externally, for example on the compiler command line.
 *
 * If it is not defined,
 * a runtime check (which is usually constant folded) is used instead.
 *
 * @note
 *   This is not necessarily defined to an integer constant.
 *
 * @see XXH_isLittleEndian() for the runtime check.
 */
#ifndef XXH_CPU_LITTLE_ENDIAN
/*
 * Try to detect endianness automatically, to avoid the nonstandard behavior
 * in `XXH_isLittleEndian()`
 */
#  if defined(_WIN32) /* Windows is always little endian */ \
     || defined(__LITTLE_ENDIAN__) \
     || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#    define XXH_CPU_LITTLE_ENDIAN 1
#  elif defined(__BIG_ENDIAN__) \
     || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#    define XXH_CPU_LITTLE_ENDIAN 0
#  else
/*!
 * @internal
 * @brief Runtime check for @ref XXH_CPU_LITTLE_ENDIAN.
 *
 * Most compilers will constant fold this.
 */
static int XXH_isLittleEndian(void)
{
    /*
     * Portable and well-defined behavior.
     * Don't use static: it is detrimental to performance.
     */
    const union { xxh_u32 u; xxh_u8 c[4]; } one = { 1 };
    return one.c[0];
}
#   define XXH_CPU_LITTLE_ENDIAN   XXH_isLittleEndian()
#  endif
#endif




/* ****************************************
*  Compiler-specific Functions and Macros
******************************************/
#define XXH_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)

#ifdef __has_builtin
#  define XXH_HAS_BUILTIN(x) __has_builtin(x)
#else
#  define XXH_HAS_BUILTIN(x) 0
#endif



/*
 * C23 and future versions have standard "unreachable()".
 * Once it has been implemented reliably we can add it as an
 * additional case:
 *
 * ```
 * #if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= XXH_C23_VN)
 * #  include <stddef.h>
 * #  ifdef unreachable
 * #    define XXH_UNREACHABLE() unreachable()
 * #  endif
 * #endif
 * ```
 *
 * Note C++23 also has std::unreachable() which can be detected
 * as follows:
 * ```
 * #if defined(__cpp_lib_unreachable) && (__cpp_lib_unreachable >= 202202L)
 * #  include <utility>
 * #  define XXH_UNREACHABLE() std::unreachable()
 * #endif
 * ```
 * NB: `__cpp_lib_unreachable` is defined in the `<version>` header.
 * We don't use that as including `<utility>` in `extern "C"` blocks
 * doesn't work on GCC12
 */

#if XXH_HAS_BUILTIN(__builtin_unreachable)
#  define XXH_UNREACHABLE() __builtin_unreachable()

#elif defined(_MSC_VER)
#  define XXH_UNREACHABLE() __assume(0)

#else
#  define XXH_UNREACHABLE()
#endif

#if XXH_HAS_BUILTIN(__builtin_assume)
#  define XXH_ASSUME(c) __builtin_assume(c)
#else
#  define XXH_ASSUME(c) if (!(c)) { XXH_UNREACHABLE(); }
#endif

/*!
 * @internal
 * @def XXH_rotl32(x,r)
 * @brief 32-bit rotate left.
 *
 * @param x The 32-bit integer to be rotated.
 * @param r The number of bits to rotate.
 * @pre
 *   @p r > 0 && @p r < 32
 * @note
 *   @p x and @p r may be evaluated multiple times.
 * @return The rotated result.
 */
#if !defined(NO_CLANG_BUILTIN) && XXH_HAS_BUILTIN(__builtin_rotateleft32) \
                               && XXH_HAS_BUILTIN(__builtin_rotateleft64)
#  define XXH_rotl32 __builtin_rotateleft32
#  define XXH_rotl64 __builtin_rotateleft64
#elif XXH_HAS_BUILTIN(__builtin_stdc_rotate_left)
#  define XXH_rotl32 __builtin_stdc_rotate_left
#  define XXH_rotl64 __builtin_stdc_rotate_left
/* Note: although _rotl exists for minGW (GCC under windows), performance seems poor */
#elif defined(_MSC_VER)
#  define XXH_rotl32(x,r) _rotl(x,r)
#  define XXH_rotl64(x,r) _rotl64(x,r)
#else
#  define XXH_rotl32(x,r) (((x) << (r)) | ((x) >> (32 - (r))))
#  define XXH_rotl64(x,r) (((x) << (r)) | ((x) >> (64 - (r))))
#endif

/*!
 * @internal
 * @fn xxh_u32 XXH_swap32(xxh_u32 x)
 * @brief A 32-bit byteswap.
 *
 * @param x The 32-bit integer to byteswap.
 * @return @p x, byteswapped.
 */
#if defined(_MSC_VER)     /* Visual Studio */
#  define XXH_swap32 _byteswap_ulong
#elif XXH_GCC_VERSION >= 403
#  define XXH_swap32 __builtin_bswap32
#else
static xxh_u32 XXH_swap32 (xxh_u32 x)
{
    return  ((x << 24) & 0xff000000 ) |
            ((x <<  8) & 0x00ff0000 ) |
            ((x >>  8) & 0x0000ff00 ) |
            ((x >> 24) & 0x000000ff );
}
#endif


/* ***************************
*  Memory reads
*****************************/

/*!
 * @internal
 * @brief Enum to indicate whether a pointer is aligned.
 */
typedef enum {
    XXH_aligned,  /*!< Aligned */
    XXH_unaligned /*!< Possibly unaligned */
} XXH_alignment;

/*
 * XXH_FORCE_MEMORY_ACCESS==3 is an endian-independent byteshift load.
 *
 * This is ideal for older compilers which don't inline memcpy.
 */
#if (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==3))

XXH_FORCE_INLINE xxh_u32 XXH_readLE32(const void* memPtr)
{
    const xxh_u8* bytePtr = (const xxh_u8 *)memPtr;
    return bytePtr[0]
         | ((xxh_u32)bytePtr[1] << 8)
         | ((xxh_u32)bytePtr[2] << 16)
         | ((xxh_u32)bytePtr[3] << 24);
}

XXH_FORCE_INLINE xxh_u32 XXH_readBE32(const void* memPtr)
{
    const xxh_u8* bytePtr = (const xxh_u8 *)memPtr;
    return bytePtr[3]
         | ((xxh_u32)bytePtr[2] << 8)
         | ((xxh_u32)bytePtr[1] << 16)
         | ((xxh_u32)bytePtr[0] << 24);
}

#else
XXH_FORCE_INLINE xxh_u32 XXH_readLE32(const void* ptr)
{
    return XXH_CPU_LITTLE_ENDIAN ? XXH_read32(ptr) : XXH_swap32(XXH_read32(ptr));
}

static xxh_u32 XXH_readBE32(const void* ptr)
{
    return XXH_CPU_LITTLE_ENDIAN ? XXH_swap32(XXH_read32(ptr)) : XXH_read32(ptr);
}
#endif

XXH_FORCE_INLINE xxh_u32
XXH_readLE32_align(const void* ptr, XXH_alignment align)
{
    if (align==XXH_unaligned) {
        return XXH_readLE32(ptr);
    } else {
        return XXH_CPU_LITTLE_ENDIAN ? *(const xxh_u32*)ptr : XXH_swap32(*(const xxh_u32*)ptr);
    }
}


/* *************************************
*  Misc
***************************************/
/*! @ingroup public */
XXH_PUBLIC_API unsigned XXH_versionNumber (void) { return XXH_VERSION_NUMBER; }


/* *******************************************************************
*  32-bit hash functions
*********************************************************************/
/*!
 * @}
 * @defgroup XXH32_impl XXH32 implementation
 * @ingroup impl
 *
 * Details on the XXH32 implementation.
 * @{
 */
 /* #define instead of static const, to be used as initializers */
#define XXH_PRIME32_1  0x9E3779B1U  /*!< 0b10011110001101110111100110110001 */
#define XXH_PRIME32_2  0x85EBCA77U  /*!< 0b10000101111010111100101001110111 */
#define XXH_PRIME32_3  0xC2B2AE3DU  /*!< 0b11000010101100101010111000111101 */
#define XXH_PRIME32_4  0x27D4EB2FU  /*!< 0b00100111110101001110101100101111 */
#define XXH_PRIME32_5  0x165667B1U  /*!< 0b00010110010101100110011110110001 */

#ifdef XXH_OLD_NAMES
#  define PRIME32_1 XXH_PRIME32_1
#  define PRIME32_2 XXH_PRIME32_2
#  define PRIME32_3 XXH_PRIME32_3
#  define PRIME32_4 XXH_PRIME32_4
#  define PRIME32_5 XXH_PRIME32_5
#endif

/*!
 * @internal
 * @brief Normal stripe processing routine.
 *
 * This shuffles the bits so that any bit from @p input impacts several bits in
 * @p acc.
 *
 * @param acc The accumulator lane.
 * @param input The stripe of input to mix.
 * @return The mixed accumulator lane.
 */
static xxh_u32 XXH32_round(xxh_u32 acc, xxh_u32 input)
{
    acc += input * XXH_PRIME32_2;
    acc  = XXH_rotl32(acc, 13);
    acc *= XXH_PRIME32_1;
#if (defined(__SSE4_1__) || defined(__aarch64__) || defined(__wasm_simd128__)) && !defined(XXH_ENABLE_AUTOVECTORIZE)
    /*
     * UGLY HACK:
     * A compiler fence is used to prevent GCC and Clang from
     * autovectorizing the XXH32 loop (pragmas and attributes don't work for some
     * reason) without globally disabling SSE4.1.
     *
     * The reason we want to avoid vectorization is because despite working on
     * 4 integers at a time, there are multiple factors slowing XXH32 down on
     * SSE4:
     * - There's a ridiculous amount of lag from pmulld (10 cycles of latency on
     *   newer chips!) making it slightly slower to multiply four integers at
     *   once compared to four integers independently. Even when pmulld was
     *   fastest, Sandy/Ivy Bridge, it is still not worth it to go into SSE
     *   just to multiply unless doing a long operation.
     *
     * - Four instructions are required to rotate,
     *      movqda tmp,  v // not required with VEX encoding
     *      pslld  tmp, 13 // tmp <<= 13
     *      psrld  v,   19 // x >>= 19
     *      por    v,  tmp // x |= tmp
     *   compared to one for scalar:
     *      roll   v, 13    // reliably fast across the board
     *      shldl  v, v, 13 // Sandy Bridge and later prefer this for some reason
     *
     * - Instruction level parallelism is actually more beneficial here because
     *   the SIMD actually serializes this operation: While v1 is rotating, v2
     *   can load data, while v3 can multiply. SSE forces them to operate
     *   together.
     *
     * This is also enabled on AArch64, as Clang is *very aggressive* in vectorizing
     * the loop. NEON is only faster on the A53, and with the newer cores, it is less
     * than half the speed.
     *
     * Additionally, this is used on WASM SIMD128 because it JITs to the same
     * SIMD instructions and has the same issue.
     */
    XXH_COMPILER_GUARD(acc);
#endif
    return acc;
}

/*!
 * @internal
 * @brief Mixes all bits to finalize the hash.
 *
 * The final mix ensures that all input bits have a chance to impact any bit in
 * the output digest, resulting in an unbiased distribution.
 *
 * @param hash The hash to avalanche.
 * @return The avalanched hash.
 */
static xxh_u32 XXH32_avalanche(xxh_u32 hash)
{
    hash ^= hash >> 15;
    hash *= XXH_PRIME32_2;
    hash ^= hash >> 13;
    hash *= XXH_PRIME32_3;
    hash ^= hash >> 16;
    return hash;
}

#define XXH_get32bits(p) XXH_readLE32_align(p, align)

/*!
 * @internal
 * @brief Sets up the initial accumulator state for XXH32().
 */
XXH_FORCE_INLINE void
XXH32_initAccs(xxh_u32 *acc, xxh_u32 seed)
{
    XXH_ASSERT(acc != NULL);
    acc[0] = seed + XXH_PRIME32_1 + XXH_PRIME32_2;
    acc[1] = seed + XXH_PRIME32_2;
    acc[2] = seed + 0;
    acc[3] = seed - XXH_PRIME32_1;
}

/*!
 * @internal
 * @brief Consumes a block of data for XXH32().
 *
 * @return the end input pointer.
 */
XXH_FORCE_INLINE const xxh_u8 *
XXH32_consumeLong(
    xxh_u32 *XXH_RESTRICT acc,
    xxh_u8 const *XXH_RESTRICT input,
    size_t len,
    XXH_alignment align
)
{
    const xxh_u8* const bEnd = input + len;
    const xxh_u8* const limit = bEnd - 15;
    XXH_ASSERT(acc != NULL);
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(len >= 16);
    do {
        acc[0] = XXH32_round(acc[0], XXH_get32bits(input)); input += 4;
        acc[1] = XXH32_round(acc[1], XXH_get32bits(input)); input += 4;
        acc[2] = XXH32_round(acc[2], XXH_get32bits(input)); input += 4;
        acc[3] = XXH32_round(acc[3], XXH_get32bits(input)); input += 4;
    } while (input < limit);

    return input;
}

/*!
 * @internal
 * @brief Merges the accumulator lanes together for XXH32()
 */
XXH_FORCE_INLINE XXH_PUREF xxh_u32
XXH32_mergeAccs(const xxh_u32 *acc)
{
    XXH_ASSERT(acc != NULL);
    return XXH_rotl32(acc[0], 1)  + XXH_rotl32(acc[1], 7)
         + XXH_rotl32(acc[2], 12) + XXH_rotl32(acc[3], 18);
}

/*!
 * @internal
 * @brief Processes the last 0-15 bytes of @p ptr.
 *
 * There may be up to 15 bytes remaining to consume from the input.
 * This final stage will digest them to ensure that all input bytes are present
 * in the final mix.
 *
 * @param hash The hash to finalize.
 * @param ptr The pointer to the remaining input.
 * @param len The remaining length, modulo 16.
 * @param align Whether @p ptr is aligned.
 * @return The finalized hash.
 * @see XXH64_finalize().
 */
static XXH_PUREF xxh_u32
XXH32_finalize(xxh_u32 hash, const xxh_u8* ptr, size_t len, XXH_alignment align)
{
#define XXH_PROCESS1 do {                             \
    hash += (*ptr++) * XXH_PRIME32_5;                 \
    hash = XXH_rotl32(hash, 11) * XXH_PRIME32_1;      \
} while (0)

#define XXH_PROCESS4 do {                             \
    hash += XXH_get32bits(ptr) * XXH_PRIME32_3;       \
    ptr += 4;                                         \
    hash  = XXH_rotl32(hash, 17) * XXH_PRIME32_4;     \
} while (0)

    if (ptr==NULL) XXH_ASSERT(len == 0);

    /* Compact rerolled version; generally faster */
    if (!XXH32_ENDJMP) {
        len &= 15;
        while (len >= 4) {
            XXH_PROCESS4;
            len -= 4;
        }
        while (len > 0) {
            XXH_PROCESS1;
            --len;
        }
        return XXH32_avalanche(hash);
    } else {
         switch(len&15) /* or switch(bEnd - p) */ {
           case 12:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 8:       XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 4:       XXH_PROCESS4;
                         return XXH32_avalanche(hash);

           case 13:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 9:       XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 5:       XXH_PROCESS4;
                         XXH_PROCESS1;
                         return XXH32_avalanche(hash);

           case 14:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 10:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 6:       XXH_PROCESS4;
                         XXH_PROCESS1;
                         XXH_PROCESS1;
                         return XXH32_avalanche(hash);

           case 15:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 11:      XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 7:       XXH_PROCESS4;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 3:       XXH_PROCESS1;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 2:       XXH_PROCESS1;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 1:       XXH_PROCESS1;
                         XXH_FALLTHROUGH;  /* fallthrough */
           case 0:       return XXH32_avalanche(hash);
        }
        XXH_ASSERT(0);
        return hash;   /* reaching this point is deemed impossible */
    }
}

#ifdef XXH_OLD_NAMES
#  define PROCESS1 XXH_PROCESS1
#  define PROCESS4 XXH_PROCESS4
#else
#  undef XXH_PROCESS1
#  undef XXH_PROCESS4
#endif

/*!
 * @internal
 * @brief The implementation for @ref XXH32().
 *
 * @param input , len , seed Directly passed from @ref XXH32().
 * @param align Whether @p input is aligned.
 * @return The calculated hash.
 */
XXH_FORCE_INLINE XXH_PUREF xxh_u32
XXH32_endian_align(const xxh_u8* input, size_t len, xxh_u32 seed, XXH_alignment align)
{
    xxh_u32 h32;

    if (input==NULL) XXH_ASSERT(len == 0);

    if (len>=16) {
        xxh_u32 acc[4];
        XXH32_initAccs(acc, seed);

        input = XXH32_consumeLong(acc, input, len, align);

        h32 = XXH32_mergeAccs(acc);
    } else {
        h32  = seed + XXH_PRIME32_5;
    }

    h32 += (xxh_u32)len;

    return XXH32_finalize(h32, input, len&15, align);
}

/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_hash_t XXH32 (const void* input, size_t len, XXH32_hash_t seed)
{
#if !defined(XXH_NO_STREAM) && XXH_SIZE_OPT >= 2
    /* Simple version, good for code maintenance, but unfortunately slow for small inputs */
    XXH32_state_t state;
    XXH32_reset(&state, seed);
    XXH32_update(&state, (const xxh_u8*)input, len);
    return XXH32_digest(&state);
#else
    if (XXH_FORCE_ALIGN_CHECK) {
        if ((((size_t)input) & 3) == 0) {   /* Input is 4-bytes aligned, leverage the speed benefit */
            return XXH32_endian_align((const xxh_u8*)input, len, seed, XXH_aligned);
    }   }

    return XXH32_endian_align((const xxh_u8*)input, len, seed, XXH_unaligned);
#endif
}



/*******   Hash streaming   *******/
#ifndef XXH_NO_STREAM
/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_state_t* XXH32_createState(void)
{
    return (XXH32_state_t*)XXH_malloc(sizeof(XXH32_state_t));
}
/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH_errorcode XXH32_freeState(XXH32_state_t* statePtr)
{
    XXH_free(statePtr);
    return XXH_OK;
}

/*! @ingroup XXH32_family */
XXH_PUBLIC_API void XXH32_copyState(XXH32_state_t* dstState, const XXH32_state_t* srcState)
{
    XXH_memcpy(dstState, srcState, sizeof(*dstState));
}

/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH_errorcode XXH32_reset(XXH32_state_t* statePtr, XXH32_hash_t seed)
{
    XXH_ASSERT(statePtr != NULL);
    memset(statePtr, 0, sizeof(*statePtr));
    XXH32_initAccs(statePtr->acc, seed);
    return XXH_OK;
}


/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH_errorcode
XXH32_update(XXH32_state_t* state, const void* input, size_t len)
{
    if (input==NULL) {
        XXH_ASSERT(len == 0);
        return XXH_OK;
    }

    state->total_len_32 += (XXH32_hash_t)len;
    state->large_len |= (XXH32_hash_t)((len>=16) | (state->total_len_32>=16));

    XXH_ASSERT(state->bufferedSize < sizeof(state->buffer));
    if (len < sizeof(state->buffer) - state->bufferedSize)  {   /* fill in tmp buffer */
        XXH_memcpy(state->buffer + state->bufferedSize, input, len);
        state->bufferedSize += (XXH32_hash_t)len;
        return XXH_OK;
    }

    {   const xxh_u8* xinput = (const xxh_u8*)input;
        const xxh_u8* const bEnd = xinput + len;

        if (state->bufferedSize) {   /* non-empty buffer: complete first */
            XXH_memcpy(state->buffer + state->bufferedSize, xinput, sizeof(state->buffer) - state->bufferedSize);
            xinput += sizeof(state->buffer) - state->bufferedSize;
            /* then process one round */
            (void)XXH32_consumeLong(state->acc, state->buffer, sizeof(state->buffer), XXH_aligned);
            state->bufferedSize = 0;
        }

        XXH_ASSERT(xinput <= bEnd);
        if ((size_t)(bEnd - xinput) >= sizeof(state->buffer)) {
            /* Process the remaining data */
            xinput = XXH32_consumeLong(state->acc, xinput, (size_t)(bEnd - xinput), XXH_unaligned);
        }

        if (xinput < bEnd) {
            /* Copy the leftover to the tmp buffer */
            XXH_memcpy(state->buffer, xinput, (size_t)(bEnd-xinput));
            state->bufferedSize = (unsigned)(bEnd-xinput);
        }
    }

    return XXH_OK;
}


/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_hash_t XXH32_digest(const XXH32_state_t* state)
{
    xxh_u32 h32;

    if (state->large_len) {
        h32 = XXH32_mergeAccs(state->acc);
    } else {
        h32 = state->acc[2] /* == seed */ + XXH_PRIME32_5;
    }

    h32 += state->total_len_32;

    return XXH32_finalize(h32, state->buffer, state->bufferedSize, XXH_aligned);
}
#endif /* !XXH_NO_STREAM */

/*******   Canonical representation   *******/

/*! @ingroup XXH32_family */
XXH_PUBLIC_API void XXH32_canonicalFromHash(XXH32_canonical_t* dst, XXH32_hash_t hash)
{
    XXH_STATIC_ASSERT(sizeof(XXH32_canonical_t) == sizeof(XXH32_hash_t));
    if (XXH_CPU_LITTLE_ENDIAN) hash = XXH_swap32(hash);
    XXH_memcpy(dst, &hash, sizeof(*dst));
}
/*! @ingroup XXH32_family */
XXH_PUBLIC_API XXH32_hash_t XXH32_hashFromCanonical(const XXH32_canonical_t* src)
{
    return XXH_readBE32(src);
}


#ifndef XXH_NO_LONG_LONG

/* *******************************************************************
*  64-bit hash functions
*********************************************************************/
/*!
 * @}
 * @ingroup impl
 * @{
 */
/*******   Memory access   *******/

typedef XXH64_hash_t xxh_u64;

#ifdef XXH_OLD_NAMES
#  define U64 xxh_u64
#endif

#if (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==3))
/*
 * Manual byteshift. Best for old compilers which don't inline memcpy.
 * We actually directly use XXH_readLE64 and XXH_readBE64.
 */
#elif (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==2))

/* Force direct memory access. Only works on CPU which support unaligned memory access in hardware */
static xxh_u64 XXH_read64(const void* memPtr)
{
    return *(const xxh_u64*) memPtr;
}

#elif (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==1))

/*
 * __attribute__((aligned(1))) is supported by gcc and clang. Originally the
 * documentation claimed that it only increased the alignment, but actually it
 * can decrease it on gcc, clang, and icc:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69502,
 * https://gcc.godbolt.org/z/xYez1j67Y.
 */
#ifdef XXH_OLD_NAMES
typedef union { xxh_u32 u32; xxh_u64 u64; } __attribute__((__packed__)) unalign64;
#endif
static xxh_u64 XXH_read64(const void* ptr)
{
    typedef __attribute__((__aligned__(1))) xxh_u64 xxh_unalign64;
    return *((const xxh_unalign64*)ptr);
}

#else

/*
 * Portable and safe solution. Generally efficient.
 * see: https://fastcompression.blogspot.com/2015/08/accessing-unaligned-memory.html
 */
static xxh_u64 XXH_read64(const void* memPtr)
{
    xxh_u64 val;
    XXH_memcpy(&val, memPtr, sizeof(val));
    return val;
}

#endif   /* XXH_FORCE_DIRECT_MEMORY_ACCESS */

#if defined(_MSC_VER)     /* Visual Studio */
#  define XXH_swap64 _byteswap_uint64
#elif XXH_GCC_VERSION >= 403
#  define XXH_swap64 __builtin_bswap64
#else
static xxh_u64 XXH_swap64(xxh_u64 x)
{
    return  ((x << 56) & 0xff00000000000000ULL) |
            ((x << 40) & 0x00ff000000000000ULL) |
            ((x << 24) & 0x0000ff0000000000ULL) |
            ((x << 8)  & 0x000000ff00000000ULL) |
            ((x >> 8)  & 0x00000000ff000000ULL) |
            ((x >> 24) & 0x0000000000ff0000ULL) |
            ((x >> 40) & 0x000000000000ff00ULL) |
            ((x >> 56) & 0x00000000000000ffULL);
}
#endif


/* XXH_FORCE_MEMORY_ACCESS==3 is an endian-independent byteshift load. */
#if (defined(XXH_FORCE_MEMORY_ACCESS) && (XXH_FORCE_MEMORY_ACCESS==3))

XXH_FORCE_INLINE xxh_u64 XXH_readLE64(const void* memPtr)
{
    const xxh_u8* bytePtr = (const xxh_u8 *)memPtr;
    return bytePtr[0]
         | ((xxh_u64)bytePtr[1] << 8)
         | ((xxh_u64)bytePtr[2] << 16)
         | ((xxh_u64)bytePtr[3] << 24)
         | ((xxh_u64)bytePtr[4] << 32)
         | ((xxh_u64)bytePtr[5] << 40)
         | ((xxh_u64)bytePtr[6] << 48)
         | ((xxh_u64)bytePtr[7] << 56);
}

XXH_FORCE_INLINE xxh_u64 XXH_readBE64(const void* memPtr)
{
    const xxh_u8* bytePtr = (const xxh_u8 *)memPtr;
    return bytePtr[7]
         | ((xxh_u64)bytePtr[6] << 8)
         | ((xxh_u64)bytePtr[5] << 16)
         | ((xxh_u64)bytePtr[4] << 24)
         | ((xxh_u64)bytePtr[3] << 32)
         | ((xxh_u64)bytePtr[2] << 40)
         | ((xxh_u64)bytePtr[1] << 48)
         | ((xxh_u64)bytePtr[0] << 56);
}

#else
XXH_FORCE_INLINE xxh_u64 XXH_readLE64(const void* ptr)
{
    return XXH_CPU_LITTLE_ENDIAN ? XXH_read64(ptr) : XXH_swap64(XXH_read64(ptr));
}

static xxh_u64 XXH_readBE64(const void* ptr)
{
    return XXH_CPU_LITTLE_ENDIAN ? XXH_swap64(XXH_read64(ptr)) : XXH_read64(ptr);
}
#endif

XXH_FORCE_INLINE xxh_u64
XXH_readLE64_align(const void* ptr, XXH_alignment align)
{
    if (align==XXH_unaligned)
        return XXH_readLE64(ptr);
    else
        return XXH_CPU_LITTLE_ENDIAN ? *(const xxh_u64*)ptr : XXH_swap64(*(const xxh_u64*)ptr);
}


/*******   xxh64   *******/
/*!
 * @}
 * @defgroup XXH64_impl XXH64 implementation
 * @ingroup impl
 *
 * Details on the XXH64 implementation.
 * @{
 */
/* #define rather that static const, to be used as initializers */
#define XXH_PRIME64_1  0x9E3779B185EBCA87ULL  /*!< 0b1001111000110111011110011011000110000101111010111100101010000111 */
#define XXH_PRIME64_2  0xC2B2AE3D27D4EB4FULL  /*!< 0b1100001010110010101011100011110100100111110101001110101101001111 */
#define XXH_PRIME64_3  0x165667B19E3779F9ULL  /*!< 0b0001011001010110011001111011000110011110001101110111100111111001 */
#define XXH_PRIME64_4  0x85EBCA77C2B2AE63ULL  /*!< 0b1000010111101011110010100111011111000010101100101010111001100011 */
#define XXH_PRIME64_5  0x27D4EB2F165667C5ULL  /*!< 0b0010011111010100111010110010111100010110010101100110011111000101 */

#ifdef XXH_OLD_NAMES
#  define PRIME64_1 XXH_PRIME64_1
#  define PRIME64_2 XXH_PRIME64_2
#  define PRIME64_3 XXH_PRIME64_3
#  define PRIME64_4 XXH_PRIME64_4
#  define PRIME64_5 XXH_PRIME64_5
#endif

/*! @copydoc XXH32_round */
static xxh_u64 XXH64_round(xxh_u64 acc, xxh_u64 input)
{
    acc += input * XXH_PRIME64_2;
    acc  = XXH_rotl64(acc, 31);
    acc *= XXH_PRIME64_1;
#if (defined(__AVX512F__)) && !defined(XXH_ENABLE_AUTOVECTORIZE)
    /*
     * DISABLE AUTOVECTORIZATION:
     * A compiler fence is used to prevent GCC and Clang from
     * autovectorizing the XXH64 loop (pragmas and attributes don't work for some
     * reason) without globally disabling AVX512.
     *
     * Autovectorization of XXH64 tends to be detrimental,
     * though the exact outcome may change depending on exact cpu and compiler version.
     * For information, it has been reported as detrimental for Skylake-X,
     * but possibly beneficial for Zen4.
     *
     * The default is to disable auto-vectorization,
     * but you can select to enable it instead using `XXH_ENABLE_AUTOVECTORIZE` build variable.
     */
    XXH_COMPILER_GUARD(acc);
#endif
    return acc;
}

static xxh_u64 XXH64_mergeRound(xxh_u64 acc, xxh_u64 val)
{
    val  = XXH64_round(0, val);
    acc ^= val;
    acc  = acc * XXH_PRIME64_1 + XXH_PRIME64_4;
    return acc;
}

/*! @copydoc XXH32_avalanche */
static xxh_u64 XXH64_avalanche(xxh_u64 hash)
{
    hash ^= hash >> 33;
    hash *= XXH_PRIME64_2;
    hash ^= hash >> 29;
    hash *= XXH_PRIME64_3;
    hash ^= hash >> 32;
    return hash;
}


#define XXH_get64bits(p) XXH_readLE64_align(p, align)

/*!
 * @internal
 * @brief Sets up the initial accumulator state for XXH64().
 */
XXH_FORCE_INLINE void
XXH64_initAccs(xxh_u64 *acc, xxh_u64 seed)
{
    XXH_ASSERT(acc != NULL);
    acc[0] = seed + XXH_PRIME64_1 + XXH_PRIME64_2;
    acc[1] = seed + XXH_PRIME64_2;
    acc[2] = seed + 0;
    acc[3] = seed - XXH_PRIME64_1;
}

/*!
 * @internal
 * @brief Consumes a block of data for XXH64().
 *
 * @return the end input pointer.
 */
XXH_FORCE_INLINE const xxh_u8 *
XXH64_consumeLong(
    xxh_u64 *XXH_RESTRICT acc,
    xxh_u8 const *XXH_RESTRICT input,
    size_t len,
    XXH_alignment align
)
{
    const xxh_u8* const bEnd = input + len;
    const xxh_u8* const limit = bEnd - 31;
    XXH_ASSERT(acc != NULL);
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(len >= 32);
    do {
        /* reroll on 32-bit */
        if (sizeof(void *) < sizeof(xxh_u64)) {
            size_t i;
            for (i = 0; i < 4; i++) {
                acc[i] = XXH64_round(acc[i], XXH_get64bits(input));
                input += 8;
            }
        } else {
            acc[0] = XXH64_round(acc[0], XXH_get64bits(input)); input += 8;
            acc[1] = XXH64_round(acc[1], XXH_get64bits(input)); input += 8;
            acc[2] = XXH64_round(acc[2], XXH_get64bits(input)); input += 8;
            acc[3] = XXH64_round(acc[3], XXH_get64bits(input)); input += 8;
        }
    } while (input < limit);

    return input;
}

/*!
 * @internal
 * @brief Merges the accumulator lanes together for XXH64()
 */
XXH_FORCE_INLINE XXH_PUREF xxh_u64
XXH64_mergeAccs(const xxh_u64 *acc)
{
    XXH_ASSERT(acc != NULL);
    {
        xxh_u64 h64 = XXH_rotl64(acc[0], 1) + XXH_rotl64(acc[1], 7)
                    + XXH_rotl64(acc[2], 12) + XXH_rotl64(acc[3], 18);
        /* reroll on 32-bit */
        if (sizeof(void *) < sizeof(xxh_u64)) {
            size_t i;
            for (i = 0; i < 4; i++) {
                h64 = XXH64_mergeRound(h64, acc[i]);
            }
        } else {
            h64 = XXH64_mergeRound(h64, acc[0]);
            h64 = XXH64_mergeRound(h64, acc[1]);
            h64 = XXH64_mergeRound(h64, acc[2]);
            h64 = XXH64_mergeRound(h64, acc[3]);
        }
        return h64;
    }
}

/*!
 * @internal
 * @brief Processes the last 0-31 bytes of @p ptr.
 *
 * There may be up to 31 bytes remaining to consume from the input.
 * This final stage will digest them to ensure that all input bytes are present
 * in the final mix.
 *
 * @param hash The hash to finalize.
 * @param ptr The pointer to the remaining input.
 * @param len The remaining length, modulo 32.
 * @param align Whether @p ptr is aligned.
 * @return The finalized hash
 * @see XXH32_finalize().
 */
XXH_STATIC XXH_PUREF xxh_u64
XXH64_finalize(xxh_u64 hash, const xxh_u8* ptr, size_t len, XXH_alignment align)
{
    if (ptr==NULL) XXH_ASSERT(len == 0);
    len &= 31;
    while (len >= 8) {
        xxh_u64 const k1 = XXH64_round(0, XXH_get64bits(ptr));
        ptr += 8;
        hash ^= k1;
        hash  = XXH_rotl64(hash,27) * XXH_PRIME64_1 + XXH_PRIME64_4;
        len -= 8;
    }
    if (len >= 4) {
        hash ^= (xxh_u64)(XXH_get32bits(ptr)) * XXH_PRIME64_1;
        ptr += 4;
        hash = XXH_rotl64(hash, 23) * XXH_PRIME64_2 + XXH_PRIME64_3;
        len -= 4;
    }
    while (len > 0) {
        hash ^= (*ptr++) * XXH_PRIME64_5;
        hash = XXH_rotl64(hash, 11) * XXH_PRIME64_1;
        --len;
    }
    return  XXH64_avalanche(hash);
}

#ifdef XXH_OLD_NAMES
#  define PROCESS1_64 XXH_PROCESS1_64
#  define PROCESS4_64 XXH_PROCESS4_64
#  define PROCESS8_64 XXH_PROCESS8_64
#else
#  undef XXH_PROCESS1_64
#  undef XXH_PROCESS4_64
#  undef XXH_PROCESS8_64
#endif

/*!
 * @internal
 * @brief The implementation for @ref XXH64().
 *
 * @param input , len , seed Directly passed from @ref XXH64().
 * @param align Whether @p input is aligned.
 * @return The calculated hash.
 */
XXH_FORCE_INLINE XXH_PUREF xxh_u64
XXH64_endian_align(const xxh_u8* input, size_t len, xxh_u64 seed, XXH_alignment align)
{
    xxh_u64 h64;
    if (input==NULL) XXH_ASSERT(len == 0);

    if (len>=32) {  /* Process a large block of data */
        xxh_u64 acc[4];
        XXH64_initAccs(acc, seed);

        input = XXH64_consumeLong(acc, input, len, align);

        h64 = XXH64_mergeAccs(acc);
    } else {
        h64  = seed + XXH_PRIME64_5;
    }

    h64 += (xxh_u64) len;

    return XXH64_finalize(h64, input, len, align);
}


/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH64_hash_t XXH64 (XXH_NOESCAPE const void* input, size_t len, XXH64_hash_t seed)
{
#if !defined(XXH_NO_STREAM) && XXH_SIZE_OPT >= 2
    /* Simple version, good for code maintenance, but unfortunately slow for small inputs */
    XXH64_state_t state;
    XXH64_reset(&state, seed);
    XXH64_update(&state, (const xxh_u8*)input, len);
    return XXH64_digest(&state);
#else
    if (XXH_FORCE_ALIGN_CHECK) {
        if ((((size_t)input) & 7)==0) {  /* Input is aligned, let's leverage the speed advantage */
            return XXH64_endian_align((const xxh_u8*)input, len, seed, XXH_aligned);
    }   }

    return XXH64_endian_align((const xxh_u8*)input, len, seed, XXH_unaligned);

#endif
}

/*******   Hash Streaming   *******/
#ifndef XXH_NO_STREAM
/*! @ingroup XXH64_family*/
XXH_PUBLIC_API XXH64_state_t* XXH64_createState(void)
{
    return (XXH64_state_t*)XXH_malloc(sizeof(XXH64_state_t));
}
/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH_errorcode XXH64_freeState(XXH64_state_t* statePtr)
{
    XXH_free(statePtr);
    return XXH_OK;
}

/*! @ingroup XXH64_family */
XXH_PUBLIC_API void XXH64_copyState(XXH_NOESCAPE XXH64_state_t* dstState, const XXH64_state_t* srcState)
{
    XXH_memcpy(dstState, srcState, sizeof(*dstState));
}

/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH_errorcode XXH64_reset(XXH_NOESCAPE XXH64_state_t* statePtr, XXH64_hash_t seed)
{
    XXH_ASSERT(statePtr != NULL);
    memset(statePtr, 0, sizeof(*statePtr));
    XXH64_initAccs(statePtr->acc, seed);
    return XXH_OK;
}

/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH_errorcode
XXH64_update (XXH_NOESCAPE XXH64_state_t* state, XXH_NOESCAPE const void* input, size_t len)
{
    if (input==NULL) {
        XXH_ASSERT(len == 0);
        return XXH_OK;
    }

    state->total_len += len;

    XXH_ASSERT(state->bufferedSize <= sizeof(state->buffer));
    if (len < sizeof(state->buffer) - state->bufferedSize)  {   /* fill in tmp buffer */
        XXH_memcpy(state->buffer + state->bufferedSize, input, len);
        state->bufferedSize += (XXH32_hash_t)len;
        return XXH_OK;
    }

    {   const xxh_u8* xinput = (const xxh_u8*)input;
        const xxh_u8* const bEnd = xinput + len;

        if (state->bufferedSize) {   /* non-empty buffer => complete first */
            XXH_memcpy(state->buffer + state->bufferedSize, xinput, sizeof(state->buffer) - state->bufferedSize);
            xinput += sizeof(state->buffer) - state->bufferedSize;
            /* and process one round */
            (void)XXH64_consumeLong(state->acc, state->buffer, sizeof(state->buffer), XXH_aligned);
            state->bufferedSize = 0;
        }

        XXH_ASSERT(xinput <= bEnd);
        if ((size_t)(bEnd - xinput) >= sizeof(state->buffer)) {
            /* Process the remaining data */
            xinput = XXH64_consumeLong(state->acc, xinput, (size_t)(bEnd - xinput), XXH_unaligned);
        }

        if (xinput < bEnd) {
            /* Copy the leftover to the tmp buffer */
            XXH_memcpy(state->buffer, xinput, (size_t)(bEnd-xinput));
            state->bufferedSize = (unsigned)(bEnd-xinput);
        }
    }

    return XXH_OK;
}


/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH64_hash_t XXH64_digest(XXH_NOESCAPE const XXH64_state_t* state)
{
    xxh_u64 h64;

    if (state->total_len >= 32) {
        h64 = XXH64_mergeAccs(state->acc);
    } else {
        h64  = state->acc[2] /*seed*/ + XXH_PRIME64_5;
    }

    h64 += (xxh_u64) state->total_len;

    return XXH64_finalize(h64, state->buffer, (size_t)state->total_len, XXH_aligned);
}
#endif /* !XXH_NO_STREAM */

/******* Canonical representation   *******/

/*! @ingroup XXH64_family */
XXH_PUBLIC_API void XXH64_canonicalFromHash(XXH_NOESCAPE XXH64_canonical_t* dst, XXH64_hash_t hash)
{
    XXH_STATIC_ASSERT(sizeof(XXH64_canonical_t) == sizeof(XXH64_hash_t));
    if (XXH_CPU_LITTLE_ENDIAN) hash = XXH_swap64(hash);
    XXH_memcpy(dst, &hash, sizeof(*dst));
}

/*! @ingroup XXH64_family */
XXH_PUBLIC_API XXH64_hash_t XXH64_hashFromCanonical(XXH_NOESCAPE const XXH64_canonical_t* src)
{
    return XXH_readBE64(src);
}

#ifndef XXH_NO_XXH3

/* *********************************************************************
*  XXH3
*  New generation hash designed for speed on small keys and vectorization
************************************************************************ */
/*!
 * @}
 * @defgroup XXH3_impl XXH3 implementation
 * @ingroup impl
 * @{
 */

/* ===   Compiler specifics   === */


#if (defined(__GNUC__) && (__GNUC__ >= 3))  \
  || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 800)) \
  || defined(__clang__)
#    define XXH_likely(x) __builtin_expect(x, 1)
#    define XXH_unlikely(x) __builtin_expect(x, 0)
#else
#    define XXH_likely(x) (x)
#    define XXH_unlikely(x) (x)
#endif

#ifndef XXH_HAS_INCLUDE
#  ifdef __has_include
/*
 * Not defined as XXH_HAS_INCLUDE(x) (function-like) because
 * this causes segfaults in Apple Clang 4.2 (on Mac OS X 10.7 Lion)
 */
#    define XXH_HAS_INCLUDE __has_include
#  else
#    define XXH_HAS_INCLUDE(x) 0
#  endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#  if defined(__ARM_FEATURE_SVE)
#    include <arm_sve.h>
#  endif
#  if defined(__ARM_NEON__) || defined(__ARM_NEON) \
   || (defined(_M_ARM) && _M_ARM >= 7) \
   || defined(_M_ARM64) || defined(_M_ARM64EC) \
   || (defined(__wasm_simd128__) && XXH_HAS_INCLUDE(<arm_neon.h>)) /* WASM SIMD128 via SIMDe */
#    define inline __inline__  /* circumvent a clang bug */
#    include <arm_neon.h>
#    undef inline
#  elif defined(__AVX2__)
#    include <immintrin.h>
#  elif defined(__SSE2__)
#    include <emmintrin.h>
#  elif defined(__loongarch_sx)
#    include <lsxintrin.h>
#  endif
#endif

#if defined(_MSC_VER)
#  include <intrin.h>
#endif

/*
 * One goal of XXH3 is to make it fast on both 32-bit and 64-bit, while
 * remaining a true 64-bit/128-bit hash function.
 *
 * This is done by prioritizing a subset of 64-bit operations that can be
 * emulated without too many steps on the average 32-bit machine.
 *
 * For example, these two lines seem similar, and run equally fast on 64-bit:
 *
 *   xxh_u64 x;
 *   x ^= (x >> 47); // good
 *   x ^= (x >> 13); // bad
 *
 * However, to a 32-bit machine, there is a major difference.
 *
 * x ^= (x >> 47) looks like this:
 *
 *   x.lo ^= (x.hi >> (47 - 32));
 *
 * while x ^= (x >> 13) looks like this:
 *
 *   // note: funnel shifts are not usually cheap.
 *   x.lo ^= (x.lo >> 13) | (x.hi << (32 - 13));
 *   x.hi ^= (x.hi >> 13);
 *
 * The first one is significantly faster than the second, simply because the
 * shift is larger than 32. This means:
 *  - All the bits we need are in the upper 32 bits, so we can ignore the lower
 *    32 bits in the shift.
 *  - The shift result will always fit in the lower 32 bits, and therefore,
 *    we can ignore the upper 32 bits in the xor.
 *
 * Thanks to this optimization, XXH3 only requires these features to be efficient:
 *
 *  - Usable unaligned access
 *  - A 32-bit or 64-bit ALU
 *      - If 32-bit, a decent ADC instruction
 *  - A 32 or 64-bit multiply with a 64-bit result
 *  - For the 128-bit variant, a decent byteswap helps short inputs.
 *
 * The first two are already required by XXH32, and almost all 32-bit and 64-bit
 * platforms which can run XXH32 can run XXH3 efficiently.
 *
 * Thumb-1, the classic 16-bit only subset of ARM's instruction set, is one
 * notable exception.
 *
 * First of all, Thumb-1 lacks support for the UMULL instruction which
 * performs the important long multiply. This means numerous __aeabi_lmul
 * calls.
 *
 * Second of all, the 8 functional registers are just not enough.
 * Setup for __aeabi_lmul, byteshift loads, pointers, and all arithmetic need
 * Lo registers, and this shuffling results in thousands more MOVs than A32.
 *
 * A32 and T32 don't have this limitation. They can access all 14 registers,
 * do a 32->64 multiply with UMULL, and the flexible operand allowing free
 * shifts is helpful, too.
 *
 * Therefore, we do a quick sanity check.
 *
 * If compiling Thumb-1 for a target which supports ARM instructions, we will
 * emit a warning, as it is not a "sane" platform to compile for.
 *
 * Usually, if this happens, it is because of an accident and you probably need
 * to specify -march, as you likely meant to compile for a newer architecture.
 *
 * Credit: large sections of the vectorial and asm source code paths
 *         have been contributed by @easyaspi314
 */
#if defined(__thumb__) && !defined(__thumb2__) && defined(__ARM_ARCH_ISA_ARM)
#   warning "XXH3 is highly inefficient without ARM or Thumb-2."
#endif

/* ==========================================
 * Vectorization detection
 * ========================================== */

#ifdef XXH_DOXYGEN
/*!
 * @ingroup tuning
 * @brief Overrides the vectorization implementation chosen for XXH3.
 *
 * Can be defined to 0 to disable SIMD or any of the values mentioned in
 * @ref XXH_VECTOR_TYPE.
 *
 * If this is not defined, it uses predefined macros to determine the best
 * implementation.
 */
#  define XXH_VECTOR XXH_SCALAR
/*!
 * @ingroup tuning
 * @brief Selects the minimum alignment for XXH3's accumulators.
 *
 * When using SIMD, this should match the alignment required for said vector
 * type, so, for example, 32 for AVX2.
 *
 * Default: Auto detected.
 */
#  define XXH_ACC_ALIGN 8
#endif

/* Actual definition */
#ifndef XXH_DOXYGEN
#endif

#ifndef XXH_VECTOR    /* can be defined on command line */
#  if defined(__ARM_FEATURE_SVE)
#    define XXH_VECTOR XXH_SVE
#  elif ( \
        defined(__ARM_NEON__) || defined(__ARM_NEON) /* gcc */ \
     || defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC) /* msvc */ \
     || (defined(__wasm_simd128__) && XXH_HAS_INCLUDE(<arm_neon.h>)) /* wasm simd128 via SIMDe */ \
   ) && ( \
        defined(_WIN32) || defined(__LITTLE_ENDIAN__) /* little endian only */ \
    || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) \
   )
#    define XXH_VECTOR XXH_NEON
#  elif defined(__AVX512F__)
#    define XXH_VECTOR XXH_AVX512
#  elif defined(__AVX2__)
#    define XXH_VECTOR XXH_AVX2
#  elif defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP == 2))
#    define XXH_VECTOR XXH_SSE2
#  elif (defined(__PPC64__) && defined(__POWER8_VECTOR__)) \
     || (defined(__s390x__) && defined(__VEC__)) \
     && defined(__GNUC__) /* TODO: IBM XL */
#    define XXH_VECTOR XXH_VSX
#  elif defined(__loongarch_sx)
#    define XXH_VECTOR XXH_LSX
#  else
#    define XXH_VECTOR XXH_SCALAR
#  endif
#endif

/* __ARM_FEATURE_SVE is only supported by GCC & Clang. */
#if (XXH_VECTOR == XXH_SVE) && !defined(__ARM_FEATURE_SVE)
#  ifdef _MSC_VER
#    pragma warning(once : 4606)
#  else
#    warning "__ARM_FEATURE_SVE isn't supported. Use SCALAR instead."
#  endif
#  undef XXH_VECTOR
#  define XXH_VECTOR XXH_SCALAR
#endif

/*
 * Controls the alignment of the accumulator,
 * for compatibility with aligned vector loads, which are usually faster.
 */
#ifndef XXH_ACC_ALIGN
#  if defined(XXH_X86DISPATCH)
#     define XXH_ACC_ALIGN 64  /* for compatibility with avx512 */
#  elif XXH_VECTOR == XXH_SCALAR  /* scalar */
#     define XXH_ACC_ALIGN 8
#  elif XXH_VECTOR == XXH_SSE2  /* sse2 */
#     define XXH_ACC_ALIGN 16
#  elif XXH_VECTOR == XXH_AVX2  /* avx2 */
#     define XXH_ACC_ALIGN 32
#  elif XXH_VECTOR == XXH_NEON  /* neon */
#     define XXH_ACC_ALIGN 16
#  elif XXH_VECTOR == XXH_VSX   /* vsx */
#     define XXH_ACC_ALIGN 16
#  elif XXH_VECTOR == XXH_AVX512  /* avx512 */
#     define XXH_ACC_ALIGN 64
#  elif XXH_VECTOR == XXH_SVE   /* sve */
#     define XXH_ACC_ALIGN 64
#  elif XXH_VECTOR == XXH_LSX   /* lsx */
#     define XXH_ACC_ALIGN 64
#  endif
#endif

#if defined(XXH_X86DISPATCH) || XXH_VECTOR == XXH_SSE2 \
    || XXH_VECTOR == XXH_AVX2 || XXH_VECTOR == XXH_AVX512
#  define XXH_SEC_ALIGN XXH_ACC_ALIGN
#elif XXH_VECTOR == XXH_SVE
#  define XXH_SEC_ALIGN XXH_ACC_ALIGN
#else
#  define XXH_SEC_ALIGN 8
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define XXH_ALIASING __attribute__((__may_alias__))
#else
#  define XXH_ALIASING /* nothing */
#endif

/*
 * UGLY HACK:
 * GCC usually generates the best code with -O3 for xxHash.
 *
 * However, when targeting AVX2, it is overzealous in its unrolling resulting
 * in code roughly 3/4 the speed of Clang.
 *
 * There are other issues, such as GCC splitting _mm256_loadu_si256 into
 * _mm_loadu_si128 + _mm256_inserti128_si256. This is an optimization which
 * only applies to Sandy and Ivy Bridge... which don't even support AVX2.
 *
 * That is why when compiling the AVX2 version, it is recommended to use either
 *   -O2 -mavx2 -march=haswell
 * or
 *   -O2 -mavx2 -mno-avx256-split-unaligned-load
 * for decent performance, or to use Clang instead.
 *
 * Fortunately, we can control the first one with a pragma that forces GCC into
 * -O2, but the other one we can't control without "failed to inline always
 * inline function due to target mismatch" warnings.
 */
#if XXH_VECTOR == XXH_AVX2 /* AVX2 */ \
  && defined(__GNUC__) && !defined(__clang__) /* GCC, not Clang */ \
  && defined(__OPTIMIZE__) && XXH_SIZE_OPT <= 0 /* respect -O0 and -Os */
#  pragma GCC push_options
#  pragma GCC optimize("-O2")
#endif

#if XXH_VECTOR == XXH_NEON

/*
 * UGLY HACK: While AArch64 GCC on Linux does not seem to care, on macOS, GCC -O3
 * optimizes out the entire hashLong loop because of the aliasing violation.
 *
 * However, GCC is also inefficient at load-store optimization with vld1q/vst1q,
 * so the only option is to mark it as aliasing.
 */
typedef uint64x2_t xxh_aliasing_uint64x2_t XXH_ALIASING;

/*!
 * @internal
 * @brief `vld1q_u64` but faster and alignment-safe.
 *
 * On AArch64, unaligned access is always safe, but on ARMv7-a, it is only
 * *conditionally* safe (`vld1` has an alignment bit like `movdq[ua]` in x86).
 *
 * GCC for AArch64 sees `vld1q_u8` as an intrinsic instead of a load, so it
 * prohibits load-store optimizations. Therefore, a direct dereference is used.
 *
 * Otherwise, `vld1q_u8` is used with `vreinterpretq_u8_u64` to do a safe
 * unaligned load.
 */
#if defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__)
XXH_FORCE_INLINE uint64x2_t XXH_vld1q_u64(void const* ptr) /* silence -Wcast-align */
{
    return *(xxh_aliasing_uint64x2_t const *)ptr;
}
#else
XXH_FORCE_INLINE uint64x2_t XXH_vld1q_u64(void const* ptr)
{
    return vreinterpretq_u64_u8(vld1q_u8((uint8_t const*)ptr));
}
#endif

/*!
 * @internal
 * @brief `vmlal_u32` on low and high halves of a vector.
 *
 * This is a workaround for AArch64 GCC < 11 which implemented arm_neon.h with
 * inline assembly and were therefore incapable of merging the `vget_{low, high}_u32`
 * with `vmlal_u32`.
 */
#if defined(__aarch64__) && defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 11
XXH_FORCE_INLINE uint64x2_t
XXH_vmlal_low_u32(uint64x2_t acc, uint32x4_t lhs, uint32x4_t rhs)
{
    /* Inline assembly is the only way */
    __asm__("umlal   %0.2d, %1.2s, %2.2s" : "+w" (acc) : "w" (lhs), "w" (rhs));
    return acc;
}
XXH_FORCE_INLINE uint64x2_t
XXH_vmlal_high_u32(uint64x2_t acc, uint32x4_t lhs, uint32x4_t rhs)
{
    /* This intrinsic works as expected */
    return vmlal_high_u32(acc, lhs, rhs);
}
#else
/* Portable intrinsic versions */
XXH_FORCE_INLINE uint64x2_t
XXH_vmlal_low_u32(uint64x2_t acc, uint32x4_t lhs, uint32x4_t rhs)
{
    return vmlal_u32(acc, vget_low_u32(lhs), vget_low_u32(rhs));
}
/*! @copydoc XXH_vmlal_low_u32
 * Assume the compiler converts this to vmlal_high_u32 on aarch64 */
XXH_FORCE_INLINE uint64x2_t
XXH_vmlal_high_u32(uint64x2_t acc, uint32x4_t lhs, uint32x4_t rhs)
{
    return vmlal_u32(acc, vget_high_u32(lhs), vget_high_u32(rhs));
}
#endif

/*!
 * @ingroup tuning
 * @brief Controls the NEON to scalar ratio for XXH3
 *
 * This can be set to 2, 4, 6, or 8.
 *
 * ARM Cortex CPUs are _very_ sensitive to how their pipelines are used.
 *
 * For example, the Cortex-A73 can dispatch 3 micro-ops per cycle, but only 2 of those
 * can be NEON. If you are only using NEON instructions, you are only using 2/3 of the CPU
 * bandwidth.
 *
 * This is even more noticeable on the more advanced cores like the Cortex-A76 which
 * can dispatch 8 micro-ops per cycle, but still only 2 NEON micro-ops at once.
 *
 * Therefore, to make the most out of the pipeline, it is beneficial to run 6 NEON lanes
 * and 2 scalar lanes, which is chosen by default.
 *
 * This does not apply to Apple processors or 32-bit processors, which run better with
 * full NEON. These will default to 8. Additionally, size-optimized builds run 8 lanes.
 *
 * This change benefits CPUs with large micro-op buffers without negatively affecting
 * most other CPUs:
 *
 *  | Chipset               | Dispatch type       | NEON only | 6:2 hybrid | Diff. |
 *  |:----------------------|:--------------------|----------:|-----------:|------:|
 *  | Snapdragon 730 (A76)  | 2 NEON/8 micro-ops  |  8.8 GB/s |  10.1 GB/s |  ~16% |
 *  | Snapdragon 835 (A73)  | 2 NEON/3 micro-ops  |  5.1 GB/s |   5.3 GB/s |   ~5% |
 *  | Marvell PXA1928 (A53) | In-order dual-issue |  1.9 GB/s |   1.9 GB/s |    0% |
 *  | Apple M1              | 4 NEON/8 micro-ops  | 37.3 GB/s |  36.1 GB/s |  ~-3% |
 *
 * It also seems to fix some bad codegen on GCC, making it almost as fast as clang.
 *
 * When using WASM SIMD128, if this is 2 or 6, SIMDe will scalarize 2 of the lanes meaning
 * it effectively becomes worse 4.
 *
 * @see XXH3_accumulate_512_neon()
 */
# ifndef XXH3_NEON_LANES
#  if (defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(_M_ARM64EC)) \
   && !defined(__APPLE__) && XXH_SIZE_OPT <= 0
#   define XXH3_NEON_LANES 6
#  else
#   define XXH3_NEON_LANES XXH_ACC_NB
#  endif
# endif
#endif  /* XXH_VECTOR == XXH_NEON */

/*
 * VSX and Z Vector helpers.
 *
 * This is very messy, and any pull requests to clean this up are welcome.
 *
 * There are a lot of problems with supporting VSX and s390x, due to
 * inconsistent intrinsics, spotty coverage, and multiple endiannesses.
 */
#if XXH_VECTOR == XXH_VSX
/* Annoyingly, these headers _may_ define three macros: `bool`, `vector`,
 * and `pixel`. This is a problem for obvious reasons.
 *
 * These keywords are unnecessary; the spec literally says they are
 * equivalent to `__bool`, `__vector`, and `__pixel` and may be undef'd
 * after including the header.
 *
 * We use pragma push_macro/pop_macro to keep the namespace clean. */
#  pragma push_macro("bool")
#  pragma push_macro("vector")
#  pragma push_macro("pixel")
/* silence potential macro redefined warnings */
#  undef bool
#  undef vector
#  undef pixel

#  if defined(__s390x__)
#    include <s390intrin.h>
#  else
#    include <altivec.h>
#  endif

/* Restore the original macro values, if applicable. */
#  pragma pop_macro("pixel")
#  pragma pop_macro("vector")
#  pragma pop_macro("bool")

typedef __vector unsigned long long xxh_u64x2;
typedef __vector unsigned char xxh_u8x16;
typedef __vector unsigned xxh_u32x4;

/*
 * UGLY HACK: Similar to aarch64 macOS GCC, s390x GCC has the same aliasing issue.
 */
typedef xxh_u64x2 xxh_aliasing_u64x2 XXH_ALIASING;

# ifndef XXH_VSX_BE
#  if defined(__BIG_ENDIAN__) \
  || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#    define XXH_VSX_BE 1
#  elif defined(__VEC_ELEMENT_REG_ORDER__) && __VEC_ELEMENT_REG_ORDER__ == __ORDER_BIG_ENDIAN__
#    warning "-maltivec=be is not recommended. Please use native endianness."
#    define XXH_VSX_BE 1
#  else
#    define XXH_VSX_BE 0
#  endif
# endif /* !defined(XXH_VSX_BE) */

# if XXH_VSX_BE
#  if defined(__POWER9_VECTOR__) || (defined(__clang__) && defined(__s390x__))
#    define XXH_vec_revb vec_revb
#  else
/*!
 * A polyfill for POWER9's vec_revb().
 */
XXH_FORCE_INLINE xxh_u64x2 XXH_vec_revb(xxh_u64x2 val)
{
    xxh_u8x16 const vByteSwap = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
                                  0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08 };
    return vec_perm(val, val, vByteSwap);
}
#  endif
# endif /* XXH_VSX_BE */

/*!
 * Performs an unaligned vector load and byte swaps it on big endian.
 */
XXH_FORCE_INLINE xxh_u64x2 XXH_vec_loadu(const void *ptr)
{
    xxh_u64x2 ret;
    XXH_memcpy(&ret, ptr, sizeof(xxh_u64x2));
# if XXH_VSX_BE
    ret = XXH_vec_revb(ret);
# endif
    return ret;
}

/*
 * vec_mulo and vec_mule are very problematic intrinsics on PowerPC
 *
 * These intrinsics weren't added until GCC 8, despite existing for a while,
 * and they are endian dependent. Also, their meaning swap depending on version.
 * */
# if defined(__s390x__)
 /* s390x is always big endian, no issue on this platform */
#  define XXH_vec_mulo vec_mulo
#  define XXH_vec_mule vec_mule
# elif defined(__clang__) && XXH_HAS_BUILTIN(__builtin_altivec_vmuleuw) && !defined(__ibmxl__)
/* Clang has a better way to control this, we can just use the builtin which doesn't swap. */
 /* The IBM XL Compiler (which defined __clang__) only implements the vec_* operations */
#  define XXH_vec_mulo __builtin_altivec_vmulouw
#  define XXH_vec_mule __builtin_altivec_vmuleuw
# else
/* gcc needs inline assembly */
/* Adapted from https://github.com/google/highwayhash/blob/master/highwayhash/hh_vsx.h. */
XXH_FORCE_INLINE xxh_u64x2 XXH_vec_mulo(xxh_u32x4 a, xxh_u32x4 b)
{
    xxh_u64x2 result;
    __asm__("vmulouw %0, %1, %2" : "=v" (result) : "v" (a), "v" (b));
    return result;
}
XXH_FORCE_INLINE xxh_u64x2 XXH_vec_mule(xxh_u32x4 a, xxh_u32x4 b)
{
    xxh_u64x2 result;
    __asm__("vmuleuw %0, %1, %2" : "=v" (result) : "v" (a), "v" (b));
    return result;
}
# endif /* XXH_vec_mulo, XXH_vec_mule */
#endif /* XXH_VECTOR == XXH_VSX */

#if XXH_VECTOR == XXH_SVE
#define ACCRND(acc, offset) \
do { \
    svuint64_t input_vec = svld1_u64(mask, xinput + offset);         \
    svuint64_t secret_vec = svld1_u64(mask, xsecret + offset);       \
    svuint64_t mixed = sveor_u64_x(mask, secret_vec, input_vec);     \
    svuint64_t swapped = svtbl_u64(input_vec, kSwap);                \
    svuint64_t mixed_lo = svextw_u64_x(mask, mixed);                 \
    svuint64_t mixed_hi = svlsr_n_u64_x(mask, mixed, 32);            \
    svuint64_t mul = svmad_u64_x(mask, mixed_lo, mixed_hi, swapped); \
    acc = svadd_u64_x(mask, acc, mul);                               \
} while (0)
#endif /* XXH_VECTOR == XXH_SVE */

/* prefetch
 * can be disabled, by declaring XXH_NO_PREFETCH build macro */
#if defined(XXH_NO_PREFETCH)
#  define XXH_PREFETCH(ptr)  (void)(ptr)  /* disabled */
#else
#  if XXH_SIZE_OPT >= 1
#    define XXH_PREFETCH(ptr) (void)(ptr)
#  elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))  /* _mm_prefetch() not defined outside of x86/x64 */
#    include <mmintrin.h>   /* https://msdn.microsoft.com/fr-fr/library/84szxsww(v=vs.90).aspx */
#    define XXH_PREFETCH(ptr)  _mm_prefetch((const char*)(ptr), _MM_HINT_T0)
#  elif defined(__GNUC__) && ( (__GNUC__ >= 4) || ( (__GNUC__ == 3) && (__GNUC_MINOR__ >= 1) ) )
#    define XXH_PREFETCH(ptr)  __builtin_prefetch((ptr), 0 /* rw==read */, 3 /* locality */)
#  else
#    define XXH_PREFETCH(ptr) (void)(ptr)  /* disabled */
#  endif
#endif  /* XXH_NO_PREFETCH */


/* ==========================================
 * XXH3 default settings
 * ========================================== */

#define XXH_SECRET_DEFAULT_SIZE 192   /* minimum XXH3_SECRET_SIZE_MIN */

#if (XXH_SECRET_DEFAULT_SIZE < XXH3_SECRET_SIZE_MIN)
#  error "default keyset is not large enough"
#endif

/*! Pseudorandom secret taken directly from FARSH. */
XXH_ALIGN(64) static const xxh_u8 XXH3_kSecret[XXH_SECRET_DEFAULT_SIZE] = {
    0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81, 0x2c, 0xf7, 0x21, 0xad, 0x1c,
    0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90, 0x97, 0xdb, 0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f,
    0xcb, 0x79, 0xe6, 0x4e, 0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d, 0xcc, 0xff, 0x72, 0x21,
    0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24, 0x8e, 0xe0, 0x35, 0x90, 0xe6, 0x81, 0x3a, 0x26, 0x4c,
    0x3c, 0x28, 0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb, 0x88, 0xd0, 0x65, 0x8b, 0x1b, 0x53, 0x2e, 0xa3,
    0x71, 0x64, 0x48, 0x97, 0xa2, 0x0d, 0xf9, 0x4e, 0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8,
    0xa8, 0xfa, 0x76, 0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7, 0xc7, 0x0b, 0x4f, 0x1d,
    0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31, 0xc8, 0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64,
    0xea, 0xc5, 0xac, 0x83, 0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63, 0xeb,
    0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16, 0x55, 0x26, 0x29, 0xd4, 0x68, 0x9e,
    0x2b, 0x16, 0xbe, 0x58, 0x7d, 0x47, 0xa1, 0xfc, 0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce,
    0x45, 0xcb, 0x3a, 0x8f, 0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb, 0xca, 0xbb, 0x4b, 0x40, 0x7e,
};

static const xxh_u64 PRIME_MX1 = 0x165667919E3779F9ULL;  /*!< 0b0001011001010110011001111001000110011110001101110111100111111001 */
static const xxh_u64 PRIME_MX2 = 0x9FB21C651E98DF25ULL;  /*!< 0b1001111110110010000111000110010100011110100110001101111100100101 */

#ifdef XXH_OLD_NAMES
#  define kSecret XXH3_kSecret
#endif

#ifdef XXH_DOXYGEN
/*!
 * @brief Calculates a 32-bit to 64-bit long multiply.
 *
 * Implemented as a macro.
 *
 * Wraps `__emulu` on MSVC x86 because it tends to call `__allmul` when it doesn't
 * need to (but it shouldn't need to anyways, it is about 7 instructions to do
 * a 64x64 multiply...). Since we know that this will _always_ emit `MULL`, we
 * use that instead of the normal method.
 *
 * If you are compiling for platforms like Thumb-1 and don't have a better option,
 * you may also want to write your own long multiply routine here.
 *
 * @param x, y Numbers to be multiplied
 * @return 64-bit product of the low 32 bits of @p x and @p y.
 */
XXH_FORCE_INLINE xxh_u64
XXH_mult32to64(xxh_u64 x, xxh_u64 y)
{
   return (x & 0xFFFFFFFF) * (y & 0xFFFFFFFF);
}
#elif defined(_MSC_VER) && defined(_M_IX86)
#    define XXH_mult32to64(x, y) __emulu((unsigned)(x), (unsigned)(y))
#else
/*
 * Downcast + upcast is usually better than masking on older compilers like
 * GCC 4.2 (especially 32-bit ones), all without affecting newer compilers.
 *
 * The other method, (x & 0xFFFFFFFF) * (y & 0xFFFFFFFF), will AND both operands
 * and perform a full 64x64 multiply -- entirely redundant on 32-bit.
 */
#    define XXH_mult32to64(x, y) ((xxh_u64)(xxh_u32)(x) * (xxh_u64)(xxh_u32)(y))
#endif

/*!
 * @brief Calculates a 64->128-bit long multiply.
 *
 * Uses `__uint128_t` and `_umul128` if available, otherwise uses a scalar
 * version.
 *
 * @param lhs , rhs The 64-bit integers to be multiplied
 * @return The 128-bit result represented in an @ref XXH128_hash_t.
 */
static XXH128_hash_t
XXH_mult64to128(xxh_u64 lhs, xxh_u64 rhs)
{
    /*
     * GCC/Clang __uint128_t method.
     *
     * On most 64-bit targets, GCC and Clang define a __uint128_t type.
     * This is usually the best way as it usually uses a native long 64-bit
     * multiply, such as MULQ on x86_64 or MUL + UMULH on aarch64.
     *
     * Usually.
     *
     * Despite being a 32-bit platform, Clang (and emscripten) define this type
     * despite not having the arithmetic for it. This results in a laggy
     * compiler builtin call which calculates a full 128-bit multiply.
     * In that case it is best to use the portable one.
     * https://github.com/Cyan4973/xxHash/issues/211#issuecomment-515575677
     */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__wasm__) \
    && defined(__SIZEOF_INT128__) \
    || (defined(_INTEGRAL_MAX_BITS) && _INTEGRAL_MAX_BITS >= 128)

    __uint128_t const product = (__uint128_t)lhs * (__uint128_t)rhs;
    XXH128_hash_t r128;
    r128.low64  = (xxh_u64)(product);
    r128.high64 = (xxh_u64)(product >> 64);
    return r128;

    /*
     * MSVC for x64's _umul128 method.
     *
     * xxh_u64 _umul128(xxh_u64 Multiplier, xxh_u64 Multiplicand, xxh_u64 *HighProduct);
     *
     * This compiles to single operand MUL on x64.
     */
#elif (defined(_M_X64) || defined(_M_IA64)) && !defined(_M_ARM64EC)

#ifndef _MSC_VER
#   pragma intrinsic(_umul128)
#endif
    xxh_u64 product_high;
    xxh_u64 const product_low = _umul128(lhs, rhs, &product_high);
    XXH128_hash_t r128;
    r128.low64  = product_low;
    r128.high64 = product_high;
    return r128;

    /*
     * MSVC for ARM64's __umulh method.
     *
     * This compiles to the same MUL + UMULH as GCC/Clang's __uint128_t method.
     */
#elif defined(_M_ARM64) || defined(_M_ARM64EC)

#ifndef _MSC_VER
#   pragma intrinsic(__umulh)
#endif
    XXH128_hash_t r128;
    r128.low64  = lhs * rhs;
    r128.high64 = __umulh(lhs, rhs);
    return r128;

#else
    /*
     * Portable scalar method. Optimized for 32-bit and 64-bit ALUs.
     *
     * This is a fast and simple grade school multiply, which is shown below
     * with base 10 arithmetic instead of base 0x100000000.
     *
     *           9 3 // D2 lhs = 93
     *         x 7 5 // D2 rhs = 75
     *     ----------
     *           1 5 // D2 lo_lo = (93 % 10) * (75 % 10) = 15
     *         4 5 | // D2 hi_lo = (93 / 10) * (75 % 10) = 45
     *         2 1 | // D2 lo_hi = (93 % 10) * (75 / 10) = 21
     *     + 6 3 | | // D2 hi_hi = (93 / 10) * (75 / 10) = 63
     *     ---------
     *         2 7 | // D2 cross = (15 / 10) + (45 % 10) + 21 = 27
     *     + 6 7 | | // D2 upper = (27 / 10) + (45 / 10) + 63 = 67
     *     ---------
     *       6 9 7 5 // D4 res = (27 * 10) + (15 % 10) + (67 * 100) = 6975
     *
     * The reasons for adding the products like this are:
     *  1. It avoids manual carry tracking. Just like how
     *     (9 * 9) + 9 + 9 = 99, the same applies with this for UINT64_MAX.
     *     This avoids a lot of complexity.
     *
     *  2. It hints for, and on Clang, compiles to, the powerful UMAAL
     *     instruction available in ARM's Digital Signal Processing extension
     *     in 32-bit ARMv6 and later, which is shown below:
     *
     *         void UMAAL(xxh_u32 *RdLo, xxh_u32 *RdHi, xxh_u32 Rn, xxh_u32 Rm)
     *         {
     *             xxh_u64 product = (xxh_u64)*RdLo * (xxh_u64)*RdHi + Rn + Rm;
     *             *RdLo = (xxh_u32)(product & 0xFFFFFFFF);
     *             *RdHi = (xxh_u32)(product >> 32);
     *         }
     *
     *     This instruction was designed for efficient long multiplication, and
     *     allows this to be calculated in only 4 instructions at speeds
     *     comparable to some 64-bit ALUs.
     *
     *  3. It isn't terrible on other platforms. Usually this will be a couple
     *     of 32-bit ADD/ADCs.
     */

    /* First calculate all of the cross products. */
    xxh_u64 const lo_lo = XXH_mult32to64(lhs & 0xFFFFFFFF, rhs & 0xFFFFFFFF);
    xxh_u64 const hi_lo = XXH_mult32to64(lhs >> 32,        rhs & 0xFFFFFFFF);
    xxh_u64 const lo_hi = XXH_mult32to64(lhs & 0xFFFFFFFF, rhs >> 32);
    xxh_u64 const hi_hi = XXH_mult32to64(lhs >> 32,        rhs >> 32);

    /* Now add the products together. These will never overflow. */
    xxh_u64 const cross = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFF) + lo_hi;
    xxh_u64 const upper = (hi_lo >> 32) + (cross >> 32)        + hi_hi;
    xxh_u64 const lower = (cross << 32) | (lo_lo & 0xFFFFFFFF);

    XXH128_hash_t r128;
    r128.low64  = lower;
    r128.high64 = upper;
    return r128;
#endif
}

/*!
 * @brief Calculates a 64-bit to 128-bit multiply, then XOR folds it.
 *
 * The reason for the separate function is to prevent passing too many structs
 * around by value. This will hopefully inline the multiply, but we don't force it.
 *
 * @param lhs , rhs The 64-bit integers to multiply
 * @return The low 64 bits of the product XOR'd by the high 64 bits.
 * @see XXH_mult64to128()
 */
static xxh_u64
XXH3_mul128_fold64(xxh_u64 lhs, xxh_u64 rhs)
{
    XXH128_hash_t product = XXH_mult64to128(lhs, rhs);
    return product.low64 ^ product.high64;
}

/*! Seems to produce slightly better code on GCC for some reason. */
XXH_FORCE_INLINE XXH_CONSTF xxh_u64 XXH_xorshift64(xxh_u64 v64, int shift)
{
    XXH_ASSERT(0 <= shift && shift < 64);
    return v64 ^ (v64 >> shift);
}

/*
 * This is a fast avalanche stage,
 * suitable when input bits are already partially mixed
 */
static XXH64_hash_t XXH3_avalanche(xxh_u64 h64)
{
    h64 = XXH_xorshift64(h64, 37);
    h64 *= PRIME_MX1;
    h64 = XXH_xorshift64(h64, 32);
    return h64;
}

/*
 * This is a stronger avalanche,
 * inspired by Pelle Evensen's rrmxmx
 * preferable when input has not been previously mixed
 */
static XXH64_hash_t XXH3_rrmxmx(xxh_u64 h64, xxh_u64 len)
{
    /* this mix is inspired by Pelle Evensen's rrmxmx */
    h64 ^= XXH_rotl64(h64, 49) ^ XXH_rotl64(h64, 24);
    h64 *= PRIME_MX2;
    h64 ^= (h64 >> 35) + len ;
    h64 *= PRIME_MX2;
    return XXH_xorshift64(h64, 28);
}


/* ==========================================
 * Short keys
 * ==========================================
 * One of the shortcomings of XXH32 and XXH64 was that their performance was
 * sub-optimal on short lengths. It used an iterative algorithm which strongly
 * favored lengths that were a multiple of 4 or 8.
 *
 * Instead of iterating over individual inputs, we use a set of single shot
 * functions which piece together a range of lengths and operate in constant time.
 *
 * Additionally, the number of multiplies has been significantly reduced. This
 * reduces latency, especially when emulating 64-bit multiplies on 32-bit.
 *
 * Depending on the platform, this may or may not be faster than XXH32, but it
 * is almost guaranteed to be faster than XXH64.
 */

/*
 * At very short lengths, there isn't enough input to fully hide secrets, or use
 * the entire secret.
 *
 * There is also only a limited amount of mixing we can do before significantly
 * impacting performance.
 *
 * Therefore, we use different sections of the secret and always mix two secret
 * samples with an XOR. This should have no effect on performance on the
 * seedless or withSeed variants because everything _should_ be constant folded
 * by modern compilers.
 *
 * The XOR mixing hides individual parts of the secret and increases entropy.
 *
 * This adds an extra layer of strength for custom secrets.
 */
XXH_FORCE_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_1to3_64b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(1 <= len && len <= 3);
    XXH_ASSERT(secret != NULL);
    /*
     * len = 1: combined = { input[0], 0x01, input[0], input[0] }
     * len = 2: combined = { input[1], 0x02, input[0], input[1] }
     * len = 3: combined = { input[2], 0x03, input[0], input[1] }
     */
    {   xxh_u8  const c1 = input[0];
        xxh_u8  const c2 = input[len >> 1];
        xxh_u8  const c3 = input[len - 1];
        xxh_u32 const combined = ((xxh_u32)c1 << 16) | ((xxh_u32)c2  << 24)
                               | ((xxh_u32)c3 <<  0) | ((xxh_u32)len << 8);
        xxh_u64 const bitflip = (XXH_readLE32(secret) ^ XXH_readLE32(secret+4)) + seed;
        xxh_u64 const keyed = (xxh_u64)combined ^ bitflip;
        return XXH64_avalanche(keyed);
    }
}

XXH_FORCE_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_4to8_64b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(secret != NULL);
    XXH_ASSERT(4 <= len && len <= 8);
    seed ^= (xxh_u64)XXH_swap32((xxh_u32)seed) << 32;
    {   xxh_u32 const input1 = XXH_readLE32(input);
        xxh_u32 const input2 = XXH_readLE32(input + len - 4);
        xxh_u64 const bitflip = (XXH_readLE64(secret+8) ^ XXH_readLE64(secret+16)) - seed;
        xxh_u64 const input64 = input2 + (((xxh_u64)input1) << 32);
        xxh_u64 const keyed = input64 ^ bitflip;
        return XXH3_rrmxmx(keyed, len);
    }
}

XXH_FORCE_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_9to16_64b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(secret != NULL);
    XXH_ASSERT(9 <= len && len <= 16);
    {   xxh_u64 const bitflip1 = (XXH_readLE64(secret+24) ^ XXH_readLE64(secret+32)) + seed;
        xxh_u64 const bitflip2 = (XXH_readLE64(secret+40) ^ XXH_readLE64(secret+48)) - seed;
        xxh_u64 const input_lo = XXH_readLE64(input)           ^ bitflip1;
        xxh_u64 const input_hi = XXH_readLE64(input + len - 8) ^ bitflip2;
        xxh_u64 const acc = len
                          + XXH_swap64(input_lo) + input_hi
                          + XXH3_mul128_fold64(input_lo, input_hi);
        return XXH3_avalanche(acc);
    }
}

XXH_FORCE_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_0to16_64b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(len <= 16);
    {   if (XXH_likely(len >  8)) return XXH3_len_9to16_64b(input, len, secret, seed);
        if (XXH_likely(len >= 4)) return XXH3_len_4to8_64b(input, len, secret, seed);
        if (len) return XXH3_len_1to3_64b(input, len, secret, seed);
        return XXH64_avalanche(seed ^ (XXH_readLE64(secret+56) ^ XXH_readLE64(secret+64)));
    }
}

/*
 * DISCLAIMER: There are known *seed-dependent* multicollisions here due to
 * multiplication by zero, affecting hashes of lengths 17 to 240.
 *
 * However, they are very unlikely.
 *
 * Keep this in mind when using the unseeded XXH3_64bits() variant: As with all
 * unseeded non-cryptographic hashes, it does not attempt to defend itself
 * against specially crafted inputs, only random inputs.
 *
 * Compared to classic UMAC where a 1 in 2^31 chance of 4 consecutive bytes
 * cancelling out the secret is taken an arbitrary number of times (addressed
 * in XXH3_accumulate_512), this collision is very unlikely with random inputs
 * and/or proper seeding:
 *
 * This only has a 1 in 2^63 chance of 8 consecutive bytes cancelling out, in a
 * function that is only called up to 16 times per hash with up to 240 bytes of
 * input.
 *
 * This is not too bad for a non-cryptographic hash function, especially with
 * only 64 bit outputs.
 *
 * The 128-bit variant (which trades some speed for strength) is NOT affected
 * by this, although it is always a good idea to use a proper seed if you care
 * about strength.
 */
XXH_FORCE_INLINE xxh_u64 XXH3_mix16B(const xxh_u8* XXH_RESTRICT input,
                                     const xxh_u8* XXH_RESTRICT secret, xxh_u64 seed64)
{
#if defined(__GNUC__) && !defined(__clang__) /* GCC, not Clang */ \
  && defined(__i386__) && defined(__SSE2__)  /* x86 + SSE2 */ \
  && !defined(XXH_ENABLE_AUTOVECTORIZE)      /* Define to disable like XXH32 hack */
    /*
     * UGLY HACK:
     * GCC for x86 tends to autovectorize the 128-bit multiply, resulting in
     * slower code.
     *
     * By forcing seed64 into a register, we disrupt the cost model and
     * cause it to scalarize. See `XXH32_round()`
     *
     * FIXME: Clang's output is still _much_ faster -- On an AMD Ryzen 3600,
     * XXH3_64bits @ len=240 runs at 4.6 GB/s with Clang 9, but 3.3 GB/s on
     * GCC 9.2, despite both emitting scalar code.
     *
     * GCC generates much better scalar code than Clang for the rest of XXH3,
     * which is why finding a more optimal codepath is an interest.
     */
    XXH_COMPILER_GUARD(seed64);
#endif
    {   xxh_u64 const input_lo = XXH_readLE64(input);
        xxh_u64 const input_hi = XXH_readLE64(input+8);
        return XXH3_mul128_fold64(
            input_lo ^ (XXH_readLE64(secret)   + seed64),
            input_hi ^ (XXH_readLE64(secret+8) - seed64)
        );
    }
}

/* For mid range keys, XXH3 uses a Mum-hash variant. */
XXH_FORCE_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_17to128_64b(const xxh_u8* XXH_RESTRICT input, size_t len,
                     const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                     XXH64_hash_t seed)
{
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(16 < len && len <= 128);

    {   xxh_u64 acc = len * XXH_PRIME64_1;
#if XXH_SIZE_OPT >= 1
        /* Smaller and cleaner, but slightly slower. */
        unsigned int i = (unsigned int)(len - 1) / 32;
        do {
            acc += XXH3_mix16B(input+16 * i, secret+32*i, seed);
            acc += XXH3_mix16B(input+len-16*(i+1), secret+32*i+16, seed);
        } while (i-- != 0);
#else
        if (len > 32) {
            if (len > 64) {
                if (len > 96) {
                    acc += XXH3_mix16B(input+48, secret+96, seed);
                    acc += XXH3_mix16B(input+len-64, secret+112, seed);
                }
                acc += XXH3_mix16B(input+32, secret+64, seed);
                acc += XXH3_mix16B(input+len-48, secret+80, seed);
            }
            acc += XXH3_mix16B(input+16, secret+32, seed);
            acc += XXH3_mix16B(input+len-32, secret+48, seed);
        }
        acc += XXH3_mix16B(input+0, secret+0, seed);
        acc += XXH3_mix16B(input+len-16, secret+16, seed);
#endif
        return XXH3_avalanche(acc);
    }
}

XXH_NO_INLINE XXH_PUREF XXH64_hash_t
XXH3_len_129to240_64b(const xxh_u8* XXH_RESTRICT input, size_t len,
                      const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                      XXH64_hash_t seed)
{
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(128 < len && len <= XXH3_MIDSIZE_MAX);

    #define XXH3_MIDSIZE_STARTOFFSET 3
    #define XXH3_MIDSIZE_LASTOFFSET  17

    {   xxh_u64 acc = len * XXH_PRIME64_1;
        xxh_u64 acc_end;
        unsigned int const nbRounds = (unsigned int)len / 16;
        unsigned int i;
        XXH_ASSERT(128 < len && len <= XXH3_MIDSIZE_MAX);
        for (i=0; i<8; i++) {
            acc += XXH3_mix16B(input+(16*i), secret+(16*i), seed);
        }
        /* last bytes */
        acc_end = XXH3_mix16B(input + len - 16, secret + XXH3_SECRET_SIZE_MIN - XXH3_MIDSIZE_LASTOFFSET, seed);
        XXH_ASSERT(nbRounds >= 8);
        acc = XXH3_avalanche(acc);
#if defined(__clang__)                                /* Clang */ \
    && (defined(__ARM_NEON) || defined(__ARM_NEON__)) /* NEON */ \
    && !defined(XXH_ENABLE_AUTOVECTORIZE)             /* Define to disable */
        /*
         * UGLY HACK:
         * Clang for ARMv7-A tries to vectorize this loop, similar to GCC x86.
         * In everywhere else, it uses scalar code.
         *
         * For 64->128-bit multiplies, even if the NEON was 100% optimal, it
         * would still be slower than UMAAL (see XXH_mult64to128).
         *
         * Unfortunately, Clang doesn't handle the long multiplies properly and
         * converts them to the nonexistent "vmulq_u64" intrinsic, which is then
         * scalarized into an ugly mess of VMOV.32 instructions.
         *
         * This mess is difficult to avoid without turning autovectorization
         * off completely, but they are usually relatively minor and/or not
         * worth it to fix.
         *
         * This loop is the easiest to fix, as unlike XXH32, this pragma
         * _actually works_ because it is a loop vectorization instead of an
         * SLP vectorization.
         */
        #pragma clang loop vectorize(disable)
#endif
        for (i=8 ; i < nbRounds; i++) {
            /*
             * Prevents clang for unrolling the acc loop and interleaving with this one.
             */
            XXH_COMPILER_GUARD(acc);
            acc_end += XXH3_mix16B(input+(16*i), secret+(16*(i-8)) + XXH3_MIDSIZE_STARTOFFSET, seed);
        }
        return XXH3_avalanche(acc + acc_end);
    }
}


/* =======     Long Keys     ======= */

#define XXH_STRIPE_LEN 64
#define XXH_SECRET_CONSUME_RATE 8   /* nb of secret bytes consumed at each accumulation */
#define XXH_ACC_NB (XXH_STRIPE_LEN / sizeof(xxh_u64))

#ifdef XXH_OLD_NAMES
#  define STRIPE_LEN XXH_STRIPE_LEN
#  define ACC_NB XXH_ACC_NB
#endif

#ifndef XXH_PREFETCH_DIST
#  ifdef __clang__
#    define XXH_PREFETCH_DIST 320
#  else
#    if (XXH_VECTOR == XXH_AVX512)
#      define XXH_PREFETCH_DIST 512
#    else
#      define XXH_PREFETCH_DIST 384
#    endif
#  endif  /* __clang__ */
#endif  /* XXH_PREFETCH_DIST */

/*
 * These macros are to generate an XXH3_accumulate() function.
 * The two arguments select the name suffix and target attribute.
 *
 * The name of this symbol is XXH3_accumulate_<name>() and it calls
 * XXH3_accumulate_512_<name>().
 *
 * It may be useful to hand implement this function if the compiler fails to
 * optimize the inline function.
 */
#define XXH3_ACCUMULATE_TEMPLATE(name)                      \
void                                                        \
XXH3_accumulate_##name(xxh_u64* XXH_RESTRICT acc,           \
                       const xxh_u8* XXH_RESTRICT input,    \
                       const xxh_u8* XXH_RESTRICT secret,   \
                       size_t nbStripes)                    \
{                                                           \
    size_t n;                                               \
    for (n = 0; n < nbStripes; n++ ) {                      \
        const xxh_u8* const in = input + n*XXH_STRIPE_LEN;  \
        XXH_PREFETCH(in + XXH_PREFETCH_DIST);               \
        XXH3_accumulate_512_##name(                         \
                 acc,                                       \
                 in,                                        \
                 secret + n*XXH_SECRET_CONSUME_RATE);       \
    }                                                       \
}


XXH_FORCE_INLINE void XXH_writeLE64(void* dst, xxh_u64 v64)
{
    if (!XXH_CPU_LITTLE_ENDIAN) v64 = XXH_swap64(v64);
    XXH_memcpy(dst, &v64, sizeof(v64));
}

/* Several intrinsic functions below are supposed to accept __int64 as argument,
 * as documented in https://software.intel.com/sites/landingpage/IntrinsicsGuide/ .
 * However, several environments do not define __int64 type,
 * requiring a workaround.
 */
#if !defined (__VMS) \
  && (defined (__cplusplus) \
  || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
    typedef int64_t xxh_i64;
#else
    /* the following type must have a width of 64-bit */
    typedef long long xxh_i64;
#endif


/*
 * XXH3_accumulate_512 is the tightest loop for long inputs, and it is the most optimized.
 *
 * It is a hardened version of UMAC, based off of FARSH's implementation.
 *
 * This was chosen because it adapts quite well to 32-bit, 64-bit, and SIMD
 * implementations, and it is ridiculously fast.
 *
 * We harden it by mixing the original input to the accumulators as well as the product.
 *
 * This means that in the (relatively likely) case of a multiply by zero, the
 * original input is preserved.
 *
 * On 128-bit inputs, we swap 64-bit pairs when we add the input to improve
 * cross-pollination, as otherwise the upper and lower halves would be
 * essentially independent.
 *
 * This doesn't matter on 64-bit hashes since they all get merged together in
 * the end, so we skip the extra step.
 *
 * Both XXH3_64bits and XXH3_128bits use this subroutine.
 */

#if (XXH_VECTOR == XXH_AVX512) \
     || (defined(XXH_DISPATCH_AVX512) && XXH_DISPATCH_AVX512 != 0)

#ifndef XXH_TARGET_AVX512
# define XXH_TARGET_AVX512  /* disable attribute target */
#endif

XXH_FORCE_INLINE XXH_TARGET_AVX512 void
XXH3_accumulate_512_avx512(void* XXH_RESTRICT acc,
                     const void* XXH_RESTRICT input,
                     const void* XXH_RESTRICT secret)
{
    __m512i* const xacc = (__m512i *) acc;
    XXH_ASSERT((((size_t)acc) & 63) == 0);
    XXH_STATIC_ASSERT(XXH_STRIPE_LEN == sizeof(__m512i));

    {
        /* data_vec    = input[0]; */
        __m512i const data_vec    = _mm512_loadu_si512   (input);
        /* key_vec     = secret[0]; */
        __m512i const key_vec     = _mm512_loadu_si512   (secret);
        /* data_key    = data_vec ^ key_vec; */
        __m512i const data_key    = _mm512_xor_si512     (data_vec, key_vec);
        /* data_key_lo = data_key >> 32; */
        __m512i const data_key_lo = _mm512_srli_epi64 (data_key, 32);
        /* product     = (data_key & 0xffffffff) * (data_key_lo & 0xffffffff); */
        __m512i const product     = _mm512_mul_epu32     (data_key, data_key_lo);
        /* xacc[0] += swap(data_vec); */
        __m512i const data_swap = _mm512_shuffle_epi32(data_vec, (_MM_PERM_ENUM)_MM_SHUFFLE(1, 0, 3, 2));
        __m512i const sum       = _mm512_add_epi64(*xacc, data_swap);
        /* xacc[0] += product; */
        *xacc = _mm512_add_epi64(product, sum);
    }
}
XXH_FORCE_INLINE XXH_TARGET_AVX512 XXH3_ACCUMULATE_TEMPLATE(avx512)

/*
 * XXH3_scrambleAcc: Scrambles the accumulators to improve mixing.
 *
 * Multiplication isn't perfect, as explained by Google in HighwayHash:
 *
 *  // Multiplication mixes/scrambles bytes 0-7 of the 64-bit result to
 *  // varying degrees. In descending order of goodness, bytes
 *  // 3 4 2 5 1 6 0 7 have quality 228 224 164 160 100 96 36 32.
 *  // As expected, the upper and lower bytes are much worse.
 *
 * Source: https://github.com/google/highwayhash/blob/0aaf66b/highwayhash/hh_avx2.h#L291
 *
 * Since our algorithm uses a pseudorandom secret to add some variance into the
 * mix, we don't need to (or want to) mix as often or as much as HighwayHash does.
 *
 * This isn't as tight as XXH3_accumulate, but still written in SIMD to avoid
 * extraction.
 *
 * Both XXH3_64bits and XXH3_128bits use this subroutine.
 */

XXH_FORCE_INLINE XXH_TARGET_AVX512 void
XXH3_scrambleAcc_avx512(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 63) == 0);
    XXH_STATIC_ASSERT(XXH_STRIPE_LEN == sizeof(__m512i));
    {   __m512i* const xacc = (__m512i*) acc;
        const __m512i prime32 = _mm512_set1_epi32((int)XXH_PRIME32_1);

        /* xacc[0] ^= (xacc[0] >> 47) */
        __m512i const acc_vec     = *xacc;
        __m512i const shifted     = _mm512_srli_epi64    (acc_vec, 47);
        /* xacc[0] ^= secret; */
        __m512i const key_vec     = _mm512_loadu_si512   (secret);
        __m512i const data_key    = _mm512_ternarylogic_epi32(key_vec, acc_vec, shifted, 0x96 /* key_vec ^ acc_vec ^ shifted */);

        /* xacc[0] *= XXH_PRIME32_1; */
        __m512i const data_key_hi = _mm512_srli_epi64 (data_key, 32);
        __m512i const prod_lo     = _mm512_mul_epu32     (data_key, prime32);
        __m512i const prod_hi     = _mm512_mul_epu32     (data_key_hi, prime32);
        *xacc = _mm512_add_epi64(prod_lo, _mm512_slli_epi64(prod_hi, 32));
    }
}

XXH_FORCE_INLINE XXH_TARGET_AVX512 void
XXH3_initCustomSecret_avx512(void* XXH_RESTRICT customSecret, xxh_u64 seed64)
{
    XXH_STATIC_ASSERT((XXH_SECRET_DEFAULT_SIZE & 63) == 0);
    XXH_STATIC_ASSERT(XXH_SEC_ALIGN == 64);
    XXH_ASSERT(((size_t)customSecret & 63) == 0);
    (void)(&XXH_writeLE64);
    {   int const nbRounds = XXH_SECRET_DEFAULT_SIZE / sizeof(__m512i);
        __m512i const seed_pos = _mm512_set1_epi64((xxh_i64)seed64);
        __m512i const seed     = _mm512_mask_sub_epi64(seed_pos, 0xAA, _mm512_set1_epi8(0), seed_pos);

        const __m512i* const src  = (const __m512i*) ((const void*) XXH3_kSecret);
              __m512i* const dest = (      __m512i*) customSecret;
        int i;
        XXH_ASSERT(((size_t)src & 63) == 0); /* control alignment */
        XXH_ASSERT(((size_t)dest & 63) == 0);
        for (i=0; i < nbRounds; ++i) {
            dest[i] = _mm512_add_epi64(_mm512_load_si512(src + i), seed);
    }   }
}

#endif

#if (XXH_VECTOR == XXH_AVX2) \
    || (defined(XXH_DISPATCH_AVX2) && XXH_DISPATCH_AVX2 != 0)

#ifndef XXH_TARGET_AVX2
# define XXH_TARGET_AVX2  /* disable attribute target */
#endif

XXH_FORCE_INLINE XXH_TARGET_AVX2 void
XXH3_accumulate_512_avx2( void* XXH_RESTRICT acc,
                    const void* XXH_RESTRICT input,
                    const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 31) == 0);
    {   __m256i* const xacc    =       (__m256i *) acc;
        /* Unaligned. This is mainly for pointer arithmetic, and because
         * _mm256_loadu_si256 requires  a const __m256i * pointer for some reason. */
        const         __m256i* const xinput  = (const __m256i *) input;
        /* Unaligned. This is mainly for pointer arithmetic, and because
         * _mm256_loadu_si256 requires a const __m256i * pointer for some reason. */
        const         __m256i* const xsecret = (const __m256i *) secret;

        size_t i;
        for (i=0; i < XXH_STRIPE_LEN/sizeof(__m256i); i++) {
            /* data_vec    = xinput[i]; */
            __m256i const data_vec    = _mm256_loadu_si256    (xinput+i);
            /* key_vec     = xsecret[i]; */
            __m256i const key_vec     = _mm256_loadu_si256   (xsecret+i);
            /* data_key    = data_vec ^ key_vec; */
            __m256i const data_key    = _mm256_xor_si256     (data_vec, key_vec);
            /* data_key_lo = data_key >> 32; */
            __m256i const data_key_lo = _mm256_srli_epi64 (data_key, 32);
            /* product     = (data_key & 0xffffffff) * (data_key_lo & 0xffffffff); */
            __m256i const product     = _mm256_mul_epu32     (data_key, data_key_lo);
            /* xacc[i] += swap(data_vec); */
            __m256i const data_swap = _mm256_shuffle_epi32(data_vec, _MM_SHUFFLE(1, 0, 3, 2));
            __m256i const sum       = _mm256_add_epi64(xacc[i], data_swap);
            /* xacc[i] += product; */
            xacc[i] = _mm256_add_epi64(product, sum);
    }   }
}
XXH_FORCE_INLINE XXH_TARGET_AVX2 XXH3_ACCUMULATE_TEMPLATE(avx2)

XXH_FORCE_INLINE XXH_TARGET_AVX2 void
XXH3_scrambleAcc_avx2(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 31) == 0);
    {   __m256i* const xacc = (__m256i*) acc;
        /* Unaligned. This is mainly for pointer arithmetic, and because
         * _mm256_loadu_si256 requires a const __m256i * pointer for some reason. */
        const         __m256i* const xsecret = (const __m256i *) secret;
        const __m256i prime32 = _mm256_set1_epi32((int)XXH_PRIME32_1);

        size_t i;
        for (i=0; i < XXH_STRIPE_LEN/sizeof(__m256i); i++) {
            /* xacc[i] ^= (xacc[i] >> 47) */
            __m256i const acc_vec     = xacc[i];
            __m256i const shifted     = _mm256_srli_epi64    (acc_vec, 47);
            __m256i const data_vec    = _mm256_xor_si256     (acc_vec, shifted);
            /* xacc[i] ^= xsecret; */
            __m256i const key_vec     = _mm256_loadu_si256   (xsecret+i);
            __m256i const data_key    = _mm256_xor_si256     (data_vec, key_vec);

            /* xacc[i] *= XXH_PRIME32_1; */
            __m256i const data_key_hi = _mm256_srli_epi64 (data_key, 32);
            __m256i const prod_lo     = _mm256_mul_epu32     (data_key, prime32);
            __m256i const prod_hi     = _mm256_mul_epu32     (data_key_hi, prime32);
            xacc[i] = _mm256_add_epi64(prod_lo, _mm256_slli_epi64(prod_hi, 32));
        }
    }
}

XXH_FORCE_INLINE XXH_TARGET_AVX2 void XXH3_initCustomSecret_avx2(void* XXH_RESTRICT customSecret, xxh_u64 seed64)
{
    XXH_STATIC_ASSERT((XXH_SECRET_DEFAULT_SIZE & 31) == 0);
    XXH_STATIC_ASSERT((XXH_SECRET_DEFAULT_SIZE / sizeof(__m256i)) == 6);
    XXH_STATIC_ASSERT(XXH_SEC_ALIGN <= 64);
    (void)(&XXH_writeLE64);
    XXH_PREFETCH(customSecret);
    {   __m256i const seed = _mm256_set_epi64x((xxh_i64)(0U - seed64), (xxh_i64)seed64, (xxh_i64)(0U - seed64), (xxh_i64)seed64);

        const __m256i* const src  = (const __m256i*) ((const void*) XXH3_kSecret);
              __m256i*       dest = (      __m256i*) customSecret;

#       if defined(__GNUC__) || defined(__clang__)
        /*
         * On GCC & Clang, marking 'dest' as modified will cause the compiler:
         *   - do not extract the secret from sse registers in the internal loop
         *   - use less common registers, and avoid pushing these reg into stack
         */
        XXH_COMPILER_GUARD(dest);
#       endif
        XXH_ASSERT(((size_t)src & 31) == 0); /* control alignment */
        XXH_ASSERT(((size_t)dest & 31) == 0);

        /* GCC -O2 need unroll loop manually */
        dest[0] = _mm256_add_epi64(_mm256_load_si256(src+0), seed);
        dest[1] = _mm256_add_epi64(_mm256_load_si256(src+1), seed);
        dest[2] = _mm256_add_epi64(_mm256_load_si256(src+2), seed);
        dest[3] = _mm256_add_epi64(_mm256_load_si256(src+3), seed);
        dest[4] = _mm256_add_epi64(_mm256_load_si256(src+4), seed);
        dest[5] = _mm256_add_epi64(_mm256_load_si256(src+5), seed);
    }
}

#endif

/* x86dispatch always generates SSE2 */
#if (XXH_VECTOR == XXH_SSE2) || defined(XXH_X86DISPATCH)

#ifndef XXH_TARGET_SSE2
# define XXH_TARGET_SSE2  /* disable attribute target */
#endif

XXH_FORCE_INLINE XXH_TARGET_SSE2 void
XXH3_accumulate_512_sse2( void* XXH_RESTRICT acc,
                    const void* XXH_RESTRICT input,
                    const void* XXH_RESTRICT secret)
{
    /* SSE2 is just a half-scale version of the AVX2 version. */
    XXH_ASSERT((((size_t)acc) & 15) == 0);
    {   __m128i* const xacc    =       (__m128i *) acc;
        /* Unaligned. This is mainly for pointer arithmetic, and because
         * _mm_loadu_si128 requires a const __m128i * pointer for some reason. */
        const         __m128i* const xinput  = (const __m128i *) input;
        /* Unaligned. This is mainly for pointer arithmetic, and because
         * _mm_loadu_si128 requires a const __m128i * pointer for some reason. */
        const         __m128i* const xsecret = (const __m128i *) secret;

        size_t i;
        for (i=0; i < XXH_STRIPE_LEN/sizeof(__m128i); i++) {
            /* data_vec    = xinput[i]; */
            __m128i const data_vec    = _mm_loadu_si128   (xinput+i);
            /* key_vec     = xsecret[i]; */
            __m128i const key_vec     = _mm_loadu_si128   (xsecret+i);
            /* data_key    = data_vec ^ key_vec; */
            __m128i const data_key    = _mm_xor_si128     (data_vec, key_vec);
            /* data_key_lo = data_key >> 32; */
            __m128i const data_key_lo = _mm_shuffle_epi32 (data_key, _MM_SHUFFLE(0, 3, 0, 1));
            /* product     = (data_key & 0xffffffff) * (data_key_lo & 0xffffffff); */
            __m128i const product     = _mm_mul_epu32     (data_key, data_key_lo);
            /* xacc[i] += swap(data_vec); */
            __m128i const data_swap = _mm_shuffle_epi32(data_vec, _MM_SHUFFLE(1,0,3,2));
            __m128i const sum       = _mm_add_epi64(xacc[i], data_swap);
            /* xacc[i] += product; */
            xacc[i] = _mm_add_epi64(product, sum);
    }   }
}
XXH_FORCE_INLINE XXH_TARGET_SSE2 XXH3_ACCUMULATE_TEMPLATE(sse2)

XXH_FORCE_INLINE XXH_TARGET_SSE2 void
XXH3_scrambleAcc_sse2(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 15) == 0);
    {   __m128i* const xacc = (__m128i*) acc;
        /* Unaligned. This is mainly for pointer arithmetic, and because
         * _mm_loadu_si128 requires a const __m128i * pointer for some reason. */
        const         __m128i* const xsecret = (const __m128i *) secret;
        const __m128i prime32 = _mm_set1_epi32((int)XXH_PRIME32_1);

        size_t i;
        for (i=0; i < XXH_STRIPE_LEN/sizeof(__m128i); i++) {
            /* xacc[i] ^= (xacc[i] >> 47) */
            __m128i const acc_vec     = xacc[i];
            __m128i const shifted     = _mm_srli_epi64    (acc_vec, 47);
            __m128i const data_vec    = _mm_xor_si128     (acc_vec, shifted);
            /* xacc[i] ^= xsecret[i]; */
            __m128i const key_vec     = _mm_loadu_si128   (xsecret+i);
            __m128i const data_key    = _mm_xor_si128     (data_vec, key_vec);

            /* xacc[i] *= XXH_PRIME32_1; */
            __m128i const data_key_hi = _mm_shuffle_epi32 (data_key, _MM_SHUFFLE(0, 3, 0, 1));
            __m128i const prod_lo     = _mm_mul_epu32     (data_key, prime32);
            __m128i const prod_hi     = _mm_mul_epu32     (data_key_hi, prime32);
            xacc[i] = _mm_add_epi64(prod_lo, _mm_slli_epi64(prod_hi, 32));
        }
    }
}

XXH_FORCE_INLINE XXH_TARGET_SSE2 void XXH3_initCustomSecret_sse2(void* XXH_RESTRICT customSecret, xxh_u64 seed64)
{
    XXH_STATIC_ASSERT((XXH_SECRET_DEFAULT_SIZE & 15) == 0);
    (void)(&XXH_writeLE64);
    {   int const nbRounds = XXH_SECRET_DEFAULT_SIZE / sizeof(__m128i);

#       if defined(_MSC_VER) && defined(_M_IX86) && _MSC_VER < 1900
        /* MSVC 32bit mode does not support _mm_set_epi64x before 2015 */
        XXH_ALIGN(16) const xxh_i64 seed64x2[2] = { (xxh_i64)seed64, (xxh_i64)(0U - seed64) };
        __m128i const seed = _mm_load_si128((__m128i const*)seed64x2);
#       else
        __m128i const seed = _mm_set_epi64x((xxh_i64)(0U - seed64), (xxh_i64)seed64);
#       endif
        int i;

        const void* const src16 = XXH3_kSecret;
        __m128i* dst16 = (__m128i*) customSecret;
#       if defined(__GNUC__) || defined(__clang__)
        /*
         * On GCC & Clang, marking 'dest' as modified will cause the compiler:
         *   - do not extract the secret from sse registers in the internal loop
         *   - use less common registers, and avoid pushing these reg into stack
         */
        XXH_COMPILER_GUARD(dst16);
#       endif
        XXH_ASSERT(((size_t)src16 & 15) == 0); /* control alignment */
        XXH_ASSERT(((size_t)dst16 & 15) == 0);

        for (i=0; i < nbRounds; ++i) {
            dst16[i] = _mm_add_epi64(_mm_load_si128((const __m128i *)src16+i), seed);
    }   }
}

#endif

#if (XXH_VECTOR == XXH_NEON)

/* forward declarations for the scalar routines */
XXH_FORCE_INLINE void
XXH3_scalarRound(void* XXH_RESTRICT acc, void const* XXH_RESTRICT input,
                 void const* XXH_RESTRICT secret, size_t lane);

XXH_FORCE_INLINE void
XXH3_scalarScrambleRound(void* XXH_RESTRICT acc,
                         void const* XXH_RESTRICT secret, size_t lane);

/*!
 * @internal
 * @brief The bulk processing loop for NEON and WASM SIMD128.
 *
 * The NEON code path is actually partially scalar when running on AArch64. This
 * is to optimize the pipelining and can have up to 15% speedup depending on the
 * CPU, and it also mitigates some GCC codegen issues.
 *
 * @see XXH3_NEON_LANES for configuring this and details about this optimization.
 *
 * NEON's 32-bit to 64-bit long multiply takes a half vector of 32-bit
 * integers instead of the other platforms which mask full 64-bit vectors,
 * so the setup is more complicated than just shifting right.
 *
 * Additionally, there is an optimization for 4 lanes at once noted below.
 *
 * Since, as stated, the most optimal amount of lanes for Cortexes is 6,
 * there needs to be *three* versions of the accumulate operation used
 * for the remaining 2 lanes.
 *
 * WASM's SIMD128 uses SIMDe's arm_neon.h polyfill because the intrinsics overlap
 * nearly perfectly.
 */

XXH_FORCE_INLINE void
XXH3_accumulate_512_neon( void* XXH_RESTRICT acc,
                    const void* XXH_RESTRICT input,
                    const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 15) == 0);
    XXH_STATIC_ASSERT(XXH3_NEON_LANES > 0 && XXH3_NEON_LANES <= XXH_ACC_NB && XXH3_NEON_LANES % 2 == 0);
    {   /* GCC for darwin arm64 does not like aliasing here */
        xxh_aliasing_uint64x2_t* const xacc = (xxh_aliasing_uint64x2_t*) acc;
        /* We don't use a uint32x4_t pointer because it causes bus errors on ARMv7. */
        uint8_t const* xinput = (const uint8_t *) input;
        uint8_t const* xsecret  = (const uint8_t *) secret;

        size_t i;
#ifdef __wasm_simd128__
        /*
         * On WASM SIMD128, Clang emits direct address loads when XXH3_kSecret
         * is constant propagated, which results in it converting it to this
         * inside the loop:
         *
         *    a = v128.load(XXH3_kSecret +  0 + $secret_offset, offset = 0)
         *    b = v128.load(XXH3_kSecret + 16 + $secret_offset, offset = 0)
         *    ...
         *
         * This requires a full 32-bit address immediate (and therefore a 6 byte
         * instruction) as well as an add for each offset.
         *
         * Putting an asm guard prevents it from folding (at the cost of losing
         * the alignment hint), and uses the free offset in `v128.load` instead
         * of adding secret_offset each time which overall reduces code size by
         * about a kilobyte and improves performance.
         */
        XXH_COMPILER_GUARD(xsecret);
#endif
        /* Scalar lanes use the normal scalarRound routine */
        for (i = XXH3_NEON_LANES; i < XXH_ACC_NB; i++) {
            XXH3_scalarRound(acc, input, secret, i);
        }
        i = 0;
        /* 4 NEON lanes at a time. */
        for (; i+1 < XXH3_NEON_LANES / 2; i+=2) {
            /* data_vec = xinput[i]; */
            uint64x2_t data_vec_1 = XXH_vld1q_u64(xinput  + (i * 16));
            uint64x2_t data_vec_2 = XXH_vld1q_u64(xinput  + ((i+1) * 16));
            /* key_vec  = xsecret[i];  */
            uint64x2_t key_vec_1  = XXH_vld1q_u64(xsecret + (i * 16));
            uint64x2_t key_vec_2  = XXH_vld1q_u64(xsecret + ((i+1) * 16));
            /* data_swap = swap(data_vec) */
            uint64x2_t data_swap_1 = vextq_u64(data_vec_1, data_vec_1, 1);
            uint64x2_t data_swap_2 = vextq_u64(data_vec_2, data_vec_2, 1);
            /* data_key = data_vec ^ key_vec; */
            uint64x2_t data_key_1 = veorq_u64(data_vec_1, key_vec_1);
            uint64x2_t data_key_2 = veorq_u64(data_vec_2, key_vec_2);

            /*
             * If we reinterpret the 64x2 vectors as 32x4 vectors, we can use a
             * de-interleave operation for 4 lanes in 1 step with `vuzpq_u32` to
             * get one vector with the low 32 bits of each lane, and one vector
             * with the high 32 bits of each lane.
             *
             * The intrinsic returns a double vector because the original ARMv7-a
             * instruction modified both arguments in place. AArch64 and SIMD128 emit
             * two instructions from this intrinsic.
             *
             *  [ dk11L | dk11H | dk12L | dk12H ] -> [ dk11L | dk12L | dk21L | dk22L ]
             *  [ dk21L | dk21H | dk22L | dk22H ] -> [ dk11H | dk12H | dk21H | dk22H ]
             */
            uint32x4x2_t unzipped = vuzpq_u32(
                vreinterpretq_u32_u64(data_key_1),
                vreinterpretq_u32_u64(data_key_2)
            );
            /* data_key_lo = data_key & 0xFFFFFFFF */
            uint32x4_t data_key_lo = unzipped.val[0];
            /* data_key_hi = data_key >> 32 */
            uint32x4_t data_key_hi = unzipped.val[1];
            /*
             * Then, we can split the vectors horizontally and multiply which, as for most
             * widening intrinsics, have a variant that works on both high half vectors
             * for free on AArch64. A similar instruction is available on SIMD128.
             *
             * sum = data_swap + (u64x2) data_key_lo * (u64x2) data_key_hi
             */
            uint64x2_t sum_1 = XXH_vmlal_low_u32(data_swap_1, data_key_lo, data_key_hi);
            uint64x2_t sum_2 = XXH_vmlal_high_u32(data_swap_2, data_key_lo, data_key_hi);
            /*
             * Clang reorders
             *    a += b * c;     // umlal   swap.2d, dkl.2s, dkh.2s
             *    c += a;         // add     acc.2d, acc.2d, swap.2d
             * to
             *    c += a;         // add     acc.2d, acc.2d, swap.2d
             *    c += b * c;     // umlal   acc.2d, dkl.2s, dkh.2s
             *
             * While it would make sense in theory since the addition is faster,
             * for reasons likely related to umlal being limited to certain NEON
             * pipelines, this is worse. A compiler guard fixes this.
             */
            XXH_COMPILER_GUARD_CLANG_NEON(sum_1);
            XXH_COMPILER_GUARD_CLANG_NEON(sum_2);
            /* xacc[i] = acc_vec + sum; */
            xacc[i]   = vaddq_u64(xacc[i], sum_1);
            xacc[i+1] = vaddq_u64(xacc[i+1], sum_2);
        }
        /* Operate on the remaining NEON lanes 2 at a time. */
        for (; i < XXH3_NEON_LANES / 2; i++) {
            /* data_vec = xinput[i]; */
            uint64x2_t data_vec = XXH_vld1q_u64(xinput  + (i * 16));
            /* key_vec  = xsecret[i];  */
            uint64x2_t key_vec  = XXH_vld1q_u64(xsecret + (i * 16));
            /* acc_vec_2 = swap(data_vec) */
            uint64x2_t data_swap = vextq_u64(data_vec, data_vec, 1);
            /* data_key = data_vec ^ key_vec; */
            uint64x2_t data_key = veorq_u64(data_vec, key_vec);
            /* For two lanes, just use VMOVN and VSHRN. */
            /* data_key_lo = data_key & 0xFFFFFFFF; */
            uint32x2_t data_key_lo = vmovn_u64(data_key);
            /* data_key_hi = data_key >> 32; */
            uint32x2_t data_key_hi = vshrn_n_u64(data_key, 32);
            /* sum = data_swap + (u64x2) data_key_lo * (u64x2) data_key_hi; */
            uint64x2_t sum = vmlal_u32(data_swap, data_key_lo, data_key_hi);
            /* Same Clang workaround as before */
            XXH_COMPILER_GUARD_CLANG_NEON(sum);
            /* xacc[i] = acc_vec + sum; */
            xacc[i] = vaddq_u64 (xacc[i], sum);
        }
    }
}
XXH_FORCE_INLINE XXH3_ACCUMULATE_TEMPLATE(neon)

XXH_FORCE_INLINE void
XXH3_scrambleAcc_neon(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 15) == 0);

    {   xxh_aliasing_uint64x2_t* xacc       = (xxh_aliasing_uint64x2_t*) acc;
        uint8_t const* xsecret = (uint8_t const*) secret;

        size_t i;
        /* WASM uses operator overloads and doesn't need these. */
#ifndef __wasm_simd128__
        /* { prime32_1, prime32_1 } */
        uint32x2_t const kPrimeLo = vdup_n_u32(XXH_PRIME32_1);
        /* { 0, prime32_1, 0, prime32_1 } */
        uint32x4_t const kPrimeHi = vreinterpretq_u32_u64(vdupq_n_u64((xxh_u64)XXH_PRIME32_1 << 32));
#endif

        /* AArch64 uses both scalar and neon at the same time */
        for (i = XXH3_NEON_LANES; i < XXH_ACC_NB; i++) {
            XXH3_scalarScrambleRound(acc, secret, i);
        }
        for (i=0; i < XXH3_NEON_LANES / 2; i++) {
            /* xacc[i] ^= (xacc[i] >> 47); */
            uint64x2_t acc_vec  = xacc[i];
            uint64x2_t shifted  = vshrq_n_u64(acc_vec, 47);
            uint64x2_t data_vec = veorq_u64(acc_vec, shifted);

            /* xacc[i] ^= xsecret[i]; */
            uint64x2_t key_vec  = XXH_vld1q_u64(xsecret + (i * 16));
            uint64x2_t data_key = veorq_u64(data_vec, key_vec);
            /* xacc[i] *= XXH_PRIME32_1 */
#ifdef __wasm_simd128__
            /* SIMD128 has multiply by u64x2, use it instead of expanding and scalarizing */
            xacc[i] = data_key * XXH_PRIME32_1;
#else
            /*
             * Expanded version with portable NEON intrinsics
             *
             *    lo(x) * lo(y) + (hi(x) * lo(y) << 32)
             *
             * prod_hi = hi(data_key) * lo(prime) << 32
             *
             * Since we only need 32 bits of this multiply a trick can be used, reinterpreting the vector
             * as a uint32x4_t and multiplying by { 0, prime, 0, prime } to cancel out the unwanted bits
             * and avoid the shift.
             */
            uint32x4_t prod_hi = vmulq_u32 (vreinterpretq_u32_u64(data_key), kPrimeHi);
            /* Extract low bits for vmlal_u32  */
            uint32x2_t data_key_lo = vmovn_u64(data_key);
            /* xacc[i] = prod_hi + lo(data_key) * XXH_PRIME32_1; */
            xacc[i] = vmlal_u32(vreinterpretq_u64_u32(prod_hi), data_key_lo, kPrimeLo);
#endif
        }
    }
}
#endif

#if (XXH_VECTOR == XXH_VSX)

XXH_FORCE_INLINE void
XXH3_accumulate_512_vsx(  void* XXH_RESTRICT acc,
                    const void* XXH_RESTRICT input,
                    const void* XXH_RESTRICT secret)
{
    /* presumed aligned */
    xxh_aliasing_u64x2* const xacc = (xxh_aliasing_u64x2*) acc;
    xxh_u8 const* const xinput   = (xxh_u8 const*) input;   /* no alignment restriction */
    xxh_u8 const* const xsecret  = (xxh_u8 const*) secret;    /* no alignment restriction */
    xxh_u64x2 const v32 = { 32, 32 };
    size_t i;
    for (i = 0; i < XXH_STRIPE_LEN / sizeof(xxh_u64x2); i++) {
        /* data_vec = xinput[i]; */
        xxh_u64x2 const data_vec = XXH_vec_loadu(xinput + 16*i);
        /* key_vec = xsecret[i]; */
        xxh_u64x2 const key_vec  = XXH_vec_loadu(xsecret + 16*i);
        xxh_u64x2 const data_key = data_vec ^ key_vec;
        /* shuffled = (data_key << 32) | (data_key >> 32); */
        xxh_u32x4 const shuffled = (xxh_u32x4)vec_rl(data_key, v32);
        /* product = ((xxh_u64x2)data_key & 0xFFFFFFFF) * ((xxh_u64x2)shuffled & 0xFFFFFFFF); */
        xxh_u64x2 const product  = XXH_vec_mulo((xxh_u32x4)data_key, shuffled);
        /* acc_vec = xacc[i]; */
        xxh_u64x2 acc_vec        = xacc[i];
        acc_vec += product;

        /* swap high and low halves */
#ifdef __s390x__
        acc_vec += vec_permi(data_vec, data_vec, 2);
#else
        acc_vec += vec_xxpermdi(data_vec, data_vec, 2);
#endif
        xacc[i] = acc_vec;
    }
}
XXH_FORCE_INLINE XXH3_ACCUMULATE_TEMPLATE(vsx)

XXH_FORCE_INLINE void
XXH3_scrambleAcc_vsx(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 15) == 0);

    {   xxh_aliasing_u64x2* const xacc = (xxh_aliasing_u64x2*) acc;
        const xxh_u8* const xsecret = (const xxh_u8*) secret;
        /* constants */
        xxh_u64x2 const v32  = { 32, 32 };
        xxh_u64x2 const v47 = { 47, 47 };
        xxh_u32x4 const prime = { XXH_PRIME32_1, XXH_PRIME32_1, XXH_PRIME32_1, XXH_PRIME32_1 };
        size_t i;
        for (i = 0; i < XXH_STRIPE_LEN / sizeof(xxh_u64x2); i++) {
            /* xacc[i] ^= (xacc[i] >> 47); */
            xxh_u64x2 const acc_vec  = xacc[i];
            xxh_u64x2 const data_vec = acc_vec ^ (acc_vec >> v47);

            /* xacc[i] ^= xsecret[i]; */
            xxh_u64x2 const key_vec  = XXH_vec_loadu(xsecret + 16*i);
            xxh_u64x2 const data_key = data_vec ^ key_vec;

            /* xacc[i] *= XXH_PRIME32_1 */
            /* prod_lo = ((xxh_u64x2)data_key & 0xFFFFFFFF) * ((xxh_u64x2)prime & 0xFFFFFFFF);  */
            xxh_u64x2 const prod_even  = XXH_vec_mule((xxh_u32x4)data_key, prime);
            /* prod_hi = ((xxh_u64x2)data_key >> 32) * ((xxh_u64x2)prime >> 32);  */
            xxh_u64x2 const prod_odd  = XXH_vec_mulo((xxh_u32x4)data_key, prime);
            xacc[i] = prod_odd + (prod_even << v32);
    }   }
}

#endif

#if (XXH_VECTOR == XXH_SVE)

XXH_FORCE_INLINE void
XXH3_accumulate_512_sve( void* XXH_RESTRICT acc,
                   const void* XXH_RESTRICT input,
                   const void* XXH_RESTRICT secret)
{
    uint64_t *xacc = (uint64_t *)acc;
    const uint64_t *xinput = (const uint64_t *)(const void *)input;
    const uint64_t *xsecret = (const uint64_t *)(const void *)secret;
    svuint64_t kSwap = sveor_n_u64_z(svptrue_b64(), svindex_u64(0, 1), 1);
    uint64_t element_count = svcntd();
    if (element_count >= 8) {
        svbool_t mask = svptrue_pat_b64(SV_VL8);
        svuint64_t vacc = svld1_u64(mask, xacc);
        ACCRND(vacc, 0);
        svst1_u64(mask, xacc, vacc);
    } else if (element_count == 2) {   /* sve128 */
        svbool_t mask = svptrue_pat_b64(SV_VL2);
        svuint64_t acc0 = svld1_u64(mask, xacc + 0);
        svuint64_t acc1 = svld1_u64(mask, xacc + 2);
        svuint64_t acc2 = svld1_u64(mask, xacc + 4);
        svuint64_t acc3 = svld1_u64(mask, xacc + 6);
        ACCRND(acc0, 0);
        ACCRND(acc1, 2);
        ACCRND(acc2, 4);
        ACCRND(acc3, 6);
        svst1_u64(mask, xacc + 0, acc0);
        svst1_u64(mask, xacc + 2, acc1);
        svst1_u64(mask, xacc + 4, acc2);
        svst1_u64(mask, xacc + 6, acc3);
    } else {
        svbool_t mask = svptrue_pat_b64(SV_VL4);
        svuint64_t acc0 = svld1_u64(mask, xacc + 0);
        svuint64_t acc1 = svld1_u64(mask, xacc + 4);
        ACCRND(acc0, 0);
        ACCRND(acc1, 4);
        svst1_u64(mask, xacc + 0, acc0);
        svst1_u64(mask, xacc + 4, acc1);
    }
}

XXH_FORCE_INLINE void
XXH3_accumulate_sve(xxh_u64* XXH_RESTRICT acc,
               const xxh_u8* XXH_RESTRICT input,
               const xxh_u8* XXH_RESTRICT secret,
               size_t nbStripes)
{
    if (nbStripes != 0) {
        uint64_t *xacc = (uint64_t *)acc;
        const uint64_t *xinput = (const uint64_t *)(const void *)input;
        const uint64_t *xsecret = (const uint64_t *)(const void *)secret;
        svuint64_t kSwap = sveor_n_u64_z(svptrue_b64(), svindex_u64(0, 1), 1);
        uint64_t element_count = svcntd();
        if (element_count >= 8) {
            svbool_t mask = svptrue_pat_b64(SV_VL8);
            svuint64_t vacc = svld1_u64(mask, xacc + 0);
            do {
                /* svprfd(svbool_t, void *, enum svfprop); */
                svprfd(mask, xinput + 128, SV_PLDL1STRM);
                ACCRND(vacc, 0);
                xinput += 8;
                xsecret += 1;
                nbStripes--;
           } while (nbStripes != 0);

           svst1_u64(mask, xacc + 0, vacc);
        } else if (element_count == 2) { /* sve128 */
            svbool_t mask = svptrue_pat_b64(SV_VL2);
            svuint64_t acc0 = svld1_u64(mask, xacc + 0);
            svuint64_t acc1 = svld1_u64(mask, xacc + 2);
            svuint64_t acc2 = svld1_u64(mask, xacc + 4);
            svuint64_t acc3 = svld1_u64(mask, xacc + 6);
            do {
                svprfd(mask, xinput + 128, SV_PLDL1STRM);
                ACCRND(acc0, 0);
                ACCRND(acc1, 2);
                ACCRND(acc2, 4);
                ACCRND(acc3, 6);
                xinput += 8;
                xsecret += 1;
                nbStripes--;
           } while (nbStripes != 0);

           svst1_u64(mask, xacc + 0, acc0);
           svst1_u64(mask, xacc + 2, acc1);
           svst1_u64(mask, xacc + 4, acc2);
           svst1_u64(mask, xacc + 6, acc3);
        } else {
            svbool_t mask = svptrue_pat_b64(SV_VL4);
            svuint64_t acc0 = svld1_u64(mask, xacc + 0);
            svuint64_t acc1 = svld1_u64(mask, xacc + 4);
            do {
                svprfd(mask, xinput + 128, SV_PLDL1STRM);
                ACCRND(acc0, 0);
                ACCRND(acc1, 4);
                xinput += 8;
                xsecret += 1;
                nbStripes--;
           } while (nbStripes != 0);

           svst1_u64(mask, xacc + 0, acc0);
           svst1_u64(mask, xacc + 4, acc1);
       }
    }
}

#endif

#if (XXH_VECTOR == XXH_LSX)
#define _LSX_SHUFFLE(z, y, x, w) (((z) << 6) | ((y) << 4) | ((x) << 2) | (w))

XXH_FORCE_INLINE void
XXH3_accumulate_512_lsx( void* XXH_RESTRICT acc,
                    const void* XXH_RESTRICT input,
                    const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 15) == 0);
    {
        __m128i* const xacc    =       (__m128i *) acc;
        const __m128i* const xinput  = (const __m128i *) input;
        const __m128i* const xsecret = (const __m128i *) secret;

        for (size_t i = 0; i < XXH_STRIPE_LEN / sizeof(__m128i); i++) {
            /* data_vec = xinput[i]; */
            __m128i const data_vec = __lsx_vld(xinput + i, 0);
            /* key_vec = xsecret[i]; */
            __m128i const key_vec = __lsx_vld(xsecret + i, 0);
            /* data_key = data_vec ^ key_vec; */
            __m128i const data_key = __lsx_vxor_v(data_vec, key_vec);
            /* data_key_lo = data_key >> 32; */
            __m128i const data_key_lo = __lsx_vsrli_d(data_key, 32);
            // __m128i const data_key_lo = __lsx_vsrli_d(data_key, 32);
            /* product = (data_key & 0xffffffff) * (data_key_lo & 0xffffffff); */
            __m128i const product = __lsx_vmulwev_d_wu(data_key, data_key_lo);
            /* xacc[i] += swap(data_vec); */
            __m128i const data_swap = __lsx_vshuf4i_w(data_vec, _LSX_SHUFFLE(1, 0, 3, 2));
            __m128i const sum = __lsx_vadd_d(xacc[i], data_swap);
            /* xacc[i] += product; */
            xacc[i] = __lsx_vadd_d(product, sum);
        }
    }
}
XXH_FORCE_INLINE XXH3_ACCUMULATE_TEMPLATE(lsx)

XXH_FORCE_INLINE void
XXH3_scrambleAcc_lsx(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    XXH_ASSERT((((size_t)acc) & 15) == 0);
    {
        __m128i* const xacc = (__m128i*) acc;
        const __m128i* const xsecret = (const __m128i *) secret;
        const __m128i prime32 = __lsx_vreplgr2vr_w((int)XXH_PRIME32_1);

        for (size_t i = 0; i < XXH_STRIPE_LEN / sizeof(__m128i); i++) {
            /* xacc[i] ^= (xacc[i] >> 47) */
            __m128i const acc_vec = xacc[i];
            __m128i const shifted = __lsx_vsrli_d(acc_vec, 47);
            __m128i const data_vec = __lsx_vxor_v(acc_vec, shifted);
            /* xacc[i] ^= xsecret[i]; */
            __m128i const key_vec = __lsx_vld(xsecret + i, 0);
            __m128i const data_key = __lsx_vxor_v(data_vec, key_vec);

            /* xacc[i] *= XXH_PRIME32_1; */
            __m128i const data_key_hi = __lsx_vsrli_d(data_key, 32);
            __m128i const prod_lo = __lsx_vmulwev_d_wu(data_key, prime32);
            __m128i const prod_hi = __lsx_vmulwev_d_wu(data_key_hi, prime32);
            xacc[i] = __lsx_vadd_d(prod_lo, __lsx_vslli_d(prod_hi, 32));
        }
    }
}

#endif

/* scalar variants - universal */

#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
/*
 * In XXH3_scalarRound(), GCC and Clang have a similar codegen issue, where they
 * emit an excess mask and a full 64-bit multiply-add (MADD X-form).
 *
 * While this might not seem like much, as AArch64 is a 64-bit architecture, only
 * big Cortex designs have a full 64-bit multiplier.
 *
 * On the little cores, the smaller 32-bit multiplier is used, and full 64-bit
 * multiplies expand to 2-3 multiplies in microcode. This has a major penalty
 * of up to 4 latency cycles and 2 stall cycles in the multiply pipeline.
 *
 * Thankfully, AArch64 still provides the 32-bit long multiply-add (UMADDL) which does
 * not have this penalty and does the mask automatically.
 */
XXH_FORCE_INLINE xxh_u64
XXH_mult32to64_add64(xxh_u64 lhs, xxh_u64 rhs, xxh_u64 acc)
{
    xxh_u64 ret;
    /* note: %x = 64-bit register, %w = 32-bit register */
    __asm__("umaddl %x0, %w1, %w2, %x3" : "=r" (ret) : "r" (lhs), "r" (rhs), "r" (acc));
    return ret;
}
#else
XXH_FORCE_INLINE xxh_u64
XXH_mult32to64_add64(xxh_u64 lhs, xxh_u64 rhs, xxh_u64 acc)
{
    return XXH_mult32to64((xxh_u32)lhs, (xxh_u32)rhs) + acc;
}
#endif

/*!
 * @internal
 * @brief Scalar round for @ref XXH3_accumulate_512_scalar().
 *
 * This is extracted to its own function because the NEON path uses a combination
 * of NEON and scalar.
 */
XXH_FORCE_INLINE void
XXH3_scalarRound(void* XXH_RESTRICT acc,
                 void const* XXH_RESTRICT input,
                 void const* XXH_RESTRICT secret,
                 size_t lane)
{
    xxh_u64* xacc = (xxh_u64*) acc;
    xxh_u8 const* xinput  = (xxh_u8 const*) input;
    xxh_u8 const* xsecret = (xxh_u8 const*) secret;
    XXH_ASSERT(lane < XXH_ACC_NB);
    XXH_ASSERT(((size_t)acc & (XXH_ACC_ALIGN-1)) == 0);
    {
        xxh_u64 const data_val = XXH_readLE64(xinput + lane * 8);
        xxh_u64 const data_key = data_val ^ XXH_readLE64(xsecret + lane * 8);
        xacc[lane ^ 1] += data_val; /* swap adjacent lanes */
        xacc[lane] = XXH_mult32to64_add64(data_key /* & 0xFFFFFFFF */, data_key >> 32, xacc[lane]);
    }
}

/*!
 * @internal
 * @brief Processes a 64 byte block of data using the scalar path.
 */
XXH_FORCE_INLINE void
XXH3_accumulate_512_scalar(void* XXH_RESTRICT acc,
                     const void* XXH_RESTRICT input,
                     const void* XXH_RESTRICT secret)
{
    size_t i;
    /* ARM GCC refuses to unroll this loop, resulting in a 24% slowdown on ARMv6. */
#if defined(__GNUC__) && !defined(__clang__) \
  && (defined(__arm__) || defined(__thumb2__)) \
  && defined(__ARM_FEATURE_UNALIGNED) /* no unaligned access just wastes bytes */ \
  && XXH_SIZE_OPT <= 0
#  pragma GCC unroll 8
#endif
    for (i=0; i < XXH_ACC_NB; i++) {
        XXH3_scalarRound(acc, input, secret, i);
    }
}
XXH_FORCE_INLINE XXH3_ACCUMULATE_TEMPLATE(scalar)

/*!
 * @internal
 * @brief Scalar scramble step for @ref XXH3_scrambleAcc_scalar().
 *
 * This is extracted to its own function because the NEON path uses a combination
 * of NEON and scalar.
 */
XXH_FORCE_INLINE void
XXH3_scalarScrambleRound(void* XXH_RESTRICT acc,
                         void const* XXH_RESTRICT secret,
                         size_t lane)
{
    xxh_u64* const xacc = (xxh_u64*) acc;   /* presumed aligned */
    const xxh_u8* const xsecret = (const xxh_u8*) secret;   /* no alignment restriction */
    XXH_ASSERT((((size_t)acc) & (XXH_ACC_ALIGN-1)) == 0);
    XXH_ASSERT(lane < XXH_ACC_NB);
    {
        xxh_u64 const key64 = XXH_readLE64(xsecret + lane * 8);
        xxh_u64 acc64 = xacc[lane];
        acc64 = XXH_xorshift64(acc64, 47);
        acc64 ^= key64;
        acc64 *= XXH_PRIME32_1;
        xacc[lane] = acc64;
    }
}

/*!
 * @internal
 * @brief Scrambles the accumulators after a large chunk has been read
 */
XXH_FORCE_INLINE void
XXH3_scrambleAcc_scalar(void* XXH_RESTRICT acc, const void* XXH_RESTRICT secret)
{
    size_t i;
    for (i=0; i < XXH_ACC_NB; i++) {
        XXH3_scalarScrambleRound(acc, secret, i);
    }
}

XXH_FORCE_INLINE void
XXH3_initCustomSecret_scalar(void* XXH_RESTRICT customSecret, xxh_u64 seed64)
{
    /*
     * We need a separate pointer for the hack below,
     * which requires a non-const pointer.
     * Any decent compiler will optimize this out otherwise.
     */
    const xxh_u8* kSecretPtr = XXH3_kSecret;
    XXH_STATIC_ASSERT((XXH_SECRET_DEFAULT_SIZE & 15) == 0);

#if defined(__GNUC__) && defined(__aarch64__)
    /*
     * UGLY HACK:
     * GCC and Clang generate a bunch of MOV/MOVK pairs for aarch64, and they are
     * placed sequentially, in order, at the top of the unrolled loop.
     *
     * While MOVK is great for generating constants (2 cycles for a 64-bit
     * constant compared to 4 cycles for LDR), it fights for bandwidth with
     * the arithmetic instructions.
     *
     *   I   L   S
     * MOVK
     * MOVK
     * MOVK
     * MOVK
     * ADD
     * SUB      STR
     *          STR
     * By forcing loads from memory (as the asm line causes the compiler to assume
     * that XXH3_kSecretPtr has been changed), the pipelines are used more
     * efficiently:
     *   I   L   S
     *      LDR
     *  ADD LDR
     *  SUB     STR
     *          STR
     *
     * See XXH3_NEON_LANES for details on the pipsline.
     *
     * XXH3_64bits_withSeed, len == 256, Snapdragon 835
     *   without hack: 2654.4 MB/s
     *   with hack:    3202.9 MB/s
     */
    XXH_COMPILER_GUARD(kSecretPtr);
#endif
    {   int const nbRounds = XXH_SECRET_DEFAULT_SIZE / 16;
        int i;
        for (i=0; i < nbRounds; i++) {
            /*
             * The asm hack causes the compiler to assume that kSecretPtr aliases with
             * customSecret, and on aarch64, this prevented LDP from merging two
             * loads together for free. Putting the loads together before the stores
             * properly generates LDP.
             */
            xxh_u64 lo = XXH_readLE64(kSecretPtr + 16*i)     + seed64;
            xxh_u64 hi = XXH_readLE64(kSecretPtr + 16*i + 8) - seed64;
            XXH_writeLE64((xxh_u8*)customSecret + 16*i,     lo);
            XXH_writeLE64((xxh_u8*)customSecret + 16*i + 8, hi);
    }   }
}


typedef void (*XXH3_f_accumulate)(xxh_u64* XXH_RESTRICT, const xxh_u8* XXH_RESTRICT, const xxh_u8* XXH_RESTRICT, size_t);
typedef void (*XXH3_f_scrambleAcc)(void* XXH_RESTRICT, const void*);
typedef void (*XXH3_f_initCustomSecret)(void* XXH_RESTRICT, xxh_u64);


#if (XXH_VECTOR == XXH_AVX512)

#define XXH3_accumulate_512 XXH3_accumulate_512_avx512
#define XXH3_accumulate     XXH3_accumulate_avx512
#define XXH3_scrambleAcc    XXH3_scrambleAcc_avx512
#define XXH3_initCustomSecret XXH3_initCustomSecret_avx512

#elif (XXH_VECTOR == XXH_AVX2)

#define XXH3_accumulate_512 XXH3_accumulate_512_avx2
#define XXH3_accumulate     XXH3_accumulate_avx2
#define XXH3_scrambleAcc    XXH3_scrambleAcc_avx2
#define XXH3_initCustomSecret XXH3_initCustomSecret_avx2

#elif (XXH_VECTOR == XXH_SSE2)

#define XXH3_accumulate_512 XXH3_accumulate_512_sse2
#define XXH3_accumulate     XXH3_accumulate_sse2
#define XXH3_scrambleAcc    XXH3_scrambleAcc_sse2
#define XXH3_initCustomSecret XXH3_initCustomSecret_sse2

#elif (XXH_VECTOR == XXH_NEON)

#define XXH3_accumulate_512 XXH3_accumulate_512_neon
#define XXH3_accumulate     XXH3_accumulate_neon
#define XXH3_scrambleAcc    XXH3_scrambleAcc_neon
#define XXH3_initCustomSecret XXH3_initCustomSecret_scalar

#elif (XXH_VECTOR == XXH_VSX)

#define XXH3_accumulate_512 XXH3_accumulate_512_vsx
#define XXH3_accumulate     XXH3_accumulate_vsx
#define XXH3_scrambleAcc    XXH3_scrambleAcc_vsx
#define XXH3_initCustomSecret XXH3_initCustomSecret_scalar

#elif (XXH_VECTOR == XXH_SVE)
#define XXH3_accumulate_512 XXH3_accumulate_512_sve
#define XXH3_accumulate     XXH3_accumulate_sve
#define XXH3_scrambleAcc    XXH3_scrambleAcc_scalar
#define XXH3_initCustomSecret XXH3_initCustomSecret_scalar

#elif (XXH_VECTOR == XXH_LSX)
#define XXH3_accumulate_512 XXH3_accumulate_512_lsx
#define XXH3_accumulate     XXH3_accumulate_lsx
#define XXH3_scrambleAcc    XXH3_scrambleAcc_lsx
#define XXH3_initCustomSecret XXH3_initCustomSecret_scalar

#else /* scalar */

#define XXH3_accumulate_512 XXH3_accumulate_512_scalar
#define XXH3_accumulate     XXH3_accumulate_scalar
#define XXH3_scrambleAcc    XXH3_scrambleAcc_scalar
#define XXH3_initCustomSecret XXH3_initCustomSecret_scalar

#endif

#if XXH_SIZE_OPT >= 1 /* don't do SIMD for initialization */
#  undef XXH3_initCustomSecret
#  define XXH3_initCustomSecret XXH3_initCustomSecret_scalar
#endif

XXH_FORCE_INLINE void
XXH3_hashLong_internal_loop(xxh_u64* XXH_RESTRICT acc,
                      const xxh_u8* XXH_RESTRICT input, size_t len,
                      const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                            XXH3_f_accumulate f_acc,
                            XXH3_f_scrambleAcc f_scramble)
{
    size_t const nbStripesPerBlock = (secretSize - XXH_STRIPE_LEN) / XXH_SECRET_CONSUME_RATE;
    size_t const block_len = XXH_STRIPE_LEN * nbStripesPerBlock;
    size_t const nb_blocks = (len - 1) / block_len;

    size_t n;

    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN);

    for (n = 0; n < nb_blocks; n++) {
        f_acc(acc, input + n*block_len, secret, nbStripesPerBlock);
        f_scramble(acc, secret + secretSize - XXH_STRIPE_LEN);
    }

    /* last partial block */
    XXH_ASSERT(len > XXH_STRIPE_LEN);
    {   size_t const nbStripes = ((len - 1) - (block_len * nb_blocks)) / XXH_STRIPE_LEN;
        XXH_ASSERT(nbStripes <= (secretSize / XXH_SECRET_CONSUME_RATE));
        f_acc(acc, input + nb_blocks*block_len, secret, nbStripes);

        /* last stripe */
        {   const xxh_u8* const p = input + len - XXH_STRIPE_LEN;
#define XXH_SECRET_LASTACC_START 7  /* not aligned on 8, last secret is different from acc & scrambler */
            XXH3_accumulate_512(acc, p, secret + secretSize - XXH_STRIPE_LEN - XXH_SECRET_LASTACC_START);
    }   }
}

XXH_FORCE_INLINE xxh_u64
XXH3_mix2Accs(const xxh_u64* XXH_RESTRICT acc, const xxh_u8* XXH_RESTRICT secret)
{
    return XXH3_mul128_fold64(
               acc[0] ^ XXH_readLE64(secret),
               acc[1] ^ XXH_readLE64(secret+8) );
}

static XXH_PUREF XXH64_hash_t
XXH3_mergeAccs(const xxh_u64* XXH_RESTRICT acc, const xxh_u8* XXH_RESTRICT secret, xxh_u64 start)
{
    xxh_u64 result64 = start;
    size_t i = 0;

    for (i = 0; i < 4; i++) {
        result64 += XXH3_mix2Accs(acc+2*i, secret + 16*i);
#if defined(__clang__)                                /* Clang */ \
    && (defined(__arm__) || defined(__thumb__))       /* ARMv7 */ \
    && (defined(__ARM_NEON) || defined(__ARM_NEON__)) /* NEON */  \
    && !defined(XXH_ENABLE_AUTOVECTORIZE)             /* Define to disable */
        /*
         * UGLY HACK:
         * Prevent autovectorization on Clang ARMv7-a. Exact same problem as
         * the one in XXH3_len_129to240_64b. Speeds up shorter keys > 240b.
         * XXH3_64bits, len == 256, Snapdragon 835:
         *   without hack: 2063.7 MB/s
         *   with hack:    2560.7 MB/s
         */
        XXH_COMPILER_GUARD(result64);
#endif
    }

    return XXH3_avalanche(result64);
}

/* do not align on 8, so that the secret is different from the accumulator */
#define XXH_SECRET_MERGEACCS_START 11

static XXH_PUREF XXH64_hash_t
XXH3_finalizeLong_64b(const xxh_u64* XXH_RESTRICT acc, const xxh_u8* XXH_RESTRICT secret, xxh_u64 len)
{
    return XXH3_mergeAccs(acc, secret + XXH_SECRET_MERGEACCS_START, len * XXH_PRIME64_1);
}

#define XXH3_INIT_ACC { XXH_PRIME32_3, XXH_PRIME64_1, XXH_PRIME64_2, XXH_PRIME64_3, \
                        XXH_PRIME64_4, XXH_PRIME32_2, XXH_PRIME64_5, XXH_PRIME32_1 }

XXH_FORCE_INLINE XXH64_hash_t
XXH3_hashLong_64b_internal(const void* XXH_RESTRICT input, size_t len,
                           const void* XXH_RESTRICT secret, size_t secretSize,
                           XXH3_f_accumulate f_acc,
                           XXH3_f_scrambleAcc f_scramble)
{
    XXH_ALIGN(XXH_ACC_ALIGN) xxh_u64 acc[XXH_ACC_NB] = XXH3_INIT_ACC;

    XXH3_hashLong_internal_loop(acc, (const xxh_u8*)input, len, (const xxh_u8*)secret, secretSize, f_acc, f_scramble);

    /* converge into final hash */
    XXH_STATIC_ASSERT(sizeof(acc) == 64);
    XXH_ASSERT(secretSize >= sizeof(acc) + XXH_SECRET_MERGEACCS_START);
    return XXH3_finalizeLong_64b(acc, (const xxh_u8*)secret, (xxh_u64)len);
}

/*
 * It's important for performance to transmit secret's size (when it's static)
 * so that the compiler can properly optimize the vectorized loop.
 * This makes a big performance difference for "medium" keys (<1 KB) when using AVX instruction set.
 * When the secret size is unknown, or on GCC 12 where the mix of NO_INLINE and FORCE_INLINE
 * breaks -Og, this is XXH_NO_INLINE.
 */
XXH3_WITH_SECRET_INLINE XXH64_hash_t
XXH3_hashLong_64b_withSecret(const void* XXH_RESTRICT input, size_t len,
                             XXH64_hash_t seed64, const xxh_u8* XXH_RESTRICT secret, size_t secretLen)
{
    (void)seed64;
    return XXH3_hashLong_64b_internal(input, len, secret, secretLen, XXH3_accumulate, XXH3_scrambleAcc);
}

/*
 * It's preferable for performance that XXH3_hashLong is not inlined,
 * as it results in a smaller function for small data, easier to the instruction cache.
 * Note that inside this no_inline function, we do inline the internal loop,
 * and provide a statically defined secret size to allow optimization of vector loop.
 */
XXH_NO_INLINE XXH_PUREF XXH64_hash_t
XXH3_hashLong_64b_default(const void* XXH_RESTRICT input, size_t len,
                          XXH64_hash_t seed64, const xxh_u8* XXH_RESTRICT secret, size_t secretLen)
{
    (void)seed64; (void)secret; (void)secretLen;
    return XXH3_hashLong_64b_internal(input, len, XXH3_kSecret, sizeof(XXH3_kSecret), XXH3_accumulate, XXH3_scrambleAcc);
}

/*
 * XXH3_hashLong_64b_withSeed():
 * Generate a custom key based on alteration of default XXH3_kSecret with the seed,
 * and then use this key for long mode hashing.
 *
 * This operation is decently fast but nonetheless costs a little bit of time.
 * Try to avoid it whenever possible (typically when seed==0).
 *
 * It's important for performance that XXH3_hashLong is not inlined. Not sure
 * why (uop cache maybe?), but the difference is large and easily measurable.
 */
XXH_FORCE_INLINE XXH64_hash_t
XXH3_hashLong_64b_withSeed_internal(const void* input, size_t len,
                                    XXH64_hash_t seed,
                                    XXH3_f_accumulate f_acc,
                                    XXH3_f_scrambleAcc f_scramble,
                                    XXH3_f_initCustomSecret f_initSec)
{
#if XXH_SIZE_OPT <= 0
    if (seed == 0)
        return XXH3_hashLong_64b_internal(input, len,
                                          XXH3_kSecret, sizeof(XXH3_kSecret),
                                          f_acc, f_scramble);
#endif
    {   XXH_ALIGN(XXH_SEC_ALIGN) xxh_u8 secret[XXH_SECRET_DEFAULT_SIZE];
        f_initSec(secret, seed);
        return XXH3_hashLong_64b_internal(input, len, secret, sizeof(secret),
                                          f_acc, f_scramble);
    }
}

/*
 * It's important for performance that XXH3_hashLong is not inlined.
 */
XXH_NO_INLINE XXH64_hash_t
XXH3_hashLong_64b_withSeed(const void* XXH_RESTRICT input, size_t len,
                           XXH64_hash_t seed, const xxh_u8* XXH_RESTRICT secret, size_t secretLen)
{
    (void)secret; (void)secretLen;
    return XXH3_hashLong_64b_withSeed_internal(input, len, seed,
                XXH3_accumulate, XXH3_scrambleAcc, XXH3_initCustomSecret);
}


typedef XXH64_hash_t (*XXH3_hashLong64_f)(const void* XXH_RESTRICT, size_t,
                                          XXH64_hash_t, const xxh_u8* XXH_RESTRICT, size_t);

XXH_FORCE_INLINE XXH64_hash_t
XXH3_64bits_internal(const void* XXH_RESTRICT input, size_t len,
                     XXH64_hash_t seed64, const void* XXH_RESTRICT secret, size_t secretLen,
                     XXH3_hashLong64_f f_hashLong)
{
    XXH_ASSERT(secretLen >= XXH3_SECRET_SIZE_MIN);
    /*
     * If an action is to be taken if `secretLen` condition is not respected,
     * it should be done here.
     * For now, it's a contract pre-condition.
     * Adding a check and a branch here would cost performance at every hash.
     * Also, note that function signature doesn't offer room to return an error.
     */
    if (len <= 16)
        return XXH3_len_0to16_64b((const xxh_u8*)input, len, (const xxh_u8*)secret, seed64);
    if (len <= 128)
        return XXH3_len_17to128_64b((const xxh_u8*)input, len, (const xxh_u8*)secret, secretLen, seed64);
    if (len <= XXH3_MIDSIZE_MAX)
        return XXH3_len_129to240_64b((const xxh_u8*)input, len, (const xxh_u8*)secret, secretLen, seed64);
    return f_hashLong(input, len, seed64, (const xxh_u8*)secret, secretLen);
}


/* ===   Public entry point   === */

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH64_hash_t XXH3_64bits(XXH_NOESCAPE const void* input, size_t length)
{
    return XXH3_64bits_internal(input, length, 0, XXH3_kSecret, sizeof(XXH3_kSecret), XXH3_hashLong_64b_default);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH64_hash_t
XXH3_64bits_withSecret(XXH_NOESCAPE const void* input, size_t length, XXH_NOESCAPE const void* secret, size_t secretSize)
{
    return XXH3_64bits_internal(input, length, 0, secret, secretSize, XXH3_hashLong_64b_withSecret);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH64_hash_t
XXH3_64bits_withSeed(XXH_NOESCAPE const void* input, size_t length, XXH64_hash_t seed)
{
    return XXH3_64bits_internal(input, length, seed, XXH3_kSecret, sizeof(XXH3_kSecret), XXH3_hashLong_64b_withSeed);
}

XXH_PUBLIC_API XXH64_hash_t
XXH3_64bits_withSecretandSeed(XXH_NOESCAPE const void* input, size_t length, XXH_NOESCAPE const void* secret, size_t secretSize, XXH64_hash_t seed)
{
    if (length <= XXH3_MIDSIZE_MAX)
        return XXH3_64bits_internal(input, length, seed, XXH3_kSecret, sizeof(XXH3_kSecret), NULL);
    return XXH3_hashLong_64b_withSecret(input, length, seed, (const xxh_u8*)secret, secretSize);
}


/* ===   XXH3 streaming   === */
#ifndef XXH_NO_STREAM
/*
 * Malloc's a pointer that is always aligned to @align.
 *
 * This must be freed with `XXH_alignedFree()`.
 *
 * malloc typically guarantees 16 byte alignment on 64-bit systems and 8 byte
 * alignment on 32-bit. This isn't enough for the 32 byte aligned loads in AVX2
 * or on 32-bit, the 16 byte aligned loads in SSE2 and NEON.
 *
 * This underalignment previously caused a rather obvious crash which went
 * completely unnoticed due to XXH3_createState() not actually being tested.
 * Credit to RedSpah for noticing this bug.
 *
 * The alignment is done manually: Functions like posix_memalign or _mm_malloc
 * are avoided: To maintain portability, we would have to write a fallback
 * like this anyways, and besides, testing for the existence of library
 * functions without relying on external build tools is impossible.
 *
 * The method is simple: Overallocate, manually align, and store the offset
 * to the original behind the returned pointer.
 *
 * Align must be a power of 2 and 8 <= align <= 128.
 */
static XXH_MALLOCF void* XXH_alignedMalloc(size_t s, size_t align)
{
    XXH_ASSERT(align <= 128 && align >= 8); /* range check */
    XXH_ASSERT((align & (align-1)) == 0);   /* power of 2 */
    XXH_ASSERT(s != 0 && s < (s + align));  /* empty/overflow */
    {   /* Overallocate to make room for manual realignment and an offset byte */
        xxh_u8* base = (xxh_u8*)XXH_malloc(s + align);
        if (base != NULL) {
            /*
             * Get the offset needed to align this pointer.
             *
             * Even if the returned pointer is aligned, there will always be
             * at least one byte to store the offset to the original pointer.
             */
            size_t offset = align - ((size_t)base & (align - 1)); /* base % align */
            /* Add the offset for the now-aligned pointer */
            xxh_u8* ptr = base + offset;

            XXH_ASSERT((size_t)ptr % align == 0);

            /* Store the offset immediately before the returned pointer. */
            ptr[-1] = (xxh_u8)offset;
            return ptr;
        }
        return NULL;
    }
}
/*
 * Frees an aligned pointer allocated by XXH_alignedMalloc(). Don't pass
 * normal malloc'd pointers, XXH_alignedMalloc has a specific data layout.
 */
static void XXH_alignedFree(void* p)
{
    if (p != NULL) {
        xxh_u8* ptr = (xxh_u8*)p;
        /* Get the offset byte we added in XXH_malloc. */
        xxh_u8 offset = ptr[-1];
        /* Free the original malloc'd pointer */
        xxh_u8* base = ptr - offset;
        XXH_free(base);
    }
}
/*! @ingroup XXH3_family */
/*!
 * @brief Allocate an @ref XXH3_state_t.
 *
 * @return An allocated pointer of @ref XXH3_state_t on success.
 * @return `NULL` on failure.
 *
 * @note Must be freed with XXH3_freeState().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH3_state_t* XXH3_createState(void)
{
    XXH3_state_t* const state = (XXH3_state_t*)XXH_alignedMalloc(sizeof(XXH3_state_t), 64);
    if (state==NULL) return NULL;
    XXH3_INITSTATE(state);
    return state;
}

/*! @ingroup XXH3_family */
/*!
 * @brief Frees an @ref XXH3_state_t.
 *
 * @param statePtr A pointer to an @ref XXH3_state_t allocated with @ref XXH3_createState().
 *
 * @return @ref XXH_OK.
 *
 * @note Must be allocated with XXH3_createState().
 *
 * @see @ref streaming_example "Streaming Example"
 */
XXH_PUBLIC_API XXH_errorcode XXH3_freeState(XXH3_state_t* statePtr)
{
    XXH_alignedFree(statePtr);
    return XXH_OK;
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API void
XXH3_copyState(XXH_NOESCAPE XXH3_state_t* dst_state, XXH_NOESCAPE const XXH3_state_t* src_state)
{
    XXH_memcpy(dst_state, src_state, sizeof(*dst_state));
}

static void
XXH3_reset_internal(XXH3_state_t* statePtr,
                    XXH64_hash_t seed,
                    const void* secret, size_t secretSize)
{
    size_t const initStart = offsetof(XXH3_state_t, bufferedSize);
    size_t const initLength = offsetof(XXH3_state_t, nbStripesPerBlock) - initStart;
    XXH_ASSERT(offsetof(XXH3_state_t, nbStripesPerBlock) > initStart);
    XXH_ASSERT(statePtr != NULL);
    /* set members from bufferedSize to nbStripesPerBlock (excluded) to 0 */
    memset((char*)statePtr + initStart, 0, initLength);
    statePtr->acc[0] = XXH_PRIME32_3;
    statePtr->acc[1] = XXH_PRIME64_1;
    statePtr->acc[2] = XXH_PRIME64_2;
    statePtr->acc[3] = XXH_PRIME64_3;
    statePtr->acc[4] = XXH_PRIME64_4;
    statePtr->acc[5] = XXH_PRIME32_2;
    statePtr->acc[6] = XXH_PRIME64_5;
    statePtr->acc[7] = XXH_PRIME32_1;
    statePtr->seed = seed;
    statePtr->useSeed = (seed != 0);
    statePtr->extSecret = (const unsigned char*)secret;
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN);
    statePtr->secretLimit = secretSize - XXH_STRIPE_LEN;
    statePtr->nbStripesPerBlock = statePtr->secretLimit / XXH_SECRET_CONSUME_RATE;
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_64bits_reset(XXH_NOESCAPE XXH3_state_t* statePtr)
{
    if (statePtr == NULL) return XXH_ERROR;
    XXH3_reset_internal(statePtr, 0, XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
    return XXH_OK;
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_64bits_reset_withSecret(XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* secret, size_t secretSize)
{
    if (statePtr == NULL) return XXH_ERROR;
    XXH3_reset_internal(statePtr, 0, secret, secretSize);
    if (secret == NULL) return XXH_ERROR;
    if (secretSize < XXH3_SECRET_SIZE_MIN) return XXH_ERROR;
    return XXH_OK;
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_64bits_reset_withSeed(XXH_NOESCAPE XXH3_state_t* statePtr, XXH64_hash_t seed)
{
    if (statePtr == NULL) return XXH_ERROR;
    if (seed==0) return XXH3_64bits_reset(statePtr);
    if ((seed != statePtr->seed) || (statePtr->extSecret != NULL))
        XXH3_initCustomSecret(statePtr->customSecret, seed);
    XXH3_reset_internal(statePtr, seed, NULL, XXH_SECRET_DEFAULT_SIZE);
    return XXH_OK;
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_64bits_reset_withSecretandSeed(XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* secret, size_t secretSize, XXH64_hash_t seed64)
{
    if (statePtr == NULL) return XXH_ERROR;
    if (secret == NULL) return XXH_ERROR;
    if (secretSize < XXH3_SECRET_SIZE_MIN) return XXH_ERROR;
    XXH3_reset_internal(statePtr, seed64, secret, secretSize);
    statePtr->useSeed = 1; /* always, even if seed64==0 */
    return XXH_OK;
}

/*!
 * @internal
 * @brief Processes a large input for XXH3_update() and XXH3_digest_long().
 *
 * Unlike XXH3_hashLong_internal_loop(), this can process data that overlaps a block.
 *
 * @param acc                Pointer to the 8 accumulator lanes
 * @param nbStripesSoFarPtr  In/out pointer to the number of leftover stripes in the block*
 * @param nbStripesPerBlock  Number of stripes in a block
 * @param input              Input pointer
 * @param nbStripes          Number of stripes to process
 * @param secret             Secret pointer
 * @param secretLimit        Offset of the last block in @p secret
 * @param f_acc              Pointer to an XXH3_accumulate implementation
 * @param f_scramble         Pointer to an XXH3_scrambleAcc implementation
 * @return                   Pointer past the end of @p input after processing
 */
XXH_FORCE_INLINE const xxh_u8 *
XXH3_consumeStripes(xxh_u64* XXH_RESTRICT acc,
                    size_t* XXH_RESTRICT nbStripesSoFarPtr, size_t nbStripesPerBlock,
                    const xxh_u8* XXH_RESTRICT input, size_t nbStripes,
                    const xxh_u8* XXH_RESTRICT secret, size_t secretLimit,
                    XXH3_f_accumulate f_acc,
                    XXH3_f_scrambleAcc f_scramble)
{
    const xxh_u8* initialSecret = secret + *nbStripesSoFarPtr * XXH_SECRET_CONSUME_RATE;
    /* Process full blocks */
    if (nbStripes >= (nbStripesPerBlock - *nbStripesSoFarPtr)) {
        /* Process the initial partial block... */
        size_t nbStripesThisIter = nbStripesPerBlock - *nbStripesSoFarPtr;

        do {
            /* Accumulate and scramble */
            f_acc(acc, input, initialSecret, nbStripesThisIter);
            f_scramble(acc, secret + secretLimit);
            input += nbStripesThisIter * XXH_STRIPE_LEN;
            nbStripes -= nbStripesThisIter;
            /* Then continue the loop with the full block size */
            nbStripesThisIter = nbStripesPerBlock;
            initialSecret = secret;
        } while (nbStripes >= nbStripesPerBlock);
        *nbStripesSoFarPtr = 0;
    }
    /* Process a partial block */
    if (nbStripes > 0) {
        f_acc(acc, input, initialSecret, nbStripes);
        input += nbStripes * XXH_STRIPE_LEN;
        *nbStripesSoFarPtr += nbStripes;
    }
    /* Return end pointer */
    return input;
}

#ifndef XXH3_STREAM_USE_STACK
# if XXH_SIZE_OPT <= 0 && !defined(__clang__) /* clang doesn't need additional stack space */
#   define XXH3_STREAM_USE_STACK 1
# endif
#endif
/*
 * Both XXH3_64bits_update and XXH3_128bits_update use this routine.
 */
XXH_FORCE_INLINE XXH_errorcode
XXH3_update(XXH3_state_t* XXH_RESTRICT const state,
            const xxh_u8* XXH_RESTRICT input, size_t len,
            XXH3_f_accumulate f_acc,
            XXH3_f_scrambleAcc f_scramble)
{
    if (input==NULL) {
        XXH_ASSERT(len == 0);
        return XXH_OK;
    }

    XXH_ASSERT(state != NULL);
    {   const xxh_u8* const bEnd = input + len;
        const unsigned char* const secret = (state->extSecret == NULL) ? state->customSecret : state->extSecret;
#if defined(XXH3_STREAM_USE_STACK) && XXH3_STREAM_USE_STACK >= 1
        /* For some reason, gcc and MSVC seem to suffer greatly
         * when operating accumulators directly into state.
         * Operating into stack space seems to enable proper optimization.
         * clang, on the other hand, doesn't seem to need this trick */
        XXH_ALIGN(XXH_ACC_ALIGN) xxh_u64 acc[8];
        XXH_memcpy(acc, state->acc, sizeof(acc));
#else
        xxh_u64* XXH_RESTRICT const acc = state->acc;
#endif
        state->totalLen += len;
        XXH_ASSERT(state->bufferedSize <= XXH3_INTERNALBUFFER_SIZE);

        /* small input : just fill in tmp buffer */
        if (len <= XXH3_INTERNALBUFFER_SIZE - state->bufferedSize) {
            XXH_memcpy(state->buffer + state->bufferedSize, input, len);
            state->bufferedSize += (XXH32_hash_t)len;
            return XXH_OK;
        }

        /* total input is now > XXH3_INTERNALBUFFER_SIZE */
        #define XXH3_INTERNALBUFFER_STRIPES (XXH3_INTERNALBUFFER_SIZE / XXH_STRIPE_LEN)
        XXH_STATIC_ASSERT(XXH3_INTERNALBUFFER_SIZE % XXH_STRIPE_LEN == 0);   /* clean multiple */

        /*
         * Internal buffer is partially filled (always, except at beginning)
         * Complete it, then consume it.
         */
        if (state->bufferedSize) {
            size_t const loadSize = XXH3_INTERNALBUFFER_SIZE - state->bufferedSize;
            XXH_memcpy(state->buffer + state->bufferedSize, input, loadSize);
            input += loadSize;
            XXH3_consumeStripes(acc,
                               &state->nbStripesSoFar, state->nbStripesPerBlock,
                                state->buffer, XXH3_INTERNALBUFFER_STRIPES,
                                secret, state->secretLimit,
                                f_acc, f_scramble);
            state->bufferedSize = 0;
        }
        XXH_ASSERT(input < bEnd);
        if (bEnd - input > XXH3_INTERNALBUFFER_SIZE) {
            size_t nbStripes = (size_t)(bEnd - 1 - input) / XXH_STRIPE_LEN;
            input = XXH3_consumeStripes(acc,
                                       &state->nbStripesSoFar, state->nbStripesPerBlock,
                                       input, nbStripes,
                                       secret, state->secretLimit,
                                       f_acc, f_scramble);
            XXH_memcpy(state->buffer + sizeof(state->buffer) - XXH_STRIPE_LEN, input - XXH_STRIPE_LEN, XXH_STRIPE_LEN);

        }
        /* Some remaining input (always) : buffer it */
        XXH_ASSERT(input < bEnd);
        XXH_ASSERT(bEnd - input <= XXH3_INTERNALBUFFER_SIZE);
        XXH_ASSERT(state->bufferedSize == 0);
        XXH_memcpy(state->buffer, input, (size_t)(bEnd-input));
        state->bufferedSize = (XXH32_hash_t)(bEnd-input);
#if defined(XXH3_STREAM_USE_STACK) && XXH3_STREAM_USE_STACK >= 1
        /* save stack accumulators into state */
        XXH_memcpy(state->acc, acc, sizeof(acc));
#endif
    }

    return XXH_OK;
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_64bits_update(XXH_NOESCAPE XXH3_state_t* state, XXH_NOESCAPE const void* input, size_t len)
{
    return XXH3_update(state, (const xxh_u8*)input, len,
                       XXH3_accumulate, XXH3_scrambleAcc);
}


XXH_FORCE_INLINE void
XXH3_digest_long (XXH64_hash_t* acc,
                  const XXH3_state_t* state,
                  const unsigned char* secret)
{
    xxh_u8 lastStripe[XXH_STRIPE_LEN];
    const xxh_u8* lastStripePtr;

    /*
     * Digest on a local copy. This way, the state remains unaltered, and it can
     * continue ingesting more input afterwards.
     */
    XXH_memcpy(acc, state->acc, sizeof(state->acc));
    if (state->bufferedSize >= XXH_STRIPE_LEN) {
        /* Consume remaining stripes then point to remaining data in buffer */
        size_t const nbStripes = (state->bufferedSize - 1) / XXH_STRIPE_LEN;
        size_t nbStripesSoFar = state->nbStripesSoFar;
        XXH3_consumeStripes(acc,
                           &nbStripesSoFar, state->nbStripesPerBlock,
                            state->buffer, nbStripes,
                            secret, state->secretLimit,
                            XXH3_accumulate, XXH3_scrambleAcc);
        lastStripePtr = state->buffer + state->bufferedSize - XXH_STRIPE_LEN;
    } else {  /* bufferedSize < XXH_STRIPE_LEN */
        /* Copy to temp buffer */
        size_t const catchupSize = XXH_STRIPE_LEN - state->bufferedSize;
        XXH_ASSERT(state->bufferedSize > 0);  /* there is always some input buffered */
        XXH_memcpy(lastStripe, state->buffer + sizeof(state->buffer) - catchupSize, catchupSize);
        XXH_memcpy(lastStripe + catchupSize, state->buffer, state->bufferedSize);
        lastStripePtr = lastStripe;
    }
    /* Last stripe */
    XXH3_accumulate_512(acc,
                        lastStripePtr,
                        secret + state->secretLimit - XXH_SECRET_LASTACC_START);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH64_hash_t XXH3_64bits_digest (XXH_NOESCAPE const XXH3_state_t* state)
{
    const unsigned char* const secret = (state->extSecret == NULL) ? state->customSecret : state->extSecret;
    if (state->totalLen > XXH3_MIDSIZE_MAX) {
        XXH_ALIGN(XXH_ACC_ALIGN) XXH64_hash_t acc[XXH_ACC_NB];
        XXH3_digest_long(acc, state, secret);
        return XXH3_finalizeLong_64b(acc, secret, (xxh_u64)state->totalLen);
    }
    /* totalLen <= XXH3_MIDSIZE_MAX: digesting a short input */
    if (state->useSeed)
        return XXH3_64bits_withSeed(state->buffer, (size_t)state->totalLen, state->seed);
    return XXH3_64bits_withSecret(state->buffer, (size_t)(state->totalLen),
                                  secret, state->secretLimit + XXH_STRIPE_LEN);
}
#endif /* !XXH_NO_STREAM */


/* ==========================================
 * XXH3 128 bits (a.k.a XXH128)
 * ==========================================
 * XXH3's 128-bit variant has better mixing and strength than the 64-bit variant,
 * even without counting the significantly larger output size.
 *
 * For example, extra steps are taken to avoid the seed-dependent collisions
 * in 17-240 byte inputs (See XXH3_mix16B and XXH128_mix32B).
 *
 * This strength naturally comes at the cost of some speed, especially on short
 * lengths. Note that longer hashes are about as fast as the 64-bit version
 * due to it using only a slight modification of the 64-bit loop.
 *
 * XXH128 is also more oriented towards 64-bit machines. It is still extremely
 * fast for a _128-bit_ hash on 32-bit (it usually clears XXH64).
 */

XXH_FORCE_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_1to3_128b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    /* A doubled version of 1to3_64b with different constants. */
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(1 <= len && len <= 3);
    XXH_ASSERT(secret != NULL);
    /*
     * len = 1: combinedl = { input[0], 0x01, input[0], input[0] }
     * len = 2: combinedl = { input[1], 0x02, input[0], input[1] }
     * len = 3: combinedl = { input[2], 0x03, input[0], input[1] }
     */
    {   xxh_u8 const c1 = input[0];
        xxh_u8 const c2 = input[len >> 1];
        xxh_u8 const c3 = input[len - 1];
        xxh_u32 const combinedl = ((xxh_u32)c1 <<16) | ((xxh_u32)c2 << 24)
                                | ((xxh_u32)c3 << 0) | ((xxh_u32)len << 8);
        xxh_u32 const combinedh = XXH_rotl32(XXH_swap32(combinedl), 13);
        xxh_u64 const bitflipl = (XXH_readLE32(secret) ^ XXH_readLE32(secret+4)) + seed;
        xxh_u64 const bitfliph = (XXH_readLE32(secret+8) ^ XXH_readLE32(secret+12)) - seed;
        xxh_u64 const keyed_lo = (xxh_u64)combinedl ^ bitflipl;
        xxh_u64 const keyed_hi = (xxh_u64)combinedh ^ bitfliph;
        XXH128_hash_t h128;
        h128.low64  = XXH64_avalanche(keyed_lo);
        h128.high64 = XXH64_avalanche(keyed_hi);
        return h128;
    }
}

XXH_FORCE_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_4to8_128b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(secret != NULL);
    XXH_ASSERT(4 <= len && len <= 8);
    seed ^= (xxh_u64)XXH_swap32((xxh_u32)seed) << 32;
    {   xxh_u32 const input_lo = XXH_readLE32(input);
        xxh_u32 const input_hi = XXH_readLE32(input + len - 4);
        xxh_u64 const input_64 = input_lo + ((xxh_u64)input_hi << 32);
        xxh_u64 const bitflip = (XXH_readLE64(secret+16) ^ XXH_readLE64(secret+24)) + seed;
        xxh_u64 const keyed = input_64 ^ bitflip;

        /* Shift len to the left to ensure it is even, this avoids even multiplies. */
        XXH128_hash_t m128 = XXH_mult64to128(keyed, XXH_PRIME64_1 + (len << 2));

        m128.high64 += (m128.low64 << 1);
        m128.low64  ^= (m128.high64 >> 3);

        m128.low64   = XXH_xorshift64(m128.low64, 35);
        m128.low64  *= PRIME_MX2;
        m128.low64   = XXH_xorshift64(m128.low64, 28);
        m128.high64  = XXH3_avalanche(m128.high64);
        return m128;
    }
}

XXH_FORCE_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_9to16_128b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(input != NULL);
    XXH_ASSERT(secret != NULL);
    XXH_ASSERT(9 <= len && len <= 16);
    {   xxh_u64 const bitflipl = (XXH_readLE64(secret+32) ^ XXH_readLE64(secret+40)) - seed;
        xxh_u64 const bitfliph = (XXH_readLE64(secret+48) ^ XXH_readLE64(secret+56)) + seed;
        xxh_u64 const input_lo = XXH_readLE64(input);
        xxh_u64       input_hi = XXH_readLE64(input + len - 8);
        XXH128_hash_t m128 = XXH_mult64to128(input_lo ^ input_hi ^ bitflipl, XXH_PRIME64_1);
        /*
         * Put len in the middle of m128 to ensure that the length gets mixed to
         * both the low and high bits in the 128x64 multiply below.
         */
        m128.low64 += (xxh_u64)(len - 1) << 54;
        input_hi   ^= bitfliph;
        /*
         * Add the high 32 bits of input_hi to the high 32 bits of m128, then
         * add the long product of the low 32 bits of input_hi and XXH_PRIME32_2 to
         * the high 64 bits of m128.
         *
         * The best approach to this operation is different on 32-bit and 64-bit.
         */
        if (sizeof(void *) < sizeof(xxh_u64)) { /* 32-bit */
            /*
             * 32-bit optimized version, which is more readable.
             *
             * On 32-bit, it removes an ADC and delays a dependency between the two
             * halves of m128.high64, but it generates an extra mask on 64-bit.
             */
            m128.high64 += (input_hi & 0xFFFFFFFF00000000ULL) + XXH_mult32to64((xxh_u32)input_hi, XXH_PRIME32_2);
        } else {
            /*
             * 64-bit optimized (albeit more confusing) version.
             *
             * Uses some properties of addition and multiplication to remove the mask:
             *
             * Let:
             *    a = input_hi.lo = (input_hi & 0x00000000FFFFFFFF)
             *    b = input_hi.hi = (input_hi & 0xFFFFFFFF00000000)
             *    c = XXH_PRIME32_2
             *
             *    a + (b * c)
             * Inverse Property: x + y - x == y
             *    a + (b * (1 + c - 1))
             * Distributive Property: x * (y + z) == (x * y) + (x * z)
             *    a + (b * 1) + (b * (c - 1))
             * Identity Property: x * 1 == x
             *    a + b + (b * (c - 1))
             *
             * Substitute a, b, and c:
             *    input_hi.hi + input_hi.lo + ((xxh_u64)input_hi.lo * (XXH_PRIME32_2 - 1))
             *
             * Since input_hi.hi + input_hi.lo == input_hi, we get this:
             *    input_hi + ((xxh_u64)input_hi.lo * (XXH_PRIME32_2 - 1))
             */
            m128.high64 += input_hi + XXH_mult32to64((xxh_u32)input_hi, XXH_PRIME32_2 - 1);
        }
        /* m128 ^= XXH_swap64(m128 >> 64); */
        m128.low64  ^= XXH_swap64(m128.high64);

        {   /* 128x64 multiply: h128 = m128 * XXH_PRIME64_2; */
            XXH128_hash_t h128 = XXH_mult64to128(m128.low64, XXH_PRIME64_2);
            h128.high64 += m128.high64 * XXH_PRIME64_2;

            h128.low64   = XXH3_avalanche(h128.low64);
            h128.high64  = XXH3_avalanche(h128.high64);
            return h128;
    }   }
}

/*
 * Assumption: `secret` size is >= XXH3_SECRET_SIZE_MIN
 */
XXH_FORCE_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_0to16_128b(const xxh_u8* input, size_t len, const xxh_u8* secret, XXH64_hash_t seed)
{
    XXH_ASSERT(len <= 16);
    {   if (len > 8) return XXH3_len_9to16_128b(input, len, secret, seed);
        if (len >= 4) return XXH3_len_4to8_128b(input, len, secret, seed);
        if (len) return XXH3_len_1to3_128b(input, len, secret, seed);
        {   XXH128_hash_t h128;
            xxh_u64 const bitflipl = XXH_readLE64(secret+64) ^ XXH_readLE64(secret+72);
            xxh_u64 const bitfliph = XXH_readLE64(secret+80) ^ XXH_readLE64(secret+88);
            h128.low64 = XXH64_avalanche(seed ^ bitflipl);
            h128.high64 = XXH64_avalanche( seed ^ bitfliph);
            return h128;
    }   }
}

/*
 * A bit slower than XXH3_mix16B, but handles multiply by zero better.
 */
XXH_FORCE_INLINE XXH128_hash_t
XXH128_mix32B(XXH128_hash_t acc, const xxh_u8* input_1, const xxh_u8* input_2,
              const xxh_u8* secret, XXH64_hash_t seed)
{
    acc.low64  += XXH3_mix16B (input_1, secret+0, seed);
    acc.low64  ^= XXH_readLE64(input_2) + XXH_readLE64(input_2 + 8);
    acc.high64 += XXH3_mix16B (input_2, secret+16, seed);
    acc.high64 ^= XXH_readLE64(input_1) + XXH_readLE64(input_1 + 8);
    return acc;
}


XXH_FORCE_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_17to128_128b(const xxh_u8* XXH_RESTRICT input, size_t len,
                      const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                      XXH64_hash_t seed)
{
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(16 < len && len <= 128);

    {   XXH128_hash_t acc;
        acc.low64 = len * XXH_PRIME64_1;
        acc.high64 = 0;

#if XXH_SIZE_OPT >= 1
        {
            /* Smaller, but slightly slower. */
            unsigned int i = (unsigned int)(len - 1) / 32;
            do {
                acc = XXH128_mix32B(acc, input+16*i, input+len-16*(i+1), secret+32*i, seed);
            } while (i-- != 0);
        }
#else
        if (len > 32) {
            if (len > 64) {
                if (len > 96) {
                    acc = XXH128_mix32B(acc, input+48, input+len-64, secret+96, seed);
                }
                acc = XXH128_mix32B(acc, input+32, input+len-48, secret+64, seed);
            }
            acc = XXH128_mix32B(acc, input+16, input+len-32, secret+32, seed);
        }
        acc = XXH128_mix32B(acc, input, input+len-16, secret, seed);
#endif
        {   XXH128_hash_t h128;
            h128.low64  = acc.low64 + acc.high64;
            h128.high64 = (acc.low64    * XXH_PRIME64_1)
                        + (acc.high64   * XXH_PRIME64_4)
                        + ((len - seed) * XXH_PRIME64_2);
            h128.low64  = XXH3_avalanche(h128.low64);
            h128.high64 = (XXH64_hash_t)0 - XXH3_avalanche(h128.high64);
            return h128;
        }
    }
}

XXH_NO_INLINE XXH_PUREF XXH128_hash_t
XXH3_len_129to240_128b(const xxh_u8* XXH_RESTRICT input, size_t len,
                       const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                       XXH64_hash_t seed)
{
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN); (void)secretSize;
    XXH_ASSERT(128 < len && len <= XXH3_MIDSIZE_MAX);

    {   XXH128_hash_t acc;
        unsigned i;
        acc.low64 = len * XXH_PRIME64_1;
        acc.high64 = 0;
        /*
         *  We set as `i` as offset + 32. We do this so that unchanged
         * `len` can be used as upper bound. This reaches a sweet spot
         * where both x86 and aarch64 get simple agen and good codegen
         * for the loop.
         */
        for (i = 32; i < 160; i += 32) {
            acc = XXH128_mix32B(acc,
                                input  + i - 32,
                                input  + i - 16,
                                secret + i - 32,
                                seed);
        }
        acc.low64 = XXH3_avalanche(acc.low64);
        acc.high64 = XXH3_avalanche(acc.high64);
        /*
         * NB: `i <= len` will duplicate the last 32-bytes if
         * len % 32 was zero. This is an unfortunate necessity to keep
         * the hash result stable.
         */
        for (i=160; i <= len; i += 32) {
            acc = XXH128_mix32B(acc,
                                input + i - 32,
                                input + i - 16,
                                secret + XXH3_MIDSIZE_STARTOFFSET + i - 160,
                                seed);
        }
        /* last bytes */
        acc = XXH128_mix32B(acc,
                            input + len - 16,
                            input + len - 32,
                            secret + XXH3_SECRET_SIZE_MIN - XXH3_MIDSIZE_LASTOFFSET - 16,
                            (XXH64_hash_t)0 - seed);

        {   XXH128_hash_t h128;
            h128.low64  = acc.low64 + acc.high64;
            h128.high64 = (acc.low64    * XXH_PRIME64_1)
                        + (acc.high64   * XXH_PRIME64_4)
                        + ((len - seed) * XXH_PRIME64_2);
            h128.low64  = XXH3_avalanche(h128.low64);
            h128.high64 = (XXH64_hash_t)0 - XXH3_avalanche(h128.high64);
            return h128;
        }
    }
}

static XXH_PUREF XXH128_hash_t
XXH3_finalizeLong_128b(const xxh_u64* XXH_RESTRICT acc, const xxh_u8* XXH_RESTRICT secret, size_t secretSize, xxh_u64 len)
{
    XXH128_hash_t h128;
    h128.low64 = XXH3_finalizeLong_64b(acc, secret, len);
    h128.high64 = XXH3_mergeAccs(acc, secret + secretSize
                                             - XXH_STRIPE_LEN - XXH_SECRET_MERGEACCS_START,
                                             ~(len * XXH_PRIME64_2));
    return h128;
}

XXH_FORCE_INLINE XXH128_hash_t
XXH3_hashLong_128b_internal(const void* XXH_RESTRICT input, size_t len,
                            const xxh_u8* XXH_RESTRICT secret, size_t secretSize,
                            XXH3_f_accumulate f_acc,
                            XXH3_f_scrambleAcc f_scramble)
{
    XXH_ALIGN(XXH_ACC_ALIGN) xxh_u64 acc[XXH_ACC_NB] = XXH3_INIT_ACC;

    XXH3_hashLong_internal_loop(acc, (const xxh_u8*)input, len, secret, secretSize, f_acc, f_scramble);

    /* converge into final hash */
    XXH_STATIC_ASSERT(sizeof(acc) == 64);
    XXH_ASSERT(secretSize >= sizeof(acc) + XXH_SECRET_MERGEACCS_START);
    return XXH3_finalizeLong_128b(acc, secret, secretSize, (xxh_u64)len);
}

/*
 * It's important for performance that XXH3_hashLong() is not inlined.
 */
XXH_NO_INLINE XXH_PUREF XXH128_hash_t
XXH3_hashLong_128b_default(const void* XXH_RESTRICT input, size_t len,
                           XXH64_hash_t seed64,
                           const void* XXH_RESTRICT secret, size_t secretLen)
{
    (void)seed64; (void)secret; (void)secretLen;
    return XXH3_hashLong_128b_internal(input, len, XXH3_kSecret, sizeof(XXH3_kSecret),
                                       XXH3_accumulate, XXH3_scrambleAcc);
}

/*
 * It's important for performance to pass @p secretLen (when it's static)
 * to the compiler, so that it can properly optimize the vectorized loop.
 *
 * When the secret size is unknown, or on GCC 12 where the mix of NO_INLINE and FORCE_INLINE
 * breaks -Og, this is XXH_NO_INLINE.
 */
XXH3_WITH_SECRET_INLINE XXH128_hash_t
XXH3_hashLong_128b_withSecret(const void* XXH_RESTRICT input, size_t len,
                              XXH64_hash_t seed64,
                              const void* XXH_RESTRICT secret, size_t secretLen)
{
    (void)seed64;
    return XXH3_hashLong_128b_internal(input, len, (const xxh_u8*)secret, secretLen,
                                       XXH3_accumulate, XXH3_scrambleAcc);
}

XXH_FORCE_INLINE XXH128_hash_t
XXH3_hashLong_128b_withSeed_internal(const void* XXH_RESTRICT input, size_t len,
                                XXH64_hash_t seed64,
                                XXH3_f_accumulate f_acc,
                                XXH3_f_scrambleAcc f_scramble,
                                XXH3_f_initCustomSecret f_initSec)
{
    if (seed64 == 0)
        return XXH3_hashLong_128b_internal(input, len,
                                           XXH3_kSecret, sizeof(XXH3_kSecret),
                                           f_acc, f_scramble);
    {   XXH_ALIGN(XXH_SEC_ALIGN) xxh_u8 secret[XXH_SECRET_DEFAULT_SIZE];
        f_initSec(secret, seed64);
        return XXH3_hashLong_128b_internal(input, len, (const xxh_u8*)secret, sizeof(secret),
                                           f_acc, f_scramble);
    }
}

/*
 * It's important for performance that XXH3_hashLong is not inlined.
 */
XXH_NO_INLINE XXH128_hash_t
XXH3_hashLong_128b_withSeed(const void* input, size_t len,
                            XXH64_hash_t seed64, const void* XXH_RESTRICT secret, size_t secretLen)
{
    (void)secret; (void)secretLen;
    return XXH3_hashLong_128b_withSeed_internal(input, len, seed64,
                XXH3_accumulate, XXH3_scrambleAcc, XXH3_initCustomSecret);
}

typedef XXH128_hash_t (*XXH3_hashLong128_f)(const void* XXH_RESTRICT, size_t,
                                            XXH64_hash_t, const void* XXH_RESTRICT, size_t);

XXH_FORCE_INLINE XXH128_hash_t
XXH3_128bits_internal(const void* input, size_t len,
                      XXH64_hash_t seed64, const void* XXH_RESTRICT secret, size_t secretLen,
                      XXH3_hashLong128_f f_hl128)
{
    XXH_ASSERT(secretLen >= XXH3_SECRET_SIZE_MIN);
    /*
     * If an action is to be taken if `secret` conditions are not respected,
     * it should be done here.
     * For now, it's a contract pre-condition.
     * Adding a check and a branch here would cost performance at every hash.
     */
    if (len <= 16)
        return XXH3_len_0to16_128b((const xxh_u8*)input, len, (const xxh_u8*)secret, seed64);
    if (len <= 128)
        return XXH3_len_17to128_128b((const xxh_u8*)input, len, (const xxh_u8*)secret, secretLen, seed64);
    if (len <= XXH3_MIDSIZE_MAX)
        return XXH3_len_129to240_128b((const xxh_u8*)input, len, (const xxh_u8*)secret, secretLen, seed64);
    return f_hl128(input, len, seed64, secret, secretLen);
}


/* ===   Public XXH128 API   === */

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t XXH3_128bits(XXH_NOESCAPE const void* input, size_t len)
{
    return XXH3_128bits_internal(input, len, 0,
                                 XXH3_kSecret, sizeof(XXH3_kSecret),
                                 XXH3_hashLong_128b_default);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t
XXH3_128bits_withSecret(XXH_NOESCAPE const void* input, size_t len, XXH_NOESCAPE const void* secret, size_t secretSize)
{
    return XXH3_128bits_internal(input, len, 0,
                                 (const xxh_u8*)secret, secretSize,
                                 XXH3_hashLong_128b_withSecret);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t
XXH3_128bits_withSeed(XXH_NOESCAPE const void* input, size_t len, XXH64_hash_t seed)
{
    return XXH3_128bits_internal(input, len, seed,
                                 XXH3_kSecret, sizeof(XXH3_kSecret),
                                 XXH3_hashLong_128b_withSeed);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t
XXH3_128bits_withSecretandSeed(XXH_NOESCAPE const void* input, size_t len, XXH_NOESCAPE const void* secret, size_t secretSize, XXH64_hash_t seed)
{
    if (len <= XXH3_MIDSIZE_MAX)
        return XXH3_128bits_internal(input, len, seed, XXH3_kSecret, sizeof(XXH3_kSecret), NULL);
    return XXH3_hashLong_128b_withSecret(input, len, seed, secret, secretSize);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t
XXH128(XXH_NOESCAPE const void* input, size_t len, XXH64_hash_t seed)
{
    return XXH3_128bits_withSeed(input, len, seed);
}


/* ===   XXH3 128-bit streaming   === */
#ifndef XXH_NO_STREAM
/*
 * All initialization and update functions are identical to 64-bit streaming variant.
 * The only difference is the finalization routine.
 */

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_128bits_reset(XXH_NOESCAPE XXH3_state_t* statePtr)
{
    return XXH3_64bits_reset(statePtr);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_128bits_reset_withSecret(XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* secret, size_t secretSize)
{
    return XXH3_64bits_reset_withSecret(statePtr, secret, secretSize);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_128bits_reset_withSeed(XXH_NOESCAPE XXH3_state_t* statePtr, XXH64_hash_t seed)
{
    return XXH3_64bits_reset_withSeed(statePtr, seed);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_128bits_reset_withSecretandSeed(XXH_NOESCAPE XXH3_state_t* statePtr, XXH_NOESCAPE const void* secret, size_t secretSize, XXH64_hash_t seed)
{
    return XXH3_64bits_reset_withSecretandSeed(statePtr, secret, secretSize, seed);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_128bits_update(XXH_NOESCAPE XXH3_state_t* state, XXH_NOESCAPE const void* input, size_t len)
{
    return XXH3_64bits_update(state, input, len);
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t XXH3_128bits_digest (XXH_NOESCAPE const XXH3_state_t* state)
{
    const unsigned char* const secret = (state->extSecret == NULL) ? state->customSecret : state->extSecret;
    if (state->totalLen > XXH3_MIDSIZE_MAX) {
        XXH_ALIGN(XXH_ACC_ALIGN) XXH64_hash_t acc[XXH_ACC_NB];
        XXH3_digest_long(acc, state, secret);
        XXH_ASSERT(state->secretLimit + XXH_STRIPE_LEN >= sizeof(acc) + XXH_SECRET_MERGEACCS_START);
        return XXH3_finalizeLong_128b(acc, secret, state->secretLimit + XXH_STRIPE_LEN,  (xxh_u64)state->totalLen);
    }
    /* len <= XXH3_MIDSIZE_MAX : short code */
    if (state->useSeed)
        return XXH3_128bits_withSeed(state->buffer, (size_t)state->totalLen, state->seed);
    return XXH3_128bits_withSecret(state->buffer, (size_t)(state->totalLen),
                                   secret, state->secretLimit + XXH_STRIPE_LEN);
}
#endif /* !XXH_NO_STREAM */
/* 128-bit utility functions */

#include <string.h>   /* memcmp, memcpy */

/* return : 1 is equal, 0 if different */
/*! @ingroup XXH3_family */
XXH_PUBLIC_API int XXH128_isEqual(XXH128_hash_t h1, XXH128_hash_t h2)
{
    /* note : XXH128_hash_t is compact, it has no padding byte */
    return !(memcmp(&h1, &h2, sizeof(h1)));
}

/* This prototype is compatible with stdlib's qsort().
 * @return : >0 if *h128_1  > *h128_2
 *           <0 if *h128_1  < *h128_2
 *           =0 if *h128_1 == *h128_2  */
/*! @ingroup XXH3_family */
XXH_PUBLIC_API int XXH128_cmp(XXH_NOESCAPE const void* h128_1, XXH_NOESCAPE const void* h128_2)
{
    XXH128_hash_t const h1 = *(const XXH128_hash_t*)h128_1;
    XXH128_hash_t const h2 = *(const XXH128_hash_t*)h128_2;
    int const hcmp = (h1.high64 > h2.high64) - (h2.high64 > h1.high64);
    /* note : bets that, in most cases, hash values are different */
    if (hcmp) return hcmp;
    return (h1.low64 > h2.low64) - (h2.low64 > h1.low64);
}


/*======   Canonical representation   ======*/
/*! @ingroup XXH3_family */
XXH_PUBLIC_API void
XXH128_canonicalFromHash(XXH_NOESCAPE XXH128_canonical_t* dst, XXH128_hash_t hash)
{
    XXH_STATIC_ASSERT(sizeof(XXH128_canonical_t) == sizeof(XXH128_hash_t));
    if (XXH_CPU_LITTLE_ENDIAN) {
        hash.high64 = XXH_swap64(hash.high64);
        hash.low64  = XXH_swap64(hash.low64);
    }
    XXH_memcpy(dst, &hash.high64, sizeof(hash.high64));
    XXH_memcpy((char*)dst + sizeof(hash.high64), &hash.low64, sizeof(hash.low64));
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH128_hash_t
XXH128_hashFromCanonical(XXH_NOESCAPE const XXH128_canonical_t* src)
{
    XXH128_hash_t h;
    h.high64 = XXH_readBE64(src);
    h.low64  = XXH_readBE64(src->digest + 8);
    return h;
}



/* ==========================================
 * Secret generators
 * ==========================================
 */
#define XXH_MIN(x, y) (((x) > (y)) ? (y) : (x))

XXH_FORCE_INLINE void XXH3_combine16(void* dst, XXH128_hash_t h128)
{
    XXH_writeLE64( dst, XXH_readLE64(dst) ^ h128.low64 );
    XXH_writeLE64( (char*)dst+8, XXH_readLE64((char*)dst+8) ^ h128.high64 );
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API XXH_errorcode
XXH3_generateSecret(XXH_NOESCAPE void* secretBuffer, size_t secretSize, XXH_NOESCAPE const void* customSeed, size_t customSeedSize)
{
#if (XXH_DEBUGLEVEL >= 1)
    XXH_ASSERT(secretBuffer != NULL);
    XXH_ASSERT(secretSize >= XXH3_SECRET_SIZE_MIN);
#else
    /* production mode, assert() are disabled */
    if (secretBuffer == NULL) return XXH_ERROR;
    if (secretSize < XXH3_SECRET_SIZE_MIN) return XXH_ERROR;
#endif

    if (customSeedSize == 0) {
        customSeed = XXH3_kSecret;
        customSeedSize = XXH_SECRET_DEFAULT_SIZE;
    }
#if (XXH_DEBUGLEVEL >= 1)
    XXH_ASSERT(customSeed != NULL);
#else
    if (customSeed == NULL) return XXH_ERROR;
#endif

    /* Fill secretBuffer with a copy of customSeed - repeat as needed */
    {   size_t pos = 0;
        while (pos < secretSize) {
            size_t const toCopy = XXH_MIN((secretSize - pos), customSeedSize);
            memcpy((char*)secretBuffer + pos, customSeed, toCopy);
            pos += toCopy;
    }   }

    {   size_t const nbSeg16 = secretSize / 16;
        size_t n;
        XXH128_canonical_t scrambler;
        XXH128_canonicalFromHash(&scrambler, XXH128(customSeed, customSeedSize, 0));
        for (n=0; n<nbSeg16; n++) {
            XXH128_hash_t const h128 = XXH128(&scrambler, sizeof(scrambler), n);
            XXH3_combine16((char*)secretBuffer + n*16, h128);
        }
        /* last segment */
        XXH3_combine16((char*)secretBuffer + secretSize - 16, XXH128_hashFromCanonical(&scrambler));
    }
    return XXH_OK;
}

/*! @ingroup XXH3_family */
XXH_PUBLIC_API void
XXH3_generateSecret_fromSeed(XXH_NOESCAPE void* secretBuffer, XXH64_hash_t seed)
{
    XXH_ALIGN(XXH_SEC_ALIGN) xxh_u8 secret[XXH_SECRET_DEFAULT_SIZE];
    XXH3_initCustomSecret(secret, seed);
    XXH_ASSERT(secretBuffer != NULL);
    memcpy(secretBuffer, secret, XXH_SECRET_DEFAULT_SIZE);
}



/* Pop our optimization override from above */
#if XXH_VECTOR == XXH_AVX2 /* AVX2 */ \
  && defined(__GNUC__) && !defined(__clang__) /* GCC, not Clang */ \
  && defined(__OPTIMIZE__) && XXH_SIZE_OPT <= 0 /* respect -O0 and -Os */
#  pragma GCC pop_options
#endif

#endif  /* XXH_NO_LONG_LONG */

#endif  /* XXH_NO_XXH3 */

/*!
 * @}
 */
#endif  /* XXH_IMPLEMENTATION */


#if defined (__cplusplus)
} /* extern "C" */
#endif

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
    if (!parts->len) return "";
    size_t sep_len = strlen(sep);
    /* compute lengths in one pass */
    size_t* lens = bld_arena_alloc(parts->len * sizeof(size_t));
    size_t total = 0;
    for (size_t i = 0; i < parts->len; i++) {
        lens[i] = strlen(parts->items[i]);
        total += lens[i] + (i > 0 ? sep_len : 0);
    }
    char* buf = bld_arena_alloc(total + 1);
    size_t off = 0;
    for (size_t i = 0; i < parts->len; i++) {
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
    if (s->len >= s->cap) {
        size_t new_cap = s->cap ? s->cap * 2 : 8;
        if (s->cap == 0 && s->items && s->len > 0) {
            /* first dynamic use of a static slice — copy from compound literal */
            const char** old = s->items;
            s->items = bld_arena_alloc(new_cap * sizeof(const char*));
            memcpy(s->items, old, s->len * sizeof(const char*));
        } else {
            s->items = bld_arena_realloc(
                s->items, s->cap * sizeof(const char*), new_cap * sizeof(const char*));
        }
        s->cap = new_cap;
    }
    s->items[s->len++] = item;
}

void bld_paths_push(Bld_Paths* s, const char* item) {
    if (s->len >= s->cap) {
        size_t new_cap = s->cap ? s->cap * 2 : 8;
        if (s->cap == 0 && s->items && s->len > 0) {
            const char** old = s->items;
            s->items = bld_arena_alloc(new_cap * sizeof(const char*));
            memcpy(s->items, old, s->len * sizeof(const char*));
        } else {
            s->items = bld_arena_realloc(
                s->items, s->cap * sizeof(const char*), new_cap * sizeof(const char*));
        }
        s->cap = new_cap;
    }
    s->items[s->len++] = item;
}

Bld_Strs bld_strs_merge(Bld_Strs a, Bld_Strs b) {
    size_t total = a.len + b.len;
    if (total == 0) return (Bld_Strs){0};
    const char** r = bld_arena_alloc(total * sizeof(const char*));
    for (size_t i = 0; i < a.len; i++) r[i] = a.items[i];
    for (size_t i = 0; i < b.len; i++) r[a.len + i] = b.items[i];
    return (Bld_Strs){r, total, total};
}

Bld_Paths bld_paths_merge(Bld_Paths a, Bld_Paths b) {
    size_t total = a.len + b.len;
    if (total == 0) return (Bld_Paths){0};
    const char** r = bld_arena_alloc(total * sizeof(const char*));
    for (size_t i = 0; i < a.len; i++) r[i] = a.items[i];
    for (size_t i = 0; i < b.len; i++) r[a.len + i] = b.items[i];
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
    if (result.len > 1)
        qsort(result.items, result.len, sizeof(const char*), bld__strcmp_indirect);

    return result;
}

Bld_Paths bld_files_exclude(Bld_Paths files, Bld_Paths exclude) {
    if (!files.items || files.len == 0) return files;
    Bld_Paths result = {0};
    for (size_t i = 0; i < files.len; i++) {
        bool skip = false;
        for (size_t j = 0; j < exclude.len; j++) {
            if (strcmp(files.items[i], exclude.items[j]) == 0) { skip = true; break; }
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
    for (size_t i = 0; i < c.defines.len; i++) {
        bld_cmd_appendf(cmd, " -D");
        bld_cmd_append_sq(cmd, c.defines.items[i]);
    }

    /* include dirs from CompileFlags */
    for (size_t i = 0; i < c.include_dirs.len; i++)
        bld_cmd_appendf(cmd, " -I%s", c.include_dirs.items[i]);

    /* system include dirs from CompileFlags */
    for (size_t i = 0; i < c.sys_include_dirs.len; i++)
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
        for (size_t i = 0; i < c.obj_paths.len; i++)
            bld_cmd_appendf(cmd, " \"%s\"", c.obj_paths.items[i]);

        bld_cmd_appendf(cmd, " -o %s", c.output);
    } else {
        /* obj paths */
        for (size_t i = 0; i < c.obj_paths.len; i++)
            bld_cmd_appendf(cmd, " \"%s\"", c.obj_paths.items[i]);

        /* lib dirs */
        for (size_t i = 0; i < c.lib_dirs.len; i++)
            bld_cmd_appendf(cmd, " -L\"%s\"", c.lib_dirs.items[i]);

        /* lib names */
        for (size_t i = 0; i < c.lib_names.len; i++)
            bld_cmd_appendf(cmd, " -l%s", c.lib_names.items[i]);

        /* rpaths */
        for (size_t i = 0; i < c.rpaths.len; i++)
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
    for (size_t i = 0; i < obj_paths.len; i++)
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

/* --- bld/bld_cache.c --- */
/* bld/bld_cache.c — artifact cache, depfile tracking, content validation */


/* ---- Depfile parsing ---- */

static Bld_PathList bld__parse_depfile(Bld_Path depfile) {
    Bld_PathList deps = {0};
    size_t len;
    const char* content = bld_fs_read_file(depfile, &len);

    const char* p = content;
    const char* end = content + len;
    while (p < end && *p != ':') p++;
    if (p < end) p++;

    char* file = bld_arena_alloc(len + 1);
    size_t flen = 0;

    while (p < end) {
        char c = *p++;
        if (c == '\\' && p < end) {
            char next = *p;
            if (next == '\n') { p++; continue; }
            if (next == ' ')  { p++; file[flen++] = ' '; continue; }
            file[flen++] = c;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n') {
            if (flen > 0) {
                file[flen] = '\0';
                bld_da_push(&deps, bld_path(bld_str_dup(file)));
                flen = 0;
            }
            continue;
        }
        file[flen++] = c;
    }
    if (flen > 0) {
        file[flen] = '\0';
        bld_da_push(&deps, bld_path(bld_str_dup(file)));
    }
    return deps;
}

static Bld_Hash bld__hash_depfile(Bld_Path depfile) {
    Bld_PathList deps = bld__parse_depfile(depfile);
    Bld_Hash h = {0};
    for (size_t i = 0; i < deps.count; i++) {
        if (bld_fs_is_file(deps.items[i]))
            h = bld_hash_combine(h, bld_hash_file(deps.items[i]));
    }
    return h;
}

/* ---- Path helpers ---- */

static Bld_Path bld__cache_art(Bld* b, Bld_Hash h) {
    return bld_path_join(bld_path_join(b->cache, bld_path("arts")), bld_path_fmt("%" PRIu64, h.value));
}

static Bld_Path bld__cache_art_meta(Bld* b, Bld_Hash h) {
    return bld_path_join(bld_path_join(b->cache, bld_path("arts")), bld_path_fmt("%" PRIu64 ".meta", h.value));
}

static Bld_Path bld__depfile_path(Bld* b, Bld_Step* s) {
    size_t len = strlen(s->name);
    char* safe = bld_arena_alloc(len + 1);
    for (size_t i = 0; i < len; i++) safe[i] = (s->name[i] == '/') ? '_' : s->name[i];
    safe[len] = '\0';
    return bld_path_join(bld_path_join(b->cache, bld_path("deps")), bld_path_fmt("%s.d", safe));
}

static Bld_Hash bld__content_hash(Bld_Path art) {
    return bld_fs_is_dir(art) ? bld_hash_dir(art) : bld_hash_file(art);
}

/* ---- Validation ---- */

static int bld__cache_validate(Bld* b, Bld_Step* s) {
    Bld_Path art = bld__step_artifact(b, s);
    Bld_Path meta = bld__cache_art_meta(b, s->cache_key);
    if (!bld_fs_exists(meta)) return 0;
    size_t len;
    const char* data = bld_fs_read_file(meta, &len);
    uint64_t stored = 0;
    if (sscanf(data, "%" SCNu64, &stored) != 1) return 0;
    Bld_Hash ch = bld__content_hash(art);
    return ch.value == stored;
}

static void bld__cache_write_meta(Bld* b, Bld_Hash key, Bld_Hash content_hash) {
    Bld_Path meta = bld__cache_art_meta(b, key);
    const char* data = bld_str_fmt("%" PRIu64, content_hash.value);
    bld_fs_write_file(meta, data, strlen(data));
}

/* ---- Public interface ---- */

static uint64_t bld__tmp_counter = 0;

Bld_Path bld__step_artifact(Bld* b, Bld_Step* s) {
    return bld__cache_art(b, s->cache_key);
}

Bld_Path bld__target_artifact(Bld* b, Bld_Target* t) {
    return bld__step_artifact(b, t->exit);
}

Bld_Path bld__cache_tmp(Bld* b) {
    uint64_t id = __atomic_fetch_add(&bld__tmp_counter, 1, __ATOMIC_RELAXED);
    return bld_path_join(bld_path_join(b->cache, bld_path("tmp")), bld_path_fmt("%" PRIu64, id));
}

int bld__cache_has(Bld* b, Bld_Step* step) {
    /* compute cache_key from input_hash + depfile */
    Bld_Hash h = step->input_hash;
    if (step->has_depfile) {
        Bld_Path dep = bld__depfile_path(b, step);
        if (bld_fs_exists(dep))
            h = bld_hash_combine(h, bld__hash_depfile(dep));
    }
    step->cache_key = h;
    step->hash_valid = true;

    if (!step->action) return 1;
    if (step->phony) return 0;
    if (!bld_fs_exists(bld__step_artifact(b, step))) return 0;
    if (step->has_depfile && !bld_fs_exists(bld__depfile_path(b, step))) return 0;
    if (!bld__cache_validate(b, step)) return 0;
    return 1;
}

void bld__cache_store(Bld* b, Bld_Step* step, Bld_Path tmp_out, Bld_Path tmp_dep) {
    /* store depfile and update cache key */
    if (step->has_depfile && bld_fs_exists(tmp_dep)) {
        Bld_Path cached_dep = bld__depfile_path(b, step);
        bld_fs_mkdir_p(bld_path_parent(cached_dep));
        bld_fs_rename(tmp_dep, cached_dep);
        step->cache_key = bld_hash_combine(step->input_hash, bld__hash_depfile(cached_dep));
    }

    /* store artifact */
    Bld_Path expected = bld__step_artifact(b, step);
    if (bld_fs_exists(tmp_out)) {
        bld_fs_mkdir_p(bld_path_parent(expected));
        bld_fs_rename(tmp_out, expected);
    }
    if (!bld_fs_exists(expected)) return;

    /* content hash + early cutoff + meta */
    Bld_Hash ch = bld__content_hash(expected);
    if (step->content_hash && ch.value != step->cache_key.value) {
        step->cache_key = ch;
        Bld_Path new_art = bld__step_artifact(b, step);
        if (bld_fs_exists(new_art)) bld_fs_remove_all(new_art);
        bld_fs_mkdir_p(bld_path_parent(new_art));
        bld_fs_rename(expected, new_art);
    }
    bld__cache_write_meta(b, step->cache_key, ch);
}
/* --- bld/bld_dep.c --- */
/* bld/bld_dep.c — external dependency discovery implementation */


static Bld_Paths bld__dep_clone_paths(Bld_Paths s) {
    if (s.len == 0) return (Bld_Paths){0};
    const char** copy = bld_arena_alloc(s.len * sizeof(const char*));
    for (size_t i = 0; i < s.len; i++) copy[i] = bld_str_dup(s.items[i]);
    return (Bld_Paths){copy, s.len, 0};
}

static Bld_Strs bld__dep_clone_strs(Bld_Strs s) {
    if (s.len == 0) return (Bld_Strs){0};
    const char** copy = bld_arena_alloc(s.len * sizeof(const char*));
    for (size_t i = 0; i < s.len; i++) copy[i] = bld_str_dup(s.items[i]);
    return (Bld_Strs){copy, s.len, 0};
}

Bld_Dep* bld__dep(const Bld_Dep* d) {
    Bld_Dep* c = bld_arena_alloc(sizeof(Bld_Dep));
    *c = *d;
    c->found = true;
    if (d->name) c->name = bld_str_dup(d->name);
    c->include_dirs = bld__dep_clone_paths(d->include_dirs);
    c->system_include_dirs = bld__dep_clone_paths(d->system_include_dirs);
    c->libs = bld__dep_clone_strs(d->libs);
    c->lib_dirs = bld__dep_clone_paths(d->lib_dirs);
    if (d->extra_cflags) c->extra_cflags = bld_str_dup(d->extra_cflags);
    if (d->extra_ldflags) c->extra_ldflags = bld_str_dup(d->extra_ldflags);
    return c;
}

static void bld__parse_pkg_flags(const char* output, Bld_Dep* dep, bool is_libs) {
    if (!output || !output[0]) return;

    const char* p = output;
    while (*p) {
        while (*p == ' ' || *p == '\n') p++;
        if (!*p) break;
        const char* tok = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        size_t len = (size_t)(p - tok);
        char* s = bld_arena_alloc(len + 1);
        memcpy(s, tok, len); s[len] = '\0';

        if (is_libs) {
            if (strncmp(s, "-L", 2) == 0)      bld_paths_push(&dep->lib_dirs, bld_str_dup(s + 2));
            else if (strncmp(s, "-l", 2) == 0)  bld_strs_push(&dep->libs, bld_str_dup(s + 2));
        } else {
            if (strncmp(s, "-isystem", 8) == 0) {
                if (len > 8) bld_paths_push(&dep->system_include_dirs, bld_str_dup(s + 8));
                else {
                    while (*p == ' ') p++;
                    const char* t2 = p;
                    while (*p && *p != ' ' && *p != '\n') p++;
                    if (p > t2) {
                        char* s2 = bld_arena_alloc((size_t)(p - t2) + 1);
                        memcpy(s2, t2, (size_t)(p - t2)); s2[p - t2] = '\0';
                        bld_paths_push(&dep->system_include_dirs, s2);
                    }
                }
            }
            else if (strncmp(s, "-I", 2) == 0)  bld_paths_push(&dep->system_include_dirs, bld_str_dup(s + 2));
        }
    }
}

Bld_Dep* bld_find_pkg(const char* name) {
    Bld_Dep* dep = bld_arena_alloc(sizeof(Bld_Dep));
    memset(dep, 0, sizeof(*dep));
    dep->name = bld_str_dup(name);

    const char* check = bld_str_fmt("pkg-config --exists %s 2>/dev/null", name);
    if (system(check) != 0) {
        dep->found = false;
        return dep;
    }
    dep->found = true;

    const char* cmd_cf = bld_str_fmt("pkg-config --cflags %s 2>/dev/null", name);
    FILE* f = popen(cmd_cf, "r");
    if (f) {
        char buf[4096] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        pclose(f);
        bld__parse_pkg_flags(buf, dep, false);
    }

    const char* cmd_libs = bld_str_fmt("pkg-config --libs %s 2>/dev/null", name);
    f = popen(cmd_libs, "r");
    if (f) {
        char buf[4096] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        pclose(f);
        bld__parse_pkg_flags(buf, dep, true);
    }

    return dep;
}
/* --- bld/bld_build.c --- */
/* bld/bld_build.c — target/step construction, build graph logic */


/* ================================================================
 *  Helpers
 * ================================================================ */


/* ================================================================
 *  Clone
 * ================================================================ */

static Bld_Strs bld__clone_strs(Bld_Strs s) {
    if (s.len == 0) return (Bld_Strs){0};
    const char** copy = bld_arena_alloc(s.len * sizeof(const char*));
    for (size_t i = 0; i < s.len; i++) copy[i] = bld_str_dup(s.items[i]);
    return (Bld_Strs){copy, s.len, 0};
}

static Bld_Paths bld__clone_paths(Bld_Paths s) {
    if (s.len == 0) return (Bld_Paths){0};
    const char** copy = bld_arena_alloc(s.len * sizeof(const char*));
    for (size_t i = 0; i < s.len; i++) copy[i] = bld_str_dup(s.items[i]);
    return (Bld_Paths){copy, s.len, 0};
}

Bld_CompileFlags bld_clone_compile_flags(Bld_CompileFlags f) {
    Bld_CompileFlags c = f;
    if (f.extra_flags) c.extra_flags = bld_str_dup(f.extra_flags);
    c.defines = bld__clone_strs(f.defines);
    c.include_dirs = bld__clone_paths(f.include_dirs);
    c.system_include_dirs = bld__clone_paths(f.system_include_dirs);
    return c;
}

static Bld_LinkFlags bld_clone_link_flags(Bld_LinkFlags f) {
    Bld_LinkFlags c = f;
    if (f.extra_flags) c.extra_flags = bld_str_dup(f.extra_flags);
    c.libs = bld__clone_strs(f.libs);
    c.lib_dirs = bld__clone_paths(f.lib_dirs);
    return c;
}

/* ================================================================
 *  Build mode defaults
 * ================================================================ */

Bld_CompileFlags bld_default_compile_flags(Bld* b) {
    Bld_CompileFlags f = {0};
    switch (b->settings.mode) {
        case BLD_MODE_DEBUG:
            f.optimize = BLD_OPT_O0;
            f.debug_info = BLD_ON;
            break;
        case BLD_MODE_RELEASE: {
            f.optimize = BLD_OPT_O2;
            f.debug_info = BLD_OFF;
            f.defines = BLD_STRS("NDEBUG");
            break;
        }
        default: break;
    }
    return f;
}

/* ================================================================
 *  Per-file overrides
 * ================================================================ */

void bld__override_file(Bld_Target* t, const char* file, const Bld_CompileFlags* flags) {
    Bld_FileOverride ov;
    ov.file = bld_str_dup(file);
    ov.flags = bld_clone_compile_flags(*flags);
    bld_da_push(&t->file_overrides, ov);
}

/* bld__dep, bld_find_pkg — implemented in bld_dep.c */

void bld_use_dep(Bld_Target* t, Bld_Dep* dep) {
    if (dep && !dep->found)
        bld_log_info("-- warning: using unfound dependency '%s' on target '%s'\n",
                     dep->name ? dep->name : "(unnamed)", t->name);
    bld_da_push(&t->ext_deps, dep);
}

/* ================================================================
 *  Step primitives
 * ================================================================ */

static Bld_Step* bld__alloc_step(Bld* b, const char* name, bool silent) {
    Bld_Step* s = bld_arena_alloc(sizeof(Bld_Step));
    memset(s, 0, sizeof(*s));
    s->name = name ? bld_str_dup(name) : "";
    s->silent = silent;
    pthread_mutex_init(&s->mutex, NULL);
    pthread_cond_init(&s->cond, NULL);
    bld_da_push(&b->all_steps, s);
    return s;
}

/* ================================================================
 *  Target primitives
 * ================================================================ */

static void bld__init_target(Bld* b, Bld_Target* t, Bld_TargetKind kind,
                              const char* name, const char* desc) {
    t->kind = kind;
    t->name = name ? bld_str_dup(name) : "";
    t->desc = desc ? bld_str_dup(desc) : "";
    for (size_t i = 0; i < b->all_targets.count; i++)
        if (strcmp(b->all_targets.items[i]->name, t->name) == 0)
            bld_panic("duplicate target name: '%s'\n", t->name);
    t->entry = bld__alloc_step(b, bld_str_fmt("%s:entry", t->name), true);
    t->exit  = bld__alloc_step(b, bld_str_dup(t->name), false);
    bld_da_push(&t->exit->deps, t->entry);
    bld_da_push(&b->all_targets, t);
}

/* ================================================================
 *  Safe casts
 * ================================================================ */

static Bld_Exe* bld__as_exe(Bld_Target* t) {
    if (t->kind != BLD_TGT_EXE) bld_panic("expected exe target '%s', got kind %d\n", t->name, t->kind);
    return (Bld_Exe*)t;
}

static Bld_Lib* bld__as_lib(Bld_Target* t) {
    if (t->kind != BLD_TGT_LIB) bld_panic("expected lib target '%s', got kind %d\n", t->name, t->kind);
    return (Bld_Lib*)t;
}

/* ================================================================
 *  Target operations
 * ================================================================ */

void bld_depends_on(Bld_Target* a, Bld_Target* b) {
    bld_da_push(&a->entry->deps, b->exit);
}

void bld_link_with(Bld_Target* a, Bld_Target* b) {
    if (b->kind != BLD_TGT_EXE && b->kind != BLD_TGT_LIB)
        bld_panic("link_with: target '%s' is not an exe or lib (got custom step '%s')\n", a->name, b->name);
    bld_da_push(&a->link_deps, b);
}

/* collect transitive link deps (depth-first, deduplicated) */
static void bld__collect_link_deps(Bld_Target* t, Bld_Target*** items, size_t* count, size_t* cap) {
    for (size_t i = 0; i < t->link_deps.count; i++) {
        Bld_Target* dep = t->link_deps.items[i];
        bool found = false;
        for (size_t j = 0; j < *count; j++)
            if ((*items)[j] == dep) { found = true; break; }
        if (found) continue;
        bld__collect_link_deps(dep, items, count, cap);
        /* push */
        if (*count >= *cap) {
            size_t nc = *cap ? *cap * 2 : 8;
            *items = bld_arena_realloc(*items, *cap * sizeof(Bld_Target*), nc * sizeof(Bld_Target*));
            *cap = nc;
        }
        (*items)[(*count)++] = dep;
    }
}

static void bld__push_ext_dep_dedup(Bld_Exe* exe, Bld_Dep* dep) {
    for (size_t i = 0; i < exe->resolved_ext_deps.count; i++)
        if (exe->resolved_ext_deps.items[i] == dep) return;
    bld_da_push(&exe->resolved_ext_deps, dep);
}

static Bld_Step* bld__ensure_publish_step(Bld* b, Bld_Lib* lib);

/* wire up link deps into steps (called after configure, before build) */
static void bld__resolve_link_deps(Bld* b) {
    for (size_t i = 0; i < b->all_targets.count; i++) {
        Bld_Target* t = b->all_targets.items[i];

        /* add target's own ext_deps to resolved list (always, even with no link_deps) */
        if (t->kind == BLD_TGT_EXE) {
            Bld_Exe* exe = bld__as_exe(t);
            for (size_t k = 0; k < t->ext_deps.count; k++)
                bld__push_ext_dep_dedup(exe, t->ext_deps.items[k]);
        }

        if (t->link_deps.count == 0) continue;

        Bld_Target** all_deps = NULL;
        size_t dep_count = 0, dep_cap = 0;
        bld__collect_link_deps(t, &all_deps, &dep_count, &dep_cap);

        for (size_t j = 0; j < dep_count; j++) {
            Bld_Target* dep = all_deps[j];
            bld_da_push(&t->exit->deps, dep->exit);

            if (t->kind == BLD_TGT_LIB && dep->kind == BLD_TGT_LIB) {
                /* static lib → static lib: ordering only */
            } else if (dep->kind == BLD_TGT_LIB && ((Bld_Lib*)dep)->opts.shared && t->kind == BLD_TGT_EXE) {
                Bld_Lib* slib = (Bld_Lib*)dep;
                bld_da_push(&bld__as_exe(t)->shared_libs, slib);
                Bld_Step* pub = bld__ensure_publish_step(b, slib);
                bld_da_push(&t->exit->deps, pub);
            } else {
                bld_da_push(&t->exit->inputs, dep->exit);
            }

            if (t->kind == BLD_TGT_EXE) {
                Bld_Exe* exe = bld__as_exe(t);
                for (size_t k = 0; k < dep->ext_deps.count; k++)
                    bld__push_ext_dep_dedup(exe, dep->ext_deps.items[k]);
            }

            /* propagate compile_propagate from lib deps as synthetic ext_dep */
            if (dep->kind == BLD_TGT_LIB) {
                Bld_CompileFlags* pub = &((Bld_Lib*)dep)->opts.compile_propagate;
                if (pub->include_dirs.len || pub->system_include_dirs.len ||
                    pub->defines.len || pub->extra_flags) {
                    /* build extra_cflags from defines */
                    const char* dcflags = NULL;
                    if (pub->defines.len) {
                        Bld_Cmd dc = {0};
                        for (size_t di = 0; di < pub->defines.len; di++)
                            bld_cmd_appendf(&dc, " -D%s", pub->defines.items[di]);
                        if (pub->extra_flags)
                            bld_cmd_appendf(&dc, " %s", pub->extra_flags);
                        dcflags = dc.items;
                    } else {
                        dcflags = pub->extra_flags;
                    }
                    Bld_Dep* syn = bld_arena_alloc(sizeof(Bld_Dep));
                    *syn = (Bld_Dep){
                        .name = bld_str_fmt("%s:pub", dep->name),
                        .found = true,
                        .include_dirs = pub->include_dirs,
                        .system_include_dirs = pub->system_include_dirs,
                        .extra_cflags = dcflags,
                    };
                    bld_da_push(&t->ext_deps, syn);
                }

                /* propagate link_propagate from lib deps as synthetic ext_dep (link-side) */
                Bld_LinkFlags* lpub = &((Bld_Lib*)dep)->opts.link_propagate;
                if (lpub->libs.len || lpub->lib_dirs.len || lpub->extra_flags) {
                    Bld_Dep* lsyn = bld_arena_alloc(sizeof(Bld_Dep));
                    *lsyn = (Bld_Dep){
                        .name = bld_str_fmt("%s:link_propagate", dep->name),
                        .found = true,
                        .libs = lpub->libs,
                        .lib_dirs = lpub->lib_dirs,
                        .extra_ldflags = lpub->extra_flags,
                    };
                    bld_da_push(&t->ext_deps, lsyn);
                    if (t->kind == BLD_TGT_EXE)
                        bld__push_ext_dep_dedup(bld__as_exe(t), lsyn);
                }
            }
        }
    }
}

Bld_LazyPath bld_output(Bld_Target* t) {
    return (Bld_LazyPath){.source = t, .path = bld_path("")};
}

Bld_LazyPath bld_output_sub(Bld_Target* t, const char* subpath) {
    return (Bld_LazyPath){.source = t, .path = bld_path(bld_str_dup(subpath))};
}

void bld_add_include_dir(Bld_Target* t, Bld_LazyPath dir) {
    if (dir.source)
        bld_da_push(&t->entry->deps, dir.source->exit);
    bld_da_push(&t->include_dirs, dir);
}

void bld_add_source(Bld_Target* t, Bld_LazyPath src) {
    if (src.source)
        bld_da_push(&t->entry->deps, src.source->exit);
    bld_da_push(&t->lazy_sources, src);
}

/* Bld_Cmd, bld_cmd_appendf, bld_cmd_append_sq — declared in bld_core.h, implemented in bld_core_impl.c */

/* ================================================================
 *  Flag rendering
 * ================================================================ */

/* bld__resolve_optimize, bld__resolve_c_standard, bld__resolve_cxx_standard
 * are defined in bld_core_impl.c (shared by toolchain renderers) */

static bool bld__toggle_val(Bld_Toggle t, Bld_Toggle global) {
    if (t == BLD_ON) return true;
    if (t == BLD_OFF) return false;
    return global == BLD_ON;
}

/* ================================================================
 *  Flag hashing
 * ================================================================ */

static Bld_Hash bld__hash_compile_flags(Bld_Hash h, const Bld_CompileFlags* f) {
    h = bld_hash_combine(h, (Bld_Hash){f->optimize});
    h = bld_hash_combine(h, (Bld_Hash){f->warnings});
    h = bld_hash_combine(h, (Bld_Hash){f->debug_info});
    if (f->extra_flags) h = bld_hash_combine(h, bld_hash_str(f->extra_flags));
    for (size_t i = 0; i < f->defines.len; i++) h = bld_hash_combine(h, bld_hash_str(f->defines.items[i]));
    for (size_t i = 0; i < f->include_dirs.len; i++) h = bld_hash_combine(h, bld_hash_str(f->include_dirs.items[i]));
    for (size_t i = 0; i < f->system_include_dirs.len; i++) h = bld_hash_combine(h, bld_hash_str(f->system_include_dirs.items[i]));
    return h;
}

/* build flags that affect compilation (asan, lto from Bld_BuildFlags) */
static Bld_Hash bld__hash_build_flags(Bld_Hash h, const Bld_BuildFlags* f) {
    h = bld_hash_combine(h, (Bld_Hash){f->asan});
    h = bld_hash_combine(h, (Bld_Hash){f->lto});
    return h;
}

/* all link flags (for link step hash) */
static Bld_Hash bld__hash_link_flags(Bld_Hash h, const Bld_LinkFlags* f) {
    if (f->extra_flags) h = bld_hash_combine(h, bld_hash_str(f->extra_flags));
    for (size_t i = 0; i < f->libs.len; i++) h = bld_hash_combine(h, bld_hash_str(f->libs.items[i]));
    for (size_t i = 0; i < f->lib_dirs.len; i++) h = bld_hash_combine(h, bld_hash_str(f->lib_dirs.items[i]));
    return h;
}

/* ================================================================
 *  Lazy path resolution
 * ================================================================ */

static Bld_Path bld__resolve_lazy(Bld* b, Bld_LazyPath lp) {
    if (lp.source) {
        Bld_Path art = bld__target_artifact(b, lp.source);
        if (lp.path.s[0]) return bld_path_join(art, lp.path);
        return art;
    }
    return bld_path_join(b->root, lp.path);
}

/* ================================================================
 *  Language inference
 * ================================================================ */

static Bld_Lang bld__infer_lang(const char* path) {
    const char* ext = bld_path_ext(bld_path(path));
    if (!strcmp(ext, ".c") || !strcmp(ext, ".m")) return BLD_LANG_C;
    if (!strcmp(ext, ".cpp") || !strcmp(ext, ".cc") || !strcmp(ext, ".cxx") ||
        !strcmp(ext, ".mm") || !strcmp(ext, ".C")) return BLD_LANG_CXX;
    if (!strcmp(ext, ".s") || !strcmp(ext, ".S")) return BLD_LANG_ASM;
    return BLD_LANG_C; /* unknown -> C */
}

/* ================================================================
 *  Obj step (compile .c -> .o)
 * ================================================================ */

typedef struct {
    Bld* b; Bld_Target* parent;
    Bld_Path source; Bld_Path orig_source;
    Bld_LazyPath lazy_source; /* if set, resolved at build time instead of source */
    Bld_CompileFlags compile; bool pic;
    Bld_Lang lang;
} Bld__ObjCtx;

/* merge override on top of base: non-zero fields replace */
static Bld_CompileFlags bld__merge_compile_flags(Bld_CompileFlags base, const Bld_CompileFlags* ov) {
    if (ov->optimize)                   base.optimize = ov->optimize;
    if (ov->warnings)                   base.warnings = ov->warnings;
    if (ov->debug_info)                 base.debug_info = ov->debug_info;
    if (ov->extra_flags)                base.extra_flags = ov->extra_flags;
    if (ov->defines.len)                base.defines = ov->defines;
    if (ov->include_dirs.len)           base.include_dirs = ov->include_dirs;
    if (ov->system_include_dirs.len)    base.system_include_dirs = ov->system_include_dirs;
    return base;
}

/* find per-file override by suffix match */
static const Bld_CompileFlags* bld__find_file_override(Bld_Target* t, const char* source) {
    for (size_t i = 0; i < t->file_overrides.count; i++) {
        const char* pattern = t->file_overrides.items[i].file;
        size_t plen = strlen(pattern), slen = strlen(source);
        if (plen <= slen && strcmp(source + slen - plen, pattern) == 0)
            return &t->file_overrides.items[i].flags;
    }
    return NULL;
}

/* resolve compile flags with per-file override applied at runtime */
static Bld_CompileFlags bld__resolve_obj_flags(Bld__ObjCtx* c) {
    const Bld_CompileFlags* ov = bld__find_file_override(c->parent, c->orig_source.s);
    return ov ? bld__merge_compile_flags(c->compile, ov) : c->compile;
}

static Bld_Path bld__obj_source(Bld__ObjCtx* c) {
    if (c->lazy_source.source)
        return bld__resolve_lazy(c->b, c->lazy_source);
    return c->source;
}

static Bld_CompileCmd bld__build_compile_cmd(Bld__ObjCtx* c, Bld_Path output, Bld_Path depfile);

/* render full compile command (used by compile_commands.json) */
static void bld__render_obj_cmd(Bld_Cmd* cmd, Bld__ObjCtx* c) {
    Bld_Path dummy_out = bld_path("placeholder.o");
    Bld_Path dummy_dep = bld_path("");
    Bld_CompileCmd cc = bld__build_compile_cmd(c, dummy_out, dummy_dep);
    c->b->toolchain->render_compile(cmd, cc);
}

static Bld_ActionResult bld__obj_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    Bld__ObjCtx* c = ctx;
    Bld_Toolchain* tc = c->b->toolchain;
    Bld_CompileCmd cc = bld__build_compile_cmd(c, output, depfile);
    Bld_Cmd cmd = {0};
    tc->render_compile(&cmd, cc);
    if (c->b->settings.verbose) bld_log_action("compile: %s\n", cmd.items);
    int rc = system(cmd.items);
    return rc == 0 ? BLD_ACTION_OK : BLD_ACTION_FAILED;
}

static Bld_Hash bld__obj_recipe_hash(void* ctx, Bld_Hash h) {
    Bld__ObjCtx* c = ctx;
    Bld_CompileFlags flags = bld__resolve_obj_flags(c);
    Bld_Path src = bld__obj_source(c);
    Bld_Toolchain* tc = c->b->toolchain;
    Bld_Compiler* comp = bld_compiler(c->b, c->lang);
    h = bld_hash_combine(h, comp->identity_hash);
    h = bld_hash_combine(h, bld_hash_str(tc->name));
    h = bld_hash_combine(h, bld_hash_str(tc->obj_ext));
    /* hash compiler standard */
    if (comp->lang == BLD_LANG_C) h = bld_hash_combine(h, (Bld_Hash){comp->c.standard});
    else if (comp->lang == BLD_LANG_CXX) h = bld_hash_combine(h, (Bld_Hash){comp->cxx.standard});
    h = bld_hash_combine(h, bld_hash_str(src.s));
    h = bld_hash_combine(h, bld_hash_file(src));
    h = bld__hash_compile_flags(h, &flags);
    h = bld__hash_build_flags(h, &c->b->build_flags);
    h = bld_hash_combine(h, (Bld_Hash){c->pic});
    h = bld_hash_combine(h, (Bld_Hash){c->parent->include_dirs.count});
    for (size_t i = 0; i < c->parent->include_dirs.count; i++) {
        Bld_LazyPath lp = c->parent->include_dirs.items[i];
        if (lp.path.s[0]) h = bld_hash_combine(h, bld_hash_str(lp.path.s));
    }
    /* hash ext_deps compile-side */
    for (size_t i = 0; i < c->parent->ext_deps.count; i++) {
        Bld_Dep* d = c->parent->ext_deps.items[i];
        for (size_t j = 0; j < d->include_dirs.len; j++) h = bld_hash_combine(h, bld_hash_str(d->include_dirs.items[j]));
        for (size_t j = 0; j < d->system_include_dirs.len; j++) h = bld_hash_combine(h, bld_hash_str(d->system_include_dirs.items[j]));
        if (d->extra_cflags) h = bld_hash_combine(h, bld_hash_str(d->extra_cflags));
    }
    return h;
}

static Bld_Step* bld__add_obj(Bld* b, Bld_Target* parent, Bld_Path source,
                               Bld_CompileFlags compile, bool pic,
                               Bld_Lang target_lang) {
    Bld_Lang lang = (target_lang != BLD_LANG_AUTO) ? target_lang : bld__infer_lang(source.s);
    Bld_Path abs_src = bld_path_join(b->root, source);
    const char* name = bld_str_fmt("%s:%s", parent->name, bld_path_replace_ext(source, bld_str_fmt(".%s", b->toolchain->obj_ext)).s);
    Bld_Step* s = bld__alloc_step(b, name, true);
    s->has_depfile = true;
    bld_da_push(&s->deps, parent->entry);

    Bld__ObjCtx* ctx = bld_arena_alloc(sizeof(Bld__ObjCtx));
    *ctx = (Bld__ObjCtx){.b = b, .parent = parent, .source = abs_src, .orig_source = source,
                          .compile = compile, .pic = pic, .lang = lang};
    s->action = bld__obj_action;
    s->action_ctx = ctx;
    s->hash_fn = bld__obj_recipe_hash;
    s->hash_fn_ctx = ctx;
    return s;
}

static Bld_Step* bld__add_lazy_obj(Bld* b, Bld_Target* parent, Bld_LazyPath lazy_source,
                                    Bld_CompileFlags compile, bool pic,
                                    Bld_Lang target_lang) {
    Bld_Lang lang = (target_lang != BLD_LANG_AUTO) ? target_lang : bld__infer_lang(lazy_source.path.s);
    const char* src_name = lazy_source.source ? lazy_source.source->name : "gen";
    const char* name = bld_str_fmt("%s:lazy_%s.%s", parent->name, src_name, b->toolchain->obj_ext);
    Bld_Step* s = bld__alloc_step(b, name, true);
    s->has_depfile = true;
    bld_da_push(&s->deps, parent->entry);
    if (lazy_source.source)
        bld_da_push(&s->inputs, lazy_source.source->exit);

    Bld__ObjCtx* ctx = bld_arena_alloc(sizeof(Bld__ObjCtx));
    *ctx = (Bld__ObjCtx){.b = b, .parent = parent, .source = bld_path(""),
                          .orig_source = bld_path(""), .lazy_source = lazy_source,
                          .compile = compile, .pic = pic, .lang = lang};
    s->action = bld__obj_action;
    s->action_ctx = ctx;
    s->hash_fn = bld__obj_recipe_hash;
    s->hash_fn_ctx = ctx;
    return s;
}

/* materialize lazy sources into obj steps (called after configure) */
static void bld__materialize_lazy_sources(Bld* b) {
    for (size_t i = 0; i < b->all_targets.count; i++) {
        Bld_Target* t = b->all_targets.items[i];
        if (t->lazy_sources.count == 0) continue;
        if (t->kind != BLD_TGT_EXE && t->kind != BLD_TGT_LIB) continue;

        Bld_StepList* obj_steps = (t->kind == BLD_TGT_EXE)
            ? &((Bld_Exe*)t)->obj_steps
            : &((Bld_Lib*)t)->obj_steps;
        Bld_CompileFlags compile = (t->kind == BLD_TGT_EXE)
            ? ((Bld_Exe*)t)->opts.compile
            : ((Bld_Lib*)t)->opts.compile;
        bool pic = (t->kind == BLD_TGT_LIB);

        Bld_Lang target_lang = (t->kind == BLD_TGT_EXE)
            ? ((Bld_Exe*)t)->opts.lang : ((Bld_Lib*)t)->opts.lang;

        for (size_t j = 0; j < t->lazy_sources.count; j++) {
            Bld_Step* obj = bld__add_lazy_obj(b, t, t->lazy_sources.items[j], compile, pic, target_lang);
            bld_da_push(obj_steps, obj);
            bld_da_push(&t->exit->inputs, obj);
        }
    }
}

/* ================================================================
 *  Link compiler selection (strongest language: CXX > C > ASM)
 * ================================================================ */

static Bld_Compiler* bld__link_compiler(Bld* b, Bld_StepList* obj_steps) {
    for (size_t i = 0; i < obj_steps->count; i++) {
        Bld__ObjCtx* ctx = obj_steps->items[i]->action_ctx;
        if (ctx && ctx->lang == BLD_LANG_CXX) return bld_compiler(b, BLD_LANG_CXX);
    }
    return bld_compiler(b, BLD_LANG_C);
}

/* ================================================================
 *  Resolve helpers (build Bld_CompileCmd / Bld_LinkCmd structs)
 * ================================================================ */

/*
 * Build a Bld_CompileCmd from an ObjCtx, resolving all flags, toggles, and
 * flattening include dirs / ext_dep cflags into the struct fields.
 * The resulting struct is passed to tc->render_compile.
 */
static Bld_CompileCmd bld__build_compile_cmd(Bld__ObjCtx* c, Bld_Path output, Bld_Path depfile) {
    Bld_CompileFlags flags = bld__resolve_obj_flags(c);
    Bld_Compiler* comp = bld_compiler(c->b, c->lang);

    /* resolve optimize: per-file > global */
    Bld_Optimize opt = flags.optimize ? flags.optimize : c->b->global_optimize;

    /* resolve warnings: ON=1, OFF=0, UNSET=global */
    bool warnings = (flags.warnings == BLD_ON) ? true :
                    (flags.warnings == BLD_OFF) ? false : c->b->global_warnings;

    /* resolve debug_info from compile flags */
    bool debug_info = bld__toggle_val(flags.debug_info, BLD_UNSET);

    /* resolve asan, lto from build_flags */
    bool asan = bld__toggle_val(c->b->build_flags.asan, BLD_UNSET);
    bool lto  = bld__toggle_val(c->b->build_flags.lto,  BLD_UNSET);

    /* Build extra_cflags: target include_dirs (quoted) + ext_dep includes/sys_includes/extra_cflags. */
    Bld_Cmd ecf = {0};
    for (size_t i = 0; i < c->parent->include_dirs.count; i++) {
        Bld_Path dir = bld__resolve_lazy(c->b, c->parent->include_dirs.items[i]);
        bld_cmd_appendf(&ecf, " -I\"%s\"", dir.s);
    }
    for (size_t i = 0; i < c->parent->ext_deps.count; i++) {
        Bld_Dep* d = c->parent->ext_deps.items[i];
        for (size_t j = 0; j < d->include_dirs.len; j++)
            bld_cmd_appendf(&ecf, " -I%s", d->include_dirs.items[j]);
        for (size_t j = 0; j < d->system_include_dirs.len; j++)
            bld_cmd_appendf(&ecf, " -isystem %s", d->system_include_dirs.items[j]);
        if (d->extra_cflags && d->extra_cflags[0])
            bld_cmd_appendf(&ecf, " %s", d->extra_cflags);
    }
    /* ecf.items starts with a leading space if non-empty; the render function
       checks [0] before appending, so we skip the leading space */
    const char* extra_cflags_str = (ecf.count > 0) ? ecf.items + 1 : NULL;

    Bld_CompileCmd result = {0};
    result.driver      = comp->driver;
    result.lang        = comp->lang;
    if (comp->lang == BLD_LANG_C)   result.c_std = comp->c.standard;
    if (comp->lang == BLD_LANG_CXX) result.cxx_std = comp->cxx.standard;
    result.optimize    = opt;
    result.warnings    = warnings;
    result.pic         = c->pic;
    result.debug_info  = debug_info;
    result.asan        = asan;
    result.lto         = lto;
    result.extra_flags = flags.extra_flags;
    result.defines          = flags.defines;
    result.include_dirs     = flags.include_dirs;
    result.sys_include_dirs = flags.system_include_dirs;
    result.extra_cflags     = extra_cflags_str;
    result.source      = bld__obj_source(c).s;
    result.output      = output.s;
    result.depfile     = depfile.s;
    return result;
}

/*
 * Build a Bld_LinkCmd for linking an executable, resolving all toggles and
 * flattening obj paths, lib dirs/names, rpaths, and extra ldflags.
 * The resulting struct is passed to tc->render_link.
 */
static Bld_LinkCmd bld__build_link_cmd_exe(Bld* b, Bld_Exe* exe, Bld_Path output) {
    /* resolve link toggles from build_flags and compile flags */
    bool debug_info = bld__toggle_val(exe->opts.compile.debug_info, BLD_UNSET);
    bool asan       = bld__toggle_val(b->build_flags.asan, BLD_UNSET);
    bool lto        = bld__toggle_val(b->build_flags.lto,  BLD_UNSET);

    /* build obj_paths from exit->inputs (only hash_valid steps) */
    Bld_Paths obj_paths = {0};
    for (size_t i = 0; i < exe->target.exit->inputs.count; i++) {
        Bld_Step* inp = exe->target.exit->inputs.items[i];
        if (inp->hash_valid)
            bld_paths_push(&obj_paths, bld__step_artifact(b, inp).s);
    }

    /* build lib_dirs and lib_names from shared_libs */
    Bld_Paths lib_dirs  = {0};
    Bld_Strs  lib_names = {0};

    if (exe->shared_libs.count > 0) {
        Bld_Path libdir = bld_path_join(b->out, bld_path("lib"));
        bld_paths_push(&lib_dirs, libdir.s);
        for (size_t i = 0; i < exe->shared_libs.count; i++)
            bld_strs_push(&lib_names, exe->shared_libs.items[i]->opts.name);
    }

    /* rpaths: $ORIGIN/../lib if shared_libs exist */
    Bld_Paths rpaths = {0};
    if (exe->shared_libs.count > 0)
        bld_paths_push(&rpaths, "$ORIGIN/../lib");

    /* merge extra_ldflags from opts.link.libs/lib_dirs + ext_deps + opts.link.extra_flags */
    Bld_Cmd ldf = {0};
    for (size_t i = 0; i < exe->opts.link.lib_dirs.len; i++)
        bld_cmd_appendf(&ldf, " -L%s", exe->opts.link.lib_dirs.items[i]);
    for (size_t i = 0; i < exe->opts.link.libs.len; i++)
        bld_cmd_appendf(&ldf, " -l%s", exe->opts.link.libs.items[i]);
    for (size_t i = 0; i < exe->resolved_ext_deps.count; i++) {
        Bld_Dep* d = exe->resolved_ext_deps.items[i];
        for (size_t j = 0; j < d->lib_dirs.len; j++)
            bld_cmd_appendf(&ldf, " -L%s", d->lib_dirs.items[j]);
        for (size_t j = 0; j < d->libs.len; j++)
            bld_cmd_appendf(&ldf, " -l%s", d->libs.items[j]);
        if (d->extra_ldflags && d->extra_ldflags[0])
            bld_cmd_appendf(&ldf, " %s", d->extra_ldflags);
    }
    if (exe->opts.link.extra_flags && exe->opts.link.extra_flags[0])
        bld_cmd_appendf(&ldf, " %s", exe->opts.link.extra_flags);
    const char* extra_ldflags_str = (ldf.count > 0) ? ldf.items + 1 : NULL;

    /* determine link driver */
    Bld_Compiler* linker = bld__link_compiler(b, &exe->obj_steps);

    Bld_LinkCmd result = {0};
    result.driver       = linker->driver;
    result.shared       = false;
    result.debug_info   = debug_info;
    result.asan         = asan;
    result.lto          = lto;
    result.obj_paths    = obj_paths;
    result.lib_dirs     = lib_dirs;
    result.lib_names    = lib_names;
    result.rpaths       = rpaths;
    result.extra_ldflags = extra_ldflags_str;
    result.output       = output.s;
    return result;
}

/* ================================================================
 *  Link exe
 * ================================================================ */

typedef struct { Bld* b; Bld_Exe* exe; } Bld__ExeCtx;

static Bld_ActionResult bld__link_exe_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)depfile;
    Bld__ExeCtx* c = ctx;
    Bld_Toolchain* tc = c->exe->toolchain;
    Bld_LinkCmd lc = bld__build_link_cmd_exe(c->b, c->exe, output);
    Bld_Cmd cmd = {0};
    tc->render_link(&cmd, lc);
    if (c->b->settings.verbose) bld_log_action("link exe: %s\n", cmd.items);
    int rc = system(cmd.items);
    return rc == 0 ? BLD_ACTION_OK : BLD_ACTION_FAILED;
}

static Bld_Hash bld__link_exe_recipe(void* ctx, Bld_Hash h) {
    Bld__ExeCtx* c = ctx;
    Bld_Toolchain* tc = c->exe->toolchain;
    h = bld_hash_combine(h, bld_hash_str(c->exe->opts.name));
    h = bld_hash_combine(h, bld_hash_str(tc->name));
    h = bld_hash_combine(h, bld_hash_str(tc->obj_ext));
    h = bld_hash_combine(h, bld_hash_str(tc->shared_lib_ext));
    Bld_Compiler* linker = bld__link_compiler(c->b, &c->exe->obj_steps);
    h = bld_hash_combine(h, linker->identity_hash);
    h = bld__hash_link_flags(h, &c->exe->opts.link);
    h = bld__hash_build_flags(h, &c->b->build_flags);
    h = bld__hash_compile_flags(h, &c->exe->opts.compile); /* debug_info affects link */
    for (size_t i = 0; i < c->exe->shared_libs.count; i++) {
        Bld_Lib* lib = (Bld_Lib*)c->exe->shared_libs.items[i];
        h = bld_hash_combine(h, bld_hash_str(lib->opts.name));
        h = bld_hash_combine(h, lib->target.exit->cache_key);
    }
    for (size_t i = 0; i < c->exe->resolved_ext_deps.count; i++) {
        Bld_Dep* d = c->exe->resolved_ext_deps.items[i];
        for (size_t j = 0; j < d->libs.len; j++) h = bld_hash_combine(h, bld_hash_str(d->libs.items[j]));
        for (size_t j = 0; j < d->lib_dirs.len; j++) h = bld_hash_combine(h, bld_hash_str(d->lib_dirs.items[j]));
        if (d->extra_ldflags) h = bld_hash_combine(h, bld_hash_str(d->extra_ldflags));
    }
    return h;
}

Bld_Target* bld__add_exe(Bld* b, const Bld_ExeOpts* opts) {
    if (!opts->name) bld_panic("exe name must not be NULL\n");
    Bld_Exe* exe = bld_arena_alloc(sizeof(Bld_Exe));
    memset(exe, 0, sizeof(*exe));
    exe->opts = *opts;
    exe->opts.name = bld_str_dup(opts->name);
    exe->opts.desc = opts->desc ? bld_str_dup(opts->desc) : "";
    exe->opts.output_name = opts->output_name ? bld_str_dup(opts->output_name) : NULL;
    exe->opts.sources = bld__clone_paths(opts->sources);
    exe->opts.compile = bld_clone_compile_flags(opts->compile);
    exe->opts.link = bld_clone_link_flags(opts->link);
    exe->toolchain = opts->toolchain ? opts->toolchain : b->toolchain;

    bld__init_target(b, &exe->target, BLD_TGT_EXE, exe->opts.name, exe->opts.desc);

    Bld__ExeCtx* ctx = bld_arena_alloc(sizeof(Bld__ExeCtx));
    *ctx = (Bld__ExeCtx){.b = b, .exe = exe};
    exe->target.exit->action = bld__link_exe_action;
    exe->target.exit->action_ctx = ctx;
    exe->target.exit->hash_fn = bld__link_exe_recipe;
    exe->target.exit->hash_fn_ctx = ctx;

    for (size_t i = 0; i < exe->opts.sources.len; i++) {
        Bld_Step* obj = bld__add_obj(b, &exe->target, bld_path(exe->opts.sources.items[i]), exe->opts.compile, false, exe->opts.lang);
        bld_da_push(&exe->obj_steps, obj);
        bld_da_push(&exe->target.exit->inputs, obj);
    }
    bld_da_push(&b->target_default->entry->deps, exe->target.exit);
    return &exe->target;
}

/* ================================================================
 *  Link lib
 * ================================================================ */

typedef struct { Bld* b; Bld_Lib* lib; } Bld__LibCtx;

static const char* bld__lib_filename(const Bld_Toolchain* tc, const Bld_LibOpts* o) {
    const char* base = (o->lib_basename && o->lib_basename[0]) ? o->lib_basename : o->name;
    return o->shared
        ? bld_str_fmt("%s%s.%s", tc->shared_lib_prefix, base, tc->shared_lib_ext)
        : bld_str_fmt("%s%s.%s", tc->static_lib_prefix, base, tc->static_lib_ext);
}

static Bld_ActionResult bld__link_lib_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)depfile;
    Bld__LibCtx* c = ctx;
    Bld_Toolchain* tc = c->lib->toolchain;
    Bld_Cmd cmd = {0};

    /* build obj paths from exit->inputs */
    Bld_Paths obj_paths = {0};
    for (size_t i = 0; i < c->lib->target.exit->inputs.count; i++) {
        Bld_Step* inp = c->lib->target.exit->inputs.items[i];
        if (inp->hash_valid)
            bld_paths_push(&obj_paths, bld__step_artifact(c->b, inp).s);
    }

    if (!c->lib->opts.shared) {
        /* static: archive */
        if (!tc->archiver.driver) bld_panic("no archive tool for toolchain '%s'\n", tc->name);
        tc->render_archive(&cmd, tc->archiver.driver, output.s, obj_paths);
    } else {
        /* shared: build LinkCmd with shared=1 */
        const char* soname = bld__lib_filename(tc, &c->lib->opts);
        Bld_Compiler* linker = bld__link_compiler(c->b, &c->lib->obj_steps);
        bool debug_info = bld__toggle_val(c->lib->opts.compile.debug_info, BLD_UNSET);
        bool asan       = bld__toggle_val(c->b->build_flags.asan, BLD_UNSET);
        bool lto        = bld__toggle_val(c->b->build_flags.lto,  BLD_UNSET);

        Bld_LinkCmd lc = {0};
        lc.driver       = linker->driver;
        lc.shared       = true;
        lc.debug_info   = debug_info;
        lc.asan         = asan;
        lc.lto          = lto;
        lc.soname       = soname;
        lc.obj_paths    = obj_paths;
        lc.extra_ldflags = c->lib->opts.link.extra_flags;
        lc.output       = output.s;
        tc->render_link(&cmd, lc);
    }
    if (c->b->settings.verbose) bld_log_action("link lib: %s\n", cmd.items);
    int rc = system(cmd.items);
    return rc == 0 ? BLD_ACTION_OK : BLD_ACTION_FAILED;
}

static Bld_Hash bld__link_lib_recipe(void* ctx, Bld_Hash h) {
    Bld__LibCtx* c = ctx;
    Bld_Toolchain* tc = c->lib->toolchain;
    h = bld_hash_combine(h, bld_hash_str(c->lib->opts.name));
    h = bld_hash_combine(h, bld_hash_str(tc->name));
    h = bld_hash_combine(h, bld_hash_str(tc->obj_ext));
    h = bld_hash_combine(h, bld_hash_str(tc->shared_lib_ext));
    h = bld_hash_combine(h, (Bld_Hash){c->lib->opts.shared});
    h = bld__hash_link_flags(h, &c->lib->opts.link);
    h = bld__hash_build_flags(h, &c->b->build_flags);
    if (c->lib->opts.shared) {
        Bld_Compiler* linker = bld__link_compiler(c->b, &c->lib->obj_steps);
        h = bld_hash_combine(h, linker->identity_hash);
    } else if (tc->archiver.driver) {
        h = bld_hash_combine(h, tc->archiver.identity_hash);
    }
    return h;
}

Bld_Target* bld__add_lib(Bld* b, const Bld_LibOpts* opts) {
    if (!opts->name) bld_panic("lib name must not be NULL\n");
    Bld_Lib* lib = bld_arena_alloc(sizeof(Bld_Lib));
    memset(lib, 0, sizeof(*lib));
    lib->opts = *opts;
    lib->opts.name = bld_str_dup(opts->name);
    lib->opts.desc = opts->desc ? bld_str_dup(opts->desc) : "";
    lib->opts.lib_basename = opts->lib_basename ? bld_str_dup(opts->lib_basename) : NULL;
    lib->opts.sources = bld__clone_paths(opts->sources);
    lib->opts.compile = bld_clone_compile_flags(opts->compile);
    lib->opts.compile_propagate = bld_clone_compile_flags(opts->compile_propagate);
    lib->opts.link = bld_clone_link_flags(opts->link);
    lib->opts.link_propagate = bld_clone_link_flags(opts->link_propagate);
    lib->toolchain = opts->toolchain ? opts->toolchain : b->toolchain;

    bld__init_target(b, &lib->target, BLD_TGT_LIB, lib->opts.name, lib->opts.desc);

    Bld__LibCtx* ctx = bld_arena_alloc(sizeof(Bld__LibCtx));
    *ctx = (Bld__LibCtx){.b = b, .lib = lib};
    lib->target.exit->action = bld__link_lib_action;
    lib->target.exit->action_ctx = ctx;
    lib->target.exit->hash_fn = bld__link_lib_recipe;
    lib->target.exit->hash_fn_ctx = ctx;

    for (size_t i = 0; i < lib->opts.sources.len; i++) {
        Bld_Step* obj = bld__add_obj(b, &lib->target, bld_path(lib->opts.sources.items[i]), lib->opts.compile, true, lib->opts.lang);
        bld_da_push(&lib->obj_steps, obj);
        bld_da_push(&lib->target.exit->inputs, obj);
    }
    bld_da_push(&b->target_default->entry->deps, lib->target.exit);
    return &lib->target;
}

/* ---- Shared lib publish (copy .so to out/lib/ for exe linking) ---- */

typedef struct { Bld* b; Bld_Lib* lib; } Bld__PublishCtx;

static Bld_ActionResult bld__publish_lib_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__PublishCtx* c = ctx;
    Bld_Path src = bld__target_artifact(c->b, &c->lib->target);
    Bld_Path dst = bld_path_join(bld_path_join(c->b->out, bld_path("lib")),
                                 bld_path(bld__lib_filename(c->lib->toolchain, &c->lib->opts)));
    bld_fs_mkdir_p(bld_path_parent(dst));
    bld_fs_copy_file(src, dst);
    return BLD_ACTION_OK;
}

static Bld_Step* bld__ensure_publish_step(Bld* b, Bld_Lib* lib) {
    if (lib->publish_step) return lib->publish_step;
    const char* name = bld_str_fmt("publish:%s", lib->opts.name);
    Bld_Step* s = bld__alloc_step(b, name, true);
    s->phony = true;
    bld_da_push(&s->deps, lib->target.exit);
    Bld__PublishCtx* ctx = bld_arena_alloc(sizeof(Bld__PublishCtx));
    *ctx = (Bld__PublishCtx){.b = b, .lib = lib};
    s->action = bld__publish_lib_action;
    s->action_ctx = ctx;
    lib->publish_step = s;
    return s;
}

/* ================================================================
 *  Custom step
 * ================================================================ */

typedef struct { Bld* b; Bld_Paths watch; } Bld__StepHashCtx;

static Bld_Hash bld__step_watch_hash(void* ctx, Bld_Hash h) {
    Bld__StepHashCtx* c = ctx;
    for (size_t i = 0; i < c->watch.len; i++) {
        Bld_Path p = bld_path_join(c->b->root, bld_path(c->watch.items[i]));
        if (bld_fs_is_dir(p)) h = bld_hash_combine(h, bld_hash_dir(p));
        else                   h = bld_hash_combine(h, bld_hash_file(p));
    }
    return h;
}

Bld_Target* bld__add_step(Bld* b, const Bld_StepOpts* opts) {
    if (!opts->name) bld_panic("step name must not be NULL\n");
    Bld_Target* t = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, t, BLD_TGT_CUSTOM, opts->name, opts->desc);
    if (opts->action) {
        t->exit->action = opts->action;
        t->exit->action_ctx = opts->action_ctx;
    }
    t->exit->has_depfile = opts->has_depfile;
    t->exit->content_hash = opts->watch.len ? true : opts->content_hash;
    if (opts->hash_fn) {
        t->exit->hash_fn = opts->hash_fn;
        t->exit->hash_fn_ctx = opts->hash_fn_ctx;
    } else if (opts->watch.len) {
        Bld__StepHashCtx* ctx = bld_arena_alloc(sizeof(Bld__StepHashCtx));
        *ctx = (Bld__StepHashCtx){.b = b, .watch = bld__clone_paths(opts->watch)};
        t->exit->hash_fn = bld__step_watch_hash;
        t->exit->hash_fn_ctx = ctx;
    }
    return t;
}

/* ================================================================
 *  Cmd — shell command step
 * ================================================================ */

typedef struct { const char* cmd; } Bld__CmdCtx;

static Bld_ActionResult bld__cmd_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__CmdCtx* c = ctx;
    int rc = system(c->cmd);
    if (rc != 0) return BLD_ACTION_FAILED;
    return BLD_ACTION_OK;
}

static Bld_Hash bld__cmd_hash(void* ctx, Bld_Hash h) {
    Bld__CmdCtx* c = ctx;
    return bld_hash_combine(h, bld_hash_str(c->cmd));
}

Bld_Target* bld__add_cmd(Bld* b, const Bld_CmdOpts* opts) {
    if (!opts->name) bld_panic("cmd name must not be NULL\n");
    if (!opts->cmd)  bld_panic("cmd command must not be NULL\n");
    Bld__CmdCtx* ctx = bld_arena_alloc(sizeof(Bld__CmdCtx));
    *ctx = (Bld__CmdCtx){.cmd = bld_str_dup(opts->cmd)};
    Bld_StepOpts step = {
        .name         = opts->name,
        .desc         = opts->desc,
        .action       = bld__cmd_action,
        .action_ctx   = ctx,
        .hash_fn      = bld__cmd_hash,
        .hash_fn_ctx  = ctx,
        .content_hash = true,
        .watch        = opts->watch,
    };
    return bld__add_step(b, &step);
}

/* ================================================================
 *  Run
 * ================================================================ */

typedef struct { Bld* b; Bld_Target* exe_tgt; Bld_RunOpts opts; } Bld__RunCtx;

static Bld_ActionResult bld__run_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__RunCtx* c = ctx;
    Bld_Cmd cmd = {0};
    if (c->opts.working_dir && c->opts.working_dir[0])
        bld_cmd_appendf(&cmd, "cd \"%s\" && ", c->opts.working_dir);
    bld_cmd_appendf(&cmd, "\"%s\"", bld__target_artifact(c->b, c->exe_tgt).s);
    for (size_t i = 0; i < c->opts.args.len; i++) bld_cmd_appendf(&cmd, " \"%s\"", c->opts.args.items[i]);
    for (size_t i = 0; i < c->b->settings.passthrough.len; i++) bld_cmd_appendf(&cmd, " \"%s\"", c->b->settings.passthrough.items[i]);
    int rc = system(cmd.items);
    if (rc != 0) return BLD_ACTION_FAILED;
    return BLD_ACTION_OK;
}

Bld_Target* bld__add_run(Bld* b, Bld_Target* target, const Bld_RunOpts* opts) {
    Bld_Target* run = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, run, BLD_TGT_CUSTOM, opts->name ? opts->name : "run", opts->desc);
    bld_da_push(&run->entry->deps, target->exit);
    run->exit->phony = true; /* always execute, never cache */
    Bld__RunCtx* ctx = bld_arena_alloc(sizeof(Bld__RunCtx));
    *ctx = (Bld__RunCtx){.b = b, .exe_tgt = target, .opts = *opts};
    run->exit->action = bld__run_action;
    run->exit->action_ctx = ctx;
    return run;
}

/* ================================================================
 *  Install
 * ================================================================ */

typedef struct { Bld* b; Bld_Target* src; Bld_Path dst; } Bld__InstallCtx;

static Bld_ActionResult bld__install_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__InstallCtx* c = ctx;
    Bld_Path src = bld__target_artifact(c->b, c->src);
    bld_fs_mkdir_p(bld_path_parent(c->dst));
    if (bld_fs_is_dir(src)) bld_fs_copy_r(src, c->dst);
    else bld_fs_copy_file(src, c->dst);
    return BLD_ACTION_OK;
}

Bld_Target* bld_install(Bld* b, Bld_Target* target, Bld_Path dst) {
    Bld_Path full = bld_path_join(b->out, dst);
    Bld_Target* inst = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, inst, BLD_TGT_CUSTOM, bld_str_fmt("install-%s", target->name),
                     bld_str_fmt("Install %s", target->name));
    bld_da_push(&inst->entry->deps, target->exit);
    inst->exit->phony = true; /* always copy to destination */
    Bld__InstallCtx* ctx = bld_arena_alloc(sizeof(Bld__InstallCtx));
    *ctx = (Bld__InstallCtx){.b = b, .src = target, .dst = full};
    inst->exit->action = bld__install_action;
    inst->exit->action_ctx = ctx;
    bld_da_push(&b->target_default->entry->deps, inst->exit);
    return inst;
}

Bld_Target* bld_install_exe(Bld* b, Bld_Target* t) {
    Bld_Exe* exe = bld__as_exe(t);
    const char* oname = (exe->opts.output_name && exe->opts.output_name[0]) ? exe->opts.output_name : exe->opts.name;
    return bld_install(b, t, bld_path_join(bld_path("bin"), bld_path(oname)));
}

Bld_Target* bld_install_lib(Bld* b, Bld_Target* t) {
    Bld_Lib* lib = bld__as_lib(t);
    return bld_install(b, t, bld_path_join(bld_path("lib"), bld_path(bld__lib_filename(lib->toolchain, &lib->opts))));
}

typedef struct { Bld* b; Bld_Paths files; Bld_Path dst; } Bld__InstallFilesCtx;

static Bld_ActionResult bld__install_files_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__InstallFilesCtx* c = ctx;
    Bld_Path full = bld_path_join(c->b->out, c->dst);
    bld_fs_mkdir_p(full);
    for (size_t i = 0; i < c->files.len; i++) {
        Bld_Path src = bld_path(c->files.items[i]);
        const char* fname = bld_path_filename(src);
        Bld_Path dest = bld_path_join(full, bld_path(fname));
        bld_fs_copy_file(src, dest);
    }
    return BLD_ACTION_OK;
}

Bld_Target* bld_install_files(Bld* b, Bld_Paths files, Bld_Path dst) {
    Bld_Target* inst = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, inst, BLD_TGT_CUSTOM, "install-files", "Install files");
    inst->exit->phony = true;
    Bld__InstallFilesCtx* ctx = bld_arena_alloc(sizeof(Bld__InstallFilesCtx));
    *ctx = (Bld__InstallFilesCtx){.b = b, .files = files, .dst = dst};
    inst->exit->action = bld__install_files_action;
    inst->exit->action_ctx = ctx;
    bld_da_push(&b->target_default->entry->deps, inst->exit);
    return inst;
}

typedef struct { Bld* b; const char* src_dir; Bld_Path dst; } Bld__InstallDirCtx;

static Bld_ActionResult bld__install_dir_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__InstallDirCtx* c = ctx;
    Bld_Path full = bld_path_join(c->b->out, c->dst);
    bld_fs_mkdir_p(full);
    bld_fs_copy_r(bld_path(c->src_dir), full);
    return BLD_ACTION_OK;
}

Bld_Target* bld_install_dir(Bld* b, const char* src_dir, Bld_Path dst) {
    Bld_Target* inst = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, inst, BLD_TGT_CUSTOM,
                     bld_str_fmt("install-dir-%s", src_dir),
                     bld_str_fmt("Install directory %s", src_dir));
    inst->exit->phony = true;
    Bld__InstallDirCtx* ctx = bld_arena_alloc(sizeof(Bld__InstallDirCtx));
    *ctx = (Bld__InstallDirCtx){.b = b, .src_dir = src_dir, .dst = dst};
    inst->exit->action = bld__install_dir_action;
    inst->exit->action_ctx = ctx;
    bld_da_push(&b->target_default->entry->deps, inst->exit);
    return inst;
}

/* ================================================================
 *  Tests
 * ================================================================ */

Bld_Target* bld__add_test(Bld* b, Bld_Target* exe, const Bld_RunOpts* opts) {
    Bld_TestEntry entry = {0};
    entry.name = opts->name ? bld_str_dup(opts->name) : exe->name;
    entry.exe = exe;
    entry.working_dir = opts->working_dir ? bld_str_dup(opts->working_dir) : NULL;
    entry.args = bld__clone_strs(opts->args);
    bld_da_push(&b->tests, entry);
    return exe;
}

/* ================================================================
 *  User options
 * ================================================================ */

static Bld_UserOption* bld__find_user_option(Bld* b, const char* name) {
    for (size_t i = 0; i < b->user_options.count; i++)
        if (strcmp(b->user_options.items[i].key, name) == 0) return &b->user_options.items[i];
    return NULL;
}

static void bld__register_option(Bld* b, const char* name, const char* desc, int type, const char* default_val) {
    /* check for duplicate registration */
    for (size_t i = 0; i < b->avail_options.count; i++)
        if (strcmp(b->avail_options.items[i].name, name) == 0)
            bld_panic("option '%s' declared twice\n", name);
    Bld_AvailableOption ao = { .name = name, .description = desc, .type = type, .default_val = default_val };
    bld_da_push(&b->avail_options, ao);
}

bool bld_option_bool(Bld* b, const char* name, const char* desc, bool default_val) {
    bld__register_option(b, name, desc, BLD_OPT_TYPE_BOOL, default_val ? "on" : "off");
    Bld_UserOption* opt = bld__find_user_option(b, name);
    if (!opt) return default_val;
    opt->used = 1;
    if (!opt->value) return true;  /* -Dfoo without = means true */
    if (strcmp(opt->value, "true") == 0 || strcmp(opt->value, "1") == 0 || strcmp(opt->value, "on") == 0) return true;
    if (strcmp(opt->value, "false") == 0 || strcmp(opt->value, "0") == 0 || strcmp(opt->value, "off") == 0) return false;
    bld_panic("invalid bool value for -D%s=%s (expected true/false/1/0/on/off)\n", name, opt->value);
    return default_val;
}

const char* bld_option_str(Bld* b, const char* name, const char* desc, const char* default_val) {
    bld__register_option(b, name, desc, BLD_OPT_TYPE_STRING, default_val);
    Bld_UserOption* opt = bld__find_user_option(b, name);
    if (!opt) return default_val;
    opt->used = 1;
    return opt->value ? opt->value : default_val;
}

/* ================================================================
 *  Child build context
 * ================================================================ */

Bld* bld_new(Bld* parent) {
    Bld* b = bld_arena_alloc(sizeof(Bld));
    memset(b, 0, sizeof(*b));
    b->root = parent->root;
    b->cache = parent->cache;
    b->out = parent->out;
    b->toolchain = parent->toolchain;
    b->global_warnings = parent->global_warnings;
    b->global_optimize = parent->global_optimize;
    b->build_flags = parent->build_flags;
    b->settings = parent->settings;
    b->settings.silent = true; /* child builds are silent by default */
    b->argc = parent->argc;
    b->argv = parent->argv;
    return b;
}

/* ================================================================
 *  Compiler setters
 * ================================================================ */

static void bld__set_driver(Bld_Compiler* comp, const char* driver) {
    comp->driver = bld_str_dup(driver);
    comp->identity_hash = bld__make_identity_hash(driver);
    comp->available = bld__has_in_path(driver);
}

void bld__set_compiler_c(Bld* b, const Bld_CCompilerOpts* opts) {
    Bld_Compiler* comp = bld_compiler(b, BLD_LANG_C);
    if (opts->driver) bld__set_driver(comp, opts->driver);
    if (opts->standard) comp->c.standard = opts->standard;
}

void bld__set_compiler_cxx(Bld* b, const Bld_CxxCompilerOpts* opts) {
    Bld_Compiler* comp = bld_compiler(b, BLD_LANG_CXX);
    if (opts->driver) bld__set_driver(comp, opts->driver);
    if (opts->standard) comp->cxx.standard = opts->standard;
}

void bld__set_compiler_asm(Bld* b, const Bld_AsmCompilerOpts* opts) {
    Bld_Compiler* comp = bld_compiler(b, BLD_LANG_ASM);
    if (opts->driver) bld__set_driver(comp, opts->driver);
}

bool bld_target_ok(Bld_Target* t) {
    return t->exit->state == BLD_STEP_OK;
}

Bld_Path bld_target_artifact(Bld* b, Bld_Target* t) {
    return bld__step_artifact(b, t->exit);
}

/* Feature checks — implemented in bld_checks.c */

/* --- bld/bld_checks.c --- */
/* bld/bld_checks.c — feature detection implementation */


typedef struct {
    const char*  define_name;
    const char*  snippet;
    bool         is_sizeof;
    bool*        bool_result;
    int*         int_result;
    Bld_Target*  target;
} Bld__Check;

struct Bld_Checks {
    Bld*        parent;
    Bld*        child;
    Bld__Check* items;
    size_t      count;
    size_t      cap;
};

typedef struct { Bld* b; const char* snippet; bool is_sizeof; } Bld__CheckCtx;

static Bld_Hash bld__check_recipe_hash(void* ctx, Bld_Hash h) {
    Bld__CheckCtx* c = ctx;
    h = bld_hash_combine(h, bld_compiler(c->b, BLD_LANG_C)->identity_hash);
    h = bld_hash_combine(h, bld_hash_str(c->snippet));
    return h;
}

static Bld_ActionResult bld__check_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    Bld__CheckCtx* c = ctx;
    const char* cc = bld_compiler(c->b, BLD_LANG_C)->driver;

    Bld_Hash snip_hash = bld_hash_str(c->snippet);
    Bld_Path src = bld_path_join(
        bld_path_join(c->b->cache, bld_path("checks")),
        bld_path_fmt("%" PRIu64 ".c", snip_hash.value));
    bld_fs_mkdir_p(bld_path_parent(src));
    bld_fs_write_file(src, c->snippet, strlen(c->snippet));

    /* compile snippet via toolchain (same compiler/flags as real builds) */
    Bld_Path tmp_obj = bld__cache_tmp(c->b);
    Bld_CompileCmd cc_cmd = {
        .driver = cc,
        .lang = BLD_LANG_C,
        .source = src.s,
        .output = tmp_obj.s,
        .depfile = (!c->is_sizeof && depfile.s && depfile.s[0]) ? depfile.s : "",
    };
    Bld_Cmd cmd = {0};
    if (c->b->toolchain)
        c->b->toolchain->render_compile(&cmd, cc_cmd);
    else
        bld_cmd_appendf(&cmd, "%s -xc %s -c -o %s 2>/dev/null", cc, src.s, tmp_obj.s);
    /* suppress errors */
    bld_cmd_appendf(&cmd, " 2>/dev/null");
    int rc = system(cmd.items);

    if (!c->is_sizeof) {
        bld_fs_write_file(output, rc == 0 ? "1" : "0", 1);
    } else {
        if (rc != 0) {
            bld_fs_write_file(output, "0", 1);
        } else {
            /* scan .o for marker */
            static const char marker[] = "bld_check_entry:qWkPxLmNvRtBjHsYcFgDzAeUoIiXpK:sizeof:";
            size_t marker_len = sizeof(marker) - 1;
            size_t obj_len;
            const char* obj = bld_fs_read_file(tmp_obj, &obj_len);
            const char* found = NULL;
            for (size_t i = 0; i + marker_len + 5 <= obj_len; i++) {
                if (memcmp(obj + i, marker, marker_len) == 0) { found = obj + i + marker_len; break; }
            }
            if (found) {
                int val = 0;
                for (int d = 0; d < 5; d++) val = val * 10 + (found[d] - '0');
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", val);
                bld_fs_write_file(output, buf, strlen(buf));
            } else {
                bld_fs_write_file(output, "0", 1);
            }
        }
    }
    return BLD_ACTION_OK;
}

static void bld__checks_add(Bld_Checks* c, const char* define_name, const char* snippet,
                              bool is_sizeof, bool* bool_result, int* int_result) {
    if (c->count >= c->cap) {
        size_t nc = c->cap ? c->cap * 2 : 16;
        c->items = bld_arena_realloc(c->items, c->cap * sizeof(Bld__Check), nc * sizeof(Bld__Check));
        c->cap = nc;
    }
    Bld__Check* ch = &c->items[c->count++];
    ch->define_name = define_name;
    ch->snippet = snippet;
    ch->is_sizeof = is_sizeof;
    ch->bool_result = bool_result;
    ch->int_result = int_result;

    Bld__CheckCtx* ctx = bld_arena_alloc(sizeof(Bld__CheckCtx));
    *ctx = (Bld__CheckCtx){.b = c->child, .snippet = snippet, .is_sizeof = is_sizeof};

    ch->target = bld__add_step(c->child, &(Bld_StepOpts){
        .name = bld_str_fmt("check:%s", define_name),
        .action = bld__check_action,
        .action_ctx = ctx,
        .hash_fn = bld__check_recipe_hash,
        .hash_fn_ctx = ctx,
        .has_depfile = !is_sizeof,
        .content_hash = false,
    });
}

Bld_Checks* bld_checks_new(Bld* parent) {
    Bld_Checks* c = bld_arena_alloc(sizeof(Bld_Checks));
    memset(c, 0, sizeof(*c));
    c->parent = parent;
    c->child = bld_new(parent);
    return c;
}

bool* bld_checks_header(Bld_Checks* c, const char* define_name, const char* header) {
    bool* result = bld_arena_alloc(sizeof(bool));
    *result = false;
    bld__checks_add(c, define_name,
        bld_str_fmt("#include <%s>\nint main(){return 0;}\n", header), false, result, NULL);
    return result;
}

bool* bld_checks_func(Bld_Checks* c, const char* define_name, const char* func, const char* header) {
    bool* result = bld_arena_alloc(sizeof(bool));
    *result = false;
    bld__checks_add(c, define_name,
        bld_str_fmt("#include <%s>\nint main(){(void)%s;return 0;}\n", header, func), false, result, NULL);
    return result;
}

int* bld_checks_sizeof(Bld_Checks* c, const char* define_name, const char* type) {
    int* result = bld_arena_alloc(sizeof(int));
    *result = 0;
    bld__checks_add(c, define_name, bld_str_fmt(
        "#include <stddef.h>\n#include <sys/types.h>\n#include <time.h>\n"
        "#define BLD__V (sizeof(%s))\n"
        "char bld__m[] = {\n"
        "    'b','l','d','_','c','h','e','c','k','_','e','n','t','r','y',':',\n"
        "    'q','W','k','P','x','L','m','N','v','R','t','B','j','H','s',\n"
        "    'Y','c','F','g','D','z','A','e','U','o','I','i','X','p','K',\n"
        "    ':','s','i','z','e','o','f',':',\n"
        "    ('0'+((BLD__V/10000)%%10)),\n"
        "    ('0'+((BLD__V/1000)%%10)),\n"
        "    ('0'+((BLD__V/100)%%10)),\n"
        "    ('0'+((BLD__V/10)%%10)),\n"
        "    ('0'+(BLD__V%%10)),\n"
        "    '\\0'\n"
        "};\n", type), true, NULL, result);
    return result;
}

bool* bld_checks_compile(Bld_Checks* c, const char* define_name, const char* source) {
    bool* result = bld_arena_alloc(sizeof(bool));
    *result = false;
    bld__checks_add(c, define_name, source, false, result, NULL);
    return result;
}

void bld_checks_run(Bld_Checks* c) {
    bld_execute(c->child);

    size_t changed = 0;
    for (size_t i = 0; i < c->count; i++) {
        Bld__Check* ch = &c->items[i];
        if (!bld_target_ok(ch->target)) continue;
        Bld_Path art = bld_target_artifact(c->child, ch->target);
        if (!bld_fs_exists(art)) continue;
        size_t len;
        const char* val = bld_fs_read_file(art, &len);
        if (ch->bool_result) {
            bool prev = *ch->bool_result;
            *ch->bool_result = (len > 0 && val[0] == '1');
            if (*ch->bool_result != prev) changed++;
        }
        if (ch->int_result) {
            int prev = *ch->int_result;
            *ch->int_result = atoi(val);
            if (*ch->int_result != prev) changed++;
        }
    }

    if (c->child->steps_executed > 0) {
        size_t yes = 0, no = 0;
        for (size_t i = 0; i < c->count; i++) {
            if (c->items[i].bool_result && *c->items[i].bool_result) yes++;
            else if (c->items[i].int_result && *c->items[i].int_result > 0) yes++;
            else if (bld_target_ok(c->items[i].target)) no++;
        }
        bld_log_info("-- %zu checks (%zu yes, %zu no)\n", c->count, yes, no);
    }
}

void bld_checks_write(Bld_Checks* c, const char* path) {
    Bld_Cmd out = {0};
    bld_cmd_appendf(&out, "/* generated by bld feature checks */\n");
    for (size_t i = 0; i < c->count; i++) {
        Bld__Check* ch = &c->items[i];
        if (ch->bool_result) {
            if (*ch->bool_result)
                bld_cmd_appendf(&out, "#define %s 1\n", ch->define_name);
            else
                bld_cmd_appendf(&out, "/* %s not found */\n", ch->define_name);
        } else if (ch->int_result) {
            if (*ch->int_result > 0)
                bld_cmd_appendf(&out, "#define %s %d\n", ch->define_name, *ch->int_result);
            else
                bld_cmd_appendf(&out, "/* %s unknown */\n", ch->define_name);
        }
    }
    bld_fs_write_file(bld_path_join(c->parent->root, bld_path(path)), out.items, out.count);
}
/* --- bld/bld_exec.c --- */
/* bld/bld_exec.c — step execution, topo sort, parallel workers */


/* ---- Build stats — stored on Bld, accessed atomically ---- */

/* ---- Step sync ---- */

static Bld_StepState bld__step_state(Bld_Step* s) {
    pthread_mutex_lock(&s->mutex); Bld_StepState st = s->state; pthread_mutex_unlock(&s->mutex); return st;
}
static int bld__step_is_done(Bld_Step* s) {
    Bld_StepState st = bld__step_state(s);
    return st == BLD_STEP_OK || st == BLD_STEP_FAILED || st == BLD_STEP_SKIPPED;
}
static void bld__step_set_state(Bld_Step* s, Bld_StepState st) {
    pthread_mutex_lock(&s->mutex); s->state = st; pthread_cond_broadcast(&s->cond); pthread_mutex_unlock(&s->mutex);
}
static void bld__step_wait(Bld_Step* s) {
    pthread_mutex_lock(&s->mutex);
    while (s->state == BLD_STEP_PENDING || s->state == BLD_STEP_RUNNING)
        pthread_cond_wait(&s->cond, &s->mutex);
    pthread_mutex_unlock(&s->mutex);
}

/* ---- Hash + cache check ---- */

static void bld__compute_input_hash(Bld* b, Bld_Step* step) {
    (void)b;
    Bld_Hash h = {0};
    for (size_t i = 0; i < step->deps.count; i++)
        if (step->deps.items[i]->hash_valid)
            h = bld_hash_combine_unordered(h, step->deps.items[i]->cache_key);
    for (size_t i = 0; i < step->inputs.count; i++)
        if (step->inputs.items[i]->hash_valid)
            h = bld_hash_combine_unordered(h, step->inputs.items[i]->cache_key);
    if (step->hash_fn)
        h = step->hash_fn(step->hash_fn_ctx, h);
    step->input_hash = h;
}

/* ---- Perform step ---- */

static void bld__perform_step(Bld* b, Bld_Step* step) {
    if (bld__step_is_done(step)) return;

    /* check if any dep failed → skip */
    for (size_t i = 0; i < step->deps.count; i++) {
        Bld_StepState ds = bld__step_state(step->deps.items[i]);
        if (ds == BLD_STEP_FAILED || ds == BLD_STEP_SKIPPED) {
            __atomic_fetch_add(&b->steps_skipped, 1, __ATOMIC_RELAXED);
            bld__step_set_state(step, BLD_STEP_SKIPPED);
            return;
        }
    }
    for (size_t i = 0; i < step->inputs.count; i++) {
        if (!step->inputs.items[i]) continue;
        Bld_StepState ds = bld__step_state(step->inputs.items[i]);
        if (ds == BLD_STEP_FAILED || ds == BLD_STEP_SKIPPED) {
            __atomic_fetch_add(&b->steps_skipped, 1, __ATOMIC_RELAXED);
            bld__step_set_state(step, BLD_STEP_SKIPPED);
            return;
        }
    }

    bld__step_set_state(step, BLD_STEP_RUNNING);
    bld__compute_input_hash(b, step);

    if (bld__cache_has(b, step)) {
        __atomic_fetch_add(&b->steps_cached, 1, __ATOMIC_RELAXED);
        bld__step_set_state(step, BLD_STEP_OK);
        return;
    }

    /* execute */
    Bld_Path tmp_out = bld__cache_tmp(b);
    Bld_Path tmp_dep = step->has_depfile ? bld__cache_tmp(b) : bld_path("");
    Bld_ActionResult result = step->action(step->action_ctx, tmp_out, tmp_dep);

    if (result != BLD_ACTION_OK) {
        __atomic_fetch_add(&b->steps_failed, 1, __ATOMIC_RELAXED);
        bld__step_set_state(step, BLD_STEP_FAILED);
        return;
    }

    bld__cache_store(b, step, tmp_out, tmp_dep);

    __atomic_fetch_add(&b->steps_executed, 1, __ATOMIC_RELAXED);
    if (!b->settings.silent) {
        uint64_t n = __atomic_fetch_add(&b->progress_current, 1, __ATOMIC_RELAXED) + 1;
        bld_log_progress(n, b->progress_total, step->name);
    }
    bld__step_set_state(step, BLD_STEP_OK);
}

/* ---- Shared infrastructure ---- */

static size_t bld__step_idx(Bld* b, Bld_Step* s) {
    for (size_t i = 0; i < b->all_steps.count; i++)
        if (b->all_steps.items[i] == s) return i;
    bld_panic("step %s not found\n", s->name);
    return 0;
}

typedef struct {
    Bld* b; Bld_Step** order; size_t count; pthread_mutex_t* mu; size_t* idx;
} Bld__WorkerCtx;

static void* bld__worker_fn(void* arg) {
    Bld__WorkerCtx* c = arg;
    while (1) {
        pthread_mutex_lock(c->mu);
        if (*c->idx >= c->count) { pthread_mutex_unlock(c->mu); break; }
        Bld_Step* step = c->order[(*c->idx)++];
        pthread_mutex_unlock(c->mu);
        for (size_t i = 0; i < step->deps.count; i++) bld__step_wait(step->deps.items[i]);
        for (size_t i = 0; i < step->inputs.count; i++)
            if (step->inputs.items[i]) bld__step_wait(step->inputs.items[i]);
        bld__perform_step(c->b, step);
    }
    return NULL;
}

static void bld__check_missing_deps(Bld* b) {
    for (size_t i = 0; i < b->all_targets.count; i++) {
        Bld_Target* t = b->all_targets.items[i];
        for (size_t j = 0; j < t->ext_deps.count; j++) {
            Bld_Dep* d = t->ext_deps.items[j];
            if (!d->found)
                bld_panic("dependency '%s' not found (required by target '%s')\n",
                          d->name ? d->name : "unknown", t->name);
        }
    }
}

static Bld_StepList bld__topo_sort(Bld* b, Bld_StepList* roots) {
    Bld_StepList order = {0};
    enum { WHITE = 0, GRAY, BLACK };
    size_t ns = b->all_steps.count;
    int* col = bld_arena_alloc(ns * sizeof(int));
    memset(col, 0, ns * sizeof(int));

    typedef struct { Bld_Step* s; size_t di, ii; } Fr;
    BLD_DA(Fr) stk = {0};

    for (size_t ti = 0; ti < roots->count; ti++) {
        size_t ri = bld__step_idx(b, roots->items[ti]);
        if (col[ri] == BLACK) continue;
        bld_da_push(&stk, ((Fr){roots->items[ti], 0, 0}));
        col[ri] = GRAY;
        while (stk.count > 0) {
            Fr* f = &stk.items[stk.count - 1];
            bool pushed = false;
            while (f->di < f->s->deps.count) {
                Bld_Step* d = f->s->deps.items[f->di++];
                size_t di = bld__step_idx(b, d);
                if (col[di] == BLACK) continue;
                if (col[di] == GRAY) bld_panic("cycle: %s -> %s\n", f->s->name, d->name);
                col[di] = GRAY; bld_da_push(&stk, ((Fr){d, 0, 0})); pushed = true; break;
            }
            if (pushed) continue;
            while (f->ii < f->s->inputs.count) {
                Bld_Step* d = f->s->inputs.items[f->ii++];
                if (!d) continue;
                size_t di = bld__step_idx(b, d);
                if (col[di] == BLACK) continue;
                if (col[di] == GRAY) bld_panic("cycle: %s -> %s\n", f->s->name, d->name);
                col[di] = GRAY; bld_da_push(&stk, ((Fr){d, 0, 0})); pushed = true; break;
            }
            if (pushed) continue;
            col[bld__step_idx(b, f->s)] = BLACK;
            bld_da_push(&order, f->s);
            stk.count--;
        }
    }
    return order;
}

static void bld__build_steps(Bld* b, Bld_StepList order) {
    b->steps_executed = 0;
    b->steps_cached = 0;
    b->steps_failed = 0;
    b->steps_skipped = 0;
    b->progress_current = 0;
    b->progress_total = 0;

    /* pre-pass: resolve cached, count dirty */
    for (size_t i = 0; i < order.count; i++) {
        Bld_Step* step = order.items[i];
        bool any_dep_dirty = false;
        for (size_t j = 0; j < step->deps.count; j++)
            if (!bld__step_is_done(step->deps.items[j])) { any_dep_dirty = true; break; }
        if (!any_dep_dirty)
            for (size_t j = 0; j < step->inputs.count; j++)
                if (step->inputs.items[j] && !bld__step_is_done(step->inputs.items[j])) { any_dep_dirty = true; break; }
        if (any_dep_dirty) {
            if (step->action) b->progress_total++;
            continue;
        }
        bld__compute_input_hash(b, step);
        if (bld__cache_has(b, step)) {
            __atomic_fetch_add(&b->steps_cached, 1, __ATOMIC_RELAXED);
            if (b->settings.show_cached && step->action && !step->silent)
                bld_log_cached(step->name);
            bld__step_set_state(step, BLD_STEP_OK);
        } else {
            b->progress_total++;
        }
    }

    if (b->progress_total == 0) return;

    /* execute */
    pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
    size_t idx = 0;
    if (b->settings.max_jobs <= 1) {
        for (size_t i = 0; i < order.count; i++) bld__perform_step(b, order.items[i]);
    } else {
        pthread_t* th = bld_arena_alloc(sizeof(pthread_t) * (size_t)b->settings.max_jobs);
        Bld__WorkerCtx ctx = {b, order.items, order.count, &mu, &idx};
        Bld__WorkerCtx* shared = bld_arena_alloc(sizeof(Bld__WorkerCtx));
        *shared = ctx;
        for (int t = 0; t < b->settings.max_jobs; t++) pthread_create(&th[t], NULL, bld__worker_fn, shared);
        for (int t = 0; t < b->settings.max_jobs; t++) pthread_join(th[t], NULL);
    }
}

/* ---- Run build (called from main) ---- */

static void bld__run_build(Bld* b) {
    /* resolve requested targets */
    Bld_StepList to_build = {0};
    for (size_t ri = 0; ri < b->settings.targets.len; ri++) {
        const char* rq = b->settings.targets.items[ri];
        bool found = false;
        for (size_t i = 0; i < b->all_targets.count; i++) {
            if (strcmp(b->all_targets.items[i]->name, rq) == 0) {
                bld_da_push(&to_build, b->all_targets.items[i]->exit);
                found = true;
            }
        }
        if (!found) bld_panic("unknown target: %s\n", rq);
    }

    bld__check_missing_deps(b);

    Bld_StepList order = bld__topo_sort(b, &to_build);

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    bld__build_steps(b, order);

    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    if (!b->settings.silent)
        bld_log_done(b->steps_executed, b->steps_cached, b->steps_failed, b->steps_skipped, elapsed, bld_arena_get()->offset);
    if (b->steps_failed > 0) exit(1);
}

/* ---- Public: execute all targets in a Bld context ---- */

void bld_execute(Bld* b) {
    bld__materialize_lazy_sources(b);
    bld__resolve_link_deps(b);
    bld__check_missing_deps(b);

    Bld_StepList roots = {0};
    for (size_t i = 0; i < b->all_targets.count; i++)
        bld_da_push(&roots, b->all_targets.items[i]->exit);

    Bld_StepList order = bld__topo_sort(b, &roots);
    bld__build_steps(b, order);
}

/* --- bld/bld_cli.c --- */
/* bld/bld_cli.c — CLI parsing, help, self-recompilation, main() */


#ifdef __APPLE__
  #define BLD__HOST_OS BLD_OS_MACOS
#elif defined(__FreeBSD__)
  #define BLD__HOST_OS BLD_OS_FREEBSD
#elif defined(_WIN32)
  #define BLD__HOST_OS BLD_OS_WINDOWS
#else
  #define BLD__HOST_OS BLD_OS_LINUX
#endif

/* ---- CLI parsing ---- */

static void bld__parse_args(Bld* b) {
    Bld_Settings* s = &b->settings;
    Bld_Strs targets = {0};
    Bld_Strs passthrough = {0};
    bool after_dd = false;
    for (int i = 1; i < b->argc; i++) {
        const char* a = b->argv[i];
        if (after_dd) { bld_strs_push(&passthrough, a); continue; }
        if (strcmp(a, "--") == 0) { after_dd = true; continue; }
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0 || strcmp(a, "help") == 0) { s->show_help = true; continue; }
        if (strcmp(a, "-v") == 0 || strcmp(a, "--verbose") == 0) { s->verbose = true; continue; }
        if (strcmp(a, "-s") == 0 || strcmp(a, "--silent") == 0) { s->silent = true; continue; }
        if (strcmp(a, "--show-cached") == 0) { s->show_cached = true; continue; }
        if (strcmp(a, "-k") == 0 || strcmp(a, "--keep-going") == 0) { s->keep_going = true; continue; }
        if (strcmp(a, "--debug") == 0) { s->mode = BLD_MODE_DEBUG; continue; }
        if (strcmp(a, "--release") == 0) { s->mode = BLD_MODE_RELEASE; continue; }
        if (strcmp(a, "--prefix") == 0) {
            if (i + 1 >= b->argc) bld_panic("expected path after --prefix\n");
            s->install_prefix = b->argv[++i]; continue;
        }
        if (strcmp(a, "-j") == 0 || strcmp(a, "--jobs") == 0) {
            if (i + 1 >= b->argc) bld_panic("expected number after %s\n", a);
            s->max_jobs = atoi(b->argv[++i]); continue;
        }
        if (strncmp(a, "-j", 2) == 0 && a[2]) { s->max_jobs = atoi(a + 2); continue; }
        if (strncmp(a, "-D", 2) == 0) {
            const char* arg = a + 2;
            if (!arg[0] && i + 1 < b->argc) arg = b->argv[++i];
            const char* eq = strchr(arg, '=');
            Bld_UserOption entry = {0};
            if (eq) {
                size_t klen = (size_t)(eq - arg);
                char* k = bld_arena_alloc(klen + 1);
                memcpy(k, arg, klen); k[klen] = '\0';
                entry.key = k;
                entry.value = bld_str_dup(eq + 1);
            } else {
                entry.key = bld_str_dup(arg);
                entry.value = NULL;
            }
            bld_da_push(&b->user_options, entry);
            continue;
        }
        bld_strs_push(&targets, a);
    }
    s->passthrough = passthrough;
    s->targets = targets;
    if (s->max_jobs <= 0) { long n = sysconf(_SC_NPROCESSORS_ONLN); s->max_jobs = n > 0 ? (int)n : 1; }
    if (s->targets.len == 0) s->show_help = true;
}

/* ---- Help ---- */

static void bld__show_help(Bld* b) {
    const char* G = bld__c(BLD_C_GREEN);
    const char* Y = bld__c(BLD_C_YELLOW);
    const char* D = bld__c(BLD_C_DIM);
    const char* R = bld__c(BLD_C_RESET);

    bld_log("Usage: %s [options] [targets] [-- args]\n\n", b->argv[0]);

    bld_log("%sOptions:%s\n", Y, R);
    bld_log("  %s-h, --help%s       Show help\n", G, R);
    bld_log("  %s-v, --verbose%s    Verbose output\n", G, R);
    bld_log("  %s-s, --silent%s     Silent mode\n", G, R);
    bld_log("  %s--show-cached%s    Show cached steps\n", G, R);
    bld_log("  %s-k, --keep-going%s Continue after errors\n", G, R);
    bld_log("  %s--debug%s          Debug mode (-O0 -g)\n", G, R);
    bld_log("  %s--release%s        Release mode (-O2 -DNDEBUG)\n", G, R);
    bld_log("  %s--prefix <path>%s  Install prefix %s(default: build/)%s\n", G, R, D, R);
    bld_log("  %s-j <N>%s           Parallel jobs\n\n", G, R);

    bld_log("%sBuilt-in:%s\n", Y, R);
    bld_log("  %s%-20s%s %s\n", G, "build", R, "Build and install all targets");
    bld_log("  %s%-20s%s %s\n", G, "clean", R, "Remove cache and build directories");
    bld_log("  %s%-20s%s %s\n", G, "test", R, "Run all registered tests");

    if (b->avail_options.count > 0) {
        /* compute auto-width for option names */
        size_t max_name = 0;
        for (size_t i = 0; i < b->avail_options.count; i++) {
            size_t len = strlen(b->avail_options.items[i].name) + 2; /* -D prefix */
            if (len > max_name) max_name = len;
        }
        if (max_name < 20) max_name = 20;
        max_name += 2; /* padding */

        bld_log("\n%sProject options:%s\n", Y, R);
        for (size_t i = 0; i < b->avail_options.count; i++) {
            Bld_AvailableOption* o = &b->avail_options.items[i];
            const char* type = o->type == BLD_OPT_TYPE_BOOL ? "bool" : "string";
            const char* flag = bld_str_fmt("-D%s", o->name);
            bld_log("  %s%-*s%s %s %s[%s, default: %s]%s\n",
                    G, (int)max_name, flag, R,
                    o->description,
                    D, type, o->default_val, R);
        }
    }

    bld_log("\n%sTargets:%s\n", Y, R);
    for (size_t i = 0; i < b->all_targets.count; i++) {
        Bld_Target* t = b->all_targets.items[i];
        if (t->exit->silent) continue;
        bld_log("  %s%-20s%s %s\n", G, t->name, R, t->desc);
    }
}

static int bld__handle_clean(Bld* b) {
    for (size_t i = 0; i < b->settings.targets.len; i++) {
        if (strcmp(b->settings.targets.items[i], "clean") == 0) {
            bld_log_info("[*] Cleaning %s and %s...\n", b->cache.s, b->out.s);
            bld_fs_remove_all(b->cache);
            bld_fs_remove_all(b->out);
            bld_log_info("[*] Clean complete.\n");
            return 1;
        }
    }
    return 0;
}

/* ---- Test runner ---- */

static int bld__handle_test(Bld* b) {
    /* check if "test" is among requested targets */
    bool want_test = false;
    for (size_t i = 0; i < b->settings.targets.len; i++)
        if (strcmp(b->settings.targets.items[i], "test") == 0) { want_test = true; break; }
    if (!want_test) return 0;
    if (b->tests.count == 0) { bld_log("no tests registered\n"); return 1; }

    /* first build all test executables */
    Bld_StepList to_build = {0};
    for (size_t i = 0; i < b->tests.count; i++)
        bld_da_push(&to_build, b->tests.items[i].exe->exit);
    bld__check_missing_deps(b);
    Bld_StepList order = bld__topo_sort(b, &to_build);

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    bld__build_steps(b, order);

    if (b->steps_failed > 0) {
        bld_log_done(b->steps_executed, b->steps_cached, b->steps_failed, b->steps_skipped,
                     0, bld_arena_get()->offset);
        return 1;
    }

    /* create test output dir */
    Bld_Path test_dir = bld_path_join(b->cache, bld_path("test-output"));
    bld_fs_mkdir_p(test_dir);

    /* run tests — fork each, collect results */
    size_t passed = 0, failed = 0;
    struct timespec test_t0;
    clock_gettime(CLOCK_MONOTONIC, &test_t0);

    for (size_t i = 0; i < b->tests.count; i++) {
        Bld_TestEntry* te = &b->tests.items[i];
        Bld_Path exe_path = bld__target_artifact(b, te->exe);
        Bld_Path log_path = bld_path_join(test_dir, bld_path(bld_str_fmt("%s.log", te->name)));

        /* build command */
        Bld_Cmd cmd = {0};
        if (te->working_dir && te->working_dir[0])
            bld_cmd_appendf(&cmd, "cd \"%s\" && ", te->working_dir);
        bld_cmd_appendf(&cmd, "\"%s\"", exe_path.s);
        for (size_t ai = 0; ai < te->args.len; ai++) bld_cmd_appendf(&cmd, " \"%s\"", te->args.items[ai]);
        bld_cmd_appendf(&cmd, " > \"%s\" 2>&1", log_path.s);

        struct timespec st0;
        clock_gettime(CLOCK_MONOTONIC, &st0);

        int rc = system(cmd.items);

        struct timespec st1;
        clock_gettime(CLOCK_MONOTONIC, &st1);
        double elapsed = (double)(st1.tv_sec - st0.tv_sec) + (double)(st1.tv_nsec - st0.tv_nsec) / 1e9;

        if (rc == 0) {
            passed++;
        } else {
            failed++;
            bld_log("%sFAIL:%s %s (%.2fs)\n", bld__c(BLD_C_RED), bld__c(BLD_C_RESET), te->name, elapsed);
            /* show tail of output */
            const char* tail_cmd = bld_str_fmt("tail -50 \"%s\"", log_path.s);
            bld_log("%s", bld__c(BLD_C_DIM));
            if (system(tail_cmd)) { /* ignore tail errors */ }
            bld_log("%s", bld__c(BLD_C_RESET));
            bld_log("  full output: %s\n", log_path.s);
        }
    }

    struct timespec test_t1;
    clock_gettime(CLOCK_MONOTONIC, &test_t1);
    double total = (double)(test_t1.tv_sec - test_t0.tv_sec) + (double)(test_t1.tv_nsec - test_t0.tv_nsec) / 1e9;

    if (failed == 0)
        bld_log("%stests:%s %zu passed, %.2fs\n", bld__c(BLD_C_GREEN), bld__c(BLD_C_RESET), passed, total);
    else
        bld_log("%stests:%s %zu passed, %s%zu failed%s, %.2fs\n",
                bld__c(BLD_C_RED), bld__c(BLD_C_RESET), passed,
                bld__c(BLD_C_RED), failed, bld__c(BLD_C_RESET), total);

    return 1; /* handled — don't run normal build */
}

/* ---- Self-recompilation ---- */

static void bld__recompile_if_needed(Bld* b) {
    Bld_Path src = bld_path_join(b->root, bld_path("build.c"));
    Bld_Hash h = bld_hash_file(src);
    Bld_Path hdr = bld_path_join(b->root, bld_path("bld.h"));
    if (bld_fs_is_file(hdr)) h = bld_hash_combine(h, bld_hash_file(hdr));

    Bld_Path hp = bld_path_join(b->cache, bld_path("bld.hash"));
    if (bld_fs_exists(hp)) {
        size_t len;
        const char* content = bld_fs_read_file(hp, &len);
        uint64_t old = 0;
        if (sscanf(content, "%" SCNu64, &old) == 1 && old == h.value) return;
    }

    bld_log_info("[*] Recompiling build tool...\n");
    int rc = system(bld_str_fmt("%s -o \"%s\"", bld_recompile_cmd, b->argv[0]));
    if (rc != 0) bld_panic("failed to recompile build tool\n");
    const char* hs = bld_str_fmt("%" PRIu64, h.value);
    bld_fs_write_file(hp, hs, strlen(hs));
    execv(b->argv[0], b->argv);
    bld_fs_remove(hp);
    bld_panic("execv failed: %s\n", strerror(errno));
}

/* ---- Init stages ---- */

/* stage 1: core paths + cache dirs */
static void bld__init_core(Bld* b, int argc, char** argv) {
    memset(b, 0, sizeof(*b));
    b->argc = argc;
    b->argv = argv;
    b->root = bld_fs_realpath(bld_path_parent(bld_path(argv[0])));

    Bld_Path cr = bld_path_join(b->root, bld_path(".cache"));
    bld_fs_mkdir_p(cr);
    b->cache = bld_fs_realpath(cr);
    Bld_Path outr = bld_path_join(b->root, bld_path("build"));
    bld_fs_mkdir_p(outr);
    b->out = bld_fs_realpath(outr);

    bld_fs_mkdir_p(bld_path_join(b->cache, bld_path("arts")));
    bld_fs_mkdir_p(bld_path_join(b->cache, bld_path("deps")));
    bld_fs_mkdir_p(bld_path_join(b->cache, bld_path("tmp")));

    bld_fs_write_file(bld_path_join(b->cache, bld_path(".gitignore")), "*", 1);
    bld_fs_write_file(bld_path_join(b->out, bld_path(".gitignore")), "*", 1);

    /* Detect compilers from environment */
    const char *cc_env = getenv("CC"), *cc = cc_env ? cc_env : "cc";
    if (!bld__has_in_path(cc) && !cc_env)
        bld_panic("C compiler '%s' not found in PATH\n", cc);
    const char *cxx_env = getenv("CXX"), *cxx = cxx_env ? cxx_env : "c++";
    const char *as_env = getenv("AS"), *as_drv = as_env ? as_env : cc;

    /* Detect target OS from compiler triple */
    const char* dumpmachine_cmd = bld_str_fmt("%s -dumpmachine 2>/dev/null", cc);
    FILE* dm = popen(dumpmachine_cmd, "r");
    Bld_OsTarget target_os;
    if (dm) {
        char triple[256] = {0};
        if (fgets(triple, sizeof(triple), dm)) {
            size_t n = strlen(triple);
            if (n > 0 && triple[n-1] == '\n') triple[n-1] = '\0';
        }
        pclose(dm);
        target_os = triple[0] ? bld__detect_os_from_triple(triple) : BLD__HOST_OS;
    } else {
        target_os = BLD__HOST_OS;
    }

    /* Create toolchain with detected tools */
    b->toolchain = bld_toolchain_gcc(target_os);
    b->toolchain->compilers[0] = (Bld_Compiler){.lang = BLD_LANG_C, .driver = cc,
                                      .identity_hash = bld__make_identity_hash(cc), .available = true};
    b->toolchain->compilers[1] = (Bld_Compiler){.lang = BLD_LANG_CXX, .driver = cxx,
                                      .identity_hash = bld__make_identity_hash(cxx),
                                      .available = bld__has_in_path(cxx) || cxx_env != NULL};
    b->toolchain->compilers[2] = (Bld_Compiler){.lang = BLD_LANG_ASM, .driver = as_drv,
                                      .identity_hash = bld__make_identity_hash(as_drv), .available = true};
    b->global_warnings = true;
}

/* stage 2: CLI parsing + prefix override */
static void bld__init_cli(Bld* b) {
    bld__parse_args(b);
    if (b->settings.install_prefix) {
        bld_fs_mkdir_p(bld_path(b->settings.install_prefix));
        b->out = bld_fs_realpath(bld_path(b->settings.install_prefix));
    }
}

/* stage 3: built-in default target */
static void bld__init_builtin_targets(Bld* b) {
    b->target_default = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, b->target_default, BLD_TGT_CUSTOM, "build", "Build and install all targets");
    b->target_default->exit->silent = true;
}

/* full init for main() */
static void bld__init(Bld* b, int argc, char** argv) {
    bld__init_core(b, argc, argv);
    bld__init_cli(b);
    bld__init_builtin_targets(b);
}

/* ---- compile_commands.json ---- */

static void bld__escape_json(Bld_Cmd* out, const char* s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { char tmp[3] = {'\\', *s, 0}; bld_cmd_appendf(out, "%s", tmp); }
        else { char tmp[2] = {*s, 0}; bld_cmd_appendf(out, "%s", tmp); }
    }
}

static void bld__write_compdb(Bld* b) {
    Bld_Cmd json = {0};
    bld_cmd_appendf(&json, "[\n");
    bool first = true;

    for (size_t i = 0; i < b->all_targets.count; i++) {
        Bld_Target* t = b->all_targets.items[i];
        if (t->kind != BLD_TGT_EXE && t->kind != BLD_TGT_LIB) continue;

        Bld_StepList* obj_steps = (t->kind == BLD_TGT_EXE)
            ? &((Bld_Exe*)t)->obj_steps
            : &((Bld_Lib*)t)->obj_steps;

        for (size_t j = 0; j < obj_steps->count; j++) {
            Bld_Step* step = obj_steps->items[j];
            if (!step->action_ctx) continue;
            Bld__ObjCtx* ctx = (Bld__ObjCtx*)step->action_ctx;

            Bld_Cmd cmd = {0};
            bld__render_obj_cmd(&cmd, ctx);

            if (!first) bld_cmd_appendf(&json, ",\n");
            first = false;

            bld_cmd_appendf(&json, "  { \"directory\": \"");
            bld__escape_json(&json, b->root.s);
            bld_cmd_appendf(&json, "\", \"file\": \"");
            bld__escape_json(&json, ctx->source.s);
            bld_cmd_appendf(&json, "\", \"command\": \"");
            bld__escape_json(&json, cmd.items);
            bld_cmd_appendf(&json, "\" }");
        }
    }

    bld_cmd_appendf(&json, "\n]\n");
    Bld_Path out = bld_path_join(b->root, bld_path("compile_commands.json"));
    bld_fs_write_file(out, json.items, json.count);
}

/* ---- main ---- */

int main(int argc, char** argv) {
    Bld b;
    bld__init(&b, argc, argv);

    /* clean tmp once per process */
    Bld_Path tmp = bld_path_join(b.cache, bld_path("tmp"));
    bld_fs_remove_all(tmp);
    bld_fs_mkdir_p(tmp);

    bld__recompile_if_needed(&b);
    if (bld__handle_clean(&b)) return 0;
    configure(&b);

    /* validate: error on unknown -D options */
    for (size_t i = 0; i < b.user_options.count; i++) {
        if (!b.user_options.items[i].used)
            bld_panic("unknown option: -D%s\n", b.user_options.items[i].key);
    }

    bld__materialize_lazy_sources(&b);
    bld__resolve_link_deps(&b);
    bld__write_compdb(&b);
    if (b.settings.show_help) { bld__show_help(&b); return 0; }
    if (bld__handle_test(&b)) return b.steps_failed > 0 ? 1 : 0;
    bld__run_build(&b);
    return 0;
}

#endif /* BLD_IMPLEMENTATION */

/* strip prefixes */
#ifdef BLD_STRIP_PREFIX

/* types */
#define Arena       Bld_Arena
#define Path        Bld_Path
#define Cmd         Bld_Cmd
#define Hash        Bld_Hash
#define PathList    Bld_PathList
#define Target      Bld_Target
#define Step        Bld_Step
#define LazyPath    Bld_LazyPath
#define Exe         Bld_Exe
#define Lib         Bld_Lib
#define ExeOpts     Bld_ExeOpts
#define LibOpts     Bld_LibOpts
#define RunOpts     Bld_RunOpts
#define StepOpts    Bld_StepOpts
#define CmdOpts     Bld_CmdOpts
#define Strs        Bld_Strs
#define Paths       Bld_Paths
#define BuildFlags  Bld_BuildFlags
#define CompileFlags Bld_CompileFlags
#define LinkFlags   Bld_LinkFlags
#define Optimize    Bld_Optimize
#define Lang        Bld_Lang
#define CStd        Bld_CStd
#define CxxStd      Bld_CxxStd
#define Compiler    Bld_Compiler
#define Toggle      Bld_Toggle
#define OsTarget    Bld_OsTarget
#define Tool        Bld_Tool
#define Toolchain   Bld_Toolchain
#define OS_LINUX    BLD_OS_LINUX
#define OS_MACOS    BLD_OS_MACOS
#define OS_WINDOWS  BLD_OS_WINDOWS
#define OS_FREEBSD  BLD_OS_FREEBSD
#define ActionFn    Bld_ActionFn
#define HashFn       Bld_HashFn
#define TargetKind  Bld_TargetKind

/* enums */
#define OPT_DEFAULT BLD_OPT_DEFAULT
#define OPT_O0     BLD_OPT_O0
#define OPT_O1     BLD_OPT_O1
#define OPT_O2     BLD_OPT_O2
#define OPT_O3     BLD_OPT_O3
#define OPT_Os     BLD_OPT_Os
#define OPT_OFAST  BLD_OPT_OFAST
#define LANG_AUTO  BLD_LANG_AUTO
#define LANG_C     BLD_LANG_C
#define LANG_CXX   BLD_LANG_CXX
#define LANG_ASM   BLD_LANG_ASM
#define C_DEFAULT  BLD_C_DEFAULT
#define C_90       BLD_C_90
#define C_99       BLD_C_99
#define C_11       BLD_C_11
#define C_17       BLD_C_17
#define C_23       BLD_C_23
#define C_GNU90    BLD_C_GNU90
#define C_GNU99    BLD_C_GNU99
#define C_GNU11    BLD_C_GNU11
#define C_GNU17    BLD_C_GNU17
#define C_GNU23    BLD_C_GNU23
#define CXX_11     BLD_CXX_11
#define CXX_14     BLD_CXX_14
#define CXX_17     BLD_CXX_17
#define CXX_20     BLD_CXX_20
#define CXX_23     BLD_CXX_23
#define CXX_GNU11  BLD_CXX_GNU11
#define CXX_GNU14  BLD_CXX_GNU14
#define CXX_GNU17  BLD_CXX_GNU17
#define CXX_GNU20  BLD_CXX_GNU20
#define CXX_GNU23  BLD_CXX_GNU23
#define TOGGLE_UNSET BLD_UNSET
#define TOGGLE_ON    BLD_ON
#define TOGGLE_OFF   BLD_OFF
#define TGT_EXE    BLD_TGT_EXE
#define TGT_LIB    BLD_TGT_LIB
#define TGT_CUSTOM BLD_TGT_CUSTOM

/* macros */
#define STRS(...)  BLD_STRS(__VA_ARGS__)
#define PATHS(...) BLD_PATHS(__VA_ARGS__)

/* arena */
#define arena_get     bld_arena_get
#define arena_alloc   bld_arena_alloc
#define arena_realloc bld_arena_realloc

/* str */
#define str_fmt  bld_str_fmt
#define str_dup    bld_str_dup
#define str_cat    bld_str_cat
#define str_lines  bld_str_lines
#define str_join   bld_str_join
#define strs_push    bld_strs_push
#define paths_push   bld_paths_push
#define strs_merge   bld_strs_merge
#define paths_merge  bld_paths_merge

/* path — not stripped: path/path_join etc. are too generic */

/* fs */
#define fs_exists       bld_fs_exists
#define fs_is_dir       bld_fs_is_dir
#define fs_is_file      bld_fs_is_file
#define fs_mkdir_p      bld_fs_mkdir_p
#define fs_remove       bld_fs_remove
#define fs_remove_all   bld_fs_remove_all
#define fs_rename       bld_fs_rename
#define fs_copy_file    bld_fs_copy_file
#define fs_copy_r       bld_fs_copy_r
#define fs_realpath     bld_fs_realpath
#define fs_getcwd       bld_fs_getcwd
#define fs_list_files_r bld_fs_list_files_r
#define fs_read_file    bld_fs_read_file
#define fs_write_file   bld_fs_write_file
#define files_glob      bld_files_glob
#define files_exclude   bld_files_exclude
#define files_merge     bld_files_merge

/* log — not stripped: "log" and "panic" conflict with common names */

/* cmd */
#define cmd_appendf   bld_cmd_appendf
#define cmd_append_sq bld_cmd_append_sq

/* hash */
#define hash_combine           bld_hash_combine
#define hash_combine_unordered bld_hash_combine_unordered
#define hash_str               bld_hash_str
#define hash_file              bld_hash_file
#define hash_dir               bld_hash_dir

/* toolchain */
#define toolchain_gcc  bld_toolchain_gcc

/* compiler setters */
#define set_compiler_c(b, ...)   bld_set_compiler_c((b), __VA_ARGS__)
#define set_compiler_cxx(b, ...) bld_set_compiler_cxx((b), __VA_ARGS__)
#define set_compiler_asm(b, ...) bld_set_compiler_asm((b), __VA_ARGS__)

/* build api */
#define add_exe(b, ...)          bld_add_exe((b), __VA_ARGS__)
#define add_lib(b, ...)          bld_add_lib((b), __VA_ARGS__)
#define add_step(b, ...)         bld_add_step((b), __VA_ARGS__)
#define add_cmd(b, ...)          bld_add_cmd((b), __VA_ARGS__)
#define add_run(b, t, ...)       bld_add_run((b), (t), __VA_ARGS__)
#define depends_on               bld_depends_on
#define link_with                bld_link_with
#define target_output            bld_output
#define target_output_sub        bld_output_sub
#define target_add_include_dir   bld_add_include_dir
#define target_add_source(t, s)  bld_add_source((t), (s))
#define install_exe              bld_install_exe
#define install_lib              bld_install_lib
#define install_target           bld_install
#define install_files            bld_install_files
#define install_dir              bld_install_dir
#define add_test(b, t, ...)      bld_add_test((b), (t), __VA_ARGS__)
#define use_dep                  bld_use_dep
#define override_file(t, f, ...) bld_override_file((t), (f), __VA_ARGS__)
#define find_pkg                 bld_find_pkg
#define clone_compile_flags      bld_clone_compile_flags
#define default_compile_flags    bld_default_compile_flags
#define option_bool              bld_option_bool
#define option_str               bld_option_str
#define target_ok                bld_target_ok
#define target_artifact          bld_target_artifact

/* feature checks */
#define checks_new               bld_checks_new
#define checks_header            bld_checks_header
#define checks_func              bld_checks_func
#define checks_sizeof            bld_checks_sizeof
#define checks_compile           bld_checks_compile
#define checks_run               bld_checks_run
#define checks_write             bld_checks_write

#endif /* BLD_STRIP_PREFIX */