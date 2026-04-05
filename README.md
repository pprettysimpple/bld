# bld

Single-header build system for C. Write `build.c`, include `bld.h`, implement `configure()`.

```c
#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"

BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    set_compiler_c(b, .standard = BLD_C_11);

    Target* exe = add_exe(b, .name = "myapp",
        .sources = BLD_PATHS("main.c"));
}
```

```
cc -std=c11 -w build.c -o b -lpthread
./b build
```

Content hashing, header dep tracking (`-MMD`), parallel compilation, auto-recompilation of build.c.

See [docs/api.md](docs/api.md) for the full API reference.

## Showcase

Real-world build scripts for [curl](showcase/01_curl/build.c) and [libuv](showcase/02_libuv/build.c).

To build them locally:

```
cd showcase/01_curl
./fetch-source.sh                         # downloads source tarball
cc -std=c11 -w build.c -o b -lpthread    # bootstrap
./b build                                 # build curl
./build/bin/curl --version                # verify
```

Same steps for `showcase/02_libuv`.
