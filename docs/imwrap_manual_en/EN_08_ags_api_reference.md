# Chapter 8: AGS API Reference Manual

This chapter lists **the complete set of functions, constants, and definitions** injected by the `agsimwrap` plugin into the Global Script of your AGS project. Each function is accompanied by a typical usage example.

---

> [!IMPORTANT]
> **Zero-indexed Values**
> Throughout the iMWrap AGS API, all IDs, parts, and channels are 0-indexed. 
> For example, if you want to affect **MIDI Channel 1** of your music, you must use the value `0` for the `channel` parameter.
>
> **Heads up: The only exception is measures (beats)!**
> To preserve historical compatibility with old iMUSE scripts and standard DAW logic, measures start at 1. So "Measure 1" in your DAW sequencer is indeed `beat` 1 in code! 🤘

## 8.1. Plugin Constants

These constants are available throughout your scripts to configure the audio driver.

```c
#define IMWRAP_PLUGIN_VERSION 101

#define IMWRAP_DRIVER_FLUIDSYNTH    0  // Modern software synthesis with SoundFont
#define IMWRAP_DRIVER_ADLIB         1  // FM emulation (SoundBlaster/OPL3 style) (Support not 100% verified in v1.0.4)
#define IMWRAP_DRIVER_HARDWARE_GM   2  // External hardware sound card (General MIDI)
#define IMWRAP_DRIVER_HARDWARE_MT32 3  // Hardware Roland MT-32 synthesizer
#define IMWRAP_DRIVER_MUNT          4  // MUNT emulator (MT-32)
```

---

## 8.2. Initialization and Devices

* `import int iMWrap_LoadBank(const string filename);`
  Loads the main `.ims` bank into memory. Usually used once in `game_start()`.
  ```c
  // Loads the game's musical sequence file
  iMWrap_LoadBank("$DATA$/music_data/ost.ims");
  ```

* `import void iMWrap_LoadSoundFont(const string filename);`
  Shortcut to load a SoundFont (`.sf2`) or compressed `.sf3` and configure the FluidSynth driver in one go.
  ```c
  iMWrap_LoadSoundFont("$DATA$/music_data/SGM-V2.01.sf2");
  ```


* `import void iMWrap_SetSFDynLoad(bool enable = false);`
  Enables (`true`) or disables (`false`) dynamic loading for `.sf3` SoundFonts. When enabled, samples are streamed into memory on demand rather than entirely decompressed at load time. Must be called BEFORE `iMWrap_LoadSoundFont`.
  ```c
  iMWrap_SetSFDynLoad(true); // Enable dynamic streaming for SF3
  iMWrap_LoadSoundFont("$DATA$/music_data/SGM-V2.01.sf3");
  ```

* `import int iMWrap_SetDriver(int driverType, const string deviceOrPath);`
  Allows manually setting the driver (`IMWRAP_DRIVER_...`). `deviceOrPath` provides the path to the `.sf2` (for FluidSynth), the pipe-separated ROMs paths (for Munt), or the port index (for Hardware MIDI). Leave `""` for AdLib.
  ```c
  // Configure for a Roland MT-32 connected to Windows MIDI port "1"
  iMWrap_SetDriver(IMWRAP_DRIVER_HARDWARE_MT32, "1");
  ```

* `import int iMWrap_GetMIDIDeviceCount();`
  Returns the number of physical MIDI ports connected to the computer.
  ```c
  int nbPorts = iMWrap_GetMIDIDeviceCount();
  Display("You have %d MIDI devices connected.", nbPorts);
  ```

* `import String iMWrap_GetMIDIDeviceName(int index);`
  Returns the textual name of the hardware port at the given index.
  ```c
  // Displays the name of the first MIDI port
  Display("Port 0: %s", iMWrap_GetMIDIDeviceName(0));
  ```

---

## 8.3. Playback Control (Play / Stop)

* `import int iMWrap_StartSound(int soundId);`
  Starts playing the sequence with ID `soundId`. Returns 1 on success, 0 otherwise.
  ```c
  // Starts the tavern music (ID 50)
  iMWrap_StartSound(50);
  ```

* `import void iMWrap_StopSound(int soundId);`
  Smoothly stops (sends Note Off signals) the sequence `soundId`.
  ```c
  // Stops the tavern music
  iMWrap_StopSound(50);
  ```

* `import void iMWrap_StopAllSounds();`
  Cuts all active sounds simultaneously. Very useful during a Game Over.
  ```c
  iMWrap_StopAllSounds();
  ```

* `import int iMWrap_IsSoundActive(int soundId);`
  Returns `1` if the sequence is currently playing, otherwise `0`.
  ```c
  if (iMWrap_IsSoundActive(50)) {
      cEgo.Say("The music is too loud here!");
  }
  ```

* `import int iMWrap_GetSoundStatus(int soundId);`
  Returns `0` (inactive), `1` (currently playing), or `2` (scheduled/waiting for Hook).
  ```c
  if (iMWrap_GetSoundStatus(50) == 2) {
      // The music is waiting for a trigger to start
  }
  ```

* `import int iMWrap_GetActiveSoundCount();`
  Returns the total number of music tracks playing simultaneously at this moment.
  ```c
  if (iMWrap_GetActiveSoundCount() > 3) {
      // Watch out for cacophony!
  }
  ```

* `import int iMWrap_GetActiveSoundId(int index);`
  Allows iterating over the currently playing sounds to retrieve their IDs.
  ```c
  int i = 0;
  while (i < iMWrap_GetActiveSoundCount()) {
      int id = iMWrap_GetActiveSoundId(i);
      iMWrap_SetSoundVolume(id, 50); // Lowers the volume of everything playing
      i++;
  }
  ```

---

## 8.4. Mixing, Effects, and Tempo

* `import void iMWrap_SetMasterVolume(int volume);`
  Master volume of the synthesizer (`0` to `127`).
  ```c
  // Called by the volume slider in the options interface
  iMWrap_SetMasterVolume(100); 
  ```

* `import void iMWrap_SetSoundVolume(int soundId, int volume);`
  Specific volume of a complete sequence (`0` to `127`).
  ```c
  // Sound 50 drops to half volume (64)
  iMWrap_SetSoundVolume(50, 64);
  ```

* `import void iMWrap_SetSoundPan(int soundId, int pan);`
  Stereo panning (`-128` to `127`). `0` is center.
  ```c
  // Music comes from the left speaker
  iMWrap_SetSoundPan(50, -128);
  ```

* `import void iMWrap_SetSoundSpeed(int soundId, int speed);`
  Playback speed. Normal is `128`.
  ```c
  // Slows down the music (dream effect)
  iMWrap_SetSoundSpeed(50, 80);
  ```

* `import void iMWrap_SetSoundTranspose(int soundId, int relative, int transpose);`
  Transposes the pitch (`transpose` is in semitones). If `relative` is `1`, it adds to the current transposition; otherwise (`0`), it overrides it.
  ```c
  // Shifts the music up one semitone from its original pitch (modulation)
  iMWrap_SetSoundTranspose(50, 0, 1);
  ```

* `import void iMWrap_SetSoundPriority(int soundId, int priority);`
  Forces the priority of the sequence in case of polyphony saturation.
  ```c
  // 127 is the maximum priority
  iMWrap_SetSoundPriority(50, 127);
  ```

* `import void iMWrap_SetPartVolume(int soundId, int channel, int volume);`
  Targeted volume for a specific track/channel (e.g., bass) (`0` to `127`).
  ```c
  // Lowers channel 2 of music 50 to 30
  iMWrap_SetPartVolume(50, 2, 30);
  ```

* `import void iMWrap_SetPartOnOff(int soundId, int channel, int onOff);`
  Mutes (`0`) or unmutes (`1`) a specific channel on the fly.
  ```c
  // Mutes the drum track (often channel 9)
  iMWrap_SetPartOnOff(50, 9, 0);
  ```

* `import void iMWrap_Fade(int soundId, int targetVolume, int timeInTicks);`
  Triggers a smooth volume fade to the target over the desired duration (in ticks).
  ```c
  // Fade to silence over about 2 measures (at 120 bpm = ~1000 ticks)
  iMWrap_Fade(50, 0, 1000);
  ```

* `import int iMWrap_GetTempo();`
  Retrieves the raw global tempo in *microseconds per quarter note*.
  ```c
  int msPerQuarter = iMWrap_GetTempo();
  int bpm = 60000000 / msPerQuarter;
  Display("Current tempo: %d BPM", bpm);
  ```

---

## 8.5. Jumps, Loops, and Hooks

* `import void iMWrap_Jump(int soundId, int track, int beat, int tick);`
  Instant (direct) jump to a specific measure/tick.
  ```c
  // Brutally jumps to measure 20
  iMWrap_Jump(50, 0, 20, 0);
  ```

* `import void iMWrap_Scan(int soundId, int track, int beat, int tick);`
  Silent fast-forward to the targeted point (without playing intermediate notes, but applying program and volume changes crossed).
  ```c
  iMWrap_Scan(50, 0, 10, 0); // Virtually starts at measure 10
  ```

* `import void iMWrap_SetLoop(int soundId, int count, int toBeat, int toTick, int fromBeat, int fromTick);`
  Forces a software time loop in AGS.
  ```c
  // Loops 5 times. Upon reaching measure 30, returns to 10.
  iMWrap_SetLoop(50, 5, 10, 0, 30, 0);
  ```

* `import void iMWrap_ClearLoop(int soundId);`
  Clears the current forced loop.
  ```c
  // Player exits combat, let the music continue
  iMWrap_ClearLoop(50);
  ```

* **Hook Wrappers**
  To arm an asynchronous action (Hook) that will be executed at the moment chosen by the composer (see Chapter 4), AGS provides you with several clear functions:
  - `import void iMWrap_SetJumpHook(int soundId, int hookId);`
  - `import void iMWrap_SetGlobalTransposeHook(int soundId, int hookId);`
  - `import void iMWrap_SetPartOnOffHook(int soundId, int hookId, int channel);`
  - `import void iMWrap_SetPartVolumeHook(int soundId, int hookId, int channel);`
  - `import void iMWrap_SetPartProgramHook(int soundId, int hookId, int channel);`
  - `import void iMWrap_SetPartTransposeHook(int soundId, int hookId, int channel);`
  
  *The `hookId` parameter is the expected ID of the Hook in the MIDI file (if `0`, the Hook triggers unconditionally as soon as the playback head crosses a Hook event of this category).*
  
  ```c
  // Arms the wait for the Volume Hook with ID 1 on channel 2.
  // The actual action (e.g. lowering the volume to 50) 
  // is ALREADY pre-programmed by the composer in the MIDI file!
  iMWrap_SetPartVolumeHook(50, 1, 2);
  ```

* `import void iMWrap_SetHook(int soundId, int hookClass, int hookValue, int hookChannel);`
  *Historical and low-level function.* It is recommended to use the wrappers above.

---


* `import int iMWrap_PopMarker();`
  Pops and returns the oldest triggered marker value (or Hook value) from the queue. Returns `-1` if the queue is empty.
  The returned value "packs" both the sound ID (on the upper bits) and the marker value (on the lower 8 bits).
  Here is how to unpack these values:
  ```c
  int packed = iMWrap_PopMarker();
  while (packed != -1)
  {
      int markerValue = packed & 0xFF;         // The lower 8 bits
      int soundId = (packed >> 8) & 0xFFFFFF;  // The remaining bits for the soundId
      
      Display("Marker %d triggered by sound %d!", markerValue, soundId);
      packed = iMWrap_PopMarker();
  }
  ```

* `import int iMWrap_GetLastMarker();`
  Returns the most recent triggered marker without emptying the queue. (The value is packed the same way as `PopMarker`, or returns `-1` if empty).

## 8.6. Queue System

The Queue system allows you to accumulate several orders that must execute **at the exact same microsecond**, synchronized on the timeline of a "master" sound. Very useful for complex, perfectly synchronized transitions. The first parameter `soundId` always designates the "master" music on which the queue is based.

* `import void iMWrap_QueueTrigger(int soundId, int markerId);`
* `import void iMWrap_QueueStartSound(int soundId, int targetSound);`
* `import void iMWrap_QueueStopSound(int soundId, int targetSound);`
* `import void iMWrap_QueueStopAllSounds(int soundId);`
* `import void iMWrap_QueueSetHook(int soundId, int targetSound, int hookType, int hookValue, int channel);`
* `import void iMWrap_QueueAddFader(int soundId, int targetSound, int targetVolume, int timeInTicks);`
* `import void iMWrap_QueueCommand(int soundId, int cmd, int a1=0, int a2=0, int a3=0, int a4=0, int a5=0, int a6=0, int a7=0);`
  These functions add instructions to the queue of the `soundId` sound, silently. Nothing will execute until you validate them.

* `import void iMWrap_CommitQueue(int soundId);`
  Validates all pending instructions for `soundId` and pushes them into the audio engine for simultaneous execution.
  ```c
  // Prepares a perfect transition
  iMWrap_ClearQueue();
  iMWrap_QueueTrigger(50, 64);
  iMWrap_QueueStartSound(50, 51);
  // Executes everything simultaneously tied to sound 50!
  iMWrap_CommitQueue(50);
  ```

* `import void iMWrap_ClearQueue();`
  Empties the queue (cancels all pending commands that have not yet been validated).

---

## 8.7. Playback Position Information

* `import int iMWrap_GetPlaybackTrack(int soundId);`
  Returns the logical system track currently playing (usually 0).
  ```c
  int track = iMWrap_GetPlaybackTrack(50);
  ```

* `import int iMWrap_GetPlaybackBeat(int soundId);`
  Returns the current measure number (Beat). Very useful for debugging your transitions.
  ```c
  if (iMWrap_GetPlaybackBeat(50) > 40) { Display("Approaching the end!"); }
  ```

* `import int iMWrap_GetPlaybackTick(int soundId);`
  Returns the tick number within the current measure (e.g., 0 to 479 for a 4/4 measure at 120 PPQN).
  ```c
  int tick = iMWrap_GetPlaybackTick(50);
  ```

---

## 8.8. Hardware Profiles and MT-32

These functions are used to simulate or force specific behaviors of old LucasArts or Roland engines.

* `import void iMWrap_SetNativeMt32(int enabled);`
  Enables (`1`) or disables (`0`) native MT-32 SysEx mapping.
  ```c
  iMWrap_SetNativeMt32(1); // The engine will send native Roland SysEx
  ```

* `import void iMWrap_SetCompatibilityProfile(int profile);`
  Selects the internal compatibility mode (0 = standard iMWrap, 1 = strict SCUMM v6 / SNM compatibility mode).
  ```c
  iMWrap_SetCompatibilityProfile(1);
  ```

* `import int iMWrap_GetCompatibilityProfile();`
  Returns the current profile (0 or 1).

* `import void iMWrap_RegisterRolandTimbreMapping(const string name, int gmProgram);`
  Manually maps a custom Roland MT-32 sound (which would have the name `name`) to a fallback General MIDI instrument (`gmProgram`), in case the player listens in GM.
  ```c
  // If MT-32 plays the "SynthBrass1" timbre, GM will play instrument 62
  iMWrap_RegisterRolandTimbreMapping("SynthBrass1", 62);
  ```

* `import void iMWrap_ClearRolandTimbreMappings();`
  Clears the manual conversion table.
  ```c
  iMWrap_ClearRolandTimbreMappings();
  ```

* `import void iMWrap_SetWelcomeMessage(const string message);`
  Displays a custom little message on the player's physical Roland MT-32 synthesizer's mini LCD screen!
  ```c
  iMWrap_SetWelcomeMessage("Welcome adventurer");
  ```

---

## 8.9. External Configuration and Logging

* `import int iMWrap_HasExternalConfig();`
  Checks if an `.imc` configuration file has been dropped by the player in the game folder.
  ```c
  if (iMWrap_HasExternalConfig()) { /* The player wants to customize audio */ }
  ```

* `import int iMWrap_ApplyExternalConfig(const string fallbackSoundFont);`
  Forces the application of the `.imc` file settings. The parameter serves as a default `.sf2` if the `.imc` requests FluidSynth without specifying a bank.
  ```c
  iMWrap_ApplyExternalConfig("$DATA$/music_data/arachno.sf2");
  ```

* `import void iMWrap_EnableLog(int enabled);`
  Enables (`1`) writing an `imwrap_debug.log` file to understand what is happening under the hood (SysEx errors, hook triggering...).
  ```c
  iMWrap_EnableLog(1);
  ```
