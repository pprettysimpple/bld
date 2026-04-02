#!/bin/bash
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$TEST_DIR/../common.sh"

setup_workdir
bld_bootstrap

# first install
bld_install
assert_success
assert_exe_output out/bin/app "version=2"  # 1+1

# no recompilation
bld_install
assert_success
assert_no_recompilation

# modify header → both .c files must recompile (depfile tracking)
replace_in_file src/config.h "APP_VERSION 1" "APP_VERSION 5"
bld_install
assert_success
assert_recompiled "app:src/main.o"
assert_recompiled "app:src/lib.o"
assert_exe_output out/bin/app "version=10"  # 5+5
