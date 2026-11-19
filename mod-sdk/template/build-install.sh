#!/bin/bash
set -euo pipefail
project="$(cd "$(dirname "$0")" && pwd)"
[[ "$(uname -s)" == Darwin ]] || { echo "Use build-install.cmd on Windows."; exit 1; }
if [[ -x /opt/homebrew/bin/brew ]]; then eval "$(/opt/homebrew/bin/brew shellenv)"; fi
command -v cmake >/dev/null || { echo "Install CMake: brew install cmake"; exit 1; }
arch="$(uname -m)"
cmake -S "$project" -B "$project/build-macos" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=$arch"
cmake --build "$project/build-macos" --config Release --parallel
# 只在构建成功后安装本次 C++ 目标输出，避免复制陈旧包或不相关文件。
destination="$HOME/Library/Application Support/OrbDuel/OrbDuel/mods"
package="$(cat "$project/build-macos/orbduel-package.txt")"
cmake "-DPACKAGE_DIR=$package" "-DDESTINATION=$destination" -P "$project/install-mod.cmake"
echo "Installed. Return to the game Mod Workshop and reload."
