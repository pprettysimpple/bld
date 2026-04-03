/* bld/bld_dep.c — external dependency discovery implementation */
#pragma once

#include "bld_dep.h"

static Bld_Paths bld__dep_clone_paths(Bld_Paths s) {
    if (s.len == 0) return (Bld_Paths){0};
    const char** copy = bld_arena_alloc(s.len * sizeof(const char*));
    for (size_t i = 0; i < s.len; i++) copy[i] = bld_str_dup(s.items[i]);
    return (Bld_Paths){copy, s.len, 0};
}

static Bld_Strs bld__dep_clone_strs(Bld_Strs s) {
    if (s.len == 0) return (Bld_Strs){0};
    const char** copy = bld_arena_alloc(s.len * sizeof(const char*));
    for (size_t i = 0; i < s.len; i++) copy[i] = bld_str_dup(s.items[i]);
    return (Bld_Strs){copy, s.len, 0};
}

Bld_Dep* bld__dep(const Bld_Dep* d) {
    Bld_Dep* c = bld_arena_alloc(sizeof(Bld_Dep));
    *c = *d;
    c->found = true;
    if (d->name) c->name = bld_str_dup(d->name);
    c->include_dirs = bld__dep_clone_paths(d->include_dirs);
    c->system_include_dirs = bld__dep_clone_paths(d->system_include_dirs);
    c->libs = bld__dep_clone_strs(d->libs);
    c->lib_dirs = bld__dep_clone_paths(d->lib_dirs);
    if (d->extra_cflags) c->extra_cflags = bld_str_dup(d->extra_cflags);
    if (d->extra_ldflags) c->extra_ldflags = bld_str_dup(d->extra_ldflags);
    return c;
}

static void bld__parse_pkg_flags(const char* output, Bld_Dep* dep, bool is_libs) {
    if (!output || !output[0]) return;

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
            if (strncmp(s, "-L", 2) == 0)      bld_paths_push(&dep->lib_dirs, bld_str_dup(s + 2));
            else if (strncmp(s, "-l", 2) == 0)  bld_strs_push(&dep->libs, bld_str_dup(s + 2));
        } else {
            if (strncmp(s, "-isystem", 8) == 0) {
                if (len > 8) bld_paths_push(&dep->system_include_dirs, bld_str_dup(s + 8));
                else {
                    while (*p == ' ') p++;
                    const char* t2 = p;
                    while (*p && *p != ' ' && *p != '\n') p++;
                    if (p > t2) {
                        char* s2 = bld_arena_alloc((size_t)(p - t2) + 1);
                        memcpy(s2, t2, (size_t)(p - t2)); s2[p - t2] = '\0';
                        bld_paths_push(&dep->system_include_dirs, s2);
                    }
                }
            }
            else if (strncmp(s, "-I", 2) == 0)  bld_paths_push(&dep->system_include_dirs, bld_str_dup(s + 2));
        }
    }
}

Bld_Dep* bld_find_pkg(const char* name) {
    Bld_Dep* dep = bld_arena_alloc(sizeof(Bld_Dep));
    memset(dep, 0, sizeof(*dep));
    dep->name = bld_str_dup(name);

    const char* check = bld_str_fmt("pkg-config --exists %s 2>/dev/null", name);
    if (system(check) != 0) {
        dep->found = false;
        return dep;
    }
    dep->found = true;

    const char* cmd_cf = bld_str_fmt("pkg-config --cflags %s 2>/dev/null", name);
    FILE* f = popen(cmd_cf, "r");
    if (f) {
        char buf[4096] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        pclose(f);
        bld__parse_pkg_flags(buf, dep, false);
    }

    const char* cmd_libs = bld_str_fmt("pkg-config --libs %s 2>/dev/null", name);
    f = popen(cmd_libs, "r");
    if (f) {
        char buf[4096] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        pclose(f);
        bld__parse_pkg_flags(buf, dep, true);
    }

    return dep;
}
