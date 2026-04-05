# bld.h API Reference

> Single-header C build system. Write build.c, include bld.h, implement configure().

## Get bld.h

Download from the [latest release](https://github.com/nicro950/bld/releases/latest)
and drop `bld.h` into your project directory. That's it — no install, no dependencies.

## Quick Start

**1. Create `build.c`:**

```c
#define BLD_IMPLEMENTATION     // include the implementation (once per project)
#define BLD_STRIP_PREFIX       // optional: write add_exe() instead of bld_add_exe()
#include "bld.h"

// how to recompile this build script (bld adds -o automatically)
BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    // define your targets here
}
```

**2. Bootstrap** (compile build.c into the build tool):

    cc -std=c11 -w build.c -o b -lpthread

`-w` suppresses warnings inside bld.h. `-lpthread` is needed for parallel builds.
You only run this once — after that, bld recompiles itself when build.c changes.

**3. Build:**

    ./b build              # build and install all targets
    ./b test               # run registered tests
    ./b build --release    # optimized build
    ./b build -v           # verbose: show compiler commands
    ./b build -Dfoo=on     # pass user option
    ./b --help             # list targets and options

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
    .compile_pub = { .include_dirs = BLD_PATHS("include") });
```

`compile` flags apply when compiling this lib's sources.
`compile_pub` flags are public: passed to any target that calls `bld_link_with(target, lib)`.

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
- `.compile_pub` — `Bld_CompileFlags` (public, propagated via `link_with`)
- `.link` — `Bld_LinkFlags` (private link flags)
- `.link_pub` — `Bld_LinkFlags` (public, propagated via `link_with`)
- `.toolchain` — override per-target toolchain (default: `b->toolchain`)

### Lib Opts Fields

Same as Exe, except:
- `.lib_basename` — override library filename base (default: `.name`).
  Toolchain adds prefix/suffix automatically (`lib` + `.a`/`.so`/`.dylib`).
  Example: `.lib_basename = "z"` produces `libz.a`.
- `.compile_pub` — public compile flags, propagated to consumers via `link_with`
- `.link_pub` — public link flags, propagated to consumers via `link_with`
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
Bld_Paths no_tests = bld_files_exclude(all, BLD_PATHS("*_test.c"));  // glob pattern on basename
Bld_Paths combined = bld_files_merge(lib_srcs, extra_srcs);
```

`bld_files_glob` supports `*` and `?` wildcards. Use `**` for recursive matching
(e.g. `"lib/**/*.c"` finds all `.c` files under `lib/` at any depth).
Patterns without a directory prefix (e.g. `"*.c"`) search the current directory.

`bld_files_exclude` accepts exact paths, glob patterns, or directory prefixes:
- `"src/test_main.c"` — exact path match
- `"*_test.c"` — glob pattern matched against basename (if contains `*`, `?`, `[`)
- `"src/win"` — directory prefix: excludes all files under `src/win/`

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

All slices are arena-backed with `{ .items, .count, .cap }`.

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

Defines must be simple tokens: `FOO`, `FOO=1`, `_GNU_SOURCE`.
For values with spaces or quotes (like `VERSION="1.0"`), use a generated header:

```c
bld_fs_write_str(bld_filepath("generated/version.h"),
    "#define VERSION \"1.0\"\n");
```

This avoids cross-platform shell escaping issues.

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

Note: strict standards (`BLD_C_11`) disable POSIX/GNU extensions. If your code uses
`strdup`, `fdopen`, `realpath`, etc., use `BLD_C_GNU11` or add `_GNU_SOURCE` to defines.

Mixed C/C++ builds: when a target contains `.cpp`/`.cxx`/`.cc` sources, bld
automatically uses the C++ compiler as the linker driver. No need to add
`-lstdc++` manually.

## Linking Targets Together

```c
bld_link_with(exe, lib);       // link lib into exe + propagate compile/link_pub
bld_depends_on(gen, codegen);  // ordering only, no artifact passing
```

`bld_link_with` is transitive: if A links B and B links C, A gets C's propagated flags.

Use `link_pub` on a library for system libs that consumers must also link
(e.g. `.link_pub = { .libs = BLD_STRS("m", "dl") }` on a lib that uses `libm`/`libdl`).

## External Dependencies

External dependencies use the same `bld_link_with` as internal targets.
`bld_find_pkg` returns a `Bld_Target*` (never NULL). Check `.found` for optional deps.

### pkg-config

```c
Bld_Target* ssl = bld_find_pkg(b, "openssl");
if (ssl->found) {
    bld_link_with(lib, ssl);   // adds include dirs, -l flags, -L flags
}
// or, for required deps (panics if not found):
bld_link_with(lib, bld_find_pkg(b, "zlib"));
```

### Manual

```c
Bld_Target* my = bld_pkg(b,
    .name         = "mylib",
    .include_dirs = BLD_PATHS("/opt/mylib/include"),
    .libs         = BLD_STRS("mylib"),
    .lib_dirs     = BLD_PATHS("/opt/mylib/lib"));
bld_link_with(target, my);
```

`bld_link_with` propagates compile flags (include dirs) and link flags transitively,
whether the dependency is an internal library, a pkg-config result, or a manual package.

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

All install paths are relative to `--prefix` (default: `build/`).

```c
bld_install_exe(b, exe);     // → <prefix>/bin/<name>
bld_install_lib(b, lib);     // → <prefix>/lib/lib<name>.a

// install specific files to a subpath under prefix
bld_install_files(b, BLD_PATHS("include/mylib.h"), bld_filepath("include"));

// install entire directory tree (recursive copy)
bld_install_dir(b, "doc", bld_filepath("share/doc/mylib"));

// install any target artifact to custom path
bld_install(b, codegen_target, bld_filepath("share/myapp"));
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
bld_override_file(lib, "vendor/dtoa.c", .extra_flags = "-DIEEE_8087");
```

Non-zero fields override the target's compile flags for that specific file.
Available override fields: `.warnings`, `.optimize`, `.extra_flags`, `.debug_info`.

## Custom Steps

### Action Function

```c
static Bld_ActionResult my_codegen(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)depfile;
    Bld* b = ctx;
    // generate code, write to output directory
    bld_fs_mkdir_p(output);
    const char* code = "#include <stdio.h>\nint gen(void){return 42;}\n";
    bld_fs_write_str(bld_path_join(output, bld_filepath("gen.c")), code);
    return BLD_ACTION_OK;  // or BLD_ACTION_FAILED on error
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

```c
Bld_Toolchain* tc = bld_toolchain_msvc();
b->toolchain = tc;  // MSVC cl.exe/lib.exe toolchain (Windows)
```

OS targets: `BLD_OS_LINUX`, `BLD_OS_MACOS`, `BLD_OS_WINDOWS`, `BLD_OS_FREEBSD`.

Per-target override:

```c
bld_add_exe(b, .name = "cross", .sources = ..., .toolchain = my_cross_tc);
```

## Filesystem Helpers

```c
bld_fs_exists(bld_filepath("file.c"))     // returns bool
bld_fs_is_dir(bld_filepath("src"))
bld_fs_is_file(bld_filepath("main.c"))
bld_fs_mkdir_p(bld_filepath("out/gen"))   // recursive mkdir
bld_fs_copy_file(from, to)
bld_fs_copy_r(from, to)              // recursive copy
bld_fs_remove(path)
bld_fs_remove_all(path)              // recursive remove
bld_fs_rename(from, to)
bld_fs_read_file(path, &len)         // returns arena-allocated content
bld_fs_write_file(path, data, len)
bld_fs_write_str(path, str)          // write null-terminated string (calls strlen for you)
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
        .compile_pub = { .include_dirs = BLD_PATHS("include") });

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
    bld_install_files(b, BLD_PATHS("include/mylib.h"), bld_filepath("include"));
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
    Bld_Target* ssl  = bld_find_pkg(b, "openssl");
    Bld_Target* zlib = bld_find_pkg(b, "zlib");

    // library
    Bld_Paths srcs = bld_files_glob("lib/*.c");
    Bld_CompileFlags cf = bld_default_compile_flags(b);
    cf.defines = BLD_STRS("HAVE_CONFIG_H");
    cf.include_dirs = BLD_PATHS("include", "lib", "generated");

    Bld_Target* lib = bld_add_lib(b,
        .name    = "mynet",
        .sources = srcs,
        .compile = cf);
    if (ssl->found)  bld_link_with(lib, ssl);
    if (zlib->found) bld_link_with(lib, zlib);

    // executable
    Bld_Target* exe = bld_add_exe(b,
        .name    = "mynet-cli",
        .sources = BLD_PATHS("src/main.c"),
        .compile = cf,
        .link    = { .libs = BLD_STRS("m", "pthread") });
    bld_link_with(exe, lib);
}
```
