# UX Issues — results of 10-project porting research

## P0-1: No install_headers (8/10 agents)

**Problem:** Every library needs to install public headers. Currently requires
a 15-line custom action function (see curl example). This is the single most
requested feature.

**Who complained:** sqlite, zlib, libuv, mbedtls, lz4, libpng, raylib, protobuf-c

**Other build systems:**
- CMake: `install(FILES foo.h DESTINATION include/)`
- Meson: `install_headers('foo.h', subdir: 'mylib')`
- Zig: `b.installDirectory(.{ .source = "include", .dest = "include" })`

**Proposed fix:** Two new functions:

```c
bld_install_files(b, BLD_PATHS("include/foo.h", "include/bar.h"), "include");
bld_install_dir(b, "include/mylib", "include/mylib");
```

`install_files` — copies explicit file list from source tree to prefix/dst.
Covers patterns A (explicit list), E (cherry-pick), and C (via files_glob).

`install_dir` — copies entire directory to prefix/dst.
Covers pattern B (clean include/ dir).

Generated headers (pattern D) use existing `install_target(b, gen_step, path)`
which copies build artifact to install prefix. Two separate calls:
```c
install_files(b, BLD_PATHS("zlib.h"), "include");           // source tree
install_target(b, gen_zconf, bld_path("include/zconf.h"));  // generated
```

No new lazy path types needed. Source-tree files → install_files.
Build artifacts → install_target (already exists).

---

## P0-2: No link_pub / public link deps propagation (6/10 agents)

**Problem:** `compile_pub` propagates include dirs through `link_with`, but
there is no equivalent for link flags. When libraylib needs `-lGL -lm -lpthread`,
every consumer must duplicate these flags. For raylib with 100+ examples, this
means 100+ copies of the same platform link string.

**Who complained:** libuv, mbedtls, redis, raylib, libpng, lz4

**Other build systems:**
- CMake: `target_link_libraries(foo PUBLIC pthread dl)` — propagates transitively
- Meson: `declare_dependency(dependencies: [dep_pthread])` — transitive
- Zig: link dependencies propagate automatically through artifact dependencies

**Proposed fix:** Restructure flag types + add `link_pub`:

Move asan/lto to global-only `Bld_BuildFlags` on Bld (not per-target).
Move debug_info to `Bld_CompileFlags` (per-target, like optimize/warnings).
Clean up `Bld_LinkFlags` to contain only link-specific data + add libs/lib_dirs.

```c
// Global — affect entire build, both compile and link
typedef struct {
    Bld_Toggle asan;
    Bld_Toggle lto;
} Bld_BuildFlags;

// Per-target compile (debug_info moves here from LinkFlags)
typedef struct {
    Bld_Optimize optimize;
    Bld_Toggle   warnings;
    Bld_Toggle   debug_info;   // was in LinkFlags, now per-target compile flag
    const char*  extra_flags;
    const char** defines;
    const char** include_dirs;
    const char** system_include_dirs;
} Bld_CompileFlags;

// Per-target link (cleaned — no asan/lto/debug)
typedef struct {
    const char*  extra_flags;
    const char** libs;       // NEW: -l names, NULL-terminated
    const char** lib_dirs;   // NEW: -L paths, NULL-terminated
} Bld_LinkFlags;
```

Add `Bld_LinkFlags link_pub` to `Bld_LibOpts` (same pattern as compile_pub).
Propagation in `bld__resolve_link_deps` creates synthetic ext_dep from
link_pub (libs, lib_dirs, extra_flags). Safe — no asan/lto can leak.

Also fixes P1-2 (LinkFlags.libs missing) — libs field is part of the new struct.

---

## P1-1: Dynamic defines builder (7/10 agents)

**Problem:** `BLD_DEFS("FOO", "BAR")` is a compound literal — cannot
conditionally extend it. Every real project has conditional `-D` flags
(platform, features). Users must jump to `bld_da_push(&defs, "FOO")` +
manual NULL termination, which is error-prone and verbose.

**Who complained:** sqlite, zlib, libuv, redis, raylib, jq, libpng

**Other build systems:**
- CMake: `target_compile_definitions(foo PRIVATE FOO BAR)` — append one at a time
- Meson: arrays are first-class, `defs += ['FOO']` works naturally
- Zig: step.defineCMacro("FOO", "1") — builder pattern

**Proposed fix:** Introduce typed newtype wrappers + merge functions.

Rename existing `Bld_Strings` (DA with count/cap) → `Bld_StringList`
(consistent with Bld_StepList, Bld_TargetList).

New typed immutable list wrappers:
```c
typedef struct { const char** items; } Bld_Strings;  // defines, lib names
typedef struct { const char** items; } Bld_Paths;    // source files, dir paths

#define BLD_STRS(...)  ((Bld_Strings){(const char*[]){__VA_ARGS__, NULL}})
#define BLD_PATHS(...) ((Bld_Paths){(const char*[]){__VA_ARGS__, NULL}})
```

Merge functions for conditional extension:
```c
Bld_Strings bld_strs_merge(Bld_Strings a, Bld_Strings b);
Bld_Paths   bld_paths_merge(Bld_Paths a, Bld_Paths b);
```

Usage:
```c
Bld_Strings defs = BLD_STRS("HAVE_CONFIG_H");
if (has_feature)
    defs = strs_merge(defs, BLD_STRS("HAVE_FEATURE"));
cflags.defines = defs;
```

Compiler catches type mismatches:
```c
.sources = BLD_STRS("main.c")  // ERROR: Bld_Strings ≠ Bld_Paths
.defines = BLD_PATHS("FOO")    // ERROR: Bld_Paths ≠ Bld_Strings
```

Struct fields use typed wrappers:
- CompileFlags: `.defines` = Bld_Strings, `.include_dirs` = Bld_Paths
- LinkFlags: `.libs` = Bld_Strings, `.lib_dirs` = Bld_Paths
- ExeOpts/LibOpts: `.sources` = Bld_Paths
- files_glob/files_exclude return Bld_Paths

---

## P1-2: LinkFlags.libs missing (5/10 agents)

**Problem:** No structured way to say "link against -ldl -lm -lpthread".
Must use `link.extra_flags = "-ldl -lm -lpthread"` (unstructured string).
CompileFlags has `.defines[]` and `.include_dirs[]` (structured), but
LinkFlags has no `.libs[]` — asymmetry.

**Who complained:** sqlite, redis, raylib, libuv, lz4

**Other build systems:**
- CMake: `target_link_libraries(foo PRIVATE dl m pthread)` — structured
- Meson: `declare_dependency(link_with: [lib_m])` or `cc.find_library('m')`
- All build systems have structured lib lists, not raw strings

**Proposed fix:** Solved by P0-2 restructuring. New LinkFlags:
```c
typedef struct {
    const char*  extra_flags;
    Bld_Strings  libs;       // -l names: BLD_STRS("m", "dl", "pthread")
    Bld_Paths    lib_dirs;   // -L paths: BLD_PATHS("/usr/local/lib")
} Bld_LinkFlags;
```

---

## P1-3: install_exe vs add_install_exe naming split (6/10 agents)

**Problem:** Dev header (`bld.h`) defines `install_exe` via strip-prefix.
Old amalgamated header (`local/include/bld.h`) defines `add_install_exe`.
Examples (01_lua, 03_mixed_lang) use `add_install_*`. New examples (02_curl)
use `install_*`. First-time users hit compile errors copying from examples.

**Who complained:** sqlite, zlib, libuv, lz4, raylib, protobuf-c

**Proposed fix:** Already fixed. Removed `add_install_*` aliases, all code
uses `install_exe`/`install_lib`/`install_target`. Examples updated.

---

## P2-1: NULL terminator foot-gun on dynamic arrays (4/10 agents)

**Problem:** When building defines/sources dynamically via `bld_da_push`,
forgetting `bld_da_push(&arr, NULL)` causes silent UB — the system reads
past the array. `BLD_DEFS()` handles this automatically, but the moment
you go dynamic you lose the safety net.

**Who complained:** sqlite, zlib, redis, libpng

**Other build systems:**
- CMake/Meson: dynamic arrays are language built-ins, no sentinel needed
- This is a C-specific problem. Zig doesn't have it either (slices have length).

**Proposed fix:** Solved by P1-1 typed wrappers. Users use BLD_STRS/BLD_PATHS
(auto-terminated) and strs_merge/paths_merge (return terminated). Manual
bld_da_push + NULL exits user-facing API entirely.

---

## P2-2: compile_pub naming unclear (3/10 agents)

**Problem:** The `_pub` suffix is terse. New users don't know it means
"propagated to targets that link_with this lib". Some suggested
`public_compile`, `compile_interface`, `exported_compile`.

**Who complained:** mbedtls, zlib, protobuf-c

**Proposed fix:** Keep `compile_pub` / `link_pub`. The `_pub` suffix is an
established convention (Zig `pub`, Rust `pub`). Alternatives are longer
without being clearer. Add a comment to the struct field definition.

---

## P2-3: Step output is cache path, not user-controlled (2/10 agents)

**Problem:** Step action receives `output` as a cache artifact path
(`build/cache/arts/<hash>`). Users expect to control where generated files
go. This makes generated config headers (pnglibconf.h, version.h) awkward —
the file ends up in cache, not at a predictable project path.

**Who complained:** jq, libpng

**Other build systems:**
- CMake: `add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/config.h ...)`
- Meson: `configure_file(output: 'config.h')` places in build dir
- Zig: generated files go to `zig-cache/` but accessed via lazy path

**Proposed fix:** By design — cache-addressed artifacts enable caching.
For generated config headers, two existing patterns work:
1. Action writes to known path directly (curl pattern: writes curl_config.h)
2. `install_target(b, gen_step, bld_path("include/config.h"))` for install
3. `target_add_include_dir(t, target_output(gen_step))` for build-time use

Document these patterns. No behavior change needed.

---

## P2-4: No add_cmd() shorthand for shell commands (2/10 agents)

**Problem:** Each codegen step requires: context struct + action function +
arena alloc + wiring. For "run bison on parser.y", this is ~20 lines of
boilerplate. An `add_cmd(b, "bison -d parser.y")` would cover 80% of cases.

**Who complained:** jq, redis

**Other build systems:**
- CMake: `add_custom_command(COMMAND bison -d parser.y ...)`
- Meson: `custom_target('parser', command: ['bison', '-d', 'parser.y'], ...)`
- Zig: `b.addSystemCommand(&.{"bison", "-d", "parser.y"})`

**Proposed fix:** Add `bld_add_cmd()` convenience wrapper:

```c
Target* gen = add_cmd(b,
    .name = "gen-parser",
    .cmd = "bison -d -o parser.c parser.y",
    .watch = BLD_PATHS("parser.y"));
```

Implementation: allocate cmd string in arena, action = `system(cmd)`,
hash = hash of cmd string. Thin wrapper around add_step. ~20 lines.

---

## P2-5: ext_deps include dirs don't propagate through link_with (1/10 agents)

**Problem:** When lib A does `use_dep(A, zlib)` and exe B does
`link_with(B, A)`, B gets zlib's link flags but NOT zlib's include dirs
for compilation. User must redundantly `use_dep(B, zlib)`.

**Who complained:** libpng

**Note:** This is related to P0-2 (link_pub). The ext_deps already propagate
link flags via `bld__resolve_link_deps`, but the compile-side includes from
ext_deps do NOT propagate. Only `compile_pub` include dirs propagate.

**Proposed fix:** Solved by P0-2/P1-1. With `compile_pub` and `link_pub`,
the lib author explicitly chooses what propagates to consumers. ext_dep
includes are private by default — if consumers need them, add to compile_pub.
This is the correct semantic (CMake PUBLIC vs PRIVATE).

---

## P2-6: BLD_DEFS vs BLD_PATHS are identical macros (3/10 agents)

**Problem:** Both expand to `((const char*[]){__VA_ARGS__, NULL})`.
Having two names for the same thing adds cognitive load without benefit.

**Who complained:** raylib, redis, zlib

**Proposed fix:** Solved by P1-1 typed wrappers. BLD_STRS and BLD_PATHS are
different types — compiler catches misuse. BLD_DEFS replaced by BLD_STRS
(defines are strings, not paths). No two identical macros anymore.

---

## P3-1: No object library concept (1/10 agents)

**Problem:** Redis has ~100 .c files shared by 5 executables. Must create
a static .a to avoid recompilation. An "object library" (compile but don't
archive) would avoid the .a overhead.

**Who complained:** redis

**Other build systems:**
- CMake: `add_library(foo OBJECT src1.c src2.c)` — compile-only target
- Meson: `static_library(..., pic: true)` then link objects
- Not common in C build systems — static lib is the standard pattern

**Proposed fix:** Defer. Static lib is the standard pattern for shared objects.
Content-hash caching means identical compilations across targets are free.
Redis pattern (5 exe sharing 100 .c via one static lib) works well.
Add OBJECT library concept only if real demand appears.

---

## P3-2: No shared lib versioning (2/10 agents)

**Problem:** No soname/symlink support for versioned shared libs
(`libz.so.1.3.1` → `libz.so.1` → `libz.so`).

**Who complained:** zlib, libpng

**Proposed fix:** Defer. Tracked in tickets/shared_lib_versioning.md.
Add `.version = {major, minor, patch}` to LibOpts when implemented.

---

## Bugs found

### BUG-1: bld_dup_strarray has no strip-prefix alias
**Fix:** Add `#define dup_strarray bld_dup_strarray` to bld.h.
With typed wrappers (P1-1), this function may become internal-only
(merge functions replace it in user-facing API).

### BUG-2: Stale amalgamated header
**Fix:** Update fetch-source.sh scripts to symlink to `../../bld.h` (dev header)
+ `../../bld` (module dir), not `local/include/bld.h`. Run `./b amalgamate`
after each release to keep local/ fresh. Or remove local/ from git entirely.
