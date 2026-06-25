[CmdletBinding()]
param(
    [string]$Configuration = "Release",
    [string]$Win32BuildDir,
    [string]$X64BuildDir,
    [string]$FluidSynthBuildDir,
    [string]$QtPrefixPath = "",
    [string]$AGSReferenceDir = ""
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
if (-not [string]::IsNullOrWhiteSpace($AGSReferenceDir)) {
    $AGSReferenceDir = [System.IO.Path]::GetFullPath($AGSReferenceDir)
}

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

function Invoke-DotNetMsBuild {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectPath,
        [Parameter(Mandatory = $true)][string[]]$Properties
    )

    $arguments = @("msbuild", $ProjectPath)
    foreach ($property in $Properties) {
        $arguments += "/p:$property"
    }

    & dotnet @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "dotnet msbuild failed with exit code $LASTEXITCODE"
    }
}

function Resolve-AgsReferenceDir {
    param([string]$ExplicitPath)

    $candidates = New-Object System.Collections.Generic.List[string]

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $candidates.Add($ExplicitPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:AGSReferenceDir)) {
        $candidates.Add($env:AGSReferenceDir)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:AGS_TYPES_DIR)) {
        $candidates.Add($env:AGS_TYPES_DIR)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:AGS_EDITOR_DIR)) {
        $candidates.Add($env:AGS_EDITOR_DIR)
    }

    $installedAgs = Get-ItemProperty 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*' -ErrorAction SilentlyContinue |
        Where-Object {
            $_.PSObject.Properties.Match('DisplayName').Count -gt 0 -and
            $_.DisplayName -like 'Adventure Game Studio*'
        } |
        Select-Object -First 1

    if ($installedAgs -and -not [string]::IsNullOrWhiteSpace($installedAgs.InstallLocation)) {
        $candidates.Add($installedAgs.InstallLocation)
    }

    $candidates.Add('D:\AGS')

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        $fullCandidate = [System.IO.Path]::GetFullPath($candidate)
        if (Test-Path -LiteralPath (Join-Path $fullCandidate 'AGS.Types.dll') -PathType Leaf) {
            return $fullCandidate
        }
    }

    throw "Could not locate AGS.Types.dll. Pass -AGSReferenceDir or set AGSReferenceDir / AGS_TYPES_DIR / AGS_EDITOR_DIR."
}

$AGSReferenceDir = Resolve-AgsReferenceDir -ExplicitPath $AGSReferenceDir

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
    "-DIMWRAP_BUILD_GUI_TOOLS=OFF",
    "-DIMWRAP_BUILD_AGS_PLUGIN=ON",
    "-DIMWRAP_BUILD_SHARED_LIB=ON",
    "-DIMWRAP_FLUIDSYNTH_BUILD_DIR=$FluidSynthBuildDir"
)

Invoke-CMakeBuild -BuildDir $Win32BuildDir -Targets @(
    "ADLMIDI_static",
    "imwrap_v6",
    "imwrap_v6_shared",
    "imwrap_shim",
    "imwrappack",
    "imsprobe",
    "imwrap_setmidi",
    "agsimwrap"
)

Invoke-CMake -Arguments @(
    "-S", $RootDir,
    "-B", $X64BuildDir,
    "-A", "x64",
    "-DCMAKE_PREFIX_PATH=$QtPrefixPath",
    "-DIMWRAP_QT6_PREFIX_PATH=$QtPrefixPath",
    "-DIMWRAP_BUILD_GUI_TOOLS=ON",
    "-DIMWRAP_BUILD_AGS_PLUGIN=OFF",
    "-DIMWRAP_BUILD_SHARED_LIB=ON"
)

Invoke-CMakeBuild -BuildDir $X64BuildDir -Targets @(
    "ADLMIDI_static",
    "imwrap_v6",
    "imwrap_v6_shared",
    "imwrappack",
    "imsprobe",
    "imwrap_packer_gui",
    "imwrap_player_gui",
    "imwrap_sysex_gui",
    "imwrap_setmidi"
)

Invoke-DotNetMsBuild -ProjectPath (Join-Path $RootDir "ags-editor-plugin\AGS.Plugin.IMWrap.Editor.csproj") -Properties @(
    "Configuration=$Configuration",
    "AGSReferenceDir=$AGSReferenceDir"
)

& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "package-windows-release.ps1") `
    -PluginBuildDir $Win32BuildDir `
    -GuiBuildDir $X64BuildDir `
    -AgsEditorPluginBuildDir (Join-Path $RootDir "ags-editor-plugin\bin\$Configuration") `
    -OutputDir (Join-Path $RootDir "final-release\\imwrap-windows")

if ($LASTEXITCODE -ne 0) {
    throw "package-windows-release.ps1 failed with exit code $LASTEXITCODE"
}

Write-Host "Windows release build completed successfully."
