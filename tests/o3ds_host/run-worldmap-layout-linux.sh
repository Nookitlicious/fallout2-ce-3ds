#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
g++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -no-pie -I tests/o3ds_host/rectmap_stubs -I src tests/o3ds_host/worldmap_layout.cc src/platform/ctr/ctr_rectmap.cc -o "$tmp/worldmap_layout"
"$tmp/worldmap_layout"
