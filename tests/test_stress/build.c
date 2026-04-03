#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"
BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    bld_set_compiler_c(b, .standard = BLD_C_11);

    // libbase: a.c b.c c.c
    Target* base = add_lib(b, .name = "base",
        .sources = BLD_PATHS("s/a.c", "s/b.c", "s/c.c"));

    // libmid1: d.c e.c -> base
    Target* mid1 = add_lib(b, .name = "mid1",
        .sources = BLD_PATHS("s/d.c", "s/e.c"));
    link_with(mid1, base);

    // libdeep: f.c g.c -> mid1
    Target* deep = add_lib(b, .name = "deep",
        .sources = BLD_PATHS("s/f.c", "s/g.c"));
    link_with(deep, mid1);

    // libmid2: h.c i.c j.c -> base
    Target* mid2 = add_lib(b, .name = "mid2",
        .sources = BLD_PATHS("s/h.c", "s/i.c", "s/j.c"));
    link_with(mid2, base);

    // libside: k.c l.c -> base
    Target* side = add_lib(b, .name = "side",
        .sources = BLD_PATHS("s/k.c", "s/l.c"));
    link_with(side, base);

    // app1: m.c n.c o.c -> deep + mid2
    Target* app1 = add_exe(b, .name = "app1",
        .sources = BLD_PATHS("s/m.c", "s/n.c", "s/o.c"));
    link_with(app1, deep);
    link_with(app1, mid2);

    // app2: p.c q.c -> mid2 + side
    Target* app2 = add_exe(b, .name = "app2",
        .sources = BLD_PATHS("s/p.c", "s/q.c"));
    link_with(app2, mid2);
    link_with(app2, side);

    // app3: r.c s.c t.c -> side + mid1
    Target* app3 = add_exe(b, .name = "app3",
        .sources = BLD_PATHS("s/r.c", "s/ss.c", "s/t.c"));
    link_with(app3, side);
    link_with(app3, mid1);

    install_exe(b, app1);
    install_exe(b, app2);
    install_exe(b, app3);
    install_lib(b, base);
    install_lib(b, mid1);
    install_lib(b, deep);
    install_lib(b, mid2);
    install_lib(b, side);
}
