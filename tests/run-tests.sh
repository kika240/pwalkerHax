#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/pw-patch-test.XXXXXX")
trap 'rm -rf "$task_dir"' EXIT
${CC:-clang} -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -Iinclude \
    source/save_patch.c tests/save_patch_test.c -o "$task_dir/tests"
"$task_dir/tests" "$@"

${CC:-clang} -std=c11 -O1 -g -Wall -Wextra -Wno-unused-variable -fsanitize=address,undefined -fno-sanitize-recover=all -Itests/stubs -Iinclude \
    source/save_patch.c source/save_import.c tests/import_flow_test.c -o "$task_dir/flow"
if (cd "$task_dir" && ./flow > flow.log); then
    tail -n 1 "$task_dir/flow.log"
else
    cat "$task_dir/flow.log"
    exit 1
fi
