/* bld/bld_dep.h — external dependency discovery */
#pragma once

#include "bld_core.h"

/* deep-copy a dependency from compound literal (sets found=1) */
Bld_Dep* bld__dep(const Bld_Dep* d);
#define bld_dep(...) bld__dep(&(Bld_Dep){__VA_ARGS__})

/* discover dependency via pkg-config (found=0 if not installed) */
Bld_Dep* bld_find_pkg(const char* name);
