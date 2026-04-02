#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"
BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    set_compiler_c(b, .standard = C_11);
    Target* exe = add_exe(b, .name = "hello", .sources = BLD_PATHS("src/main.c"));
    add_install_exe(b, exe);
}
