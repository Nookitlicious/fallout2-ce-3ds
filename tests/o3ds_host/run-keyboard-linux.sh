#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
{ cat tests/o3ds_host/keyboard_mock_prefix.h; sed -n '/^\/\/ Fallout.s legacy text fields/,/^void ctr_input_init(void)/{ /^void ctr_input_init(void)/d; p; }' src/platform/ctr/ctr_input.cc; cat tests/o3ds_host/keyboard_mock_tests.inc; } > "$tmp/test.cc"
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie "$tmp/test.cc" -o "$tmp/test"
"$tmp/test"
