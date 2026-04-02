#!/bin/bash
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$TEST_DIR/../common.sh"

ROUNDS=30
CLEAN_CHECK_EVERY=10

setup_workdir
bash "$TEST_DIR/gen_sources.sh"
bld_bootstrap

# reference build
bld_install
assert_success

ref_hash=$(find out -type f | sort | xargs md5sum | md5sum | cut -d' ' -f1)

# backup sources
mkdir -p .src_backup
find . -path ./.src_backup -prune -o \( -name '*.c' -o -name '*.cpp' -o -name '*.h' \) -print | while read f; do
    mkdir -p ".src_backup/$(dirname "$f")"
    cp "$f" ".src_backup/$f"
done

mapfile -t ALL_SOURCES < <(find . -path ./.src_backup -prune -o \( -name '*.c' -o -name '*.cpp' \) -print | grep -v .src_backup)

get_output_hash() {
    find out -type f | sort | xargs md5sum | md5sum | cut -d' ' -f1
}

for round in $(seq 1 "$ROUNDS"); do
    # mutate 1..8 random files
    n=$(( RANDOM % 8 + 1 ))
    targets=$(printf '%s\n' "${ALL_SOURCES[@]}" | shuf | head -n "$n")
    for f in $targets; do echo "/* r=$round */" >> "$f"; done

    # incremental build
    bld_install
    assert_success
    inc_hash=$(get_output_hash)

    # periodically verify incremental == clean
    if [ $((round % CLEAN_CHECK_EVERY)) -eq 0 ]; then
        rm -rf .cache out
        bld_install
        assert_success
        clean_hash=$(get_output_hash)
        [ "$inc_hash" = "$clean_hash" ] || die "round $round: incremental != clean"
    fi

    # revert
    for f in $targets; do cp ".src_backup/$f" "$f"; done

    # incremental back to original
    bld_install
    assert_success
    cur_hash=$(get_output_hash)
    [ "$cur_hash" = "$ref_hash" ] || die "round $round: revert hash mismatch"
done

# kill mid-build + recovery
(
    set +e
    actual_kills=0
    for round in $(seq 1 20); do
        rm -rf .cache out 2>/dev/null
        ./b install -j4 --prefix out >/dev/null 2>&1 &
        pid=$!
        sleep "0.$((RANDOM % 70 + 30))"
        if kill -9 "$pid" 2>/dev/null; then
            wait "$pid" 2>/dev/null
            ((actual_kills++))
        else
            wait "$pid" 2>/dev/null
        fi
        rm -rf .cache/tmp .cache 2>/dev/null
        bld_bootstrap

        BUILD_EXIT=0
        BUILD_OUTPUT="$(./b install -j4 --prefix out 2>&1)" || BUILD_EXIT=$?
        if [ "$BUILD_EXIT" -ne 0 ]; then
            echo "FAIL: kill round $round: recovery failed" >&2
            echo "$BUILD_OUTPUT" >&2
            exit 1
        fi
        cur_hash=$(find out -type f | sort | xargs md5sum | md5sum | cut -d' ' -f1)
        if [ "$cur_hash" != "$ref_hash" ]; then
            echo "FAIL: kill round $round: hash mismatch" >&2
            exit 1
        fi
    done
    if [ "$actual_kills" -eq 0 ]; then
        echo "FAIL: no builds were actually killed" >&2
        exit 1
    fi
) || exit 1
