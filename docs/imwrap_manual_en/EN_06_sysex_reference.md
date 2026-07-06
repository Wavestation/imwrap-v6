# Chapter 6: Detailed SysEx Reference

For those who want to understand the bits and bytes, this chapter details the structure of the SysEx commands exclusive to iMWrap. 

These messages always start with the manufacturer ID `F0 7D` (Non-commercial/Educational), followed by the specific command.

> [!WARNING]
> All parameters sent in MIDI SysEx must be "nibblized" (split into 4-bit packets) if their value can exceed 127, because a standard MIDI byte cannot exceed `0x7F`.

---

## 6.1. Allocate Part (`0x00`)
Dynamically initializes a channel. Essential for MT-32 mode, also useful in GM to define default mixing.

**Format**: `F0 7D 00 [Flags1] [Flags2] [Priority] [Volume] [Pan] [Transpose] [Detune] [Pitchbend] [Program] F7`

- **Flags1**: Bitmask (Part On, Reverb, Percussion).
- **Flags2**: Reserved.
- **Priority**: 0-127.
- **Pan**: 0-127 (64 is center).

---

## 6.2. Parameter Adjust (`0x21`)
Modifies a parameter of an already allocated part on the fly.

**Format**: `F0 7D 21 [Part/Channel] [ParamID] [Value...] F7`

*Note: This feature is historically present in iMUSE but currently ignored by the iMWrap runtime. Use AGS script commands instead.*

---

## 6.3. Hooks (`0x3X`)
Hooks are waiting points.

**Format**: `F0 7D [HookClass] [ConditionalFlag] F7`
- `0x30`: Jump Hook
- `0x32`: Part On/Off Hook
- `0x33`: Set Volume Hook
- `0x34`: Set Program Hook
- `0x35`: Transpose Hook

If `ConditionalFlag` is 1, the hook persists after being triggered. If 0, it is consumed.

---

## 6.4. Marker (`0x40`)
Sends a trigger to the AGS game engine.

**Format**: `F0 7D 40 [MarkerID] F7`
- **MarkerID**: 0-127.

---

## 6.5. Set Loop (`0x50`)
Defines an internal sequencer loop.

**Format**: `F0 7D 50 [Count] [ToBeat_MSB] [ToBeat_LSB] [ToTick_MSB] [ToTick_LSB] [FromBeat_MSB] [FromBeat_LSB] [FromTick_MSB] [FromTick_LSB] F7`

- **Count**: Number of repetitions (0 = infinite loop).
- **From**: Start point of the loop (where the playback head currently is).
- **To**: Return point of the loop (where it jumps back to).

> [!CAUTION]
> The `FROM` measure must strictly be greater than the `TO` measure. You cannot loop forward in time.

---

## 6.6. Native AdLib Payload (`0x10`)
Allows sending raw OPL register values to bypass MIDI and directly program the AdLib chip.

**Format**: `F0 7D 10 [Register] [Value] F7`
