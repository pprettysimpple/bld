/* bld/bld_cache.h — artifact cache interface */
#pragma once

#include "bld_core.h"

/* path resolution */
Bld_Path bld__step_artifact(Bld* b, Bld_Step* s);
Bld_Path bld__target_artifact(Bld* b, Bld_Target* t);
Bld_Path bld__step_depfile_cache(Bld* b, Bld_Step* s);
Bld_Path bld__cache_tmp(Bld* b);

/* depfile hashing */
Bld_Hash bld__hash_depfile_contents(Bld_Path depfile);

/* content hashing */
Bld_Hash bld__hash_artifact(Bld_Path art);

/* lookup */
int bld__cache_has(Bld* b, Bld_Step* step);

/* store */
void bld__cache_store_depfile(Bld* b, Bld_Step* step, Bld_Path tmp_dep);
void bld__cache_store_artifact(Bld* b, Bld_Step* step, Bld_Path tmp_out);
