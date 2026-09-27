#!/bin/sh
set -eu

rom_dir=${1:-}
if [ -z "$rom_dir" ]; then
    echo "usage: $0 ROM_DIR" >&2
    exit 2
fi

if [ ! -d "$rom_dir" ]; then
    echo "ROM directory not found: $rom_dir" >&2
    exit 2
fi

check_rom()
{
    name=$1
    size=$2
    sha256=$3
    path="$rom_dir/$name"

    if [ ! -f "$path" ]; then
        echo "MISSING: $name" >&2
        return 1
    fi

    actual_size=$(wc -c < "$path" | tr -d ' ')
    if [ "$actual_size" -ne "$size" ]; then
        echo "SIZE MISMATCH: $name (expected $size, got $actual_size)" >&2
        return 1
    fi

    actual_sha=$(sha256sum "$path" | awk '{print $1}')
    if [ "$actual_sha" != "$sha256" ]; then
        echo "HASH MISMATCH: $name" >&2
        echo "  expected: $sha256" >&2
        echo "  actual:   $actual_sha" >&2
        return 1
    fi

    echo "OK: $name ($size bytes)"
}

status=0

check_rom 213-p1.p1 2097152 b9ff8f07e59a7a24aa69248d1578ab0b53bbcc4ce9558614fcfa573c544d84c5 || status=1
check_rom 213-s1.s1 131072 62951bf18d21425159e3163f858997fd840c3f3de342ae5b79bc6a98825711ad || status=1
check_rom 213-c1.c1 4194304 c728a599d327d3ba49996c32cdbaa6e5af18288d8525bc8d86c693d80e241b89 || status=1
check_rom 213-c2.c2 4194304 84d4ef916ce24cf26b404a0cfb58d69e2c6f76f87bcc69275f01334bd58a95bd || status=1
check_rom 213-m1.m1 131072 b99efaad3bbc4826586afb7d81f3d0c0ddf9a502ce6be81fac7c99dffb1316f2 || status=1
check_rom 213-v1.v1 2097152 b25c5a80ae9e9c09585758758703ca54351ac7b33c84212a5105f3e03fd5e0d2 || status=1
check_rom 213-v2.v2 2097152 4592df62d0bae0204c97ec2d6a72833d9eea320de12b3d4bfa61e7bd52dbff57 || status=1

if [ "$status" -ne 0 ]; then
    echo "Neo Drift Out ROM validation FAILED." >&2
    exit 1
fi

echo "Neo Drift Out ROM validation passed."
