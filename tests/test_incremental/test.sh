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

# --- kill-recovery: kill bld mid-compilation, verify incremental rebuild
# produces byte-identical output to the reference build ---

delays=(0.001 0.005 0.01 0.02 0.05)
actual_kills=0
for i in "${!delays[@]}"; do
    round=$((i + 1))
    rm -rf .cache out 2>/dev/null
    bld_bootstrap

    # start build in background, kill it mid-flight
    ./b install -j4 --prefix out >/dev/null 2>&1 &
    pid=$!

    sleep "${delays[$i]}"

    set +e
    kill -9 "$pid" 2>/dev/null
    kill_ok=$?
    wait "$pid" 2>/dev/null
    set -e

    if [ "$kill_ok" -eq 0 ]; then actual_kills=$((actual_kills + 1)); fi

    # incremental rebuild on top of partial state — must match reference
    bld_install
    assert_success

    cur_hash=$(find out -type f | sort | xargs md5sum | md5sum | cut -d' ' -f1)
    [ "$cur_hash" = "$ref_hash" ] || die "kill round $round: hash mismatch"
done

[ "$actual_kills" -gt 0 ] || die "no builds were actually killed ($actual_kills)"
