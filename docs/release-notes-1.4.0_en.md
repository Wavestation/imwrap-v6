# iMWrap 1.4.0 (Major Revision)

Date: 2026-06-30

This major version 1.4.0 fixes a long-standing and critical OPL2 operator mapping issue in the AdLib emulation that previously caused corrupted and severely degraded audio rendering.

## New Features & Major Fixes

- **AdLib (OPL) Emulation Fix**: Resolved a critical swap between the Modulator and Carrier parameters when converting iMUSE timbres to the `ADL_Instrument` structure of **libADLMIDI**.
- **Mapping Alignment**: Carrier parameters (`data[5..9]`) are now correctly assigned to `operators[0]`, and Modulator parameters (`data[0..4]`) are assigned to `operators[1]`, aligning perfectly with libADLMIDI requirements.
- This correction is applied uniformly across the entire repository:
  - Inside the AGS plugin (`agsimwrap-x32.dll`)
  - Inside the native shim (`imwrap_shim.dll` / `imwrap_shim_x64.dll`)
  - Inside the GUI preview tools (Explorer, Player, SysEx Editor)

## Improvements

- The audio rendering of AdLib/OPL variants is now fully faithful to the original iMUSE instrument patches and soundtracks of DOS-era LucasArts classics (such as *Monkey Island 2* or *Day of the Tentacle*).

## Windows Bundle

The Windows bundle for this version includes corrected and rebuilt binaries for:

- `agsimwrap-x32.dll` (AGS Plugin)
- `imwrap_explorer_gui-x64.exe`
- `imwrap_player_gui-x64.exe`
- `imwrap_sysex_gui-x64.exe`

Recommended archive name:

`imwrap-v1.4.0-windows.zip`
