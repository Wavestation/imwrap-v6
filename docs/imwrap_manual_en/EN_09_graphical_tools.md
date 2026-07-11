# Chapter 9: Graphical Tools (GUI)

Working with interactive music involves manipulating technical data: mixing parameters, binary logic, and hexadecimal SysEx. 
The **iMWrap v6** project therefore provides three graphical interfaces (coded in Qt) to allow you to work visually: the **SysEx Generator**, the **Packer**, and the **Explorer**.

Here is the "core essence" and user manual for these tools, based on their code and profound utility in the composition workflow.

---

## 9.1. SysEx Generator: The Composer's Swiss Army Knife

<img width="802" height="801" alt="image" src="https://github.com/user-attachments/assets/8e27fbbb-bea2-44b0-8440-aa112e59ab7d" />

Forget hexadecimal converters on the internet, or even your good old TI-85. The **SysEx Generator** application is designed to be open alongside your sequencer (Cubase, Reaper, Logic, Digital Performer) while you compose.

Thanks to a tool like [LoopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) by Tobias Erichsen, you can establish a virtual MIDI cable between the SysEx Generator and your DAW, and record SysEx messages there as if they were any other MIDI event.

### "Generator" Tab (SysEx Creation)
This is where you craft the interactive messages to paste into your DAW.
1. **Type Dropdown**: Select what you want to generate (*Allocate Part*, *Hook Jump*, *Marker*, *Set Loop*, etc.).
2. **Dynamic Form**: Depending on your choice, the interface displays specific fields:
   - For *Allocate Part*: Checkboxes for `Part On`, `Reverb`, `Percussion`, and spinboxes for `Volume`, `Pan` (displayed clearly from -64 to +63), `Detune`, and MIDI `Program`. 
   - For *Hook*: Fields to target the *Track*, *Beat* (target measure), *Tick*, and the *Relative* value of the hook.
   - For *Set Loop*: The 4 vital fields: *Loop To Beat*, *To Tick*, *From Beat*, *From Tick*, and the *Count* (number of loops).
3. **AdLib Panel**: If you choose to send native AdLib parameters (`0x10`), the tool unlocks a full Hex editor and even allows **Import/Export of SBI files** (the old SoundBlaster Instrument format)!
4. **Direct Send**: The magic feature. The tool can connect directly to your PC's MIDI output (`sendMidiSysEx`). You can thus send the SysEx directly to your DAW, previously set to record mode, via a virtual MIDI cable, as explained above.
5. **Hexadecimal Generation**: At the bottom of the window, the software processes all your data, handles "nibble" encoding, and spits out the exact string (e.g., `F0 7D 00 01 05 0A... F7`). Click **Copy** and paste it onto your DAW's grid!

### "Decoder" Tab
The exact opposite: copy-paste a hexadecimal mess you found in an old iMUSE MIDI file, and the tool will describe in plain text what it does ("*This line allocates channel 2 with a volume of 127*").

---

## 9.2. Packer: The Bank Creation Workshop (.ims)

<img width="1002" height="782" alt="image" src="https://github.com/user-attachments/assets/b4137e92-a762-4989-97c5-de6afc16f7bb" />

> [!TIP]
> **The Packer also exists as a plugin for the AGS Editor!**
> This is the most flexible way to work: the plugin adds a node directly in your AGS project tree, a dedicated editing panel (integrated into your workspace), and an "iMWrap" menu in the AGS top menu bar. This allows you to manipulate and view the contents of your audio banks directly without ever leaving AGS!
> <img width="1325" height="816" alt="image" src="https://github.com/user-attachments/assets/7eb7139a-3377-443e-a0ee-fe70d8124d7b" />

The maintained `imwrappack` CLI now covers much of the same bank-editing workflow, but the **Packer GUI** remains the visual companion where you shape your soundtrack architecture more comfortably.

### How to use it?
1. **Sound List**: On the left, add your tracks. For example, click *Add Sound*, assign it ID `80`, and the name `Tavern`.
2. **Variant Box**: The same sound (ID 80) can contain multiple physical music files (for example General MIDI, MT-32, and AdLib). Select `GMD`, `ROL`, or `ADL` from the dropdown and click **Import MIDI** to locate the corresponding file.
3. **Tracks Table**: When importing a `.mid`, the tool analyzes the file and displays all the tracks it contains. You can delete or rearrange them (*Move Up* / *Move Down* buttons) if your DAW exported the tracks in the wrong order.
4. **Dynamic MDhd Header**: This is the Packer's great strength. Instead of writing AGS code like `iMWrap_SetSoundVolume(80, 50)`, you can check `Include MDHD` in the Packer and set the default **Priority**, **Volume**, **Pan**, **Transposition**, and **Speed** *directly inside the bank file*. The interface provides a "SpinBox" for each parameter.
5. **Save**: Save everything (File `->` Save Project). The tool saves **directly** to the final `.ims` bank format! You have no intermediate project file to manage.
6. The saved file is robust and ready to be dragged into the AGS folder. If you reopen it later in the Packer or Explorer, your entire architecture (sounds, variants, MDhd) will be restored.

Note that adding a MIDI file supports **all SMF formats**, with the following behaviors depending on the case:
* SMF Format 0: The file is directly added as a **Track** to the SMF 2 file embedded in the **Song**.
* SMF Format 1: All tracks in the file are merged, and the whole is imported as a **Track** to the SMF 2 file embedded in the **Song**.
* SMF Format 2: All tracks in the file are added individually as **Tracks** to the SMF 2 file embedded in the **Song**.

---

## 9.3. Explorer: The detailed analysis and editing tool for .ims projects

<img width="1402" height="1008" alt="image" src="https://github.com/user-attachments/assets/5e258643-6046-4571-8160-f4a169b96eed" />

The **Explorer** is the ultimate tool. It doesn't just look inside `.ims` files; it lets you listen to them and even modify them surgically *after* compilation.

### Audit and Visualization
- **Tree Widget**: On the left, navigate the hierarchy: `Sound (ID) -> Variant (GMD/ROL/ADL) -> Track`.
- **Hexadecimal Details**: Click on any event to see its binary structure decoded (`DetailsEdit_` panel).
- **Events Table**: Clicking a track displays a massive central table listing all notes, Control Changes, and SysEx of the track chronologically!

### Built-in Player (Preview)
No need for AGS to listen to your bank! The Explorer integrates the entire **iMWrap Core**. 
- Choose your *Device* and *Profile* (WinMM, Adlib emulation, strict SNM mode).
- You have *Play*, *Stop* buttons, and a playback timer (*Playback Position Label*) ticking away (e.g., `M:1 B:2 T:120`) so you can watch time pass live. You can even test looping behavior (Loop Check).

### On-the-Fly Remastering and Modification
The intent behind the Explorer is to let you "patch" an authoring mistake without having to reopen your sequencer (DAW) and recompile the whole project:
- **Add iMWrap Command / Edit Selected Event**: You can click on a MIDI event in the grid (e.g., the first beat of measure 10) and directly inject a Hook or a Marker in the middle of the track! 
- The interface provides dropdown menus to select the hook, and validates the injection of the event into the data structure.
- It mathematically handles the conversion between human-readable `Measure:Beat:Tick` (MBT) display and internal clocks, ensuring you know exactly where you place your event.

Once your edits on events or tracks are done (you can also *Import/Delete* tracks here), hit "Save" and your `.ims` file is updated on disk, corrected and ready.

Note that these tools may be improved as the development of the iMWrap system progresses!
