#!/bin/bash
# tests/common.sh — shared test helpers for bld.h integration tests
#
# Usage: source this from test.sh after setting TEST_DIR
#   TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
#   source "$TEST_DIR/../common.sh"

set -euo pipefail

: "${BLD_ROOT:?BLD_ROOT must be set}"

WORKDIR=""
BUILD_OUTPUT=""
BUILD_EXIT=0

# ---- Setup / Teardown ----

setup_workdir() {
    local test_name
    test_name="$(basename "$TEST_DIR")"
    WORKDIR="$(mktemp -d "/tmp/bld-test-${test_name}-XXXXXX")"
    trap cleanup EXIT

    # copy test project into workdir
    cp "$TEST_DIR/build.c" "$WORKDIR/"
    if [ -d "$TEST_DIR/src" ]; then
        cp -r "$TEST_DIR/src" "$WORKDIR/"
    fi

    # symlink bld.h and bld/ directory for includes
    ln -sf "$BLD_ROOT/bld.h" "$WORKDIR/bld.h"
    ln -sf "$BLD_ROOT/bld" "$WORKDIR/bld"

    cd "$WORKDIR"
}

cleanup() {
    if [ -n "$WORKDIR" ] && [ -d "$WORKDIR" ]; then
        rm -rf "$WORKDIR"
    fi
}

# ---- Build helpers ----

bld_bootstrap() {
    cc -std=c11 -w build.c -o b -lpthread 2>&1 || die "bootstrap failed"
}

bld_install() {
    BUILD_EXIT=0
    BUILD_OUTPUT="$(./b install --prefix out "$@" 2>&1)" || BUILD_EXIT=$?
}

# ---- Output parsing ----
# bld output: "bld done: N built, M cached, Xs, YKB arena"
# or just:    "bld done: M cached, Xs, YKB arena" (when nothing built)

bld_count_built() {
    local n
    n=$(echo "$BUILD_OUTPUT" | grep -oP '\d+(?= built)' || echo "")
    echo "${n:-0}"
}

bld_count_cached() {
    local n
    n=$(echo "$BUILD_OUTPUT" | grep -oP '\d+(?= cached)' || echo "")
    echo "${n:-0}"
}

# ---- Assertions ----

die() {
    echo "FAIL: $*" >&2
    if [ -n "$BUILD_OUTPUT" ]; then
        echo "--- build output ---" >&2
        echo "$BUILD_OUTPUT" >&2
        echo "---" >&2
    fi
    exit 1
}

assert_success() {
    [ "$BUILD_EXIT" -eq 0 ] || die "expected build to succeed (exit=$BUILD_EXIT)"
}

assert_failure() {
    [ "$BUILD_EXIT" -ne 0 ] || die "expected build to fail"
}

assert_file_exists() {
    [ -f "$1" ] || die "file does not exist: $1"
}

assert_built() {
    local expected="$1"
    local actual
    actual="$(bld_count_built)"
    [ "$actual" -eq "$expected" ] || die "expected $expected built, got $actual"
}

assert_cached() {
    local expected="$1"
    local actual
    actual="$(bld_count_cached)"
    [ "$actual" -eq "$expected" ] || die "expected $expected cached, got $actual"
}

assert_exe_output() {
    local exe="$1"
    local pattern="$2"
    local out
    out="$("$exe" 2>&1)" || die "executable $exe failed"
    echo "$out" | grep -q "$pattern" || die "output of $exe does not match '$pattern', got: $out"
}

assert_no_recompilation() {
    # check that no .o compilation steps ran (install steps are expected)
    if echo "$BUILD_OUTPUT" | grep -qP '\[\d+/\d+\] .*\.o$'; then
        die "unexpected recompilation detected"
    fi
}

assert_recompiled() {
    local pattern="$1"
    if ! echo "$BUILD_OUTPUT" | grep -qP "\[\d+/\d+\] .*${pattern}"; then
        die "expected recompilation of '$pattern'"
    fi
}

assert_not_recompiled() {
    local pattern="$1"
    if echo "$BUILD_OUTPUT" | grep -qP "\[\d+/\d+\] .*${pattern}"; then
        die "unexpected recompilation of '$pattern'"
    fi
}

# ---- File mutation ----

modify_file() {
    local file="$1"
    shift
    echo "$*" >> "$file"
}

replace_in_file() {
    local file="$1"
    local old="$2"
    local new="$3"
    sed -i "s|$old|$new|g" "$file"
}
