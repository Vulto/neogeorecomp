#!/bin/sh
set -eu

ROOT=${1:-games/neodriftout/src/autorecomp}

if [ ! -d "$ROOT" ]; then
    echo "decomp patch: directory not found: $ROOT" >&2
    exit 2
fi

files=$(find "$ROOT" -type f -name '*.c' -print)
changed=0

for file in $files; do
    before=$(grep -Ec 'TODO \$[0-9A-Fa-f]+: rox[rl]\.l ' "$file" || true)

    if [ "$before" -eq 0 ]; then
        continue
    fi

    perl -0pi -e '
        s{
            /\* TODO \$[0-9A-Fa-f]+: roxr\.l d([0-7]), d([0-7])\s+\[[^]]+\] \*/
            \n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);
        }{M68K_ROXR32(g_m68k.d[$2], g_m68k.d[$1]);}gx;

        s{
            /\* TODO \$[0-9A-Fa-f]+: roxl\.l #\$([0-9]+), d([0-7])\s+\[[^]]+\] \*/
            \n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);
        }{M68K_ROXL32(g_m68k.d[$2], $1);}gx;
    ' "$file"

    after=$(grep -Ec 'TODO \$[0-9A-Fa-f]+: rox[rl]\.l ' "$file" || true)
    if [ "$after" -ge "$before" ]; then
        echo "decomp patch: failed to replace ROX instructions in $file" >&2
        exit 1
    fi

    changed=$((changed + before - after))
done

echo "decomp patch: replaced $changed Neo Drift Out ROXR/ROXL instructions"
