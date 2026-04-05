/*  bld.h — single-header C build system
 *
 *  #define BLD_IMPLEMENTATION   — include implementation (do this in exactly one .c file)
 *  #define BLD_STRIP_PREFIX     — strip bld_/Bld_/BLD_ prefixes for convenience
 */
#pragma once

#include "bld/bld_core.h"
#include "bld/bld_cache.h"
#include "bld/bld_dep.h"
#include "bld/bld_checks.h"

/* implementation */
#ifdef BLD_IMPLEMENTATION
#include "bld/bld_core_impl.c"
#include "bld/bld_cache.c"
#include "bld/bld_dep.c"
#include "bld/bld_build.c"
#include "bld/bld_checks.c"
#include "bld/bld_exec.c"
#include "bld/bld_cli.c"
#endif /* BLD_IMPLEMENTATION */

/* strip prefixes */
#ifdef BLD_STRIP_PREFIX

/* types */
#define Arena       Bld_Arena
#define Path        Bld_Path
#define Cmd         Bld_Cmd
#define Hash        Bld_Hash
#define PathList    Bld_PathList
#define Target      Bld_Target
#define Step        Bld_Step
#define LazyPath    Bld_LazyPath
#define Exe         Bld_Exe
#define Lib         Bld_Lib
#define ExeOpts     Bld_ExeOpts
#define LibOpts     Bld_LibOpts
#define RunOpts     Bld_RunOpts
#define StepOpts    Bld_StepOpts
#define CmdOpts     Bld_CmdOpts
#define Strs        Bld_Strs
#define Paths       Bld_Paths
#define BuildFlags  Bld_BuildFlags
#define CompileFlags Bld_CompileFlags
#define LinkFlags   Bld_LinkFlags
#define Optimize    Bld_Optimize
#define Lang        Bld_Lang
#define CStd        Bld_CStd
#define CxxStd      Bld_CxxStd
#define Compiler    Bld_Compiler
#define Toggle      Bld_Toggle
#define OsTarget    Bld_OsTarget
#define Tool        Bld_Tool
#define Toolchain   Bld_Toolchain
#define OS_LINUX    BLD_OS_LINUX
#define OS_MACOS    BLD_OS_MACOS
#define OS_WINDOWS  BLD_OS_WINDOWS
#define OS_FREEBSD  BLD_OS_FREEBSD
#define ActionFn    Bld_ActionFn
#define HashFn       Bld_HashFn
#define TargetKind  Bld_TargetKind

/* enums */
#define OPT_DEFAULT BLD_OPT_DEFAULT
#define OPT_O0     BLD_OPT_O0
#define OPT_O1     BLD_OPT_O1
#define OPT_O2     BLD_OPT_O2
#define OPT_O3     BLD_OPT_O3
#define OPT_Os     BLD_OPT_Os
#define OPT_OFAST  BLD_OPT_OFAST
#define LANG_AUTO  BLD_LANG_AUTO
#define LANG_C     BLD_LANG_C
#define LANG_CXX   BLD_LANG_CXX
#define LANG_ASM   BLD_LANG_ASM
#define C_DEFAULT  BLD_C_DEFAULT
#define C_90       BLD_C_90
#define C_99       BLD_C_99
#define C_11       BLD_C_11
#define C_17       BLD_C_17
#define C_23       BLD_C_23
#define C_GNU90    BLD_C_GNU90
#define C_GNU99    BLD_C_GNU99
#define C_GNU11    BLD_C_GNU11
#define C_GNU17    BLD_C_GNU17
#define C_GNU23    BLD_C_GNU23
#define CXX_11     BLD_CXX_11
#define CXX_14     BLD_CXX_14
#define CXX_17     BLD_CXX_17
#define CXX_20     BLD_CXX_20
#define CXX_23     BLD_CXX_23
#define CXX_GNU11  BLD_CXX_GNU11
#define CXX_GNU14  BLD_CXX_GNU14
#define CXX_GNU17  BLD_CXX_GNU17
#define CXX_GNU20  BLD_CXX_GNU20
#define CXX_GNU23  BLD_CXX_GNU23
#define TOGGLE_UNSET BLD_UNSET
#define TOGGLE_ON    BLD_ON
#define TOGGLE_OFF   BLD_OFF
#define TGT_EXE    BLD_TGT_EXE
#define TGT_LIB    BLD_TGT_LIB
#define TGT_CUSTOM BLD_TGT_CUSTOM

/* macros */
#define STRS(...)  BLD_STRS(__VA_ARGS__)
#define PATHS(...) BLD_PATHS(__VA_ARGS__)

/* arena */
#define arena_get     bld_arena_get
#define arena_alloc   bld_arena_alloc
#define arena_realloc bld_arena_realloc

/* str */
#define str_fmt  bld_str_fmt
#define str_dup    bld_str_dup
#define str_cat    bld_str_cat
#define str_lines  bld_str_lines
#define str_join   bld_str_join
#define strs_push    bld_strs_push
#define paths_push   bld_paths_push
#define strs_merge   bld_strs_merge
#define paths_merge  bld_paths_merge

/* path — not stripped: path/path_join etc. are too generic */

/* fs */
#define fs_exists       bld_fs_exists
#define fs_is_dir       bld_fs_is_dir
#define fs_is_file      bld_fs_is_file
#define fs_mkdir_p      bld_fs_mkdir_p
#define fs_remove       bld_fs_remove
#define fs_remove_all   bld_fs_remove_all
#define fs_rename       bld_fs_rename
#define fs_copy_file    bld_fs_copy_file
#define fs_copy_r       bld_fs_copy_r
#define fs_realpath     bld_fs_realpath
#define fs_getcwd       bld_fs_getcwd
#define fs_list_files_r bld_fs_list_files_r
#define fs_read_file    bld_fs_read_file
#define fs_write_file   bld_fs_write_file
#define fs_write_str    bld_fs_write_str
#define files_glob      bld_files_glob
#define files_exclude   bld_files_exclude
#define files_merge     bld_files_merge

/* log — not stripped: "log" and "panic" conflict with common names */

/* cmd */
#define cmd_appendf   bld_cmd_appendf
#define cmd_append_sq bld_cmd_append_sq

/* hash */
#define hash_combine           bld_hash_combine
#define hash_combine_unordered bld_hash_combine_unordered
#define hash_str               bld_hash_str
#define hash_file              bld_hash_file
#define hash_dir               bld_hash_dir

/* toolchain */
#define toolchain_gcc  bld_toolchain_gcc

/* compiler setters */
#define set_compiler_c(b, ...)   bld_set_compiler_c((b), __VA_ARGS__)
#define set_compiler_cxx(b, ...) bld_set_compiler_cxx((b), __VA_ARGS__)
#define set_compiler_asm(b, ...) bld_set_compiler_asm((b), __VA_ARGS__)

/* build api */
#define add_exe(b, ...)          bld_add_exe((b), __VA_ARGS__)
#define add_lib(b, ...)          bld_add_lib((b), __VA_ARGS__)
#define add_step(b, ...)         bld_add_step((b), __VA_ARGS__)
#define add_cmd(b, ...)          bld_add_cmd((b), __VA_ARGS__)
#define add_run(b, t, ...)       bld_add_run((b), (t), __VA_ARGS__)
#define depends_on               bld_depends_on
#define link_with                bld_link_with
#define target_output            bld_output
#define target_output_sub        bld_output_sub
#define target_add_include_dir   bld_add_include_dir
#define target_add_source(t, s)  bld_add_source((t), (s))
#define install_exe              bld_install_exe
#define install_lib              bld_install_lib
#define install_target           bld_install
#define install_files            bld_install_files
#define install_dir              bld_install_dir
#define add_test(b, t, ...)      bld_add_test((b), (t), __VA_ARGS__)
#define use_dep                  bld_use_dep
#define override_file(t, f, ...) bld_override_file((t), (f), __VA_ARGS__)
#define find_pkg                 bld_find_pkg
#define clone_compile_flags      bld_clone_compile_flags
#define default_compile_flags    bld_default_compile_flags
#define option_bool              bld_option_bool
#define option_str               bld_option_str
#define target_ok                bld_target_ok
#define target_artifact          bld_target_artifact

/* feature checks */
#define checks_new               bld_checks_new
#define checks_header            bld_checks_header
#define checks_func              bld_checks_func
#define checks_sizeof            bld_checks_sizeof
#define checks_compile           bld_checks_compile
#define checks_run               bld_checks_run
#define checks_write             bld_checks_write

#endif /* BLD_STRIP_PREFIX */
