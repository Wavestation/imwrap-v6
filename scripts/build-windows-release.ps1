[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$Win32BuildDir,
    [string]$X64BuildDir,
    [string]$FluidSynthBuildDir,
    [string]$QtPrefixPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RootDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

if ([string]::IsNullOrWhiteSpace($Win32BuildDir)) {
    $Win32BuildDir = Join-Path $RootDir ".build\\windows-release\\win32"
}
if ([string]::IsNullOrWhiteSpace($X64BuildDir)) {
    $X64BuildDir = Join-Path $RootDir ".build\\windows-release\\x64"
}
if ([string]::IsNullOrWhiteSpace($FluidSynthBuildDir)) {
    $FluidSynthBuildDir = Join-Path $RootDir "third_party\\fluidsynth-build"
}

$Win32BuildDir = [System.IO.Path]::GetFullPath($Win32BuildDir)
$X64BuildDir = [System.IO.Path]::GetFullPath($X64BuildDir)
$FluidSynthBuildDir = [System.IO.Path]::GetFullPath($FluidSynthBuildDir)

if ([string]::IsNullOrWhiteSpace($QtPrefixPath)) {
    if (-not [string]::IsNullOrWhiteSpace($env:Qt6_DIR)) {
        $QtPrefixPath = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $env:Qt6_DIR))
    } elseif (-not [string]::IsNullOrWhiteSpace($env:QT_ROOT_DIR)) {
        $QtPrefixPath = $env:QT_ROOT_DIR
    }
}

if ([string]::IsNullOrWhiteSpace($QtPrefixPath)) {
    throw "QtPrefixPath is required to build the x64 GUI tools. Pass -QtPrefixPath or set Qt6_DIR / QT_ROOT_DIR."
}

$QtPrefixPath = [System.IO.Path]::GetFullPath($QtPrefixPath)

function Invoke-CMake {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    & cmake @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "cmake failed with exit code $LASTEXITCODE"
    }
}

function Invoke-CMakeBuild {
    param(
        [Parameter(Mandatory = $true)][string]$BuildDir,
        [Parameter(Mandatory = $true)][string[]]$Targets
    )

    $arguments = @("--build", $BuildDir, "--config", $Configuration)
    if ($Targets.Count -gt 0) {
        $arguments += @("--target")
        $arguments += $Targets
    }

    Invoke-CMake -Arguments $arguments
}

if (-not (Test-Path -LiteralPath (Join-Path $FluidSynthBuildDir "include\\fluidsynth.h") -PathType Leaf)) {
    throw "Missing vendored FluidSynth build tree at $FluidSynthBuildDir. Expected include\\fluidsynth.h."
}
if (-not (Test-Path -LiteralPath (Join-Path $FluidSynthBuildDir "src\\Release\\libfluidsynth-3.lib") -PathType Leaf)) {
    throw "Missing vendored FluidSynth static library at $FluidSynthBuildDir\\src\\Release\\libfluidsynth-3.lib."
}

Invoke-CMake -Arguments @(
    "-S", $RootDir,
    "-B", $Win32BuildDir,
    "-A", "Win32",
    "-DIMUSE_BUILD_GUI_TOOLS=OFF",
    "-DIMUSE_BUILD_AGS_PLUGIN=ON",
    "-DIMUSE_BUILD_SHARED_LIB=ON",
    "-DIMUSE_FLUIDSYNTH_BUILD_DIR=$FluidSynthBuildDir"
)

Invoke-CMakeBuild -BuildDir $Win32BuildDir -Targets @(
    "ADLMIDI_static",
    "imuse_v6",
    "imuse_v6_shared",
    "imusepack",
    "imsprobe",
    "imuse_setmidi",
    "agsimuse"
)

Invoke-CMake -Arguments @(
    "-S", $RootDir,
    "-B", $X64BuildDir,
    "-A", "x64",
    "-DCMAKE_PREFIX_PATH=$QtPrefixPath",
    "-DIMUSE_QT6_PREFIX_PATH=$QtPrefixPath",
    "-DIMUSE_BUILD_GUI_TOOLS=ON",
    "-DIMUSE_BUILD_AGS_PLUGIN=OFF",
    "-DIMUSE_BUILD_SHARED_LIB=ON"
)

Invoke-CMakeBuild -BuildDir $X64BuildDir -Targets @(
    "ADLMIDI_static",
    "imuse_v6",
    "imuse_v6_shared",
    "imusepack",
    "imsprobe",
    "imuse_packer_gui",
    "imuse_player_gui",
    "imuse_sysex_gui",
    "imuse_setmidi"
)

& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "package-windows-release.ps1") `
    -PluginBuildDir $Win32BuildDir `
    -GuiBuildDir $X64BuildDir

if ($LASTEXITCODE -ne 0) {
    throw "package-windows-release.ps1 failed with exit code $LASTEXITCODE"
}

Write-Host "Windows release build completed successfully."
