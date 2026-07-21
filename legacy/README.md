# Legacy Tools

This directory contains surfaces that are intentionally kept for compatibility,
historical reference, or downstream reuse, but are no longer part of the
maintained day-to-day toolchain.

Current legacy areas:

- `swift-app/`: retired SwiftUI authoring utilities.
- `swift-shim/`: legacy native shim used by wrapper/editor compatibility flows.
- `cli/imwrappack_v1.cpp`: the original command-line packer implementation.

Maintained replacements and active surfaces live elsewhere in the repository:

- `tools/imwrappack.cpp`: current command-line packer.
- `tools_gui/`: maintained Qt graphical tools.
- `include/imwrap/` and `src/`: maintained public library and engine sources.
