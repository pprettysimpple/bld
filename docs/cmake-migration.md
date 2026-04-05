# CMake → bld Migration Guide

> You know CMake. Here's how to do the same things in bld.

## 1. Project Setup

**CMake:**
```cmake
cmake_minimum_required(VERSION 3.20)
project(myapp C)
```

**bld:**
```c
#define BLD_IMPLEMENTATION
#include "bld.h"

BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    bld_set_compiler_c(b, .standard = BLD_C_11);
    // targets go here
}
```

Bootstrap: `cc -std=c11 -w build.c -o b -lpthread && ./b build`

---

## 2. Executables

**CMake:**
```cmake
add_executable(myapp src/main.c src/util.c)
```

**bld:**
```c
Bld_Target* app = bld_add_exe(b,
    .name    = "myapp",
    .sources = BLD_PATHS("src/main.c", "src/util.c"));
```

### Output name override

**CMake:**
```cmake
set_target_properties(myapp PROPERTIES OUTPUT_NAME "my-tool")
```

**bld:**
```c
Bld_Target* app = bld_add_exe(b,
    .name        = "myapp",
    .output_name = "my-tool",
    .sources     = BLD_PATHS("src/main.c"));
```

### Multiple executables

**CMake:**
```cmake
add_executable(client src/client.c)
add_executable(server src/server.c)
```

**bld:**
```c
Bld_Target* client = bld_add_exe(b, .name = "client", .sources = BLD_PATHS("src/client.c"));
Bld_Target* server = bld_add_exe(b, .name = "server", .sources = BLD_PATHS("src/server.c"));
```

---

## 3. Libraries

### Static library

**CMake:**
```cmake
add_library(mylib STATIC src/foo.c src/bar.c)
```

**bld:**
```c
Bld_Target* lib = bld_add_lib(b,
    .name    = "mylib",
    .sources = BLD_PATHS("src/foo.c", "src/bar.c"));
```

### Shared library

**CMake:**
```cmake
add_library(mylib SHARED src/foo.c src/bar.c)
```

**bld:**
```c
Bld_Target* lib = bld_add_lib(b,
    .name    = "mylib",
    .shared  = true,
    .sources = BLD_PATHS("src/foo.c", "src/bar.c"));
```

### Library with public headers

**CMake:**
```cmake
add_library(mylib STATIC src/foo.c)
target_include_directories(mylib PUBLIC include)
```

**bld:**
```c
Bld_Target* lib = bld_add_lib(b,
    .name        = "mylib",
    .sources     = BLD_PATHS("src/foo.c"),
    .compile_pub = { .include_dirs = BLD_PATHS("include") });
```

---

## 4. Source Files

### Explicit list

**CMake:**
```cmake
set(SRCS src/main.c src/util.c src/net.c)
add_executable(app ${SRCS})
```

**bld:**
```c
Bld_Paths srcs = BLD_PATHS("src/main.c", "src/util.c", "src/net.c");
Bld_Target* app = bld_add_exe(b, .name = "app", .sources = srcs);
```

### Glob

**CMake:**
```cmake
file(GLOB SRCS src/*.c)
```

**bld:**
```c
Bld_Paths srcs = bld_files_glob("src/*.c");
```

### Recursive glob

**CMake:**
```cmake
file(GLOB_RECURSE SRCS src/**/*.c)
```

**bld:**
```c
Bld_Paths srcs = bld_files_glob("src/**/*.c");
```

### Exclude files

**CMake:**
```cmake
file(GLOB SRCS src/*.c)
list(REMOVE_ITEM SRCS src/test_main.c)
```

**bld:**
```c
Bld_Paths srcs = bld_files_glob("src/*.c");
srcs = bld_files_exclude(srcs, BLD_PATHS("src/test_main.c"));
```

### Exclude by pattern

**CMake:**
```cmake
file(GLOB SRCS src/*.c)
list(FILTER SRCS EXCLUDE REGEX ".*_test\\.c$")
```

**bld:**
```c
Bld_Paths srcs = bld_files_glob("src/*.c");
srcs = bld_files_exclude(srcs, BLD_PATHS("*_test.c"));
```

### Conditional sources

**CMake:**
```cmake
if(WIN32)
    list(APPEND SRCS src/platform_win.c)
else()
    list(APPEND SRCS src/platform_unix.c)
endif()
```

**bld:**
```c
if (b->toolchain->os == BLD_OS_WINDOWS)
    bld_paths_push(&srcs, "src/platform_win.c");
else
    bld_paths_push(&srcs, "src/platform_unix.c");
```

### Merge source lists

**CMake:**
```cmake
set(ALL_SRCS ${LIB_SRCS} ${EXTRA_SRCS})
```

**bld:**
```c
Bld_Paths all = bld_paths_merge(lib_srcs, extra_srcs);
```

---

## 5. Compile Definitions

### Private defines

**CMake:**
```cmake
target_compile_definitions(mylib PRIVATE FOO BAR=1)
```

**bld:**
```c
Bld_Target* lib = bld_add_lib(b,
    .name    = "mylib",
    .sources = srcs,
    .compile = { .defines = BLD_STRS("FOO", "BAR=1") });
```

### Public defines (propagated to consumers)

**CMake:**
```cmake
target_compile_definitions(mylib PUBLIC MYLIB_STATIC)
```

**bld:**
```c
Bld_Target* lib = bld_add_lib(b,
    .name        = "mylib",
    .sources     = srcs,
    .compile_pub = { .defines = BLD_STRS("MYLIB_STATIC") });
```

### Global defines

**CMake:**
```cmake
add_compile_definitions(_GNU_SOURCE)
```

**bld:**
```c
b->global_defines = BLD_STRS("_GNU_SOURCE");
```

### Conditional defines

**CMake:**
```cmake
if(USE_SSL)
    target_compile_definitions(app PRIVATE USE_SSL)
endif()
```

**bld:**
```c
Bld_Strs defs = {0};
if (use_ssl) bld_strs_push(&defs, "USE_SSL");
// ... use defs in .compile = { .defines = defs }
```

### Complex values (VERSION="1.0")

**CMake:**
```cmake
target_compile_definitions(app PRIVATE VERSION="1.0")
```

**bld** — use a generated header instead (cross-platform safe):
```c
bld_fs_write_str(bld_filepath("generated/version.h"),
    "#define VERSION \"1.0\"\n");
// add "generated" to include_dirs, add HAVE_VERSION_H to defines
```

---

## 6. Include Directories

### Private

**CMake:**
```cmake
target_include_directories(mylib PRIVATE src)
```

**bld:**
```c
.compile = { .include_dirs = BLD_PATHS("src") }
```

### Public (propagated)

**CMake:**
```cmake
target_include_directories(mylib PUBLIC include)
```

**bld:**
```c
.compile_pub = { .include_dirs = BLD_PATHS("include") }
```

### System includes (suppress warnings)

**CMake:**
```cmake
target_include_directories(mylib SYSTEM PUBLIC /opt/sdk/include)
```

**bld:**
```c
.compile_pub = { .system_include_dirs = BLD_PATHS("/opt/sdk/include") }
```

---

## 7. Compile Options & Standards

### C standard

**CMake:**
```cmake
set(CMAKE_C_STANDARD 11)
```

**bld:**
```c
bld_set_compiler_c(b, .standard = BLD_C_11);
// GNU extensions: BLD_C_GNU11
```

### C++ standard

**CMake:**
```cmake
set(CMAKE_CXX_STANDARD 17)
```

**bld:**
```c
bld_set_compiler_cxx(b, .standard = BLD_CXX_17);
```

### Optimization

**CMake:**
```cmake
target_compile_options(app PRIVATE -O2)
```

**bld:**
```c
.compile = { .optimize = BLD_OPT_O2 }
```

### Warnings

**CMake:**
```cmake
target_compile_options(app PRIVATE -Wall)
```

**bld:**
```c
.compile = { .warnings = BLD_ON }
```

### Extra flags

**CMake:**
```cmake
target_compile_options(app PRIVATE -fno-strict-aliasing -march=native)
```

**bld:**
```c
.compile = { .extra_flags = "-fno-strict-aliasing -march=native" }
```

### Per-file overrides

**CMake:**
```cmake
set_source_files_properties(vendor/noisy.c PROPERTIES COMPILE_FLAGS -w)
```

**bld:**
```c
bld_override_file(lib, "vendor/noisy.c", .warnings = BLD_OFF);
bld_override_file(lib, "hot_path.c", .optimize = BLD_OPT_O3);
```

---

## 8. Linking

### Internal library

**CMake:**
```cmake
target_link_libraries(app PRIVATE mylib)
```

**bld:**
```c
bld_link_with(app, lib);
```

### External library (pkg-config)

**CMake:**
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(SSL REQUIRED openssl)
target_link_libraries(app PRIVATE ${SSL_LIBRARIES})
target_include_directories(app PRIVATE ${SSL_INCLUDE_DIRS})
```

**bld:**
```c
Bld_Target* ssl = bld_find_pkg(b, "openssl");
if (ssl->found) bld_link_with(app, ssl);
```

### Manual external dependency

**CMake:**
```cmake
target_include_directories(app PRIVATE /opt/mylib/include)
target_link_directories(app PRIVATE /opt/mylib/lib)
target_link_libraries(app PRIVATE mylib)
```

**bld:**
```c
Bld_Target* dep = bld_pkg(b,
    .name         = "mylib",
    .include_dirs = BLD_PATHS("/opt/mylib/include"),
    .libs         = BLD_STRS("mylib"),
    .lib_dirs     = BLD_PATHS("/opt/mylib/lib"));
bld_link_with(app, dep);
```

### System libraries

**CMake:**
```cmake
target_link_libraries(app PRIVATE m pthread dl)
```

**bld:**
```c
.link = { .libs = BLD_STRS("m", "pthread", "dl") }
```

### Public link deps (transitive)

**CMake:**
```cmake
target_link_libraries(mylib PUBLIC pthread dl)
```

**bld:**
```c
.link_pub = { .libs = BLD_STRS("pthread", "dl") }
```

### Extra linker flags

**CMake:**
```cmake
target_link_options(app PRIVATE -Wl,--as-needed)
```

**bld:**
```c
.link = { .extra_flags = "-Wl,--as-needed" }
```

---

## 9. External Dependencies

### Required dependency

**CMake:**
```cmake
find_package(OpenSSL REQUIRED)
target_link_libraries(app PRIVATE OpenSSL::SSL)
```

**bld:**
```c
bld_link_with(app, bld_find_pkg(b, "openssl"));
// panics with clear error if not found
```

### Optional dependency

**CMake:**
```cmake
find_package(ZLIB)
if(ZLIB_FOUND)
    target_link_libraries(app PRIVATE ZLIB::ZLIB)
    target_compile_definitions(app PRIVATE HAVE_ZLIB)
endif()
```

**bld:**
```c
Bld_Target* zlib = bld_find_pkg(b, "zlib");
if (zlib->found) {
    bld_link_with(app, zlib);
    bld_strs_push(&defs, "HAVE_ZLIB");
}
```

---

## 10. Feature Detection

### Check header

**CMake:**
```cmake
include(CheckIncludeFile)
check_include_file(unistd.h HAVE_UNISTD_H)
```

**bld:**
```c
Bld_Checks* chk = bld_checks_new(b);
bld_checks_header(chk, "HAVE_UNISTD_H", "unistd.h");
```

### Check function

**CMake:**
```cmake
include(CheckFunctionExists)
check_function_exists(strlcpy HAVE_STRLCPY)
```

**bld:**
```c
bld_checks_func(chk, "HAVE_STRLCPY", "strlcpy", "string.h");
```

### Check type size

**CMake:**
```cmake
include(CheckTypeSize)
check_type_size("long" SIZEOF_LONG)
```

**bld:**
```c
int* sz = bld_checks_sizeof(chk, "SIZEOF_LONG", "long");
```

### Check source compiles

**CMake:**
```cmake
include(CheckCSourceCompiles)
check_c_source_compiles("
    #include <stdatomic.h>
    int main() { _Atomic int x = 0; return x; }
" HAVE_ATOMIC)
```

**bld:**
```c
bld_checks_compile(chk, "HAVE_ATOMIC",
    "#include <stdatomic.h>\n"
    "int main() { _Atomic int x = 0; return x; }");
```

### Run checks and write config.h

**CMake:**
```cmake
configure_file(config.h.in config.h)
```

**bld:**
```c
bld_checks_run(chk);
bld_checks_write(chk, "generated/config.h");
// produces:
//   #define HAVE_UNISTD_H 1
//   /* HAVE_STRLCPY not found */
//   #define SIZEOF_LONG 8
```

### Use results conditionally

**CMake:**
```cmake
if(HAVE_UNISTD_H)
    # ...
endif()
```

**bld:**
```c
bool* has_unistd = bld_checks_header(chk, "HAVE_UNISTD_H", "unistd.h");
bld_checks_run(chk);
if (*has_unistd) {
    // ...
}
```

---

## 11. Installation

### Install executable

**CMake:**
```cmake
install(TARGETS myapp DESTINATION bin)
```

**bld:**
```c
bld_install_exe(b, app);   // → <prefix>/bin/myapp
```

### Install library

**CMake:**
```cmake
install(TARGETS mylib DESTINATION lib)
```

**bld:**
```c
bld_install_lib(b, lib);   // → <prefix>/lib/libmylib.a
```

### Install headers

**CMake:**
```cmake
install(FILES include/mylib.h DESTINATION include)
```

**bld:**
```c
bld_install_files(b, BLD_PATHS("include/mylib.h"), bld_filepath("include"));
```

### Install directory

**CMake:**
```cmake
install(DIRECTORY include/mylib DESTINATION include)
```

**bld:**
```c
bld_install_dir(b, "include/mylib", bld_filepath("include/mylib"));
```

### Custom prefix

**CMake:**
```
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
```

**bld:**
```
./b build --prefix /usr/local
```

---

## 12. Build Modes

### Debug / Release

**CMake:**
```
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake -DCMAKE_BUILD_TYPE=Release ..
```

**bld:**
```
./b build --debug     # -O0 -g
./b build --release   # -O2
```

### User options

**CMake:**
```cmake
option(USE_SSL "Enable SSL support" ON)
```

**bld:**
```c
bool use_ssl = bld_option_bool(b, "ssl", "Enable SSL support", true);
```

**CMake:**
```cmake
set(BACKEND "openssl" CACHE STRING "TLS backend")
```

**bld:**
```c
const char* backend = bld_option_str(b, "backend", "TLS backend", "openssl");
```

### Passing options

**CMake:**
```
cmake -DUSE_SSL=OFF -DBACKEND=wolfssl ..
```

**bld:**
```
./b build -Dssl=off -Dbackend=wolfssl
```

Options appear in `./b --help`.

---

## 13. Testing

### Basic test

**CMake:**
```cmake
add_executable(test_runner tests/main.c)
add_test(NAME unit-tests COMMAND test_runner)
```

**bld:**
```c
Bld_Target* test_exe = bld_add_exe(b,
    .name = "test-runner", .sources = BLD_PATHS("tests/main.c"));
bld_add_test(b, test_exe, .name = "unit-tests");
```

### Test with arguments

**CMake:**
```cmake
add_test(NAME smoke COMMAND test_runner --self-test)
```

**bld:**
```c
bld_add_test(b, test_exe,
    .name = "smoke",
    .args = BLD_STRS("--self-test"));
```

### Running tests

**CMake:**
```
ctest
```

**bld:**
```
./b test
```

---

## 14. Code Generation

### configure_file (template substitution)

**CMake:**
```cmake
configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h)
```

**bld** — write directly in C:
```c
bld_fs_write_str(bld_filepath("generated/config.h"),
    bld_str_fmt("#define VERSION \"%s\"\n"
                "#define BUILD_DATE \"%s\"\n",
                version, __DATE__));
```

### Custom command with output

**CMake:**
```cmake
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/gen.c
    COMMAND python3 codegen.py -o ${CMAKE_BINARY_DIR}/gen.c
    DEPENDS codegen.py schema.json)
```

**bld:**
```c
Bld_Target* gen = bld_add_cmd(b,
    .name  = "codegen",
    .cmd   = "python3 codegen.py -o generated/gen.c",
    .watch = BLD_PATHS("codegen.py", "schema.json"));
```

### Add generated source to target

**CMake:**
```cmake
add_executable(app main.c ${CMAKE_BINARY_DIR}/gen.c)
add_dependencies(app codegen)
```

**bld:**
```c
Bld_Target* app = bld_add_exe(b, .name = "app", .sources = BLD_PATHS("main.c"));
bld_add_source(app, bld_output_sub(gen, "gen.c"));
bld_add_include_dir(app, bld_output(gen));
```

### Custom step with C action

**CMake:**
```cmake
add_custom_command(OUTPUT gen.c
    COMMAND ${CMAKE_COMMAND} -E echo "generating..."
    COMMAND my_generator)
```

**bld:**
```c
static Bld_ActionResult my_gen(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)depfile;
    bld_fs_mkdir_p(output);
    bld_fs_write_str(bld_path_join(output, bld_filepath("gen.c")),
        "#include <stdio.h>\nint gen(void){return 42;}\n");
    return BLD_ACTION_OK;
}

Bld_Target* gen = bld_add_step(b,
    .name   = "codegen",
    .action = my_gen,
    .watch  = BLD_PATHS("schema.json"));
```

---

## 15. Toolchain & Cross-compilation

### Setting compiler

**CMake:**
```
cmake -DCMAKE_C_COMPILER=clang ..
```

**bld:**
```
CC=clang ./b build
```

### Setting C++ compiler

**CMake:**
```
cmake -DCMAKE_CXX_COMPILER=clang++ ..
```

**bld:**
```
CXX=clang++ ./b build
```

### MSVC

**CMake:**
```
cmake -G "Visual Studio 17 2022" ..
```

**bld** — auto-detected when cl.exe is in PATH:
```
# from VS Developer Command Prompt:
cl /std:c11 /W0 /Fe:b.exe build.c
b.exe build
```

### Cross-compile toolchain

**CMake:**
```cmake
# toolchain.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
```

**bld:**
```c
void configure(Bld* b) {
    bld_set_compiler_c(b, .driver = "aarch64-linux-gnu-gcc", .standard = BLD_C_11);
    // toolchain auto-detected from compiler's -dumpmachine
}
```

---

## 16. Platform Detection

**CMake:**
```cmake
if(WIN32)
    # Windows
elseif(APPLE)
    # macOS
elseif(UNIX)
    # Linux / BSD
endif()
```

**bld:**
```c
switch (b->toolchain->os) {
    case BLD_OS_WINDOWS: /* ... */ break;
    case BLD_OS_MACOS:   /* ... */ break;
    case BLD_OS_LINUX:   /* ... */ break;
    case BLD_OS_FREEBSD: /* ... */ break;
}
```

---

## 17. Global Build Flags

### AddressSanitizer

**CMake:**
```cmake
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
```

**bld:**
```c
b->build_flags.asan = BLD_ON;
```

### LTO

**CMake:**
```cmake
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
```

**bld:**
```c
b->build_flags.lto = BLD_ON;
```

---

## 18. What CMake has that bld doesn't

| CMake | bld alternative |
|-------|----------------|
| FetchContent / ExternalProject | fetch scripts (`fetch-source.sh`) or git submodules |
| Generator expressions `$<...>` | plain C `if`/`else` in configure() |
| IMPORTED targets | `bld_find_pkg` / `bld_pkg` |
| Object libraries | use static library (content-hash caching avoids recompilation) |
| CPack (packaging) | not planned — use distro tools |
| install(EXPORT) | not planned — use pkg-config |
| cmake_parse_arguments | not needed — C has named struct initializers |
| INTERFACE libraries | use `bld_pkg` with include_dirs only |
| file(DOWNLOAD) | `system("curl -LO ...")` or fetch script |

---

## Key differences in philosophy

| CMake | bld |
|-------|-----|
| DSL (CMake language) | Real C code |
| Generate build files (Ninja/Make) | IS the build system |
| Many generators | One build engine |
| Complex escaping rules | Simple tokens, complex values in headers |
| Package manager integration | pkg-config + manual paths |
| ~500 page docs | Single header, one API reference |
