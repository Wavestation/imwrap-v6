# IMS v1

The `.ims` file is an iMUSE bank format for AGS. The format is chunked, big-endian, and stays close to the spirit of SCUMM `SOUN` resources.

## Global Rules

- Each chunk starts with `FourCC(4)` followed by `u32be size`.
- `size` includes the 8-byte header.
- The full file is a root `IMSB` chunk.
- Tracks stored in `GMD ` or `ROL ` variants are type-2 SMF files carrying the original iMUSE SysEx events.

## Canonical Structure

```text
IMSB
  IHDR
  SDIR
  SOUN
  SOUN
  ...
```

## `IHDR`

16-byte body:

```text
u16be version_major
u16be version_minor
u32be sound_count
u32be flags
u32be reserved
```

For v1:

- `version_major = 1`
- `version_minor = 0`
- `flags = 0`
- `reserved = 0`

## `SDIR`

Compact index table. One entry = 12 bytes:

```text
u16be sound_id
u16be variant_mask
u32be sound_offset
u32be sound_size
```

Variant mask:

- `0x0001` = `GMD `
- `0x0002` = `ROL `

## `SOUN`

```text
SOUN
  SIDN
  NAME   ; optional
  GMD    ; optional, but at least one variant is required
  ROL    ; optional
```

## `SIDN`

4-byte body:

```text
u16be sound_id
u16be reserved
```

## `NAME`

Raw UTF-8 text, no mandatory terminator. Informational only.

## `GMD ` / `ROL `

The variant body may contain:

- an optional `MDhd` chunk;
- then a full MIDI stream starting with `MThd`.

The v1 loader preserves the MIDI stream byte-for-byte starting at `MThd`.

## `MDhd`

8-byte body:

```text
u8  reserved0
u8  reserved1
u8  priority
u8  volume
s8  pan
s8  transpose
s8  detune
u8  speed
```

Default values when `MDhd` is absent:

- `priority = 128`
- `volume = 127`
- `pan = 0`
- `transpose = 0`
- `detune = 0`
- `speed = 128`
