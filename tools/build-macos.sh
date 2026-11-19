#!/bin/bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
[[ "$(uname -s)" == Darwin ]] || { echo "Use build.ps1 on Windows."; exit 1; }
if [[ -x /opt/homebrew/bin/brew ]]; then eval "$(/opt/homebrew/bin/brew shellenv)"; fi
xcode-select -p >/dev/null 2>&1 || { echo "First run: xcode-select --install"; exit 1; }
if ! command -v cmake >/dev/null || ! command -v pkg-config >/dev/null || ! pkg-config --exists sdl2 SDL2_image SDL2_ttf; then
  command -v brew >/dev/null || { echo "Install Homebrew (https://brew.sh), then rerun this script."; exit 1; }
  brew install cmake pkgconf sdl2-compat sdl2_image sdl2_ttf
fi
cmake -S "$root" -B "$root/build-macos" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF "-DCMAKE_OSX_ARCHITECTURES=$(uname -m)"
cmake --build "$root/build-macos" --config Release --parallel
echo "Built C++ game: $root/build-macos/bin/orbduel"
echo "Open Play.command to play. See docs/MACOS.md to create mods."
