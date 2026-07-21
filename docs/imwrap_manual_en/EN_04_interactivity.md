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

In AGS, it's very simple. At each game loop (in your `repeatedly_execute`), you will ask the engine "Have you crossed a marker?". This is called *polling*. The engine stores the encountered markers in a Queue, and the `iMWrap_PopMarker` function allows you to pop them one by one. Since it must return two pieces of information (the sound ID and the marker ID), it returns a "packed" (combined) integer that you must split.

```c
// To be placed in your GlobalScript.asc
function repeatedly_execute()
{
    // Pop the oldest waiting marker
    int packed = iMWrap_PopMarker();
    while (packed > 0)
    {
        // Extract the soundId (high bits) and markerId (low 8 bits)
        int soundId = (packed >> 8) & 0xFFFFFF;
        int markerId = packed & 0xFF;

        if (soundId == 80)
        {
            if (markerId == 1)
            {
                // Marker 1 has been reached! Launch the lightning!
                Display("BOOM!");
                cEgo.Say("What a storm!");
            }
            else if (markerId == 2)
            {
                // A strong drum beat, we make the character dance for one frame
                cDancer.Animate(1, 0, eOnce, eNoBlock);
            }
        }
        
        // Move to the next marker in the queue
        packed = iMWrap_PopMarker();
    }
}
```

---

## 4.4. Hooks: Asynchronous Action

The Hook is the reverse concept of the Marker. With a Hook, **the game (AGS) gives an order to the music engine, but the order is paused**. The order will only execute when the musical score allows it.

Why do this? Let's take the volume example again. You want the volume of the violins to increase when you enter a room, but you want it done *on the first beat of the measure* so the dramatic effect is perfect, rather than randomly while the player is walking.

The composer writes empty "Hook SysEx" at strategic points in their score (for example, at the beginning of each measure).

In AGS, you "arm" the hook using *wrappers* (specific functions for each type of Hook):
```c
// iMWrap_SetPartVolumeHook(soundId, hookId, channel)

// We arm a Volume Change Hook on channel 0.
// hookId = 1: we specifically wait for the Hook with ID "1" placed by the composer.
// (If hookId was 0, any volume Hook would trigger the action unconditionally).
// Note: The new volume is NOT passed in this AGS code. 
// The actual action is ALREADY encoded in the MIDI SysEx by the composer!
iMWrap_SetPartVolumeHook(80, 1, 0);
```

The music plays... it encounters the point prepared by the composer... BAM! The Hook triggers and the volume changes in rhythm.

**The Hook wrappers available in AGS:**
- `iMWrap_SetJumpHook`: Jump Hook (Asynchronous rhythmic jump)
- `iMWrap_SetGlobalTransposeHook`: Global Transposition Hook
- `iMWrap_SetPartOnOffHook`: Part On/Off Hook (Mute/unmute an instrument)
- `iMWrap_SetPartVolumeHook`: Volume Hook (Modify volume of a channel)
- `iMWrap_SetPartProgramHook`: Program Hook (Change instrument)
- `iMWrap_SetPartTransposeHook`: Track Transposition Hook

> [!TIP]
> If the music you are playing contains no "Hook" event coded by the composer, your calls to `iMWrap_Set...Hook` functions in AGS will **never** execute, because the engine will wait forever for authorization from the score. It's real teamwork!

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
function solve_puzzle()
{
    // We arm the Jump Hook.
    // hookId = 0 (unconditional). When the music reaches measure 10,
    // it will validate this Hook and jump to its finale.
    iMWrap_SetJumpHook(80, 0);
}

// Later, in your repeatedly_execute, we poll the markers:
function repeatedly_execute()
{
    int packed = iMWrap_PopMarker();
    while (packed > 0)
    {
        int soundId = (packed >> 8) & 0xFFFFFF;
        int markerId = packed & 0xFF;

        if (soundId == 80 && markerId == 10)
        {
            // Music 80 has cleanly finished its conclusion!
            iMWrap_StopSound(80);
            
            // We start the victory music!
            iMWrap_StartSound(81);
        }
        
        packed = iMWrap_PopMarker();
    }
}
```
This combo is the very essence of iMUSE-style interactive music: the game initiates the change, the music orchestrates it musically to stay in rhythm, then the music hands control back to the game for the next step!

### 4.6. Advanced Example: Perfect Transition with Queues

In complex adventure games, you will often need to execute a batch of musical actions perfectly synchronized with a musical event (like a Hook). This is where the Queue system shines. The following example shows how to manage the musical transition between two rooms:

```c
// ==========================================
// When the player enters the room:
// ==========================================
function room_Load()
{
    // 1. We clear the global queue in case old commands are lingering
    iMWrap_ClearQueue();
    
    // 2. We prepare the sequence of instructions for the master music (MUSIC_BASEROOM):
    // - We raise marker 64 to signal the state change to the game
    iMWrap_QueueTrigger(MUSIC_BASEROOM, 64);
    // - We request the start of the new music (MUSIC_HOLODECK)
    iMWrap_QueueStartSound(MUSIC_BASEROOM, MUSIC_HOLODECK);
    
    // 3. We validate this queue on the master sound
    iMWrap_CommitQueue(MUSIC_BASEROOM);
    
    // 4. We arm the Jump Hook. 
    // When the base music (MUSIC_BASEROOM) reaches its transition point 
    // (planned by the composer with JH_BASE_TO_HOLO), it will instantly execute
    // all the commands we just committed to its Queue!
    iMWrap_SetJumpHook(MUSIC_BASEROOM, JH_BASE_TO_HOLO);
    
    // 5. Fallback case (anti-silence):
    // If the base music was stopped (for example after loading a save),
    // we start the new music manually to avoid silence.
    if (iMWrap_GetActiveSoundCount() == 0)
    {
        iMWrap_StartSound(MUSIC_HOLODECK);
    }
}

// ==========================================
// When the player leaves the room (Return):
// ==========================================
function room_Leave()
{
    // We repeat exactly the same logic, but in reverse!
    // This time, MUSIC_HOLODECK is the master music of the transition.
    
    iMWrap_ClearQueue(); 
    iMWrap_QueueTrigger(MUSIC_HOLODECK, 64);
    iMWrap_QueueStartSound(MUSIC_HOLODECK, MUSIC_BASEROOM);
    iMWrap_CommitQueue(MUSIC_HOLODECK);
    
    iMWrap_SetJumpHook(MUSIC_HOLODECK, JH_HOLO_TO_BASE);
}
```

This example is very telling: it shows the interaction between the "Hook" which waits for authorization from the score (the right rhythmic moment), and the "Queue" which executes a batch of precise actions at the exact moment of this Hook!

In **Chapter 5**, we will step through the looking glass: how the composer prepares all these interactive elements in their sequencer!
