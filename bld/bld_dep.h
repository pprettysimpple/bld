/* bld/bld_dep.h — external dependency discovery */
#pragma once

#include "bld_core.h"

/* discover dependency via pkg-config (found=false if not installed) */
Bld_Target* bld_find_pkg(Bld* b, const char* name);
