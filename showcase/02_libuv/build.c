/*
 * build.c — bld.h build script for libuv 1.50.0 (Linux)
 *
 * Builds libuv as a static library with all Linux-specific sources.
 * Optionally builds the test runner executable.
 *
 * Bootstrap:  cc -std=c11 -w build.c -o b -lpthread
 * Build:      ./b build
 * Test:       ./b test
 * Verbose:    ./b build -v
 * Install:    ./b build --prefix /usr/local
 */

#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "../../bld.h"

BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    set_compiler_c(b, .standard = BLD_C_GNU11);

    /* ------------------------------------------------------------------ */
    /*  Feature detection                                                  */
    /* ------------------------------------------------------------------ */

    Bld_Checks* chk = checks_new(b);
    checks_header(chk, "HAVE_PTHREAD_H",   "pthread.h");
    checks_header(chk, "HAVE_SYS_EPOLL_H", "sys/epoll.h");
    checks_run(chk);

    /* ------------------------------------------------------------------ */
    /*  Source file selection                                               */
    /*                                                                     */
    /*  Strategy: glob all .c files under src/ recursively, then exclude   */
    /*  everything that does not belong to a Linux build. This is robust   */
    /*  against new files being added to libuv — any new common or Linux   */
    /*  file is picked up automatically; only non-Linux platform files     */
    /*  need explicit exclusion.                                           */
    /* ------------------------------------------------------------------ */

    Bld_Paths srcs = files_glob("prj/src/**/*.c");

    if (b->toolchain->os == BLD_OS_WINDOWS) {
        /* Windows: use win/ backend, exclude unix/ */
        srcs = files_exclude(srcs, BLD_PATHS("prj/src/unix"));
    } else {
        /* Unix: use unix/ backend, exclude win/ and non-Linux platforms */
        srcs = files_exclude(srcs, BLD_PATHS(
            "prj/src/win",
            "*aix*", "*darwin*", "*freebsd*", "*netbsd*", "*openbsd*",
            "*sunos*", "*haiku*", "*hurd*", "*cygwin*", "*qnx*",
            "*ibmi*", "*os390*",
            "*kqueue*", "*bsd-ifaddrs*", "*bsd-proctitle*",
            "*fsevents*", "*no-fsevents*", "*no-proctitle*",
            "*posix-poll*", "*posix-hrtime*",
            "*random-getentropy*"
        ));
    }

    /* ------------------------------------------------------------------ */
    /*  Compile flags                                                      */
    /* ------------------------------------------------------------------ */

    Bld_CompileFlags cflags = default_compile_flags(b);

    if (b->toolchain->os == BLD_OS_WINDOWS) {
        cflags.defines = BLD_STRS("WIN32_LEAN_AND_MEAN", "_WIN32_WINNT=0x0A00");
        cflags.include_dirs = BLD_PATHS("prj/include", "prj/src");
    } else {
        cflags.defines = BLD_STRS("_GNU_SOURCE", "_POSIX_C_SOURCE=200112",
                                  "_FILE_OFFSET_BITS=64");
        cflags.include_dirs = BLD_PATHS("prj/include", "prj/src", "prj/src/unix");
    }

    /* ------------------------------------------------------------------ */
    /*  Static library target                                              */
    /* ------------------------------------------------------------------ */

    Bld_Target* uv = add_lib(b,
        .name              = "uv",
        .desc              = "libuv — cross-platform async I/O (static)",
        .lib_basename      = "uv",
        .sources           = srcs,
        .compile           = cflags,

        /* Public include path: consumers that link_with(target, uv)
         * automatically get -Iinclude for uv.h */
        .compile_pub = {
            .include_dirs = BLD_PATHS("prj/include")
        },

        /* Public link dependencies: libuv consumers must link these.
         * - pthread: threading, mutexes, conditions
         * - dl: dlopen/dlsym for shared library loading
         * - rt: clock_gettime, timer_create (older glibc) */
        .link_pub = {
            .libs = (b->toolchain->os == BLD_OS_WINDOWS)
                ? BLD_STRS("ws2_32", "iphlpapi", "userenv", "psapi", "dbghelp", "ole32", "shell32")
                : BLD_STRS("pthread", "dl", "rt")
        }
    );

    /* ------------------------------------------------------------------ */
    /*  Installation                                                       */
    /* ------------------------------------------------------------------ */

    install_lib(b, uv);
    install_files(b, BLD_PATHS("prj/include/uv.h"), bld_filepath("include"));
    install_dir(b, "prj/include/uv", bld_filepath("include/uv"));

    /* ------------------------------------------------------------------ */
    /*  Test runner                                                        */
    /*                                                                     */
    /*  libuv's test suite is a single executable that includes all test   */
    /*  files. The runner framework (runner.c, runner-unix.c) provides     */
    /*  process isolation and timeout handling.                             */
    /* ------------------------------------------------------------------ */

    bool build_tests = option_bool(b, "tests", "Build and register test runner",
        b->toolchain->os != BLD_OS_WINDOWS);  /* tests use runner-unix.c, skip on Windows */

    if (build_tests) {
        /* Glob all test sources, exclude Windows runner and benchmarks */
        Bld_Paths test_srcs = files_glob("prj/test/*.c");
        test_srcs = files_exclude(test_srcs, BLD_PATHS(
            "prj/test/runner-win.c",       /* Windows test runner */
            "prj/test/run-benchmarks.c",   /* Separate benchmark binary */
            "*benchmark-*"             /* All benchmark source files */
        ));

        Bld_CompileFlags test_cflags = default_compile_flags(b);
        test_cflags.defines = BLD_STRS(
            "_GNU_SOURCE",
            "_POSIX_C_SOURCE=200112",
            "_FILE_OFFSET_BITS=64"
        );
        test_cflags.include_dirs = BLD_PATHS(
            "prj/include",
            "prj/src",       /* tests include private libuv headers */
            "prj/src/unix",
            "prj/test"       /* test infrastructure headers (task.h, runner.h) */
        );

        Bld_Target* test_exe = add_exe(b,
            .name    = "uv-test-runner",
            .desc    = "libuv test suite runner",
            .sources = test_srcs,
            .compile = test_cflags,
            .link    = { .libs = BLD_STRS("util") }
        );
        link_with(test_exe, uv);
        install_exe(b, test_exe);

        /* Register as bld test. Note: 2 multicast tests (udp_multicast_join,
         * udp_multicast_join6) may timeout without multicast-capable networking.
         * This is a known libuv CI issue, not a build problem. */
        add_test(b, test_exe,
            .name = "libuv-tests",
            .desc = "Run libuv test suite");
    }
}
