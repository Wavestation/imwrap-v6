# Chapter 1: Introduction to iMWrap and iMUSE

Welcome to the comprehensive manual for **iMWrap v6 (current version: 1.0.7)**. If you are here, you likely have an ambition: for your game's music to react to the action, just like a film score adapts to what happens on screen.

In this chapter, we will demystify the basic concepts, understand where this technology comes from, and see how it integrates with the Adventure Game Studio (AGS) engine.

---

## 1.1. The Legacy of iMUSE

In the early 90s, composers Michael Land and Peter McConnell created a revolutionary system for LucasArts called **iMUSE** (Interactive Music Streaming Engine). Its first masterful appearance was in *Monkey Island 2: LeChuck's Revenge*.

At the time, video game music was often a simple loop playing in the background. With iMUSE, music became "smart":
* When moving from one room to another (e.g., entering a building), the music would smoothly change instrumentation or melody, without ever losing the beat.
* Game events (like a joke or a sudden action) could trigger musical accents (stingers or cymbal crashes) that always landed perfectly on the strong beat of the measure.

**iMWrap v6** is the direct heir to this system. It recreates the logic of interactive jumps, smooth transitions, and rhythm synchronization (the famous *Hooks* and *Markers*), while making them available to developers using AGS today.

---

## 1.2. What is iMWrap v6?

iMWrap is not a simple "MP3 player" or "OGG player". It is an **interactive MIDI sequencer**.

Unlike an audio file (MP3, WAV) which is frozen, a MIDI file is a digital score. It tells the system: *"Play a C major with a piano sound for 1 second"*. 
Because the music is not frozen, **the engine can manipulate it in real-time**:
- It can mute the drum track on the fly.
- It can wait for the end of the measure to jump to another section.
- It can send a signal to the game at the exact moment a note is played.

To work in a modern way without relying on old 90s sound cards, the `agsimwrap` plugin (which you will use) embeds several technologies:

1. **The iMWrap Core**: This is the brain. It reads the score, calculates the tempo, interprets interactive commands (loops, jumps), and decides which notes to play.
2. **The Synthesizer**: This is the orchestra. iMWrap integrates **FluidSynth**, a high-performance software synthesizer that uses *SoundFonts* (`.sf2` files containing audio samples of real instruments) to produce the final sound. It also supports FM emulation (AdLib) and hardware cards (Roland MT-32).
3. **The Audio Engine (miniaudio)**: This is the speaker. It ensures that the sound is generated in the background of the computer, smoothly, without being interrupted by AGS loading screens or framerate drops.

> [!TIP]
> Why v6 in the name, when the official version is 1.0.4? In fact, this v6 is a nod to SCUMM v6, the engine version where iMUSE truly stabilized. Consider it a tribute...

---

## 1.3. Different Sound Profiles

Depending on the aesthetic you want to give your game, iMWrap handles several MIDI profiles. It is crucial to understand them, as this will influence how you prepare your music:

### General MIDI (GM) (FluidSynth or Hardware)
This is the current standard. General MIDI guarantees that instrument number 1 is a piano, 25 is a guitar, etc. By providing a *SoundFont* (.sf2) with your game, you guarantee via the FluidSynth driver that all players will hear exactly the same thing. But iMWrap also allows sending the General MIDI stream to a real hardware synthesizer (such as a Roland SC-55 or a Yamaha MU128 for the most well-known) plugged into the computer!

### Roland MT-32 (Hardware or Emulated)
The Roland MT-32 was the Rolls-Royce of PC sound in the late 80s. Unlike GM, it allows sculpting custom sounds (Timbres) via subtractive synthesis. iMWrap knows how to send the specific MT-32 "SysEx" data to faithfully recreate the atmosphere of vintage games. If the player does not have a real MT-32, they can use the open-source emulation provided by the MUNT project.

### AdLib / OPL
The famous AdLib (and SoundBlaster) sound card generated sound via "FM Synthesis". This is the metallic and nostalgic "beep-boop" sound characteristic of the early 90s. iMWrap has a native AdLib driver for this very specific rendering. *(Note: In the current 1.0.4 version, experimental AdLib support is included but is not yet 100% verified).*

---

## 1.4. Global Flow Architecture

To summarize, here is what happens under the hood of your AGS game:

1. **AGS (The global conductor)** says: *"I'm loading the graveyard room, play music 80!"*
2. **iMWrap Core** reads the `.ims` musical file (the compiled file containing all the game's musical files).
3. **iMWrap** encounters an event in the music: *"At measure 4, tell AGS to display a lightning bolt."* -> Sends a **Marker** to AGS.
4. **iMWrap** sends the MIDI notes to **FluidSynth**.
5. **FluidSynth** reads the sounds in the SoundFont and generates the audio signal.
6. The audio stream goes out through the speakers via **miniaudio**.

Now that the stage is set, let's move to **Chapter 2** to install all this in your AGS project!
