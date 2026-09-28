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
    before=$(grep -Ec 'TODO \$[0-9A-Fa-f]+: (rox[rl]\.[bwl] |sbcd\.b |nbcd\.b |movep\.|bftst |abcd\.b )' "$file" || true)

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
        s{/\* TODO \$[0-9A-Fa-f]+: sbcd\.b d([0-7]), d([0-7])\s+\[[^]]+\] \*/}{M68K_SBCD8(g_m68k.d[$2], g_m68k.d[$1]);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: nbcd\.b d([0-7])\s+\[[^]]+\] \*/}{M68K_NBCD8(g_m68k.d[$1]);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: nbcd\.b \(a([0-7])\)\+\s+\[[^]]+\] \*/}{{ uint32_t _ea = g_m68k.a[$1]; uint8_t _tmp = bus_read8(_ea); M68K_NBCD8(_tmp); bus_write8(_ea, _tmp); g_m68k.a[$1] += 1; }}g;
        s{/\* TODO \$[0-9A-Fa-f]+: movep\.w \$([0-9A-Fa-f]+)\(a([0-7])\), d([0-7])\s+\[[^]]+\] \*/}{M68K_MOVEP16_MEM_TO_REG(g_m68k.d[$3], g_m68k.a[$2] + 0x$1);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: movep\.l \$([0-9A-Fa-f]+)\(a([0-7])\), d([0-7])\s+\[[^]]+\] \*/}{M68K_MOVEP32_MEM_TO_REG(g_m68k.d[$3], g_m68k.a[$2] + 0x$1);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: movep\.w -\$([0-9A-Fa-f]+)\(a([0-7])\), d([0-7])\s+\[[^]]+\] \*/}{M68K_MOVEP16_MEM_TO_REG(g_m68k.d[$3], g_m68k.a[$2] - 0x$1);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: movep\.l -\$([0-9A-Fa-f]+)\(a([0-7])\), d([0-7])\s+\[[^]]+\] \*/}{M68K_MOVEP32_MEM_TO_REG(g_m68k.d[$3], g_m68k.a[$2] - 0x$1);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: movep\.w d([0-7]), \$([0-9A-Fa-f]+)\(a([0-7])\)\s+\[[^]]+\] \*/}{M68K_MOVEP16_REG_TO_MEM(g_m68k.d[$1], g_m68k.a[$3] + 0x$2);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: movep\.l d([0-7]), \$([0-9A-Fa-f]+)\(a([0-7])\)\s+\[[^]]+\] \*/}{M68K_MOVEP32_REG_TO_MEM(g_m68k.d[$1], g_m68k.a[$3] + 0x$2);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: movep\.w d([0-7]), -\$([0-9A-Fa-f]+)\(a([0-7])\)\s+\[[^]]+\] \*/}{M68K_MOVEP16_REG_TO_MEM(g_m68k.d[$1], g_m68k.a[$3] - 0x$2);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: movep\.l d([0-7]), -\$([0-9A-Fa-f]+)\(a([0-7])\)\s+\[[^]]+\] \*/}{M68K_MOVEP32_REG_TO_MEM(g_m68k.d[$1], g_m68k.a[$3] - 0x$2);}g;
        s{/\* TODO \$0FB688: bftst -\$20\(a0, a5\.w\)\{0:4\}\s+\[[^]]+\] \*/}{M68K_BFTST_MEMORY(g_m68k.a[0] + (int16_t)(uint16_t)g_m68k.a[5] - 0x20, 0, 4);}g;
        s{/\* TODO \$077D54: bftst \$0\.w\{0:32\}\s+\[[^]]+\] \*/}{M68K_BFTST_MEMORY(0x000000, 0, 32);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: abcd\.b d([0-7]), d([0-7])\s+\[[^]]+\] \*/}{M68K_ABCD8(g_m68k.d[$2], g_m68k.d[$1]);}g;
        s{/\* TODO \$[0-9A-Fa-f]+: abcd\.b -\(a([0-7])\), -\(a([0-7])\)\s+\[[^]]+\] \*/}{{ g_m68k.a[$1] -= 1; uint8_t _src = bus_read8(g_m68k.a[$1]); g_m68k.a[$2] -= 1; uint8_t _dst = bus_read8(g_m68k.a[$2]); M68K_ABCD8(_dst, _src); bus_write8(g_m68k.a[$2], _dst); }}g;
    ' "$file"

    after=$(grep -Ec 'TODO \$[0-9A-Fa-f]+: (rox[rl]\.[bwl] |sbcd\.b |nbcd\.b |movep\.|bftst |abcd\.b )' "$file" || true)
    if [ "$after" -ge "$before" ]; then
        echo "decomp patch: failed to replace ROX instructions in $file" >&2
        exit 1
    fi

    changed=$((changed + before - after))
done

echo "decomp patch: replaced $changed missing Neo Drift Out instructions"
