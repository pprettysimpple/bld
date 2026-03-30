# Rules

- When committing changes you wrote, use your own authorship:
  `--author="Claude <noreply@anthropic.com>"`

# bld.h — C build system

Single-header build system. User writes `build.c`, includes `bld.h`, implements `configure()`.
bld provides `main()`, CLI parsing, caching, parallel compilation, self-recompilation.

## Quick start

```c
#define BLD_CC "cc"
#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"

void configure(Bld* b) {
    /* shared flags — define once, reuse across targets */
    CompileFlags common = {
        .standard = STD_C11, .optimize = OPT_O2,
        .include_dirs = BLD_PATHS("include/"),
    };

    Target* lib = add_lib(b, .name = "mylib",
        .sources = BLD_PATHS("foo.c", "bar.c"),
        .compile = common);

    Target* exe = add_exe(b, .name = "myapp",
        .sources = BLD_PATHS("main.c"),
        .compile = common);

    link_with(exe, lib);
    add_run(b, exe, .name = "run", .desc = "Run the app");
    add_install_exe(b, exe);
    add_install_lib(b, lib);
}
```

Bootstrap: `cc -std=c11 -w build.c -o b -lpthread`
Build: `./b build`, `./b build -v`, `./b clean`, `./b install`

## API reference (with BLD_STRIP_PREFIX)

### Target creation
```c
Target* add_exe(b, .name, .desc, .sources, .compile, .link);
Target* add_lib(b, .name, .desc, .sources, .compile, .link, .shared);
Target* add_step(b, .name, .desc, .action, .action_ctx, .watch);
Target* add_run(b, target, .name, .desc, .args, .working_dir);
```

### Target relationships
```c
link_with(exe, lib);      // link exe against lib — transitive!
depends_on(a, b);         // pure ordering dependency (not transitive)

// Transitive example: exe only needs to link top-level lib
link_with(mbedx509, mbedcrypto);   // x509 depends on crypto
link_with(mbedtls, mbedx509);      // tls depends on x509
link_with(app, mbedtls);           // app gets tls + x509 + crypto automatically
```

### Target output (for code generation)
```c
target_output(step);                // LazyPath to step's output
target_output_sub(step, "subpath"); // LazyPath to subpath within output
target_add_include_dir(target, lazy_path); // add -I from LazyPath
```

### Install
```c
add_install_exe(b, exe);
add_install_lib(b, lib);
add_install(b, target, bld_path("custom/path"));
```

### Source discovery
```c
const char** files_glob("dir/*.c");          // single directory
const char** files_glob("dir/**/*.c");       // recursive (all subdirs)
const char** files_exclude(files, BLD_PATHS("dir/skip.c"));
```

### Compile flags
```c
.compile = {
    .optimize     = OPT_O2,      // OPT_O0/O1/O2/O3/Os/OFAST
    .standard     = STD_C11,     // STD_C90/C99/C11/C17/C23/GNU11/GNU17/...
    .warnings     = TOGGLE_ON,   // TOGGLE_ON/TOGGLE_OFF/TOGGLE_UNSET
    .defines      = BLD_DEFS("FOO", "BAR=1"),
    .include_dirs = BLD_PATHS("include/", "vendor/"),
    .extra_flags  = "-march=native",
}

// Note: STD_C99/C11/etc. = strict ISO, disables GNU extensions.
// Most Linux projects need GNU extensions (memrchr, strtok_r, etc.)
// Use STD_GNU99/GNU11/GNU17 for POSIX/Linux code.

// Tip: use a variable to share flags across targets, override per-target:
CompileFlags common = { .standard = STD_GNU11, .optimize = OPT_O2 };
CompileFlags debug = clone_compile_flags(common);
debug.defines = BLD_DEFS("DEBUG");
```

### Link flags
```c
.link = {
    .debug_info  = TOGGLE_ON,
    .asan        = TOGGLE_ON,
    .lto         = TOGGLE_ON,
    .extra_flags = "-lm -lpthread",  // system libs, raw linker flags
}
```

### Shared libraries
```c
Target* lib = add_lib(b, .name = "mylib", .sources = ..., .shared = 1);
// -fPIC auto-added for all libs, soname set, rpath configured, .so copied to build/lib/
```

### Custom steps (code generation)
```c
void gen_fn(void* ctx, Bld_Path output, Bld_Path depfile) {
    bld_fs_mkdir_p(output);  // output is a path — make it a dir for headers
    Bld_Path hdr = bld_path_join(output, bld_path("generated.h"));
    bld_fs_write_file(hdr, content, len);
}

Target* gen = add_step(b, .name = "gen", .action = gen_fn,
                       .watch = BLD_PATHS("build.c"));  // re-run when watched files change
Target* exe = add_exe(b, .name = "app", .sources = BLD_PATHS("main.c"));
target_add_include_dir(exe, target_output(gen));
depends_on(exe, gen);
```

Custom steps use content-hash: if output doesn't change, downstream stays cached.

### Filesystem utilities
```c
bld_fs_exists(path), bld_fs_is_dir(path), bld_fs_is_file(path)
bld_fs_mkdir_p(path), bld_fs_remove(path), bld_fs_remove_all(path)
bld_fs_read_file(path, &len), bld_fs_write_file(path, data, len)
bld_fs_copy_file(from, to), bld_fs_rename(from, to)
```

### Paths (NOT stripped — always use bld_ prefix)
```c
bld_path("literal")
bld_path_join(a, b), bld_path_parent(p), bld_path_filename(p)
```

### Macros (NOT stripped — always use BLD_ prefix)
```c
BLD_PATHS("a.c", "b.c")   // NULL-terminated const char**
BLD_DEFS("FOO", "BAR=1")  // NULL-terminated const char**
BLD_DA(T)                  // dynamic array type
bld_da_push(&da, item)     // push to dynamic array
```

### Logging (NOT stripped)
```c
bld_log("info: %s\n", msg);
bld_panic("fatal: %s\n", msg);  // aborts build
```

## CLI
```
./b build [-v] [-s] [-j N] [--show-cached] [--debug] [--release]
./b clean
./b install
./b <target> [-- args...]    # args after -- forwarded to run targets
./b --help
```

## Build modes

`--debug` and `--release` control optimization and debug info.
Use `default_compile_flags(b)` / `default_link_flags(b)` to get mode-aware defaults:

| Mode | optimize | debug_info | defines |
|------|----------|-----------|---------|
| (default) | — | — | — |
| `--debug` | -O0 | -g | — |
| `--release` | -O2 | (off) | NDEBUG |

Usage:
```c
void configure(Bld* b) {
    CompileFlags flags = default_compile_flags(b);  // respects --debug/--release
    LinkFlags lflags = default_link_flags(b);
    flags.standard = STD_C11;
    flags.include_dirs = BLD_PATHS("include/");

    Target* lib = add_lib(b, .name = "mylib",
        .sources = BLD_PATHS("foo.c", "bar.c"),
        .compile = flags, .link = lflags);

    // per-target override: always O3 regardless of mode
    CompileFlags hot = flags;
    hot.optimize = OPT_O3;
    Target* fast = add_lib(b, .name = "hotpath",
        .sources = BLD_PATHS("hot.c"), .compile = hot);
}
```

If you don't call `default_*_flags()`, targets get no special flags (compiler defaults).

## Key behaviors
- Content hashing (not mtime) — `touch` alone doesn't trigger rebuild
- Header deps tracked via `-MMD` depfiles
- Parallel compilation (auto-detects cores, `-j N` to override)
- Self-recompilation: changes to build.c/bld.h auto-rebuild the build tool
- Target names must be unique (duplicate names cause error)
- `-fPIC` added for all library targets automatically

# Testing bld.h with agents

## Agent setup
- Run `bash amalgamate.sh` before launching agents (generates `bld_amalgamated.h`)
- Each agent works in its own directory under `examples/`
- Each agent MUST have bash access (they compile and run code)
- Agents should create symlink: `ln -sf ../../bld_amalgamated.h bld.h`
- Bootstrap: `cc -std=c11 -w build.c -o b -lpthread`

## What agents should do
- NOT one-shot "create and build". They should iterate: create, build, change, rebuild, verify
- Simulate real dev workflow: add files, refactor, change headers, change flags
- Track what recompiles and what caches — this is the main signal
- Report: crashes, wrong caching, missing rebuilds, confusing output, API problems
