#!/usr/bin/env bash
set -euo pipefail

qt_prefix="${1:-${QT_PREFIX:-/opt/Qt/6.11.0/gcc_64}}"
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/build/package-linux"
dist_dir="$repo_root/dist"
parallel_jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-4}"

if [[ ! -d "$qt_prefix" ]]; then
    echo "Qt prefix not found: $qt_prefix" >&2
    echo "Pass a Qt path as the first argument, or set QT_PREFIX." >&2
    exit 1
fi

cd "$repo_root"

cmake --fresh -S "$repo_root" -B "$build_dir" \
    -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DCMAKE_PREFIX_PATH="$qt_prefix"
cmake --build "$build_dir" --parallel "$parallel_jobs"
cmake --build "$build_dir" --target all_qmllint --parallel "$parallel_jobs"

mkdir -p "$dist_dir"
cpack --config "$build_dir/CPackConfig.cmake" \
    -C Release \
    -G TGZ \
    -B "$dist_dir"

echo "Linux package written under $dist_dir"
