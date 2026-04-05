/* bld/bld_platform.h — platform abstraction types */
#pragma once

#include "bld_core.h"

/* subprocess flags and result */
typedef enum {
    BLD_PROC_DEFAULT  = 0,
    BLD_PROC_SILENT   = 1 << 0,
    BLD_PROC_PASSTHRU = 1 << 1,
} Bld_ProcFlags;

typedef struct {
    int      exit_code;
    Bld_Path output_file;  /* tmp file with captured stdout+stderr, empty if not captured */
} Bld_ProcResult;

/* directory iteration handle (opaque, defined in bld_platform.c) */
typedef struct Bld_Dir Bld_Dir;

/* worker function type for threading */
typedef void (*Bld_WorkerFn)(void* ctx);

/* host OS detection */
#ifdef __APPLE__
  #define BLD__HOST_OS BLD_OS_MACOS
#elif defined(__FreeBSD__)
  #define BLD__HOST_OS BLD_OS_FREEBSD
#elif defined(_WIN32)
  #define BLD__HOST_OS BLD_OS_WINDOWS
#else
  #define BLD__HOST_OS BLD_OS_LINUX
#endif
