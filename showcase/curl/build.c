/*
 * build.c -- bld.h build script for curl
 *
 * Builds libcurl (static library) and the curl CLI executable.
 * Replicates the essential parts of curl's CMake build:
 *   - lib/ sources  -> libcurl.a  (with subdirs: vtls, vauth, vquic, vssh)
 *   - src/ sources  -> curl       (with curlx helper files from lib)
 *
 * Pre-generated headers in generated/ provide curl_config.h and curl_checks.h.
 *
 * Usage:
 *   cc -std=c11 -w build.c -o b -lpthread
 *   ./b build
 *   ./b build -Dssl=off -Dzlib=off
 *   ./b build --release
 *   ./build/bin/curl --version
 */

#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "../../bld.h"

BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

void configure(Bld* b) {
    set_compiler_c(b, .standard = BLD_C_GNU11);

    /* ------------------------------------------------------------------ */
    /* User options                                                        */
    /* ------------------------------------------------------------------ */
    bool use_ssl  = option_bool(b, "ssl",  "Enable OpenSSL support", true);
    bool use_zlib = option_bool(b, "zlib", "Enable zlib compression", true);

    /* ------------------------------------------------------------------ */
    /* External dependencies (pkg-config)                                  */
    /* ------------------------------------------------------------------ */
    Bld_Dep* dep_ssl  = NULL;
    Bld_Dep* dep_zlib = NULL;

    if (use_ssl) {
        dep_ssl = find_pkg("openssl");
        if (!dep_ssl->found)
            bld_log_info("[!] openssl not found via pkg-config -- building without SSL\n");
    }
    if (use_zlib) {
        dep_zlib = find_pkg("zlib");
        if (!dep_zlib->found)
            bld_log_info("[!] zlib not found via pkg-config -- building without compression\n");
    }

    /* ------------------------------------------------------------------ */
    /* Feature detection                                                   */
    /* ------------------------------------------------------------------ */
    Bld_Checks* chk = checks_new(b);

    /* Headers */
    checks_header(chk, "HAVE_SYS_SOCKET_H",  "sys/socket.h");
    checks_header(chk, "HAVE_NETINET_IN_H",   "netinet/in.h");
    checks_header(chk, "HAVE_ARPA_INET_H",    "arpa/inet.h");
    checks_header(chk, "HAVE_NETDB_H",        "netdb.h");
    checks_header(chk, "HAVE_UNISTD_H",       "unistd.h");
    checks_header(chk, "HAVE_POLL_H",         "poll.h");
    checks_header(chk, "HAVE_FCNTL_H",        "fcntl.h");
    checks_header(chk, "HAVE_PTHREAD_H",      "pthread.h");
    checks_header(chk, "HAVE_STDBOOL_H",      "stdbool.h");
    checks_header(chk, "HAVE_STRINGS_H",      "strings.h");
    checks_header(chk, "HAVE_SYS_STAT_H",     "sys/stat.h");
    checks_header(chk, "HAVE_SYS_TIME_H",     "sys/time.h");

    /* Functions */
    checks_func(chk, "HAVE_GETADDRINFO",  "getaddrinfo",  "netdb.h");
    checks_func(chk, "HAVE_GETTIMEOFDAY", "gettimeofday", "sys/time.h");
    checks_func(chk, "HAVE_SOCKET",       "socket",       "sys/socket.h");
    checks_func(chk, "HAVE_POLL",         "poll",         "poll.h");
    checks_func(chk, "HAVE_SELECT",       "select",       "sys/select.h");
    checks_func(chk, "HAVE_SIGACTION",    "sigaction",    "signal.h");
    checks_func(chk, "HAVE_STRCASECMP",   "strcasecmp",   "strings.h");

    /* Sizes */
    checks_sizeof(chk, "SIZEOF_LONG",       "long");
    checks_sizeof(chk, "SIZEOF_SIZE_T",     "size_t");
    checks_sizeof(chk, "SIZEOF_CURL_OFF_T", "long long");

    checks_run(chk);

    /* ------------------------------------------------------------------ */
    /* Common compile flags                                                */
    /* ------------------------------------------------------------------ */
    Bld_Strs lib_defines = {0};
    strs_push(&lib_defines, "HAVE_CONFIG_H");
    strs_push(&lib_defines, "BUILDING_LIBCURL");
    strs_push(&lib_defines, "CURL_STATICLIB");
    strs_push(&lib_defines, "_GNU_SOURCE");
    /* Disable features we do not have headers for */
    strs_push(&lib_defines, "CURL_DISABLE_LDAP");
    strs_push(&lib_defines, "CURL_DISABLE_LDAPS");

    if (dep_ssl && dep_ssl->found)
        strs_push(&lib_defines, "USE_OPENSSL");
    if (dep_zlib && dep_zlib->found)
        strs_push(&lib_defines, "HAVE_LIBZ");

    Bld_CompileFlags lib_cflags = default_compile_flags(b);
    lib_cflags.defines      = lib_defines;
    lib_cflags.include_dirs = BLD_PATHS("include", "lib", "generated");

    /* ------------------------------------------------------------------ */
    /* libcurl -- static library                                           */
    /* ------------------------------------------------------------------ */

    /* Gather all lib/ .c files recursively (includes vtls, vauth, vquic, vssh) */
    Bld_Paths lib_srcs = files_glob("lib/**/*.c");

    /* Exclude files that require missing platform headers or are Windows-only */
    lib_srcs = files_exclude(lib_srcs, BLD_PATHS(
        "lib/ldap.c",           /* needs LDAP headers */
        "lib/openldap.c",       /* needs LDAP headers */
        "lib/dllmain.c"         /* Windows DLL entry point */
    ));

    /* System libraries for Linux */
    Bld_Strs sys_libs = {0};
    strs_push(&sys_libs, "pthread");
    if (b->toolchain->os == BLD_OS_LINUX) {
        strs_push(&sys_libs, "dl");
        strs_push(&sys_libs, "rt");
    }

    Bld_Target* libcurl = add_lib(b,
        .name              = "curl",
        .desc              = "libcurl -- the multiprotocol file transfer library",
        .lib_basename      = "curl",
        .sources           = lib_srcs,
        .compile           = lib_cflags,
        .compile_propagate = {
            .include_dirs = BLD_PATHS("include"),
            .defines      = BLD_STRS("CURL_STATICLIB"),
        },
        .link_propagate    = {
            .libs = sys_libs,
        });

    /* Apply external deps to the library */
    if (dep_ssl  && dep_ssl->found)  use_dep(libcurl, dep_ssl);
    if (dep_zlib && dep_zlib->found) use_dep(libcurl, dep_zlib);

    /* ------------------------------------------------------------------ */
    /* curl CLI executable                                                 */
    /* ------------------------------------------------------------------ */

    /* src/*.c -- the curl tool sources */
    Bld_Paths exe_srcs = files_glob("src/*.c");

    /* Exclude Windows resource file */
    exe_srcs = files_exclude(exe_srcs, BLD_PATHS("src/curl.rc"));

    /*
     * curlx helper files: these are lib/ sources reused by the curl CLI.
     * They are compiled into the exe WITHOUT BUILDING_LIBCURL, so they get
     * the tool's compile flags instead.
     * See src/Makefile.inc CURLX_CFILES for the canonical list.
     */
    Bld_Paths curlx_srcs = BLD_PATHS(
        "lib/base64.c",
        "lib/curl_get_line.c",
        "lib/curl_multibyte.c",
        "lib/dynbuf.c",
        "lib/nonblock.c",
        "lib/strtoofft.c",
        "lib/timediff.c",
        "lib/version_win32.c",
        "lib/warnless.c"
    );

    Bld_Paths all_exe_srcs = files_merge(exe_srcs, curlx_srcs);

    Bld_Strs exe_defines = {0};
    strs_push(&exe_defines, "HAVE_CONFIG_H");
    strs_push(&exe_defines, "CURL_STATICLIB");
    strs_push(&exe_defines, "_GNU_SOURCE");

    if (dep_ssl && dep_ssl->found)
        strs_push(&exe_defines, "USE_OPENSSL");
    if (dep_zlib && dep_zlib->found)
        strs_push(&exe_defines, "HAVE_LIBZ");

    Bld_CompileFlags exe_cflags = default_compile_flags(b);
    exe_cflags.defines      = exe_defines;
    exe_cflags.include_dirs = BLD_PATHS("include", "lib", "generated", "src");

    Bld_Target* curl_exe = add_exe(b,
        .name    = "curl",
        .desc    = "curl -- command line tool for transferring data with URLs",
        .sources = all_exe_srcs,
        .compile = exe_cflags);

    /* Link against libcurl (propagates include dirs and system libs) */
    link_with(curl_exe, libcurl);

    /* ------------------------------------------------------------------ */
    /* Installation                                                        */
    /* ------------------------------------------------------------------ */
    install_exe(b, curl_exe);
    install_lib(b, libcurl);
    install_dir(b, "include/curl", bld_filepath("include/curl"));
}
