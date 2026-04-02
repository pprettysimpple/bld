/* bld/bld_exec.c — step execution, topo sort, parallel workers */
#pragma once

#include "bld_build.c"

/* ---- Build stats — stored on Bld, accessed atomically ---- */

/* ---- Step sync ---- */

static Bld_StepState bld__step_state(Bld_Step* s) {
    pthread_mutex_lock(&s->mutex); Bld_StepState st = s->state; pthread_mutex_unlock(&s->mutex); return st;
}
static int bld__step_is_done(Bld_Step* s) {
    Bld_StepState st = bld__step_state(s);
    return st == BLD_STEP_OK || st == BLD_STEP_FAILED || st == BLD_STEP_SKIPPED;
}
static void bld__step_set_state(Bld_Step* s, Bld_StepState st) {
    pthread_mutex_lock(&s->mutex); s->state = st; pthread_cond_broadcast(&s->cond); pthread_mutex_unlock(&s->mutex);
}
static void bld__step_wait(Bld_Step* s) {
    pthread_mutex_lock(&s->mutex);
    while (s->state == BLD_STEP_PENDING || s->state == BLD_STEP_RUNNING)
        pthread_cond_wait(&s->cond, &s->mutex);
    pthread_mutex_unlock(&s->mutex);
}

/* ---- Hash + cache check ---- */

static void bld__compute_step_hash(Bld* b, Bld_Step* step) {
    Bld_Hash h = {0};
    for (size_t i = 0; i < step->deps.count; i++)
        if (step->deps.items[i]->hash_valid)
            h = bld_hash_combine_unordered(h, step->deps.items[i]->full_hash_value);
    for (size_t i = 0; i < step->inputs.count; i++)
        if (step->inputs.items[i]->hash_valid)
            h = bld_hash_combine_unordered(h, step->inputs.items[i]->full_hash_value);
    if (step->recipe_hash)
        h = step->recipe_hash(step->recipe_hash_ctx, h);
    step->recipe_hash_value = h;

    Bld_Hash full_h = h;
    Bld_Path cached_dep = bld__step_depfile_cache(b, step);
    int have_cached_depfile = step->has_depfile && bld_fs_exists(cached_dep);
    if (have_cached_depfile)
        full_h = bld_hash_combine(h, bld__hash_depfile_contents(cached_dep));
    step->full_hash_value = full_h;
    step->hash_valid = 1;
}

/* bld__cache_has() is defined in bld_build.c */

/* ---- Perform step ---- */

static void bld__perform_step(Bld* b, Bld_Step* step) {
    if (bld__step_is_done(step)) return;

    /* check if any dep failed → skip */
    for (size_t i = 0; i < step->deps.count; i++) {
        Bld_StepState ds = bld__step_state(step->deps.items[i]);
        if (ds == BLD_STEP_FAILED || ds == BLD_STEP_SKIPPED) {
            __atomic_fetch_add(&b->steps_skipped, 1, __ATOMIC_RELAXED);
            bld__step_set_state(step, BLD_STEP_SKIPPED);
            return;
        }
    }
    for (size_t i = 0; i < step->inputs.count; i++) {
        if (!step->inputs.items[i]) continue;
        Bld_StepState ds = bld__step_state(step->inputs.items[i]);
        if (ds == BLD_STEP_FAILED || ds == BLD_STEP_SKIPPED) {
            __atomic_fetch_add(&b->steps_skipped, 1, __ATOMIC_RELAXED);
            bld__step_set_state(step, BLD_STEP_SKIPPED);
            return;
        }
    }

    bld__step_set_state(step, BLD_STEP_RUNNING);
    bld__compute_step_hash(b, step);

    if (!step->action) { bld__step_set_state(step, BLD_STEP_OK); return; }

    if (bld__cache_has(b, step)) {
        __atomic_fetch_add(&b->steps_cached, 1, __ATOMIC_RELAXED);
        bld__step_set_state(step, BLD_STEP_OK);
        return;
    }

    /* execute */
    Bld_Path tmp_out = bld__cache_tmp(b);
    Bld_Path tmp_dep = step->has_depfile ? bld__cache_tmp(b) : bld_path("");
    Bld_ActionResult result = step->action(step->action_ctx, tmp_out, tmp_dep);

    if (result != BLD_ACTION_OK) {
        __atomic_fetch_add(&b->steps_failed, 1, __ATOMIC_RELAXED);
        bld__step_set_state(step, BLD_STEP_FAILED);
        return;
    }

    bld__cache_store_depfile(b, step, tmp_dep);
    bld__cache_store_artifact(b, step, tmp_out);

    __atomic_fetch_add(&b->steps_executed, 1, __ATOMIC_RELAXED);
    if (!b->settings.silent) {
        uint64_t n = __atomic_fetch_add(&b->progress_current, 1, __ATOMIC_RELAXED) + 1;
        bld_log_progress(n, b->progress_total, step->name);
    }
    bld__step_set_state(step, BLD_STEP_OK);
}

/* ---- Shared infrastructure ---- */

static size_t bld__step_idx(Bld* b, Bld_Step* s) {
    for (size_t i = 0; i < b->all_steps.count; i++)
        if (b->all_steps.items[i] == s) return i;
    bld_panic("step %s not found\n", s->name);
    return 0;
}

typedef struct {
    Bld* b; Bld_Step** order; size_t count; pthread_mutex_t* mu; size_t* idx;
} Bld__WorkerCtx;

static void* bld__worker_fn(void* arg) {
    Bld__WorkerCtx* c = arg;
    while (1) {
        pthread_mutex_lock(c->mu);
        if (*c->idx >= c->count) { pthread_mutex_unlock(c->mu); break; }
        Bld_Step* step = c->order[(*c->idx)++];
        pthread_mutex_unlock(c->mu);
        for (size_t i = 0; i < step->deps.count; i++) bld__step_wait(step->deps.items[i]);
        for (size_t i = 0; i < step->inputs.count; i++)
            if (step->inputs.items[i]) bld__step_wait(step->inputs.items[i]);
        bld__perform_step(c->b, step);
    }
    return NULL;
}

static void bld__check_missing_deps(Bld* b) {
    for (size_t i = 0; i < b->all_targets.count; i++) {
        Bld_Target* t = b->all_targets.items[i];
        for (size_t j = 0; j < t->ext_deps.count; j++) {
            Bld_Dep* d = t->ext_deps.items[j];
            if (!d->found)
                bld_panic("dependency '%s' not found (required by target '%s')\n",
                          d->name ? d->name : "unknown", t->name);
        }
    }
}

static Bld_StepList bld__topo_sort(Bld* b, Bld_StepList* roots) {
    Bld_StepList order = {0};
    enum { WHITE = 0, GRAY, BLACK };
    size_t ns = b->all_steps.count;
    int* col = bld_arena_alloc(ns * sizeof(int));
    memset(col, 0, ns * sizeof(int));

    typedef struct { Bld_Step* s; size_t di, ii; } Fr;
    BLD_DA(Fr) stk = {0};

    for (size_t ti = 0; ti < roots->count; ti++) {
        size_t ri = bld__step_idx(b, roots->items[ti]);
        if (col[ri] == BLACK) continue;
        bld_da_push(&stk, ((Fr){roots->items[ti], 0, 0}));
        col[ri] = GRAY;
        while (stk.count > 0) {
            Fr* f = &stk.items[stk.count - 1];
            int pushed = 0;
            while (f->di < f->s->deps.count) {
                Bld_Step* d = f->s->deps.items[f->di++];
                size_t di = bld__step_idx(b, d);
                if (col[di] == BLACK) continue;
                if (col[di] == GRAY) bld_panic("cycle: %s -> %s\n", f->s->name, d->name);
                col[di] = GRAY; bld_da_push(&stk, ((Fr){d, 0, 0})); pushed = 1; break;
            }
            if (pushed) continue;
            while (f->ii < f->s->inputs.count) {
                Bld_Step* d = f->s->inputs.items[f->ii++];
                if (!d) continue;
                size_t di = bld__step_idx(b, d);
                if (col[di] == BLACK) continue;
                if (col[di] == GRAY) bld_panic("cycle: %s -> %s\n", f->s->name, d->name);
                col[di] = GRAY; bld_da_push(&stk, ((Fr){d, 0, 0})); pushed = 1; break;
            }
            if (pushed) continue;
            col[bld__step_idx(b, f->s)] = BLACK;
            bld_da_push(&order, f->s);
            stk.count--;
        }
    }
    return order;
}

static void bld__build_steps(Bld* b, Bld_StepList order) {
    b->steps_executed = 0;
    b->steps_cached = 0;
    b->steps_failed = 0;
    b->steps_skipped = 0;
    b->progress_current = 0;
    b->progress_total = 0;

    /* pre-pass: resolve cached, count dirty */
    for (size_t i = 0; i < order.count; i++) {
        Bld_Step* step = order.items[i];
        int any_dep_dirty = 0;
        for (size_t j = 0; j < step->deps.count; j++)
            if (!bld__step_is_done(step->deps.items[j])) { any_dep_dirty = 1; break; }
        if (!any_dep_dirty)
            for (size_t j = 0; j < step->inputs.count; j++)
                if (step->inputs.items[j] && !bld__step_is_done(step->inputs.items[j])) { any_dep_dirty = 1; break; }
        if (any_dep_dirty) {
            if (step->action) b->progress_total++;
            continue;
        }
        bld__compute_step_hash(b, step);
        if (bld__cache_has(b, step)) {
            __atomic_fetch_add(&b->steps_cached, 1, __ATOMIC_RELAXED);
            if (b->settings.show_cached && step->action && !step->silent)
                bld_log_cached(step->name);
            bld__step_set_state(step, BLD_STEP_OK);
        } else {
            b->progress_total++;
        }
    }

    if (b->progress_total == 0) return;

    /* execute */
    pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
    size_t idx = 0;
    if (b->settings.max_jobs <= 1) {
        for (size_t i = 0; i < order.count; i++) bld__perform_step(b, order.items[i]);
    } else {
        pthread_t* th = bld_arena_alloc(sizeof(pthread_t) * (size_t)b->settings.max_jobs);
        Bld__WorkerCtx ctx = {b, order.items, order.count, &mu, &idx};
        Bld__WorkerCtx* shared = bld_arena_alloc(sizeof(Bld__WorkerCtx));
        *shared = ctx;
        for (int t = 0; t < b->settings.max_jobs; t++) pthread_create(&th[t], NULL, bld__worker_fn, shared);
        for (int t = 0; t < b->settings.max_jobs; t++) pthread_join(th[t], NULL);
    }
}

/* ---- Run build (called from main) ---- */

static void bld__run_build(Bld* b) {
    /* resolve requested targets */
    Bld_StepList to_build = {0};
    for (const char** rq = b->settings.targets; rq && *rq; rq++) {
        int found = 0;
        for (size_t i = 0; i < b->all_targets.count; i++) {
            if (strcmp(b->all_targets.items[i]->name, *rq) == 0) {
                bld_da_push(&to_build, b->all_targets.items[i]->exit);
                found = 1;
            }
        }
        if (!found) bld_panic("unknown target: %s\n", *rq);
    }

    bld__check_missing_deps(b);

    Bld_StepList order = bld__topo_sort(b, &to_build);

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    bld__build_steps(b, order);

    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    if (!b->settings.silent)
        bld_log_done(b->steps_executed, b->steps_cached, b->steps_failed, b->steps_skipped, elapsed, bld_arena_get()->offset);
    if (b->steps_failed > 0) exit(1);
}

/* ---- Public: execute all targets in a Bld context ---- */

void bld_execute(Bld* b) {
    bld__materialize_lazy_sources(b);
    bld__resolve_link_deps(b);
    bld__check_missing_deps(b);

    Bld_StepList roots = {0};
    for (size_t i = 0; i < b->all_targets.count; i++)
        bld_da_push(&roots, b->all_targets.items[i]->exit);

    Bld_StepList order = bld__topo_sort(b, &roots);
    bld__build_steps(b, order);
}

