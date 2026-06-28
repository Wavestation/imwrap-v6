# MusicManager — Integration Guide

AGS module for interactive music management using the **States and Sequences** pattern (iMUSE / LucasArts style),  
based on the **agsimwrap** plugin.

---

## Files

| File | Role |
|---|---|
| `MusicManager.ash` | Header — public declarations, enums, structs |
| `MusicManager.asc` | Script — full logic, `iMWrap_OnTrigger` callback |

---

## Philosophy

The module **does not manage loops from AGS code**.  
Music loops are defined inside the `.ims` file by the composer using the `0x50` SysEx:

```
F0 7D 50 00 [COUNT16] [TO_BEAT16] [TO_TICK16] [FROM_BEAT16] [FROM_TICK16] F7
```

The script only:
- **Clears** the previous loop → `iMWrap_ClearLoop()`
- **Jumps** to the entry point of the target state → `iMWrap_Jump()`

The iMWrap engine reads the `0x50` SysEx and arms the loop automatically.

---

## Installation

1. Copy `MusicManager.ash` and `MusicManager.asc` into your AGS project folder.
2. In the editor: right-click **Scripts** → *Add existing script* → select both files.

---

## Setup in `game_start()`

```c
function game_start()
{
    // 1. Start the audio engine
    iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "music/arachno.sf2");
    iMWrap_LoadBank("music/game.ims");
    iMWrap_StartSound(80);

    // 2. Initialize MusicManager
    //    soundId            = ID of the sound in the .ims bank
    //    transitionMarkerId = byte of the 0x40 SysEx placed at the end of each bridge
    //                         F0 7D 40 00 [transitionMarkerId] F7
    MusicManager.Init(80, 99);

    // 3. Register states
    //    RegisterState(stateId, trackId, entryBeat, entryTick)
    //    trackId   = MIDI track of the section (0 for single-track layouts)
    //    entryBeat = first measure of the section in the MIDI
    MusicManager.RegisterState(STATE_MENU,        0,  1,  0);
    MusicManager.RegisterState(STATE_EXPLORATION, 0,  9,  0);
    MusicManager.RegisterState(STATE_TENSION,     0, 25,  0);
    MusicManager.RegisterState(STATE_COMBAT,      0, 37,  0);
    MusicManager.RegisterState(STATE_VICTORY,     0, 53,  0);
    MusicManager.RegisterState(STATE_SAD,         0, 57,  0);

    // 4. Register transition sequences
    //    RegisterSequence(seqId, trackId, startBeat, startTick, hookValue)
    //    trackId   = MIDI track of the bridge
    //    hookValue = CMD value of the 0x30 Hook Jump SysEx (synchronized jump)
    //                pass 0 for an immediate direct jump without a Hook
    MusicManager.RegisterSequence(SEQ_MENU_TO_EXPLORATION,    0, 65, 0, 1);
    MusicManager.RegisterSequence(SEQ_EXPLORATION_TO_TENSION, 0, 69, 0, 2);
    MusicManager.RegisterSequence(SEQ_TENSION_TO_COMBAT,      0, 73, 0, 3);
    MusicManager.RegisterSequence(SEQ_COMBAT_TO_VICTORY,      0, 77, 0, 4);
    MusicManager.RegisterSequence(SEQ_COMBAT_TO_EXPLORATION,  0, 81, 0, 5);
    MusicManager.RegisterSequence(SEQ_ANY_TO_SAD,             0, 85, 0, 6);

    // 5. Start on the initial state
    MusicManager.SetState(STATE_MENU);
}
```

---

## Public API

### `MusicManager.Init(int soundId, int transitionEndMarkerId)`
Initializes the module. Call once in `game_start()`, after `iMWrap_StartSound()`.

### `MusicManager.RegisterState(int stateId, int trackId, int entryBeat, int entryTick)`
Registers a music state. `entryBeat` is the measure where `iMWrap_Jump()` will seek the playhead, on `trackId`.  
Pass `trackId = 0` if all your states share the same main track.  
The loop is handled by the `0x50` SysEx in the `.ims` — the module has no knowledge of it.

### `MusicManager.RegisterSequence(int seqId, int trackId, int startBeat, int startTick, int hookValue)`
Registers a transition sequence.

| `hookValue` | Mode | Behaviour |
|---|---|---|
| `> 0` | **Synchronized Hook** | `iMWrap_SetHook()` is armed; the `0x30` SysEx in the MIDI triggers the jump (incl. target track). `trackId` is unused by the script. |
| `0` | **Direct jump** | `iMWrap_Jump()` to `trackId:startBeat` immediately, no Hook SysEx required in the MIDI. |

### `MusicManager.SetState(int stateId)`
Switches immediately to a registered state (direct jump, no wait for bar end).

### `MusicManager.GetCurrentState()`
Returns the ID of the currently active state, or `STATE_NONE` if none.

### `MusicManager.Transition(int seqId, int targetStateId)`
Triggers a synchronized transition to a new state.  
Arms the iMUSE Hook (`iMWrap_SetHook`) and waits for the engine to execute the Hook Jump SysEx in the MIDI to jump to the bridge. When the bridge ends (Marker `0x40`), `SetState(targetStateId)` is called automatically.

### `MusicManager.IsTransitionPending()`
Returns `true` if a transition is in progress (useful to prevent stacking two at once).

### `MusicManager.Stop()`
Silences the music and resets the module to its initial state.

---

## In-Game Usage

### Switch state directly
```c
// When entering a room:
function room_AfterFadeIn()
{
    MusicManager.SetState(STATE_TENSION);
}
```

### Trigger a musical transition
```c
// When combat starts:
function btnAttack_Click(GUIControl *control, MouseButton button)
{
    if (!MusicManager.IsTransitionPending()) {
        MusicManager.Transition(SEQ_TENSION_TO_COMBAT, STATE_COMBAT);
    }
}
```

### Branch behaviour based on the current state
```c
if (MusicManager.GetCurrentState() == STATE_COMBAT) {
    // Speed up AI, change lighting, etc.
}
```

### Cut the music
```c
// Before a cutscene, a fade to black, etc.
MusicManager.Stop();
```

---

## Adding Gameplay Synchronization

`iMWrap_OnTrigger` is already declared in `MusicManager.asc`.  
Add your own markers in the **free zone** of that function:

```c
function iMWrap_OnTrigger(int soundId, int markerId)
{
    // MusicManager block (do not modify)
    if (soundId == _soundId && markerId == _transitionMarkerId) { ... return; }

    // Your gameplay synchronizations:
    if (soundId == 80 && markerId == 10) {
        cMonster.Animate(2, 3, eOnce, eNoBlock);
    }
}
```

---

## Responsibilities

| Who | What |
|---|---|
| **MusicManager.ash / .asc** | Generic logic — do not modify to add project data |
| **Composer** (`.ims` file) | `0x50` SysEx (loops), `0x30` (Hook Jumps), `0x40` (end-of-bridge markers) |
| **`game_start()`** | `Init()` + `RegisterState()` + `RegisterSequence()` |
| **Room / GUI scripts** | `SetState()` or `Transition()` as needed |

---

## Extending the Project

1. Add a state → one line in the `MusicStateId` enum (in `MusicManager.ash`) + one `RegisterState()` call in `game_start()`.
2. Add a sequence → one line in `MusicSequenceId` + one `RegisterSequence()` call.
3. If you need more than 32 states or sequences, increase `MUSIC_MAX_STATES` / `MUSIC_MAX_SEQUENCES` in `MusicManager.ash`.

---

## Multi-Track MIDI Architectures

### Architecture A — Single track (classic)
All content on track `0`. The simplest setup.

```c
// All trackIds set to 0
MusicManager.RegisterState(STATE_COMBAT, 0, 37, 0);
MusicManager.RegisterSequence(SEQ_TENSION_TO_COMBAT, 0, 73, 0, 3); // Hook
```

```
Track 0 : [STATE_MENU loop] [STATE_EXPL loop] ... [bridge 1] [bridge 2] ...
```

---

### Architecture B — States and bridges on separate tracks
States loop on track `0`, transition bridges live on track `1`.

```c
// States on track 0
MusicManager.RegisterState(STATE_MENU,   0, 1,  0);
MusicManager.RegisterState(STATE_COMBAT, 0, 37, 0);

// Bridges on track 1, Hook mode (the 0x30 SysEx encodes the target track)
MusicManager.RegisterSequence(SEQ_MENU_TO_EXPLORATION, 1, 65, 0, 1);
```

> **Note:** in Hook mode the script does not need to know the bridge's track —
> the `0x30` SysEx in the MIDI encodes it: `F0 7D 30 00 01 [TRACK=00 01] [BEAT] [TICK] F7`.
> `trackId = 1` is passed for documentary consistency but is not used by the script.

```
Track 0 : [STATE_MENU loop] ──Hook 0x30──►  Track 1 : [bridge 1] [bridge 2] ...
                                                        ↓ Marker 99
                ◄──── SetState() iMWrap_Jump(track=0) ──┘
```

---

### Architecture C — Each state on its own track
Advanced variant: each state has a dedicated track, bridges too.

```c
MusicManager.RegisterState(STATE_MENU,    0, 1, 0);  // track 0
MusicManager.RegisterState(STATE_COMBAT,  2, 1, 0);  // track 2
MusicManager.RegisterState(STATE_VICTORY, 3, 1, 0);  // track 3

// Bridge on track 4, direct jump (hookValue=0) — no Hook SysEx needed in MIDI
MusicManager.RegisterSequence(SEQ_COMBAT_TO_VICTORY, 4, 1, 0, 0);
```

> **Direct jump mode (`hookValue = 0`):** `iMWrap_Jump()` seeks immediately
> to `trackId:startBeat` without waiting for a Hook SysEx in the MIDI.
> Suitable when your bridges have no Hook Jump SysEx, or when exact bar-level
> synchronization is not required.

---

## What the Composer Places in the `.ims`

```
Bar  1 : F0 7D 50 ...          ← STATE_MENU loop   (SysEx 0x50)
Bar  9 : F0 7D 50 ...          ← STATE_EXPLORATION loop
...
Bar 65 : F0 7D 30 00 01 ...    ← Hook Jump CMD=1 to bridge (SEQ_MENU_TO_EXPLORATION)
         ... bridge notes ...
Bar 68 : F0 7D 40 00 63 F7     ← Marker ID=99 → triggers SetState automatically
```

> **Rule:** transition bridges must **not** contain a `0x50` SysEx (no loop).  
> Each `hookValue` must be **unique** to prevent a Hook Jump from firing the wrong sequence.
