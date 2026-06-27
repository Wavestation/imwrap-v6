# Detailed Reference for iMUSE / iMWrap SysEx

This document is a detailed technical reference for the SysEx messages recognized by `imwrap-v6`.

It is based primarily on the actual source code behavior, especially:

- `src/IMWrapSysex.cpp` for parsing and encoding
- `src/IMWrapEngine.cpp` for runtime effects
- `src/Instrument.cpp` and `src/AgsIMWrapPlugin.cpp` for MT-32 and AdLib specifics

When historical documentation and the code disagree, this document follows the code.

## 1. Scope

This document covers:

- the general iMWrap / iMUSE SysEx format
- the detailed syntax of each recognized message
- the actual runtime effect of each message in the engine
- the practical status of each message: mandatory, recommended, or optional
- iMWrap-specific details related to MT-32 and AdLib formats

It does not fully document all standard MIDI events (`Note On`, `CC`, `Program Change`, tempo meta events, and so on), except where they interact with SysEx behavior.

## 2. General Structure

The canonical form of an iMWrap SysEx is:

```text
F0 7D [CODE] [PAYLOAD...] F7
```

- `F0`: standard MIDI SysEx start
- `7D`: non-commercial manufacturer id used here by iMWrap / iMUSE
- `CODE`: command id
- `PAYLOAD`: command arguments
- `F7`: standard MIDI SysEx end

### 2.1. Parser Tolerance

The internal parser strips a leading `F0` and a trailing `F7` if they are present.

In practice, it therefore accepts:

- a full `F0 ... F7` message
- or directly the body `7D [CODE] [PAYLOAD...]`

This is an implementation detail. For real MIDI authoring, always write the full form with `F0` and `F7`.

### 2.2. Nibble Encoding

Many parameters are nibble-encoded to stay compatible with the 7-bit MIDI transport.

Example:

```text
byte 5A -> 05 0A
byte AB -> 0A 0B
uint16 000A -> 00 00 00 0A
```

The parser reconstructs each byte from the low 4 bits of two successive bytes.

## 3. General Conventions

### 3.1. Part and Channel Indices

- `part`: logical iMUSE / iMWrap part, `0..15`
- `chan`: same general idea on the logical part/channel side, `0..15`

In this engine, MIDI sequence events are dispatched by logical part index, not by strict General MIDI channel semantics.

### 3.2. Endianness

The 16-bit integers decoded from nibbles are read as big-endian.

Example:

```text
00 0A -> 10
01 2C -> 300
```

### 3.3. Signed Values

Several fields are signed:

- `pan` in `0x00`
- `detune`
- `signedValue` in `0x31` and `0x35`

The code does not always handle them the way a sequencer-oriented document might suggest. The exact behavior is described message by message below.

## 4. Overview of Recognized Messages

| Code | Name | Runtime Effect | Practical Status |
| --- | --- | --- | --- |
| `0x00` | Allocate Part | Allocates/configures a part | Almost mandatory for a real iMUSE track |
| `0x01` | Shutdown Part | Stops and resets a part | Optional |
| `0x02` | Start Song | Resets all parts for the sound | Recommended at tick 0 |
| `0x10` | AdLib Part Instrument | Loads an AdLib instrument onto one part | Optional |
| `0x11` | AdLib Global Instrument | Broadcasts an AdLib instrument to all active parts | Optional |
| `0x21` | Parameter Adjust | No current runtime effect | Optional, currently inert |
| `0x30` | Hook Jump | Smart conditional jump | Optional |
| `0x31` | Hook Global Transpose | Transposes the full sound | Optional |
| `0x32` | Hook Part On/Off | Turns a part on/off | Optional |
| `0x33` | Hook Set Volume | Changes a part volume | Optional |
| `0x34` | Hook Set Program | Changes a part instrument | Optional |
| `0x35` | Hook Set Transpose | Changes a part transpose | Optional |
| `0x40` | Marker | Triggers markers / command cues | Optional |
| `0x50` | Set Loop | Arms a time-based loop | Optional |
| `0x51` | Clear Loop | Clears the current loop | Optional |
| `0x60` | Set Instrument | Assigns an instrument id `bank<<8 | program` | Optional |

## 5. Detailed Message Reference

### 5.1. `0x00` Allocate Part

#### Canonical Syntax

```text
F0 7D 00 [PART] [8 bytes encoded as 16 nibbles] F7
```

The parser also accepts a shorter form, but the useful canonical form is the 8-byte variant.

#### Decoded Structure

| Byte | Name | Interpretation |
| --- | --- | --- |
| `b0` | flags | bit 0 = `partOn`, bit 1 = `reverb` |
| `b1` | priority | raw priority |
| `b2` | volume | part volume |
| `b3` | pan | signed 8-bit value |
| `b4` | trans/perc | bit 7 = percussion, bits 0-6 = signed 7-bit transpose |
| `b5` | detune | signed 8-bit value |
| `b6` | pitchbendFactor | pitch bend range |
| `b7` | program | base program |

#### Example

```text
F0 7D 00 03 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
```

This configures part `03` with:

- `partOn = 1`
- `reverb = 0`
- raw priority `0x5A`
- volume `0x7F`
- pan `0`
- transpose `0`
- detune `0`
- pitch bend factor `2`
- program `0`

#### Runtime Effect

The engine:

- creates or fetches the logical part
- marks it as present
- applies on/off, reverb, volume, transpose, pan, priority, percussion, detune, and pitch bend factor
- then calls `partSetProgram()`
- allocates a physical channel if needed

#### Pitfalls

- `pan` is read as an `int8_t`, then clamped to `[-64, +63]`.
  So `00` is the logical center in this engine.
  The common "0..127 centered on 64" convention is not what this code actually does.
- The priority field is not used directly: the engine converts `priority` into `priority - 0x40` to obtain an internal signed priority.
  In practice, `0x40` behaves like the neutral point.
- The high nibble of `b0` may be emitted by the encoder, but the runtime does not give it a dedicated meaning.

#### Practical Status

For a truly part-controlled iMUSE track, `0x00` is the most important message.

It is not strictly mandatory to get sound at all, because parts can auto-initialize on the first `Note On`, but it is practically essential for serious authoring.

### 5.2. `0x01` Shutdown Part

#### Syntax

```text
F0 7D 01 [PART] F7
```

#### Example

```text
F0 7D 01 03 F7
```

#### Runtime Effect

The engine:

- stops the part
- stops its notes
- releases its physical channel
- resets its state

#### Practical Status

Optional. Useful for explicit transitions or shutdowns.

### 5.3. `0x02` Start Song

#### Syntax

```text
F0 7D 02 F7
```

#### Runtime Effect

Resets all parts of the current sound:

- notes are stopped
- channels are released
- part state is cleared

#### Pitfalls

- If you place this message in the middle of a track, it clears the state of every part.
- The parser tolerates extra bytes, but they have no useful meaning.

#### Practical Status

Recommended at tick `0` to make the sequence start explicit and robust.

### 5.4. `0x10` AdLib Part Instrument

#### Syntax

```text
F0 7D 10 [PART] [UNKNOWN] [AdLib data in nibbles...] F7
```

#### Runtime Effect

- if the decoded data is at least `11` bytes long, the engine treats it as an AdLib instrument and applies it to the part
- if the data is shorter but non-empty, the engine may do a limited `Program Change` fallback

#### Authoring Recommendation

Even though this runtime path accepts `>= 11` bytes, an iMUSE canonical AdLib instrument should be treated as `30` bytes.

See the AdLib appendix for the detailed structure.

#### Practical Status

Optional. Reserved for AdLib tracks or variants.

### 5.5. `0x11` AdLib Global Instrument

#### Syntax

```text
F0 7D 11 [UNKNOWN] [VALUE] [PROGRAM] [AdLib data in nibbles...] F7
```

#### Runtime Effect

If the decoded data is at least `11` bytes long, the engine broadcasts this AdLib instrument to every active part of the sound.

#### Notes

- `UNKNOWN`, `VALUE`, and `PROGRAM` are all parsed
- their concrete role in the current runtime is secondary compared with the AdLib payload itself

#### Practical Status

Optional.

### 5.6. `0x21` Parameter Adjust

#### Syntax

```text
F0 7D 21 [PART] [UNKNOWN] [PARAM16 in 4 nibbles] [VALUE16 in 4 nibbles] F7
```

#### Example

```text
F0 7D 21 03 00 00 00 01 05 00 00 06 04 F7
```

#### Runtime Effect

The message is parsed correctly, but the engine returns immediately without performing any action.

#### Conclusion

In the current state of `imwrap-v6`, this message has no runtime effect.

#### Practical Status

Optional, and not recommended if you need a real effect today.

### 5.7. `0x30` Hook Jump

#### Syntax

```text
F0 7D 30 [UNKNOWN] [CMD] [TRACK16] [BEAT16] [TICK16] F7
```

The 7 bytes `[CMD][TRACK16][BEAT16][TICK16]` are nibble-encoded.

#### Example

```text
F0 7D 30 00 00 07 00 00 00 00 00 00 00 0A 00 00 00 00 F7
```

#### Runtime Effect

Triggers a smart conditional jump:

- checks the hook condition
- schedules any needed remaining note-offs
- performs the jump
- releases sustain pedals if needed

#### Practical Status

Optional. Use it when the game arms hooks on the runtime side.

### 5.8. `0x31` Hook Global Transpose

#### Syntax

```text
F0 7D 31 [UNKNOWN] [CMD] [RELATIVE] [SIGNED_VALUE] F7
```

The 3 bytes `[CMD][RELATIVE][SIGNED_VALUE]` are nibble-encoded.

#### Runtime Effect

Transposes the entire sound.

- `RELATIVE = 00`: absolute value
- `RELATIVE != 00`: relative transpose

#### Pitfalls

- In relative mode, the engine first folds the value into a musical window around `[-7, +7]`, then applies internal clamps.
- The engine does recompute effective part transpositions, but this path does not explicitly push an immediate per-part MIDI refresh here.

#### Practical Status

Optional.

### 5.9. `0x32` Hook Part On/Off

#### Syntax

```text
F0 7D 32 [CHAN] [CMD] [VALUE] F7
```

`[CMD][VALUE]` are nibble-encoded.

#### Runtime Effect

- `VALUE = 00`: turns the part off
- `VALUE != 00`: turns the part on

#### Practical Status

Optional.

### 5.10. `0x33` Hook Set Volume

#### Syntax

```text
F0 7D 33 [CHAN] [CMD] [VALUE] F7
```

#### Runtime Effect

Changes the volume of the targeted part.

The useful value range is `0..127`.

#### Practical Status

Optional.

### 5.11. `0x34` Hook Set Program

#### Syntax

```text
F0 7D 34 [CHAN] [CMD] [VALUE] F7
```

#### Runtime Effect

Changes the program of the targeted part.

In MT-32 mode, this may trigger:

- an MT-32 patch memory update
- extra refresh `Program Change` messages

#### Practical Status

Optional.

### 5.12. `0x35` Hook Set Transpose

#### Syntax

```text
F0 7D 35 [CHAN] [CMD] [RELATIVE] [SIGNED_VALUE] F7
```

#### Runtime Effect

Transposes the targeted part.

- `RELATIVE = 00`: absolute value
- `RELATIVE != 00`: relative value

#### Notes

In relative mode, the engine first performs a musical fold on `[-7, +7]`, then a final clamp on `[-24, +24]`.

#### Practical Status

Optional.

### 5.13. `0x40` Marker

#### Syntax

```text
F0 7D 40 [UNKNOWN] [MARKER_BYTES...] F7
```

#### Robust Example

```text
F0 7D 40 00 01 F7
```

#### Actual Runtime Effect

The engine does not treat the full payload as one string marker.

It iterates over each byte of `MARKER_BYTES` and calls `handleMarker()` once per byte.

#### Practical Consequence

- If you send `"INTRO"`, you get five successive triggers: `I`, `N`, `T`, `R`, `O`.
- If you want one logical trigger id, use one useful byte only.

#### Practical Status

Optional, but very useful for game/music synchronization.

### 5.14. `0x50` Set Loop

#### Syntax

```text
F0 7D 50 [UNKNOWN] [COUNT16] [TO_BEAT16] [TO_TICK16] [FROM_BEAT16] [FROM_TICK16] F7
```

The 10 bytes are encoded as 20 nibbles.

#### Runtime Semantics

The engine loops:

- when it reaches `FROM_BEAT:FROM_TICK`
- by jumping to `TO_BEAT:TO_TICK`

In other words:

- `FROM` = loop trigger point
- `TO` = loop return point

#### Example

```text
F0 7D 50 00 00 00 00 02 00 00 00 04 00 00 00 00 00 00 00 0A 00 00 00 00 F7
```

This means:

- `count = 2`
- when reaching `beat 10 tick 0`
- jump to `beat 4 tick 0`

#### Important Pitfalls

- Contrary to some historical docs, `count = 0` does not mean "infinite loop" in this code.
  With `count = 0`, there is no active loop.
- The engine refuses loops if `toBeat + 1 >= fromBeat`.
  In practice, `FROM` must be strictly later than `TO`.

#### Practical Status

Optional.

### 5.15. `0x51` Clear Loop

#### Syntax

```text
F0 7D 51 F7
```

#### Runtime Effect

Sets `loopCounter` to `0` and clears the current loop.

#### Notes

The parser tolerates extra payload bytes, but they have no useful role.

#### Practical Status

Optional.

### 5.16. `0x60` Set Instrument

#### Syntax

```text
F0 7D 60 [CHAN] [N1] [N2] [N3] [N4] F7
```

The 4 nibbles reconstruct a `uint16`:

```text
instrument = (N1 << 12) | (N2 << 8) | (N3 << 4) | N4
```

#### Runtime Interpretation

The engine does:

- `bank = high byte`
- `program = low byte`

So:

```text
0x007C -> bank 0x00, program 0x7C
0x1234 -> bank 0x12, program 0x34
```

#### Example

```text
F0 7D 60 02 00 00 07 0C F7
```

#### Practical Status

Optional.

## 6. Hooks: General Usage Rule

All hooks `0x30..0x35` follow the same principle:

- the game arms a hook state through `PlayerSetHook` / `iMWrap_SetHook`
- the sequence then contains a conditional hook SysEx
- if the current hook state matches `CMD`, the action runs

### 6.1. `CMD` Values

- `00`: unconditional
- `01..7F`: conditional and consumable
- `80..FF`: conditional and not consumed

In the engine's initial state, hooks are reset to `FF`.

In practice, a sequenced hook with `CMD = FF` behaves like a persistent hook armed by default.

## 7. Which Messages Are Mandatory?

### 7.1. Simple MIDI Playback

No iMUSE SysEx is strictly mandatory for a MIDI file to produce sound through this engine.

Parts can auto-initialize on the first MIDI activity.

### 7.2. Properly Authored iMUSE Track

In practice:

- `0x02` at tick `0`: recommended
- `0x00` for each logical part you use: strongly recommended

### 7.3. Optional Messages Depending on Need

- `0x40` if you need triggers
- `0x50` / `0x51` if you need loops
- `0x30..0x35` if you need hook-driven interactivity
- `0x10` / `0x11` if you work in AdLib
- `0x60` if you use the `bank/program` instrument id path

### 7.4. Recognized but Currently Inert

- `0x21 Parameter Adjust`

## 8. Ready-to-Copy Templates

### 8.1. Minimal iMUSE Track Start

```text
F0 7D 02 F7
F0 7D 00 00 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
F0 7D 00 01 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 01 F7
```

### 8.2. Simple One-Byte Marker

```text
F0 7D 40 00 01 F7
```

### 8.3. Counted Loop

```text
F0 7D 50 00 00 00 00 03 00 00 00 01 00 00 00 00 00 00 00 08 00 00 00 00 F7
```

This means:

- loop `3` times
- trigger at `beat 8`
- jump back to `beat 1`

Adjust `TO` and `FROM` to match your musical structure.

## Appendix A. MT-32: Format and iMWrap-Specific Adaptations

This appendix covers two separate things:

- real Roland MT-32 SysEx transported inside the sequence
- iMWrap internal adaptations around custom timbres

### A.1. The "Real Roland SysEx" Path

If a SysEx event is not recognized as an iMWrap `7D` SysEx, the engine treats it as an external SysEx.

If it starts with `41`, it is considered a potential Roland SysEx.

The engine then tries to associate this SysEx with a logical part using the low nibble of the second byte in the payload.

Example of the general form:

```text
F0 41 [xx] 16 12 [addr1] [addr2] [addr3] [data...] [checksum] F7
```

### A.2. iMWrap Convention for Part Association

The code does:

```text
logicalPart = payload[1] & 0x0F
```

This means, as inferred from the runtime code, that the low nibble of the second byte of the Roland SysEx is used as the logical part id when iMWrap tries to attach the dump to a part.

This is not a universal Roland MIDI convention. It is an iMWrap usage convention inferred from the engine.

### A.3. Internal Storage

When that association succeeds, the engine stores the raw Roland payload into:

```text
part.customRolandSysex
```

If the part is already routed to a melodic MT-32 physical channel (`1..8`), the engine may immediately re-emit an adapted version of this timbre.

### A.4. MT-32 Format Generated by iMWrap

When iMWrap builds an MT-32 SysEx itself, it uses this structure:

```text
41 10 16 12 [addr_hi] [addr_mid] [addr_lo] [data...] [checksum] F7
```

The checksum is computed as the negative sum of the bytes starting from the address bytes, then masked to 7 bits.

### A.5. Custom Timbre Rewriting

When a stored Roland dump is reapplied to an MT-32 part, the engine expects:

- at least `254` bytes of raw payload
- an optional trailing `F7`
- a useful `246`-byte timbre block after the header/address/checksum section

The processing is:

1. extract the `246` timbre bytes
2. write them into the target physical channel `Memory Timbre` slot:

```text
addr = 0x20000 + (physicalChannel << 8)
```

3. update the physical channel `Patch Memory`:

```text
addr = 0x14000 + (physicalChannel << 3)
data = 02 [physicalChannel]
```

4. send "dummy then real" `Program Change` messages to force the MT-32 to reload the patch

### A.6. MT-32 Factory Timbres

When there is no `customRolandSysex`, iMWrap simply writes the factory timbre selection into `Patch Memory`:

```text
addr = 0x14000 + (physicalChannel << 3)
data = [program >> 6] [program & 0x3F]
```

Then it sends a refresh `Program Change`.

### A.7. iMWrap Internal Roland Instrument

Besides real Roland SysEx present in the sequence, iMWrap also has an internal Roland timbre abstraction:

- internal type `ROL `
- internal blob size `258` bytes
- timbre name read as 10 characters from offset `7`

This internal format is not a MIDI SysEx to write directly into a track.

It is used for exchanging data between the `Instrument` object and `MidiSink` backends.

### A.8. Practical MT-32 Advice

- Place custom Roland SysEx dumps before `Allocate Part`.
- Leave some timing margin, ideally around fifty ticks, before the first notes or first part allocation.
- If you want the engine to attach a dump to a logical part, respect the observed association convention on the second payload byte.

## Appendix B. AdLib: iMWrap Syntax and Useful Layout

### B.1. Two Different Mechanisms

AdLib support in iMWrap appears in two forms:

- the iMWrap SysEx messages `0x10` and `0x11`
- an internal custom instrument abstraction of type `ADL `

### B.2. Canonical Format of an iMUSE AdLib Instrument

The AdLib sink comment documents a canonical `30`-byte layout for an iMUSE OPL2 instrument:

| Bytes | Role |
| --- | --- |
| `0..4` | modulator operator: `avekf`, `ksl_tl`, `ar_dr`, `sl_rr`, `waveform` |
| `5..9` | carrier operator: `avekf`, `ksl_tl`, `ar_dr`, `sl_rr`, `waveform` |
| `10` | `C0` register: feedback + connection |
| `11..29` | flags / extra / additional data |

The basic AdLib sink in `imwrap-v6` explicitly uses only the standard melodic 2-operator configuration.

### B.3. `0x10` AdLib Part Instrument

Format:

```text
F0 7D 10 [PART] [UNKNOWN] [30 AdLib bytes in nibbles] F7
```

Recommendation:

- treat `30` decoded bytes as the canonical authoring format
- even if some engine paths are more permissive, do not rely on truncated payloads in real authoring

### B.4. `0x11` AdLib Global Instrument

Format:

```text
F0 7D 11 [UNKNOWN] [VALUE] [PROGRAM] [30 AdLib bytes in nibbles] F7
```

Usage:

- same data family as `0x10`
- broadcast to all active parts

### B.5. What the iMWrap AdLib Sink Does

When the internal `ADL ` instrument is sent to the AdLib backend:

- iMWrap creates a dedicated custom bank
- `MSB = 0x7D`
- `LSB = channel`
- the instrument is stored in slot `0` of that bank
- the part then switches to that bank and patch

In implementation terms:

- iMWrap custom AdLib bank = `MSB 0x7D`
- sub-bank = logical channel number
- patch used = `0`

### B.6. `0x60 Set Instrument` and AdLib

`0x60` is not the same mechanism as `0x10` / `0x11`.

- `0x10` / `0x11`: transport a real OPL instrument payload
- `0x60`: assigns a `bank/program` id

For AdLib authoring centered on OPL register content, the key messages are `0x10` and `0x11`, not `0x60`.

## Appendix C. Important Differences Between Simplified Docs and Code

### C.1. `0x21 Parameter Adjust`

- simplified docs: often presented as dynamic
- current code: recognized, but no runtime effect

### C.2. `0x40 Marker`

- simplified docs: may suggest one text marker
- current code: one useful payload byte = one trigger

### C.3. `0x50 Set Loop`

- simplified docs: may suggest `count = 0` means infinite loop
- current code: `count = 0` disables looping

### C.4. `0x00 Allocate Part` and Pan

- simplified docs: often described with `0..127` logic
- current code: reads `pan` as a signed value centered on `0`

## 9. Final Authoring Recommendation

For a stable and predictable iMWrap track:

1. place `0x02` at the very beginning
2. place one `0x00` for each part you use
3. use `0x21` only if you accept that it currently has no effect
4. use `0x40` with one useful byte per trigger
5. for `0x50`, use `count >= 1`
6. for custom MT-32, place dumps early enough before musical activation
7. for AdLib, treat `30` bytes as the canonical instrument size
