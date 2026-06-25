[CmdletBinding()]
param(
    [string]$PluginBuildDir,
    [string]$GuiBuildDir,
    [string]$AgsEditorPluginBuildDir,
    [string]$OutputDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RootDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

if ([string]::IsNullOrWhiteSpace($PluginBuildDir)) {
    $PluginBuildDir = Join-Path $RootDir ".build\\windows-release\\win32"
}
if ([string]::IsNullOrWhiteSpace($GuiBuildDir)) {
    $GuiBuildDir = Join-Path $RootDir ".build\\windows-release\\x64"
}
if ([string]::IsNullOrWhiteSpace($AgsEditorPluginBuildDir)) {
    $AgsEditorPluginBuildDir = Join-Path $RootDir "ags-editor-plugin\bin\Release"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $RootDir "final-release\imwrap-windows"
}

$PluginBuildDir = [System.IO.Path]::GetFullPath($PluginBuildDir)
$GuiBuildDir = [System.IO.Path]::GetFullPath($GuiBuildDir)
$AgsEditorPluginBuildDir = [System.IO.Path]::GetFullPath($AgsEditorPluginBuildDir)
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)

function Assert-WithinWorkspace {
    param([Parameter(Mandatory = $true)][string]$Path)

    $normalizedRoot = [System.IO.Path]::GetFullPath($RootDir).TrimEnd("\")
    $normalizedPath = [System.IO.Path]::GetFullPath($Path)
    $rootWithSlash = "$normalizedRoot\"

    if ($normalizedPath -ne $normalizedRoot -and
        -not $normalizedPath.StartsWith($rootWithSlash, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the workspace: $normalizedPath"
    }
}

function Ensure-CleanDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)

    Assert-WithinWorkspace -Path $Path
    if ([System.IO.Path]::GetFullPath($Path).TrimEnd("\") -eq [System.IO.Path]::GetFullPath($RootDir).TrimEnd("\")) {
        throw "Refusing to clean the workspace root."
    }

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }

    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Ensure-Directory {
    param([Parameter(Mandatory = $true)][string]$Path)

    Assert-WithinWorkspace -Path $Path
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Require-File {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing required file: $Path"
    }

    return [System.IO.Path]::GetFullPath($Path)
}

function Copy-FileAs {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $sourcePath = Require-File -Path $Source
    $destinationPath = [System.IO.Path]::GetFullPath($Destination)
    Assert-WithinWorkspace -Path $destinationPath
    $parentDir = Split-Path -Parent $destinationPath
    Ensure-Directory -Path $parentDir
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}

function Copy-FirstExistingFileAs {
    param(
        [Parameter(Mandatory = $true)][string[]]$Sources,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    foreach ($source in $Sources) {
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            Copy-FileAs -Source $source -Destination $Destination
            return
        }
    }

    throw "Missing required file. Tried: $($Sources -join ', ')"
}

function Copy-DirectoryTree {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Missing required directory: $Source"
    }

    $destinationPath = [System.IO.Path]::GetFullPath($Destination)
    Assert-WithinWorkspace -Path $destinationPath
    $parentDir = Split-Path -Parent $destinationPath
    Ensure-Directory -Path $parentDir
    Copy-Item -LiteralPath $Source -Destination $destinationPath -Recurse -Force
}

function Copy-DirectoryTreeIfExists {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (Test-Path -LiteralPath $Source -PathType Container) {
        Copy-DirectoryTree -Source $Source -Destination $Destination
    }
}

Ensure-CleanDirectory -Path $OutputDir

$docsDir = Join-Path $OutputDir "docs"
$examplesDir = Join-Path $OutputDir "examples"
$wrappersDir = Join-Path $OutputDir "wrappers"
$licensesDir = Join-Path $OutputDir "licenses"
$toolsDir = Join-Path $OutputDir "tools"
$agsEditorPluginDir = Join-Path $OutputDir "ags-editor-plugin"

foreach ($dir in @($docsDir, $examplesDir, $wrappersDir, $licensesDir, $toolsDir, $agsEditorPluginDir)) {
    Ensure-Directory -Path $dir
}

$projectBinaries = @(
    @{ Sources = @((Join-Path $PluginBuildDir "Release\agsimwrap.dll")); Name = "agsimwrap-x32.dll" },
    @{ Sources = @((Join-Path $PluginBuildDir "Release\agsimwrap.lib")); Name = "agsimwrap-x32.lib" },
    @{ Sources = @((Join-Path $PluginBuildDir "Release\imwrap_v6.dll")); Name = "imwrap_v6-x32.dll" },
    @{ Sources = @((Join-Path $PluginBuildDir "Release\imwrap_v6_dll.lib")); Name = "imwrap_v6-x32.lib" },
    @{ Sources = @((Join-Path $PluginBuildDir "Release\imwrap_v6_static.lib"), (Join-Path $PluginBuildDir "Release\imwrap_v6.lib")); Name = "imwrap_v6-static-x32.lib" },
    @{ Sources = @((Join-Path $PluginBuildDir "Release\ADLMIDI.lib")); Name = "ADLMIDI-x32.lib" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrap_v6.dll")); Name = "imwrap_v6-x64.dll" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrap_v6_dll.lib")); Name = "imwrap_v6-x64.lib" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrap_v6_static.lib"), (Join-Path $GuiBuildDir "Release\imwrap_v6.lib")); Name = "imwrap_v6-static-x64.lib" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\ADLMIDI.lib")); Name = "ADLMIDI-x64.lib" }
)

foreach ($binary in $projectBinaries) {
    Copy-FirstExistingFileAs -Sources $binary.Sources -Destination (Join-Path $OutputDir $binary.Name)
}

$toolBinaries = @(
    @{ Sources = @((Join-Path $PluginBuildDir "Release\imwrappack.exe")); Name = "imwrappack-x32.exe" },
    @{ Sources = @((Join-Path $PluginBuildDir "Release\imsprobe.exe")); Name = "imsprobe-x32.exe" },
    @{ Sources = @((Join-Path $PluginBuildDir "Release\SetMIDI.exe")); Name = "SetMIDI-x32.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrappack.exe")); Name = "imwrappack-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imsprobe.exe")); Name = "imsprobe-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrap_packer_gui.exe")); Name = "imwrap_packer_gui-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrap_player_gui.exe")); Name = "imwrap_player_gui-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrap_sysex_gui.exe")); Name = "imwrap_sysex_gui-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\SetMIDI.exe")); Name = "SetMIDI-x64.exe" }
)

foreach ($binary in $toolBinaries) {
    Copy-FirstExistingFileAs -Sources $binary.Sources -Destination (Join-Path $toolsDir $binary.Name)
}

$agsEditorPluginFiles = @(
    @{ Sources = @((Join-Path $AgsEditorPluginBuildDir "AGS.Plugin.IMWrap.Editor.dll")); Name = "AGS.Plugin.IMWrap.Editor.dll" },
    @{ Sources = @((Join-Path $AgsEditorPluginBuildDir "AGS.Plugin.IMWrap.Editor.pdb")); Name = "AGS.Plugin.IMWrap.Editor.pdb"; Optional = $true },
    @{ Sources = @((Join-Path $PluginBuildDir "Release\imwrap_shim.dll")); Name = "imwrap_shim.dll" },
    @{ Sources = @((Join-Path $PluginBuildDir "Release\imwrap_shim.pdb")); Name = "imwrap_shim.pdb"; Optional = $true },
    @{ Sources = @((Join-Path $RootDir "ags-editor-plugin\README.md")); Name = "README.md" }
)

foreach ($artifact in $agsEditorPluginFiles) {
    if ($artifact.ContainsKey("Optional") -and $artifact.Optional) {
        $optionalSource = $artifact.Sources[0]
        if (Test-Path -LiteralPath $optionalSource -PathType Leaf) {
            Copy-FileAs -Source $optionalSource -Destination (Join-Path $agsEditorPluginDir $artifact.Name)
        }
        continue
    }

    Copy-FirstExistingFileAs -Sources $artifact.Sources -Destination (Join-Path $agsEditorPluginDir $artifact.Name)
}

$qtRuntimeFiles = @(
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll"
)

foreach ($qtFile in $qtRuntimeFiles) {
    Copy-FileAs -Source (Join-Path $GuiBuildDir "Release\$qtFile") -Destination (Join-Path $toolsDir $qtFile)
}

$qtPluginDirs = @(
    "platforms",
    "imageformats",
    "styles",
    "generic"
)

foreach ($qtDir in $qtPluginDirs) {
    Copy-DirectoryTreeIfExists -Source (Join-Path $GuiBuildDir "Release\$qtDir") -Destination (Join-Path $toolsDir $qtDir)
}

$documentationFiles = @(
    @{ Source = Join-Path $RootDir "docs\windows-release.md"; Name = "README.md" },
    @{ Source = Join-Path $RootDir "README.md"; Name = "docs\imwrap-v6-overview.md" },
    @{ Source = Join-Path $RootDir "docs\ags-plugin.md"; Name = "docs\ags-plugin.md" },
    @{ Source = Join-Path $RootDir "docs\imwrappack.md"; Name = "docs\imwrappack.md" },
    @{ Source = Join-Path $RootDir "docs\ims-v1.md"; Name = "docs\ims-v1.md" },
    @{ Source = Join-Path $RootDir "docs\guide_compositeur.md"; Name = "docs\guide-compositeur.md" }
)

foreach ($doc in $documentationFiles) {
    Copy-FileAs -Source $doc.Source -Destination (Join-Path $OutputDir $doc.Name)
}

$wrapperFiles = @(
    @{ Source = Join-Path $RootDir "ags-wrapper\IMWrapShim.cs"; Name = "wrappers\IMWrapShim.cs" },
    @{ Source = Join-Path $RootDir "ags-wrapper\IMWrapShim.vb"; Name = "wrappers\IMWrapShim.vb" }
)

foreach ($wrapper in $wrapperFiles) {
    Copy-FileAs -Source $wrapper.Source -Destination (Join-Path $OutputDir $wrapper.Name)
}

$licenseFiles = @(
    @{ Source = Join-Path $RootDir "LICENSE"; Name = "licenses\LICENSE-imwrap-v6.txt" },
    @{ Source = Join-Path $RootDir "third_party\libadlmidi\LICENSE"; Name = "licenses\LICENSE-libADLMIDI.txt" },
    @{ Source = Join-Path $RootDir "third_party\libadlmidi\LICENSE.GPL-3.txt"; Name = "licenses\LICENSE-libADLMIDI-GPL-3.txt" },
    @{ Source = Join-Path $RootDir "third_party\libadlmidi\LICENSE.LGPL-2.1.txt"; Name = "licenses\LICENSE-libADLMIDI-LGPL-2.1.txt" },
    @{ Source = Join-Path $RootDir "third_party\fluidsynth-master\LICENSE"; Name = "licenses\LICENSE-FluidSynth.txt" }
)

foreach ($license in $licenseFiles) {
    Copy-FileAs -Source $license.Source -Destination (Join-Path $OutputDir $license.Name)
}

$exampleFiles = @(
    @{ Source = Join-Path $RootDir "baka\midiseqs\lucasarts\monkey.ims"; Name = "examples\monkey-island.ims" },
    @{ Source = Join-Path $RootDir "samples\openquest-lite.ims"; Name = "examples\openquest-lite.ims" }
)

foreach ($example in $exampleFiles) {
    Copy-FileAs -Source $example.Source -Destination (Join-Path $OutputDir $example.Name)
}

Write-Host "Windows release bundle created in $OutputDir"
