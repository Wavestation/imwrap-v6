# Chapter 3: AGS Scripting (Direct Control)

Now that you know how to start music (`iMWrap_StartSound`), we will learn how to manipulate it in real-time while it is playing. This is called **direct control**.

In iMWrap, all modifications you make to the music happen without stuttering. You can adjust the volume, panning (left/right), speed, or even mute specific instruments mid-flight.

---

## 3.1. Volume and Panning Control

The engine offers three levels for managing volume, from the macro scale (the whole game) to the micro scale (a specific instrument).

### Master Volume
It defines the overall sound level of the entire game. Useful for the options screen where the player sets their sound preferences. The value ranges from `0` (silence) to `127` (maximum).
```c
iMWrap_SetMasterVolume(100); 
```

### Sound Volume
It modifies the volume of the currently playing musical sequence, in a relative manner.
```c
// Lowers music 80 to make room for dialogs
iMWrap_SetSoundVolume(80, 40); 
```

### Panning (Pan)
You can move the origin of the sound in the stereo space (very useful if a radio is playing music in the background on the left side of the screen).
The value ranges from `-128` (completely to the left) to `127` (completely to the right). `0` is center.
```c
// Places the music completely on the right speaker
iMWrap_SetSoundPan(80, 127);
```

---

## 3.2. Fades

Changing the volume abruptly sometimes lacks elegance. iMWrap features an extremely smooth automated fading system.

> [!IMPORTANT]
> **The concept of a Tick**
> In iMWrap, time is not measured in seconds, but in "Ticks". It is the rhythmic unit of the score. Often, the resolution of a MIDI file is **120 ticks per quarter note**. At an average tempo of 120 BPM, a tick lasts about `4 milliseconds`. A fade over `120` ticks therefore lasts the duration of a heartbeat (one quarter note). A fade over `480` ticks lasts a whole measure (4 beats).

To smoothly lower the forest music (ID 80) to silence (volume 0) over a duration of about 2 measures (e.g., `1000` ticks):
```c
iMWrap_Fade(80, 0, 1000);
```
*(You can obviously use it for a Fade In by starting the music at volume 0, then calling `iMWrap_Fade(80, 127, 1000)`.)*

---

## 3.3. Controlling Instruments Separately (Parts)

Each music track is composed of tracks (called "MIDI Channels" or "**Parts**"). Usually, channel 0 is the melody, channel 1 the bass, channel 9 the drums. iMWrap allows you to control these channels individually!

Imagine a stealth scene. You are playing music 80. When the player is hidden, only the bass and drums are playing. If they are spotted by a guard, you can instantly activate the brass track (on channel 3) to add tension!

**Mute / Unmute a channel:**
```c
// Mutes channel 3 (0 = inactive) of music 80
iMWrap_SetPartOnOff(80, 3, 0);

// Reactivates channel 3 (1 = active)
iMWrap_SetPartOnOff(80, 3, 1);
```

**Modify the volume of a single channel:**
```c
// Discreetly lowers the bass (channel 1) to 30/127
iMWrap_SetPartVolume(80, 1, 30);
```

---

## 3.4. Speed and Transposition

### Transposition (Pitch)
You can transpose an entire piece to make it higher or lower. Very useful if the player shrinks or falls into a dream! Transposition is done by semitones (like on a piano keyboard).
```c
// Parameter 2: 0 (absolute) or 1 (relative)
// Parameter 3: Number of semitones (-12 to 12, etc.)
// Lowers the music by an octave (-12 semitones)
iMWrap_SetSoundTranspose(80, 0, -12); 
```

### Speed
The normal speed (tempo) of the piece corresponds to the base value `128`. Lower values slow down, higher values speed up playback. Speeding up a sequence does not affect its pitch (unlike speeding up a vinyl record).
```c
// Drastically speeds up music 80 (e.g., a chase scene)
iMWrap_SetSoundSpeed(80, 180);

// Returns to normal speed
iMWrap_SetSoundSpeed(80, 128);
```

---

## 3.5. Knowing Where You Are

If your script needs to know precisely where the playback head is on the score (for example, to trigger an animation exactly at the beginning of measure 10), you can query the "playback head".

The musical position is defined by three concepts (Track, Beat, Tick):
* **Track**: The global "sequential" track of the file (often 0).
* **Beat**: The current measure.
* **Tick**: The temporal subdivision within the measure.

```c
int currentMeasure = iMWrap_GetPlaybackBeat(80);

if (currentMeasure >= 10) {
    // Playback has passed measure 10
}
```

However, constantly polling the `iMWrap_GetPlaybackBeat` function in the AGS `repeatedly_execute` function is not the most elegant or precise method.

In **Chapter 4**, we will see how to use **Hooks** and **Markers**, which allow the music to notify you asynchronously and with millisecond precision!
