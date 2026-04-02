#!/bin/bash
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$TEST_DIR/../common.sh"

setup_workdir
bld_bootstrap

# first install
bld_install
assert_success
assert_exe_output out/bin/app "42"

# no recompilation
bld_install
assert_success
assert_no_recompilation

# modify util.c body → only util.o recompiles
replace_in_file src/util.c "return 42" "return 99"
bld_install
assert_success
assert_recompiled "app:src/util.o"
assert_exe_output out/bin/app "99"

assert_not_recompiled "app:src/main.o"
