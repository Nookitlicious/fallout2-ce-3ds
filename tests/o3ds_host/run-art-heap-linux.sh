#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -ffunction-sections -fdata-sections -Ithird_party/fpattern -c third_party/fpattern/fpattern.c -o "$tmp/fpattern.o"
g++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -ffunction-sections -fdata-sections -Itests/o3ds_host -Isrc -Ithird_party/fpattern tests/o3ds_host/art_heap.cc src/art.cc src/heap.cc src/memory.cc src/db.cc src/xfile.cc src/dfile.cc src/file_find.cc src/file_utils.cc src/platform_compat.cc "$tmp/fpattern.o" -no-pie -Wl,--gc-sections -l:libSDL2-2.0.so.0 -lz -o "$tmp/art_heap"
cd "$tmp"
./art_heap
