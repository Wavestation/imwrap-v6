[CmdletBinding()]
param(
    [string]$PluginBuildDir,
    [string]$GuiBuildDir,
    [string]$Vst3BuildDir,
    [string]$AgsEditorPluginBuildDir,
    [string]$WebBuildDir,
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
if ([string]::IsNullOrWhiteSpace($Vst3BuildDir)) {
    $Vst3BuildDir = $GuiBuildDir
}
if ([string]::IsNullOrWhiteSpace($AgsEditorPluginBuildDir)) {
    $AgsEditorPluginBuildDir = Join-Path $RootDir "ags-editor-plugin\bin\Release"
}
if ([string]::IsNullOrWhiteSpace($WebBuildDir)) {
    $WebBuildDir = "D:\Prog\imwrap-ags-web\ags\build-web-release"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $RootDir "final-release\imwrap-windows"
}

$PluginBuildDir = [System.IO.Path]::GetFullPath($PluginBuildDir)
$GuiBuildDir = [System.IO.Path]::GetFullPath($GuiBuildDir)
$Vst3BuildDir = [System.IO.Path]::GetFullPath($Vst3BuildDir)
$AgsEditorPluginBuildDir = [System.IO.Path]::GetFullPath($AgsEditorPluginBuildDir)
$WebBuildDir = [System.IO.Path]::GetFullPath($WebBuildDir)
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

    Write-Warning "Missing required file. Tried: $($Sources -join ', ')"
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

function Copy-FirstMatchingDirectoryTreeAs {
    param(
        [Parameter(Mandatory = $true)][string]$SearchRoot,
        [Parameter(Mandatory = $true)][string]$DirectoryName,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (-not (Test-Path -LiteralPath $SearchRoot -PathType Container)) {
        return $false
    }

    $match = Get-ChildItem -Path $SearchRoot -Directory -Recurse -Filter $DirectoryName -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $match) {
        return $false
    }

    Copy-DirectoryTree -Source $match.FullName -Destination $Destination
    return $true
}

Ensure-CleanDirectory -Path $OutputDir

$docsDir = Join-Path $OutputDir "docs"
$examplesDir = Join-Path $OutputDir "examples"
$wrappersDir = Join-Path $OutputDir "wrappers"
$licensesDir = Join-Path $OutputDir "licenses"
$toolsDir = Join-Path $OutputDir "tools"
$agsEditorPluginDir = Join-Path $OutputDir "ags-editor-plugin"
$webExportDir = Join-Path $OutputDir "web-export"

foreach ($dir in @($docsDir, $examplesDir, $wrappersDir, $licensesDir, $toolsDir, $agsEditorPluginDir, $webExportDir)) {
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
    @{ Sources = @((Join-Path $PluginBuildDir "Release\ims2soun.exe")); Name = "ims2soun-x32.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrappack.exe")); Name = "imwrappack-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imsprobe.exe")); Name = "imsprobe-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrap_explorer_gui.exe")); Name = "imwrap_explorer_gui-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrap_packer_gui.exe")); Name = "imwrap_packer_gui-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrap_player_gui.exe")); Name = "imwrap_player_gui-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\imwrap_sysex_gui.exe")); Name = "imwrap_sysex_gui-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\SetMIDI.exe")); Name = "SetMIDI-x64.exe" },
    @{ Sources = @((Join-Path $GuiBuildDir "Release\ims2soun.exe")); Name = "ims2soun-x64.exe" }
)

foreach ($binary in $toolBinaries) {
    Copy-FirstExistingFileAs -Sources $binary.Sources -Destination (Join-Path $toolsDir $binary.Name)
}

[void](Copy-FirstMatchingDirectoryTreeAs `
    -SearchRoot $Vst3BuildDir `
    -DirectoryName "imwrap_sysex_tool.vst3" `
    -Destination (Join-Path $toolsDir "imwrap_sysex_tool.vst3"))

$agsEditorPluginFiles = @(
    @{ Sources = @((Join-Path $AgsEditorPluginBuildDir "AGS.Plugin.IMWrap.Editor.dll")); Name = "AGS.Plugin.IMWrap.Editor.dll" },
    @{ Sources = @((Join-Path $AgsEditorPluginBuildDir "AGS.Plugin.IMWrap.Editor.pdb")); Name = "AGS.Plugin.IMWrap.Editor.pdb"; Optional = $true },

    @{ Sources = @((Join-Path $RootDir "ags-editor-plugin\README.md")); Name = "README.md" },
    @{ Sources = @((Join-Path $RootDir "ags-editor-plugin\README_fr.md")); Name = "README_fr.md"; Optional = $true }
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

$webFiles = @(
    @{ Sources = @((Join-Path $WebBuildDir "ags.html")); Name = "ags.html"; Optional = $true },
    @{ Sources = @((Join-Path $WebBuildDir "ags.js")); Name = "ags.js"; Optional = $true },
    @{ Sources = @((Join-Path $WebBuildDir "ags.wasm")); Name = "ags.wasm"; Optional = $true }
)

foreach ($artifact in $webFiles) {
    if ($artifact.ContainsKey("Optional") -and $artifact.Optional) {
        $optionalSource = $artifact.Sources[0]
        if (Test-Path -LiteralPath $optionalSource -PathType Leaf) {
            Copy-FileAs -Source $optionalSource -Destination (Join-Path $webExportDir $artifact.Name)
        }
        continue
    }

    Copy-FirstExistingFileAs -Sources $artifact.Sources -Destination (Join-Path $webExportDir $artifact.Name)
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
    @{ Source = Join-Path $RootDir "legacy\docs\windows-release.md"; Name = "README.md" },
    @{ Source = Join-Path $RootDir "README.md"; Name = "docs\imwrap-v6-overview.md" },
    @{ Source = Join-Path $RootDir "README.md"; Name = "docs\imwrap-v6-overview_en.md" },
    @{ Source = Join-Path $RootDir "README_fr.md"; Name = "docs\imwrap-v6-overview_fr.md" },
    @{ Source = Join-Path $RootDir "legacy\docs\ags-plugin.md"; Name = "docs\ags-plugin.md" },
    @{ Source = Join-Path $RootDir "legacy\docs\imwrappack.md"; Name = "docs\imwrappack.md" },
    @{ Source = Join-Path $RootDir "legacy\docs\ims-v1.md"; Name = "docs\ims-v1.md" },
    @{ Source = Join-Path $RootDir "legacy\docs\guide_compositeur_fr.md"; Name = "docs\guide-compositeur.md" }
)

foreach ($doc in $documentationFiles) {
    Copy-FileAs -Source $doc.Source -Destination (Join-Path $OutputDir $doc.Name)
}

Get-ChildItem -Path (Join-Path $RootDir "docs") -File | ForEach-Object {
    Copy-FileAs -Source $_.FullName -Destination (Join-Path $docsDir $_.Name)
}

$wrapperFiles = @(
    @{ Source = Join-Path $RootDir "ags-wrapper\IMWrapShim.cs"; Name = "wrappers\IMWrapShim.cs" },
    @{ Source = Join-Path $RootDir "ags-wrapper\IMWrapShim.vb"; Name = "wrappers\IMWrapShim.vb" }
)

foreach ($wrapper in $wrapperFiles) {
    Copy-FileAs -Source $wrapper.Source -Destination (Join-Path $OutputDir $wrapper.Name)
}

$licenseFiles = @(
    @{ Source = Join-Path $RootDir "LICENSE.GPL"; Name = "licenses\LICENSE-imwrap-v6-GPL.txt" },
    @{ Source = Join-Path $RootDir "LICENSE.LESSER"; Name = "licenses\LICENSE-imwrap-v6-LESSER.txt" },
    @{ Source = Join-Path $RootDir "third_party\libadlmidi\LICENSE"; Name = "licenses\LICENSE-libADLMIDI.txt" },
    @{ Source = Join-Path $RootDir "third_party\libadlmidi\LICENSE.GPL-3.txt"; Name = "licenses\LICENSE-libADLMIDI-GPL-3.txt" },
    @{ Source = Join-Path $RootDir "third_party\libadlmidi\LICENSE.LGPL-2.1.txt"; Name = "licenses\LICENSE-libADLMIDI-LGPL-2.1.txt" },
    @{ Source = Join-Path $RootDir "third_party\fluidsynth-master\LICENSE"; Name = "licenses\LICENSE-FluidSynth.txt" }
)

foreach ($license in $licenseFiles) {
    Copy-FileAs -Source $license.Source -Destination (Join-Path $OutputDir $license.Name)
}

$samplesDir = Join-Path $RootDir "samples"
if (Test-Path -LiteralPath $samplesDir -PathType Container) {
    Copy-Item -Path (Join-Path $samplesDir "*") -Destination $examplesDir -Recurse -Force
}

Write-Host "Windows release bundle created in $OutputDir"
