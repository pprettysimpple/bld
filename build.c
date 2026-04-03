/* build.c — bld builds itself
 *
 * Bootstrap: cc -std=c11 -w build.c -o b -lpthread
 *
 * Targets:
 *   ./b build                         — build libbld.a
 *   ./b amalgamate                    — generate bld_amalgamated.h
 *   ./b install [--prefix /usr/local] — install bld.h + libbld.a
 *   ./b selftest                     — run integration tests
 */
#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"

BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

/* ---- Unity codegen ---- */

static Bld_ActionResult gen_unity(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)depfile;
    Bld* b = ctx;
    bld_fs_mkdir_p(output);
    const char* src = bld_str_fmt(
        "#define BLD_IMPLEMENTATION\n"
        "#include \"%s/bld.h\"\n", b->root.s);
    bld_fs_write_file(bld_path_join(output, bld_path("bld_unity.c")),
                      src, strlen(src));
    return BLD_ACTION_OK;
}

/* ---- Amalgamation codegen ---- */

static void amalg_file(Bld_Strs* out, Bld_Path root, const char* relpath) {
    size_t len;
    const char* content = bld_fs_read_file(bld_path_join(root, bld_path(relpath)), &len);
    bld_strs_push(out, bld_str_fmt("/* --- %s --- */", relpath));
    Bld_Strs lines = bld_str_lines(content);
    for (size_t i = 0; i < lines.len; i++) {
        if (strcmp(lines.items[i], "#pragma once") == 0) continue;
        if (strncmp(lines.items[i], "#include \"bld_", 14) == 0) continue;
        if (strncmp(lines.items[i], "#include \"xxhash.h\"", 19) == 0) {
            amalg_file(out, root, "bld/xxhash.h");
            continue;
        }
        bld_strs_push(out, lines.items[i]);
    }
}

static Bld_ActionResult gen_amalgamated(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)depfile;
    Bld* b = ctx;
    size_t len;
    const char* content = bld_fs_read_file(bld_path_join(b->root, bld_path("bld.h")), &len);
    Bld_Strs out = {0};

    /* embed docs/api.md as a block comment at the top */
    Bld_Path api_md = bld_path_join(b->root, bld_path("docs/api.md"));
    if (bld_fs_is_file(api_md)) {
        size_t doc_len;
        const char* doc = bld_fs_read_file(api_md, &doc_len);
        bld_strs_push(&out, "/*");
        Bld_Strs doc_lines = bld_str_lines(doc);
        for (size_t i = 0; i < doc_lines.len; i++) {
            /* escape star-slash inside doc to avoid breaking the block comment */
            const char* line = doc_lines.items[i];
            if (strstr(line, "*/")) {
                Bld_Cmd esc = {0};
                for (const char* p = line; *p; p++) {
                    if (p[0] == '*' && p[1] == '/') {
                        bld_cmd_appendf(&esc, "*\\/");
                        p++;
                    } else {
                        char c[2] = {*p, '\0'};
                        bld_cmd_appendf(&esc, "%s", c);
                    }
                }
                bld_strs_push(&out, bld_str_fmt(" * %s", esc.items));
            } else {
                bld_strs_push(&out, bld_str_fmt(" * %s", line));
            }
        }
        bld_strs_push(&out, " */");
        bld_strs_push(&out, "");
    }

    Bld_Strs lines = bld_str_lines(content);
    for (size_t i = 0; i < lines.len; i++) {
        if (strncmp(lines.items[i], "#include \"bld/", 14) == 0) {
            const char* q1 = strchr(lines.items[i], '"') + 1;
            const char* q2 = strchr(q1, '"');
            amalg_file(&out, b->root, bld_str_fmt("%.*s", (int)(q2 - q1), q1));
        } else {
            bld_strs_push(&out, lines.items[i]);
        }
    }

    const char* result = bld_str_join(&out, "\n");
    bld_fs_mkdir_p(output);
    bld_fs_write_file(bld_path_join(output, bld_path("bld_amalgamated.h")), result, strlen(result));
    return BLD_ACTION_OK;
}

/* ---- Self-tests ---- */

static Bld_ActionResult run_selftests(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)output; (void)depfile;
    Bld* b = ctx;
    const char* cmd = bld_str_fmt("bash \"%s/tests/run_all.sh\" \"%s\"", b->root.s, b->root.s);
    int rc = system(cmd);
    return rc == 0 ? BLD_ACTION_OK : BLD_ACTION_FAILED;
}

/* ---- Configure ---- */

void configure(Bld* b) {
    bld_set_compiler_c(b, .standard = BLD_C_11);
    CompileFlags flags = default_compile_flags(b);
    flags.warnings = TOGGLE_OFF;

    Bld_Paths src_files = BLD_PATHS("bld.h", "bld/bld_core.h",
        "bld/bld_core_impl.c", "bld/bld_cache.h", "bld/bld_cache.c",
        "bld/bld_dep.h", "bld/bld_dep.c",
        "bld/bld_build.c", "bld/bld_checks.h", "bld/bld_checks.c",
        "bld/bld_exec.c", "bld/bld_cli.c", "bld/xxhash.h");

    Target* unity = add_step(b,
        .name = "unity",
        .desc = "Generate unity source",
        .action = gen_unity,
        .action_ctx = b,
        .watch = src_files);

    Target* lib = add_lib(b, .name = "bld",
        .compile = flags);
    target_add_source(lib, target_output_sub(unity, "bld_unity.c"));

    Bld_Paths amalg_watch = bld_paths_merge(src_files, BLD_PATHS("docs/api.md"));
    Target* amalg = add_step(b,
        .name = "amalgamate",
        .desc = "Generate amalgamated header",
        .action = gen_amalgamated,
        .action_ctx = b,
        .watch = amalg_watch);

    install_lib(b, lib);
    install_target(b, amalg, bld_path("include"));

    Target* selftest = add_step(b,
        .name = "selftest",
        .desc = "Run integration tests",
        .action = run_selftests,
        .action_ctx = b);
    selftest->exit->phony = true;
}
