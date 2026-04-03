#!/bin/bash
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$TEST_DIR/../common.sh"

setup_workdir
bld_bootstrap

# clean build
bld_install
assert_success
ref_hash=$(find out -type f | sort | xargs md5sum | md5sum | cut -d' ' -f1)

# no-op rebuild
bld_install
assert_success
assert_no_recompilation

# for each source file: append a comment, rebuild, revert, rebuild, check hash
for f in s/*.c; do
    cp "$f" "$f.bak"
    echo "/* touched */" >> "$f"

    bld_install
    assert_success

    mv "$f.bak" "$f"
    bld_install
    assert_success

    cur_hash=$(find out -type f | sort | xargs md5sum | md5sum | cut -d' ' -f1)
    [ "$cur_hash" = "$ref_hash" ] || die "hash mismatch after reverting $f"
done
