# iMWrap Composer Guide

This guide is a short, practice-oriented version for authoring iMWrap SysEx in a sequencer.

For the full specification, see:

- [imwrap_sysex_format_en.md](./imwrap_sysex_format_en.md)
- [imwrap_sysex_reference_detaillee_en.md](./imwrap_sysex_reference_detaillee_en.md)

## 1. The Simple Rule

If you are composing an interactive track for `imwrap-v6`, use this pattern:

1. at the very beginning, send `Start Song`
2. allocate each useful part with `Allocate Part`
3. place your normal MIDI notes afterwards
4. add `Marker`, `Set Loop`, and `Hooks` only where needed

The general form is always:

```text
F0 7D [CODE] [PAYLOAD] F7
```

## 2. The Minimal Kit

### 2.1. Track Reset

```text
F0 7D 02 F7
```

Place this message at tick `0`.

### 2.2. Part Allocation

Example of a minimal part:

```text
F0 7D 00 00 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
```

In practice this means:

- logical part `0`
- part enabled
- priority `0x5A`
- volume `0x7F`
- centered pan
- transpose `0`
- detune `0`
- pitch bend factor `2`
- program `0`

Then create a similar allocation for each logical part you need.

## 3. The Most Useful Messages in Composition

### 3.1. Marker

Format:

```text
F0 7D 40 00 [ID] F7
```

Example:

```text
F0 7D 40 00 01 F7
```

Important advice:

- use a single useful byte if you want one trigger
- do not send a full ASCII string if you expect one logical marker

In the current code, each useful byte triggers a separate marker.

### 3.2. Set Loop

Format:

```text
F0 7D 50 00 [COUNT16] [TO_BEAT16] [TO_TICK16] [FROM_BEAT16] [FROM_TICK16] F7
```

Example:

```text
F0 7D 50 00 00 00 00 02 00 00 00 04 00 00 00 00 00 00 00 0A 00 00 00 00 F7
```

Here:

- loop `2` times
- when reaching `beat 10 tick 0`
- jump to `beat 4 tick 0`

Things to remember:

- `count = 0` does not create an infinite loop in this engine
- use `count >= 1`
- the `FROM` point must be later than the `TO` point

To clear a loop:

```text
F0 7D 51 F7
```

### 3.3. Hooks

Hooks are used to condition an action on a state armed by the game.

The most useful ones are:

- `0x30`: jump
- `0x32`: part on/off
- `0x33`: volume
- `0x34`: program
- `0x35`: part transpose

In practice:

- `cmd = 00`: unconditional action
- `cmd = 01..7F`: conditional action
- `cmd = FF`: persistent hook armed by default in the engine reset state

If you do not drive hooks from the game side, you can ignore this family.

## 4. What You Should Avoid

### `0x21` Parameter Adjust

Do not rely on it today.

The current engine parses it, but does nothing with it.

### Long Text Marker

Avoid:

```text
F0 7D 40 00 49 4E 54 52 4F F7
```

if you want one logical trigger.

In this code, that fires five successive markers:

- `I`
- `N`
- `T`
- `R`
- `O`

## 5. Advice by Sound Profile

### General MIDI

The simplest setup:

- `Start Song`
- `Allocate Part`
- standard MIDI notes
- `Marker` / `Loop` if needed

### Roland MT-32

If you use custom Roland timbres:

- place Roland SysEx dumps before `Allocate Part`
- leave some margin, ideally around fifty ticks, before the first notes

The engine can then:

- store the dump per part
- reload it into MT-32 timbre memory
- refresh the active patch

### AdLib

If you work with OPL instruments:

- use `0x10` for one part
- use `0x11` for a global broadcast
- treat `30` bytes as the canonical size of an iMUSE AdLib instrument

## 6. Suggested Sequencer Workflow

### Control Track

Use a dedicated track, often track `0`, for SysEx:

- `Start Song`
- `Allocate Part`
- `Marker`
- `Set Loop`
- `Hooks`

### Musical Tracks

Then keep normal musical tracks for:

- notes
- standard CC messages
- `Program Change` when needed

## 7. Example of a Simple Structure

### Intro Then Loop

Tick `0`:

```text
F0 7D 02 F7
F0 7D 00 00 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
F0 7D 00 01 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 01 F7
```

At the loop entry point:

```text
F0 7D 40 00 01 F7
```

At the section exit point:

```text
F0 7D 50 00 00 00 00 03 00 00 00 08 00 00 00 00 00 00 00 10 00 00 00 00 F7
```

This last example means:

- loop `3` times
- when reaching `beat 16`
- jump to `beat 8`

## 8. When in Doubt

If you want to author quickly and cleanly:

1. use `Start Song` at the beginning
2. use `Allocate Part` for each part
3. use `Marker` with one useful byte
4. use `Set Loop` with `count >= 1`
5. leave `Parameter Adjust` aside

If you need the full byte-level view, refer to:

- [imwrap_sysex_format_en.md](./imwrap_sysex_format_en.md)
- [imwrap_sysex_reference_detaillee_en.md](./imwrap_sysex_reference_detaillee_en.md)
