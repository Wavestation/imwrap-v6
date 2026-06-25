# AGS Editor Plugin

This folder contains a separate .NET project for an AGS 3.6.2 editor plugin.

## Current Features

- adds an `iMWrap` menu to the AGS editor
- adds a `Banks` root node to the project tree
- discovers `.ims` files in the project and lists them in that tree
- opens the IMS editor as a document tab
- displays the sounds, tracks, and control events for the selected bank
- supports creating, renaming, and deleting `.ims` banks from the tree
- reuses the existing C# wrapper in `ags-wrapper/IMWrapShim.cs`

## Build

The project expects `AGS.Types.dll` from a locally installed AGS editor folder.

Supported properties:

- `AGSReferenceDir`
- `AGS_TYPES_DIR`
- `AGS_EDITOR_DIR`

Example:

```powershell
dotnet msbuild .\AGS.Plugin.IMWrap.Editor.csproj /p:Configuration=Release /p:AGSReferenceDir=C:\Program Files (x86)\Adventure Game Studio
```

The project now targets `.NET Framework 4.8` so it can build on modern Windows machines that do not ship the 4.6 targeting pack by default.

## Installing Into AGS

1. Copy `AGS.Plugin.IMWrap.Editor.dll` into the AGS editor folder.
2. Copy `imwrap_shim.dll` and its native dependencies next to it if they are not already present.
3. Make sure the native DLL architecture matches the AGS editor process architecture.

AGS automatically loads assemblies whose file name starts with `AGS.Plugin.`.

## Notes

- This editor plugin is separate from the AGS runtime plugin already present in `src/`.
- The native runtime already knows how to load `.ims` banks from packaged game data.
- The current IMS editor is intentionally minimal and focused on inspection/tree management before adding playback, import/export, or build actions.
