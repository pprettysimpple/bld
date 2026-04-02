#!/bin/bash
set -euo pipefail
: "${BLD_ROOT:?BLD_ROOT must be set}"

WORKDIR=""
BUILD_OUTPUT=""
BUILD_EXIT=0

setup_workdir() {
    WORKDIR="$(mktemp -d "/tmp/bld-test-$(basename "$TEST_DIR")-XXXXXX")"
    trap 'rm -rf "$WORKDIR"' EXIT
    cp "$TEST_DIR/build.c" "$WORKDIR/"
    [ -d "$TEST_DIR/src" ] && cp -r "$TEST_DIR/src" "$WORKDIR/"
    ln -sf "$BLD_ROOT/bld.h" "$WORKDIR/bld.h"
    ln -sf "$BLD_ROOT/bld" "$WORKDIR/bld"
    cd "$WORKDIR"
}

bld_bootstrap() {
    cc -std=c11 -w build.c -o b -lpthread 2>&1 || die "bootstrap failed"
}

bld_install() {
    BUILD_EXIT=0
    BUILD_OUTPUT="$(./b install -j4 --prefix out "$@" 2>&1)" || BUILD_EXIT=$?
}

die() {
    echo "FAIL: $*" >&2
    [ -n "$BUILD_OUTPUT" ] && echo "$BUILD_OUTPUT" >&2
    exit 1
}

assert_success() { [ "$BUILD_EXIT" -eq 0 ] || die "build failed (exit=$BUILD_EXIT)"; }
assert_file_exists() { [ -f "$1" ] || die "missing: $1"; }

assert_exe_output() {
    local out
    out="$("$1" 2>&1)" || die "$1 crashed"
    echo "$out" | grep -q "$2" || die "$1: expected '$2', got '$out'"
}

assert_no_recompilation() {
    if echo "$BUILD_OUTPUT" | grep -qP '\[\d+/\d+\] .*\.o$'; then
        die "unexpected recompilation"
    fi
}

assert_recompiled() {
    if ! echo "$BUILD_OUTPUT" | grep -qP "\[\d+/\d+\] .*$1"; then
        die "expected recompilation of '$1'"
    fi
}

assert_not_recompiled() {
    if echo "$BUILD_OUTPUT" | grep -qP "\[\d+/\d+\] .*$1"; then
        die "unexpected recompilation of '$1'"
    fi
}

replace_in_file() { sed -i "s|$2|$3|g" "$1"; }
