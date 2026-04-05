/* bld/bld_cache.c — artifact cache, depfile tracking, content validation */
#pragma once

#include "bld_cache.h"

/* ---- Depfile parsing ---- */

static Bld_PathList bld__parse_depfile(Bld_Path depfile) {
    Bld_PathList deps = {0};
    size_t len;
    const char* content = bld_fs_read_file(depfile, &len);

    const char* p = content;
    const char* end = content + len;
    while (p < end && *p != ':') p++;
    if (p < end) p++;

    char* file = bld_arena_alloc(len + 1);
    size_t flen = 0;

    while (p < end) {
        char c = *p++;
        if (c == '\\' && p < end) {
            char next = *p;
            if (next == '\n') { p++; continue; }
            if (next == ' ')  { p++; file[flen++] = ' '; continue; }
            file[flen++] = c;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n') {
            if (flen > 0) {
                file[flen] = '\0';
                bld_da_push(&deps, bld_path(bld_str_dup(file)));
                flen = 0;
            }
            continue;
        }
        file[flen++] = c;
    }
    if (flen > 0) {
        file[flen] = '\0';
        bld_da_push(&deps, bld_path(bld_str_dup(file)));
    }
    return deps;
}

static Bld_Hash bld__hash_depfile(Bld_Path depfile) {
    Bld_PathList deps = bld__parse_depfile(depfile);
    Bld_Hash h = {0};
    for (size_t i = 0; i < deps.count; i++) {
        if (bld_fs_is_file(deps.items[i]))
            h = bld_hash_combine(h, bld_hash_file(deps.items[i]));
    }
    return h;
}

/* ---- Path helpers ---- */

static Bld_Path bld__cache_art(Bld* b, Bld_Hash h) {
    return bld_path_join(bld_path_join(b->cache, bld_path("arts")), bld_path_fmt("%" PRIu64, h.value));
}

static Bld_Path bld__cache_art_meta(Bld* b, Bld_Hash h) {
    return bld_path_join(bld_path_join(b->cache, bld_path("arts")), bld_path_fmt("%" PRIu64 ".meta", h.value));
}

static Bld_Path bld__depfile_path(Bld* b, Bld_Step* s) {
    size_t len = strlen(s->name);
    char* safe = bld_arena_alloc(len + 1);
    for (size_t i = 0; i < len; i++) {
        char c = s->name[i];
        safe[i] = (c == '/' || c == ':' || c == '\\') ? '_' : c;
    }
    safe[len] = '\0';
    return bld_path_join(bld_path_join(b->cache, bld_path("deps")), bld_path_fmt("%s.d", safe));
}

static Bld_Hash bld__content_hash(Bld_Path art) {
    return bld_fs_is_dir(art) ? bld_hash_dir(art) : bld_hash_file(art);
}

/* ---- Validation ---- */

static int bld__cache_validate(Bld* b, Bld_Step* s) {
    Bld_Path art = bld__step_artifact(b, s);
    Bld_Path meta = bld__cache_art_meta(b, s->cache_key);
    if (!bld_fs_exists(meta)) return 0;
    size_t len;
    const char* data = bld_fs_read_file(meta, &len);
    uint64_t stored = 0;
    if (sscanf(data, "%" SCNu64, &stored) != 1) return 0;
    Bld_Hash ch = bld__content_hash(art);
    return ch.value == stored;
}

static void bld__cache_write_meta(Bld* b, Bld_Hash key, Bld_Hash content_hash) {
    Bld_Path meta = bld__cache_art_meta(b, key);
    const char* data = bld_str_fmt("%" PRIu64, content_hash.value);
    bld_fs_write_file(meta, data, strlen(data));
}

/* ---- Public interface ---- */

static uint64_t bld__tmp_counter = 0;

Bld_Path bld__step_artifact(Bld* b, Bld_Step* s) {
    return bld__cache_art(b, s->cache_key);
}

Bld_Path bld__target_artifact(Bld* b, Bld_Target* t) {
    return bld__step_artifact(b, t->exit);
}

Bld_Path bld__cache_tmp(Bld* b) {
    uint64_t id = bld__atomic_fetch_add64(&bld__tmp_counter, 1);
    return bld_path_join(bld_path_join(b->cache, bld_path("tmp")), bld_path_fmt("%" PRIu64, id));
}

int bld__cache_has(Bld* b, Bld_Step* step) {
    /* compute cache_key from input_hash + depfile */
    Bld_Hash h = step->input_hash;
    if (step->has_depfile) {
        Bld_Path dep = bld__depfile_path(b, step);
        if (bld_fs_exists(dep))
            h = bld_hash_combine(h, bld__hash_depfile(dep));
    }
    step->cache_key = h;
    step->hash_valid = true;

    if (!step->action) return 1;
    if (step->phony) return 0;
    if (!bld_fs_exists(bld__step_artifact(b, step))) return 0;
    /* depfile missing is OK if compilation failed (e.g. feature check for
     * nonexistent header). The artifact ("0") is still valid. Freshness
     * is tracked via system include path hash in the recipe instead. */
    if (step->has_depfile) {
        Bld_Path dep = bld__depfile_path(b, step);
        if (bld_fs_exists(dep)) { /* depfile exists — already included in cache_key above */ }
        /* else: no depfile, cache_key was computed without it — that's fine */
    }
    if (!bld__cache_validate(b, step)) return 0;
    return 1;
}

void bld__cache_store(Bld* b, Bld_Step* step, Bld_Path tmp_out, Bld_Path tmp_dep) {
    /* store depfile and update cache key */
    if (step->has_depfile && bld_fs_exists(tmp_dep)) {
        Bld_Path cached_dep = bld__depfile_path(b, step);
        bld_fs_mkdir_p(bld_path_parent(cached_dep));
        bld_fs_rename(tmp_dep, cached_dep);
        step->cache_key = bld_hash_combine(step->input_hash, bld__hash_depfile(cached_dep));
    }

    /* store artifact */
    Bld_Path expected = bld__step_artifact(b, step);
    if (bld_fs_exists(tmp_out)) {
        bld_fs_mkdir_p(bld_path_parent(expected));
        bld_fs_rename(tmp_out, expected);
    }
    if (!bld_fs_exists(expected)) return;

    /* content hash + early cutoff + meta */
    Bld_Hash ch = bld__content_hash(expected);
    if (step->content_hash && ch.value != step->cache_key.value) {
        step->cache_key = ch;
        Bld_Path new_art = bld__step_artifact(b, step);
        if (bld_fs_exists(new_art)) bld_fs_remove_all(new_art);
        bld_fs_mkdir_p(bld_path_parent(new_art));
        bld_fs_rename(expected, new_art);
    }
    bld__cache_write_meta(b, step->cache_key, ch);
}
