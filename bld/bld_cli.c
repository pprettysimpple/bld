/* bld/bld_cli.c — CLI parsing, help, self-recompilation, main() */
#pragma once

#include "bld_exec.c"

/* ---- CLI parsing ---- */

static void bld__parse_args(Bld* b) {
    Bld_Settings* s = &b->settings;
    Bld_Strings targets = {0};
    Bld_Strings passthrough = {0};
    int after_dd = 0;
    for (int i = 1; i < b->argc; i++) {
        const char* a = b->argv[i];
        if (after_dd) { bld_da_push(&passthrough, a); continue; }
        if (strcmp(a, "--") == 0) { after_dd = 1; continue; }
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0 || strcmp(a, "help") == 0) { s->show_help = 1; continue; }
        if (strcmp(a, "-v") == 0 || strcmp(a, "--verbose") == 0) { s->verbose = 1; continue; }
        if (strcmp(a, "-s") == 0 || strcmp(a, "--silent") == 0) { s->silent = 1; continue; }
        if (strcmp(a, "--show-cached") == 0) { s->show_cached = 1; continue; }
        if (strcmp(a, "-k") == 0 || strcmp(a, "--keep-going") == 0) { s->keep_going = 1; continue; }
        if (strcmp(a, "--debug") == 0) { s->mode = BLD_MODE_DEBUG; continue; }
        if (strcmp(a, "--release") == 0) { s->mode = BLD_MODE_RELEASE; continue; }
        if (strcmp(a, "--prefix") == 0) {
            if (i + 1 >= b->argc) bld_panic("expected path after --prefix\n");
            s->install_prefix = b->argv[++i]; continue;
        }
        if (strcmp(a, "-j") == 0 || strcmp(a, "--jobs") == 0) {
            if (i + 1 >= b->argc) bld_panic("expected number after %s\n", a);
            s->max_jobs = atoi(b->argv[++i]); continue;
        }
        if (strncmp(a, "-j", 2) == 0 && a[2]) { s->max_jobs = atoi(a + 2); continue; }
        if (strncmp(a, "-D", 2) == 0) {
            const char* arg = a + 2;
            if (!arg[0] && i + 1 < b->argc) arg = b->argv[++i];
            const char* eq = strchr(arg, '=');
            Bld_UserOption entry = {0};
            if (eq) {
                size_t klen = (size_t)(eq - arg);
                char* k = bld_arena_alloc(klen + 1);
                memcpy(k, arg, klen); k[klen] = '\0';
                entry.key = k;
                entry.value = bld_str_dup(eq + 1);
            } else {
                entry.key = bld_str_dup(arg);
                entry.value = NULL;
            }
            bld_da_push(&b->user_options, entry);
            continue;
        }
        bld_da_push(&targets, a);
    }
    bld_da_push(&passthrough, (const char*)NULL);
    s->passthrough = passthrough.items;
    bld_da_push(&targets, (const char*)NULL);
    s->targets = targets.items;
    if (s->max_jobs <= 0) { long n = sysconf(_SC_NPROCESSORS_ONLN); s->max_jobs = n > 0 ? (int)n : 1; }
    if (!s->targets[0]) s->show_help = 1;
}

/* ---- Help ---- */

static void bld__show_help(Bld* b) {
    const char* G = bld__c(BLD_C_GREEN);
    const char* Y = bld__c(BLD_C_YELLOW);
    const char* D = bld__c(BLD_C_DIM);
    const char* R = bld__c(BLD_C_RESET);

    bld_log("Usage: %s [options] [targets] [-- args]\n\n", b->argv[0]);

    bld_log("%sOptions:%s\n", Y, R);
    bld_log("  %s-h, --help%s       Show help\n", G, R);
    bld_log("  %s-v, --verbose%s    Verbose output\n", G, R);
    bld_log("  %s-s, --silent%s     Silent mode\n", G, R);
    bld_log("  %s--show-cached%s    Show cached steps\n", G, R);
    bld_log("  %s-k, --keep-going%s Continue after errors\n", G, R);
    bld_log("  %s--debug%s          Debug mode (-O0 -g)\n", G, R);
    bld_log("  %s--release%s        Release mode (-O2 -DNDEBUG)\n", G, R);
    bld_log("  %s--prefix <path>%s  Install prefix %s(default: build/)%s\n", G, R, D, R);
    bld_log("  %s-j <N>%s           Parallel jobs\n\n", G, R);

    bld_log("%sBuilt-in:%s\n", Y, R);
    bld_log("  %s%-20s%s %s\n", G, "clean", R, "Remove cache and build directories");
    bld_log("  %s%-20s%s %s\n", G, "test", R, "Run all registered tests");

    if (b->avail_options.count > 0) {
        /* compute auto-width for option names */
        size_t max_name = 0;
        for (size_t i = 0; i < b->avail_options.count; i++) {
            size_t len = strlen(b->avail_options.items[i].name) + 2; /* -D prefix */
            if (len > max_name) max_name = len;
        }
        if (max_name < 20) max_name = 20;
        max_name += 2; /* padding */

        bld_log("\n%sProject options:%s\n", Y, R);
        for (size_t i = 0; i < b->avail_options.count; i++) {
            Bld_AvailableOption* o = &b->avail_options.items[i];
            const char* type = o->type == BLD_OPT_TYPE_BOOL ? "bool" : "string";
            const char* flag = bld_str_fmt("-D%s", o->name);
            bld_log("  %s%-*s%s %s %s[%s, default: %s]%s\n",
                    G, (int)max_name, flag, R,
                    o->description,
                    D, type, o->default_val, R);
        }
    }

    bld_log("\n%sTargets:%s\n", Y, R);
    for (size_t i = 0; i < b->all_targets.count; i++) {
        Bld_Target* t = b->all_targets.items[i];
        if (t->exit->silent) continue;
        bld_log("  %s%-20s%s %s\n", G, t->name, R, t->desc);
    }
}

static int bld__handle_clean(Bld* b) {
    for (const char** rq = b->settings.targets; rq && *rq; rq++) {
        if (strcmp(*rq, "clean") == 0) {
            bld_log_info("[*] Cleaning %s and %s...\n", b->cache.s, b->out.s);
            bld_fs_remove_all(b->cache);
            bld_fs_remove_all(b->out);
            bld_log_info("[*] Clean complete.\n");
            return 1;
        }
    }
    return 0;
}

/* ---- Test runner ---- */

static int bld__handle_test(Bld* b) {
    /* check if "test" is among requested targets */
    int want_test = 0;
    for (const char** rq = b->settings.targets; rq && *rq; rq++)
        if (strcmp(*rq, "test") == 0) { want_test = 1; break; }
    if (!want_test) return 0;
    if (b->tests.count == 0) { bld_log("no tests registered\n"); return 1; }

    /* first build all test executables */
    Bld_StepList to_build = {0};
    for (size_t i = 0; i < b->tests.count; i++)
        bld_da_push(&to_build, b->tests.items[i].exe->exit);
    bld__check_missing_deps(b);
    Bld_StepList order = bld__topo_sort(b, &to_build);

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    bld__build_steps(b, order);

    if (b->steps_failed > 0) {
        bld_log_done(b->steps_executed, b->steps_cached, b->steps_failed, b->steps_skipped,
                     0, bld_arena_get()->offset);
        return 1;
    }

    /* create test output dir */
    Bld_Path test_dir = bld_path_join(b->cache, bld_path("test-output"));
    bld_fs_mkdir_p(test_dir);

    /* run tests — fork each, collect results */
    size_t passed = 0, failed = 0;
    struct timespec test_t0;
    clock_gettime(CLOCK_MONOTONIC, &test_t0);

    for (size_t i = 0; i < b->tests.count; i++) {
        Bld_TestEntry* te = &b->tests.items[i];
        Bld_Path exe_path = bld__target_artifact(b, te->exe);
        Bld_Path log_path = bld_path_join(test_dir, bld_path(bld_str_fmt("%s.log", te->name)));

        /* build command */
        Bld__Cmd cmd = {0};
        if (te->working_dir && te->working_dir[0])
            bld__cmd_appendf(&cmd, "cd \"%s\" && ", te->working_dir);
        bld__cmd_appendf(&cmd, "\"%s\"", exe_path.s);
        if (te->args) for (const char** a = te->args; *a; a++) bld__cmd_appendf(&cmd, " \"%s\"", *a);
        bld__cmd_appendf(&cmd, " > \"%s\" 2>&1", log_path.s);

        struct timespec st0;
        clock_gettime(CLOCK_MONOTONIC, &st0);

        int rc = system(cmd.items);

        struct timespec st1;
        clock_gettime(CLOCK_MONOTONIC, &st1);
        double elapsed = (double)(st1.tv_sec - st0.tv_sec) + (double)(st1.tv_nsec - st0.tv_nsec) / 1e9;

        if (rc == 0) {
            passed++;
        } else {
            failed++;
            bld_log("%sFAIL:%s %s (%.2fs)\n", bld__c(BLD_C_RED), bld__c(BLD_C_RESET), te->name, elapsed);
            /* show tail of output */
            const char* tail_cmd = bld_str_fmt("tail -50 \"%s\"", log_path.s);
            bld_log("%s", bld__c(BLD_C_DIM));
            if (system(tail_cmd)) { /* ignore tail errors */ }
            bld_log("%s", bld__c(BLD_C_RESET));
            bld_log("  full output: %s\n", log_path.s);
        }
    }

    struct timespec test_t1;
    clock_gettime(CLOCK_MONOTONIC, &test_t1);
    double total = (double)(test_t1.tv_sec - test_t0.tv_sec) + (double)(test_t1.tv_nsec - test_t0.tv_nsec) / 1e9;

    if (failed == 0)
        bld_log("%stests:%s %zu passed, %.2fs\n", bld__c(BLD_C_GREEN), bld__c(BLD_C_RESET), passed, total);
    else
        bld_log("%stests:%s %zu passed, %s%zu failed%s, %.2fs\n",
                bld__c(BLD_C_RED), bld__c(BLD_C_RESET), passed,
                bld__c(BLD_C_RED), failed, bld__c(BLD_C_RESET), total);

    return 1; /* handled — don't run normal build */
}

/* ---- Self-recompilation ---- */

static void bld__recompile_if_needed(Bld* b) {
    Bld_Path src = bld_path_join(b->root, bld_path("build.c"));
    Bld_Hash h = bld_hash_file(src);
    Bld_Path hdr = bld_path_join(b->root, bld_path("bld.h"));
    if (bld_fs_is_file(hdr)) h = bld_hash_combine(h, bld_hash_file(hdr));

    Bld_Path hp = bld_path_join(b->cache, bld_path("bld.hash"));
    if (bld_fs_exists(hp)) {
        size_t len;
        const char* content = bld_fs_read_file(hp, &len);
        uint64_t old = 0;
        if (sscanf(content, "%" SCNu64, &old) == 1 && old == h.value) return;
    }

    bld_log_info("[*] Recompiling build tool...\n");
    int rc = system(bld_str_fmt("%s -o \"%s\"", bld_recompile_cmd, b->argv[0]));
    if (rc != 0) bld_panic("failed to recompile build tool\n");
    const char* hs = bld_str_fmt("%" PRIu64, h.value);
    bld_fs_write_file(hp, hs, strlen(hs));
    execv(b->argv[0], b->argv);
    bld_fs_remove(hp);
    bld_panic("execv failed: %s\n", strerror(errno));
}

/* ---- Init stages ---- */

/* stage 1: core paths + cache dirs */
static void bld__init_core(Bld* b, int argc, char** argv) {
    memset(b, 0, sizeof(*b));
    b->argc = argc;
    b->argv = argv;
    b->root = bld_fs_realpath(bld_path_parent(bld_path(argv[0])));

    Bld_Path cr = bld_path_join(b->root, bld_path(".cache"));
    bld_fs_mkdir_p(cr);
    b->cache = bld_fs_realpath(cr);
    Bld_Path outr = bld_path_join(b->root, bld_path("build"));
    bld_fs_mkdir_p(outr);
    b->out = bld_fs_realpath(outr);

    bld_fs_mkdir_p(bld_path_join(b->cache, bld_path("arts")));
    bld_fs_mkdir_p(bld_path_join(b->cache, bld_path("deps")));
    bld_fs_mkdir_p(bld_path_join(b->cache, bld_path("tmp")));

    bld_fs_write_file(bld_path_join(b->cache, bld_path(".gitignore")), "*", 1);
    bld_fs_write_file(bld_path_join(b->out, bld_path(".gitignore")), "*", 1);

    /* C compiler */
    const char *cc_env = getenv("CC"), *cc = cc_env ? cc_env : "cc";
    b->compilers[0] = (Bld_Compiler){.lang = BLD_LANG_C, .driver = cc,
                                      .identity_hash = bld_hash_str(cc), .available = 1};
    /* C++ compiler */
    const char *cxx_env = getenv("CXX"), *cxx = cxx_env ? cxx_env : "c++";
    b->compilers[1] = (Bld_Compiler){.lang = BLD_LANG_CXX, .driver = cxx,
                                      .identity_hash = bld_hash_str(cxx),
                                      .available = bld__has_in_path(cxx) || cxx_env != NULL};
    /* ASM assembler (falls back to C compiler) */
    const char *as_env = getenv("AS"), *as_drv = as_env ? as_env : cc;
    b->compilers[2] = (Bld_Compiler){.lang = BLD_LANG_ASM, .driver = as_drv,
                                      .identity_hash = bld_hash_str(as_drv), .available = 1};

    if (bld__has_in_path("llvm-ar")) b->static_link_tool = "llvm-ar";
    else if (bld__has_in_path("ar")) b->static_link_tool = "ar";
    b->global_warnings = 1;
}

/* stage 2: CLI parsing + prefix override */
static void bld__init_cli(Bld* b) {
    bld__parse_args(b);
    if (b->settings.install_prefix) {
        bld_fs_mkdir_p(bld_path(b->settings.install_prefix));
        b->out = bld_fs_realpath(bld_path(b->settings.install_prefix));
    }
}

/* stage 3: built-in targets (install, build) */
static void bld__init_builtin_targets(Bld* b) {
    b->install_target = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, b->install_target, BLD_TGT_CUSTOM, "install", "Install targets");
    b->install_target->exit->silent = 1;
    b->build_all_target = bld_arena_alloc(sizeof(Bld_Target));
    bld__init_target(b, b->build_all_target, BLD_TGT_CUSTOM, "build", "Build all targets");
    b->build_all_target->exit->silent = 1;
}

/* full init for main() */
static void bld__init(Bld* b, int argc, char** argv) {
    bld__init_core(b, argc, argv);
    bld__init_cli(b);
    bld__init_builtin_targets(b);
}

/* ---- compile_commands.json ---- */

static void bld__escape_json(Bld__Cmd* out, const char* s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { char tmp[3] = {'\\', *s, 0}; bld__cmd_appendf(out, "%s", tmp); }
        else { char tmp[2] = {*s, 0}; bld__cmd_appendf(out, "%s", tmp); }
    }
}

static void bld__write_compdb(Bld* b) {
    Bld__Cmd json = {0};
    bld__cmd_appendf(&json, "[\n");
    int first = 1;

    for (size_t i = 0; i < b->all_targets.count; i++) {
        Bld_Target* t = b->all_targets.items[i];
        if (t->kind != BLD_TGT_EXE && t->kind != BLD_TGT_LIB) continue;

        Bld_StepList* obj_steps = (t->kind == BLD_TGT_EXE)
            ? &((Bld_Exe*)t)->obj_steps
            : &((Bld_Lib*)t)->obj_steps;

        for (size_t j = 0; j < obj_steps->count; j++) {
            Bld_Step* step = obj_steps->items[j];
            if (!step->action_ctx) continue;
            Bld__ObjCtx* ctx = (Bld__ObjCtx*)step->action_ctx;

            Bld__Cmd cmd = {0};
            bld__render_obj_cmd(&cmd, ctx);

            if (!first) bld__cmd_appendf(&json, ",\n");
            first = 0;

            bld__cmd_appendf(&json, "  { \"directory\": \"");
            bld__escape_json(&json, b->root.s);
            bld__cmd_appendf(&json, "\", \"file\": \"");
            bld__escape_json(&json, ctx->source.s);
            bld__cmd_appendf(&json, "\", \"command\": \"");
            bld__escape_json(&json, cmd.items);
            bld__cmd_appendf(&json, "\" }");
        }
    }

    bld__cmd_appendf(&json, "\n]\n");
    Bld_Path out = bld_path_join(b->root, bld_path("compile_commands.json"));
    bld_fs_write_file(out, json.items, json.count);
}

/* ---- main ---- */

int main(int argc, char** argv) {
    Bld b;
    bld__init(&b, argc, argv);

    /* clean tmp once per process */
    Bld_Path tmp = bld_path_join(b.cache, bld_path("tmp"));
    bld_fs_remove_all(tmp);
    bld_fs_mkdir_p(tmp);

    bld__recompile_if_needed(&b);
    if (bld__handle_clean(&b)) return 0;
    configure(&b);

    /* validate: error on unknown -D options */
    for (size_t i = 0; i < b.user_options.count; i++) {
        if (!b.user_options.items[i].used)
            bld_panic("unknown option: -D%s\n", b.user_options.items[i].key);
    }

    bld__materialize_lazy_sources(&b);
    bld__resolve_link_deps(&b);
    bld__write_compdb(&b);
    if (b.settings.show_help) { bld__show_help(&b); return 0; }
    if (bld__handle_test(&b)) return b.steps_failed > 0 ? 1 : 0;
    bld__run_build(&b);
    return 0;
}

