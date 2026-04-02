/* bld/bld_cache.h — artifact cache interface */
#pragma once

#include "bld_core.h"

/* artifact path resolution */
Bld_Path bld__step_artifact(Bld* b, Bld_Step* s);
Bld_Path bld__target_artifact(Bld* b, Bld_Target* t);

/* tmp file allocation */
Bld_Path bld__cache_tmp(Bld* b);

/* compute full cache key from input_hash, incorporating cached depfile if present */
void bld__cache_compute_key(Bld* b, Bld_Step* step);

/* check if step result is cached and valid */
int bld__cache_has(Bld* b, Bld_Step* step);

/* store action results (depfile + artifact + meta) in cache */
void bld__cache_store(Bld* b, Bld_Step* step, Bld_Path tmp_out, Bld_Path tmp_dep);
