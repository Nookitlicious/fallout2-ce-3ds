#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -g -O1 -fsanitize=address,undefined -ffunction-sections -fdata-sections -Ithird_party/fpattern -c third_party/fpattern/fpattern.c -o "$tmp/fpattern.o"
g++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections -I tests/o3ds_host -I src -I third_party/fpattern tests/o3ds_host/performance.cc src/art.cc src/cache.cc src/heap.cc src/memory.cc src/db.cc src/xfile.cc src/dfile.cc src/file_find.cc src/platform_compat.cc "$tmp/fpattern.o" -Wl,--gc-sections -l:libSDL2-2.0.so.0 -lz -o "$tmp/performance"
cd "$tmp"
./performance
