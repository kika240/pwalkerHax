#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ $# != 3 ]]; then echo 'Usage: tests/test-firmware.sh POKESTROLLER_SOURCE ROM EEPROM' >&2; exit 2; fi
task_emulator="$1"; task_rom="$2"; task_eeprom="$3"
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/pw-firmware-test.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
task_flags=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-strict-aliasing)
clang -std=gnu11 "${task_flags[@]}" -Wno-unused-variable -Itests/stubs -Iinclude -c source/pokewalker.c -o "$task_dir/protocol.o"
clang -std=gnu11 "${task_flags[@]}" -Itests/stubs -Iinclude -c tests/protocol_stubs.c -o "$task_dir/stubs.o"
task_sources=()
while IFS= read -r source; do task_sources+=("$source"); done < <(find "$task_emulator/third_party/pocketwalker/core" -name '*.cpp')
clang++ -std=c++2b "${task_flags[@]}" -Itests/stubs -Iinclude -I"$task_emulator/src" -I"$task_emulator/third_party/pocketwalker" \
    tests/firmware_import_test.cpp "$task_emulator/src/emulator.cpp" "${task_sources[@]}" "$task_dir/protocol.o" "$task_dir/stubs.o" -o "$task_dir/test"
"$task_dir/test" "$task_rom" "$task_eeprom"
