#!/bin/bash
cd "$(dirname "$0")" || exit 1
/bin/bash ./tools/build-macos.sh "$@"
result=$?
read -r -p "Press Enter to close..." _
exit "$result"
