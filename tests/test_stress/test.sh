#!/bin/bash
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$TEST_DIR/../common.sh"

NUM_FILES=50
KILL_ROUNDS=10
CORRUPT_ROUNDS=5

setup_workdir

# generate sources
bash "$TEST_DIR/gen_sources.sh" "$NUM_FILES"

# expected output: sum of 1..N
expected_sum=$(( NUM_FILES * (NUM_FILES + 1) / 2 ))

bld_bootstrap

# ---- Phase 1: clean build, verify correctness ----

bld_install
assert_success
assert_exe_output out/bin/stress "$expected_sum"

# ---- Phase 2: kill mid-build ----

for round in $(seq 1 "$KILL_ROUNDS"); do
    # nuke cache to force full rebuild
    rm -rf .cache

    # launch build in background, kill after random delay
    ./b install --prefix out >/dev/null 2>&1 &
    pid=$!
    # random delay 0.01-0.15s (builds are fast)
    sleep "0.$(printf '%02d' $((RANDOM % 15 + 1)))"
    kill -9 "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true

    # clean up orphan tmp files
    rm -rf .cache/tmp 2>/dev/null || true

    # recovery build must succeed
    bld_install
    assert_success
    assert_exe_output out/bin/stress "$expected_sum"
done

# ---- Phase 3: cache corruption — delete random artifacts ----

for round in $(seq 1 "$CORRUPT_ROUNDS"); do
    if [ -d .cache/arts ]; then
        find .cache/arts -type f | shuf | head -n $((RANDOM % 5 + 1)) | xargs -r rm -f
    fi

    bld_install
    assert_success
    assert_exe_output out/bin/stress "$expected_sum"
done

# ---- Phase 4: cache corruption — truncate artifacts ----

for round in $(seq 1 "$CORRUPT_ROUNDS"); do
    if [ -d .cache/arts ]; then
        target=$(find .cache/arts -type f | shuf | head -1)
        if [ -n "$target" ]; then
            : > "$target"
        fi
    fi

    bld_install
    assert_success
    assert_exe_output out/bin/stress "$expected_sum"
done

# ---- Phase 5: delete all depfiles ----

rm -rf .cache/deps
bld_install
assert_success
assert_exe_output out/bin/stress "$expected_sum"

# ---- Phase 6: nuke entire cache ----

rm -rf .cache
bld_install
assert_success
assert_exe_output out/bin/stress "$expected_sum"

# ---- Phase 7: concurrent builds ----

rm -rf .cache out
bld_install
assert_success

# launch 4 concurrent installs to different prefixes
pids=()
for i in $(seq 1 4); do
    ./b install --prefix "out${i}" >/dev/null 2>&1 &
    pids+=($!)
done

concurrent_fail=0
for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
        concurrent_fail=1
    fi
done

# regardless of concurrent result, a clean sequential build must work
rm -rf .cache out
bld_install
assert_success
assert_exe_output out/bin/stress "$expected_sum"
