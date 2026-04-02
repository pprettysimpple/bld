/* bld/bld_checks.h — feature detection interface */
#pragma once

#include "bld_core.h"

typedef struct Bld_Checks Bld_Checks;

Bld_Checks* bld_checks_new(Bld* parent);
bool* bld_checks_header(Bld_Checks* c, const char* define_name, const char* header);
bool* bld_checks_func(Bld_Checks* c, const char* define_name, const char* func, const char* header);
int*  bld_checks_sizeof(Bld_Checks* c, const char* define_name, const char* type);
bool* bld_checks_compile(Bld_Checks* c, const char* define_name, const char* source);
void  bld_checks_run(Bld_Checks* c);
void  bld_checks_write(Bld_Checks* c, const char* path);
