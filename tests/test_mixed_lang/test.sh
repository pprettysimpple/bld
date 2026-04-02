#!/bin/bash
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$TEST_DIR/../common.sh"

setup_workdir
bld_bootstrap

# first install
bld_install
assert_success
assert_exe_output out/bin/mixed "Result: 42"

# no recompilation
bld_install
assert_success
assert_no_recompilation

# modify only .cpp → only helper.o recompiles
modify_file src/helper.cpp "/* touched */"
bld_install
assert_success
assert_recompiled "mixed:src/helper.o"
assert_not_recompiled "mixed:src/main.o"

# no recompilation
bld_install
assert_success
assert_no_recompilation

# modify only .c → only main.o recompiles
modify_file src/main.c "/* touched */"
bld_install
assert_success
assert_recompiled "mixed:src/main.o"
assert_not_recompiled "mixed:src/helper.o"
