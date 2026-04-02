#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"
BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    set_compiler_c(b, .standard = C_11);
    set_compiler_cxx(b, .standard = CXX_17);

    CompileFlags base = default_compile_flags(b);
    base.include_dirs = BLD_PATHS("include");
    LinkFlags lf = default_link_flags(b);

    /* ---- shared_base (C++) ---- */
    Target* sbase = add_lib(b, .name = "sbase",
        .sources = files_glob("sbase/*.cpp"),
        .compile = base, .link = lf);

    /* ---- static libs ---- */
    Target* core = add_lib(b, .name = "core",
        .sources = files_glob("lcore/*.c"),
        .compile = base, .link = lf);
    link_with(core, sbase);

    Target* math = add_lib(b, .name = "math",
        .sources = files_glob("lmath/*.c"),
        .compile = base, .link = lf);
    link_with(math, core);

    Target* text = add_lib(b, .name = "text",
        .sources = files_glob("ltext/*.c"),
        .compile = base, .link = lf);
    link_with(text, core);

    Target* io = add_lib(b, .name = "io",
        .sources = files_glob("lio/*.c"),
        .compile = base, .link = lf);
    link_with(io, core);

    Target* util = add_lib(b, .name = "util",
        .sources = files_glob("lutil/*.c"),
        .compile = base, .link = lf);
    link_with(util, sbase);

    Target* fmt = add_lib(b, .name = "fmt",
        .sources = files_glob("lfmt/*.c"),
        .compile = base, .link = lf);
    link_with(fmt, util);
    link_with(fmt, text);

    Target* net = add_lib(b, .name = "net",
        .sources = files_glob("lnet/*.c"),
        .compile = base, .link = lf);
    link_with(net, sbase);

    /* ---- executables ---- */
    Target* app_main = add_exe(b, .name = "app_main",
        .sources = files_glob("amain/*.c"),
        .compile = base, .link = lf);
    link_with(app_main, math);
    link_with(app_main, text);
    link_with(app_main, fmt);

    Target* app_tool = add_exe(b, .name = "app_tool",
        .sources = files_glob("atool/*.c"),
        .compile = base, .link = lf);
    link_with(app_tool, core);
    link_with(app_tool, util);

    Target* app_serv = add_exe(b, .name = "app_serv",
        .sources = files_glob("aserv/*.c"),
        .compile = base, .link = lf);
    link_with(app_serv, net);
    link_with(app_serv, io);
    link_with(app_serv, core);

    Target* app_cli = add_exe(b, .name = "app_cli",
        .sources = files_glob("acli/*.c"),
        .compile = base, .link = lf);
    link_with(app_cli, math);
    link_with(app_cli, fmt);
    link_with(app_cli, net);

    /* ---- plugins (shared libs) ---- */
    Target* pluga = add_lib(b, .name = "pluga",
        .sources = files_glob("pluga/*.c"),
        .compile = base, .link = lf);
    link_with(pluga, core);

    Target* plugb = add_lib(b, .name = "plugb",
        .sources = files_glob("plugb/*.c"),
        .compile = base, .link = lf);
    link_with(plugb, util);
    link_with(plugb, text);

    Target* plugc = add_lib(b, .name = "plugc",
        .sources = files_glob("plugc/*.c"),
        .compile = base, .link = lf);
    link_with(plugc, math);

    /* ---- test exes ---- */
    Target* tcore = add_exe(b, .name = "test_core",
        .sources = files_glob("tcore/*.c"),
        .compile = base, .link = lf);
    link_with(tcore, core);

    Target* tmath = add_exe(b, .name = "test_math",
        .sources = files_glob("tmath/*.c"),
        .compile = base, .link = lf);
    link_with(tmath, math);

    Target* tnet = add_exe(b, .name = "test_net",
        .sources = files_glob("tnet/*.c"),
        .compile = base, .link = lf);
    link_with(tnet, net);
    link_with(tnet, sbase);

    /* ---- install all ---- */
    add_install_exe(b, app_main);
    add_install_exe(b, app_tool);
    add_install_exe(b, app_serv);
    add_install_exe(b, app_cli);
    add_install_exe(b, tcore);
    add_install_exe(b, tmath);
    add_install_exe(b, tnet);
    add_install_lib(b, sbase);
    add_install_lib(b, core);
    add_install_lib(b, math);
    add_install_lib(b, text);
    add_install_lib(b, io);
    add_install_lib(b, util);
    add_install_lib(b, fmt);
    add_install_lib(b, net);
    add_install_lib(b, pluga);
    add_install_lib(b, plugb);
    add_install_lib(b, plugc);
}
