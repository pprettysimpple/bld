/* bld/bld_build.c — target/step construction, build graph logic */
#pragma once

#include "bld_cache.h"

/* ================================================================
 *  Helpers
 * ================================================================ */


/* ================================================================
 *  Clone
 * ================================================================ */

static Bld_Strs bld__clone_strs(Bld_Strs s) {
    if (s.count == 0) return (Bld_Strs){0};
    const char** copy = bld_arena_alloc(s.count * sizeof(const char*));
    for (size_t i = 0; i < s.count; i++) copy[i] = bld_str_dup(s.items[i]);
    return (Bld_Strs){copy, s.count, 0};
}

static Bld_Paths bld__clone_paths(Bld_Paths s) {
    if (s.count == 0) return (Bld_Paths){0};
    const char** copy = bld_arena_alloc(s.count * sizeof(const char*));
    for (size_t i = 0; i < s.count; i++) copy[i] = bld_str_dup(s.items[i]);
    return (Bld_Paths){copy, s.count, 0};
}

Bld_CompileFlags bld_clone_compile_flags(Bld_CompileFlags f) {
    Bld_CompileFlags c = f;
    if (f.extra_flags) c.extra_flags = bld_str_dup(f.extra_flags);
    c.defines = bld__clone_strs(f.defines);
    c.include_dirs = bld__clone_paths(f.include_dirs);
    c.system_include_dirs = bld__clone_paths(f.system_include_dirs);
    return c;
}

static Bld_LinkFlags bld_clone_link_flags(Bld_LinkFlags f) {
    Bld_LinkFlags c = f;
    if (f.extra_flags) c.extra_flags = bld_str_dup(f.extra_flags);
    c.libs = bld__clone_strs(f.libs);
    c.lib_dirs = bld__clone_paths(f.lib_dirs);
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
            f.debug_info = BLD_ON;
            break;
        case BLD_MODE_RELEASE: {
            f.optimize = BLD_OPT_O2;
            f.debug_info = BLD_OFF;
            f.defines = BLD_STRS("NDEBUG");
            break;
        }
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
    ov.flags = bld_clone_compile_flags(*flags);
    bld_da_push(&t->file_overrides, ov);
}

/* bld__dep, bld_find_pkg — implemented in bld_dep.c */

void bld_use_dep(Bld_Target* t, Bld_Dep* dep) {
    if (dep && !dep->found)
        bld_log_info("-- warning: using unfound dependency '%s' on target '%s'\n",
                     dep->name ? dep->name : "(unnamed)", t->name);
    bld_da_push(&t->ext_deps, dep);
}

/* ================================================================
 *  Step primitives
 * ================================================================ */

static Bld_Step* bld__alloc_step(Bld* b, const char* name, bool silent) {
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

static const char* bld__target_prefix(Bld_TargetKind kind) {
    switch (kind) {
        case BLD_TGT_EXE: return "exe:";
        case BLD_TGT_LIB: return "lib:";
        default:           return "";
    }
}

static void bld__init_target(Bld* b, Bld_Target* t, Bld_TargetKind kind,
                              const char* name, const char* desc) {
    t->kind = kind;
    const char* raw = name ? name : "";
    t->name = bld_str_fmt("%s%s", bld__target_prefix(kind), raw);
    t->desc = desc ? bld_str_dup(desc) : "";
    for (size_t i = 0; i < b->all_targets.count; i++)
        if (strcmp(b->all_targets.items[i]->name, t->name) == 0)
            bld_panic("duplicate target name: '%s'\n", t->name);
    t->entry = bld__alloc_step(b, bld_str_fmt("%s:entry", t->name), true);
    t->exit  = bld__alloc_step(b, bld_str_dup(t->name), false);
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
    if (b->kind != BLD_TGT_EXE && b->kind != BLD_TGT_LIB)
        bld_panic("link_with: target '%s' is not an exe or lib (got custom step '%s')\n", a->name, b->name);
    bld_da_push(&a->link_deps, b);
}

/* collect transitive link deps (depth-first, deduplicated) */
static void bld__collect_link_deps(Bld_Target* t, Bld_Target*** items, size_t* count, size_t* cap) {
    for (size_t i = 0; i < t->link_deps.count; i++) {
        Bld_Target* dep = t->link_deps.items[i];
        bool found = false;
        for (size_t j = 0; j < *count; j++)
            if ((*items)[j] == dep) { found = true; break; }
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

            /* propagate compile_propagate from lib deps as synthetic ext_dep */
            if (dep->kind == BLD_TGT_LIB) {
                Bld_CompileFlags* pub = &((Bld_Lib*)dep)->opts.compile_propagate;
                if (pub->include_dirs.count || pub->system_include_dirs.count ||
                    pub->defines.count || pub->extra_flags) {
                    /* build extra_cflags from defines */
                    const char* dcflags = NULL;
                    if (pub->defines.count) {
                        Bld_Cmd dc = {0};
                        for (size_t di = 0; di < pub->defines.count; di++)
                            bld_cmd_appendf(&dc, " -D%s", pub->defines.items[di]);
                        if (pub->extra_flags)
                            bld_cmd_appendf(&dc, " %s", pub->extra_flags);
                        dcflags = dc.items;
                    } else {
                        dcflags = pub->extra_flags;
                    }
                    Bld_Dep* syn = bld_arena_alloc(sizeof(Bld_Dep));
                    *syn = (Bld_Dep){
                        .name = bld_str_fmt("%s:pub", dep->name),
                        .found = true,
                        .include_dirs = pub->include_dirs,
                        .system_include_dirs = pub->system_include_dirs,
                        .extra_cflags = dcflags,
                    };
                    bld_da_push(&t->ext_deps, syn);
                }

                /* propagate link_propagate from lib deps as synthetic ext_dep (link-side) */
                Bld_LinkFlags* lpub = &((Bld_Lib*)dep)->opts.link_propagate;
                if (lpub->libs.count || lpub->lib_dirs.count || lpub->extra_flags) {
                    Bld_Dep* lsyn = bld_arena_alloc(sizeof(Bld_Dep));
                    *lsyn = (Bld_Dep){
                        .name = bld_str_fmt("%s:link_propagate", dep->name),
                        .found = true,
                        .libs = lpub->libs,
                        .lib_dirs = lpub->lib_dirs,
                        .extra_ldflags = lpub->extra_flags,
                    };
                    bld_da_push(&t->ext_deps, lsyn);
                    if (t->kind == BLD_TGT_EXE)
                        bld__push_ext_dep_dedup(bld__as_exe(t), lsyn);
                }
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

/* Bld_Cmd, bld_cmd_appendf, bld_cmd_append_sq — declared in bld_core.h, implemented in bld_core_impl.c */

/* ================================================================
 *  Flag rendering
 * ================================================================ */

/* bld__resolve_optimize, bld__resolve_c_standard, bld__resolve_cxx_standard
 * are defined in bld_core_impl.c (shared by toolchain renderers) */

static bool bld__toggle_val(Bld_Toggle t, Bld_Toggle global) {
    if (t == BLD_ON) return true;
    if (t == BLD_OFF) return false;
    return global == BLD_ON;
}

/* ================================================================
 *  Flag hashing
 * ================================================================ */

static Bld_Hash bld__hash_compile_flags(Bld_Hash h, const Bld_CompileFlags* f) {
    h = bld_hash_combine(h, (Bld_Hash){f->optimize});
    h = bld_hash_combine(h, (Bld_Hash){f->warnings});
    h = bld_hash_combine(h, (Bld_Hash){f->debug_info});
    if (f->extra_flags) h = bld_hash_combine(h, bld_hash_str(f->extra_flags));
    for (size_t i = 0; i < f->defines.count; i++) h = bld_hash_combine(h, bld_hash_str(f->defines.items[i]));
    for (size_t i = 0; i < f->include_dirs.count; i++) h = bld_hash_combine(h, bld_hash_str(f->include_dirs.items[i]));
    for (size_t i = 0; i < f->system_include_dirs.count; i++) h = bld_hash_combine(h, bld_hash_str(f->system_include_dirs.items[i]));
    return h;
}

/* build flags that affect compilation (asan, lto from Bld_BuildFlags) */
static Bld_Hash bld__hash_build_flags(Bld_Hash h, const Bld_BuildFlags* f) {
    h = bld_hash_combine(h, (Bld_Hash){f->asan});
    h = bld_hash_combine(h, (Bld_Hash){f->lto});
    return h;
}

/* all link flags (for link step hash) */
static Bld_Hash bld__hash_link_flags(Bld_Hash h, const Bld_LinkFlags* f) {
    if (f->extra_flags) h = bld_hash_combine(h, bld_hash_str(f->extra_flags));
    for (size_t i = 0; i < f->libs.count; i++) h = bld_hash_combine(h, bld_hash_str(f->libs.items[i]));
    for (size_t i = 0; i < f->lib_dirs.count; i++) h = bld_hash_combine(h, bld_hash_str(f->lib_dirs.items[i]));
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
    Bld_CompileFlags compile; bool pic;
    Bld_Lang lang;
} Bld__ObjCtx;

/* merge override on top of base: non-zero fields replace */
static Bld_CompileFlags bld__merge_compile_flags(Bld_CompileFlags base, const Bld_CompileFlags* ov) {
    if (ov->optimize)                   base.optimize = ov->optimize;
    if (ov->warnings)                   base.warnings = ov->warnings;
    if (ov->debug_info)                 base.debug_info = ov->debug_info;
    if (ov->extra_flags)                base.extra_flags = ov->extra_flags;
    if (ov->defines.count)                base.defines = ov->defines;
    if (ov->include_dirs.count)           base.include_dirs = ov->include_dirs;
    if (ov->system_include_dirs.count)    base.system_include_dirs = ov->system_include_dirs;
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

static Bld_CompileCmd bld__build_compile_cmd(Bld__ObjCtx* c, Bld_Path output, Bld_Path depfile);

/* render full compile command (used by compile_commands.json) */
static void bld__render_obj_cmd(Bld_Cmd* cmd, Bld__ObjCtx* c) {
    Bld_Path dummy_out = bld_path("placeholder.o");
    Bld_Path dummy_dep = bld_path("");
    Bld_CompileCmd cc = bld__build_compile_cmd(c, dummy_out, dummy_dep);
    c->b->toolchain->render_compile(cmd, cc);
}

static Bld_ActionResult bld__obj_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    Bld__ObjCtx* c = ctx;
    Bld_Toolchain* tc = c->b->toolchain;
    Bld_CompileCmd cc = bld__build_compile_cmd(c, output, depfile);
    Bld_Cmd cmd = {0};
    tc->render_compile(&cmd, cc);
    if (c->b->settings.verbose) bld_log_action("compile: %s\n", cmd.items);
    int rc = system(cmd.items);
    return rc == 0 ? BLD_ACTION_OK : BLD_ACTION_FAILED;
}

static Bld_Hash bld__obj_recipe_hash(void* ctx, Bld_Hash h) {
    Bld__ObjCtx* c = ctx;
    Bld_CompileFlags flags = bld__resolve_obj_flags(c);
    Bld_Path src = bld__obj_source(c);
    Bld_Toolchain* tc = c->b->toolchain;
    Bld_Compiler* comp = bld_compiler(c->b, c->lang);
    h = bld_hash_combine(h, comp->identity_hash);
    h = bld_hash_combine(h, bld_hash_str(tc->name));
    h = bld_hash_combine(h, bld_hash_str(tc->obj_ext));
    /* hash compiler standard */
    if (comp->lang == BLD_LANG_C) h = bld_hash_combine(h, (Bld_Hash){comp->c.standard});
    else if (comp->lang == BLD_LANG_CXX) h = bld_hash_combine(h, (Bld_Hash){comp->cxx.standard});
    h = bld_hash_combine(h, bld_hash_str(src.s));
    h = bld_hash_combine(h, bld_hash_file(src));
    h = bld__hash_compile_flags(h, &flags);
    h = bld__hash_build_flags(h, &c->b->build_flags);
    h = bld_hash_combine(h, (Bld_Hash){c->pic});
    h = bld_hash_combine(h, (Bld_Hash){c->parent->include_dirs.count});
    for (size_t i = 0; i < c->parent->include_dirs.count; i++) {
        Bld_LazyPath lp = c->parent->include_dirs.items[i];
        if (lp.path.s[0]) h = bld_hash_combine(h, bld_hash_str(lp.path.s));
    }
    /* hash ext_deps compile-side */
    for (size_t i = 0; i < c->parent->ext_deps.count; i++) {
        Bld_Dep* d = c->parent->ext_deps.items[i];
        for (size_t j = 0; j < d->include_dirs.count; j++) h = bld_hash_combine(h, bld_hash_str(d->include_dirs.items[j]));
        for (size_t j = 0; j < d->system_include_dirs.count; j++) h = bld_hash_combine(h, bld_hash_str(d->system_include_dirs.items[j]));
        if (d->extra_cflags) h = bld_hash_combine(h, bld_hash_str(d->extra_cflags));
    }
    return h;
}

static Bld_Step* bld__add_obj(Bld* b, Bld_Target* parent, Bld_Path source,
                               Bld_CompileFlags compile, bool pic,
                               Bld_Lang target_lang) {
    Bld_Lang lang = (target_lang != BLD_LANG_AUTO) ? target_lang : bld__infer_lang(source.s);
    Bld_Path abs_src = bld_path_join(b->root, source);
    const char* name = bld_str_fmt("%s:%s", parent->name, bld_path_replace_ext(source, bld_str_fmt(".%s", b->toolchain->obj_ext)).s);
    Bld_Step* s = bld__alloc_step(b, name, true);
    s->has_depfile = true;
    bld_da_push(&s->deps, parent->entry);

    Bld__ObjCtx* ctx = bld_arena_alloc(sizeof(Bld__ObjCtx));
    *ctx = (Bld__ObjCtx){.b = b, .parent = parent, .source = abs_src, .orig_source = source,
                          .compile = compile, .pic = pic, .lang = lang};
    s->action = bld__obj_action;
    s->action_ctx = ctx;
    s->hash_fn = bld__obj_recipe_hash;
    s->hash_fn_ctx = ctx;
    return s;
}

static Bld_Step* bld__add_lazy_obj(Bld* b, Bld_Target* parent, Bld_LazyPath lazy_source,
                                    Bld_CompileFlags compile, bool pic,
                                    Bld_Lang target_lang) {
    Bld_Lang lang = (target_lang != BLD_LANG_AUTO) ? target_lang : bld__infer_lang(lazy_source.path.s);
    const char* src_name = lazy_source.source ? lazy_source.source->name : "gen";
    const char* name = bld_str_fmt("%s:lazy_%s.%s", parent->name, src_name, b->toolchain->obj_ext);
    Bld_Step* s = bld__alloc_step(b, name, true);
    s->has_depfile = true;
    bld_da_push(&s->deps, parent->entry);
    if (lazy_source.source)
        bld_da_push(&s->inputs, lazy_source.source->exit);

    Bld__ObjCtx* ctx = bld_arena_alloc(sizeof(Bld__ObjCtx));
    *ctx = (Bld__ObjCtx){.b = b, .parent = parent, .source = bld_path(""),
                          .orig_source = bld_path(""), .lazy_source = lazy_source,
                          .compile = compile, .pic = pic, .lang = lang};
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
        bool pic = (t->kind == BLD_TGT_LIB);

        Bld_Lang target_lang = (t->kind == BLD_TGT_EXE)
            ? ((Bld_Exe*)t)->opts.lang : ((Bld_Lib*)t)->opts.lang;

        for (size_t j = 0; j < t->lazy_sources.count; j++) {
            Bld_Step* obj = bld__add_lazy_obj(b, t, t->lazy_sources.items[j], compile, pic, target_lang);
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
 *  Resolve helpers (build Bld_CompileCmd / Bld_LinkCmd structs)
 * ================================================================ */

/*
 * Build a Bld_CompileCmd from an ObjCtx, resolving all flags, toggles, and
 * flattening include dirs / ext_dep cflags into the struct fields.
 * The resulting struct is passed to tc->render_compile.
 */
static Bld_CompileCmd bld__build_compile_cmd(Bld__ObjCtx* c, Bld_Path output, Bld_Path depfile) {
    Bld_CompileFlags flags = bld__resolve_obj_flags(c);
    Bld_Compiler* comp = bld_compiler(c->b, c->lang);

    /* resolve optimize: per-file > global */
    Bld_Optimize opt = flags.optimize ? flags.optimize : c->b->global_optimize;

    /* resolve warnings: ON=1, OFF=0, UNSET=global */
    bool warnings = (flags.warnings == BLD_ON) ? true :
                    (flags.warnings == BLD_OFF) ? false : c->b->global_warnings;

    /* resolve debug_info from compile flags */
    bool debug_info = bld__toggle_val(flags.debug_info, BLD_UNSET);

    /* resolve asan, lto from build_flags */
    bool asan = bld__toggle_val(c->b->build_flags.asan, BLD_UNSET);
    bool lto  = bld__toggle_val(c->b->build_flags.lto,  BLD_UNSET);

    /* Build extra_cflags: target include_dirs (quoted) + ext_dep includes/sys_includes/extra_cflags. */
    Bld_Cmd ecf = {0};
    for (size_t i = 0; i < c->parent->include_dirs.count; i++) {
        Bld_Path dir = bld__resolve_lazy(c->b, c->parent->include_dirs.items[i]);
        bld_cmd_appendf(&ecf, " -I\"%s\"", dir.s);
    }
    for (size_t i = 0; i < c->parent->ext_deps.count; i++) {
        Bld_Dep* d = c->parent->ext_deps.items[i];
        for (size_t j = 0; j < d->include_dirs.count; j++)
            bld_cmd_appendf(&ecf, " -I%s", d->include_dirs.items[j]);
        for (size_t j = 0; j < d->system_include_dirs.count; j++)
            bld_cmd_appendf(&ecf, " -isystem %s", d->system_include_dirs.items[j]);
        if (d->extra_cflags && d->extra_cflags[0])
            bld_cmd_appendf(&ecf, " %s", d->extra_cflags);
    }
    /* ecf.items starts with a leading space if non-empty; the render function
       checks [0] before appending, so we skip the leading space */
    const char* extra_cflags_str = (ecf.count > 0) ? ecf.items + 1 : NULL;

    Bld_CompileCmd result = {0};
    result.driver      = comp->driver;
    result.lang        = comp->lang;
    if (comp->lang == BLD_LANG_C)   result.c_std = comp->c.standard;
    if (comp->lang == BLD_LANG_CXX) result.cxx_std = comp->cxx.standard;
    result.optimize    = opt;
    result.warnings    = warnings;
    result.pic         = c->pic;
    result.debug_info  = debug_info;
    result.asan        = asan;
    result.lto         = lto;
    result.extra_flags = flags.extra_flags;
    result.defines          = flags.defines;
    result.include_dirs     = flags.include_dirs;
    result.sys_include_dirs = flags.system_include_dirs;
    result.extra_cflags     = extra_cflags_str;
    result.source      = bld__obj_source(c).s;
    result.output      = output.s;
    result.depfile     = depfile.s;
    return result;
}

/*
 * Build a Bld_LinkCmd for linking an executable, resolving all toggles and
 * flattening obj paths, lib dirs/names, rpaths, and extra ldflags.
 * The resulting struct is passed to tc->render_link.
 */
static Bld_LinkCmd bld__build_link_cmd_exe(Bld* b, Bld_Exe* exe, Bld_Path output) {
    /* resolve link toggles from build_flags and compile flags */
    bool debug_info = bld__toggle_val(exe->opts.compile.debug_info, BLD_UNSET);
    bool asan       = bld__toggle_val(b->build_flags.asan, BLD_UNSET);
    bool lto        = bld__toggle_val(b->build_flags.lto,  BLD_UNSET);

    /* build obj_paths from exit->inputs (only hash_valid steps) */
    Bld_Paths obj_paths = {0};
    for (size_t i = 0; i < exe->target.exit->inputs.count; i++) {
        Bld_Step* inp = exe->target.exit->inputs.items[i];
        if (inp->hash_valid)
            bld_paths_push(&obj_paths, bld__step_artifact(b, inp).s);
    }

    /* build lib_dirs and lib_names from shared_libs */
    Bld_Paths lib_dirs  = {0};
    Bld_Strs  lib_names = {0};

    if (exe->shared_libs.count > 0) {
        Bld_Path libdir = bld_path_join(b->out, bld_path("lib"));
        bld_paths_push(&lib_dirs, libdir.s);
        for (size_t i = 0; i < exe->shared_libs.count; i++)
            bld_strs_push(&lib_names, exe->shared_libs.items[i]->opts.name);
    }

    /* rpaths: $ORIGIN/../lib if shared_libs exist */
    Bld_Paths rpaths = {0};
    if (exe->shared_libs.count > 0)
        bld_paths_push(&rpaths, "$ORIGIN/../lib");

    /* merge extra_ldflags from opts.link.libs/lib_dirs + ext_deps + opts.link.extra_flags */
    Bld_Cmd ldf = {0};
    for (size_t i = 0; i < exe->opts.link.lib_dirs.count; i++)
        bld_cmd_appendf(&ldf, " -L%s", exe->opts.link.lib_dirs.items[i]);
    for (size_t i = 0; i < exe->opts.link.libs.count; i++)
        bld_cmd_appendf(&ldf, " -l%s", exe->opts.link.libs.items[i]);
    for (size_t i = 0; i < exe->resolved_ext_deps.count; i++) {
        Bld_Dep* d = exe->resolved_ext_deps.items[i];
        for (size_t j = 0; j < d->lib_dirs.count; j++)
            bld_cmd_appendf(&ldf, " -L%s", d->lib_dirs.items[j]);
        for (size_t j = 0; j < d->libs.count; j++)
            bld_cmd_appendf(&ldf, " -l%s", d->libs.items[j]);
        if (d->extra_ldflags && d->extra_ldflags[0])
            bld_cmd_appendf(&ldf, " %s", d->extra_ldflags);
    }
    if (exe->opts.link.extra_flags && exe->opts.link.extra_flags[0])
        bld_cmd_appendf(&ldf, " %s", exe->opts.link.extra_flags);
    const char* extra_ldflags_str = (ldf.count > 0) ? ldf.items + 1 : NULL;

    /* determine link driver */
    Bld_Compiler* linker = bld__link_compiler(b, &exe->obj_steps);

    Bld_LinkCmd result = {0};
    result.driver       = linker->driver;
    result.shared       = false;
    result.debug_info   = debug_info;
    result.asan         = asan;
    result.lto          = lto;
    result.obj_paths    = obj_paths;
    result.lib_dirs     = lib_dirs;
    result.lib_names    = lib_names;
    result.rpaths       = rpaths;
    result.extra_ldflags = extra_ldflags_str;
    result.output       = output.s;
    return result;
}

/* ================================================================
 *  Link exe
 * ================================================================ */

typedef struct { Bld* b; Bld_Exe* exe; } Bld__ExeCtx;

static Bld_ActionResult bld__link_exe_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)depfile;
    Bld__ExeCtx* c = ctx;
    Bld_Toolchain* tc = c->exe->toolchain;
    Bld_LinkCmd lc = bld__build_link_cmd_exe(c->b, c->exe, output);
    Bld_Cmd cmd = {0};
    tc->render_link(&cmd, lc);
    if (c->b->settings.verbose) bld_log_action("link exe: %s\n", cmd.items);
    int rc = system(cmd.items);
    return rc == 0 ? BLD_ACTION_OK : BLD_ACTION_FAILED;
}

static Bld_Hash bld__link_exe_recipe(void* ctx, Bld_Hash h) {
    Bld__ExeCtx* c = ctx;
    Bld_Toolchain* tc = c->exe->toolchain;
    h = bld_hash_combine(h, bld_hash_str(c->exe->opts.name));
    h = bld_hash_combine(h, bld_hash_str(tc->name));
    h = bld_hash_combine(h, bld_hash_str(tc->obj_ext));
    h = bld_hash_combine(h, bld_hash_str(tc->shared_lib_ext));
    Bld_Compiler* linker = bld__link_compiler(c->b, &c->exe->obj_steps);
    h = bld_hash_combine(h, linker->identity_hash);
    h = bld__hash_link_flags(h, &c->exe->opts.link);
    h = bld__hash_build_flags(h, &c->b->build_flags);
    h = bld__hash_compile_flags(h, &c->exe->opts.compile); /* debug_info affects link */
    for (size_t i = 0; i < c->exe->shared_libs.count; i++) {
        Bld_Lib* lib = (Bld_Lib*)c->exe->shared_libs.items[i];
        h = bld_hash_combine(h, bld_hash_str(lib->opts.name));
        h = bld_hash_combine(h, lib->target.exit->cache_key);
    }
    for (size_t i = 0; i < c->exe->resolved_ext_deps.count; i++) {
        Bld_Dep* d = c->exe->resolved_ext_deps.items[i];
        for (size_t j = 0; j < d->libs.count; j++) h = bld_hash_combine(h, bld_hash_str(d->libs.items[j]));
        for (size_t j = 0; j < d->lib_dirs.count; j++) h = bld_hash_combine(h, bld_hash_str(d->lib_dirs.items[j]));
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
    exe->opts.sources = bld__clone_paths(opts->sources);
    exe->opts.compile = bld_clone_compile_flags(opts->compile);
    exe->opts.link = bld_clone_link_flags(opts->link);
    exe->toolchain = opts->toolchain ? opts->toolchain : b->toolchain;

    bld__init_target(b, &exe->target, BLD_TGT_EXE, exe->opts.name, exe->opts.desc);

    Bld__ExeCtx* ctx = bld_arena_alloc(sizeof(Bld__ExeCtx));
    *ctx = (Bld__ExeCtx){.b = b, .exe = exe};
    exe->target.exit->action = bld__link_exe_action;
    exe->target.exit->action_ctx = ctx;
    exe->target.exit->hash_fn = bld__link_exe_recipe;
    exe->target.exit->hash_fn_ctx = ctx;

    for (size_t i = 0; i < exe->opts.sources.count; i++) {
        Bld_Step* obj = bld__add_obj(b, &exe->target, bld_path(exe->opts.sources.items[i]), exe->opts.compile, false, exe->opts.lang);
        bld_da_push(&exe->obj_steps, obj);
        bld_da_push(&exe->target.exit->inputs, obj);
    }
    bld_da_push(&b->target_default->entry->deps, exe->target.exit);
    return &exe->target;
}

/* ================================================================
 *  Link lib
 * ================================================================ */

typedef struct { Bld* b; Bld_Lib* lib; } Bld__LibCtx;

static const char* bld__lib_filename(const Bld_Toolchain* tc, const Bld_LibOpts* o) {
    const char* base = (o->lib_basename && o->lib_basename[0]) ? o->lib_basename : o->name;
    return o->shared
        ? bld_str_fmt("%s%s.%s", tc->shared_lib_prefix, base, tc->shared_lib_ext)
        : bld_str_fmt("%s%s.%s", tc->static_lib_prefix, base, tc->static_lib_ext);
}

static Bld_ActionResult bld__link_lib_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)depfile;
    Bld__LibCtx* c = ctx;
    Bld_Toolchain* tc = c->lib->toolchain;
    Bld_Cmd cmd = {0};

    /* build obj paths from exit->inputs */
    Bld_Paths obj_paths = {0};
    for (size_t i = 0; i < c->lib->target.exit->inputs.count; i++) {
        Bld_Step* inp = c->lib->target.exit->inputs.items[i];
        if (inp->hash_valid)
            bld_paths_push(&obj_paths, bld__step_artifact(c->b, inp).s);
    }

    if (!c->lib->opts.shared) {
        /* static: archive */
        if (!tc->archiver.driver) bld_panic("no archive tool for toolchain '%s'\n", tc->name);
        tc->render_archive(&cmd, tc->archiver.driver, output.s, obj_paths);
    } else {
        /* shared: build LinkCmd with shared=1 */
        const char* soname = bld__lib_filename(tc, &c->lib->opts);
        Bld_Compiler* linker = bld__link_compiler(c->b, &c->lib->obj_steps);
        bool debug_info = bld__toggle_val(c->lib->opts.compile.debug_info, BLD_UNSET);
        bool asan       = bld__toggle_val(c->b->build_flags.asan, BLD_UNSET);
        bool lto        = bld__toggle_val(c->b->build_flags.lto,  BLD_UNSET);

        Bld_LinkCmd lc = {0};
        lc.driver       = linker->driver;
        lc.shared       = true;
        lc.debug_info   = debug_info;
        lc.asan         = asan;
        lc.lto          = lto;
        lc.soname       = soname;
        lc.obj_paths    = obj_paths;
        lc.extra_ldflags = c->lib->opts.link.extra_flags;
        lc.output       = output.s;
        tc->render_link(&cmd, lc);
    }
    if (c->b->settings.verbose) bld_log_action("link lib: %s\n", cmd.items);
    int rc = system(cmd.items);
    return rc == 0 ? BLD_ACTION_OK : BLD_ACTION_FAILED;
}

static Bld_Hash bld__link_lib_recipe(void* ctx, Bld_Hash h) {
    Bld__LibCtx* c = ctx;
    Bld_Toolchain* tc = c->lib->toolchain;
    h = bld_hash_combine(h, bld_hash_str(c->lib->opts.name));
    h = bld_hash_combine(h, bld_hash_str(tc->name));
    h = bld_hash_combine(h, bld_hash_str(tc->obj_ext));
    h = bld_hash_combine(h, bld_hash_str(tc->shared_lib_ext));
    h = bld_hash_combine(h, (Bld_Hash){c->lib->opts.shared});
    h = bld__hash_link_flags(h, &c->lib->opts.link);
    h = bld__hash_build_flags(h, &c->b->build_flags);
    if (c->lib->opts.shared) {
        Bld_Compiler* linker = bld__link_compiler(c->b, &c->lib->obj_steps);
        h = bld_hash_combine(h, linker->identity_hash);
    } else if (tc->archiver.driver) {
        h = bld_hash_combine(h, tc->archiver.identity_hash);
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
    lib->opts.lib_basename = opts->lib_basename ? bld_str_dup(opts->lib_basename) : NULL;
    lib->opts.sources = bld__clone_paths(opts->sources);
    lib->opts.compile = bld_clone_compile_flags(opts->compile);
    lib->opts.compile_propagate = bld_clone_compile_flags(opts->compile_propagate);
    lib->opts.link = bld_clone_link_flags(opts->link);
    lib->opts.link_propagate = bld_clone_link_flags(opts->link_propagate);
    lib->toolchain = opts->toolchain ? opts->toolchain : b->toolchain;

    bld__init_target(b, &lib->target, BLD_TGT_LIB, lib->opts.name, lib->opts.desc);

    Bld__LibCtx* ctx = bld_arena_alloc(sizeof(Bld__LibCtx));
    *ctx = (Bld__LibCtx){.b = b, .lib = lib};
    lib->target.exit->action = bld__link_lib_action;
    lib->target.exit->action_ctx = ctx;
    lib->target.exit->hash_fn = bld__link_lib_recipe;
    lib->target.exit->hash_fn_ctx = ctx;

    for (size_t i = 0; i < lib->opts.sources.count; i++) {
        Bld_Step* obj = bld__add_obj(b, &lib->target, bld_path(lib->opts.sources.items[i]), lib->opts.compile, true, lib->opts.lang);
        bld_da_push(&lib->obj_steps, obj);
        bld_da_push(&lib->target.exit->inputs, obj);
    }
    bld_da_push(&b->target_default->entry->deps, lib->target.exit);
    return &lib->target;
}

/* ---- Shared lib publish (copy .so to out/lib/ for exe linking) ---- */

typedef struct { Bld* b; Bld_Lib* lib; } Bld__PublishCtx;

static Bld_ActionResult bld__publish_lib_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__PublishCtx* c = ctx;
    Bld_Path src = bld__target_artifact(c->b, &c->lib->target);
    Bld_Path dst = bld_path_join(bld_path_join(c->b->out, bld_path("lib")),
                                 bld_path(bld__lib_filename(c->lib->toolchain, &c->lib->opts)));
    bld_fs_mkdir_p(bld_path_parent(dst));
    bld_fs_copy_file(src, dst);
    return BLD_ACTION_OK;
}

static Bld_Step* bld__ensure_publish_step(Bld* b, Bld_Lib* lib) {
    if (lib->publish_step) return lib->publish_step;
    const char* name = bld_str_fmt("publish:%s", lib->opts.name);
    Bld_Step* s = bld__alloc_step(b, name, true);
    s->phony = true;
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

typedef struct { Bld* b; Bld_Paths watch; } Bld__StepHashCtx;

static Bld_Hash bld__step_watch_hash(void* ctx, Bld_Hash h) {
    Bld__StepHashCtx* c = ctx;
    for (size_t i = 0; i < c->watch.count; i++) {
        Bld_Path p = bld_path_join(c->b->root, bld_path(c->watch.items[i]));
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
    t->exit->content_hash = opts->watch.count ? true : opts->content_hash;
    if (opts->hash_fn) {
        t->exit->hash_fn = opts->hash_fn;
        t->exit->hash_fn_ctx = opts->hash_fn_ctx;
    } else if (opts->watch.count) {
        Bld__StepHashCtx* ctx = bld_arena_alloc(sizeof(Bld__StepHashCtx));
        *ctx = (Bld__StepHashCtx){.b = b, .watch = bld__clone_paths(opts->watch)};
        t->exit->hash_fn = bld__step_watch_hash;
        t->exit->hash_fn_ctx = ctx;
    }
    return t;
}

/* ================================================================
 *  Cmd — shell command step
 * ================================================================ */

typedef struct { const char* cmd; } Bld__CmdCtx;

static Bld_ActionResult bld__cmd_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__CmdCtx* c = ctx;
    int rc = system(c->cmd);
    if (rc != 0) return BLD_ACTION_FAILED;
    return BLD_ACTION_OK;
}

static Bld_Hash bld__cmd_hash(void* ctx, Bld_Hash h) {
    Bld__CmdCtx* c = ctx;
    return bld_hash_combine(h, bld_hash_str(c->cmd));
}

Bld_Target* bld__add_cmd(Bld* b, const Bld_CmdOpts* opts) {
    if (!opts->name) bld_panic("cmd name must not be NULL\n");
    if (!opts->cmd)  bld_panic("cmd command must not be NULL\n");
    Bld__CmdCtx* ctx = bld_arena_alloc(sizeof(Bld__CmdCtx));
    *ctx = (Bld__CmdCtx){.cmd = bld_str_dup(opts->cmd)};
    Bld_StepOpts step = {
        .name         = opts->name,
        .desc         = opts->desc,
        .action       = bld__cmd_action,
        .action_ctx   = ctx,
        .hash_fn      = bld__cmd_hash,
        .hash_fn_ctx  = ctx,
        .content_hash = true,
        .watch        = opts->watch,
    };
    return bld__add_step(b, &step);
}

/* ================================================================
 *  Run
 * ================================================================ */

typedef struct { Bld* b; Bld_Target* exe_tgt; Bld_RunOpts opts; } Bld__RunCtx;

static Bld_ActionResult bld__run_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__RunCtx* c = ctx;
    Bld_Cmd cmd = {0};
    if (c->opts.working_dir && c->opts.working_dir[0])
        bld_cmd_appendf(&cmd, "cd \"%s\" && ", c->opts.working_dir);
    bld_cmd_appendf(&cmd, "\"%s\"", bld__target_artifact(c->b, c->exe_tgt).s);
    for (size_t i = 0; i < c->opts.args.count; i++) bld_cmd_appendf(&cmd, " \"%s\"", c->opts.args.items[i]);
    for (size_t i = 0; i < c->b->settings.passthrough.count; i++) bld_cmd_appendf(&cmd, " \"%s\"", c->b->settings.passthrough.items[i]);
    int rc = system(cmd.items);
    if (rc != 0) return BLD_ACTION_FAILED;
    return BLD_ACTION_OK;
}

Bld_Target* bld__add_run(Bld* b, Bld_Target* target, const Bld_RunOpts* opts) {
    Bld_Target* run = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, run, BLD_TGT_CUSTOM, opts->name ? opts->name : "run", opts->desc);
    bld_da_push(&run->entry->deps, target->exit);
    run->exit->phony = true; /* always execute, never cache */
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
    inst->exit->phony = true; /* always copy to destination */
    Bld__InstallCtx* ctx = bld_arena_alloc(sizeof(Bld__InstallCtx));
    *ctx = (Bld__InstallCtx){.b = b, .src = target, .dst = full};
    inst->exit->action = bld__install_action;
    inst->exit->action_ctx = ctx;
    bld_da_push(&b->target_default->entry->deps, inst->exit);
    return inst;
}

Bld_Target* bld_install_exe(Bld* b, Bld_Target* t) {
    Bld_Exe* exe = bld__as_exe(t);
    const char* oname = (exe->opts.output_name && exe->opts.output_name[0]) ? exe->opts.output_name : exe->opts.name;
    return bld_install(b, t, bld_path_join(bld_path("bin"), bld_path(oname)));
}

Bld_Target* bld_install_lib(Bld* b, Bld_Target* t) {
    Bld_Lib* lib = bld__as_lib(t);
    return bld_install(b, t, bld_path_join(bld_path("lib"), bld_path(bld__lib_filename(lib->toolchain, &lib->opts))));
}

typedef struct { Bld* b; Bld_Paths files; Bld_Path dst; } Bld__InstallFilesCtx;

static Bld_ActionResult bld__install_files_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__InstallFilesCtx* c = ctx;
    Bld_Path full = bld_path_join(c->b->out, c->dst);
    bld_fs_mkdir_p(full);
    for (size_t i = 0; i < c->files.count; i++) {
        Bld_Path src = bld_path(c->files.items[i]);
        const char* fname = bld_path_filename(src);
        Bld_Path dest = bld_path_join(full, bld_path(fname));
        bld_fs_copy_file(src, dest);
    }
    return BLD_ACTION_OK;
}

Bld_Target* bld_install_files(Bld* b, Bld_Paths files, Bld_Path dst) {
    Bld_Target* inst = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, inst, BLD_TGT_CUSTOM, "install-files", "Install files");
    inst->exit->phony = true;
    Bld__InstallFilesCtx* ctx = bld_arena_alloc(sizeof(Bld__InstallFilesCtx));
    *ctx = (Bld__InstallFilesCtx){.b = b, .files = files, .dst = dst};
    inst->exit->action = bld__install_files_action;
    inst->exit->action_ctx = ctx;
    bld_da_push(&b->target_default->entry->deps, inst->exit);
    return inst;
}

typedef struct { Bld* b; const char* src_dir; Bld_Path dst; } Bld__InstallDirCtx;

static Bld_ActionResult bld__install_dir_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld__InstallDirCtx* c = ctx;
    Bld_Path full = bld_path_join(c->b->out, c->dst);
    bld_fs_mkdir_p(full);
    bld_fs_copy_r(bld_path(c->src_dir), full);
    return BLD_ACTION_OK;
}

Bld_Target* bld_install_dir(Bld* b, const char* src_dir, Bld_Path dst) {
    Bld_Target* inst = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, inst, BLD_TGT_CUSTOM,
                     bld_str_fmt("install-dir-%s", src_dir),
                     bld_str_fmt("Install directory %s", src_dir));
    inst->exit->phony = true;
    Bld__InstallDirCtx* ctx = bld_arena_alloc(sizeof(Bld__InstallDirCtx));
    *ctx = (Bld__InstallDirCtx){.b = b, .src_dir = src_dir, .dst = dst};
    inst->exit->action = bld__install_dir_action;
    inst->exit->action_ctx = ctx;
    bld_da_push(&b->target_default->entry->deps, inst->exit);
    return inst;
}

/* ================================================================
 *  Tests
 * ================================================================ */

Bld_Target* bld__add_test(Bld* b, Bld_Target* exe, const Bld_RunOpts* opts) {
    Bld_TestEntry entry = {0};
    entry.name = opts->name ? bld_str_dup(opts->name) : exe->name;
    entry.exe = exe;
    entry.working_dir = opts->working_dir ? bld_str_dup(opts->working_dir) : NULL;
    entry.args = bld__clone_strs(opts->args);
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
    b->toolchain = parent->toolchain;
    b->global_warnings = parent->global_warnings;
    b->global_optimize = parent->global_optimize;
    b->build_flags = parent->build_flags;
    b->settings = parent->settings;
    b->settings.silent = true; /* child builds are silent by default */
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

bool bld_target_ok(Bld_Target* t) {
    return t->exit->state == BLD_STEP_OK;
}

Bld_Path bld_target_artifact(Bld* b, Bld_Target* t) {
    return bld__step_artifact(b, t->exit);
}

/* Feature checks — implemented in bld_checks.c */

