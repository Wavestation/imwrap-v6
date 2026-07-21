# iMWrap 1.0.4

Date: 2026-06-29

Version 1.0.4 focuses on the Windows tools, the AdLib/OPL workflow, and clearer MIDI event inspection in the GUI.

## What's New

- Explorer and Player now recognize standard MIDI events instead of showing most of them as raw data.
- The main event types now identified are `Note On`, `Note Off`, `Control Change`, `Program Change`, `Pitch Bend`, plus the most useful `Meta` events.
- The Explorer SysEx editor and the SysEx generator can now import and export OPL timbres in `SBI` format.
- The `SBI` to iMUSE AdLib mapping follows ScummVM's AdLib instrument interpretation as closely as possible.
- Explorer and Player can now audition AdLib variants directly through embedded OPL audio playback inside the application.

## Improvements

- The Windows GUI tools now switch automatically to an embedded AdLib audio backend for OPL variants, without requiring an external MIDI device.
- AdLib SysEx data edited in Explorer can now be converted more easily to `SBI` timbres for use in other Sound Blaster compatible tools.
- The Player event view is much more useful when reviewing sequences, tracking controller changes, and checking markers and meta events.

## Compatibility Notes

- `SBI` only stores the core OPL timbre data. Extended bytes specific to the iMUSE AdLib format cannot all be preserved in `SBI` exports.
- When an `SBI` export drops non-representable extended data, the tools report it.

## Windows Bundle

The Windows bundle for this version includes rebuilt GUI binaries for:

- `imwrap_explorer_gui-x64.exe`
- `imwrap_player_gui-x64.exe`
- `imwrap_sysex_gui-x64.exe`

Recommended archive name:

`imwrap-v1.0.4-windows.zip`
