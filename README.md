# bld

Single-header build system for C. Spiritual successor of [buildpp](https://github.com/pprettysimpple/buildpp), rewritten from scratch. Write `build.c`, include `bld.h`, implement `configure()`.

```c
#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"

BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    bld_set_compiler_c(b, .standard = BLD_C_11);

    Target* exe = add_exe(b, .name = "myapp",
        .sources = BLD_PATHS("main.c"));
}
```

```
cc -std=c11 -w build.c -o b -lpthread
./b build
```

Uses content hashing, tracks header deps via `-MMD`, compiles in parallel, recompiles itself when `build.c` changes.

## Building bld itself

```
cc -std=c11 -w build.c -o b -lpthread
./b build
./b amalgamate
./b install --prefix /usr/local
```

## Disclaimer

This codebase is largely AI-generated. I have no idea what consequences this may bring. You have been warned.
