#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"
BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    set_compiler_c(b, .standard = C_11);
    set_compiler_cxx(b, .standard = CXX_17);
    Target* exe = add_exe(b, .name = "mixed",
        .sources = BLD_PATHS("src/main.c", "src/helper.cpp"),
        .compile = { .include_dirs = BLD_PATHS("src") });
    add_install_exe(b, exe);
}
