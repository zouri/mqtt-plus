#!/usr/bin/env bash
set -euo pipefail

usage() {
    printf '%s\n' \
        'Usage: ./scripts/dev.sh [check|build|run|test|lint|configure] [debug|release]' \
        'Default: check debug (configure, build, QML lint, then all tests).' \
        'QT_PREFIX defaults to $HOME/Qt/6.11.1/macos.' \
        'CMAKE_BUILD_PARALLEL_LEVEL defaults to 4.'
}

action="${1:-check}"
mode="${2:-debug}"
case "$action" in
    -h|--help) usage; exit 0 ;;
    check|build|run|test|lint|configure) ;;
    *) usage >&2; exit 2 ;;
esac
if [[ $# -gt 2 || ( "$mode" != debug && "$mode" != release ) ]]; then
    usage >&2
    exit 2
fi
if [[ "$(uname -s)" != Darwin ]]; then
    echo 'This helper uses the macOS presets. See README for other platforms.' >&2
    exit 1
fi

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
qt_prefix="${QT_PREFIX:-$HOME/Qt/6.11.1/macos}"
preset="qt6.11-$mode"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-4}"

for tool in cmake ctest ninja node npm; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing required tool on PATH: $tool" >&2
        exit 1
    fi
done
if [[ ! -f "$qt_prefix/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    echo 'Qt not found. Set QT_PREFIX to your Qt 6.11 macOS installation.' >&2
    exit 1
fi

cd "$repo_root"
cmake --preset "$preset" -DCMAKE_PREFIX_PATH="$qt_prefix" -DBUILD_TESTING=ON
case "$action" in
    configure) exit 0 ;;
    build|run)
        cmake --build --preset "$preset" --target mqtt_plus_app
        ;;
    check|test)
        cmake --build --preset "$preset"
        ;;
esac
if [[ "$action" == check || "$action" == lint ]]; then
    cmake --build --preset "$preset" --target all_qmllint
fi
if [[ "$action" == check || "$action" == test ]]; then
    ctest --preset "$preset"
fi
if [[ "$action" == run ]]; then
    exec "$repo_root/build/$preset/mqtt-plus.app/Contents/MacOS/mqtt-plus"
fi
