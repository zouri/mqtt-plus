param(
    [string] $QtPrefix = $env:QT_ROOT_DIR,
    [string] $BuildDirectory = "build/package-windows",
    [int] $ParallelJobs = 4
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $repoRoot "dist"
$buildDir = Join-Path $repoRoot $BuildDirectory

function Assert-NativeSuccess
{
    param([string] $Operation)

    if ($LASTEXITCODE -ne 0) {
        throw "$Operation failed with exit code $LASTEXITCODE."
    }
}

if ([string]::IsNullOrWhiteSpace($QtPrefix)) {
    $QtPrefix = "C:/Qt/6.11.1/msvc2022_64"
}

if (-not (Test-Path $QtPrefix)) {
    throw "Qt prefix not found: $QtPrefix. Pass -QtPrefix with your Windows Qt 6.11 path."
}

Push-Location $repoRoot
try {
    $generatorArguments = @()
    $buildToolArguments = @()
    $vswherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    if (Test-Path $vswherePath) {
        $vsInstance = & $vswherePath `
            -latest `
            -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -format json | ConvertFrom-Json | Select-Object -First 1

        if ($vsInstance) {
            $vsMajorVersion = [int]($vsInstance.installationVersion -split '\.')[0]
            $generatorName = switch ($vsMajorVersion) {
                18 { "Visual Studio 18 2026" }
                17 { "Visual Studio 17 2022" }
                default { $null }
            }

            if ($generatorName) {
                $generatorArguments = @("-G", $generatorName, "-A", "x64")
                $buildToolArguments = @("--", "/nodeReuse:false")
            }
        }
    }

    if ($generatorArguments.Count -eq 0) {
        $ninjaCommand = Get-Command ninja.exe -ErrorAction SilentlyContinue
        if (-not $ninjaCommand) {
            $bundledNinja = "C:/Qt/Tools/Ninja/ninja.exe"
            if (Test-Path $bundledNinja) {
                $ninjaCommand = Get-Item $bundledNinja
            }
        }
        if (-not $ninjaCommand) {
            throw "No supported Visual Studio installation or Ninja executable was found."
        }
        $ninjaPath = $ninjaCommand.Source
        if (-not $ninjaPath) {
            $ninjaPath = $ninjaCommand.FullName
        }
        $generatorArguments = @(
            "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_MAKE_PROGRAM=$ninjaPath"
        )
    }

    $configureArguments = @(
        "--fresh",
        "-S", $repoRoot,
        "-B", $buildDir
    ) + $generatorArguments + @(
        "-DBUILD_TESTING=OFF",
        "-DCMAKE_PREFIX_PATH=$QtPrefix"
    )
    & cmake @configureArguments
    Assert-NativeSuccess "CMake configure"

    cmake --build $buildDir --config Release --parallel $ParallelJobs @buildToolArguments
    Assert-NativeSuccess "Release build"
    cmake --build $buildDir --config Release --target all_qmllint --parallel $ParallelJobs @buildToolArguments
    Assert-NativeSuccess "QML lint"

    New-Item -ItemType Directory -Force -Path $distDir | Out-Null
    cpack --config (Join-Path $buildDir "CPackConfig.cmake") `
        -C Release `
        -G ZIP `
        -B $distDir
    Assert-NativeSuccess "CPack ZIP generation"
}
finally {
    Pop-Location
}

Write-Host "Windows package written under $distDir"
