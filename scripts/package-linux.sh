#!/usr/bin/env bash
set -euo pipefail

qt_prefix="${1:-${QT_PREFIX:-/opt/Qt/6.11.0/gcc_64}}"
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/build/package-linux"
dist_dir="$repo_root/dist"
parallel_jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
linuxdeploy_version="1-alpha-20251107-1"
linuxdeploy_sha256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
linuxdeploy_url="https://github.com/linuxdeploy/linuxdeploy/releases/download/$linuxdeploy_version/linuxdeploy-x86_64.AppImage"
linuxdeploy="$build_dir/package-tools/linuxdeploy-x86_64.AppImage"

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
    -G DEB \
    -B "$dist_dir"

package_version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build_dir/CMakeCache.txt")"
if [[ -z "$package_version" ]]; then
    echo "Could not determine the project version from $build_dir/CMakeCache.txt" >&2
    exit 1
fi

if [[ "$(uname -m)" != "x86_64" ]]; then
    echo "AppImage packaging currently supports only x86_64 Linux runners." >&2
    exit 1
fi

mkdir -p "$(dirname "$linuxdeploy")"
if [[ ! -f "$linuxdeploy" ]] \
    || ! echo "$linuxdeploy_sha256  $linuxdeploy" | sha256sum --check --status; then
    curl --fail --location --retry 3 --output "$linuxdeploy" "$linuxdeploy_url"
fi
echo "$linuxdeploy_sha256  $linuxdeploy" | sha256sum --check
chmod +x "$linuxdeploy"

app_dir="$build_dir/AppDir"
appimage_output_dir="$build_dir/appimage-output"
rm -rf "$app_dir" "$appimage_output_dir"
mkdir -p "$app_dir" "$appimage_output_dir"

DESTDIR="$app_dir" cmake --install "$build_dir" \
    --config Release \
    --prefix /usr

# Qt deploys every SQL driver plugin, including drivers whose proprietary
# client libraries are unavailable on the AppImage build host. MQTT Plus only
# uses SQLite for local persistence.
find "$app_dir/usr/lib/mqtt-plus/plugins/sqldrivers" \
    -type f ! -name 'libqsqlite.so' -delete

(
    cd "$appimage_output_dir"
    APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64 "$linuxdeploy" \
        --appdir "$app_dir" \
        --executable "$app_dir/usr/lib/mqtt-plus/mqtt_plus_app" \
        --desktop-file "$repo_root/packaging/linux/mqtt-plus.desktop" \
        --icon-file "$repo_root/assets/icons/app-icon.png" \
        --output appimage
)

shopt -s nullglob
generated_appimages=("$appimage_output_dir"/*.AppImage)
if [[ ${#generated_appimages[@]} -ne 1 ]]; then
    echo "Expected one generated AppImage, found ${#generated_appimages[@]}." >&2
    exit 1
fi

appimage_path="$dist_dir/mqtt-plus-$package_version-x86_64.AppImage"
mv "${generated_appimages[0]}" "$appimage_path"
chmod +x "$appimage_path"

echo "Linux DEB and AppImage packages written under $dist_dir"
