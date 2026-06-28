# Windows Bundle `final-release/imwrap-windows`

This bundle gathers the Windows outputs of the project into a single distributable folder.

## Layout

- Project binaries are copied at the root of `final-release/imwrap-windows`.
- Tool executables are stored under `final-release/imwrap-windows/tools/`.
- Project-owned binaries use an architecture suffix:
  - `-x32` for Win32 outputs
  - `-x64` for x64 outputs
- The engine is shipped both as a DLL and as a static library:
  - `imwrap_v6-x32.dll` and `imwrap_v6-x64.dll`
  - `imwrap_v6-x32.lib` and `imwrap_v6-x64.lib` for the DLL import libraries
  - `imwrap_v6-static-x32.lib` and `imwrap_v6-static-x64.lib` for static linking
- Qt DLLs are not renamed because their names are imposed by the Qt executables.
- Qt DLLs and their plugin subfolders are copied into `tools/` so the GUI tools remain portable:
  - `tools/platforms/`
  - `tools/styles/`
  - `tools/generic/`
- `tools/imageformats/` is copied only if `windeployqt` actually generated it.
- If the VST3 SysEx proof of concept is built, its `imwrap_sysex_tool.vst3/` bundle is also copied into `tools/`.
- `SetMIDI` is now a native Win32 utility. It is shipped in both `x32` and `x64` variants and no longer depends on Qt.
- Documentation, wrappers, licenses, and examples are stored in:
  - `docs/`
  - `wrappers/`
  - `licenses/`
  - `examples/`
- The AGS editor plugin and its native shim are stored in `ags-editor-plugin/`.

## Generation

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows-release.ps1
```

To rebuild the full Windows release from scratch:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows-release.ps1 -QtPrefixPath "C:\Qt\6.8.0\msvc2022_64"
```

With an explicit local VST3 SDK:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows-release.ps1 -QtPrefixPath "C:\Qt\6.8.0\msvc2022_64" -Vst3SdkDir "D:\Prog\third_party\vst3sdk"
```

By default, the packaging script assembles:

- `.build\windows-release\win32\Release` for the AGS plugin and Win32 tools
- `.build\windows-release\x64\Release` for x64 Qt tools and x64 libraries
- `ags-editor-plugin\bin\Release` for the AGS editor plugin assembly

The full build script instead works from:

- `.build\windows-release\win32` for Win32
- `.build\windows-release\x64` for x64
- `third_party\fluidsynth-build` for the vendored Win32 FluidSynth dependency used by the AGS plugin
- `third_party\vst3sdk` if you want the Windows build to automatically include the VST3 proof of concept

## GitHub Release v1

The workflow [release-v1.yml](/D:/Prog/imwrap-v6/.github/workflows/release-v1.yml) is intended to:

1. install Qt 6.8.0 on Windows;
2. build the full Windows bundle;
3. regenerate `final-release/imwrap-windows`;
4. create a zip named `imwrap-vX.Y.Z-windows.zip`;
5. publish that zip as a release asset for a `v*` tag.

## Useful Notes

- `SetMIDI-x32.exe` and `SetMIDI-x64.exe` ignore the other utilities shipped in the same bundle, including the `-x32` and `-x64` suffixed variants.
- `SetMIDI-x32.exe` is the best choice to ship next to a Win32 AGS game executable.
- `SetMIDI-x64.exe` is also provided for x64 Windows environments.
- `agsimwrap-x32.dll` is the AGS runtime plugin ready to be copied into a Win32 AGS project.
- `ags-editor-plugin/AGS.Plugin.IMWrap.Editor.dll` is the AGS editor plugin to copy into the AGS editor folder together with `ags-editor-plugin/imwrap_shim.dll`.
- The bundle also includes the `openquest-lite.ims` sample file so a reference bank is immediately available.
