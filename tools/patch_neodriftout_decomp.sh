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
    before=$(grep -Ec 'TODO \$[0-9A-Fa-f]+: rox[rl]\.[bwl] ' "$file" || true)

    if [ "$before" -eq 0 ]; then
        continue
    fi

    perl -0pi -e '
        s{/\* TODO \$[0-9A-Fa-f]+: roxr\.b #\$([0-9]+), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXR8(g_m68k.d[$2], $1);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxr\.w #\$([0-9]+), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXR16(g_m68k.d[$2], $1);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxr\.l #\$([0-9]+), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXR32(g_m68k.d[$2], $1);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxl\.b #\$([0-9]+), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXL8(g_m68k.d[$2], $1);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxl\.w #\$([0-9]+), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXL16(g_m68k.d[$2], $1);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxl\.l #\$([0-9]+), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXL32(g_m68k.d[$2], $1);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxr\.b d([0-7]), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXR8(g_m68k.d[$2], g_m68k.d[$1]);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxr\.w d([0-7]), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXR16(g_m68k.d[$2], g_m68k.d[$1]);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxr\.l d([0-7]), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXR32(g_m68k.d[$2], g_m68k.d[$1]);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxl\.b d([0-7]), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXL8(g_m68k.d[$2], g_m68k.d[$1]);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxl\.w d([0-7]), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXL16(g_m68k.d[$2], g_m68k.d[$1]);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxl\.l d([0-7]), d([0-7])\s+\[[^]]+\] \*/\s*(?:\n\s*M68K_OR8\(g_m68k\.d\[1\], g_m68k\.d\[0\]\);)?}{M68K_ROXL32(g_m68k.d[$2], g_m68k.d[$1]);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxr\.w \$([0-9A-Fa-f]+)\(a([0-7])\)\s+\[[^]]+\] \*/}{{ uint32_t _ea = (g_m68k.a[$2] + 0x$1); uint16_t _tmp = bus_read16(_ea); M68K_ROXR16(_tmp, 1); bus_write16(_ea, _tmp); }}g;
        s{/\* TODO \$[0-9A-Fa-f]+: roxl\.w \$([0-9A-Fa-f]+)\(a([0-7])\)\s+\[[^]]+\] \*/}{{ uint32_t _ea = (g_m68k.a[$2] + 0x$1); uint16_t _tmp = bus_read16(_ea); M68K_ROXL16(_tmp, 1); bus_write16(_ea, _tmp); }}g;
    ' "$file"

    after=$(grep -Ec 'TODO \$[0-9A-Fa-f]+: rox[rl]\.[bwl] ' "$file" || true)
    if [ "$after" -ge "$before" ]; then
        echo "decomp patch: failed to replace ROX instructions in $file" >&2
        exit 1
    fi

    changed=$((changed + before - after))
done

echo "decomp patch: replaced $changed Neo Drift Out ROXR/ROXL instructions"
