/* bld/bld_checks.c — feature detection implementation */
#pragma once

#include "bld_checks.h"
#include "bld_cache.h"

typedef struct {
    const char*  define_name;
    const char*  snippet;
    bool         is_sizeof;
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

typedef struct { Bld* b; const char* snippet; bool is_sizeof; } Bld__CheckCtx;

static Bld_Hash bld__check_recipe_hash(void* ctx, Bld_Hash h) {
    Bld__CheckCtx* c = ctx;
    h = bld_hash_combine(h, bld_compiler(c->b, BLD_LANG_C)->identity_hash);
    h = bld_hash_combine(h, bld_hash_str(c->snippet));
    return h;
}

static Bld_ActionResult bld__check_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    Bld__CheckCtx* c = ctx;
    const char* cc = bld_compiler(c->b, BLD_LANG_C)->driver;

    Bld_Hash snip_hash = bld_hash_str(c->snippet);
    Bld_Path src = bld_path_join(
        bld_path_join(c->b->cache, bld_path("checks")),
        bld_path_fmt("%" PRIu64 ".c", snip_hash.value));
    bld_fs_mkdir_p(bld_path_parent(src));
    bld_fs_write_file(src, c->snippet, strlen(c->snippet));

    /* compile snippet via toolchain (same compiler/flags as real builds) */
    Bld_Path tmp_obj = bld__cache_tmp(c->b);
    Bld_CompileCmd cc_cmd = {
        .driver = cc,
        .lang = BLD_LANG_C,
        .source = src.s,
        .output = tmp_obj.s,
        .depfile = (!c->is_sizeof && depfile.s && depfile.s[0]) ? depfile.s : "",
    };
    Bld_Cmd cmd = {0};
    if (c->b->toolchain)
        c->b->toolchain->render_compile(&cmd, cc_cmd);
    else
        bld_cmd_appendf(&cmd, "%s -xc %s -c -o %s 2>/dev/null", cc, src.s, tmp_obj.s);
    /* suppress errors */
    bld_cmd_appendf(&cmd, " 2>/dev/null");
    int rc = system(cmd.items);

    if (!c->is_sizeof) {
        bld_fs_write_file(output, rc == 0 ? "1" : "0", 1);
    } else {
        if (rc != 0) {
            bld_fs_write_file(output, "0", 1);
        } else {
            /* scan .o for marker */
            static const char marker[] = "bld_check_entry:qWkPxLmNvRtBjHsYcFgDzAeUoIiXpK:sizeof:";
            size_t marker_len = sizeof(marker) - 1;
            size_t obj_len;
            const char* obj = bld_fs_read_file(tmp_obj, &obj_len);
            const char* found = NULL;
            for (size_t i = 0; i + marker_len + 5 <= obj_len; i++) {
                if (memcmp(obj + i, marker, marker_len) == 0) { found = obj + i + marker_len; break; }
            }
            if (found) {
                int val = 0;
                for (int d = 0; d < 5; d++) val = val * 10 + (found[d] - '0');
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", val);
                bld_fs_write_file(output, buf, strlen(buf));
            } else {
                bld_fs_write_file(output, "0", 1);
            }
        }
    }
    return BLD_ACTION_OK;
}

static void bld__checks_add(Bld_Checks* c, const char* define_name, const char* snippet,
                              bool is_sizeof, bool* bool_result, int* int_result) {
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
        .hash_fn = bld__check_recipe_hash,
        .hash_fn_ctx = ctx,
        .has_depfile = !is_sizeof,
        .content_hash = false,
    });
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
    bld__checks_add(c, define_name,
        bld_str_fmt("#include <%s>\nint main(){return 0;}\n", header), false, result, NULL);
    return result;
}

bool* bld_checks_func(Bld_Checks* c, const char* define_name, const char* func, const char* header) {
    bool* result = bld_arena_alloc(sizeof(bool));
    *result = false;
    bld__checks_add(c, define_name,
        bld_str_fmt("#include <%s>\nint main(){(void)%s;return 0;}\n", header, func), false, result, NULL);
    return result;
}

int* bld_checks_sizeof(Bld_Checks* c, const char* define_name, const char* type) {
    int* result = bld_arena_alloc(sizeof(int));
    *result = 0;
    bld__checks_add(c, define_name, bld_str_fmt(
        "#include <stddef.h>\n#include <sys/types.h>\n#include <time.h>\n"
        "#define BLD__V (sizeof(%s))\n"
        "char bld__m[] = {\n"
        "    'b','l','d','_','c','h','e','c','k','_','e','n','t','r','y',':',\n"
        "    'q','W','k','P','x','L','m','N','v','R','t','B','j','H','s',\n"
        "    'Y','c','F','g','D','z','A','e','U','o','I','i','X','p','K',\n"
        "    ':','s','i','z','e','o','f',':',\n"
        "    ('0'+((BLD__V/10000)%%10)),\n"
        "    ('0'+((BLD__V/1000)%%10)),\n"
        "    ('0'+((BLD__V/100)%%10)),\n"
        "    ('0'+((BLD__V/10)%%10)),\n"
        "    ('0'+(BLD__V%%10)),\n"
        "    '\\0'\n"
        "};\n", type), true, NULL, result);
    return result;
}

bool* bld_checks_compile(Bld_Checks* c, const char* define_name, const char* source) {
    bool* result = bld_arena_alloc(sizeof(bool));
    *result = false;
    bld__checks_add(c, define_name, source, false, result, NULL);
    return result;
}

void bld_checks_run(Bld_Checks* c) {
    bld_execute(c->child);

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
    Bld_Cmd out = {0};
    bld_cmd_appendf(&out, "/* generated by bld feature checks */\n");
    for (size_t i = 0; i < c->count; i++) {
        Bld__Check* ch = &c->items[i];
        if (ch->bool_result) {
            if (*ch->bool_result)
                bld_cmd_appendf(&out, "#define %s 1\n", ch->define_name);
            else
                bld_cmd_appendf(&out, "/* %s not found */\n", ch->define_name);
        } else if (ch->int_result) {
            if (*ch->int_result > 0)
                bld_cmd_appendf(&out, "#define %s %d\n", ch->define_name, *ch->int_result);
            else
                bld_cmd_appendf(&out, "/* %s unknown */\n", ch->define_name);
        }
    }
    bld_fs_write_file(bld_path_join(c->parent->root, bld_path(path)), out.items, out.count);
}
