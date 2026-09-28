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
    grep -v ': chk.w ' || true
)

todo_count=$(printf '%s\n' "$todo_records" | sed '/^$/d' | wc -l | tr -d ' ')

unhandled_data=0
unhandled_code=0
for file in $(find "$ROOT" -type f -name '*.c' -print); do
    counts=$(awk '
        function flush() {
            if (function_name == "") return;
            if (data_function) data += unhandled;
            else code += unhandled;
        }
        /^void / {
            flush();
            function_name = $0;
            sub(/^void /, "", function_name);
            sub(/\(.*/, "", function_name);
            data_function = (function_name ~ /^jt_/);
            unhandled = 0;
        }
        /UNHANDLED_[A-Z_]+/ { unhandled++; }
        /WARNING: function did not end with RTS/ { data_function = 1; }
        END {
            flush();
            printf "%d %d", data + 0, code + 0;
        }
    ' "$file")
    set -- $counts
    unhandled_data=$((unhandled_data + $1))
    unhandled_code=$((unhandled_code + $2))
done

echo "Neo Drift Out decompilation coverage audit"
echo "=========================================="
echo "Generated source: $ROOT"
echo "Untranslated instruction TODOs: $todo_count"
echo "Raw dc.w data records: $(grep -Rho 'TODO \$[0-9A-Fa-f]*: dc.w ' "$ROOT" --include='*.c' 2>/dev/null | wc -l | tr -d ' ')"
echo "Generated SBCD/CHK jump-table records: $(grep -Rho 'TODO \$[0-9A-Fa-f]*: sbcd -(a[0-7]), -(a[0-7]), #\$[0-9A-Fa-f]* ' "$ROOT" --include='*.c' 2>/dev/null | wc -l | tr -d ' ') + $(grep -Rho 'TODO \$[0-9A-Fa-f]*: chk.w ' "$ROOT" --include='*.c' 2>/dev/null | wc -l | tr -d ' ')"
echo "Unhandled expressions in generated non-RTS/jump-table fragments: $unhandled_data"
echo "Unhandled expressions in executable functions: $unhandled_code"

if [ "$todo_count" -gt 0 ]; then
    echo
    echo "Untranslated instruction classes:"
    printf '%s\n' "$todo_records" |
        sed 's/^TODO \$[0-9A-Fa-f]*: //' |
        awk '{print $1}' |
        sort |
        uniq -c |
        sort -nr
fi

if [ "$unhandled_code" -gt 0 ]; then
    echo
    echo "Unhandled operand classes in executable functions:"
    awk '
        /^void / { in_jt = ($0 ~ /^void jt_/); }
        /UNHANDLED_[A-Z_]+/ && !in_jt {
            match($0, /UNHANDLED_[A-Z_]+/);
            count[substr($0, RSTART, RLENGTH)]++;
        }
        END {
            for (name in count) print count[name], name;
        }
    ' $(find "$ROOT" -type f -name '*.c' -print) | sort -nr
fi

echo
echo "Audit completed after the deterministic decomp patch stage."
echo "dc.w, SBCD/CHK jump-table records, and generated non-RTS/jump-table unhandled operands are reported separately because they are generated data, not executable decomp gaps."
