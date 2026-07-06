# Chapter 2: AGS Plugin - Installation and Quickstart

In this chapter, we will set up the iMWrap engine within your AGS project. We will install it, configure the audio driver, and play your first musical note.

Even if you have never used plugins in AGS, follow this step-by-step guide.

---

## 2.1. Prerequisites and Plugin Installation

The integration of iMWrap into AGS relies on a simple DLL (Dynamic Link Library) plugin on Windows. More specifically, there are two distinct plugins: one for the runtime (the iMWrap engine itself) and one for the editor, which facilitates the handling of iMWrap's specific resource files.

### Step 1: Obtain the files
You should have the following elements:
- The runtime DLL: `agsimwrap-x32.dll` (the core engine for Windows)
- The editor plugin: `AGS.Plugin.IMWrap.Editor.dll` and `imwrap_shim.dll` (required for the AGS editor interface)
- A music bank: an `.ims` file containing your music. One is provided in the "examples" folder within the release archive.
- (Optional but recommended) A SoundFont: an `.sf2` file containing the instruments if you use the General MIDI profile. If you do not have one, you can find a very high-quality one via this link: [https://archive.org/details/SGM-V2.01](https://archive.org/details/SGM-V2.01)

### Step 2: Add the plugins to the AGS editor
1. For the system to work, drag these 3 DLL files (`agsimwrap-x32.dll`, `AGS.Plugin.IMWrap.Editor.dll`, and `imwrap_shim.dll`) **into the main AGS installation folder** (where `AGSEditor.exe` is located).
2. Open your project with the AGS editor.
3. In the tree on the right (the *Project Tree*), expand the **Plugins** node.
4. Find **iMWrap v6 AGS Plugin**, right-click on it, and check the box to activate it.

<img width="303" height="111" alt="image" src="https://github.com/user-attachments/assets/724ddb34-f295-4c61-a9c1-59d3f6cdd3fa" />

You will notice that the editor plugin itself does not need to be activated, and its icon (a small musical note) appears directly in the tree. We will come back to the use of this plugin in a later chapter.

> [!TIP]
> **Verification**: Once activated, the AGS editor automatically integrates all the plugin's functions (like `iMWrap_StartSound`) into its auto-completion system! You don't have an `.ash` script to import manually.

---

## 2.2. File Organization (The Data Folder)

For the music to play, the engine needs to find your `.ims` and `.sf2` files. 
The modern and clean way to handle this in AGS (version 3.6.0+) is to use a "Custom Data" folder.

1. Create a folder named, for example, `music_data` directly in your game folder.
2. Place your files there (e.g., `game.ims` and `SGM-V2.01.sf2`).
3. In the AGS editor, open the **General Settings** node, go down to the **Compiler** category.
4. In the **Package custom data folder(s)** field, type `music_data`.

During compilation, AGS will discreetly package your music into the executable or the `.vox` file, protecting your resources from prying eyes!

---

## 2.3. Initialization: The `game_start()` script

This is where the magic begins. We will tell AGS to turn on the audio engine as soon as the game launches. Open the **GlobalScript.asc**, and find the `game_start()` function.

Add these few lines:

```c
function game_start() {
    
    // 1. Set the audio driver (Synthesizer)
    // We use the built-in synthesizer (FluidSynth) and provide our SoundFont.
    // The keyword $DATA$ tells AGS to look in the configured "Package custom data folder".
    iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "$DATA$/music_data/SGM-V2.01.sf2");
    
    // 2. Load our score (the IMS bank)
    iMWrap_LoadBank("$DATA$/music_data/game.ims");
    
    // The engine is ready!
}
```

### Understanding `iMWrap_SetDriver`
The function takes two parameters: the driver type, and a string (which serves as a file path, or identifier depending on the driver).
- `IMWRAP_DRIVER_FLUIDSYNTH`: Uses the `.sf2` engine. Parameter 2 is the path to the SoundFont.
- `IMWRAP_DRIVER_ADLIB`: Emulates the OPL chip. Parameter 2 is ignored (leave `""`).
- `IMWRAP_DRIVER_HARDWARE_MT32`: Sends the signal to a real Roland synthesizer plugged into your PC. Parameter 2 is the Windows MIDI port index (e.g., `"0"`).

*(Note: We will see in Chapter 7 how to let the player choose their favorite driver).*

---

## 2.4. Play and Stop a Sound

Now that the bank is loaded, you can trigger the music anywhere in your game code (when the player enters a room, clicks a button, etc.).

In iMWrap, music tracks do not have direct "file names". They are stored in the IMS bank as **numbers (Sound ID)**. By LucasArts tradition, we give numbers in tens or twenties (e.g., 80 for the forest music, 81 for the inside of a cabin), but you can organize it however you like.

To start music 80:
```c
iMWrap_StartSound(80);
```

To stop music 80 smoothly:
```c
iMWrap_StopSound(80);
```

To cut everything (useful during a brutal Game Over or jumping to the main menu):
```c
iMWrap_StopAllSounds();
```

To check if a music track is currently playing:
```c
if (iMWrap_IsSoundActive(80)) {
    Display("The forest music is currently playing.");
}
```

> [!NOTE]
> The `iMWrap_StopSound(80)` function is not a "pause" in the audio sense of the word. It cleanly cuts the current notes (by sending MIDI *Note Off* signals so the sound fades naturally) and rewinds the score.

You now know how to initialize the engine and play music! In **Chapter 3**, we will learn how to manipulate this sound in real-time (change the volume, the tempo, and do cinematic fades).
