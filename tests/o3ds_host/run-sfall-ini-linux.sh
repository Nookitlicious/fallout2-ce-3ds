#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -ffunction-sections -fdata-sections -Ithird_party/fpattern -c third_party/fpattern/fpattern.c -o "$tmp/fpattern.o"
g++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -ffunction-sections -fdata-sections -Itests/o3ds_host -Isrc -Ithird_party/fpattern tests/o3ds_host/sfall_ini_test.cc src/sfall_ini.cc src/sfall_arrays.cc src/interpreter.cc src/config.cc src/dictionary.cc src/memory.cc src/memory_manager.cc src/db.cc src/xfile.cc src/dfile.cc src/file_find.cc src/platform_compat.cc "$tmp/fpattern.o" -no-pie -Wl,--gc-sections -l:libSDL2-2.0.so.0 -lz -o "$tmp/test"
cd "$tmp"
./test
