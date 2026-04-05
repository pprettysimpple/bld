/* bld/bld_dep.c — external dependency discovery implementation */
#pragma once

#include "bld_dep.h"

static void bld__parse_pkg_cflags(const char* output, Bld_Pkg* pkg) {
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

        if (strncmp(s, "-isystem", 8) == 0) {
            if (len > 8) bld_paths_push(&pkg->compile_pub.system_include_dirs, bld_str_dup(s + 8));
            else {
                while (*p == ' ') p++;
                const char* t2 = p;
                while (*p && *p != ' ' && *p != '\n') p++;
                if (p > t2) {
                    char* s2 = bld_arena_alloc((size_t)(p - t2) + 1);
                    memcpy(s2, t2, (size_t)(p - t2)); s2[p - t2] = '\0';
                    bld_paths_push(&pkg->compile_pub.system_include_dirs, s2);
                }
            }
        }
        else if (strncmp(s, "-I", 2) == 0) bld_paths_push(&pkg->compile_pub.system_include_dirs, bld_str_dup(s + 2));
    }
}

static void bld__parse_pkg_libs(const char* output, Bld_Pkg* pkg) {
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

        if (strncmp(s, "-L", 2) == 0)      bld_paths_push(&pkg->link_pub.lib_dirs, bld_str_dup(s + 2));
        else if (strncmp(s, "-l", 2) == 0)  bld_strs_push(&pkg->link_pub.libs, bld_str_dup(s + 2));
    }
}

Bld_Target* bld_find_pkg(Bld* b, const char* name) {
    Bld_PkgOpts opts = {0};
    opts.name = name;
    Bld_Target* t = bld__add_pkg(b, &opts);
    Bld_Pkg* pkg = (Bld_Pkg*)t;

    const char* check = bld_str_fmt("pkg-config --exists %s 2>/dev/null", name);
    if (system(check) != 0) {
        t->found = false;
        return t;
    }
    t->found = true;

    const char* cmd_cf = bld_str_fmt("pkg-config --cflags %s 2>/dev/null", name);
    FILE* f = popen(cmd_cf, "r");
    if (f) {
        char buf[4096] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        pclose(f);
        bld__parse_pkg_cflags(buf, pkg);
    }

    const char* cmd_libs = bld_str_fmt("pkg-config --libs %s 2>/dev/null", name);
    f = popen(cmd_libs, "r");
    if (f) {
        char buf[4096] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = '\0';
        pclose(f);
        bld__parse_pkg_libs(buf, pkg);
    }

    return t;
}
