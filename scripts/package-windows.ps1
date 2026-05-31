param(
    [string] $QtPrefix = "C:/Qt/6.11.1/msvc2022_64"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $repoRoot "dist"

if (-not (Test-Path $QtPrefix)) {
    throw "Qt prefix not found: $QtPrefix. Pass -QtPrefix with your Windows Qt 6.11 path."
}

Push-Location $repoRoot
try {
    cmake --preset qt6.11-windows-release -DCMAKE_PREFIX_PATH="$QtPrefix"
    cmake --build --preset qt6.11-windows-release
    cmake --build --preset qt6.11-windows-release --target all_qmllint

    New-Item -ItemType Directory -Force -Path $distDir | Out-Null
    cpack --preset qt6.11-windows-zip
}
finally {
    Pop-Location
}

Write-Host "Windows package written under $distDir"
