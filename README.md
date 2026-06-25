# iMWrap v6

iMWrap v6 is a modern, modular C++ reimplementation of the interactive music
engine used by LucasArts iMUSE v6-era games.

The repository contains:
- the core C++ engine in `src/` and `include/`
- AGS integration helpers in `ags-wrapper/` and `ags-editor-plugin/`
- SwiftUI utilities in `swift-app/`
- command-line tools in `tools/`
- vendored dependencies in `third_party/`

## Build Notes

The repository can be configured in a minimal mode without Qt or AGS-specific
dependencies:

```powershell
cmake -S . -B build/minimal `
  -DIMWRAP_BUILD_GUI_TOOLS=OFF `
  -DIMWRAP_BUILD_AGS_PLUGIN=OFF `
  -DIMWRAP_BUILD_SETMIDI=OFF `
  -DIMWRAP_BUILD_SHIM=OFF
```

Windows GUI builds require Qt 6 to be discoverable through `CMAKE_PREFIX_PATH`
or `IMWRAP_QT6_PREFIX_PATH`.

The AGS plugin and some packaging scripts also expect vendored third-party build
artifacts to be available under `third_party/*-build/`.

## Repository Layout

- `src/`, `include/`: core engine sources and headers
- `tools/`: `imwrappack` and `imsprobe`
- `tools_gui/`: native Windows GUI utilities
- `swift-shim/`, `swift-app/`: native shim and macOS apps
- `ags-wrapper/`, `ags-editor-plugin/`: .NET and AGS editor integration
- `samples/`: example `.ims` banks
- `third_party/`: vendored dependency sources

## Licensing

iMWrap v6 is distributed under the GNU General Public License v3.0 or later.
See `LICENSE`.

Vendored dependency licenses are tracked in `THIRD_PARTY_LICENSES.md` and in the
upstream license files kept under `third_party/`.
