/* bld/bld_build.c — target/step construction, depfile parsing, all build graph logic */
#pragma once

#include "bld_core_impl.c"

/* ================================================================
 *  Depfile parsing
 * ================================================================ */

/* Parse a make-format depfile, return list of dependency paths (arena-allocated).
   Format: target: dep1 dep2 \
           dep3 dep4
   Backslash-newline is continuation. Backslash-space is escaped space in filename. */
static Bld_PathList bld__parse_depfile(Bld_Path depfile) {
    Bld_PathList deps = {0};
    size_t len;
    const char* content = bld_fs_read_file(depfile, &len);

    /* skip past first ':' (the target part) */
    const char* p = content;
    const char* end = content + len;
    while (p < end && *p != ':') p++;
    if (p < end) p++; /* skip ':' */

    /* parse dependency paths */
    char* file = bld_arena_alloc(len + 1); /* worst case: one big filename */
    size_t flen = 0;

    while (p < end) {
        char c = *p++;
        if (c == '\\' && p < end) {
            char next = *p;
            if (next == '\n') { p++; continue; }           /* line continuation */
            if (next == ' ')  { p++; file[flen++] = ' '; continue; } /* escaped space */
            file[flen++] = c; /* literal backslash */
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

/* Hash all files listed in a depfile. Returns combined hash. */
static Bld_Hash bld__hash_depfile_contents(Bld_Path depfile) {
    Bld_PathList deps = bld__parse_depfile(depfile);
    Bld_Hash h = {0};
    for (size_t i = 0; i < deps.count; i++) {
        if (bld_fs_is_file(deps.items[i]))
            h = bld_hash_combine(h, bld_hash_file(deps.items[i]));
    }
    return h;
}

/* ================================================================
 *  Helpers
 * ================================================================ */

static const char** bld__dup_strarray(const char** arr) {
    if (!arr) return NULL;
    size_t n = 0;
    while (arr[n]) n++;
    const char** copy = bld_arena_alloc((n + 1) * sizeof(const char*));
    for (size_t i = 0; i < n; i++) copy[i] = bld_str_dup(arr[i]);
    copy[n] = NULL;
    return copy;
}

/* ================================================================
 *  Clone
 * ================================================================ */

Bld_CompileFlags bld_clone_compile_flags(Bld_CompileFlags f) {
    Bld_CompileFlags c = f;
    if (f.extra_flags) c.extra_flags = bld_str_dup(f.extra_flags);
    c.defines = bld__dup_strarray(f.defines);
    c.include_dirs = bld__dup_strarray(f.include_dirs);
    c.system_include_dirs = bld__dup_strarray(f.system_include_dirs);
    return c;
}

/* ================================================================
 *  Build mode defaults
 * ================================================================ */

Bld_CompileFlags bld_default_compile_flags(Bld* b) {
    Bld_CompileFlags f = {0};
    switch (b->settings.mode) {
        case BLD_MODE_DEBUG:
            f.optimize = BLD_OPT_O0;
            break;
        case BLD_MODE_RELEASE: {
            f.optimize = BLD_OPT_O2;
            const char** defs = bld_arena_alloc(2 * sizeof(const char*));
            defs[0] = "NDEBUG"; defs[1] = NULL;
            f.defines = defs;
            break;
        }
        default: break;
    }
    return f;
}

Bld_LinkFlags bld_default_link_flags(Bld* b) {
    Bld_LinkFlags f = {0};
    switch (b->settings.mode) {
        case BLD_MODE_DEBUG:
            f.debug_info = BLD_ON;
            break;
        case BLD_MODE_RELEASE:
            f.debug_info = BLD_OFF;
            break;
        default: break;
    }
    return f;
}

/* ================================================================
 *  Per-file overrides
 * ================================================================ */

void bld__override_file(Bld_Target* t, const char* file, const Bld_CompileFlags* flags) {
    Bld_FileOverride ov;
    ov.file = bld_str_dup(file);
    ov.flags = *flags;
    ov.flags.defines = bld__dup_strarray(flags->defines);
    ov.flags.include_dirs = bld__dup_strarray(flags->include_dirs);
    ov.flags.system_include_dirs = bld__dup_strarray(flags->system_include_dirs);
    if (flags->extra_flags) ov.flags.extra_flags = bld_str_dup(flags->extra_flags);
    bld_da_push(&t->file_overrides, ov);
}

/* ================================================================
 *  External dependencies
 * ================================================================ */

Bld_Dep* bld__dep(const Bld_Dep* d) {
    Bld_Dep* c = bld_arena_alloc(sizeof(Bld_Dep));
    *c = *d;
    c->found = 1;
    if (d->name) c->name = bld_str_dup(d->name);
    c->include_dirs = bld__dup_strarray(d->include_dirs);
    c->system_include_dirs = bld__dup_strarray(d->system_include_dirs);
    c->libs = bld__dup_strarray(d->libs);
    c->lib_dirs = bld__dup_strarray(d->lib_dirs);
    if (d->extra_cflags) c->extra_cflags = bld_str_dup(d->extra_cflags);
    if (d->extra_ldflags) c->extra_ldflags = bld_str_dup(d->extra_ldflags);
    return c;
}

void bld_use_dep(Bld_Target* t, Bld_Dep* dep) {
    bld_da_push(&t->ext_deps, dep);
}

/* parse pkg-config output into arrays */
static void bld__parse_pkg_flags(const char* output, Bld_Dep* dep, int is_libs) {
    if (!output || !output[0]) return;
    Bld_Strings sysinc = {0}, libs = {0}, libdirs = {0};

    const char* p = output;
    while (*p) {
        while (*p == ' ' || *p == '\n') p++;
        if (!*p) break;
        const char* tok = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        size_t len = (size_t)(p - tok);
        char* s = bld_arena_alloc(len + 1);
        memcpy(s, tok, len); s[len] = '\0';

        if (is_libs) {
            if (strncmp(s, "-L", 2) == 0)      bld_da_push(&libdirs, bld_str_dup(s + 2));
            else if (strncmp(s, "-l", 2) == 0)  bld_da_push(&libs, bld_str_dup(s + 2));
            /* other flags go into extra_ldflags — skip for now */
        } else {
            if (strncmp(s, "-isystem", 8) == 0) {
                /* -isystem may be followed by path as next token or attached */
                if (len > 8) bld_da_push(&sysinc, bld_str_dup(s + 8));
                else {
                    while (*p == ' ') p++;
                    const char* t2 = p;
                    while (*p && *p != ' ' && *p != '\n') p++;
                    if (p > t2) {
                        char* s2 = bld_arena_alloc((size_t)(p - t2) + 1);
                        memcpy(s2, t2, (size_t)(p - t2)); s2[p - t2] = '\0';
                        bld_da_push(&sysinc, s2);
                    }
                }
            }
            else if (strncmp(s, "-I", 2) == 0)  bld_da_push(&sysinc, bld_str_dup(s + 2));
            /* other flags go into extra_cflags — skip for now */
        }
    }

    if (is_libs) {
        if (libs.count) { bld_da_push(&libs, (const char*)NULL); dep->libs = libs.items; }
        if (libdirs.count) { bld_da_push(&libdirs, (const char*)NULL); dep->lib_dirs = libdirs.items; }
    } else {
        if (sysinc.count) { bld_da_push(&sysinc, (const char*)NULL); dep->system_include_dirs = sysinc.items; }
    }
}

Bld_Dep* bld_find_pkg(const char* name) {
    Bld_Dep* dep = bld_arena_alloc(sizeof(Bld_Dep));
    memset(dep, 0, sizeof(*dep));
    dep->name = bld_str_dup(name);

    const char* check = bld_str_fmt("pkg-config --exists %s 2>/dev/null", name);
    if (system(check) != 0) {
        dep->found = 0;
        return dep;
    }
    dep->found = 1;

    const char* cmd_cf = bld_str_fmt("pkg-config --cflags %s 2>/dev/null", name);
    FILE* f = popen(cmd_cf, "r");
    if (f) {
        char buf[4096] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        pclose(f);
        bld__parse_pkg_flags(buf, dep, 0);
    }

    const char* cmd_libs = bld_str_fmt("pkg-config --libs %s 2>/dev/null", name);
    f = popen(cmd_libs, "r");
    if (f) {
        char buf[4096] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        pclose(f);
        bld__parse_pkg_flags(buf, dep, 1);
    }

    return dep;
}

/* ================================================================
 *  Step primitives
 * ================================================================ */

static Bld_Step* bld__alloc_step(Bld* b, const char* name, int silent) {
    Bld_Step* s = bld_arena_alloc(sizeof(Bld_Step));
    memset(s, 0, sizeof(*s));
    s->name = name ? bld_str_dup(name) : "";
    s->silent = silent;
    pthread_mutex_init(&s->mutex, NULL);
    pthread_cond_init(&s->cond, NULL);
    bld_da_push(&b->all_steps, s);
    return s;
}

/* ================================================================
 *  Target primitives
 * ================================================================ */

static void bld__init_target(Bld* b, Bld_Target* t, Bld_TargetKind kind,
                              const char* name, const char* desc) {
    t->kind = kind;
    t->name = name ? bld_str_dup(name) : "";
    t->desc = desc ? bld_str_dup(desc) : "";
    for (size_t i = 0; i < b->all_targets.count; i++)
        if (strcmp(b->all_targets.items[i]->name, t->name) == 0)
            bld_panic("duplicate target name: '%s'\n", t->name);
    t->entry = bld__alloc_step(b, bld_str_fmt("%s:entry", t->name), 1);
    t->exit  = bld__alloc_step(b, bld_str_dup(t->name), 0);
    bld_da_push(&t->exit->deps, t->entry);
    bld_da_push(&b->all_targets, t);
}

/* ================================================================
 *  Safe casts
 * ================================================================ */

static Bld_Exe* bld__as_exe(Bld_Target* t) {
    if (t->kind != BLD_TGT_EXE) bld_panic("expected exe target '%s', got kind %d\n", t->name, t->kind);
    return (Bld_Exe*)t;
}

static Bld_Lib* bld__as_lib(Bld_Target* t) {
    if (t->kind != BLD_TGT_LIB) bld_panic("expected lib target '%s', got kind %d\n", t->name, t->kind);
    return (Bld_Lib*)t;
}

/* ================================================================
 *  Target operations
 * ================================================================ */

void bld_depends_on(Bld_Target* a, Bld_Target* b) {
    bld_da_push(&a->entry->deps, b->exit);
}

void bld_link_with(Bld_Target* a, Bld_Target* b) {
    bld_da_push(&a->link_deps, b);
}

/* collect transitive link deps (depth-first, deduplicated) */
static void bld__collect_link_deps(Bld_Target* t, Bld_Target*** items, size_t* count, size_t* cap) {
    for (size_t i = 0; i < t->link_deps.count; i++) {
        Bld_Target* dep = t->link_deps.items[i];
        int found = 0;
        for (size_t j = 0; j < *count; j++)
            if ((*items)[j] == dep) { found = 1; break; }
        if (found) continue;
        bld__collect_link_deps(dep, items, count, cap);
        /* push */
        if (*count >= *cap) {
            size_t nc = *cap ? *cap * 2 : 8;
            *items = bld_arena_realloc(*items, *cap * sizeof(Bld_Target*), nc * sizeof(Bld_Target*));
            *cap = nc;
        }
        (*items)[(*count)++] = dep;
    }
}

static void bld__push_ext_dep_dedup(Bld_Exe* exe, Bld_Dep* dep) {
    for (size_t i = 0; i < exe->resolved_ext_deps.count; i++)
        if (exe->resolved_ext_deps.items[i] == dep) return;
    bld_da_push(&exe->resolved_ext_deps, dep);
}

static Bld_Step* bld__ensure_publish_step(Bld* b, Bld_Lib* lib);

/* wire up link deps into steps (called after configure, before build) */
static void bld__resolve_link_deps(Bld* b) {
    for (size_t i = 0; i < b->all_targets.count; i++) {
        Bld_Target* t = b->all_targets.items[i];

        /* add target's own ext_deps to resolved list (always, even with no link_deps) */
        if (t->kind == BLD_TGT_EXE) {
            Bld_Exe* exe = bld__as_exe(t);
            for (size_t k = 0; k < t->ext_deps.count; k++)
                bld__push_ext_dep_dedup(exe, t->ext_deps.items[k]);
        }

        if (t->link_deps.count == 0) continue;

        Bld_Target** all_deps = NULL;
        size_t dep_count = 0, dep_cap = 0;
        bld__collect_link_deps(t, &all_deps, &dep_count, &dep_cap);

        for (size_t j = 0; j < dep_count; j++) {
            Bld_Target* dep = all_deps[j];
            bld_da_push(&t->exit->deps, dep->exit);

            if (t->kind == BLD_TGT_LIB && dep->kind == BLD_TGT_LIB) {
                /* static lib → static lib: ordering only */
            } else if (dep->kind == BLD_TGT_LIB && ((Bld_Lib*)dep)->opts.shared && t->kind == BLD_TGT_EXE) {
                Bld_Lib* slib = (Bld_Lib*)dep;
                bld_da_push(&bld__as_exe(t)->shared_libs, slib);
                Bld_Step* pub = bld__ensure_publish_step(b, slib);
                bld_da_push(&t->exit->deps, pub);
            } else {
                bld_da_push(&t->exit->inputs, dep->exit);
            }

            if (t->kind == BLD_TGT_EXE) {
                Bld_Exe* exe = bld__as_exe(t);
                for (size_t k = 0; k < dep->ext_deps.count; k++)
                    bld__push_ext_dep_dedup(exe, dep->ext_deps.items[k]);
            }
        }
    }
}

Bld_LazyPath bld_output(Bld_Target* t) {
    return (Bld_LazyPath){.source = t, .path = bld_path("")};
}

Bld_LazyPath bld_output_sub(Bld_Target* t, const char* subpath) {
    return (Bld_LazyPath){.source = t, .path = bld_path(bld_str_dup(subpath))};
}

void bld_add_include_dir(Bld_Target* t, Bld_LazyPath dir) {
    if (dir.source)
        bld_da_push(&t->entry->deps, dir.source->exit);
    bld_da_push(&t->include_dirs, dir);
}

void bld_add_source(Bld_Target* t, Bld_LazyPath src) {
    if (src.source)
        bld_da_push(&t->entry->deps, src.source->exit);
    bld_da_push(&t->lazy_sources, src);
}

/* ================================================================
 *  Command buffer
 * ================================================================ */

typedef struct { char* items; size_t count, cap; } Bld__Cmd;

static void bld__cmd_appendf(Bld__Cmd* cmd, const char* fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n <= 0) { va_end(ap2); return; }
    size_t needed = cmd->count + (size_t)n + 1;
    if (needed > cmd->cap) {
        size_t new_cap = cmd->cap ? cmd->cap : 64;
        while (new_cap < needed) new_cap *= 2;
        cmd->items = bld_arena_realloc(cmd->items, cmd->cap, new_cap);
        cmd->cap = new_cap;
    }
    vsnprintf(cmd->items + cmd->count, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    cmd->count += (size_t)n;
}

/* shell-safe single-quote a string */
static void bld__cmd_append_sq(Bld__Cmd* cmd, const char* s) {
    if (!strchr(s, '\'')) {
        bld__cmd_appendf(cmd, "'%s'", s);
    } else {
        bld__cmd_appendf(cmd, "'");
        for (const char* c = s; *c; c++) {
            if (*c == '\'') bld__cmd_appendf(cmd, "'\\''");
            else { char tmp[2] = {*c, 0}; bld__cmd_appendf(cmd, "%s", tmp); }
        }
        bld__cmd_appendf(cmd, "'");
    }
}

/* ================================================================
 *  Flag rendering
 * ================================================================ */

static const char* bld__resolve_optimize(Bld_Optimize o) {
    switch (o) {
        case BLD_OPT_DEFAULT: return "";
        case BLD_OPT_O0: return "-O0"; case BLD_OPT_O1: return "-O1";
        case BLD_OPT_O2: return "-O2"; case BLD_OPT_O3: return "-O3";
        case BLD_OPT_Os: return "-Os"; case BLD_OPT_OFAST: return "-Ofast";
    }
    return "";
}

static const char* bld__resolve_c_standard(Bld_CStd s) {
    switch (s) {
        case BLD_C_DEFAULT: return "";
        case BLD_C_90: return "-std=c90";
        case BLD_C_99: return "-std=c99"; case BLD_C_11: return "-std=c11";
        case BLD_C_17: return "-std=c17"; case BLD_C_23: return "-std=c23";
        case BLD_C_GNU90: return "-std=gnu90"; case BLD_C_GNU99: return "-std=gnu99";
        case BLD_C_GNU11: return "-std=gnu11"; case BLD_C_GNU17: return "-std=gnu17";
        case BLD_C_GNU23: return "-std=gnu23";
    }
    return "";
}

static const char* bld__resolve_cxx_standard(Bld_CxxStd s) {
    switch (s) {
        case BLD_CXX_DEFAULT: return "";
        case BLD_CXX_11: return "-std=c++11"; case BLD_CXX_14: return "-std=c++14";
        case BLD_CXX_17: return "-std=c++17"; case BLD_CXX_20: return "-std=c++20";
        case BLD_CXX_23: return "-std=c++23";
        case BLD_CXX_GNU11: return "-std=gnu++11"; case BLD_CXX_GNU14: return "-std=gnu++14";
        case BLD_CXX_GNU17: return "-std=gnu++17"; case BLD_CXX_GNU20: return "-std=gnu++20";
        case BLD_CXX_GNU23: return "-std=gnu++23";
    }
    return "";
}

static int bld__toggle_val(Bld_Toggle t, Bld_Toggle global) {
    if (t == BLD_ON) return 1;
    if (t == BLD_OFF) return 0;
    return global == BLD_ON ? 1 : 0;
}

static void bld__cmd_render_compile(Bld__Cmd* cmd, Bld* b, Bld_Compiler* comp,
                                     const Bld_CompileFlags* f, const Bld_LinkFlags* lf) {
    bld__cmd_appendf(cmd, "%s", comp->driver);
    Bld_Optimize opt = f->optimize ? f->optimize : b->global_optimize;
    int warnings = (f->warnings == BLD_ON) ? 1 : (f->warnings == BLD_OFF) ? 0 : b->global_warnings;

    const char* os = bld__resolve_optimize(opt); if (os[0]) bld__cmd_appendf(cmd, " %s", os);
    const char* ss = "";
    if (comp->lang == BLD_LANG_C) ss = bld__resolve_c_standard(comp->c.standard);
    else if (comp->lang == BLD_LANG_CXX) ss = bld__resolve_cxx_standard(comp->cxx.standard);
    if (ss[0]) bld__cmd_appendf(cmd, " %s", ss);
    if (!warnings) bld__cmd_appendf(cmd, " -w");
    if (warnings) bld__cmd_appendf(cmd, " -Wall");
    if (f->extra_flags && f->extra_flags[0]) bld__cmd_appendf(cmd, " %s", f->extra_flags);
    if (f->defines) for (const char** d = f->defines; *d; d++) {
        bld__cmd_appendf(cmd, " -D");
        bld__cmd_append_sq(cmd, *d);
    }
    if (f->include_dirs) for (const char** d = f->include_dirs; *d; d++) bld__cmd_appendf(cmd, " -I%s", *d);
    if (f->system_include_dirs) for (const char** d = f->system_include_dirs; *d; d++) bld__cmd_appendf(cmd, " -isystem %s", *d);
    if (lf) {
        if (bld__toggle_val(lf->debug_info, b->global_link.debug_info)) bld__cmd_appendf(cmd, " -g");
        if (bld__toggle_val(lf->asan, b->global_link.asan)) bld__cmd_appendf(cmd, " -fsanitize=address");
        if (bld__toggle_val(lf->lto, b->global_link.lto)) bld__cmd_appendf(cmd, " -flto");
    }
}

/* ================================================================
 *  Cache — artifact storage, validation, tmp files
 * ================================================================ */

/* ---- Path helpers ---- */

static Bld_Path bld__cache_art(Bld* b, Bld_Hash h) {
    return bld_path_join(bld_path_join(b->cache, bld_path("arts")), bld_path_fmt("%" PRIu64, h.value));
}

static Bld_Path bld__cache_art_meta(Bld* b, Bld_Hash h) {
    return bld_path_join(bld_path_join(b->cache, bld_path("arts")), bld_path_fmt("%" PRIu64 ".meta", h.value));
}

static Bld_Path bld__step_artifact(Bld* b, Bld_Step* s) { return bld__cache_art(b, s->cache_key); }
static Bld_Path bld__target_artifact(Bld* b, Bld_Target* t) { return bld__step_artifact(b, t->exit); }

static Bld_Path bld__step_depfile_cache(Bld* b, Bld_Step* s) {
    size_t len = strlen(s->name);
    char* safe = bld_arena_alloc(len + 1);
    for (size_t i = 0; i < len; i++) safe[i] = (s->name[i] == '/') ? '_' : s->name[i];
    safe[len] = '\0';
    return bld_path_join(bld_path_join(b->cache, bld_path("deps")), bld_path_fmt("%s.d", safe));
}

static uint64_t bld__tmp_counter = 0;
static Bld_Path bld__cache_tmp(Bld* b) {
    uint64_t id = __atomic_fetch_add(&bld__tmp_counter, 1, __ATOMIC_RELAXED);
    return bld_path_join(bld_path_join(b->cache, bld_path("tmp")), bld_path_fmt("%" PRIu64, id));
}

/* ---- Content validation ---- */

static Bld_Hash bld__hash_artifact(Bld_Path art) {
    return bld_fs_is_dir(art) ? bld_hash_dir(art) : bld_hash_file(art);
}

static void bld__cache_write_meta(Bld* b, Bld_Hash key, Bld_Hash content_hash) {
    Bld_Path meta = bld__cache_art_meta(b, key);
    const char* data = bld_str_fmt("%" PRIu64, content_hash.value);
    bld_fs_write_file(meta, data, strlen(data));
}

static int bld__cache_validate(Bld* b, Bld_Step* s) {
    Bld_Path art = bld__step_artifact(b, s);
    Bld_Path meta = bld__cache_art_meta(b, s->cache_key);
    if (!bld_fs_exists(meta)) return 0;
    size_t len;
    const char* data = bld_fs_read_file(meta, &len);
    uint64_t stored = 0;
    if (sscanf(data, "%" SCNu64, &stored) != 1) return 0;
    Bld_Hash ch = bld__hash_artifact(art);
    return ch.value == stored;
}

/* ---- Cache lookup ---- */

static int bld__cache_has(Bld* b, Bld_Step* step) {
    if (!step->action) return 1;
    if (step->phony) return 0;
    if (!bld_fs_exists(bld__step_artifact(b, step))) return 0;
    if (step->has_depfile && !bld_fs_exists(bld__step_depfile_cache(b, step))) return 0;
    if (!bld__cache_validate(b, step)) return 0;
    return 1;
}

/* ---- Cache store ---- */

static void bld__cache_store_depfile(Bld* b, Bld_Step* step, Bld_Path tmp_dep) {
    if (!step->has_depfile || !bld_fs_exists(tmp_dep)) return;
    Bld_Path cached_dep = bld__step_depfile_cache(b, step);
    bld_fs_mkdir_p(bld_path_parent(cached_dep));
    bld_fs_rename(tmp_dep, cached_dep);
    step->cache_key = bld_hash_combine(step->input_hash,
                                              bld__hash_depfile_contents(cached_dep));
}

static void bld__cache_store_artifact(Bld* b, Bld_Step* step, Bld_Path tmp_out) {
    Bld_Path expected = bld__step_artifact(b, step);
    if (bld_fs_exists(tmp_out)) {
        bld_fs_mkdir_p(bld_path_parent(expected));
        bld_fs_rename(tmp_out, expected);
    }
    if (!bld_fs_exists(expected)) return;
    Bld_Hash ch = bld__hash_artifact(expected);
    /* early cutoff: if content differs from input hash, use content hash as key */
    if (step->content_hash && ch.value != step->cache_key.value) {
        step->cache_key = ch;
        Bld_Path new_art = bld__step_artifact(b, step);
        if (bld_fs_exists(new_art)) bld_fs_remove_all(new_art);
        bld_fs_mkdir_p(bld_path_parent(new_art));
        bld_fs_rename(expected, new_art);
    }
    bld__cache_write_meta(b, step->cache_key, ch);
}

static void bld__cmd_append_inputs(Bld__Cmd* cmd, Bld* b, Bld_Step* exit_step) {
    for (size_t i = 0; i < exit_step->inputs.count; i++) {
        Bld_Step* inp = exit_step->inputs.items[i];
        if (inp->hash_valid) bld__cmd_appendf(cmd, " \"%s\"", bld__step_artifact(b, inp).s);
    }
}

static void bld__cmd_render_link(Bld__Cmd* cmd, Bld* b, Bld_Compiler* comp, const Bld_LinkFlags* lf) {
    bld__cmd_appendf(cmd, "%s", comp->driver);
    if (lf) {
        if (bld__toggle_val(lf->debug_info, b->global_link.debug_info)) bld__cmd_appendf(cmd, " -g");
        if (bld__toggle_val(lf->asan, b->global_link.asan)) bld__cmd_appendf(cmd, " -fsanitize=address");
        if (bld__toggle_val(lf->lto, b->global_link.lto)) bld__cmd_appendf(cmd, " -flto");
    }
}

/* ================================================================
 *  Flag hashing
 * ================================================================ */

static Bld_Hash bld__hash_compile_flags(Bld_Hash h, const Bld_CompileFlags* f) {
    h = bld_hash_combine(h, (Bld_Hash){f->optimize});
    h = bld_hash_combine(h, (Bld_Hash){f->warnings});
    if (f->extra_flags) h = bld_hash_combine(h, bld_hash_str(f->extra_flags));
    if (f->defines) for (const char** d = f->defines; *d; d++) h = bld_hash_combine(h, bld_hash_str(*d));
    if (f->include_dirs) for (const char** d = f->include_dirs; *d; d++) h = bld_hash_combine(h, bld_hash_str(*d));
    if (f->system_include_dirs) for (const char** d = f->system_include_dirs; *d; d++) h = bld_hash_combine(h, bld_hash_str(*d));
    return h;
}

/* link flags that affect compilation (-g, -fsanitize, -flto) */
static Bld_Hash bld__hash_link_compile_flags(Bld_Hash h, const Bld_LinkFlags* f) {
    h = bld_hash_combine(h, (Bld_Hash){f->asan});
    h = bld_hash_combine(h, (Bld_Hash){f->debug_info});
    h = bld_hash_combine(h, (Bld_Hash){f->lto});
    return h;
}

/* all link flags (for link step hash) */
static Bld_Hash bld__hash_link_flags(Bld_Hash h, const Bld_LinkFlags* f) {
    h = bld__hash_link_compile_flags(h, f);
    if (f->extra_flags) h = bld_hash_combine(h, bld_hash_str(f->extra_flags));
    return h;
}

/* ================================================================
 *  Lazy path resolution
 * ================================================================ */

static Bld_Path bld__resolve_lazy(Bld* b, Bld_LazyPath lp) {
    if (lp.source) {
        Bld_Path art = bld__target_artifact(b, lp.source);
        if (lp.path.s[0]) return bld_path_join(art, lp.path);
        return art;
    }
    return bld_path_join(b->root, lp.path);
}

/* ================================================================
 *  Language inference
 * ================================================================ */

static Bld_Lang bld__infer_lang(const char* path) {
    const char* ext = bld_path_ext(bld_path(path));
    if (!strcmp(ext, ".c") || !strcmp(ext, ".m")) return BLD_LANG_C;
    if (!strcmp(ext, ".cpp") || !strcmp(ext, ".cc") || !strcmp(ext, ".cxx") ||
        !strcmp(ext, ".mm") || !strcmp(ext, ".C")) return BLD_LANG_CXX;
    if (!strcmp(ext, ".s") || !strcmp(ext, ".S")) return BLD_LANG_ASM;
    return BLD_LANG_C; /* unknown -> C */
}

/* ================================================================
 *  Obj step (compile .c -> .o)
 * ================================================================ */

typedef struct {
    Bld* b; Bld_Target* parent;
    Bld_Path source; Bld_Path orig_source;
    Bld_LazyPath lazy_source; /* if set, resolved at build time instead of source */
    Bld_CompileFlags compile; Bld_LinkFlags* link; int pic;
    Bld_Lang lang;
} Bld__ObjCtx;

/* merge override on top of base: non-zero fields replace */
static Bld_CompileFlags bld__merge_compile_flags(Bld_CompileFlags base, const Bld_CompileFlags* ov) {
    if (ov->optimize)           base.optimize = ov->optimize;
    if (ov->warnings)           base.warnings = ov->warnings;
    if (ov->extra_flags)        base.extra_flags = ov->extra_flags;
    if (ov->defines)            base.defines = ov->defines;
    if (ov->include_dirs)       base.include_dirs = ov->include_dirs;
    if (ov->system_include_dirs) base.system_include_dirs = ov->system_include_dirs;
    return base;
}

/* find per-file override by suffix match */
static const Bld_CompileFlags* bld__find_file_override(Bld_Target* t, const char* source) {
    for (size_t i = 0; i < t->file_overrides.count; i++) {
        const char* pattern = t->file_overrides.items[i].file;
        size_t plen = strlen(pattern), slen = strlen(source);
        if (plen <= slen && strcmp(source + slen - plen, pattern) == 0)
            return &t->file_overrides.items[i].flags;
    }
    return NULL;
}

/* resolve compile flags with per-file override applied at runtime */
static Bld_CompileFlags bld__resolve_obj_flags(Bld__ObjCtx* c) {
    const Bld_CompileFlags* ov = bld__find_file_override(c->parent, c->orig_source.s);
    return ov ? bld__merge_compile_flags(c->compile, ov) : c->compile;
}

static Bld_Path bld__obj_source(Bld__ObjCtx* c) {
    if (c->lazy_source.source)
        return bld__resolve_lazy(c->b, c->lazy_source);
    return c->source;
}

/* render compile command prefix (flags + includes, without -c -o -MMD) */
static void bld__render_obj_cmd(Bld__Cmd* cmd, Bld__ObjCtx* c) {
    Bld_CompileFlags flags = bld__resolve_obj_flags(c);
    Bld_Compiler* comp = bld_compiler(c->b, c->lang);
    bld__cmd_render_compile(cmd, c->b, comp, &flags, c->link);
    if (c->pic) bld__cmd_appendf(cmd, " -fPIC");
    for (size_t i = 0; i < c->parent->include_dirs.count; i++) {
        Bld_Path dir = bld__resolve_lazy(c->b, c->parent->include_dirs.items[i]);
        bld__cmd_appendf(cmd, " -I\"%s\"", dir.s);
    }
    for (size_t i = 0; i < c->parent->ext_deps.count; i++) {
        Bld_Dep* d = c->parent->ext_deps.items[i];
        if (d->include_dirs) for (const char** p = d->include_dirs; *p; p++) bld__cmd_appendf(cmd, " -I%s", *p);
        if (d->system_include_dirs) for (const char** p = d->system_include_dirs; *p; p++) bld__cmd_appendf(cmd, " -isystem %s", *p);
        if (d->extra_cflags && d->extra_cflags[0]) bld__cmd_appendf(cmd, " %s", d->extra_cflags);
    }
    bld__cmd_appendf(cmd, " \"%s\"", bld__obj_source(c).s);
}

static Bld_ActionResult bld__obj_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    Bld__ObjCtx* c = ctx;
    Bld__Cmd cmd = {0};
    bld__render_obj_cmd(&cmd, c);
    bld__cmd_appendf(&cmd, " -c -o %s", output.s);
    if (depfile.s && depfile.s[0])
        bld__cmd_appendf(&cmd, " -MMD -MF %s", depfile.s);
    if (c->b->settings.verbose) bld_log_action("compile: %s\n", cmd.items);
    int rc = system(cmd.items);
    if (rc != 0) return BLD_ACTION_FAILED;
    return BLD_ACTION_OK;
}

static Bld_Hash bld__obj_recipe_hash(void* ctx, Bld_Hash h) {
    Bld__ObjCtx* c = ctx;
    Bld_CompileFlags flags = bld__resolve_obj_flags(c);
    Bld_Path src = bld__obj_source(c);
    Bld_Compiler* comp = bld_compiler(c->b, c->lang);
    h = bld_hash_combine(h, comp->identity_hash);
    /* hash compiler standard */
    if (comp->lang == BLD_LANG_C) h = bld_hash_combine(h, (Bld_Hash){comp->c.standard});
    else if (comp->lang == BLD_LANG_CXX) h = bld_hash_combine(h, (Bld_Hash){comp->cxx.standard});
    h = bld_hash_combine(h, bld_hash_str(src.s));
    h = bld_hash_combine(h, bld_hash_file(src));
    h = bld__hash_compile_flags(h, &flags);
    if (c->link) h = bld__hash_link_compile_flags(h, c->link);
    h = bld_hash_combine(h, (Bld_Hash){c->pic});
    h = bld_hash_combine(h, (Bld_Hash){c->parent->include_dirs.count});
    for (size_t i = 0; i < c->parent->include_dirs.count; i++) {
        Bld_LazyPath lp = c->parent->include_dirs.items[i];
        if (lp.path.s[0]) h = bld_hash_combine(h, bld_hash_str(lp.path.s));
    }
    /* hash ext_deps compile-side */
    for (size_t i = 0; i < c->parent->ext_deps.count; i++) {
        Bld_Dep* d = c->parent->ext_deps.items[i];
        if (d->include_dirs) for (const char** p = d->include_dirs; *p; p++) h = bld_hash_combine(h, bld_hash_str(*p));
        if (d->system_include_dirs) for (const char** p = d->system_include_dirs; *p; p++) h = bld_hash_combine(h, bld_hash_str(*p));
        if (d->extra_cflags) h = bld_hash_combine(h, bld_hash_str(d->extra_cflags));
    }
    return h;
}

static Bld_Step* bld__add_obj(Bld* b, Bld_Target* parent, Bld_Path source,
                               Bld_CompileFlags compile, Bld_LinkFlags* link, int pic,
                               Bld_Lang target_lang) {
    Bld_Lang lang = (target_lang != BLD_LANG_AUTO) ? target_lang : bld__infer_lang(source.s);
    Bld_Path abs_src = bld_path_join(b->root, source);
    const char* name = bld_str_fmt("%s:%s", parent->name, bld_path_replace_ext(source, ".o").s);
    Bld_Step* s = bld__alloc_step(b, name, 1);
    s->has_depfile = 1;
    bld_da_push(&s->deps, parent->entry);

    Bld__ObjCtx* ctx = bld_arena_alloc(sizeof(Bld__ObjCtx));
    *ctx = (Bld__ObjCtx){.b = b, .parent = parent, .source = abs_src, .orig_source = source,
                          .compile = compile, .link = link, .pic = pic, .lang = lang};
    s->action = bld__obj_action;
    s->action_ctx = ctx;
    s->hash_fn = bld__obj_recipe_hash;
    s->hash_fn_ctx = ctx;
    return s;
}

static Bld_Step* bld__add_lazy_obj(Bld* b, Bld_Target* parent, Bld_LazyPath lazy_source,
                                    Bld_CompileFlags compile, Bld_LinkFlags* link, int pic,
                                    Bld_Lang target_lang) {
    Bld_Lang lang = (target_lang != BLD_LANG_AUTO) ? target_lang : bld__infer_lang(lazy_source.path.s);
    const char* src_name = lazy_source.source ? lazy_source.source->name : "gen";
    const char* name = bld_str_fmt("%s:lazy_%s.o", parent->name, src_name);
    Bld_Step* s = bld__alloc_step(b, name, 1);
    s->has_depfile = 1;
    bld_da_push(&s->deps, parent->entry);
    if (lazy_source.source)
        bld_da_push(&s->inputs, lazy_source.source->exit);

    Bld__ObjCtx* ctx = bld_arena_alloc(sizeof(Bld__ObjCtx));
    *ctx = (Bld__ObjCtx){.b = b, .parent = parent, .source = bld_path(""),
                          .orig_source = bld_path(""), .lazy_source = lazy_source,
                          .compile = compile, .link = link, .pic = pic, .lang = lang};
    s->action = bld__obj_action;
    s->action_ctx = ctx;
    s->hash_fn = bld__obj_recipe_hash;
    s->hash_fn_ctx = ctx;
    return s;
}

/* materialize lazy sources into obj steps (called after configure) */
static void bld__materialize_lazy_sources(Bld* b) {
    for (size_t i = 0; i < b->all_targets.count; i++) {
        Bld_Target* t = b->all_targets.items[i];
        if (t->lazy_sources.count == 0) continue;
        if (t->kind != BLD_TGT_EXE && t->kind != BLD_TGT_LIB) continue;

        Bld_StepList* obj_steps = (t->kind == BLD_TGT_EXE)
            ? &((Bld_Exe*)t)->obj_steps
            : &((Bld_Lib*)t)->obj_steps;
        Bld_CompileFlags compile = (t->kind == BLD_TGT_EXE)
            ? ((Bld_Exe*)t)->opts.compile
            : ((Bld_Lib*)t)->opts.compile;
        Bld_LinkFlags* link = (t->kind == BLD_TGT_EXE)
            ? &((Bld_Exe*)t)->opts.link
            : &((Bld_Lib*)t)->opts.link;
        int pic = (t->kind == BLD_TGT_LIB) ? 1 : 0;

        Bld_Lang target_lang = (t->kind == BLD_TGT_EXE)
            ? ((Bld_Exe*)t)->opts.lang : ((Bld_Lib*)t)->opts.lang;

        for (size_t j = 0; j < t->lazy_sources.count; j++) {
            Bld_Step* obj = bld__add_lazy_obj(b, t, t->lazy_sources.items[j], compile, link, pic, target_lang);
            bld_da_push(obj_steps, obj);
            bld_da_push(&t->exit->inputs, obj);
        }
    }
}

/* ================================================================
 *  Link compiler selection (strongest language: CXX > C > ASM)
 * ================================================================ */

static Bld_Compiler* bld__link_compiler(Bld* b, Bld_StepList* obj_steps) {
    for (size_t i = 0; i < obj_steps->count; i++) {
        Bld__ObjCtx* ctx = obj_steps->items[i]->action_ctx;
        if (ctx && ctx->lang == BLD_LANG_CXX) return bld_compiler(b, BLD_LANG_CXX);
    }
    return bld_compiler(b, BLD_LANG_C);
}

/* ================================================================
 *  Link exe
 * ================================================================ */

typedef struct { Bld* b; Bld_Exe* exe; } Bld__ExeCtx;

static Bld_ActionResult bld__link_exe_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)depfile;
    Bld__ExeCtx* c = ctx;
    Bld__Cmd cmd = {0};
    Bld_Compiler* linker = bld__link_compiler(c->b, &c->exe->obj_steps);
    bld__cmd_render_link(&cmd, c->b, linker, &c->exe->opts.link);
    bld__cmd_append_inputs(&cmd, c->b, c->exe->target.exit);
    if (c->exe->shared_libs.count > 0) {
        Bld_Path libdir = bld_path_join(c->b->out, bld_path("lib"));
        bld__cmd_appendf(&cmd, " -L\"%s\"", libdir.s);
        for (size_t i = 0; i < c->exe->shared_libs.count; i++) {
            Bld_Lib* lib = c->exe->shared_libs.items[i];
            bld__cmd_appendf(&cmd, " -l%s", lib->opts.name);
        }
        bld__cmd_appendf(&cmd, " -Wl,-rpath,$ORIGIN/../lib");
    }
    bld__cmd_appendf(&cmd, " -o %s", output.s);
    /* ext_deps: transitive link flags */
    for (size_t i = 0; i < c->exe->resolved_ext_deps.count; i++) {
        Bld_Dep* d = c->exe->resolved_ext_deps.items[i];
        if (d->lib_dirs) for (const char** p = d->lib_dirs; *p; p++) bld__cmd_appendf(&cmd, " -L%s", *p);
        if (d->libs) for (const char** p = d->libs; *p; p++) bld__cmd_appendf(&cmd, " -l%s", *p);
        if (d->extra_ldflags && d->extra_ldflags[0]) bld__cmd_appendf(&cmd, " %s", d->extra_ldflags);
    }
    if (c->exe->opts.link.extra_flags && c->exe->opts.link.extra_flags[0])
        bld__cmd_appendf(&cmd, " %s", c->exe->opts.link.extra_flags);
    if (c->b->settings.verbose) bld_log_action("link exe: %s\n", cmd.items);
    int rc = system(cmd.items);
    if (rc != 0) return BLD_ACTION_FAILED;
    return BLD_ACTION_OK;
}

static Bld_Hash bld__link_exe_recipe(void* ctx, Bld_Hash h) {
    Bld__ExeCtx* c = ctx;
    h = bld_hash_combine(h, bld_hash_str(c->exe->opts.name));
    Bld_Compiler* linker = bld__link_compiler(c->b, &c->exe->obj_steps);
    h = bld_hash_combine(h, linker->identity_hash);
    h = bld__hash_link_flags(h, &c->exe->opts.link);
    for (size_t i = 0; i < c->exe->shared_libs.count; i++) {
        Bld_Lib* lib = (Bld_Lib*)c->exe->shared_libs.items[i];
        h = bld_hash_combine(h, bld_hash_str(lib->opts.name));
        h = bld_hash_combine(h, lib->target.exit->cache_key);
    }
    for (size_t i = 0; i < c->exe->resolved_ext_deps.count; i++) {
        Bld_Dep* d = c->exe->resolved_ext_deps.items[i];
        if (d->libs) for (const char** p = d->libs; *p; p++) h = bld_hash_combine(h, bld_hash_str(*p));
        if (d->lib_dirs) for (const char** p = d->lib_dirs; *p; p++) h = bld_hash_combine(h, bld_hash_str(*p));
        if (d->extra_ldflags) h = bld_hash_combine(h, bld_hash_str(d->extra_ldflags));
    }
    return h;
}

Bld_Target* bld__add_exe(Bld* b, const Bld_ExeOpts* opts) {
    if (!opts->name) bld_panic("exe name must not be NULL\n");
    Bld_Exe* exe = bld_arena_alloc(sizeof(Bld_Exe));
    memset(exe, 0, sizeof(*exe));
    exe->opts = *opts;
    exe->opts.name = bld_str_dup(opts->name);
    exe->opts.desc = opts->desc ? bld_str_dup(opts->desc) : "";
    exe->opts.output_name = opts->output_name ? bld_str_dup(opts->output_name) : NULL;
    exe->opts.sources = bld__dup_strarray(opts->sources);
    exe->opts.compile = bld_clone_compile_flags(opts->compile);
    if (opts->link.extra_flags) exe->opts.link.extra_flags = bld_str_dup(opts->link.extra_flags);

    bld__init_target(b, &exe->target, BLD_TGT_EXE, exe->opts.name, exe->opts.desc);

    Bld__ExeCtx* ctx = bld_arena_alloc(sizeof(Bld__ExeCtx));
    *ctx = (Bld__ExeCtx){.b = b, .exe = exe};
    exe->target.exit->action = bld__link_exe_action;
    exe->target.exit->action_ctx = ctx;
    exe->target.exit->hash_fn = bld__link_exe_recipe;
    exe->target.exit->hash_fn_ctx = ctx;

    if (exe->opts.sources) {
        for (const char** s = exe->opts.sources; *s; s++) {
            Bld_Step* obj = bld__add_obj(b, &exe->target, bld_path(*s), exe->opts.compile, &exe->opts.link, 0, exe->opts.lang);
            bld_da_push(&exe->obj_steps, obj);
            bld_da_push(&exe->target.exit->inputs, obj);
        }
    }
    bld_da_push(&b->build_all_target->entry->deps, exe->target.exit);
    return &exe->target;
}

/* ================================================================
 *  Link lib
 * ================================================================ */

typedef struct { Bld* b; Bld_Lib* lib; } Bld__LibCtx;

static const char* bld__lib_filename(const Bld_LibOpts* o) {
    const char* base = (o->output_name && o->output_name[0]) ? o->output_name : o->name;
    return o->shared ? bld_str_fmt("lib%s.so", base) : bld_str_fmt("lib%s.a", base);
}

static Bld_ActionResult bld__link_lib_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)depfile;
    Bld__LibCtx* c = ctx;
    Bld__Cmd cmd = {0};
    if (!c->lib->opts.shared) {
        if (!c->b->static_link_tool) bld_panic("no ar/llvm-ar found for static linking\n");
        bld__cmd_appendf(&cmd, "%s rcs %s", c->b->static_link_tool, output.s);
        bld__cmd_append_inputs(&cmd, c->b, c->lib->target.exit);
    } else {
        const char* soname = bld__lib_filename(&c->lib->opts);
        Bld_Compiler* linker = bld__link_compiler(c->b, &c->lib->obj_steps);
        bld__cmd_render_link(&cmd, c->b, linker, &c->lib->opts.link);
        bld__cmd_appendf(&cmd, " -shared -Wl,-soname,%s", soname);
        bld__cmd_append_inputs(&cmd, c->b, c->lib->target.exit);
        bld__cmd_appendf(&cmd, " -o %s", output.s);
        if (c->lib->opts.link.extra_flags && c->lib->opts.link.extra_flags[0])
            bld__cmd_appendf(&cmd, " %s", c->lib->opts.link.extra_flags);
    }
    if (c->b->settings.verbose) bld_log_action("link lib: %s\n", cmd.items);
    int rc = system(cmd.items);
    if (rc != 0) return BLD_ACTION_FAILED;
    return BLD_ACTION_OK;
}

static Bld_Hash bld__link_lib_recipe(void* ctx, Bld_Hash h) {
    Bld__LibCtx* c = ctx;
    h = bld_hash_combine(h, bld_hash_str(c->lib->opts.name));
    h = bld_hash_combine(h, (Bld_Hash){c->lib->opts.shared});
    h = bld__hash_link_flags(h, &c->lib->opts.link);
    if (c->lib->opts.shared) {
        Bld_Compiler* linker = bld__link_compiler(c->b, &c->lib->obj_steps);
        h = bld_hash_combine(h, linker->identity_hash);
    } else if (c->b->static_link_tool) {
        h = bld_hash_combine(h, bld_hash_str(c->b->static_link_tool));
    }
    return h;
}

Bld_Target* bld__add_lib(Bld* b, const Bld_LibOpts* opts) {
    if (!opts->name) bld_panic("lib name must not be NULL\n");
    Bld_Lib* lib = bld_arena_alloc(sizeof(Bld_Lib));
    memset(lib, 0, sizeof(*lib));
    lib->opts = *opts;
    lib->opts.name = bld_str_dup(opts->name);
    lib->opts.desc = opts->desc ? bld_str_dup(opts->desc) : "";
    lib->opts.output_name = opts->output_name ? bld_str_dup(opts->output_name) : NULL;
    lib->opts.sources = bld__dup_strarray(opts->sources);
    lib->opts.compile = bld_clone_compile_flags(opts->compile);
    if (opts->link.extra_flags) lib->opts.link.extra_flags = bld_str_dup(opts->link.extra_flags);

    bld__init_target(b, &lib->target, BLD_TGT_LIB, lib->opts.name, lib->opts.desc);

    Bld__LibCtx* ctx = bld_arena_alloc(sizeof(Bld__LibCtx));
    *ctx = (Bld__LibCtx){.b = b, .lib = lib};
    lib->target.exit->action = bld__link_lib_action;
    lib->target.exit->action_ctx = ctx;
    lib->target.exit->hash_fn = bld__link_lib_recipe;
    lib->target.exit->hash_fn_ctx = ctx;

    if (lib->opts.sources) {
        for (const char** s = lib->opts.sources; *s; s++) {
            Bld_Step* obj = bld__add_obj(b, &lib->target, bld_path(*s), lib->opts.compile, &lib->opts.link, 1, lib->opts.lang);
            bld_da_push(&lib->obj_steps, obj);
            bld_da_push(&lib->target.exit->inputs, obj);
        }
    }
    bld_da_push(&b->build_all_target->entry->deps, lib->target.exit);
    return &lib->target;
}

/* ---- Shared lib publish (copy .so to out/lib/ for exe linking) ---- */

typedef struct { Bld* b; Bld_Lib* lib; } Bld__PublishCtx;

static Bld_ActionResult bld__publish_lib_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__PublishCtx* c = ctx;
    Bld_Path src = bld__target_artifact(c->b, &c->lib->target);
    Bld_Path dst = bld_path_join(bld_path_join(c->b->out, bld_path("lib")),
                                 bld_path(bld__lib_filename(&c->lib->opts)));
    bld_fs_mkdir_p(bld_path_parent(dst));
    bld_fs_copy_file(src, dst);
    return BLD_ACTION_OK;
}

static Bld_Step* bld__ensure_publish_step(Bld* b, Bld_Lib* lib) {
    if (lib->publish_step) return lib->publish_step;
    const char* name = bld_str_fmt("publish:%s", lib->opts.name);
    Bld_Step* s = bld__alloc_step(b, name, 1);
    s->phony = 1;
    bld_da_push(&s->deps, lib->target.exit);
    Bld__PublishCtx* ctx = bld_arena_alloc(sizeof(Bld__PublishCtx));
    *ctx = (Bld__PublishCtx){.b = b, .lib = lib};
    s->action = bld__publish_lib_action;
    s->action_ctx = ctx;
    lib->publish_step = s;
    return s;
}

/* ================================================================
 *  Custom step
 * ================================================================ */

typedef struct { Bld* b; const char** watch; } Bld__StepHashCtx;

static Bld_Hash bld__step_watch_hash(void* ctx, Bld_Hash h) {
    Bld__StepHashCtx* c = ctx;
    for (const char** w = c->watch; *w; w++) {
        Bld_Path p = bld_path_join(c->b->root, bld_path(*w));
        if (bld_fs_is_dir(p)) h = bld_hash_combine(h, bld_hash_dir(p));
        else                   h = bld_hash_combine(h, bld_hash_file(p));
    }
    return h;
}

Bld_Target* bld__add_step(Bld* b, const Bld_StepOpts* opts) {
    if (!opts->name) bld_panic("step name must not be NULL\n");
    Bld_Target* t = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, t, BLD_TGT_CUSTOM, opts->name, opts->desc);
    if (opts->action) {
        t->exit->action = opts->action;
        t->exit->action_ctx = opts->action_ctx;
    }
    t->exit->has_depfile = opts->has_depfile;
    t->exit->content_hash = 1;
    if (opts->watch) {
        Bld__StepHashCtx* ctx = bld_arena_alloc(sizeof(Bld__StepHashCtx));
        *ctx = (Bld__StepHashCtx){.b = b, .watch = bld__dup_strarray(opts->watch)};
        t->exit->hash_fn = bld__step_watch_hash;
        t->exit->hash_fn_ctx = ctx;
    }
    return t;
}

/* ================================================================
 *  Run
 * ================================================================ */

typedef struct { Bld* b; Bld_Target* exe_tgt; Bld_RunOpts opts; } Bld__RunCtx;

static Bld_ActionResult bld__run_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__RunCtx* c = ctx;
    Bld__Cmd cmd = {0};
    if (c->opts.working_dir && c->opts.working_dir[0])
        bld__cmd_appendf(&cmd, "cd \"%s\" && ", c->opts.working_dir);
    bld__cmd_appendf(&cmd, "\"%s\"", bld__target_artifact(c->b, c->exe_tgt).s);
    if (c->opts.args) for (const char** a = c->opts.args; *a; a++) bld__cmd_appendf(&cmd, " \"%s\"", *a);
    if (c->b->settings.passthrough) for (const char** a = c->b->settings.passthrough; *a; a++) bld__cmd_appendf(&cmd, " \"%s\"", *a);
    int rc = system(cmd.items);
    if (rc != 0) return BLD_ACTION_FAILED;
    return BLD_ACTION_OK;
}

Bld_Target* bld__add_run(Bld* b, Bld_Target* target, const Bld_RunOpts* opts) {
    Bld_Target* run = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, run, BLD_TGT_CUSTOM, opts->name ? opts->name : "run", opts->desc);
    bld_da_push(&run->entry->deps, target->exit);
    run->exit->phony = 1; /* always execute, never cache */
    Bld__RunCtx* ctx = bld_arena_alloc(sizeof(Bld__RunCtx));
    *ctx = (Bld__RunCtx){.b = b, .exe_tgt = target, .opts = *opts};
    run->exit->action = bld__run_action;
    run->exit->action_ctx = ctx;
    return run;
}

/* ================================================================
 *  Install
 * ================================================================ */

typedef struct { Bld* b; Bld_Target* src; Bld_Path dst; } Bld__InstallCtx;

static Bld_ActionResult bld__install_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__InstallCtx* c = ctx;
    Bld_Path src = bld__target_artifact(c->b, c->src);
    bld_fs_mkdir_p(bld_path_parent(c->dst));
    if (bld_fs_is_dir(src)) bld_fs_copy_r(src, c->dst);
    else bld_fs_copy_file(src, c->dst);
    return BLD_ACTION_OK;
}

Bld_Target* bld_install(Bld* b, Bld_Target* target, Bld_Path dst) {
    Bld_Path full = bld_path_join(b->out, dst);
    Bld_Target* inst = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, inst, BLD_TGT_CUSTOM, bld_str_fmt("install-%s", target->name),
                     bld_str_fmt("Install %s", target->name));
    bld_da_push(&inst->entry->deps, target->exit);
    inst->exit->phony = 1; /* always copy to destination */
    Bld__InstallCtx* ctx = bld_arena_alloc(sizeof(Bld__InstallCtx));
    *ctx = (Bld__InstallCtx){.b = b, .src = target, .dst = full};
    inst->exit->action = bld__install_action;
    inst->exit->action_ctx = ctx;
    bld_da_push(&b->install_target->entry->deps, inst->exit);
    return inst;
}

Bld_Target* bld_install_exe(Bld* b, Bld_Target* t) {
    Bld_Exe* exe = bld__as_exe(t);
    const char* oname = (exe->opts.output_name && exe->opts.output_name[0]) ? exe->opts.output_name : exe->opts.name;
    return bld_install(b, t, bld_path_join(bld_path("bin"), bld_path(oname)));
}

Bld_Target* bld_install_lib(Bld* b, Bld_Target* t) {
    Bld_Lib* lib = bld__as_lib(t);
    return bld_install(b, t, bld_path_join(bld_path("lib"), bld_path(bld__lib_filename(&lib->opts))));
}

/* ================================================================
 *  Tests
 * ================================================================ */

Bld_Target* bld__add_test(Bld* b, Bld_Target* exe, const Bld_RunOpts* opts) {
    Bld_TestEntry entry = {0};
    entry.name = opts->name ? bld_str_dup(opts->name) : exe->name;
    entry.exe = exe;
    entry.working_dir = opts->working_dir ? bld_str_dup(opts->working_dir) : NULL;
    entry.args = bld__dup_strarray(opts->args);
    bld_da_push(&b->tests, entry);
    return exe;
}

/* ================================================================
 *  User options
 * ================================================================ */

static Bld_UserOption* bld__find_user_option(Bld* b, const char* name) {
    for (size_t i = 0; i < b->user_options.count; i++)
        if (strcmp(b->user_options.items[i].key, name) == 0) return &b->user_options.items[i];
    return NULL;
}

static void bld__register_option(Bld* b, const char* name, const char* desc, int type, const char* default_val) {
    /* check for duplicate registration */
    for (size_t i = 0; i < b->avail_options.count; i++)
        if (strcmp(b->avail_options.items[i].name, name) == 0)
            bld_panic("option '%s' declared twice\n", name);
    Bld_AvailableOption ao = { .name = name, .description = desc, .type = type, .default_val = default_val };
    bld_da_push(&b->avail_options, ao);
}

bool bld_option_bool(Bld* b, const char* name, const char* desc, bool default_val) {
    bld__register_option(b, name, desc, BLD_OPT_TYPE_BOOL, default_val ? "on" : "off");
    Bld_UserOption* opt = bld__find_user_option(b, name);
    if (!opt) return default_val;
    opt->used = 1;
    if (!opt->value) return true;  /* -Dfoo without = means true */
    if (strcmp(opt->value, "true") == 0 || strcmp(opt->value, "1") == 0 || strcmp(opt->value, "on") == 0) return true;
    if (strcmp(opt->value, "false") == 0 || strcmp(opt->value, "0") == 0 || strcmp(opt->value, "off") == 0) return false;
    bld_panic("invalid bool value for -D%s=%s (expected true/false/1/0/on/off)\n", name, opt->value);
    return default_val;
}

const char* bld_option_str(Bld* b, const char* name, const char* desc, const char* default_val) {
    bld__register_option(b, name, desc, BLD_OPT_TYPE_STRING, default_val);
    Bld_UserOption* opt = bld__find_user_option(b, name);
    if (!opt) return default_val;
    opt->used = 1;
    return opt->value ? opt->value : default_val;
}

/* ================================================================
 *  Child build context
 * ================================================================ */

Bld* bld_new(Bld* parent) {
    Bld* b = bld_arena_alloc(sizeof(Bld));
    memset(b, 0, sizeof(*b));
    b->root = parent->root;
    b->cache = parent->cache;
    b->out = parent->out;
    memcpy(b->compilers, parent->compilers, sizeof(parent->compilers));
    b->static_link_tool = parent->static_link_tool;
    b->global_warnings = parent->global_warnings;
    b->global_optimize = parent->global_optimize;
    b->global_link = parent->global_link;
    b->settings = parent->settings;
    b->settings.silent = 1; /* child builds are silent by default */
    b->argc = parent->argc;
    b->argv = parent->argv;
    return b;
}

/* ================================================================
 *  Compiler setters
 * ================================================================ */

static void bld__set_driver(Bld_Compiler* comp, const char* driver) {
    comp->driver = bld_str_dup(driver);
    comp->identity_hash = bld__make_identity_hash(driver);
    comp->available = bld__has_in_path(driver);
}

void bld__set_compiler_c(Bld* b, const Bld_CCompilerOpts* opts) {
    Bld_Compiler* comp = bld_compiler(b, BLD_LANG_C);
    if (opts->driver) bld__set_driver(comp, opts->driver);
    if (opts->standard) comp->c.standard = opts->standard;
}

void bld__set_compiler_cxx(Bld* b, const Bld_CxxCompilerOpts* opts) {
    Bld_Compiler* comp = bld_compiler(b, BLD_LANG_CXX);
    if (opts->driver) bld__set_driver(comp, opts->driver);
    if (opts->standard) comp->cxx.standard = opts->standard;
}

void bld__set_compiler_asm(Bld* b, const Bld_AsmCompilerOpts* opts) {
    Bld_Compiler* comp = bld_compiler(b, BLD_LANG_ASM);
    if (opts->driver) bld__set_driver(comp, opts->driver);
}

int bld_target_ok(Bld_Target* t) {
    return t->exit->state == BLD_STEP_OK;
}

Bld_Path bld_target_artifact(Bld* b, Bld_Target* t) {
    return bld__step_artifact(b, t->exit);
}

/* ================================================================
 *  Feature checks
 * ================================================================ */

typedef struct {
    const char*  define_name;
    const char*  snippet;
    int          is_sizeof;
    bool*        bool_result;
    int*         int_result;
    Bld_Target*  target;
} Bld__Check;

struct Bld_Checks {
    Bld*        parent;
    Bld*        child;
    Bld__Check* items;
    size_t      count;
    size_t      cap;
};

typedef struct { Bld* b; const char* snippet; int is_sizeof; } Bld__CheckCtx;

static Bld_Hash bld__check_recipe_hash(void* ctx, Bld_Hash h) {
    Bld__CheckCtx* c = ctx;
    h = bld_hash_combine(h, bld_compiler(c->b, BLD_LANG_C)->identity_hash);
    h = bld_hash_combine(h, bld_hash_str(c->snippet));
    return h;
}

static Bld_ActionResult bld__check_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    Bld__CheckCtx* c = ctx;
    const char* cc = bld_compiler(c->b, BLD_LANG_C)->driver;

    /* write snippet to stable path (not tmp — survives between runs for depfile caching) */
    Bld_Hash snip_hash = bld_hash_str(c->snippet);
    Bld_Path src = bld_path_join(
        bld_path_join(c->b->cache, bld_path("checks")),
        bld_path_fmt("%" PRIu64 ".c", snip_hash.value));
    bld_fs_mkdir_p(bld_path_parent(src));
    const char* src_path = src.s;
    bld_fs_write_file(bld_path(src_path), c->snippet, strlen(c->snippet));

    if (!c->is_sizeof) {
        /* compile only — success = "1", failure = "0" */
        const char* cmd = bld_str_fmt("%s -xc %s -c -o /dev/null -MMD -MF %s 2>/dev/null",
                                       cc, src_path,
                                       depfile.s && depfile.s[0] ? depfile.s : "/dev/null");
        int rc = system(cmd);
        bld_fs_write_file(output, rc == 0 ? "1" : "0", 1);
    } else {
        /* compile + run — output is the printed value, or "0" on failure */
        Bld_Path bin = bld__cache_tmp(c->b);
        const char* cmd = bld_str_fmt("%s -xc %s -o %s 2>/dev/null && %s",
                                       cc, src_path, bin.s, bin.s);
        FILE* f = popen(cmd, "r");
        char buf[64] = {0};
        if (f) {
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            buf[n] = '\0';
            pclose(f);
        }
        if (buf[0] == '\0') strcpy(buf, "0");
        bld_fs_write_file(output, buf, strlen(buf));
    }
    return BLD_ACTION_OK;
}

static void bld__checks_add(Bld_Checks* c, const char* define_name, const char* snippet,
                              int is_sizeof, bool* bool_result, int* int_result) {
    if (c->count >= c->cap) {
        size_t nc = c->cap ? c->cap * 2 : 16;
        c->items = bld_arena_realloc(c->items, c->cap * sizeof(Bld__Check), nc * sizeof(Bld__Check));
        c->cap = nc;
    }
    Bld__Check* ch = &c->items[c->count++];
    ch->define_name = define_name;
    ch->snippet = snippet;
    ch->is_sizeof = is_sizeof;
    ch->bool_result = bool_result;
    ch->int_result = int_result;

    Bld__CheckCtx* ctx = bld_arena_alloc(sizeof(Bld__CheckCtx));
    *ctx = (Bld__CheckCtx){.b = c->child, .snippet = snippet, .is_sizeof = is_sizeof};

    ch->target = bld__add_step(c->child, &(Bld_StepOpts){
        .name = bld_str_fmt("check:%s", define_name),
        .action = bld__check_action,
        .action_ctx = ctx,
        .has_depfile = !is_sizeof,
    });
    ch->target->exit->hash_fn = bld__check_recipe_hash;
    ch->target->exit->hash_fn_ctx = ctx;
    ch->target->exit->content_hash = 0; /* checks should cache normally, not re-run every time */
}

Bld_Checks* bld_checks_new(Bld* parent) {
    Bld_Checks* c = bld_arena_alloc(sizeof(Bld_Checks));
    memset(c, 0, sizeof(*c));
    c->parent = parent;
    c->child = bld_new(parent);
    return c;
}

bool* bld_checks_header(Bld_Checks* c, const char* define_name, const char* header) {
    bool* result = bld_arena_alloc(sizeof(bool));
    *result = false;
    const char* snippet = bld_str_fmt("#include <%s>\nint main(){return 0;}\n", header);
    bld__checks_add(c, define_name, snippet, 0, result, NULL);
    return result;
}

bool* bld_checks_func(Bld_Checks* c, const char* define_name, const char* func, const char* header) {
    bool* result = bld_arena_alloc(sizeof(bool));
    *result = false;
    const char* snippet = bld_str_fmt("#include <%s>\nint main(){(void)%s;return 0;}\n", header, func);
    bld__checks_add(c, define_name, snippet, 0, result, NULL);
    return result;
}

int* bld_checks_sizeof(Bld_Checks* c, const char* define_name, const char* type) {
    int* result = bld_arena_alloc(sizeof(int));
    *result = 0;
    const char* snippet = bld_str_fmt("#include <stdio.h>\nint main(){printf(\"%%zu\",sizeof(%s));return 0;}\n", type);
    bld__checks_add(c, define_name, snippet, 1, NULL, result);
    return result;
}

bool* bld_checks_compile(Bld_Checks* c, const char* define_name, const char* source) {
    bool* result = bld_arena_alloc(sizeof(bool));
    *result = false;
    bld__checks_add(c, define_name, source, 0, result, NULL);
    return result;
}

void bld_checks_run(Bld_Checks* c) {
    bld_execute(c->child);

    /* read results from artifacts */
    size_t changed = 0;
    for (size_t i = 0; i < c->count; i++) {
        Bld__Check* ch = &c->items[i];
        if (!bld_target_ok(ch->target)) continue;

        Bld_Path art = bld_target_artifact(c->child, ch->target);
        if (!bld_fs_exists(art)) continue;

        size_t len;
        const char* val = bld_fs_read_file(art, &len);
        if (ch->bool_result) {
            bool prev = *ch->bool_result;
            *ch->bool_result = (len > 0 && val[0] == '1');
            if (*ch->bool_result != prev) changed++;
        }
        if (ch->int_result) {
            int prev = *ch->int_result;
            *ch->int_result = atoi(val);
            if (*ch->int_result != prev) changed++;
        }
    }

    /* summary — only if checks actually ran (not all cached) */
    if (c->child->steps_executed > 0) {
        size_t yes = 0, no = 0;
        for (size_t i = 0; i < c->count; i++) {
            if (c->items[i].bool_result && *c->items[i].bool_result) yes++;
            else if (c->items[i].int_result && *c->items[i].int_result > 0) yes++;
            else if (bld_target_ok(c->items[i].target)) no++;
        }
        bld_log_info("-- %zu checks (%zu yes, %zu no)\n", c->count, yes, no);
    }
}

void bld_checks_write(Bld_Checks* c, const char* path) {
    Bld__Cmd out = {0};
    bld__cmd_appendf(&out, "/* generated by bld feature checks */\n");
    for (size_t i = 0; i < c->count; i++) {
        Bld__Check* ch = &c->items[i];
        if (ch->bool_result) {
            if (*ch->bool_result)
                bld__cmd_appendf(&out, "#define %s 1\n", ch->define_name);
            else
                bld__cmd_appendf(&out, "/* %s not found */\n", ch->define_name);
        } else if (ch->int_result) {
            if (*ch->int_result > 0)
                bld__cmd_appendf(&out, "#define %s %d\n", ch->define_name, *ch->int_result);
            else
                bld__cmd_appendf(&out, "/* %s unknown */\n", ch->define_name);
        }
    }
    Bld_Path full = bld_path_join(c->parent->root, bld_path(path));
    bld_fs_write_file(full, out.items, out.count);
}

