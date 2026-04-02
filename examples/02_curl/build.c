/* build.c — curl 8.12.0 build matching the official CMake/autotools build
 *
 * Bootstrap: cc -std=c11 -w build.c -o b -lpthread
 * Usage:
 *   ./b build                           — build with auto-detected deps
 *   ./b build -Dssl=none                — build without TLS
 *   ./b build -Dssl=openssl             — require OpenSSL (fail if missing)
 *   ./b build -Dzlib=off                — disable zlib
 *   ./b build -Dnghttp2=off             — disable HTTP/2
 *   ./b build -Dssh2=on                 — enable libssh2
 *   ./b build -Dbrotli=on               — enable Brotli
 *   ./b build -Dzstd=on                 — enable Zstandard
 *   ./b build -Dcares=on                — enable c-ares resolver
 *   ./b build -Dhttp-only=on            — HTTP(S) only
 *   ./b build -Dipv6=off                — disable IPv6
 *   ./b install --prefix /usr/local     — install curl + libcurl.a + headers
 *   ./b test                            — run curl --version
 */
#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"

BLD_RECOMPILE_CMD("cc -std=c11 -I../../ -w build.c -lpthread")

/* ---- Install headers custom step ---- */

static Bld_ActionResult install_headers_action(void* ctx, Bld_Path output, Bld_Path depfile) {
    (void)ctx;
    (void)depfile;
    bld_fs_mkdir_p(output);
    Bld_Path src = bld_path("include/curl");
    Bld_Path dst = bld_path_join(output, bld_path("curl"));
    bld_fs_mkdir_p(dst);
    Bld_PathList files = bld_fs_list_files_r(src);
    for (size_t i = 0; i < files.count; i++) {
        const char* fname = bld_path_filename(files.items[i]);
        const char* ext = bld_path_ext(files.items[i]);
        if (ext && strcmp(ext, ".h") == 0) {
            bld_fs_copy_file(files.items[i], bld_path_join(dst, bld_path(fname)));
        }
    }
    return BLD_ACTION_OK;
}

/* ---- Feature detection & config header generation ---- */

static void generate_config(Bld* b,
                             bool has_ssl, bool has_zlib, bool has_nghttp2,
                             bool has_ssh2, bool has_idn2, bool has_psl,
                             bool has_brotli, bool has_zstd, bool has_cares,
                             bool opt_ipv6,
                             bool no_ws, bool no_proxy, bool no_cookies,
                             bool no_auth, bool no_doh, bool no_mime,
                             bool no_netrc, bool no_file)
{
    Bld_Checks* c = bld_checks_new(b);

    /* -- Header checks (~35) -- */
    bld_checks_header(c, "HAVE_ARPA_INET_H",    "arpa/inet.h");
    bld_checks_header(c, "HAVE_DIRENT_H",        "dirent.h");
    bld_checks_header(c, "HAVE_FCNTL_H",         "fcntl.h");
    bld_checks_header(c, "HAVE_IFADDRS_H",       "ifaddrs.h");
    bld_checks_header(c, "HAVE_LIBGEN_H",        "libgen.h");
    bld_checks_header(c, "HAVE_LINUX_TCP_H",     "linux/tcp.h");
    bld_checks_header(c, "HAVE_LOCALE_H",        "locale.h");
    bld_checks_header(c, "HAVE_NETDB_H",         "netdb.h");
    bld_checks_header(c, "HAVE_NETINET_IN_H",    "netinet/in.h");
    bld_checks_header(c, "HAVE_NETINET_TCP_H",   "netinet/tcp.h");
    bld_checks_header(c, "HAVE_NETINET_UDP_H",   "netinet/udp.h");
    bld_checks_header(c, "HAVE_NET_IF_H",        "net/if.h");
    bld_checks_header(c, "HAVE_POLL_H",          "poll.h");
    bld_checks_header(c, "HAVE_PWD_H",           "pwd.h");
    bld_checks_header(c, "HAVE_STDATOMIC_H",     "stdatomic.h");
    bld_checks_header(c, "HAVE_STDBOOL_H",       "stdbool.h");
    bld_checks_header(c, "HAVE_STRINGS_H",       "strings.h");
    bld_checks_header(c, "HAVE_SYS_EVENTFD_H",   "sys/eventfd.h");
    bld_checks_header(c, "HAVE_SYS_FILIO_H",     "sys/filio.h");
    bld_checks_header(c, "HAVE_SYS_IOCTL_H",     "sys/ioctl.h");
    bld_checks_header(c, "HAVE_SYS_PARAM_H",     "sys/param.h");
    bld_checks_header(c, "HAVE_SYS_POLL_H",      "sys/poll.h");
    bld_checks_header(c, "HAVE_SYS_RESOURCE_H",  "sys/resource.h");
    bld_checks_header(c, "HAVE_SYS_SELECT_H",    "sys/select.h");
    bld_checks_header(c, "HAVE_SYS_SOCKET_H",    "sys/socket.h");
    bld_checks_header(c, "HAVE_SYS_SOCKIO_H",    "sys/sockio.h");
    bld_checks_header(c, "HAVE_SYS_TYPES_H",     "sys/types.h");
    bld_checks_header(c, "HAVE_SYS_UN_H",        "sys/un.h");
    bld_checks_header(c, "HAVE_SYS_UTIME_H",     "sys/utime.h");
    bld_checks_header(c, "HAVE_TERMIO_H",        "termio.h");
    bld_checks_header(c, "HAVE_TERMIOS_H",       "termios.h");
    bld_checks_header(c, "HAVE_SYS_STAT_H",     "sys/stat.h");
    bld_checks_header(c, "HAVE_SYS_TIME_H",     "sys/time.h");
    bld_checks_header(c, "HAVE_UNISTD_H",        "unistd.h");
    bld_checks_header(c, "HAVE_UTIME_H",         "utime.h");

    /* -- Function checks (~40) -- */
    bld_checks_func(c, "HAVE_ALARM",          "alarm",          "unistd.h");
    bld_checks_func(c, "HAVE_BASENAME",       "basename",       "libgen.h");
    bld_checks_func(c, "HAVE_FCNTL",          "fcntl",          "fcntl.h");
    bld_checks_func(c, "HAVE_FNMATCH",        "fnmatch",        "fnmatch.h");
    bld_checks_func(c, "HAVE_FREEADDRINFO",   "freeaddrinfo",   "netdb.h");
    bld_checks_func(c, "HAVE_FSEEKO",         "fseeko",         "stdio.h");
    bld_checks_func(c, "HAVE_GETADDRINFO",    "getaddrinfo",    "netdb.h");
    bld_checks_func(c, "HAVE_GETEUID",        "geteuid",        "unistd.h");
    bld_checks_func(c, "HAVE_GETHOSTNAME",    "gethostname",    "unistd.h");
    bld_checks_func(c, "HAVE_GETIFADDRS",     "getifaddrs",     "ifaddrs.h");
    bld_checks_func(c, "HAVE_GETPEERNAME",    "getpeername",    "sys/socket.h");
    bld_checks_func(c, "HAVE_GETPWUID",       "getpwuid",       "pwd.h");
    bld_checks_func(c, "HAVE_GETPWUID_R",     "getpwuid_r",     "pwd.h");
    bld_checks_func(c, "HAVE_GETRLIMIT",      "getrlimit",      "sys/resource.h");
    bld_checks_func(c, "HAVE_GETSOCKNAME",    "getsockname",    "sys/socket.h");
    bld_checks_func(c, "HAVE_GETTIMEOFDAY",   "gettimeofday",   "sys/time.h");
    bld_checks_func(c, "HAVE_GMTIME_R",       "gmtime_r",       "time.h");
    bld_checks_func(c, "HAVE_IF_NAMETOINDEX", "if_nametoindex", "net/if.h");
    bld_checks_func(c, "HAVE_INET_NTOP",      "inet_ntop",      "arpa/inet.h");
    bld_checks_func(c, "HAVE_INET_PTON",      "inet_pton",      "arpa/inet.h");
    bld_checks_func(c, "HAVE_LOCALTIME_R",    "localtime_r",    "time.h");
    bld_checks_func(c, "HAVE_OPENDIR",        "opendir",        "dirent.h");
    bld_checks_func(c, "HAVE_PIPE",           "pipe",           "unistd.h");
    bld_checks_func(c, "HAVE_POLL",           "poll",           "poll.h");
    bld_checks_func(c, "HAVE_REALPATH",       "realpath",       "stdlib.h");
    bld_checks_func(c, "HAVE_RECV",           "recv",           "sys/socket.h");
    bld_checks_func(c, "HAVE_SCHED_YIELD",    "sched_yield",    "sched.h");
    bld_checks_func(c, "HAVE_SELECT",         "select",         "sys/select.h");
    bld_checks_func(c, "HAVE_SEND",           "send",           "sys/socket.h");
    bld_checks_func(c, "HAVE_SENDMSG",        "sendmsg",        "sys/socket.h");
    bld_checks_func(c, "HAVE_SETLOCALE",      "setlocale",      "locale.h");
    bld_checks_func(c, "HAVE_SETRLIMIT",      "setrlimit",      "sys/resource.h");
    bld_checks_func(c, "HAVE_SIGACTION",      "sigaction",      "signal.h");
    bld_checks_func(c, "HAVE_SIGNAL",         "signal",         "signal.h");
    bld_checks_func(c, "HAVE_SOCKET",         "socket",         "sys/socket.h");
    bld_checks_func(c, "HAVE_SOCKETPAIR",     "socketpair",     "sys/socket.h");
    bld_checks_func(c, "HAVE_STRCASECMP",     "strcasecmp",     "strings.h");
    bld_checks_func(c, "HAVE_STRERROR_R",     "strerror_r",     "string.h");
    bld_checks_func(c, "HAVE_UTIME",          "utime",          "utime.h");
    bld_checks_func(c, "HAVE_UTIMES",         "utimes",         "sys/time.h");

    /* Functions requiring _GNU_SOURCE — use compile checks with #define */
    bld_checks_compile(c, "HAVE_ACCEPT4",
        "#define _GNU_SOURCE\n#include <sys/socket.h>\n"
        "int main(void){(void)accept4;return 0;}");
    bld_checks_compile(c, "HAVE_PIPE2",
        "#define _GNU_SOURCE\n#include <unistd.h>\n"
        "int main(void){(void)pipe2;return 0;}");
    bld_checks_compile(c, "HAVE_SENDMMSG",
        "#define _GNU_SOURCE\n#include <sys/socket.h>\n"
        "int main(void){(void)sendmmsg;return 0;}");
    bld_checks_compile(c, "HAVE_MEMRCHR",
        "#define _GNU_SOURCE\n#include <string.h>\n"
        "int main(void){(void)memrchr;return 0;}");
    bld_checks_compile(c, "HAVE_EVENTFD",
        "#define _GNU_SOURCE\n#include <sys/eventfd.h>\n"
        "int main(void){(void)eventfd;return 0;}");
    bld_checks_compile(c, "HAVE_ARC4RANDOM",
        "#include <stdlib.h>\n"
        "int main(void){(void)arc4random;return 0;}");

    /* -- strerror_r variant detection -- */
    bld_checks_compile(c, "HAVE_GLIBC_STRERROR_R",
        "#include <string.h>\n#include <errno.h>\n"
        "static void check(char c){(void)c;}\n"
        "int main(void){char buf[1024];\n"
        "check(strerror_r(EACCES,buf,sizeof(buf))[0]);return 0;}");
    bld_checks_compile(c, "HAVE_POSIX_STRERROR_R",
        "#include <string.h>\n#include <errno.h>\n"
        "static void check(float f){(void)f;}\n"
        "int main(void){char buf[1024];\n"
        "check(strerror_r(EACCES,buf,sizeof(buf)));return 0;}");

    /* -- gethostbyname_r argument count detection -- */
    bld_checks_compile(c, "HAVE_GETHOSTBYNAME_R_6",
        "#include <sys/types.h>\n#include <netdb.h>\n"
        "int main(void){struct hostent h,*hp;char buf[8192];int err;\n"
        "gethostbyname_r(\"x\",&h,buf,8192,&hp,&err);return 0;}");
    bld_checks_compile(c, "HAVE_GETHOSTBYNAME_R_5",
        "#include <sys/types.h>\n#include <netdb.h>\n"
        "int main(void){struct hostent h;char buf[8192];int err;\n"
        "gethostbyname_r(\"x\",&h,buf,8192,&err);return 0;}");
    bld_checks_compile(c, "HAVE_GETHOSTBYNAME_R_3",
        "#include <sys/types.h>\n#include <netdb.h>\n"
        "int main(void){struct hostent h;struct hostent_data hd;\n"
        "gethostbyname_r(\"x\",&h,&hd);return 0;}");

    /* -- fsetxattr variant detection -- */
    bld_checks_compile(c, "HAVE_FSETXATTR_5",
        "#include <sys/xattr.h>\n"
        "int main(void){return fsetxattr(0,\"a\",\"b\",1,0);}");
    bld_checks_compile(c, "HAVE_FSETXATTR_6",
        "#include <sys/xattr.h>\n"
        "int main(void){return fsetxattr(0,\"a\",\"b\",1,0,0);}");

    /* -- struct member checks -- */
    bld_checks_compile(c, "HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID",
        "#include <sys/types.h>\n#include <netinet/in.h>\n"
        "int main(void){struct sockaddr_in6 s;s.sin6_scope_id=0;return (int)s.sin6_scope_id;}");
    bld_checks_compile(c, "USE_UNIX_SOCKETS",
        "#include <sys/un.h>\n"
        "int main(void){struct sockaddr_un s;s.sun_path[0]='x';return 0;}");

    /* -- additional feature checks -- */
    bld_checks_compile(c, "HAVE_CLOCK_GETTIME_MONOTONIC_RAW",
        "#include <time.h>\n"
        "int main(void){struct timespec ts;return clock_gettime(CLOCK_MONOTONIC_RAW,&ts);}");
    bld_checks_compile(c, "HAVE_GETADDRINFO_THREADSAFE",
        "#define _POSIX_C_SOURCE 200809L\n#include <netdb.h>\n"
        "int main(void){struct addrinfo *r;getaddrinfo(\"x\",\"80\",0,&r);freeaddrinfo(r);return 0;}");
    bld_checks_compile(c, "HAVE_DECL_FSEEKO",
        "#include <stdio.h>\nint main(void){(void)fseeko;return 0;}");
    bld_checks_compile(c, "HAVE_SIGSETJMP",
        "#include <setjmp.h>\n"
        "int main(void){sigjmp_buf b;if(sigsetjmp(b,1))return 1;return 0;}");
    bld_checks_compile(c, "HAVE_SIGINTERRUPT",
        "#include <signal.h>\n"
        "int main(void){siginterrupt(2,1);return 0;}");

    /* -- Sizeof checks -- */
    bld_checks_sizeof(c, "SIZEOF_INT",        "int");
    bld_checks_sizeof(c, "SIZEOF_LONG",       "long");
    bld_checks_sizeof(c, "SIZEOF_OFF_T",      "off_t");
    bld_checks_sizeof(c, "SIZEOF_SIZE_T",     "size_t");
    bld_checks_sizeof(c, "SIZEOF_TIME_T",     "time_t");
    bld_checks_sizeof(c, "SIZEOF_CURL_OFF_T", "long long");

    /* -- Compile checks for types/features -- */
    bld_checks_compile(c, "HAVE_LONGLONG",
        "int main(void){long long x=0;(void)x;return 0;}");
    bld_checks_compile(c, "HAVE_BOOL_T",
        "#include <stdbool.h>\nint main(void){bool x=true;(void)x;return 0;}");
    bld_checks_compile(c, "HAVE_STRUCT_TIMEVAL",
        "#include <sys/time.h>\nint main(void){struct timeval tv;(void)tv;return 0;}");
    bld_checks_compile(c, "HAVE_STRUCT_SOCKADDR_STORAGE",
        "#include <sys/socket.h>\nint main(void){struct sockaddr_storage ss;(void)ss;return 0;}");
    bld_checks_compile(c, "HAVE_CLOCK_GETTIME_MONOTONIC",
        "#include <time.h>\nint main(void){struct timespec ts;return clock_gettime(CLOCK_MONOTONIC,&ts);}");
    bld_checks_compile(c, "HAVE_ATOMIC",
        "#include <stdatomic.h>\nint main(void){_Atomic int x=0;(void)x;return 0;}");
    bld_checks_compile(c, "HAVE_VARIADIC_MACROS_C99",
        "#define F(...) 1\nint main(void){return F(1,2);}");
    bld_checks_compile(c, "HAVE_WRITABLE_ARGV",
        "int main(int c,char**v){v[0][0]='x';(void)c;return 0;}");
    bld_checks_compile(c, "HAVE_SUSECONDS_T",
        "#include <sys/types.h>\nint main(void){suseconds_t x=0;(void)x;return 0;}");
    bld_checks_compile(c, "HAVE_SA_FAMILY_T",
        "#include <sys/socket.h>\nint main(void){sa_family_t x=0;(void)x;return 0;}");

    bld_checks_run(c);

    /* Write detection results to curl_checks.h */
    bld_fs_mkdir_p(bld_path("generated"));
    bld_checks_write(c, "generated/curl_checks.h");

    /* Write curl_config.h — includes checks + manual config + feature flags */
    Bld_Strings cfg = {0};
    #define L(s) bld_da_push(&cfg, (s))
    L("/* Generated by build.c — do not edit */");
    L("#ifndef CURL_CONFIG_H");
    L("#define CURL_CONFIG_H");
    L("");
    L("/* Feature detection results */");
    L("#include \"curl_checks.h\"");
    L("");
    L("/* Platform */");
    L("#define OS \"Linux\"");
    L("#define STDC_HEADERS 1");
    L("#define HAVE_FILE_OFFSET_BITS 1");
    L("#define _FILE_OFFSET_BITS 64");
    L("");
    L("/* gethostbyname_r — detected variant defines HAVE_GETHOSTBYNAME_R */");
    L("#if defined(HAVE_GETHOSTBYNAME_R_6) || defined(HAVE_GETHOSTBYNAME_R_5) || defined(HAVE_GETHOSTBYNAME_R_3)");
    L("#define HAVE_GETHOSTBYNAME_R 1");
    L("#endif");
    L("");
    L("/* fsetxattr — detected variant defines HAVE_FSETXATTR */");
    L("#if defined(HAVE_FSETXATTR_5) || defined(HAVE_FSETXATTR_6)");
    L("#define HAVE_FSETXATTR 1");
    L("#endif");
    L("");
    L("/* recv/send type signatures (POSIX) */");
    L("#define RECV_TYPE_ARG1 int");
    L("#define RECV_TYPE_ARG2 void *");
    L("#define RECV_TYPE_ARG3 size_t");
    L("#define RECV_TYPE_ARG4 int");
    L("#define RECV_TYPE_RETV ssize_t");
    L("#define SEND_TYPE_ARG1 int");
    L("#define SEND_QUAL_ARG2 const");
    L("#define SEND_TYPE_ARG2 void *");
    L("#define SEND_TYPE_ARG3 size_t");
    L("#define SEND_TYPE_ARG4 int");
    L("#define SEND_TYPE_RETV ssize_t");
    L("#define HAVE_MSG_NOSIGNAL 1");
    L("#define HAVE_FCNTL_O_NONBLOCK 1");
    L("#define CURL_OS \"Linux\"");
    L("");
    L("/* Threading (POSIX) */");
    L("#define USE_THREADS_POSIX 1");
    L("#define HAVE_PTHREAD_H 1");
    if (!has_cares)
        L("#define USE_RESOLV_THREADED 1");
    L("");
    L("/* CA bundle */");
    if (bld_fs_is_file(bld_path("/etc/ssl/certs/ca-certificates.crt")))
        L("#define CURL_CA_BUNDLE \"/etc/ssl/certs/ca-certificates.crt\"");
    else if (bld_fs_is_file(bld_path("/etc/pki/tls/certs/ca-bundle.crt")))
        L("#define CURL_CA_BUNDLE \"/etc/pki/tls/certs/ca-bundle.crt\"");
    else if (bld_fs_is_file(bld_path("/etc/ssl/ca-bundle.pem")))
        L("#define CURL_CA_BUNDLE \"/etc/ssl/ca-bundle.pem\"");
    if (bld_fs_is_dir(bld_path("/etc/ssl/certs")))
        L("#define CURL_CA_PATH \"/etc/ssl/certs\"");
    L("");
    L("/* IPv6 */");
    if (opt_ipv6)
        L("#define ENABLE_IPV6 1");
    L("");
    L("/* Optional features (based on -D options) */");
    if (has_ssl)     L("#define USE_OPENSSL 1");
    if (has_zlib)    L("#define HAVE_LIBZ 1");
    if (has_nghttp2) L("#define USE_NGHTTP2 1");
    if (has_ssh2)    L("#define USE_LIBSSH2 1");
    if (has_idn2) {
        L("#define HAVE_LIBIDN2 1");
        L("#define HAVE_IDN2_H 1");
    }
    if (has_psl)     L("#define USE_LIBPSL 1");
    if (has_brotli)  L("#define HAVE_BROTLI 1");
    if (has_zstd)    L("#define HAVE_ZSTD 1");
    if (has_cares) {
        L("#define USE_ARES 1");
        L("#define USE_RESOLV_ARES 1");
    }
    L("");
    L("/* Protocol/feature disable flags */");
    if (no_ws)      L("#define CURL_DISABLE_WEBSOCKETS 1");
    if (no_proxy)   L("#define CURL_DISABLE_PROXY 1");
    if (no_cookies) L("#define CURL_DISABLE_COOKIES 1");
    if (no_auth)    L("#define CURL_DISABLE_HTTP_AUTH 1");
    if (no_doh)     L("#define CURL_DISABLE_DOH 1");
    if (no_mime)    L("#define CURL_DISABLE_MIME 1");
    if (no_netrc)   L("#define CURL_DISABLE_NETRC 1");
    if (no_file)    L("#define CURL_DISABLE_FILE 1");
    L("");
    L("#endif /* CURL_CONFIG_H */");
    #undef L

    const char* content = bld_str_join(&cfg, "\n");
    bld_fs_write_file(bld_path("generated/curl_config.h"), content, strlen(content));
}

/* ---- configure ---- */

void configure(Bld* b) {
    set_compiler_c(b, .standard = C_GNU99);

    /* ---- User options ---- */

    const char* ssl = option_str(b, "ssl",
        "TLS backend: openssl, wolfssl, mbedtls, gnutls, none, auto", "auto");
    bool opt_zlib    = option_bool(b, "zlib",    "zlib compression", true);
    bool opt_nghttp2 = option_bool(b, "nghttp2", "HTTP/2 via nghttp2", true);
    bool opt_ssh2    = option_bool(b, "ssh2",    "SSH via libssh2", false);
    bool opt_idn2    = option_bool(b, "idn2",    "IDN via libidn2", false);
    bool opt_psl     = option_bool(b, "psl",     "PSL via libpsl", false);
    bool opt_brotli  = option_bool(b, "brotli",  "Brotli compression", false);
    bool opt_zstd    = option_bool(b, "zstd",    "Zstandard compression", false);
    bool opt_cares   = option_bool(b, "cares",   "c-ares DNS resolver", false);
    bool opt_ipv6    = option_bool(b, "ipv6",    "IPv6 support", true);

    bool http_only = option_bool(b, "http-only", "HTTP(S) only, disable all other protocols", false);
    bool no_ftp    = option_bool(b, "disable-ftp",    "Disable FTP",    false);
    bool no_ldap   = option_bool(b, "disable-ldap",   "Disable LDAP",   true);
    bool no_ldaps  = option_bool(b, "disable-ldaps",  "Disable LDAPS",  true);
    bool no_rtsp   = option_bool(b, "disable-rtsp",   "Disable RTSP",   false);
    bool no_dict   = option_bool(b, "disable-dict",   "Disable DICT",   false);
    bool no_telnet = option_bool(b, "disable-telnet", "Disable Telnet", false);
    bool no_tftp   = option_bool(b, "disable-tftp",   "Disable TFTP",   false);
    bool no_pop3   = option_bool(b, "disable-pop3",   "Disable POP3",   false);
    bool no_imap   = option_bool(b, "disable-imap",   "Disable IMAP",   false);
    bool no_smtp   = option_bool(b, "disable-smtp",   "Disable SMTP",   false);
    bool no_gopher = option_bool(b, "disable-gopher", "Disable Gopher", false);
    bool no_mqtt   = option_bool(b, "disable-mqtt",   "Disable MQTT",   false);
    bool no_ws     = option_bool(b, "disable-websockets", "Disable WebSocket", false);
    bool no_proxy  = option_bool(b, "disable-proxy",     "Disable proxy", false);
    bool no_cookies = option_bool(b, "disable-cookies",  "Disable cookies", false);
    bool no_auth   = option_bool(b, "disable-http-auth", "Disable HTTP auth", false);
    bool no_doh    = option_bool(b, "disable-doh",       "Disable DNS-over-HTTPS", false);
    bool no_mime   = option_bool(b, "disable-mime",      "Disable MIME", false);
    bool no_netrc  = option_bool(b, "disable-netrc",     "Disable netrc", false);
    bool no_file   = option_bool(b, "disable-file",      "Disable FILE protocol", false);

    /* ---- Find dependencies via pkg-config ---- */

    Bld_Dep* dep_ssl = NULL;
    bool has_ssl = false;
    if (strcmp(ssl, "none") != 0) {
        const char* pkg = NULL;
        if (strcmp(ssl, "openssl") == 0 || strcmp(ssl, "auto") == 0) pkg = "openssl";
        else if (strcmp(ssl, "wolfssl") == 0)  pkg = "wolfssl";
        else if (strcmp(ssl, "mbedtls") == 0)  pkg = "mbedtls";
        else if (strcmp(ssl, "gnutls") == 0)   pkg = "gnutls";
        else bld_panic("Unknown SSL backend: '%s'", ssl);

        dep_ssl = find_pkg(pkg);
        has_ssl = dep_ssl->found;
        if (!has_ssl && strcmp(ssl, "auto") != 0)
            bld_panic("%s not found (required by -Dssl=%s)", pkg, ssl);
        if (!has_ssl)
            bld_log_info("-- No TLS backend found, building without SSL\n");
    }

    Bld_Dep* dep_zlib    = opt_zlib    ? find_pkg("zlib")        : NULL;
    Bld_Dep* dep_nghttp2 = opt_nghttp2 ? find_pkg("libnghttp2")  : NULL;
    Bld_Dep* dep_ssh2    = opt_ssh2    ? find_pkg("libssh2")     : NULL;
    Bld_Dep* dep_idn2    = opt_idn2   ? find_pkg("libidn2")     : NULL;
    Bld_Dep* dep_psl     = opt_psl    ? find_pkg("libpsl")      : NULL;
    Bld_Dep* dep_brotli  = opt_brotli ? find_pkg("libbrotlidec") : NULL;
    Bld_Dep* dep_zstd    = opt_zstd   ? find_pkg("libzstd")     : NULL;
    Bld_Dep* dep_cares   = opt_cares  ? find_pkg("libcares")    : NULL;

    bool has_zlib    = dep_zlib    && dep_zlib->found;
    bool has_nghttp2 = dep_nghttp2 && dep_nghttp2->found;
    bool has_ssh2    = dep_ssh2    && dep_ssh2->found;
    bool has_idn2    = dep_idn2   && dep_idn2->found;
    bool has_psl     = dep_psl    && dep_psl->found;
    bool has_brotli  = dep_brotli && dep_brotli->found;
    bool has_zstd    = dep_zstd   && dep_zstd->found;
    bool has_cares   = dep_cares  && dep_cares->found;

    /* ---- Generate config headers ---- */

    generate_config(b, has_ssl, has_zlib, has_nghttp2, has_ssh2,
                    has_idn2, has_psl, has_brotli, has_zstd, has_cares,
                    opt_ipv6,
                    no_ws, no_proxy, no_cookies, no_auth, no_doh,
                    no_mime, no_netrc, no_file);

    /* ---- Compile defines ---- */

    /* Common defines (shared by lib and tool) */
    Bld_Strings common_defs = {0};
    bld_da_push(&common_defs, "HAVE_CONFIG_H");
    bld_da_push(&common_defs, "CURL_STATICLIB");
    if (http_only) bld_da_push(&common_defs, "HTTP_ONLY");
    if (no_ftp)    bld_da_push(&common_defs, "CURL_DISABLE_FTP");
    if (no_ldap)   bld_da_push(&common_defs, "CURL_DISABLE_LDAP");
    if (no_ldaps)  bld_da_push(&common_defs, "CURL_DISABLE_LDAPS");
    if (no_rtsp)   bld_da_push(&common_defs, "CURL_DISABLE_RTSP");
    if (no_dict)   bld_da_push(&common_defs, "CURL_DISABLE_DICT");
    if (no_telnet) bld_da_push(&common_defs, "CURL_DISABLE_TELNET");
    if (no_tftp)   bld_da_push(&common_defs, "CURL_DISABLE_TFTP");
    if (no_pop3)   bld_da_push(&common_defs, "CURL_DISABLE_POP3");
    if (no_imap)   bld_da_push(&common_defs, "CURL_DISABLE_IMAP");
    if (no_smtp)   bld_da_push(&common_defs, "CURL_DISABLE_SMTP");
    if (no_gopher) bld_da_push(&common_defs, "CURL_DISABLE_GOPHER");
    if (no_mqtt)   bld_da_push(&common_defs, "CURL_DISABLE_MQTT");

    /* Lib-specific defines */
    Bld_Strings lib_defs = {0};
    for (size_t i = 0; i < common_defs.count; i++)
        bld_da_push(&lib_defs, common_defs.items[i]);
    bld_da_push(&lib_defs, "BUILDING_LIBCURL");
    bld_da_push(&lib_defs, "CURL_HIDDEN_SYMBOLS");
    bld_da_push(&lib_defs, (const char*)NULL);

    /* Tool-specific defines */
    Bld_Strings tool_defs = {0};
    for (size_t i = 0; i < common_defs.count; i++)
        bld_da_push(&tool_defs, common_defs.items[i]);
    bld_da_push(&tool_defs, (const char*)NULL);

    /* ---- libcurl.a ---- */

    CompileFlags lib_cflags = default_compile_flags(b);
    lib_cflags.optimize = OPT_O2;
    lib_cflags.extra_flags = "-fvisibility=hidden -D_GNU_SOURCE";
    lib_cflags.defines = lib_defs.items;
    lib_cflags.include_dirs = BLD_PATHS("include", "lib", "generated");

    /*
     * curl compiles ALL source files unconditionally — feature gating is
     * via #ifdef inside the .c files, not by excluding files from the build.
     */
    const char** lib_srcs = files_glob("lib/**/*.c");

    Target* libcurl = add_lib(b,
        .name = "curl-lib",
        .output_name = "libcurl",
        .desc = "libcurl static library",
        .sources = lib_srcs,
        .compile = lib_cflags);

    /* Apply external dependencies to the library */
    if (has_ssl)     use_dep(libcurl, dep_ssl);
    if (has_zlib)    use_dep(libcurl, dep_zlib);
    if (has_nghttp2) use_dep(libcurl, dep_nghttp2);
    if (has_ssh2)    use_dep(libcurl, dep_ssh2);
    if (has_idn2)    use_dep(libcurl, dep_idn2);
    if (has_psl)     use_dep(libcurl, dep_psl);
    if (has_brotli)  use_dep(libcurl, dep_brotli);
    if (has_zstd)    use_dep(libcurl, dep_zstd);
    if (has_cares)   use_dep(libcurl, dep_cares);

    /* ---- curl executable ---- */

    CompileFlags tool_cflags = default_compile_flags(b);
    tool_cflags.optimize = OPT_O2;
    tool_cflags.extra_flags = "-D_GNU_SOURCE";
    tool_cflags.defines = tool_defs.items;
    tool_cflags.include_dirs = BLD_PATHS("include", "lib", "src", "generated");

    /*
     * The tool uses curlx_* functions which are Curl_* functions renamed
     * via macros when BUILDING_LIBCURL is not defined. These lib sources
     * must be compiled separately for the tool (without BUILDING_LIBCURL)
     * so the curlx_* symbols are available at link time.
     */
    const char** curlx_srcs = BLD_PATHS(
        "lib/base64.c", "lib/curl_get_line.c", "lib/curl_multibyte.c",
        "lib/dynbuf.c", "lib/nonblock.c", "lib/strtoofft.c",
        "lib/timediff.c", "lib/version_win32.c", "lib/warnless.c");
    const char** tool_srcs = files_merge(files_glob("src/**/*.c"), curlx_srcs);

    Target* curl_exe = add_exe(b,
        .name = "curl",
        .desc = "curl command-line tool",
        .sources = tool_srcs,
        .compile = tool_cflags,
        .link = (LinkFlags){ .extra_flags = "-lpthread" });
    link_with(curl_exe, libcurl);

    /* Static link: tool needs all lib deps for symbol resolution */
    if (has_ssl)     use_dep(curl_exe, dep_ssl);
    if (has_zlib)    use_dep(curl_exe, dep_zlib);
    if (has_nghttp2) use_dep(curl_exe, dep_nghttp2);
    if (has_ssh2)    use_dep(curl_exe, dep_ssh2);
    if (has_idn2)    use_dep(curl_exe, dep_idn2);
    if (has_psl)     use_dep(curl_exe, dep_psl);
    if (has_brotli)  use_dep(curl_exe, dep_brotli);
    if (has_zstd)    use_dep(curl_exe, dep_zstd);
    if (has_cares)   use_dep(curl_exe, dep_cares);

    /* ---- Installation ---- */

    add_install_exe(b, curl_exe);
    add_install_lib(b, libcurl);

    /* Install public headers: include/curl/*.h → $(prefix)/include/curl/ */
    Target* hdrs = add_step(b,
        .name = "headers",
        .desc = "Public headers",
        .action = install_headers_action,
        .action_ctx = b,
        .watch = BLD_PATHS("include/curl"));
    add_install(b, hdrs, bld_path("include"));

    /* ---- Test ---- */

    add_test(b, curl_exe,
        .name = "curl-version",
        .desc = "Run curl --version",
        .args = BLD_PATHS("--version"));

    /* ---- Run target: ./b run -- <args> ---- */

    add_run(b, curl_exe, .name = "run", .desc = "Run curl");
}

/*
 * ============================================================================
 * CASES FROM THE ORIGINAL CURL BUILD SYSTEM THAT bld.h CANNOT HANDLE
 * ============================================================================
 *
 * 1. WINDOWS SUPPORT
 *    curl's CMake/autotools supports Windows (MSVC, MinGW) with Schannel TLS,
 *    SSPI authentication, DLL exports via __declspec and .def/.rc files,
 *    WinSock2 (ws2_32), and Win32 IDN (normaliz). bld.h does not support
 *    Windows at all (#error on _WIN32 in bld_core.h).
 *
 * 2. SHARED LIBRARY WITH SYMBOL VERSIONING
 *    curl builds libcurl.so with a version script (libcurl.vers) for ABI
 *    versioning (SOVERSION 4.x.y). bld.h can build shared libraries but has
 *    no support for -Wl,--version-script or SONAME versioning.
 *
 * 3. MULTIPLE TLS BACKENDS SIMULTANEOUSLY (multi-SSL)
 *    curl can link multiple TLS backends (e.g. OpenSSL + Schannel) with
 *    runtime selection via CURLSSLBACKEND_*. This build supports only a
 *    single TLS backend at a time.
 *
 * 4. CODE GENERATION REQUIRING PERL
 *    Several curl build artifacts require Perl scripts:
 *    - tool_hugehelp.c: embedded help text from man page (mkhelp.pl)
 *    - tool_ca_embed.c: embedded CA certificate bundle (mk-file-embed.pl)
 *    - easyoptions.c: options table from curl.h (optiontable.pl)
 *    - Man pages: .md → .1 conversion (cd2nroff)
 *    bld.h has no mechanism to invoke Perl or other scripting interpreters
 *    as build steps. These could be added via custom steps, but are not
 *    implemented here.
 *
 * 5. PKG-CONFIG AND CMAKE PACKAGE CONFIG GENERATION
 *    curl generates libcurl.pc and CMake config files (CURLConfig.cmake,
 *    CURLConfigVersion.cmake, CURLTargets.cmake) at install time. bld.h
 *    does not generate .pc files (planned) or CMake config files.
 *
 * 6. CURL-CONFIG SHELL SCRIPT
 *    curl generates and installs curl-config, a shell script for consumers
 *    to query compile/link flags. No equivalent in bld.h.
 *
 * 7. INSTALLING MAN PAGES
 *    curl installs man pages (curl.1, curl_easy_*.3, etc.) to
 *    $(prefix)/man/. This build does not generate or install man pages.
 *
 * 8. TEST INFRASTRUCTURE
 *    curl has a Perl-based test harness (runtests.pl) with custom test HTTP/
 *    FTP/SMTP servers built from tests/server/. Unit tests link against
 *    special static builds with UNITTESTS define. bld.h's test API runs
 *    executables but cannot replicate this infrastructure.
 *
 * 9. PLATFORM-SPECIFIC TARGETS (AmigaOS, VMS, DOS)
 *    curl has specific source files and build logic for AmigaOS, VMS, and
 *    DOS (DJGPP). These are niche platforms and not supported by bld.h.
 *
 * 10. DEBUG BUILD POSTFIX
 *     curl's CMake adds a "-d" suffix to debug builds (libcurl-d.so). bld.h
 *     has no mechanism for build-mode-specific output name suffixes.
 *
 * 11. UNITY BUILD MODE
 *     CMake supports CMAKE_UNITY_BUILD for faster compilation via batched
 *     source inclusion. bld.h does not have a built-in unity build mode
 *     (it could be emulated via a custom codegen step, as bld itself does).
 *
 * 12. STATIC ANALYSIS INTEGRATION
 *     curl's CMake integrates clang-tidy, GCC -fanalyzer, and custom lint
 *     scripts. bld.h has no built-in static analysis support.
 *
 * 13. UNINSTALL TARGET
 *     curl provides uninstall capability. bld.h has no uninstall mechanism.
 *
 * 14. CROSS-COMPILATION
 *     curl supports cross-compilation via CMake toolchain files. bld.h has
 *     no cross-compilation support.
 *
 * 15. WCURL WRAPPER SCRIPT
 *     curl installs a wcurl wrapper script for simple downloads. This is
 *     just a shell script copy, but bld.h's install doesn't handle scripts.
 */
