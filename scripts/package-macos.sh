#!/usr/bin/env bash
set -euo pipefail

qt_prefix="${1:-${QT_PREFIX:-}}"
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
dist_dir="$repo_root/dist"

if [[ -z "$qt_prefix" ]]; then
    echo "Qt prefix is required." >&2
    echo "Pass a Qt path as the first argument, or set QT_PREFIX." >&2
    exit 1
fi

if [[ ! -d "$qt_prefix" ]]; then
    echo "Qt prefix not found: $qt_prefix" >&2
    echo "Pass a Qt path as the first argument, or set QT_PREFIX." >&2
    exit 1
fi

cd "$repo_root"

cmake --preset qt6.11-macos-release -DCMAKE_PREFIX_PATH="$qt_prefix"
cmake --build --preset qt6.11-macos-release
cmake --build --preset qt6.11-macos-release --target all_qmllint

mkdir -p "$dist_dir"
cpack --preset qt6.11-macos-dmg

echo "macOS package written under $dist_dir"
