# AGS Plugin `agsimwrap`

This plugin integrates the iMWrap v6 engine, compatible with the SCUMM v6 / iMUSE legacy, into Adventure Game Studio (AGS). It handles MIDI transport, tempo changes, loops, and interactive jumps (*hooks*) autonomously in a multithreaded runtime.

## Internal Behavior

The plugin embeds:
1. **FluidSynth** as a software synthesizer for General MIDI and Roland MT-32 content.
2. **miniaudio** to open the machine audio device in the background. The miniaudio render callback is responsible for:
   - advancing musical time (`advanceAll` with realtime seconds -> ticks conversion);
   - generating audio samples continuously into the output buffer.

## AGS Script API

At startup, the plugin injects the following script header into the AGS editor, making these control and driver-selection functions available everywhere:

```c
#define IMWRAP_PLUGIN_VERSION 101

#define IMWRAP_DRIVER_FLUIDSYNTH    0
#define IMWRAP_DRIVER_ADLIB         1
#define IMWRAP_DRIVER_HARDWARE_GM   2
#define IMWRAP_DRIVER_HARDWARE_MT32 3

// Load an iMWrap instrument/sequence bank in .ims format
import void iMWrap_LoadBank(const string filename);

// Load a .sf2 SoundFont (shortcut for iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, filename))
import void iMWrap_LoadSoundFont(const string filename);

// Configure the audio driver and associated MIDI device
// - driverType: desired driver (IMWRAP_DRIVER_FLUIDSYNTH, IMWRAP_DRIVER_ADLIB, etc.)
// - deviceOrPath:
//   - For FluidSynth: path to the .sf2 file (example: "music/arachno.sf2")
//   - For AdLib: unused (pass "")
//   - For hardware MIDI OUT: device index as a string (example: "0", "1")
import void iMWrap_SetDriver(int driverType, const string deviceOrPath);

// Return the number of hardware MIDI OUT ports available on the machine
import int  iMWrap_GetMIDIDeviceCount();

// Return the name of a MIDI OUT device by index
import const string iMWrap_GetMIDIDeviceName(int index);

// Start playback for a sound by sound ID
import void iMWrap_StartSound(int soundId);

// Stop playback for a sound by sound ID
import void iMWrap_StopSound(int soundId);

// Stop every currently playing sound
import void iMWrap_StopAllSounds();

// Return 1 if the specified sound is active/currently playing, otherwise 0
import int  iMWrap_IsSoundActive(int soundId);

// Configure an interactive iMWrap hook/jump
// - soundId: target sound ID
// - hookClass: hook class
// - hookValue: hook-associated value
// - hookChannel: target MIDI channel
import void iMWrap_SetHook(int soundId, int hookClass, int hookValue, int hookChannel);

// --- GLOBAL PLAYER CONTROL ---

// Change library master volume (0 to 127)
import void iMWrap_SetMasterVolume(int volume);

// Change whole-sound volume (0 to 127)
import void iMWrap_SetSoundVolume(int soundId, int volume);

// Change whole-sound pan (-128 to 127)
import void iMWrap_SetSoundPan(int soundId, int pan);

// Transpose the whole sound (relative: 1 or 0, transpose: semitones)
import void iMWrap_SetSoundTranspose(int soundId, int relative, int transpose);

// Change global sound playback speed (0 to 255)
import void iMWrap_SetSoundSpeed(int soundId, int speed);

// Set the global sound priority
import void iMWrap_SetSoundPriority(int soundId, int priority);

// --- TRACK / CHANNEL CONTROL ---

// Change the volume of a specific channel (0 to 127)
import void iMWrap_SetPartVolume(int soundId, int channel, int volume);

// Enable or disable a specific channel (1 or 0)
import void iMWrap_SetPartOnOff(int soundId, int channel, int onOff);

// --- FLOW CONTROL (JUMPS & LOOPS) ---

// Perform a direct interactive jump to a given beat/tick position
import void iMWrap_Jump(int soundId, int track, int beat, int tick);

// Scan the MIDI file forward to a given beat/tick without playing notes
import void iMWrap_Scan(int soundId, int track, int beat, int tick);

// Define a dynamic time loop
import void iMWrap_SetLoop(int soundId, int count, int toBeat, int toTick, int fromBeat, int fromTick);

// Clear the active loop
import void iMWrap_ClearLoop(int soundId);

// Perform a smooth fade toward a target volume over a duration in ticks
import void iMWrap_Fade(int soundId, int targetVolume, int timeInTicks);

// --- MISC CONFIGURATION ---

// Enable (1) or disable (0) native Roland MT-32 mode
import void iMWrap_SetNativeMt32(int enabled);

// Return the current playback track for a sound
import int  iMWrap_GetPlaybackTrack(int soundId);

// Return the current playback beat for a sound
import int  iMWrap_GetPlaybackBeat(int soundId);

// Return the current playback tick within the beat
import int  iMWrap_GetPlaybackTick(int soundId);

// --- ADDITIONAL STATUS QUERIES ---

// Return sound status (0 = inactive, 1 = active/playing, 2 = queued/scheduled)
import int  iMWrap_GetSoundStatus(int soundId);

// Return the number of currently active sounds
import int  iMWrap_GetActiveSoundCount();

// Return the active sound ID at the given index
import int  iMWrap_GetActiveSoundId(int index);

// Return the current tempo in microseconds per quarter note (example: 500000 for 120 BPM)
import int  iMWrap_GetTempo();

// --- COMPATIBILITY & ROLAND MT-32 MAPPING ---

// Set the engine compatibility profile (0 = Generic standard v6, 1 = SNM)
import void iMWrap_SetCompatibilityProfile(int profile);

// Return the current compatibility profile (0 = Generic standard v6, 1 = SNM)
import int  iMWrap_GetCompatibilityProfile();

// Map a Roland MT-32 timbre name to a General MIDI program
import void iMWrap_RegisterRolandTimbreMapping(const string name, int gmProgram);

// Clear all registered Roland MT-32 timbre mappings
import void iMWrap_ClearRolandTimbreMappings();

// Set the custom welcome message sent to the MT-32 emulator/synth display (20 chars max)
import void iMWrap_SetWelcomeMessage(const string message);

// --- EXTERNAL CONFIGURATION ---

// Return 1 if a valid external configuration file (<game_name>.imc) exists, otherwise 0
import int  iMWrap_HasExternalConfig();

// Apply the configuration from the external file (<game_name>.imc).
// If FluidSynth is configured, the plugin uses fallbackSoundFont.
// Returns 1 on success, 0 on failure or when no config is present.
import int  iMWrap_ApplyExternalConfig(const string fallbackSoundFont);

// Enable (1) or disable (0) logging to imwrap_debug.log and the console (disabled by default)
import void iMWrap_EnableLog(int enabled);
```

## AGS Integration

1. **Place the plugin**:
   - Copy the plugin DLL (`agsimwrap.dll` on Windows, or `libagsimwrap.so` on Linux) into your AGS project folder.
   - Enable the plugin in the AGS editor (right-click "Plugins" in the project tree, then select "iMWrap v6 AGS Plugin").

2. **Write the driver setup script**:
   ```c
   // In game_start():
   
   // Option 1: FluidSynth software synth (GM)
   iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "music/arachno.sf2");
   
   // Option 2: AdLib software emulator (standalone FM OPL3)
   // iMWrap_SetDriver(IMWRAP_DRIVER_ADLIB, "");
   
   // Option 3: Physical MT-32 synth connected to port 0
   // iMWrap_SetDriver(IMWRAP_DRIVER_HARDWARE_MT32, "0");

   iMWrap_LoadBank("music/openquest-lite.ims");

   // Start music
   iMWrap_StartSound(80);

   // Optional: configure an infinite loop from bar 10 to bar 20
   iMWrap_SetLoop(80, 0, 10, 0, 20, 0);
   ```

3. **Receive musical synchronization triggers**:
   The C++ plugin monitors playback and intercepts MIDI markers. When a marker is reached, it calls a function in your AGS Global Script.
   
   ```c
   // In your Global Script:
   function iMWrap_OnTrigger(int soundId, int markerId) {
       if (soundId == 80 && markerId == 12) {
           // Synchronize an animation with the music
           Display("Marker 12 triggered!");
       }
   }
   ```

## Resource Handling Inside the AGS Package (`.vox`)

To ship your game with music banks (`.ims`) and SoundFonts (`.sf2`), you can use the following approaches in AGS:

### 1. Loose Files
The simplest method is to place your files in the compiled game folder (for example `Compiled/Windows/music/`).
- **Script loading:**
  ```c
  iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "music/default.sf2");
  iMWrap_LoadBank("music/game.ims");
  ```
- **Advantages:** easy to update and no impact on game compilation time.
- **Drawbacks:** music files are directly visible to players in the install folder.

### 2. "Package custom data folder(s)"
Starting with AGS 3.6.0+, you can package arbitrary resource folders directly into the compiled game package (`.vox` or the executable):

1. Create a folder (for example `music_data`) at the root of your AGS project and place your `.ims` and `.sf2` files there.
2. In the AGS editor, open **General Settings**, then find the **Compiler** section.
3. In **Package custom data folder(s)**, enter your folder name (for example `music_data`).
4. In your scripts, reference those resources with the `$DATA$` prefix:
   ```c
   iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "$DATA$/music_data/default.sf2");
   iMWrap_LoadBank("$DATA$/music_data/game.ims");
   ```

### 3. Technical Behavior Under the Hood
- **`.ims` files:**
  The plugin calls `IAGSEngine::OpenFileStream` provided by AGS (available from interface version 28 / AGS 3.6.0+). That API searches and reads the file directly from the AGS virtual archive (`.vox` or executable). The plugin loads the full byte stream into memory and passes it to the iMWrap parser.
- **`.sf2` SoundFonts:**
  FluidSynth requires a plain filesystem path and does not consume the AGS stream API directly. The plugin first calls `IAGSEngine::ResolveFilePath` (interface 27+) to resolve special paths such as `$DATA$/...`. If that path does not map to a real file but AGS can still open the resource through `OpenFileStream` (interface 28+), the plugin automatically extracts the SoundFont to a temporary file before giving it to FluidSynth.
  *Note: this fallback allows loading SoundFonts packaged in AGS game data, not only loose files next to the executable.*

## External MIDI Configuration (`.imc` and `SetMIDI`)

The plugin lets players choose their own MIDI driver (FluidSynth, AdLib, hardware General MIDI, hardware Roland MT-32) through an external configuration file.

### 1. Configuration File Format (`.imc`)
The configuration file uses the standard INI format. It must use the game executable name with a `.imc` extension (for example `MyGame.imc`) and be placed next to the game executable.

Example for a hardware device:
```ini
[MIDI]
Driver=2
Device=loopMIDI Port
```

- **Driver**: driver type to use.
  - `0`: FluidSynth (software synthesizer, requires a SoundFont)
  - `1`: AdLib (FM OPL3 emulation)
  - `2`: Hardware General MIDI
  - `3`: Hardware Roland MT-32
- **Device**: MIDI output port name (for example `loopMIDI Port`) or device index like `0`. Ignored for FluidSynth and AdLib.

### 2. Integration Into the AGS Game Script
You can detect whether a player-provided configuration exists and apply it. If it does not, you can define a default driver (for example FluidSynth with your packaged SoundFont).

```c
// In game_start():

if (iMWrap_HasExternalConfig()) {
    // Try to apply the player's choice.
    // If the player chose FluidSynth, the plugin loads the SoundFont passed here.
    if (!iMWrap_ApplyExternalConfig("music/arachno.sf2")) {
        // Fallback if the player's device could not be opened
    }
} else {
    // The player did not configure a driver.
    iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "music/arachno.sf2");
}

iMWrap_LoadBank("music/game.ims");
iMWrap_StartSound(80);
```

If the player provides no configuration and the developer does not configure any driver through `iMWrap_SetDriver`, the plugin stays silent by default without crashing.

### 3. `SetMIDI` Utility
`SetMIDI.exe` (shipped with the plugin) is a small graphical configuration utility written in native Win32.
- **Placement:** it must be copied next to the game executable.
- **Automatic detection:** on startup it searches for the game executable in its own folder and automatically edits the matching `.imc` file.
- **Localization:** the interface automatically adapts to French, Spanish, or English, depending on the system language.
- **Port selection:** when the user selects a hardware driver (GM or MT-32), the list of available MIDI OUT ports becomes active so the target device can be selected.
