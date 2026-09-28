#!/bin/sh
set -eu

ROOT=${1:-games/neodriftout/src/autorecomp}

if [ ! -d "$ROOT" ]; then
    echo "decomp audit: directory not found: $ROOT" >&2
    exit 2
fi

todo_records=$(
    grep -Rho 'TODO \$[0-9A-Fa-f]*: .*' "$ROOT" --include='*.c' 2>/dev/null |
    grep -v ': dc.w ' |
    grep -v ': sbcd -(a[0-7]), -(a[0-7]), #\$[0-9A-Fa-f]* ' |
    grep -v ': chk.w (a1)+, d0 ' || true
)

unhandled_count=$(grep -Rho 'UNHANDLED_[A-Z_]*' "$ROOT" --include='*.c' 2>/dev/null | sort | uniq -c || true)
todo_count=$(printf '%s\n' "$todo_records" | sed '/^$/d' | wc -l | tr -d ' ')

echo "Neo Drift Out decompilation coverage audit"
echo "=========================================="
echo "Generated source: $ROOT"
echo "Untranslated instruction TODOs: $todo_count"
echo "Raw dc.w data records: $(grep -Rho 'TODO \$[0-9A-Fa-f]*: dc.w ' "$ROOT" --include='*.c' 2>/dev/null | wc -l | tr -d ' ')"
echo "Generated SBCD/CHK jump-table records: $(grep -Rho 'TODO \$[0-9A-Fa-f]*: sbcd -(a[0-7]), -(a[0-7]), #\$[0-9A-Fa-f]* ' "$ROOT" --include='*.c' 2>/dev/null | wc -l | tr -d ' ') + $(grep -Rho 'TODO \$[0-9A-Fa-f]*: chk.w (a1)+, d0 ' "$ROOT" --include='*.c' 2>/dev/null | wc -l | tr -d ' ')"

if [ "$todo_count" -gt 0 ]; then
    echo
    echo "Untranslated instruction classes:"
    printf '%s\n' "$todo_records" |
        sed 's/^TODO \$[0-9A-Fa-f]*: //' | awk '{print $1}' |
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
echo "Audit completed after the deterministic decomp patch stage."
echo "dc.w and recognized SBCD/CHK jump-table records are reported separately because they are generated data, not untranslated instructions."
