# iMWrap v6

iMWrap v6 is a modern, modular C++ reimplementation of the interactive music
engine used by LucasArts iMUSE v6-era games.

The repository contains:
- the core C++ engine in `src/` and `include/`
- maintained command-line tools in `tools/`
- maintained Qt GUI tools in `tools_gui/`
- AGS integration helpers in `ags-wrapper/` and `ags-editor-plugin/`
- legacy compatibility surfaces in `legacy-tools/`
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

`IMWRAP_BUILD_SHIM` controls the legacy `imwrap_shim` compatibility DLL. It is
still buildable, but its sources now live under `legacy-tools/`.

## Repository Layout

- `src/`, `include/`: core engine sources and headers
- `tools/`: maintained command-line tools, including the current `imwrappack`
- `tools_gui/`: native Windows GUI utilities
- `ags-wrapper/`, `ags-editor-plugin/`: .NET and AGS editor integration
- `legacy-tools/`: retired Swift tools, legacy shim sources, and the original CLI packer source
- `Package.swift`: legacy Swift package manifest kept for compatibility with the moved legacy sources
- `samples/`: example `.ims` banks
- `third_party/`: vendored dependency sources

## Author

Masami Komuro

## Licensing

iMWrap v6 is distributed under the GNU Lesser General Public License v3.0 or later.
See `LICENSE.LESSER` and `LICENSE.GPL`.

Vendored dependency licenses are tracked in `THIRD_PARTY_LICENSES.md` and in the
upstream license files kept under `third_party/`.
