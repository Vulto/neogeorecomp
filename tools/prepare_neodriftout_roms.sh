#!/bin/sh
set -eu

input=${1:-}
output=${2:-}

if [ -z "$input" ] || [ -z "$output" ]; then
    echo "usage: $0 ROM_ZIP_OR_DIR OUTPUT_DIR" >&2
    exit 2
fi

if [ ! -e "$input" ]; then
    echo "ROM source not found: $input" >&2
    exit 2
fi

mkdir -p "$output"

roms='
000-lo.lo
213-p1.p1
213-s1.s1
213-c1.c1
213-c2.c2
213-m1.m1
213-v1.v1
213-v2.v2
'

if [ -d "$input" ]; then
    for name in $roms; do
        if [ ! -f "$input/$name" ]; then
            echo "MISSING: $name" >&2
            exit 1
        fi
        cp "$input/$name" "$output/$name"
    done
else
    case "$input" in
        *.zip|*.ZIP) ;;
        *)
            echo "ROM source must be a directory or .zip file: $input" >&2
            exit 2
            ;;
    esac

    command -v unzip >/dev/null 2>&1 || {
        echo "unzip is required to prepare a ROM ZIP." >&2
        exit 2
    }

    tmp=$(mktemp -d)
    trap 'rm -rf "$tmp"' EXIT HUP INT TERM

    unzip -j -q -o "$input" \
        '000-lo.lo' \
        '213-p1.p1' \
        '213-s1.s1' \
        '213-c1.c1' \
        '213-c2.c2' \
        '213-m1.m1' \
        '213-v1.v1' \
        '213-v2.v2' \
        -d "$tmp"

    for name in $roms; do
        if [ ! -f "$tmp/$name" ]; then
            echo "MISSING from archive: $name" >&2
            exit 1
        fi
        cp "$tmp/$name" "$output/$name"
    done
fi

echo "Prepared Neo Drift Out ROM set in: $output"
