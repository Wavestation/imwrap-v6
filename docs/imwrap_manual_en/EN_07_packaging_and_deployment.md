# Chapter 7: Packaging and Configuration

You have programmed your interactions in AGS, and your `.mid` files are packed with carefully constructed SysEx. It's time to package it all up for the players!

In this final chapter, we will see how to merge your files with `imwrappack`, and how to let the player choose their favorite sound card thanks to the `.imc` configuration file.

---

## 7.1. The "imwrappack" utility

The iMWrap engine does not directly read `.mid` files. It reads `.ims` (iMWrap Music Set) banks.
Not only does the IMS bank contain all the game's music tracks, but it can group **multiple variants of the same piece**.

Why do this?
Imagine you composed "Music 80" specifically for the synthetic rendering of the Roland MT-32, but you made another version for General MIDI.
Thanks to the IMS bank, the AGS game will simply call `iMWrap_StartSound(80)`. The engine will look at the player's sound card and automatically pick the MT-32 version or the GM version from within the `.ims` file!

### Using imwrappack

The `imwrappack` tool is used from the command line (in the Windows command prompt or Mac/Linux terminal).

The current CLI can both build a bank from scratch and edit an existing `.ims` bank.

**Basic build syntax:**
```bash
imwrappack build output.ims \
  --name=80:Forest \
  --mdhd=80:gmd:90:127:0:0:0:128 \
  80:gmd=80_forest_generalmidi.mid \
  80:rol=80_forest_mt32.mid \
  80:adl=80_forest_adlib.mid \
  81:gmd=81_interior.mid
```

In this example:
1. We create the `output.ims` file.
2. `--name=80:Forest`: We assign an informative internal name to track 80 (very handy for debugging).
3. `80:gmd=...`: We integrate our `.mid` file as the **General MIDI** variant (`gmd`) for sound 80.
4. `80:rol=...`: We integrate our second `.mid` file as the **Roland MT-32** variant (`rol`) for the same sound 80.
5. `80:adl=...`: We integrate an **AdLib** variant (`adl`) for the same sound.
6. `81:gmd=...`: We add music 81 (GM variant only).

Supported variants are `gmd`, `rol`, and `adl`.

Unlike the original v1 CLI, the maintained packer accepts **SMF 0, 1, and 2**:
- SMF 0 is imported as one track.
- SMF 1 is merged into one format-0 style track.
- SMF 2 imports each source track as its own IMS track.

### Configuring priorities and volumes (`MDhd`)
You can force the default parameters of a variant (priority, volume, speed, etc.) directly during packaging without having to edit the MIDI, by injecting an `MDhd` chunk:

```bash
--mdhd=80:gmd:90:127:0:0:0:128
```
*(The order is: soundId : variant : priority : volume : pan : transpose : detune : speed. Here, we force a priority of 90 and a volume of 127).*

The same CLI can also inspect and edit an existing bank:

```bash
imwrappack inspect output.ims
imwrappack import-midi output.ims 80 gmd replacement.mid
imwrappack move-track output.ims 80 gmd 1 up
imwrappack export-track output.ims 80 gmd 0 forest_track0.mid
```

If you prefer a visual workflow, **Chapter 9** covers the graphical tools, especially the **Packer**.

---

## 7.2. Player Configuration (The `.imc` file)

The charm of a retro game is also letting the player tinker with their audio settings. The iMWrap plugin manages this via an external configuration file.

This file must be named **[YourExeName].imc** (for example `MyGame.imc`) and must be placed in the same folder as the game executable.

### Structure of the `.imc` file (INI Format)
```ini
[MIDI]
Driver=2
Device=loopMIDI Port
```

**Explanation of parameters:**
- `Driver=0`: The player wants to use FluidSynth (Software General MIDI synth).
- `Driver=1`: The player wants AdLib FM emulation (retro bleep-bloop).
- `Driver=2`: Hardware General MIDI (use the real hardware synth plugged into the computer).
- `Driver=3`: Hardware Roland MT-32 (real vintage synthesizer).
- `Device`: Name of the Windows hardware port (only required for Drivers 2 and 3).

### Handling it in the AGS script
In your `game_start()` function (see Chapter 2), replace the basic code with this to allow the player to "override" your choices:

```c
function game_start() {
    // If the player has placed a MyGame.imc file
    if (iMWrap_HasExternalConfig()) {
        // Try to apply their settings. If they chose FluidSynth (0), 
        // we force our own default SoundFont as a fallback parameter.
        iMWrap_ApplyExternalConfig("$DATA$/music_data/SGM-V2.01.sf2");
    } else {
        // Otherwise, no .imc file found, we configure the game 
        // in modern General MIDI by default.
        iMWrap_SetDriver(IMWRAP_DRIVER_FLUIDSYNTH, "$DATA$/music_data/SGM-V2.01.sf2");
    }

    iMWrap_LoadBank("$DATA$/music_data/game.ims");
}
```

---

## 7.3. The graphical utility `SetMIDI.exe`

Rather than forcing your players to manually write an `.imc` text file, you can distribute the small `SetMIDI.exe` utility provided with iMWrap.

Put `SetMIDI.exe` in the same folder as your game. When the player double-clicks it, a native graphical interface opens (in French, Spanish, or English, depending on the system language). 
The tool will scan the folder, find your game's executable, list the real MIDI ports plugged into the computer, and automatically generate the `.imc` file corresponding to the player's choice!

---

**Congratulations!** You have almost reached the end of the iMWrap v6 manual. From loading an AGS plugin to the mysteries of hexadecimal SysEx, interactive music in the style of iMUSE holds no more secrets for you. The **next two chapters** are reference manuals for the **iMWrap AGS plugin functions** as well as for **using the graphical tools**.

Happy developing and happy composing!
