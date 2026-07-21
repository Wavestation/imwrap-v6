# IWRFlexMusic - Integration Guide

AGS module for interactive music management using the **States and Sequences** pattern (iMUSE / LucasArts style), based on the **agsimwrap** plugin.

## Philosophy

The architecture is designed for games where **each room or mood has its own `Sound ID`** (its own MIDI sequence).

Inside this MIDI sequence:
* **Track 0**: The main loop of the room.
* **Track > 0**: The *outgoing* transition tracks leading to other rooms.

The jump from Track 0 to the transitions is handled by a **Hook Jump** integrated into the MIDI file by the composer.

## Quick Start

In `game_start()`:
```ags
// Load the global sound bank
iMWrap_LoadBank("music/game.ims");

// Start the music for the first room (e.g. Sequence 90)
IWRFlexMusic.Play(90);
```

In-game, to transition from the base room (Seq 90) to the Hologram room (Seq 91) with a synchronized transition:
```ags
// Parameters: (Current Seq, Target Seq, HookID of the transition in the Current Seq)
IWRFlexMusic.SetTransition(90, 91, 1);
```

## What happens under the hood?

1. `SetTransition(90, 91, 1)` tells iMWrap to clear the current loop and arm **Hook 1** on Sequence 90.
2. When the MIDI playhead encounters the corresponding Hook Jump, Sequence 90 jumps to the transition track.
3. At the end of this transition track, the composer has placed **Marker 64**.
4. The global callback `iMWrap_OnTrigger` intercepts Marker 64, automatically stops Sequence 90, and starts Sequence 91 (which begins on its Track 0).

## Chaining Transitions

Thanks to this system, you can chain transitions easily. If the player turns back from the Hologram room to the base room:

```ags
// Seq 91 is playing. We arm its Hook 1 to trigger its return transition track.
// At Marker 64, the system will start Seq 90.
IWRFlexMusic.SetTransition(91, 90, 1);
```
