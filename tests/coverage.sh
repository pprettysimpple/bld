#!/bin/bash
# tests/coverage.sh — run all tests with coverage, generate lcov report
#
# Usage: bash tests/coverage.sh [BLD_ROOT]
# Requires: gcc, lcov, genhtml (nix-shell -p lcov)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export BLD_ROOT="${1:-$(cd "$SCRIPT_DIR/.." && pwd)}"
export COVERAGE=1

LCOV_OPTS="--rc branch_coverage=1 --ignore-errors inconsistent,inconsistent --ignore-errors negative,negative --ignore-errors deprecated,deprecated"

COV_DIR="$BLD_ROOT/.coverage"
rm -rf "$COV_DIR"
mkdir -p "$COV_DIR"

infos=()
workdirs=()

for test_dir in "$SCRIPT_DIR"/test_*/; do
    [ -f "$test_dir/test.sh" ] || continue
    test_name="$(basename "$test_dir")"

    # skip stress test — kill/corruption is incompatible with coverage
    if [ "$test_name" = "test_stress" ]; then
        echo "=== $test_name === (skipped in coverage mode)"
        continue
    fi

    echo "=== $test_name ==="
    if bash "$test_dir/test.sh" 2>&1; then
        echo "[PASS] $test_name"
    else
        echo "[FAIL] $test_name (continuing for coverage)"
    fi

    # find the workdir (COVERAGE=1 prevents cleanup)
    workdir=$(ls -dt /tmp/bld-test-${test_name}-* 2>/dev/null | head -1)
    if [ -z "$workdir" ]; then
        echo "  (workdir not found)"
        continue
    fi

    gcda_count=$(find "$workdir" -name '*.gcda' 2>/dev/null | wc -l)
    if [ "$gcda_count" -eq 0 ]; then
        echo "  (no .gcda files in $workdir)"
        continue
    fi

    workdirs+=("$workdir")

    # capture lcov info
    info_file="$COV_DIR/${test_name}.info"
    lcov --capture --directory "$workdir" \
         --output-file "$info_file" \
         --quiet $LCOV_OPTS 2>/dev/null || true

    if [ -f "$info_file" ]; then
        # normalize paths: /tmp/bld-test-XXX/bld/file.c → $BLD_ROOT/bld/file.c
        # this ensures coverage from different tests merges on the same source files
        sed -i "s|${workdir}/bld/|${BLD_ROOT}/bld/|g" "$info_file"
        sed -i "s|${workdir}/build.c|${BLD_ROOT}/tests/${test_name}/build.c|g" "$info_file"
        infos+=("$info_file")
    fi
done

if [ ${#infos[@]} -eq 0 ]; then
    echo "No coverage data collected"
    for wd in "${workdirs[@]}"; do rm -rf "$wd"; done
    exit 1
fi

# merge all info files
merge_args=()
for info in "${infos[@]}"; do
    merge_args+=("-a" "$info")
done

lcov "${merge_args[@]}" -o "$COV_DIR/merged.info" \
     --quiet $LCOV_OPTS

# filter: keep only bld source files, remove glibc headers and test build.c
lcov --extract "$COV_DIR/merged.info" \
     "*/bld/bld_core.h" "*/bld/bld_core_impl.c" "*/bld/bld_build.c" \
     "*/bld/bld_exec.c" "*/bld/bld_cli.c" \
     -o "$COV_DIR/bld.info" \
     --quiet $LCOV_OPTS 2>/dev/null || \
    cp "$COV_DIR/merged.info" "$COV_DIR/bld.info"

# generate HTML report
genhtml "$COV_DIR/bld.info" --output-directory "$COV_DIR/html" \
        --quiet $LCOV_OPTS \
        --prefix "$BLD_ROOT"

# print summary
echo ""
echo "=== Coverage Summary ==="
lcov --summary "$COV_DIR/bld.info" $LCOV_OPTS 2>&1 | grep -E 'lines|functions|branches'

# per-file breakdown
echo ""
echo "=== Per-file ==="
lcov --list "$COV_DIR/bld.info" $LCOV_OPTS 2>&1 | grep -E '\.c|\.h|===|Filename' | head -20

echo ""
echo "HTML report: $COV_DIR/html/index.html"

# cleanup workdirs
for wd in "${workdirs[@]}"; do
    rm -rf "$wd"
done
