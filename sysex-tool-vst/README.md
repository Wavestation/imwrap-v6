# iMWrap SysEx Tool VST3

This directory hosts a proof-of-concept VST3 version of the iMWrap SysEx generator.

Current scope:

- VST3 instrument-style SysEx tool built on the official Steinberg VST3 SDK
- Silent stereo audio output so DAWs can expose it in their instrument menus
- Embedded native Win32 editor for the supported iMWrap SysEx message set
- Numeric iMWrap SysEx messages only in this first pass:
  - Allocate Part
  - Shutdown Part
  - Start Song
  - Parameter Adjust
  - Hook Jump
  - Hook Global Transpose
  - Hook Part On/Off
  - Hook Set Volume
  - Hook Set Program
  - Hook Set Transpose
  - Set Loop
  - Clear Loop
  - Set Instrument

Not supported yet in this VSTi editor:

- Marker text
- AdLib register payloads
- Full parity with the standalone desktop SysEx tool tabs and guide content

## Build

Point `IMWRAP_VST3_SDK_DIR` to a local checkout of the official SDK:

- Repository: [steinbergmedia/vst3sdk](https://github.com/steinbergmedia/vst3sdk)

Example configure command on Windows:

```powershell
cmake -S . -B .build\vst3-x64 -A x64 `
  -DIMWRAP_BUILD_GUI_TOOLS=ON `
  -DIMWRAP_BUILD_AGS_PLUGIN=OFF `
  -DIMWRAP_BUILD_VST3_TOOL=ON `
  -DIMWRAP_VST3_SDK_DIR=D:\Prog\third_party\vst3sdk
```

Then build the plugin target:

```powershell
cmake --build .build\vst3-x64 --config Release --target imwrap_sysex_tool
```

## Release Packaging

When the target is built, the Windows packaging script looks for an `imwrap_sysex_tool.vst3` bundle under the x64 build tree and copies it into `final-release\imwrap-windows\tools\`.
