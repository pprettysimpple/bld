#!/bin/bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export BLD_ROOT="${1:-$(cd "$SCRIPT_DIR/.." && pwd)}"

passed=0 failed=0 failures=()

for test_dir in "$SCRIPT_DIR"/test_*/; do
    [ -f "$test_dir/test.sh" ] || continue
    name="$(basename "$test_dir")"

    if bash "$test_dir/test.sh" 2>&1; then
        echo "[PASS] $name"
        ((passed++))
    else
        echo "[FAIL] $name"
        ((failed++))
        failures+=("$name")
    fi
done

echo ""
total=$((passed + failed))
if [ "$failed" -eq 0 ]; then
    echo "$passed/$total passed"
else
    echo "$passed/$total passed, $failed failed:"
    printf '  - %s\n' "${failures[@]}"
    exit 1
fi
