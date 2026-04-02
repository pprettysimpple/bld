#!/bin/bash
# Generate a complex project with 20 targets forming a dependency DAG:
#
#   shared_base (3 files, shared lib)
#   ├── libcore (8 files, static, depends on shared_base)
#   │   ├── libmath (5 files, static, depends on libcore)
#   │   ├── libtext (4 files, static, depends on libcore)
#   │   └── libio (6 files, static, depends on libcore)
#   ├── libutil (7 files, static, depends on shared_base)
#   │   └── libformat (3 files, static, depends on libutil + libtext)
#   └── libnet (4 files, static, depends on shared_base)
#
#   app_main (10 files, exe, depends on libmath + libtext + libformat)
#   app_tool (5 files, exe, depends on libcore + libutil)
#   app_server (8 files, exe, depends on libnet + libio + libcore)
#   app_cli (3 files, exe, depends on libmath + libformat + libnet)
#
#   plugin_a (4 files, shared lib, depends on libcore)
#   plugin_b (6 files, shared lib, depends on libutil + libtext)
#   plugin_c (2 files, shared lib, depends on libmath)
#
#   test_core (3 files, exe, depends on libcore)
#   test_math (2 files, exe, depends on libmath)
#   test_net (2 files, exe, depends on libnet + shared_base)
#
# Total: 20 targets, ~85 source files, mix of static/shared/exe
# All C except shared_base (C++) for multi-language coverage

set -e

# common header included by everything
mkdir -p include
cat > include/project.h << 'H'
#ifndef PROJECT_H
#define PROJECT_H
#define PROJECT_VERSION 1
int project_id(void);
#endif
H

gen_lib() {
    local dir="$1" prefix="$2" nfiles="$3" header_deps="$4"
    mkdir -p "$dir"

    # header
    cat > "$dir/${prefix}.h" << EOF
#ifndef ${prefix^^}_H
#define ${prefix^^}_H
EOF
    for i in $(seq 1 "$nfiles"); do
        echo "int ${prefix}_f${i}(void);" >> "$dir/${prefix}.h"
    done
    echo "#endif" >> "$dir/${prefix}.h"

    # source files
    for i in $(seq 1 "$nfiles"); do
        cat > "$dir/${prefix}_f${i}.c" << EOF
#include "../include/project.h"
${header_deps}
#include "${prefix}.h"
int ${prefix}_f${i}(void) { return ${i} * PROJECT_VERSION; }
EOF
    done
}

gen_cxx_lib() {
    local dir="$1" prefix="$2" nfiles="$3"
    mkdir -p "$dir"

    # C-compatible header
    cat > "$dir/${prefix}.h" << EOF
#ifndef ${prefix^^}_H
#define ${prefix^^}_H
#ifdef __cplusplus
extern "C" {
#endif
EOF
    for i in $(seq 1 "$nfiles"); do
        echo "int ${prefix}_f${i}(void);" >> "$dir/${prefix}.h"
    done
    cat >> "$dir/${prefix}.h" << 'EOF'
#ifdef __cplusplus
}
#endif
#endif
EOF

    # C++ source files
    for i in $(seq 1 "$nfiles"); do
        cat > "$dir/${prefix}_f${i}.cpp" << EOF
#include "../include/project.h"
#include "${prefix}.h"
extern "C" int ${prefix}_f${i}(void) { return ${i} * PROJECT_VERSION; }
EOF
    done
}

gen_exe() {
    local dir="$1" name="$2" nfiles="$3" includes="$4" calls="$5"
    mkdir -p "$dir"

    # helper files
    for i in $(seq 1 $((nfiles - 1))); do
        cat > "$dir/${name}_helper${i}.c" << EOF
#include "../include/project.h"
int ${name}_h${i}(void) { return ${i} + PROJECT_VERSION; }
EOF
    done

    # main file
    cat > "$dir/${name}_main.c" << EOF
#include <stdio.h>
#include "../include/project.h"
${includes}
EOF

    for i in $(seq 1 $((nfiles - 1))); do
        echo "int ${name}_h${i}(void);" >> "$dir/${name}_main.c"
    done

    cat >> "$dir/${name}_main.c" << EOF
int main(void) {
    int sum = 0;
EOF

    for i in $(seq 1 $((nfiles - 1))); do
        echo "    sum += ${name}_h${i}();" >> "$dir/${name}_main.c"
    done

    # add calls to library functions
    echo "$calls" >> "$dir/${name}_main.c"

    cat >> "$dir/${name}_main.c" << 'EOF'
    printf("result=%d\n", sum);
    return 0;
}
EOF
}

# ---- Generate all targets ----

# shared_base: 3 C++ files (shared lib)
gen_cxx_lib sbase sbase 3

# libcore: 8 files
gen_lib lcore core 8 '#include "../sbase/sbase.h"'

# libmath: 5 files
gen_lib lmath math 5 '#include "../lcore/core.h"'

# libtext: 4 files
gen_lib ltext text 4 '#include "../lcore/core.h"'

# libio: 6 files
gen_lib lio io 6 '#include "../lcore/core.h"'

# libutil: 7 files
gen_lib lutil util 7 '#include "../sbase/sbase.h"'

# libformat: 3 files
gen_lib lfmt fmt 3 '#include "../lutil/util.h"
#include "../ltext/text.h"'

# libnet: 4 files
gen_lib lnet net 4 '#include "../sbase/sbase.h"'

# app_main: 10 files
gen_exe amain app_main 10 \
    '#include "../lmath/math.h"
#include "../ltext/text.h"
#include "../lfmt/fmt.h"' \
    '    sum += math_f1() + text_f1() + fmt_f1();'

# app_tool: 5 files
gen_exe atool app_tool 5 \
    '#include "../lcore/core.h"
#include "../lutil/util.h"' \
    '    sum += core_f1() + util_f1();'

# app_server: 8 files
gen_exe aserv app_serv 8 \
    '#include "../lnet/net.h"
#include "../lio/io.h"
#include "../lcore/core.h"' \
    '    sum += net_f1() + io_f1() + core_f1();'

# app_cli: 3 files
gen_exe acli app_cli 3 \
    '#include "../lmath/math.h"
#include "../lfmt/fmt.h"
#include "../lnet/net.h"' \
    '    sum += math_f1() + fmt_f1() + net_f1();'

# plugin_a: 4 files (shared)
gen_lib pluga pluga 4 '#include "../lcore/core.h"'

# plugin_b: 6 files (shared)
gen_lib plugb plugb 6 '#include "../lutil/util.h"
#include "../ltext/text.h"'

# plugin_c: 2 files (shared)
gen_lib plugc plugc 2 '#include "../lmath/math.h"'

# test_core: 3 files
gen_exe tcore test_core 3 \
    '#include "../lcore/core.h"' \
    '    sum += core_f1();'

# test_math: 2 files
gen_exe tmath test_math 2 \
    '#include "../lmath/math.h"' \
    '    sum += math_f1();'

# test_net: 2 files
gen_exe tnet test_net 2 \
    '#include "../lnet/net.h"
#include "../sbase/sbase.h"' \
    '    sum += net_f1() + sbase_f1();'
