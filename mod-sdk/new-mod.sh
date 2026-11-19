#!/bin/bash
set -euo pipefail
sdk="$(cd "$(dirname "$0")" && pwd)"
id=""; kind="vortex"; name=""; destination=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --id) id="$2"; shift 2 ;;
    --kind) kind="$2"; shift 2 ;;
    --name) name="$2"; shift 2 ;;
    --destination) destination="$2"; shift 2 ;;
    *) echo "Usage: bash new-mod.sh --id my-mod [--kind vortex|summon|ability|rewind|guardian] [--name name] [--destination path]"; exit 2 ;;
  esac
done
if [[ -z "$id" ]]; then read -r -p "Mod ID (for example my-summons): " id; fi
if [[ -z "$name" ]]; then name="$id"; fi
if [[ -z "$destination" ]]; then destination="$sdk/../user/mod-projects/$id"; fi
command -v cmake >/dev/null || { echo "Install CMake first: brew install cmake"; exit 1; }
cmake "-DID=$id" "-DKIND=$kind" "-DNAME=$name" "-DDESTINATION=$destination" -P "$sdk/new-mod.cmake"
