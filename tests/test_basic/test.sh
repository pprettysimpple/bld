#!/bin/bash
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$TEST_DIR/../common.sh"

setup_workdir
bld_bootstrap

# first install
bld_install
assert_success
assert_file_exists out/bin/hello
assert_exe_output out/bin/hello "hello bld"

# second install: no recompilation
bld_install
assert_success
assert_no_recompilation
