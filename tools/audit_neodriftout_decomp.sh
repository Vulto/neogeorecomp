#!/bin/sh
set -eu

ROOT=${1:-games/neodriftout/src/autorecomp}

if [ ! -d "$ROOT" ]; then
    echo "decomp audit: directory not found: $ROOT" >&2
    exit 2
fi

todo_count=$(grep -Rho 'TODO \$[0-9A-Fa-f]*: [^ ]*' "$ROOT" --include='*.c' 2>/dev/null | wc -l)
unhandled_count=$(grep -Rho 'UNHANDLED_[A-Z_]*' "$ROOT" --include='*.c' 2>/dev/null | sort | uniq -c || true)

echo "Neo Drift Out decompilation coverage audit"
echo "=========================================="
echo "Generated source: $ROOT"
echo "TODO instructions: $todo_count"

if [ "$todo_count" -gt 0 ]; then
    echo
    echo "Untranslated instruction classes:"
    grep -Rho 'TODO \$[0-9A-Fa-f]*: [^ ]*' "$ROOT" --include='*.c' 2>/dev/null |
        sed 's/.*: //' |
        sort |
        uniq -c |
        sort -nr
fi

if [ -n "$unhandled_count" ]; then
    echo
    echo "Unhandled operand expressions:"
    echo "$unhandled_count"
fi

echo
echo "Audit completed. TODOs are reported, not treated as a successful decompilation."
