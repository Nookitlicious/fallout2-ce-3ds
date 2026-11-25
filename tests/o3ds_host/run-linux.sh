#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
root="$PWD"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -ffunction-sections -fdata-sections -Ithird_party/fpattern -c third_party/fpattern/fpattern.c -o "$tmp/fpattern.o"
g++ -DO3DS_TEST_ARCHIVE_POOL -DO3DS_TEST_V7 -DO3DS_TEST_V8 -std=c++17 -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -ffunction-sections -fdata-sections -I tests/o3ds_host -I src -I third_party/fpattern tests/o3ds_host/regression.cc src/memory.cc src/db.cc src/xfile.cc src/dfile.cc src/file_find.cc src/file_utils.cc src/platform_compat.cc "$tmp/fpattern.o" -no-pie -Wl,--gc-sections -l:libSDL2-2.0.so.0 -lz -o "$tmp/regression"
cd "$tmp"
./regression
