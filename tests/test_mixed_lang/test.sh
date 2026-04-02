#!/bin/bash
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$TEST_DIR/../common.sh"

setup_workdir
bld_bootstrap

bld_install
assert_success
assert_exe_output out/bin/mixed "Result: 42"

bld_install
assert_success
assert_no_recompilation

echo "/* touch */" >> src/helper.cpp
bld_install
assert_success
assert_recompiled "mixed:src/helper.o"
assert_not_recompiled "mixed:src/main.o"
