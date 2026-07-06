# Chapter 5: The iMWrap Composer's Guide

Welcome to the other side of the screen. If you are the composer, you have the power to make the music truly interactive. The AGS programmer will only be able to trigger magic if you have paved the way.

In iMWrap (as in the original iMUSE), interactivity is coded by inserting **SysEx (System Exclusive)** events into your usual sequencer (Cubase, Reaper, Logic, Digital Performer, etc.). Don't worry, you won't be absolutely forced to enter these messages by hand byte by byte (which is what I actually did at the very beginning of development). In fact, I wrote a tool, discussed in **Chapter 9**, that allows you to generate the messages directly by entering the necessary parameters.

---

## 5.1. Thinking and structuring game music focusing on interactivity

Unlike a linear piece (verse, chorus, end), an interactive adventure game music must be thought of in "blocks" or "stems".

- **The Stems (Channels)**: Think of your music by groups of instruments separated on distinct MIDI channels. The programmer, or yourself via hooks, will be able to turn these channels on and off (e.g., only activate the percussion if an enemy is near).
- **The Blocks**: Prepare long tracks that contain different variations of the same theme, aligned on the rhythmic grid (e.g., Calm theme from measure 1 to 20, Action theme from measure 21 to 40). You will use loops and jumps to navigate between these blocks. You can also use the concept of "Tracks" to put several transitions and/or variations in the same "Song" that will be played sequentially, thanks to the power of the **MIDI 2** file format.

### The Control Track Workflow
To keep your DAW (Digital Audio Workstation) session clear, **always reserve a distinct MIDI track (often the first or the last) for all your interactivity SysEx events**. Do not put any musical notes on it. 
Keep your other tracks (1, 2, 3...) purely musical (Notes, CC, Program Changes).
Minor exception: regarding Part declaration messages (Part Allocation), it is clearer and more appropriate to place them on the tracks concerned.

<img width="1034" height="353" alt="image" src="https://github.com/user-attachments/assets/04e578ac-7f1d-452b-865c-1ebd5708d039" />

---

## 5.2. Anatomy of an iMWrap SysEx

In the MIDI world, a SysEx message is traditionally used to send specific commands to a hardware synthesizer. iMWrap uses this channel to listen to your instructions.

All iMWrap messages follow this raw format (in hexadecimal):
```text
F0 7D [COMMAND_CODE] [PARAMETERS...] F7
```
- `F0`: Mandatory start of any MIDI SysEx.
- `7D`: The "iMUSE/iMWrap" identifier. The engine ignores all other SysEx (except MT-32 specifics).
- `COMMAND_CODE`: What you want to do (Allocate, Loop, etc.).
- `PARAMETERS`: The options (over several bytes).
- `F7`: Mandatory end.

> [!IMPORTANT]
> **Golden Rule: Everything is zero-indexed!**
> In the iMWrap ecosystem (like in C programming), values always start at `0`. 
> Thus, **Part 0** or **Channel 0** in your commands corresponds to **MIDI Channel 1** in your DAW sequencer. Always keep this one-unit offset in mind when writing your SysEx and AGS scripts!

---

## 5.3. The Survival Kit: Reset and Allocation

For your music to play in the game, **you must absolutely use two commands** at the very beginning of your MIDI file (at Tick 0).

### 1. Start Song (Reset)
At the very first tick, send this message to clean the engine's memory and reset all tracks.
```text
F0 7D 02 F7
```

### 2. Allocate Part (Open a channel)
In iMWrap, notes from a MIDI channel will only be heard if this channel has been explicitly "allocated" by a SysEx. This allows the engine to assign a priority and fine-tune settings for each instrument.
The code is `00`. 
Example to allocate channel 0 (in hexadecimal):
```text
F0 7D 00 00 00 01 05 0A 07 0F 00 00 00 00 00 00 00 02 00 00 F7
```
*(We will dissect the exact meaning of each pair of numbers in **Chapter 6**. For now, just know that you need to copy-paste and adapt this SysEx for each channel used, changing the 4th byte `00` to `01`, `02`, etc.).*

> [!CAUTION]
> If the Song contains several Tracks containing sequences that must be linked together, you must not put a Song Start message anywhere other than on the first track, at the risk of seeing the music abruptly cut off during the transition.

---

## 5.4. Adding Interactivity in the DAW

Here are the 3 most important tools for a composer.

### 1. The Marker (Trigger to AGS)
You want AGS to display a lightning bolt on the bass drum of measure 4.
Go to measure 4 in your sequencer, on your control track, and insert a SysEx of type `40` (Marker).
```text
F0 7D 40 00 01 F7
```
This sends ID `1` to the AGS script.

> [!CAUTION]
> Never write literal text in an iMWrap Marker (e.g., `F0 7D 40 00 "INTRO" F7`). The engine would treat each letter as a different marker (it would send 'I', then 'N', then 'T'...). **Only use a single-byte numeric identifier** (e.g., 01, 02, 0A, 0B...).

### 2. Loops (Set Loop)
If you want the music to loop infinitely without the help of the AGS developer, use the code `50`.
For example, to loop 3 times from measure 16 to measure 8:
```text
F0 7D 50 00 00 00 00 03 00 00 00 08 00 00 00 00 00 00 00 10 00 00 00 00 F7
```
*(See **Chapter 6** for the exact construction)*.

To cancel the current loop (Clear Loop), it's the code `51`:
```text
F0 7D 51 F7
```

### 3. Preparing Hooks
Remember Chapter 4: the AGS programmer wants to send an order, but must wait for the music to allow it. 
To give them the "green light", you (the composer) must place Hook SysEx (family `3X`) on the strong beats.

If you want to allow AGS to change the drum volume (channel 9) at the beginning of each measure, place this SysEx at the beginning of the measure:
```text
// 33 = Volume Hook, 09 = Channel, 01 = Hook ID (hookValue)
// (Followed by the rest of the parameters required by the volume SysEx)
F0 7D 33 09 01 00 F7
```
When the MIDI playback head passes over this event, if the programmer had previously called `iMWrap_SetHook(..., IMWRAP_HOOK_PART_VOLUME, 1, ...)`, the action finally executes!

---

## 5.5. The perfect piece template

Here is what the structure of your "Control" (0) track should look like:

* **Measure 1, Tick 0**: `Start Song` (`F0 7D 02 F7`)
* **Measure 1, Tick 1**: `Allocate Part` for the piano (Channel 0)
* **Measure 1, Tick 2**: `Allocate Part` for the bass (Channel 1)
* **Measure 1, Tick 3**: `Allocate Part` for the strings (Channel 2)
* **Measure 2, Beat 1**: *(Beginning of the music, notes on other tracks)*
* **Measure 8, Beat 1**: Placement of a `Marker 01` (to animate a scenery element).
* **Measure 12, Beat 4**: Placement of a `Set Loop` (to loop to measure 2).

In **Chapter 6**, we will do some hexadecimal arithmetic to understand how to manually forge the parameters of each SysEx message without making mistakes!
