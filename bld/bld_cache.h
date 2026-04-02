/* bld/bld_cache.h — artifact cache interface */
#pragma once

#include "bld_core.h"

/* ---- Path utilities (used by build actions, install, public API) ---- */

Bld_Path bld__step_artifact(Bld* b, Bld_Step* s);
Bld_Path bld__target_artifact(Bld* b, Bld_Target* t);
Bld_Path bld__cache_tmp(Bld* b);

/* ---- Cache operations ---- */

/* Compute cache_key from input_hash (+ depfile), check if cached and valid.
   Caller must set step->input_hash before calling. Sets step->cache_key + hash_valid. */
int bld__cache_has(Bld* b, Bld_Step* step);

/* Store action results in cache. Handles depfile, artifact, early cutoff, meta. */
void bld__cache_store(Bld* b, Bld_Step* step, Bld_Path tmp_out, Bld_Path tmp_dep);
