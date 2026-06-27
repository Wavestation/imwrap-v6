# iMWrap SysEx Message Format

This document is a short and reliable reference for the SysEx messages recognized by `imwrap-v6`.

For the detailed version, see [imwrap_sysex_reference_detaillee_en.md](./imwrap_sysex_reference_detaillee_en.md).

## 1. General Form

All iMWrap SysEx messages use this form:

```text
F0 7D [CODE] [PAYLOAD...] F7
```

- `F0`: SysEx start
- `7D`: iMWrap / iMUSE identifier
- `CODE`: command code
- `PAYLOAD`: parameters
- `F7`: SysEx end

### Nibble Encoding

Many values are nibble-encoded:

```text
5A -> 05 0A
AB -> 0A 0B
000A -> 00 00 00 0A
```

## 2. Main Messages

### `0x00` Allocate Part

Canonical form:

```text
F0 7D 00 [PART] [8 bytes encoded as 16 nibbles] F7
```

Decoded payload:

1. `Flags`: bit 0 = part on, bit 1 = reverb
2. `Priority`
3. `Volume`
4. `Pan`
5. `Trans/Perc`: bit 7 = percussion, bits 0-6 = transpose
6. `Detune`
7. `PitchBendFactor`
8. `Program`

Effect: allocates and configures a logical part.

Important note: in the current engine, `pan` is interpreted as a signed value centered on `0`, not as `0..127` centered on `64`.

### `0x01` Shutdown Part

```text
F0 7D 01 [PART] F7
```

Effect: stops the part and resets its state.

### `0x02` Start Song

```text
F0 7D 02 F7
```

Effect: resets all parts of the current sound.

Recommended usage: at the very beginning of the track, at tick `0`.

### `0x21` Parameter Adjust

```text
F0 7D 21 [PART] [UNKNOWN] [PARAM16] [VALUE16] F7
```

Current effect in `imwrap-v6`: none.

The message is parsed, but ignored by the runtime engine.

### `0x30` Hook Jump

```text
F0 7D 30 [UNKNOWN] [CMD] [TRACK16] [BEAT16] [TICK16] F7
```

Effect: smart conditional jump.

### `0x31` Hook Global Transpose

```text
F0 7D 31 [UNKNOWN] [CMD] [RELATIVE] [SIGNED_VALUE] F7
```

Effect: transposes the full sound.

### `0x32` Hook Part On/Off

```text
F0 7D 32 [CHAN] [CMD] [VALUE] F7
```

Effect:

- `VALUE = 00`: turns the part off
- `VALUE != 00`: turns the part on

### `0x33` Hook Set Volume

```text
F0 7D 33 [CHAN] [CMD] [VALUE] F7
```

Effect: changes a part volume.

### `0x34` Hook Set Program

```text
F0 7D 34 [CHAN] [CMD] [VALUE] F7
```

Effect: changes a part instrument.

### `0x35` Hook Set Transpose

```text
F0 7D 35 [CHAN] [CMD] [RELATIVE] [SIGNED_VALUE] F7
```

Effect: changes a part transpose.

### `0x40` Marker

```text
F0 7D 40 [UNKNOWN] [MARKER_BYTES...] F7
```

Actual engine behavior: one trigger per useful byte.

Robust example:

```text
F0 7D 40 00 01 F7
```

If you send a text like `INTRO`, the engine fires five successive markers, one per character.

### `0x50` Set Loop

```text
F0 7D 50 [UNKNOWN] [COUNT16] [TO_BEAT16] [TO_TICK16] [FROM_BEAT16] [FROM_TICK16] F7
```

Actual effect:

- when playback reaches `FROM_BEAT:FROM_TICK`
- it jumps to `TO_BEAT:TO_TICK`

Important:

- in the current code, `COUNT = 0` does not mean "infinite"
- `COUNT = 0` disables the loop
- `FROM` must be strictly later than `TO`

Example:

```text
F0 7D 50 00 00 00 00 02 00 00 00 04 00 00 00 00 00 00 00 0A 00 00 00 00 F7
```

Here:

- loop `2` times
- when reaching `beat 10 tick 0`
- jump to `beat 4 tick 0`

### `0x51` Clear Loop

```text
F0 7D 51 F7
```

Effect: clears the current loop.

### `0x60` Set Instrument

```text
F0 7D 60 [CHAN] [N1] [N2] [N3] [N4] F7
```

The 4 nibbles form a `uint16`, interpreted as:

- high byte = `bank`
- low byte = `program`

Example:

```text
F0 7D 60 02 00 00 07 0C F7
```

corresponds to:

- `bank = 0x00`
- `program = 0x7C`

## 3. AdLib Messages

### `0x10` AdLib Part Instrument

```text
F0 7D 10 [PART] [UNKNOWN] [ADLIB_DATA in nibbles] F7
```

### `0x11` AdLib Global Instrument

```text
F0 7D 11 [UNKNOWN] [VALUE] [PROGRAM] [ADLIB_DATA in nibbles] F7
```

For authoring, a canonical AdLib instrument should be treated as `30` bytes.

See the AdLib appendix in the detailed reference for the full layout.

## 4. Roland MT-32 Messages

The engine can also carry native Roland MT-32 SysEx, separate from the `7D` SysEx family.

These messages are not part of the `0x00..0x60` family, but they are used for:

- custom timbres
- MT-32 initialization
- patch memory updates

See the MT-32 appendix in the detailed reference for the exact structure.

## 5. What Is Actually Mandatory

To produce sound at all, no iMWrap SysEx is strictly mandatory.

For a properly authored iMUSE track, recommended:

1. `0x02` at tick `0`
2. one `0x00` for each logical part you use

Then:

- `0x40` if you need triggers
- `0x50` / `0x51` if you need loops
- `0x30..0x35` if you use hooks
- `0x10` / `0x11` if you author for AdLib

## 6. Minimal Track Start

```text
F0 7D 02 F7
F0 7D 00 00 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
F0 7D 00 01 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 01 F7
```

## 7. Important Differences from Some Simplified Docs

- `0x21 Parameter Adjust`: recognized, but currently no runtime effect
- `0x40 Marker`: one useful byte = one trigger
- `0x50 Set Loop`: `count = 0` is not an infinite loop in this code
- `0x00 Allocate Part`: `pan` is treated as a signed value centered on `0`
