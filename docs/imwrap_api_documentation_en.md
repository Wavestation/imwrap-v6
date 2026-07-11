# iMWrap v6 C++ API Reference

This document reflects the public headers currently exposed in `include/imwrap/`.
It is a map of the maintained library surface, not a legacy snapshot limited to
three classes.

## Public API Overview

The public API is organized in five main layers:

- bank and format access
- MIDI and sequence parsing
- runtime playback and iMUSE-style control
- SysEx authoring and decoding
- bank authoring helpers

## 1. Bank And Format Layer

Headers:

- `imwrap/ImsFormat.h`
- `imwrap/ByteView.h`
- `imwrap/ResourceBank.h`

Main types:

- `VariantKind`: `Gmd`, `Rol`, `Adl`
- `TargetProfile`: `GeneralMidi`, `Mt32`, `Adlib`
- `MdhdData`: default runtime parameters embedded in a variant
- `SoundVariantView`: raw access to a stored variant chunk and its SMF payload
- `SoundResource`: loaded logical sound with variant selection helpers
- `ResourceBank`: opens and enumerates `.ims` banks

Key entry points:

- `ResourceBank::openFromFile(...)`
- `ResourceBank::openFromMemory(...)`
- `ResourceBank::header()`
- `ResourceBank::hasSound(...)`
- `ResourceBank::loadSound(...)`
- `ResourceBank::soundIds()`
- `SoundResource::hasVariant(...)`
- `SoundResource::variant(...)`
- `SoundResource::selectVariant(...)`

Typical usage:

```cpp
imwrap::ResourceBank bank;
std::string error;
if (!bank.openFromFile("game.ims", &error)) {
    throw std::runtime_error(error);
}

imwrap::SoundResource sound = bank.loadSound(80, &error);
imwrap::SoundVariantView variant = sound.selectVariant(imwrap::TargetProfile::Mt32);
```

## 2. MIDI And Sequence Layer

Headers:

- `imwrap/SmfSequence.h`
- `imwrap/IMWrapSequence.h`

Main types:

- `MidiEventType`
- `MidiEvent`
- `SmfTrack`
- `SmfSequence`
- `IMWrapSequence`

Key entry points:

- `SmfParser::Parse(...)`
- `SmfSerializer::Serialize(...)`
- `LoadIMWrapSequence(...)`

Behavior notes:

- SMF parsing is exposed independently from the runtime.
- `LoadIMWrapSequence(...)` combines a stored bank variant with decoded iMWrap
  control events.
- The maintained tools rely on this layer for importing SMF 0, 1, and 2 data.

## 3. Runtime Playback Layer

Headers:

- `imwrap/MidiSink.h`
- `imwrap/IMWrapCommand.h`
- `imwrap/IMWrapEngine.h`

Main types:

- `MidiSink`
- `Command`
- `CommandPacket`
- `IMWrapEngine`

`MidiSink` is the callback interface used by the runtime:

- `onMidiMessage(...)`
- `onSysEx(...)`
- `onCustomInstrument(...)`
- `onTempoChange(...)`
- `onAllNotesOff()`

`IMWrapEngine` is the main runtime object. The maintained public methods include:

Configuration:

- `setResourceBank(...)`
- `setTargetProfile(...)`
- `targetProfile()`
- `setMidiSink(...)`
- `initMt32()`
- `resetMt32Initialization()`
- `setCompatibilityProfile(...)`
- `compatibilityProfile()`
- `setLogCallback(...)`
- `setMarkerCallback(...)`
- `setNativeMt32Output(...)`
- `nativeMt32Output()`
- `setWelcomeMessage(...)`

Playback and state:

- `startSound(...)`
- `stopSound(...)`
- `stopAllSounds()`
- `isSoundActive(...)`
- `getSoundStatus(...)`
- `activeSoundIds()`
- `getPlaybackLocation(...)`
- `currentTempoMicrosPerQuarter()`
- `transportTicksPerSecond()`

Serialization:

- `Serialize(...)`
- `Deserialize(...)`

Clock advancement:

- `advanceAll(...)`
- `advanceMicroseconds(...)`
- `advanceSound(...)`

Direct control:

- `doCommand(const CommandPacket&)`
- `doCommand(uint16_t argc, const int16_t* args)`
- `checkScriptTrigger(...)`
- `fireAllScriptTriggers(...)`

Compatibility profiles:

- `CompatibilityProfile::GenericV6`
- `CompatibilityProfile::Snm`

## 4. SysEx Authoring And Decoding

Header:

- `imwrap/IMWrapSysex.h`

Main types:

- `IMWrapSysexDialect`: `GenericV6`, `Snm`
- `IMWrapSysexType`
- `IMWrapControlEvent`

Currently exposed SysEx types:

- `AllocatePart`
- `ShutdownPart`
- `StartSong`
- `AdlibPartInstrument`
- `AdlibGlobalInstrument`
- `ParameterAdjust`
- `HookJump`
- `HookGlobalTranspose`
- `HookPartOnOff`
- `HookSetVolume`
- `HookSetProgram`
- `HookSetTranspose`
- `Marker`
- `SetLoop`
- `ClearLoop`
- `SetInstrument`
- `Unknown`

Key entry points:

- `DecodeIMWrapSysex(...)`
- `EncodeIMWrapSysex(...)`
- `DescribeIMWrapSysex(...)`
- `ParseIMWrapSysexDescription(...)`

This layer is used by both the runtime and the authoring tools.

## 5. Bank Authoring Layer

Header:

- `imwrap/ImsWriter.h`

Main types:

- `VariantSource`
- `SoundBankInput`
- `ImsWriter`

Key entry points:

- `ImsWriter::build(...)`
- `ImsWriter::writeFile(...)`

Important details:

- a variant may come from `sourcePath` or from in-memory `smfData`
- `ImsWriter` writes `GMD`, `ROL`, and `ADL` chunks
- `MDhd` embedding is controlled per variant

## 6. Instruments, Channels, And Utilities

Headers:

- `imwrap/Instrument.h`
- `imwrap/InstrumentState.h`
- `imwrap/MidiChannel.h`
- `imwrap/SinkMidiChannel.h`
- `imwrap/ChannelAllocator.h`
- `imwrap/ScummAdlibSink.h`

Highlights:

- `Instrument` supports `Program`, `AdLib`, `Roland`, and `PcSpk`
- `RegisterRolandTimbreMapping(...)` and `ClearRolandTimbreMappings()`
- `SinkMidiChannel` adapts `MidiChannel` calls to a `MidiSink`
- `ChannelAllocator` exposes profile-aware MIDI channel assignment helpers
- `ScummAdlibSink` is a concrete `MidiSink` for AdLib-style rendering and preview

## 7. Public Header Index

For completeness, the maintained public headers are:

- `ByteView.h`: lightweight byte span helper
- `ChannelAllocator.h`: logical-to-physical channel allocation policy
- `Export.h`: DLL export macros
- `ImsFormat.h`: IMS container metadata and enums
- `ImsWriter.h`: bank authoring API
- `IMWrapCommand.h`: runtime command enum and packet type
- `IMWrapEngine.h`: main playback runtime
- `IMWrapSequence.h`: decoded bank variant plus control-event view
- `IMWrapSysex.h`: iMWrap SysEx parsing and encoding
- `Instrument.h`: instrument authoring and transmission
- `InstrumentState.h`: plain instrument state structures
- `MidiChannel.h`: abstract outgoing MIDI channel
- `MidiSink.h`: abstract runtime sink
- `ResourceBank.h`: IMS loading and sound lookup
- `ScummAdlibSink.h`: AdLib preview/render sink
- `SinkMidiChannel.h`: sink-backed MIDI channel implementation
- `SmfSequence.h`: SMF parser and serializer API

## 8. Scope Note

This document intentionally describes the maintained C++ library surface.
Legacy Swift tooling and the original CLI packer now live under `legacy-tools/`
and are no longer part of the primary maintained API surface.
