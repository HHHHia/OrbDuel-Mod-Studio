#!/bin/bash
set -euo pipefail
root="$(cd "$(dirname "$0")" && pwd)"
if [[ ! -x "$root/build-macos/bin/orbduel" ]]; then /bin/bash "$root/tools/build-macos.sh"; fi
cd "$root"
exec "$root/build-macos/bin/orbduel" --content "$root/content" --mods "$root/build-macos/bin/mods" "$@"
