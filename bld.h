/*  bld.h — single-header C build system
 *
 *  #define BLD_IMPLEMENTATION   — include implementation (do this in exactly one .c file)
 *  #define BLD_STRIP_PREFIX     — strip bld_/Bld_/BLD_ prefixes for convenience
 */
#pragma once

#include "bld/bld_core.h"

/* implementation */
#ifdef BLD_IMPLEMENTATION
#include "bld/bld_core_impl.c"
#include "bld/bld_build.c"
#include "bld/bld_exec.c"
#include "bld/bld_cli.c"
#endif /* BLD_IMPLEMENTATION */

/* strip prefixes */
#ifdef BLD_STRIP_PREFIX

/* types */
#define Arena       Bld_Arena
#define Path        Bld_Path
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
#define CompileFlags Bld_CompileFlags
#define LinkFlags   Bld_LinkFlags
#define Optimize    Bld_Optimize
#define Standard    Bld_Standard
#define Toggle      Bld_Toggle
#define ActionFn    Bld_ActionFn
#define RecipeHashFn Bld_RecipeHashFn
#define TargetKind  Bld_TargetKind

/* enums */
#define OPT_DEFAULT BLD_OPT_DEFAULT
#define OPT_O0     BLD_OPT_O0
#define OPT_O1     BLD_OPT_O1
#define OPT_O2     BLD_OPT_O2
#define OPT_O3     BLD_OPT_O3
#define OPT_Os     BLD_OPT_Os
#define OPT_OFAST  BLD_OPT_OFAST
#define STD_DEFAULT BLD_STD_DEFAULT
#define STD_C90    BLD_STD_C90
#define STD_C99    BLD_STD_C99
#define STD_C11    BLD_STD_C11
#define STD_C17    BLD_STD_C17
#define STD_C23    BLD_STD_C23
#define STD_GNU99  BLD_STD_GNU99
#define STD_GNU11  BLD_STD_GNU11
#define STD_GNU17  BLD_STD_GNU17
#define STD_GNU23  BLD_STD_GNU23
#define STD_CXX11  BLD_STD_CXX11
#define STD_CXX14  BLD_STD_CXX14
#define STD_CXX17  BLD_STD_CXX17
#define STD_CXX20  BLD_STD_CXX20
#define STD_CXX23  BLD_STD_CXX23
#define TOGGLE_UNSET BLD_UNSET
#define TOGGLE_ON    BLD_ON
#define TOGGLE_OFF   BLD_OFF
#define TGT_EXE    BLD_TGT_EXE
#define TGT_LIB    BLD_TGT_LIB
#define TGT_CUSTOM BLD_TGT_CUSTOM

/* macros — not stripped: DA, PATHS, DEFS, path() are too generic */

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
#define files_glob      bld_files_glob
#define files_exclude   bld_files_exclude
#define files_merge     bld_files_merge

/* log — not stripped: "log" and "panic" conflict with common names */

/* hash */
#define hash_combine           bld_hash_combine
#define hash_combine_unordered bld_hash_combine_unordered
#define hash_str               bld_hash_str
#define hash_file              bld_hash_file
#define hash_dir               bld_hash_dir

/* build api */
#define add_exe(b, ...)          bld_add_exe((b), __VA_ARGS__)
#define add_lib(b, ...)          bld_add_lib((b), __VA_ARGS__)
#define add_step(b, ...)         bld_add_step((b), __VA_ARGS__)
#define add_run(b, t, ...)       bld_add_run((b), (t), __VA_ARGS__)
#define depends_on               bld_depends_on
#define link_with                bld_link_with
#define target_output            bld_output
#define target_output_sub        bld_output_sub
#define target_add_include_dir   bld_add_include_dir
#define target_add_source(t, s)  bld_add_source((t), (s))
#define add_install_exe          bld_install_exe
#define add_install_lib          bld_install_lib
#define add_install              bld_install
#define add_test(b, t, ...)      bld_add_test((b), (t), __VA_ARGS__)
#define use_dep                  bld_use_dep
#define override_file(t, f, ...) bld_override_file((t), (f), __VA_ARGS__)
#define find_pkg                 bld_find_pkg
#define clone_compile_flags      bld_clone_compile_flags
#define default_compile_flags    bld_default_compile_flags
#define default_link_flags       bld_default_link_flags
#define option_bool              bld_option_bool
#define option_str               bld_option_str
#define target_ok                bld_target_ok
#define target_artifact          bld_target_artifact

#endif /* BLD_STRIP_PREFIX */
