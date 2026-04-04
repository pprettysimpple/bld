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
#include "bld.h"

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

    Bld_Paths srcs = files_glob("src/**/*.c");

    srcs = files_exclude(srcs, BLD_PATHS(
        /* --- Windows backend (entire directory) --- */
        "src/win",

        /* --- Other Unix platform backends ---
         * Each of these files implements OS-specific functionality for a
         * non-Linux platform. On Linux, the equivalent code lives in
         * linux.c, procfs-exepath.c, proctitle.c, etc.
         */
        "*aix*",               /* AIX */
        "*darwin*",            /* macOS / Darwin */
        "*freebsd*",           /* FreeBSD */
        "*netbsd*",            /* NetBSD */
        "*openbsd*",           /* OpenBSD */
        "*sunos*",             /* Solaris / illumos */
        "*haiku*",             /* Haiku OS */
        "*hurd*",              /* GNU Hurd */
        "*cygwin*",            /* Cygwin (Windows POSIX layer) */
        "*qnx*",               /* QNX RTOS */
        "*ibmi*",              /* IBM i (AS/400) */
        "*os390*",             /* z/OS (IBM mainframe) */

        /* --- BSD-specific subsystems ---
         * kqueue.c: BSD/macOS event loop backend (Linux uses epoll)
         * bsd-ifaddrs.c: BSD getifaddrs implementation
         * bsd-proctitle.c: BSD setproctitle implementation
         */
        "*kqueue*",
        "*bsd-ifaddrs*",
        "*bsd-proctitle*",

        /* --- macOS FSEvents ---
         * fsevents.c: macOS FSEvents file watcher
         * no-fsevents.c: stub for non-macOS platforms that still use kqueue
         *   (Linux uses inotify via linux.c, not this stub)
         */
        "*fsevents*",
        "*no-fsevents*",

        /* --- Stubs for platforms lacking proctitle ---
         * no-proctitle.c: stub for platforms without setproctitle
         *   (Linux has its own implementation in proctitle.c)
         */
        "*no-proctitle*",

        /* --- Portable fallbacks not used on Linux ---
         * posix-poll.c: portable poll() fallback (Linux uses epoll)
         * posix-hrtime.c: portable clock_gettime fallback
         *   (Linux has optimized implementation in linux.c)
         */
        "*posix-poll*",
        "*posix-hrtime*",

        /* --- Random number fallbacks ---
         * random-getentropy.c: BSD/macOS getentropy() call
         *   (Linux uses getrandom() via random-getrandom.c, with
         *    /dev/urandom fallback via random-devurandom.c, and the
         *    sysctl fallback via random-sysctl-linux.c)
         * NOTE: random-devurandom.c IS included — random.c calls it
         *   as a fallback when getrandom() returns ENOSYS on older kernels.
         */
        "*random-getentropy*"
    ));

    /* ------------------------------------------------------------------ */
    /*  Compile flags                                                      */
    /* ------------------------------------------------------------------ */

    Bld_CompileFlags cflags = default_compile_flags(b);

    cflags.defines = BLD_STRS(
        /* Required for full POSIX + GNU API surface.
         * libuv uses pipe2(), accept4(), epoll_create1(), etc. */
        "_GNU_SOURCE",

        /* POSIX.1-2001 baseline — ensures sigaction, pselect, etc. */
        "_POSIX_C_SOURCE=200112",

        /* Large file support — required for stat64/off_t on 32-bit */
        "_FILE_OFFSET_BITS=64"
    );

    /* Private include dirs — needed to compile libuv sources */
    cflags.include_dirs = BLD_PATHS(
        "include",     /* public headers (uv.h, uv/*.h) */
        "src",         /* private headers (uv-common.h, etc.) */
        "src/unix"     /* private unix headers (internal.h, etc.) */
    );

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
        .compile_propagate = {
            .include_dirs = BLD_PATHS("include")
        },

        /* Public link dependencies: libuv consumers must link these.
         * - pthread: threading, mutexes, conditions
         * - dl: dlopen/dlsym for shared library loading
         * - rt: clock_gettime, timer_create (older glibc) */
        .link_propagate = {
            .libs = BLD_STRS("pthread", "dl", "rt")
        }
    );

    /* ------------------------------------------------------------------ */
    /*  Installation                                                       */
    /* ------------------------------------------------------------------ */

    install_lib(b, uv);
    install_files(b, BLD_PATHS("include/uv.h"), bld_filepath("include"));
    install_dir(b, "include/uv", bld_filepath("include/uv"));

    /* ------------------------------------------------------------------ */
    /*  Test runner                                                        */
    /*                                                                     */
    /*  libuv's test suite is a single executable that includes all test   */
    /*  files. The runner framework (runner.c, runner-unix.c) provides     */
    /*  process isolation and timeout handling.                             */
    /* ------------------------------------------------------------------ */

    bool build_tests = option_bool(b, "tests", "Build and register test runner", true);

    if (build_tests) {
        /* Glob all test sources, exclude Windows runner and benchmarks */
        Bld_Paths test_srcs = files_glob("test/*.c");
        test_srcs = files_exclude(test_srcs, BLD_PATHS(
            "test/runner-win.c",       /* Windows test runner */
            "test/run-benchmarks.c",   /* Separate benchmark binary */
            "*benchmark-*"             /* All benchmark source files */
        ));

        Bld_CompileFlags test_cflags = default_compile_flags(b);
        test_cflags.defines = BLD_STRS(
            "_GNU_SOURCE",
            "_POSIX_C_SOURCE=200112",
            "_FILE_OFFSET_BITS=64"
        );
        test_cflags.include_dirs = BLD_PATHS(
            "include",
            "src",       /* tests include private libuv headers */
            "src/unix",
            "test"       /* test infrastructure headers (task.h, runner.h) */
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
