#!/bin/bash
# tests/run_all.sh — enumerate and run all bld.h integration tests
#
# Usage: bash tests/run_all.sh [BLD_ROOT]

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export BLD_ROOT="${1:-$(cd "$SCRIPT_DIR/.." && pwd)}"

passed=0
failed=0
failures=()

for test_dir in "$SCRIPT_DIR"/test_*/; do
    [ -f "$test_dir/test.sh" ] || continue
    test_name="$(basename "$test_dir")"

    # detect nix shell variants
    shells=()
    if [ -f "$test_dir/shell.nix" ] && command -v nix-shell &>/dev/null; then
        shells+=("$test_dir/shell.nix")
    fi
    for variant in "$test_dir"/shell-*.nix; do
        [ -f "$variant" ] && command -v nix-shell &>/dev/null && shells+=("$variant")
    done

    if [ ${#shells[@]} -eq 0 ]; then
        # run directly without nix
        if bash "$test_dir/test.sh" 2>&1; then
            echo "[PASS] $test_name"
            ((passed++))
        else
            echo "[FAIL] $test_name"
            ((failed++))
            failures+=("$test_name")
        fi
    else
        # run once per nix shell variant
        for shell_nix in "${shells[@]}"; do
            variant_name="$(basename "$shell_nix" .nix)"
            label="$test_name ($variant_name)"
            if nix-shell "$shell_nix" --run "bash '$test_dir/test.sh'" 2>&1; then
                echo "[PASS] $label"
                ((passed++))
            else
                echo "[FAIL] $label"
                ((failed++))
                failures+=("$label")
            fi
        done
    fi
done

echo ""
total=$((passed + failed))
if [ "$failed" -eq 0 ]; then
    echo "$passed/$total tests passed"
    exit 0
else
    echo "$passed/$total tests passed, $failed failed:"
    for f in "${failures[@]}"; do
        echo "  - $f"
    done
    exit 1
fi
