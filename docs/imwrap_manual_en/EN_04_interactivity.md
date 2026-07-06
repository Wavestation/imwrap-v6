# Chapter 4: Interactive Music (Hooks, Callbacks...)

This is where the iMWrap engine reveals its full potential. Up until now, we were "slaves" to the music, or we manipulated it brutally (lowering the volume at any time, muting a channel instantly).

With the system inherited from iMUSE, **the music becomes an actor**. It decides the *ideal moment* for your scripts to execute, in order to preserve the magic and fluidity.

However, you will see that this is not the most optimal way to handle the music's reactivity. Indeed, when you use these commands, and especially Jumps and Loops, directly from the scripts, the **programmer** has the control. Yet, it is often more appropriate to leave these decisions in the hands of the composer. This is what you will discover in **paragraph 4.4** and in **Chapter 5**.

---

## 4.1. The Time Jump

Imagine that the intro music of your game has a waiting sequence (vamping) that loops. When the player clicks "New Game", you don't want to start a different music track brutally. You want the intro music to jump to its glorious grand finale, but **in rhythm**.

This is what `iMWrap_Jump` allows. This function asks the sequencer to move the playback head to a specific point.

```c
// Jumps sound 80 to measure 15 (beat), tick 0.
// The "track" parameter is almost always 0.
iMWrap_Jump(80, 0, 15, 0);
```

> [!CAUTION]
> A `Jump` executes **immediately**. If you call it in the middle of a beat, the drums will miss a beat and the jump will be audible. To jump musically (for example, at the end of the current measure), you must use **Hooks** or **Loops**.

---

## 4.2. Dynamic Loops

Of course, you can loop your sequences directly from the MIDI sequencer (see Chapter 5). But it is sometimes very useful to force a loop **from AGS** depending on what the player does.

The `iMWrap_SetLoop` function tells the sequencer: *"When you reach this point (A), rewind to this point (B)"*.

```c
// iMWrap_SetLoop(soundId, count, toBeat, toTick, fromBeat, fromTick)
// "In music 80, loop infinitely (count=0). 
// When you reach Beat 20 (Tick 0), return to Beat 10 (Tick 0)."
iMWrap_SetLoop(80, 0, 10, 0, 20, 0);
```

**Why is this interactive?**
Imagine the player is fighting a boss. You put a loop between measure 10 and 20. The music stays frantic. When the boss dies, you call `iMWrap_ClearLoop(80);`. The music will reach measure 20, cross the loop, and enter measure 21 which contains the victory music!

---

## 4.3. Callbacks (Markers): When the music wakes up the game

Here is the most spectacular feature. You want a character to dance to the tempo of the music, or a lightning bolt to strike exactly at the moment of a cymbal crash.

The composer prepared their MIDI file by inserting **Markers**. A Marker is an invisible message (SysEx) placed on the score. For example, the composer placed Marker "1" at measure 5, and Marker "2" on every strong beat of the drums.

In AGS, it's very simple. The plugin will automatically call the `iMWrap_OnTrigger` function in your `GlobalScript.asc` every time the MIDI playback head crosses a Marker.

```c
// To be placed in your GlobalScript.asc
function iMWrap_OnTrigger(int soundId, int markerId) {
    if (soundId == 80) {
        if (markerId == 1) {
            // Marker 1 has been reached! Launch the lightning!
            Display("BOOM!");
            cEgo.Say("What a storm!");
        }
        else if (markerId == 2) {
            // A strong drum beat, we make the character dance for one frame
            cDancer.Animate(1, 0, eOnce, eNoBlock);
        }
    }
}
```

---

## 4.4. Hooks: Asynchronous Action

The Hook is the reverse concept of the Marker. With a Hook, **the game (AGS) gives an order to the music engine, but the order is paused**. The order will only execute when the musical score allows it.

Why do this? Let's take the volume example again. You want the volume of the violins to increase when you enter a room, but you want it done *on the first beat of the measure* so the dramatic effect is perfect, rather than randomly while the player is walking.

The composer writes empty "Hook SysEx" at strategic points in their score (for example, at the beginning of each measure).

In AGS, you "arm" the hook:
```c
// iMWrap_SetHook(soundId, hookClass, hookValue, hookChannel)

// We arm a Volume Change Hook (IMWRAP_HOOK_PART_VOLUME).
// hookValue = 1: we specifically wait for the Hook with ID "1" placed by the composer.
// (If hookValue was 0, any volume Hook would trigger the action unconditionally).
// Note: The new volume is NOT passed in this AGS code. 
// The actual action is ALREADY encoded in the MIDI SysEx by the composer!
iMWrap_SetHook(80, IMWRAP_HOOK_PART_VOLUME, 1, 0);
```

The music plays... it encounters the point prepared by the composer... BAM! The Hook triggers and the volume changes in rhythm.

**The Hook classes (`hookClass`), defined by constants in AGS:**
- `IMWRAP_HOOK_JUMP` (0): Jump Hook (Asynchronous rhythmic jump)
- `IMWRAP_HOOK_TRANSPOSE` (1): Global Transposition Hook
- `IMWRAP_HOOK_PART_ONOFF` (2): Part On/Off Hook (Mute/unmute an instrument)
- `IMWRAP_HOOK_PART_VOLUME` (3): Volume Hook (Modify volume of a channel)
- `IMWRAP_HOOK_PART_PROGRAM` (4): Program Hook (Change instrument)
- `IMWRAP_HOOK_PART_TRANSPOSE` (5): Track Transposition Hook

> [!TIP]
> If the music you are playing contains no "Hook" event coded by the composer, your calls to `iMWrap_SetHook` in AGS will **never** execute, because the engine will wait forever for authorization from the score. It's real teamwork!

### 4.5. Advanced Example: Combining Hook and Callback for a transition

Imagine the player solves a puzzle. We want:
1. The current music (ID 80) to start its "clean" ending sequence (via a Jump Hook).
2. Once the ending sequence is finished, the music signals the game (via a Marker) to start the new victory music (ID 81).

**Composer Side (Music 80):**
- At measure 10, the composer placed a **Jump Hook**. In this SysEx, they already encoded the destination (jump to measure 50)!
- At measure 50 (the true glorious end of the piece), they placed a **Marker ID 10**.

**Programmer Side (AGS):**
```c
// When the player solves the puzzle:
function solve_puzzle() {
    // We arm the Jump Hook (IMWRAP_HOOK_JUMP).
    // hookValue = 0 (unconditional). When the music reaches measure 10,
    // it will validate this Hook and jump to its finale.
    iMWrap_SetHook(80, IMWRAP_HOOK_JUMP, 0, 0);
}

// Later, when the music plays its ending and reaches measure 50...
function iMWrap_OnTrigger(int soundId, int markerId) {
    if (soundId == 80 && markerId == 10) {
        // Music 80 has cleanly finished its conclusion!
        iMWrap_StopSound(80);
        
        // We start the victory music!
        iMWrap_StartSound(81);
    }
}
```
This combo is the very essence of iMUSE-style interactive music: the game initiates the change, the music orchestrates it musically to stay in rhythm, then the music hands control back to the game for the next step!

In **Chapter 5**, we will step through the looking glass: how the composer prepares all these interactive elements in their sequencer!
