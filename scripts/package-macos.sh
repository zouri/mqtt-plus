#!/usr/bin/env bash
set -euo pipefail

qt_prefix="${1:-${QT_PREFIX:-}}"
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/build/package-macos"
dist_dir="$repo_root/dist"
parallel_jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-4}"

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

cmake --fresh -S "$repo_root" -B "$build_dir" \
    -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DBUILD_TESTING=OFF \
    -DCMAKE_PREFIX_PATH="$qt_prefix"
cmake --build "$build_dir" --parallel "$parallel_jobs"
cmake --build "$build_dir" --target all_qmllint --parallel "$parallel_jobs"

mkdir -p "$dist_dir"
cpack --config "$build_dir/CPackConfig.cmake" \
    -C Release \
    -G DragNDrop \
    -B "$dist_dir"

echo "macOS package written under $dist_dir"
