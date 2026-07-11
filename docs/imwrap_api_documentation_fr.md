# Reference de l'API C++ iMWrap v6

Ce document reflete les headers publics exposes actuellement dans
`include/imwrap/`. Il sert de carte de la surface maintenue de la librairie.

## Vue D'Ensemble

L'API publique est organisee en cinq couches principales :

- acces au format IMS et aux banques
- parsing MIDI et sequences
- runtime de lecture et controle type iMUSE
- authoring et decodage SysEx
- ecriture de banques

## 1. Couche Banque Et Format

Headers :

- `imwrap/ImsFormat.h`
- `imwrap/ByteView.h`
- `imwrap/ResourceBank.h`

Types principaux :

- `VariantKind` : `Gmd`, `Rol`, `Adl`
- `TargetProfile` : `GeneralMidi`, `Mt32`, `Adlib`
- `MdhdData` : parametres runtime par defaut embarques dans une variante
- `SoundVariantView` : acces brut a une variante stockee et a son payload SMF
- `SoundResource` : son logique charge avec helpers de selection de variante
- `ResourceBank` : ouverture et enumeration des banques `.ims`

Points d'entree principaux :

- `ResourceBank::openFromFile(...)`
- `ResourceBank::openFromMemory(...)`
- `ResourceBank::header()`
- `ResourceBank::hasSound(...)`
- `ResourceBank::loadSound(...)`
- `ResourceBank::soundIds()`
- `SoundResource::hasVariant(...)`
- `SoundResource::variant(...)`
- `SoundResource::selectVariant(...)`

Exemple typique :

```cpp
imwrap::ResourceBank bank;
std::string error;
if (!bank.openFromFile("game.ims", &error)) {
    throw std::runtime_error(error);
}

imwrap::SoundResource sound = bank.loadSound(80, &error);
imwrap::SoundVariantView variant = sound.selectVariant(imwrap::TargetProfile::Mt32);
```

## 2. Couche MIDI Et Sequences

Headers :

- `imwrap/SmfSequence.h`
- `imwrap/IMWrapSequence.h`

Types principaux :

- `MidiEventType`
- `MidiEvent`
- `SmfTrack`
- `SmfSequence`
- `IMWrapSequence`

Points d'entree principaux :

- `SmfParser::Parse(...)`
- `SmfSerializer::Serialize(...)`
- `LoadIMWrapSequence(...)`

Notes de comportement :

- le parsing SMF est expose independamment du runtime
- `LoadIMWrapSequence(...)` combine une variante issue d'une banque avec les
  evenements de controle iMWrap decodes
- les outils maintenus s'appuient sur cette couche pour importer du SMF 0, 1 et 2

## 3. Couche Runtime De Lecture

Headers :

- `imwrap/MidiSink.h`
- `imwrap/IMWrapCommand.h`
- `imwrap/IMWrapEngine.h`

Types principaux :

- `MidiSink`
- `Command`
- `CommandPacket`
- `IMWrapEngine`

`MidiSink` est l'interface de callbacks du runtime :

- `onMidiMessage(...)`
- `onSysEx(...)`
- `onCustomInstrument(...)`
- `onTempoChange(...)`
- `onAllNotesOff()`

`IMWrapEngine` est l'objet runtime principal. Les methodes publiques maintenues
incluent :

Configuration :

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

Lecture et etat :

- `startSound(...)`
- `stopSound(...)`
- `stopAllSounds()`
- `isSoundActive(...)`
- `getSoundStatus(...)`
- `activeSoundIds()`
- `getPlaybackLocation(...)`
- `currentTempoMicrosPerQuarter()`
- `transportTicksPerSecond()`

Serialization :

- `Serialize(...)`
- `Deserialize(...)`

Avancement du temps :

- `advanceAll(...)`
- `advanceMicroseconds(...)`
- `advanceSound(...)`

Controle direct :

- `doCommand(const CommandPacket&)`
- `doCommand(uint16_t argc, const int16_t* args)`
- `checkScriptTrigger(...)`
- `fireAllScriptTriggers(...)`

Profils de compatibilite :

- `CompatibilityProfile::GenericV6`
- `CompatibilityProfile::Snm`

## 4. Authoring Et Decodage SysEx

Header :

- `imwrap/IMWrapSysex.h`

Types principaux :

- `IMWrapSysexDialect` : `GenericV6`, `Snm`
- `IMWrapSysexType`
- `IMWrapControlEvent`

Types SysEx actuellement exposes :

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

Points d'entree principaux :

- `DecodeIMWrapSysex(...)`
- `EncodeIMWrapSysex(...)`
- `DescribeIMWrapSysex(...)`
- `ParseIMWrapSysexDescription(...)`

Cette couche est utilisee a la fois par le runtime et par les outils d'authoring.

## 5. Couche D'Ecriture De Banque

Header :

- `imwrap/ImsWriter.h`

Types principaux :

- `VariantSource`
- `SoundBankInput`
- `ImsWriter`

Points d'entree principaux :

- `ImsWriter::build(...)`
- `ImsWriter::writeFile(...)`

Details importants :

- une variante peut venir de `sourcePath` ou de `smfData` en memoire
- `ImsWriter` ecrit des chunks `GMD`, `ROL` et `ADL`
- l'injection de `MDhd` se controle par variante

## 6. Instruments, Canaux Et Utilitaires

Headers :

- `imwrap/Instrument.h`
- `imwrap/InstrumentState.h`
- `imwrap/MidiChannel.h`
- `imwrap/SinkMidiChannel.h`
- `imwrap/ChannelAllocator.h`
- `imwrap/ScummAdlibSink.h`

Points marquants :

- `Instrument` supporte `Program`, `AdLib`, `Roland` et `PcSpk`
- `RegisterRolandTimbreMapping(...)` et `ClearRolandTimbreMappings()`
- `SinkMidiChannel` adapte les appels `MidiChannel` vers un `MidiSink`
- `ChannelAllocator` expose une logique d'allocation de canaux dependante du profil
- `ScummAdlibSink` est un `MidiSink` concret pour le rendu et la preecoute AdLib

## 7. Index Des Headers Publics

Pour etre complet, les headers publics maintenus sont :

- `ByteView.h` : vue legere sur un buffer d'octets
- `ChannelAllocator.h` : politique d'allocation des canaux logiques/physiques
- `Export.h` : macros d'export DLL
- `ImsFormat.h` : metadonnees IMS et enums
- `ImsWriter.h` : API d'ecriture de banque
- `IMWrapCommand.h` : enum de commandes runtime et paquet de commande
- `IMWrapEngine.h` : runtime principal de lecture
- `IMWrapSequence.h` : variante de banque decodee plus vue des evenements de controle
- `IMWrapSysex.h` : parsing et encodage SysEx iMWrap
- `Instrument.h` : authoring et emission d'instruments
- `InstrumentState.h` : structures d'etat instrument
- `MidiChannel.h` : canal MIDI de sortie abstrait
- `MidiSink.h` : sink runtime abstrait
- `ResourceBank.h` : chargement IMS et lookup des sons
- `ScummAdlibSink.h` : sink de rendu/preview AdLib
- `SinkMidiChannel.h` : implementation de canal basee sur un sink
- `SmfSequence.h` : API de parsing et de serialisation SMF

## 8. Note De Perimetre

Ce document decrit volontairement la surface maintenue de la librairie C++.
Les anciens outils Swift et l'ancien packer CLI vivent maintenant sous
`legacy-tools/` et ne font plus partie de la surface API principale maintenue.
