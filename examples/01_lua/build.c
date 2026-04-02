/* build.c — Lua 5.4.7 build matching the official Makefile
 *
 * Bootstrap: cc -std=c11 -w build.c -o b -lpthread
 * Usage:
 *   ./b build                              — build for linux (default)
 *   ./b build -Dplatform=macosx            — build for macOS
 *   ./b build -Dplatform=linux-readline    — Linux with readline
 *   ./b build -Dcompat=off                 — disable Lua 5.3 compatibility
 *   ./b install --prefix /usr/local        — install lua, luac, liblua.a
 *   ./b test                               — run lua -v
 */
#define BLD_IMPLEMENTATION
#define BLD_STRIP_PREFIX
#include "bld.h"

BLD_RECOMPILE_CMD("cc -std=c11 -w build.c -lpthread")

/* Core VM sources (CORE_O in Makefile) */
static const char* CORE_SOURCES[] = {
    "src/lapi.c",    "src/lcode.c",   "src/lctype.c",  "src/ldebug.c",
    "src/ldo.c",     "src/ldump.c",   "src/lfunc.c",   "src/lgc.c",
    "src/llex.c",    "src/lmem.c",    "src/lobject.c", "src/lopcodes.c",
    "src/lparser.c", "src/lstate.c",  "src/lstring.c", "src/ltable.c",
    "src/ltm.c",     "src/lundump.c", "src/lvm.c",     "src/lzio.c",
    NULL
};

/* Standard library sources (LIB_O in Makefile) */
static const char* LIB_SOURCES[] = {
    "src/lauxlib.c", "src/lbaselib.c", "src/lcorolib.c", "src/ldblib.c",
    "src/liolib.c",  "src/lmathlib.c", "src/loadlib.c",  "src/loslib.c",
    "src/lstrlib.c", "src/ltablib.c",  "src/lutf8lib.c", "src/linit.c",
    NULL
};

void configure(Bld* b) {
    /* ---- User options ---- */

    const char* platform = option_str(b, "platform",
        "Target platform: linux, linux-readline, macosx, freebsd, bsd, posix, solaris, generic",
        "linux");
    bool compat = option_bool(b, "compat", "Enable Lua 5.3 compatibility (LUA_COMPAT_5_3)", true);

    /* ---- Platform-specific flags (mirrors src/Makefile platform targets) ---- */

    const char* syscflags = "";
    const char* syslibs   = "";    /* combined into link extra_flags with -lm */
    const char* sysldflags = "";

    if (strcmp(platform, "linux") == 0 || strcmp(platform, "linux-noreadline") == 0) {
        syscflags  = "-DLUA_USE_LINUX";
        syslibs    = "-Wl,-E -ldl";
    } else if (strcmp(platform, "linux-readline") == 0) {
        syscflags  = "-DLUA_USE_LINUX -DLUA_USE_READLINE";
        syslibs    = "-Wl,-E -ldl -lreadline";
    } else if (strcmp(platform, "macosx") == 0 || strcmp(platform, "darwin") == 0) {
        syscflags  = "-DLUA_USE_MACOSX -DLUA_USE_READLINE";
        syslibs    = "-lreadline";
    } else if (strcmp(platform, "freebsd") == 0) {
        syscflags  = "-DLUA_USE_LINUX -DLUA_USE_READLINE -I/usr/include/edit";
        syslibs    = "-Wl,-E -ledit";
    } else if (strcmp(platform, "bsd") == 0) {
        syscflags  = "-DLUA_USE_POSIX -DLUA_USE_DLOPEN";
        syslibs    = "-Wl,-E";
    } else if (strcmp(platform, "posix") == 0) {
        syscflags  = "-DLUA_USE_POSIX";
    } else if (strcmp(platform, "solaris") == 0) {
        syscflags  = "-DLUA_USE_POSIX -DLUA_USE_DLOPEN -D_REENTRANT";
        syslibs    = "-ldl";
    } else if (strcmp(platform, "generic") == 0) {
        /* bare minimum — no platform features */
    } else {
        bld_panic("Unknown platform: '%s'. Use -Dplatform=<name> or see ./b2 --help", platform);
    }

    /* ---- Compile flags ---- */

    set_compiler_c(b, .standard = C_GNU99);
    CompileFlags cflags = default_compile_flags(b);
    cflags.optimize = OPT_O2;
    cflags.warnings = BLD_ON;          /* -Wall -Wextra */
    cflags.extra_flags = syscflags;    /* platform defines via -D in extra_flags */
    cflags.include_dirs = BLD_PATHS("src");

    if (compat) {
        cflags.defines = BLD_DEFS("LUA_COMPAT_5_3");
    }

    /* ---- Link flags ---- */

    const char* link_extra = bld_str_fmt("-lm %s %s", syslibs, sysldflags);

    /* ---- liblua.a (BASE_O = CORE_O + LIB_O) ---- */

    const char** all_sources = bld_files_merge(CORE_SOURCES, LIB_SOURCES);

    Target* liblua = add_lib(b,
        .name = "lua-lib",
        .output_name = "liblua",
        .desc = "Lua static library",
        .sources = all_sources,
        .compile = cflags);

    /*
     * Per-file overrides: the Makefile applies $(CMCFLAGS) to llex.o, lparser.o,
     * lcode.o — intended for -Os to reduce compiler frontend code size.
     */
    override_file(liblua, "src/llex.c",    .optimize = OPT_Os);
    override_file(liblua, "src/lparser.c",  .optimize = OPT_Os);
    override_file(liblua, "src/lcode.c",    .optimize = OPT_Os);

    /* ---- lua interpreter ---- */

    Target* lua = add_exe(b,
        .name = "lua",
        .desc = "Lua interpreter",
        .sources = BLD_PATHS("src/lua.c"),
        .compile = cflags,
        .link = (LinkFlags){ .extra_flags = link_extra });
    link_with(lua, liblua);

    /* ---- luac compiler ---- */

    Target* luac = add_exe(b,
        .name = "luac",
        .desc = "Lua compiler (source → bytecode)",
        .sources = BLD_PATHS("src/luac.c"),
        .compile = cflags,
        .link = (LinkFlags){ .extra_flags = link_extra });
    link_with(luac, liblua);

    /* ---- Installation ---- */

    add_install_exe(b, lua);
    add_install_exe(b, luac);
    add_install_lib(b, liblua);

    /* ---- Test (matches Makefile `make test`: just run lua -v) ---- */

    add_test(b, lua,
        .name = "lua-version",
        .desc = "Verify lua runs (lua -v)",
        .args = BLD_PATHS("-v"));
}

/*
 * ============================================================================
 * CASES FROM THE ORIGINAL LUA MAKEFILE THAT bld.h CANNOT HANDLE
 * ============================================================================
 *
 * 1. WINDOWS / MINGW BUILD (mingw target)
 *    The Lua Makefile builds lua54.dll + lua.exe + luac.exe on Windows using
 *    -DLUA_BUILD_AS_DLL and __declspec(dllexport/dllimport). bld.h does not
 *    support Windows at all (#error in bld_core.h when _WIN32 is defined).
 *
 * 2. AUTOMATIC PLATFORM DETECTION (guess target)
 *    `make guess` runs `uname` and re-invokes make with the detected platform
 *    name. bld.h has no built-in platform auto-detection; the user must pass
 *    -Dplatform=<name> explicitly. Could be implemented in configure() with
 *    uname(2), but that would be custom C code, not a bld.h feature.
 *
 * 3. UNINSTALL TARGET
 *    `make uninstall` removes all installed files. bld.h provides install
 *    targets but has no uninstall mechanism.
 *
 * 4. PKG-CONFIG METADATA GENERATION (pc target)
 *    `make pc` echoes a pkg-config .pc file to stdout. bld.h does not
 *    generate .pc files. This is a planned but unimplemented feature.
 *
 * 5. INSTALLING HEADERS AND MAN PAGES
 *    The Makefile installs:
 *      - Headers: lua.h luaconf.h lualib.h lauxlib.h lua.hpp → include/
 *      - Man pages: lua.1 luac.1 → man/man1/
 *    bld.h's install API (install_exe, install_lib) only handles build
 *    artifacts. Arbitrary file installation would require a custom step.
 *
 * 6. CREATING MODULE DIRECTORIES AT INSTALL TIME
 *    The Makefile creates:
 *      $(prefix)/share/lua/5.4/  (pure Lua modules)
 *      $(prefix)/lib/lua/5.4/    (C modules)
 *    These are empty directories for the runtime module search path.
 *    bld.h has no mechanism for creating empty directories during install.
 *
 * 7. MYCFLAGS / MYLDFLAGS / MYLIBS / MYOBJS HOOKS
 *    The Makefile provides user-override variables for injecting arbitrary
 *    flags and extra object files without editing the Makefile itself.
 *    bld.h has -Dkey=value options but no equivalent passthrough mechanism.
 *    The user would edit build2.c directly instead.
 *
 * 8. AIX TARGET (xlc compiler)
 *    The AIX target uses the xlc compiler (not gcc) and special linker flags
 *    (-brtl -bexpall). bld.h uses a single compile_driver (default: cc) and
 *    has no mechanism to switch compilers per-platform within configure().
 *
 * 9. C89 MODE DIAGNOSTIC
 *    `make c89` uses gcc -std=c89, defines LUA_USE_C89, and prints a warning
 *    about potential 64-bit integer issues. While BLD_STD_C90 exists (closest
 *    to C89), the build-time diagnostic message is Makefile-specific behavior
 *    that bld.h cannot replicate (no build-time "echo" facility).
 *
 * 10. LOCAL INSTALL SHORTCUT
 *     `make local` installs to ../install/ relative to source. bld.h uses
 *     --prefix for install location and has no built-in relative shortcut.
 *
 * 11. RANLIB CUSTOMIZATION
 *     The Makefile allows overriding AR and RANLIB tools. bld.h uses a
 *     hardcoded static_link_tool (default: "ar") and does not expose RANLIB
 *     as a separate configurable step.
 *
 * NOTE: The Makefile's `depend` target (regenerate header deps via gcc -MM)
 * is NOT listed as a gap — bld.h handles this automatically and better via
 * -MMD depfile tracking with content-hash caching.
 */
