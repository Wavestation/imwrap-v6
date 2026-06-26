# Detailed iMWrap v6 C++ API Documentation

This document exhaustively references the public methods of the main classes in the iMWrap v6 engine (`ResourceBank`, `MidiSink`, and `IMWrapEngine`), providing usage examples for each.

---

## 1. Class `imwrap::ResourceBank`

The `ResourceBank` class manages memory and loading of `.ims` (Interactive Music System) files.

### `bool openFromFile(const std::string &path, std::string *error = nullptr)`
Loads a music bank from a file path on disk.
- **Parameters**: 
  - `path`: The path to the `.ims` file.
  - `error` (optional): Pointer to a string to retrieve an error message.
- **Returns**: `true` if loading succeeded, otherwise `false`.
- **Example**:
```cpp
imwrap::ResourceBank bank;
std::string err;
if (!bank.openFromFile("data/music.ims", &err)) {
    std::cerr << "Loading error: " << err << std::endl;
}
```

### `bool openFromMemory(std::vector<uint8_t> data, std::string *error = nullptr)`
Loads a music bank from a memory buffer (useful if your assets are packed/compressed).
- **Parameters**: 
  - `data`: Byte vector containing the `.ims` file structure.
  - `error` (optional): Pointer to a string for the error message.
- **Returns**: `true` on success.
- **Example**:
```cpp
std::vector<uint8_t> myData = LoadMyVFSFile("music.ims");
bank.openFromMemory(myData);
```

### `bool isOpen() const`
Checks if a valid bank is currently loaded.
- **Returns**: `true` if open.
- **Example**:
```cpp
if (bank.isOpen()) { std::cout << "Bank is ready!" << std::endl; }
```

### `bool hasSound(uint16_t soundId) const`
Checks if a specific sound ID exists in the bank.
- **Parameters**: `soundId`: The iMUSE sound identifier.
- **Example**:
```cpp
if (bank.hasSound(10)) { engine.startSound(10); }
```

### `std::vector<uint16_t> soundIds() const`
Retrieves the list of all available sound identifiers in this bank.
- **Returns**: A vector of IDs (`uint16_t`).
- **Example**:
```cpp
for (uint16_t id : bank.soundIds()) {
    std::cout << "Available sound: " << id << std::endl;
}
```

---

## 2. Interface `imwrap::MidiSink`

This virtual interface is used by the engine to output MIDI events to be played. You **must** derive a class from it to link it to your audio backend.

### `virtual void onMidiMessage(uint16_t soundId, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) = 0`
**Required.** Called for every standard MIDI instruction (Note On, Note Off, Control Change, Pitch Bend...).
- **Parameters**:
  - `soundId`: The sound that emitted the instruction.
  - `status`: The MIDI status byte (e.g., `0x90` for Note On on channel 0).
  - `data1`: First data byte (e.g., the note, from 0 to 127).
  - `hasData2`: Indicates whether the command has a second data byte.
  - `data2`: Second data byte (e.g., velocity).
- **Example**:
```cpp
void onMidiMessage(uint16_t sid, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) override {
    uint32_t msg = status | (data1 << 8);
    if (hasData2) msg |= (data2 << 16);
    midiOutShortMsg(hMidiOut, msg); // Windows WinMM API example
}
```

### `virtual void onSysEx(uint16_t soundId, ByteView message)`
Called when a long System Exclusive (SysEx) event is triggered (heavily used on MT-32 or for synth initialization).
- **Example**:
```cpp
void onSysEx(uint16_t soundId, imwrap::ByteView message) override {
    // Use message.data() and message.size() to send the MIDI SysEx buffer
    SendSysExBufferToHardware(message.data(), message.size());
}
```

### `virtual void onTempoChange(uint16_t soundId, uint32_t microsPerQuarter)`
Called when the engine signals a tempo change (usually originating from the internal SMF file).
- **Example**:
```cpp
void onTempoChange(uint16_t soundId, uint32_t microsPerQuarter) override {
    double bpm = 60000000.0 / microsPerQuarter;
    std::cout << "Tempo changed to: " << bpm << " BPM" << std::endl;
}
```

### `virtual void onAllNotesOff()`
Called when the engine forces all sound to stop (e.g., during `stopAllSounds()`). Useful for sending a "Panic" command to a synthesizer.

---

## 3. Class `imwrap::IMWrapEngine`

The core of the system. This instance manages time, sound priorities, and executes interactive logic.

### Initialization and Configuration

#### `explicit IMWrapEngine(const ResourceBank *bank = nullptr)`
#### `void setResourceBank(const ResourceBank *bank)`
#### `void setMidiSink(MidiSink *sink)`
Associates the engine with a resource bank and an output interface.
- **Example**:
```cpp
imwrap::IMWrapEngine engine;
engine.setResourceBank(&myBank);
engine.setMidiSink(&mySynth);
```

#### `void setTargetProfile(TargetProfile profile)`
Configures the target output format. Useful for adapting events to hardware standards.
- **Parameters**: `profile`: `TargetProfile::GeneralMidi`, `TargetProfile::RolandMT32`, etc.
- **Example**:
```cpp
engine.setTargetProfile(imwrap::TargetProfile::GeneralMidi);
```

#### `void setLogCallback(LogCallback cb)`
#### `void setMarkerCallback(MarkerCallback cb)`
Registers anonymous functions (lambdas) to capture text events (logs) or musical events (iMUSE MIDI markers).
- **Example**:
```cpp
engine.setMarkerCallback([](uint16_t soundId, uint8_t marker) {
    std::cout << "Sound " << soundId << " reached marker " << (int)marker << "!\n";
    // Perfect for synchronizing visual animations with the music.
});
```

### Playback Control

#### `bool startSound(uint16_t soundId)`
Starts or schedules the playback of a sound.
- **Returns**: `true` if the sound was found and initiated.
- **Example**: `engine.startSound(15);`

#### `void stopSound(uint16_t soundId)`
Gracefully stops a sound (by generating appropriate Note Off messages).
- **Example**: `engine.stopSound(15);`

#### `void stopAllSounds()`
Stops all currently playing music.

#### `bool isSoundActive(uint16_t soundId) const`
Checks if a sound is still playing.
- **Example**: 
```cpp
if (engine.isSoundActive(10)) { /* Intro music is still playing */ }
```

#### `int getSoundStatus(uint16_t soundId) const`
Returns an integer describing the specific status of the sound (0 if inactive, 1 if active... useful for a high-level wrapper).

#### `bool getPlaybackLocation(uint16_t soundId, uint16_t *track, uint16_t *beat, uint16_t *tick) const`
Retrieves the precise location of the playback head. Extremely useful for debugging or "Now Playing" UI displays.
- **Example**:
```cpp
uint16_t trk, beat, tick;
if (engine.getPlaybackLocation(10, &trk, &beat, &tick)) {
    printf("Position: Track %d, Beat %d, Tick %d\n", trk, beat, tick);
}
```

### Time Management

#### `double transportTicksPerSecond() const`
Retrieves the engine's time resolution (the theoretical number of sequencer "ticks" that must elapse per second). Generally, this is based on the Standard MIDI format (e.g., 60 or 120 Hz).
- **Example**: `double ticksPerSec = engine.transportTicksPerSecond();`

#### `void advanceAll(uint32_t deltaTicks)`
Advances the main sequencer clock for all sounds. **This is the most important function to call in your main loop.**
- **Example**:
```cpp
// If the frame lasts 0.016s and the transportTick is at 120Hz:
// deltaTicks = 0.016 * 120 = 1.92 -> cast to 2 ticks.
engine.advanceAll(2);
```

### Interactive Commands (iMUSE Protocol)

#### `int32_t doCommand(uint16_t argc, const int16_t *args)`
Sends an arbitrary iMUSE interactive instruction to the sequencer. Command codes are located in `imwrap::Command`.

**Example 1: Changing a specific track's volume**
(iMUSE Syntax: Set Volume)
```cpp
int16_t args[] = { 
    (int16_t)imwrap::Command::PlayerSetPartVolume, 
    10,    // soundId
    0,     // channel (0 = track 1)
    100    // New volume (0-127)
};
engine.doCommand(4, args);
```

**Example 2: Jumping to a rhythm marker (Jump)**
Jump to "beat 10" of "track 0".
```cpp
int16_t args[] = {
    (int16_t)imwrap::Command::PlayerJump,
    10,    // soundId
    0,     // track
    10,    // beat
    0      // tick
};
engine.doCommand(5, args);
```

**Example 3: Setting a "Hook" (fade or conditional action)**
Automatically triggers an iMUSE command when the musical sequence reaches a certain point.
```cpp
// Arguments depend on the iMUSE Hook payload.
int16_t args[] = { (int16_t)imwrap::Command::PlayerSetHook, /* ... */ };
engine.doCommand(argc, args);
```

### Save and Restore (Save states)

#### `bool Serialize(std::ostream &os) const`
#### `bool Deserialize(std::istream &is)`
Saves the exact state of the engine (playing notes, command states, sequencers) to a binary stream, or restores it. Indispensable for implementing a "Save/Load" state system in a game.
- **Example**:
```cpp
std::ofstream saveFile("savegame.bin", std::ios::binary);
engine.Serialize(saveFile);

// Later on:
std::ifstream loadFile("savegame.bin", std::ios::binary);
engine.Deserialize(loadFile);
```
