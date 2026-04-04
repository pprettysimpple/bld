# bld.h API Reference

> Single-header C build system. Write build.c, include bld.h, implement configure().

## Quick Start

Every build script follows this pattern:

```c
#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX   // optional: use add_exe instead of bld_add_exe
#include "bld.h"

BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    // define targets here
}
```

Bootstrap and run:

    cc -std=c11 -w build.c -o b -lpthread
    ./b build              # build and install all targets
    ./b test               # run registered tests
    ./b build --prefix /usr/local  # custom install prefix
    ./b build -v           # verbose: show compiler commands
    ./b build -Dfoo=on     # pass user option
    ./b --help             # list targets and options

bld auto-recompiles itself when build.c changes.

## Targets

### Executable

```c
Bld_Target* exe = bld_add_exe(b,
    .name    = "myapp",
    .desc    = "My application",
    .sources = BLD_PATHS("src/main.c", "src/util.c"),
    .compile = { .optimize = BLD_OPT_O2, .warnings = BLD_ON },
    .link    = { .libs = BLD_STRS("m", "pthread") });
```

### Static Library

```c
Bld_Target* lib = bld_add_lib(b,
    .name    = "mylib",
    .sources = BLD_PATHS("src/foo.c", "src/bar.c"),
    .compile           = { .optimize = BLD_OPT_O2 },
    .compile_propagate = { .include_dirs = BLD_PATHS("include") });
```

`compile` flags apply when compiling this lib's sources.
`compile_propagate` flags propagate to any target that calls `bld_link_with(target, lib)`.

### Shared Library

```c
Bld_Target* so = bld_add_lib(b,
    .name    = "mylib",
    .shared  = true,
    .sources = BLD_PATHS("src/foo.c", "src/bar.c"));
```

### Exe Opts Fields

- `.name` (required) — target identifier. Automatically prefixed: `lib:name` / `exe:name`.
  A lib and exe may share the same `.name` (e.g. both "lz4" → `lib:lz4` + `exe:lz4`)
- `.desc` — shown in `--help`
- `.output_name` — override output filename (default: `.name`)
- `.sources` — `Bld_Paths` of source files
- `.lang` — force language: `BLD_LANG_C`, `BLD_LANG_CXX`, `BLD_LANG_ASM`, or `BLD_LANG_AUTO` (default, detect by extension)
- `.compile` — `Bld_CompileFlags` (private compile flags)
- `.compile_propagate` — `Bld_CompileFlags` (public, propagated via `link_with`)
- `.link` — `Bld_LinkFlags` (private link flags)
- `.link_propagate` — `Bld_LinkFlags` (public, propagated via `link_with`)
- `.toolchain` — override per-target toolchain (default: `b->toolchain`)

### Lib Opts Fields

Same as Exe, except:
- `.lib_basename` — override library filename base (default: `.name`).
  Toolchain adds prefix/suffix automatically (`lib` + `.a`/`.so`/`.dylib`).
  Example: `.lib_basename = "z"` produces `libz.a`.
- `.compile_propagate` — public compile flags, propagated to consumers via `link_with`
- `.link_propagate` — public link flags, propagated to consumers via `link_with`
- `.shared` — `true` for shared library, `false` (default) for static

## Source Files

### Literal Lists

```c
.sources = BLD_PATHS("src/main.c", "src/util.c")
```

`BLD_PATHS` and `BLD_STRS` are arena-allocated — safe to store and use later.

### Globbing

```c
Bld_Paths all      = bld_files_glob("src/*.c");          // non-recursive
Bld_Paths deep     = bld_files_glob("src/**/*.c");       // recursive into subdirs
Bld_Paths filtered = bld_files_exclude(all, BLD_PATHS("src/test_main.c"));
Bld_Paths combined = bld_files_merge(lib_srcs, extra_srcs);
```

`bld_files_glob` supports `*` and `?` wildcards. Use `**` for recursive matching
(e.g. `"lib/**/*.c"` finds all `.c` files under `lib/` at any depth).
Patterns without a directory prefix (e.g. `"*.c"`) search the current directory.

### Dynamic Construction

```c
Bld_Paths srcs = {0};
bld_paths_push(&srcs, "src/main.c");
bld_paths_push(&srcs, "src/util.c");
if (use_ssl) bld_paths_push(&srcs, "src/ssl.c");
```

## Typed Slices: Bld_Strs and Bld_Paths

Two sized slice types for strings and paths:

```c
Bld_Strs  — for defines, library names, arguments
Bld_Paths — for file paths, directory paths

// literal (arena-allocated, safe to store)
BLD_STRS("FOO=1", "BAR=2")
BLD_PATHS("src/a.c", "src/b.c")

// dynamic (arena-backed, growable)
Bld_Strs defs = {0};
bld_strs_push(&defs, "FOO=1");

Bld_Paths dirs = {0};
bld_paths_push(&dirs, "/usr/include");

// merge two slices
Bld_Strs  all_defs  = bld_strs_merge(a, b);
Bld_Paths all_paths = bld_paths_merge(a, b);
```

All slices are arena-backed with `{ .items, .len, .cap }`.

## Compile Flags

```c
Bld_CompileFlags flags = {
    .optimize    = BLD_OPT_O2,       // O0, O1, O2, O3, Os, OFAST
    .warnings    = BLD_ON,           // ON, OFF, or UNSET (inherit global)
    .debug_info  = BLD_ON,           // -g
    .extra_flags = "-fno-strict-aliasing",
    .defines          = BLD_STRS("VERSION=2", "DEBUG"),
    .include_dirs     = BLD_PATHS("src", "vendor"),
    .system_include_dirs = BLD_PATHS("/opt/sdk/include"),
};
```

### Reusing and Modifying Flags

```c
Bld_CompileFlags base = bld_default_compile_flags(b);  // mode-aware defaults
base.optimize = BLD_OPT_O3;

Bld_CompileFlags copy = bld_clone_compile_flags(base);  // deep copy
copy.include_dirs = BLD_PATHS("extra");
```

`bld_default_compile_flags` returns sensible defaults based on build mode:
- `--release`: O2, warnings on
- `--debug`: O0, debug_info on, warnings on
- default: O0, warnings on

## Link Flags

```c
Bld_LinkFlags lflags = {
    .libs        = BLD_STRS("ssl", "crypto"),   // -l names
    .lib_dirs    = BLD_PATHS("/opt/openssl/lib"), // -L paths
    .extra_flags = "-Wl,--as-needed",
};
```

## Build Flags (Global)

```c
b->build_flags.asan = BLD_ON;   // -fsanitize=address for all targets
b->build_flags.lto  = BLD_ON;   // -flto for all targets
```

These apply globally to all compile and link commands.

## Compiler Setup

```c
bld_set_compiler_c(b,   .driver = "gcc",   .standard = BLD_C_11);
bld_set_compiler_cxx(b, .driver = "g++",   .standard = BLD_CXX_17);
bld_set_compiler_asm(b, .driver = "nasm");
```

If `.driver` is omitted, bld auto-detects from PATH.
Standards: `BLD_C_90/99/11/17/23`, `BLD_C_GNU90/99/11/17/23`,
`BLD_CXX_11/14/17/20/23`, `BLD_CXX_GNU11/14/17/20/23`.

## Linking Targets Together

```c
bld_link_with(exe, lib);       // link lib into exe + propagate compile/link_propagate
bld_depends_on(gen, codegen);  // ordering only, no artifact passing
```

`bld_link_with` is transitive: if A links B and B links C, A gets C's propagated flags.

## External Dependencies

### pkg-config

```c
Bld_Dep* ssl = bld_find_pkg("openssl");
if (ssl->found) {
    bld_use_dep(lib, ssl);   // adds include dirs, -l flags, -L flags
}
```

### Manual

```c
Bld_Dep* my = bld_dep(
    .name         = "mylib",
    .include_dirs = BLD_PATHS("/opt/mylib/include"),
    .libs         = BLD_STRS("mylib"),
    .lib_dirs     = BLD_PATHS("/opt/mylib/lib"));
bld_use_dep(target, my);
```

`bld_use_dep` applies compile flags (include dirs) privately and link flags transitively.

## Feature Detection

```c
Bld_Checks* chk = bld_checks_new(b);

// header existence
bool* has_unistd  = bld_checks_header(chk, "HAVE_UNISTD_H", "unistd.h");
bool* has_windows = bld_checks_header(chk, "HAVE_WINDOWS_H", "windows.h");

// function existence (provide header that declares it)
bool* has_strtoll = bld_checks_func(chk, "HAVE_STRTOLL", "strtoll", "stdlib.h");

// type sizes (cross-compilation safe: binary scan, no execution)
int* sz_long = bld_checks_sizeof(chk, "SIZEOF_LONG", "long");
int* sz_ptr  = bld_checks_sizeof(chk, "SIZEOF_VOID_P", "void*");

// arbitrary compile test
bool* has_builtin = bld_checks_compile(chk, "HAS_BUILTIN_EXPECT",
    "int main(){return __builtin_expect(0,0);}");

// execute all checks in parallel
bld_checks_run(chk);

// now pointers are valid:
if (*has_unistd) { // ... }
int long_size = *sz_long;  // e.g. 8

// write results as #define / #undef to a header
bld_checks_write(chk, "generated/config.h");
// produces:
//   #define HAVE_UNISTD_H 1
//   #undef  HAVE_WINDOWS_H
//   #define SIZEOF_LONG 8
```

## Platform Detection

Use `b->toolchain->os` to branch on the target platform:

```c
if (b->toolchain->os == BLD_OS_LINUX) {
    bld_paths_push(&srcs, "src/platform_linux.c");
} else if (b->toolchain->os == BLD_OS_MACOS) {
    bld_paths_push(&srcs, "src/platform_macos.c");
}
```

Available: `BLD_OS_LINUX`, `BLD_OS_MACOS`, `BLD_OS_WINDOWS`, `BLD_OS_FREEBSD`.

## Installation

```c
bld_install_exe(b, exe);     // installs to <prefix>/bin/
bld_install_lib(b, lib);     // installs to <prefix>/lib/

// install specific files to a subpath under prefix
bld_install_files(b, BLD_PATHS("include/mylib.h"), bld_path("include"));

// install entire directory tree
bld_install_dir(b, "doc", bld_path("share/doc/mylib"));

// install any target artifact to custom path
bld_install(b, codegen_target, bld_path("share/myapp"));
```

`./b build` compiles, links, and installs everything.
Default install prefix is `./build/` (relative to project root, not `/usr/local`).
Override with `--prefix`: `./b build --prefix /usr/local`

## Tests

```c
bld_add_test(b, exe_target,
    .name = "smoke-test",
    .desc = "Verify basic functionality",
    .args = BLD_STRS("--self-test"));
```

CLI: `./b test`

## User Options

```c
bool use_ssl = bld_option_bool(b, "ssl", "Enable SSL support", true);
const char* backend = bld_option_str(b, "backend", "TLS backend", "openssl");
```

CLI: `./b build -Dssl=off -Dbackend=wolfssl`
Options appear in `./b --help`.

## Per-file Compile Flag Overrides

```c
bld_override_file(lib, "third_party/noisy.c", .warnings = BLD_OFF);
bld_override_file(lib, "hot_path.c", .optimize = BLD_OPT_O3);
```

Non-zero fields override the target's compile flags for that specific file.

## Custom Steps

### Action Function

```c
static Bld_ActionResult my_codegen(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)depfile;
    Bld* b = ctx;
    // generate code, write to output directory
    bld_fs_mkdir_p(output);
    const char* code = "#include <stdio.h>\nint gen(void){return 42;}\n";
    bld_fs_write_file(bld_path_join(output, bld_path("gen.c")), code, strlen(code));
    return BLD_ACTION_OK;
}

void configure(Bld* b) {
    Bld_Target* gen = bld_add_step(b,
        .name   = "codegen",
        .desc   = "Generate source files",
        .action = my_codegen,
        .action_ctx = b,
        .watch  = BLD_PATHS("schema.json"));  // re-run when these change

    Bld_Target* exe = bld_add_exe(b, .name = "app", .sources = BLD_PATHS("main.c"));
    bld_add_source(exe, bld_output_sub(gen, "gen.c"));  // add generated source
    bld_add_include_dir(exe, bld_output(gen));           // add generated dir to -I
}
```

### Shell Command Step

```c
Bld_Target* gen = bld_add_cmd(b,
    .name  = "protoc",
    .desc  = "Generate protobuf sources",
    .cmd   = "protoc --c_out=generated/ schema.proto",
    .watch = BLD_PATHS("schema.proto"));
```

### Running a Built Target

```c
Bld_Target* tool = bld_add_exe(b, .name = "tool", .sources = BLD_PATHS("tool.c"));
Bld_Target* run  = bld_add_run(b, tool,
    .name = "run-tool",
    .desc = "Run the tool",
    .args = BLD_STRS("--generate", "output.c"));
```

## Toolchain

```c
Bld_Toolchain* tc = bld_toolchain_gcc(BLD_OS_LINUX);
b->toolchain = tc;  // set as default for all targets
```

OS targets: `BLD_OS_LINUX`, `BLD_OS_MACOS`, `BLD_OS_WINDOWS`, `BLD_OS_FREEBSD`.

Per-target override:

```c
bld_add_exe(b, .name = "cross", .sources = ..., .toolchain = my_cross_tc);
```

## Filesystem Helpers

```c
bld_fs_exists(bld_path("file.c"))     // returns bool
bld_fs_is_dir(bld_path("src"))
bld_fs_is_file(bld_path("main.c"))
bld_fs_mkdir_p(bld_path("out/gen"))   // recursive mkdir
bld_fs_copy_file(from, to)
bld_fs_copy_r(from, to)              // recursive copy
bld_fs_remove(path)
bld_fs_remove_all(path)              // recursive remove
bld_fs_rename(from, to)
bld_fs_read_file(path, &len)         // returns arena-allocated content
bld_fs_write_file(path, data, len)
bld_fs_realpath(path)
bld_fs_getcwd()
```

## String Helpers

```c
bld_str_fmt("hello %s", name)   // arena-allocated sprintf
bld_str_dup("copy me")          // arena-allocated strdup
bld_str_cat("a", "b", "c", NULL)  // concatenate (NULL sentinel)
bld_str_lines(text)             // split into Bld_Strs by newline
bld_str_join(&strs, ", ")      // join Bld_Strs with separator
```

## Complete Example: Library + Executable + Tests

```c
#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"

BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    bld_set_compiler_c(b, .standard = BLD_C_11);

    Bld_CompileFlags cflags = bld_default_compile_flags(b);
    cflags.optimize = BLD_OPT_O2;

    // static library
    Bld_Paths lib_srcs = bld_files_glob("src/lib/*.c");
    Bld_Target* lib = bld_add_lib(b,
        .name              = "mylib",
        .sources           = lib_srcs,
        .compile           = cflags,
        .compile_propagate = { .include_dirs = BLD_PATHS("include") });

    // executable
    Bld_Target* app = bld_add_exe(b,
        .name    = "myapp",
        .sources = BLD_PATHS("src/main.c"),
        .compile = cflags,
        .link    = { .libs = BLD_STRS("m") });
    bld_link_with(app, lib);

    // test
    Bld_Target* test_exe = bld_add_exe(b,
        .name    = "test-runner",
        .sources = BLD_PATHS("tests/test_main.c"),
        .compile = cflags);
    bld_link_with(test_exe, lib);
    bld_add_test(b, test_exe, .name = "unit-tests");

    // install
    bld_install_exe(b, app);
    bld_install_lib(b, lib);
    bld_install_files(b, BLD_PATHS("include/mylib.h"), bld_path("include"));
}
```

## Complete Example: Feature Detection + External Deps

```c
#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"

BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    bld_set_compiler_c(b, .standard = BLD_C_99);

    // feature detection
    Bld_Checks* chk = bld_checks_new(b);
    bld_checks_header(chk, "HAVE_UNISTD_H", "unistd.h");
    bld_checks_header(chk, "HAVE_SYS_SOCKET_H", "sys/socket.h");
    bld_checks_func(chk, "HAVE_STRLCPY", "strlcpy", "string.h");
    int* sz_long = bld_checks_sizeof(chk, "SIZEOF_LONG", "long");
    bld_checks_run(chk);
    bld_checks_write(chk, "generated/config.h");

    // external deps
    Bld_Dep* ssl  = bld_find_pkg("openssl");
    Bld_Dep* zlib = bld_find_pkg("zlib");

    // library
    Bld_Paths srcs = bld_files_glob("lib/*.c");
    Bld_CompileFlags cf = bld_default_compile_flags(b);
    cf.defines = BLD_STRS("HAVE_CONFIG_H");
    cf.include_dirs = BLD_PATHS("include", "lib", "generated");

    Bld_Target* lib = bld_add_lib(b,
        .name    = "mynet",
        .sources = srcs,
        .compile = cf);
    if (ssl->found)  bld_use_dep(lib, ssl);
    if (zlib->found) bld_use_dep(lib, zlib);

    // executable
    Bld_Target* exe = bld_add_exe(b,
        .name    = "mynet-cli",
        .sources = BLD_PATHS("src/main.c"),
        .compile = cf,
        .link    = { .libs = BLD_STRS("m", "pthread") });
    bld_link_with(exe, lib);
}
```
