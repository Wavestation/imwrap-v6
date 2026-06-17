/* ==========================================================================
 *
 * iMWRAP V6 - A modern iMuse implementation attempt with Adventure Game Studio Companion Plugin
 *
 * This program is the legal property of Masami Komuro and few other contributors,
 * Please refer to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ========================================================================== */

#ifndef IMUSE_IMUSE_ENGINE_H
#define IMUSE_IMUSE_ENGINE_H

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <vector>

#include "imuse/ImuseCommand.h"
#include "imuse/ImuseSequence.h"
#include "imuse/Instrument.h"
#include "imuse/MidiSink.h"
#include "imuse/ResourceBank.h"

namespace imuse {

class ImuseEngine {
public:
    enum class CompatibilityProfile {
        GenericV6,
        SamAndMax
    };

    explicit ImuseEngine(const ResourceBank *bank = nullptr) : _bank(bank) {}

    void setResourceBank(const ResourceBank *bank) { _bank = bank; }
    void setTargetProfile(TargetProfile profile) { _profile = profile; }
    TargetProfile targetProfile() const { return _profile; }
    void setMidiSink(MidiSink *sink) { _midiSink = sink; }
    void setCompatibilityProfile(CompatibilityProfile profile) { _compatibility = profile; }
    CompatibilityProfile compatibilityProfile() const { return _compatibility; }
    void setNativeMt32Output(bool enabled) { _nativeMt32Output = enabled; }
    bool nativeMt32Output() const { return _nativeMt32Output; }

    bool startSound(uint16_t soundId);
    void stopSound(uint16_t soundId);
    void stopAllSounds();
    bool isSoundActive(uint16_t soundId) const;
    int getSoundStatus(uint16_t soundId) const;
    std::vector<uint16_t> activeSoundIds() const;
    bool getPlaybackLocation(uint16_t soundId, uint16_t *track, uint16_t *beat, uint16_t *tick) const;
    uint32_t currentTempoMicrosPerQuarter() const;
    double transportTicksPerSecond() const;
    void advanceAll(uint32_t deltaTicks);
    bool advanceSound(uint16_t soundId, uint32_t deltaTicks);

    int32_t doCommand(const CommandPacket &packet);
    int32_t doCommand(uint16_t argc, const int16_t *args);

    int checkScriptTrigger(uint16_t soundId, int marker) const;
    int fireAllScriptTriggers(uint16_t soundId);

private:
    struct HookState {
        std::array<uint8_t, 2> jump = {{0, 0}};
        uint8_t transpose = 0;
        std::array<uint8_t, 16> partOnOff = {{0}};
        std::array<uint8_t, 16> partVolume = {{0}};
        std::array<uint8_t, 16> partProgram = {{0}};
        std::array<uint8_t, 16> partTranspose = {{0}};

        void reset();
        int queryParam(int param, uint8_t chan) const;
        int set(uint8_t cls, uint8_t value, uint8_t chan);
    };

    struct PartState {
        bool initialized = false;
        bool present = false;
        bool transmitted = false;
        bool on = false;
        bool percussion = false;
        bool unassignedInstrument = false;
        uint8_t channel = 0;
        uint8_t volume = 127;
        uint8_t effectiveVolume = 127;
        uint8_t program = 0;
        uint16_t instrumentValue = 0;
        int16_t pitchBend = 0;
        uint8_t pitchBendFactor = 2;
        uint8_t volControlSensitivity = 127;
        int8_t transpose = 0;
        int8_t transposeEffective = 0;
        int16_t detune = 0;
        int16_t detuneEffective = 0;
        int8_t pan = 0;
        int8_t panEffective = 0;
        uint8_t polyphony = 1;
        uint8_t modwheel = 0;
        bool pedal = false;
        int8_t priority = 0;
        uint8_t priorityEffective = 0;
        uint8_t effectLevel = 64;
        uint8_t chorus = 0;
        uint8_t bank = 0;
        int8_t outputChannel = -1;
        uint16_t ownerSoundId = 0;
        Instrument instrument;
        std::array<uint8_t, 128> sourceNotes = {{}};
        std::array<uint8_t, 128> sourceOutputNotes = {{}};
        std::array<uint8_t, 128> sourceOutputChannels = {{}};
        std::array<uint8_t, 128> sourceVelocities = {{}};
    };

    struct FadeState {
        bool active = false;
        uint8_t param = 0;
        int target = 0;
        int time = 0;
    };

    struct MidiChannelState {
        std::array<uint8_t, 128> heldNotes = {{}};
        std::array<uint8_t, 128> soundingNotes = {{}};
        uint8_t sustainValue = 0;

        void reset();
        bool sustainOn() const { return sustainValue >= 64; }
        bool hasActiveNotes() const;
    };

    struct HangingNote {
        uint8_t channel = 0;
        uint8_t note = 0;
        uint32_t ticksLeft = 0;
    };

    struct ActiveSound {
        uint16_t soundId = 0;
        SoundVariantView variant;
        ImuseSequence sequence;
        bool isMidi = true;
        bool isMt32 = false;
        bool supportsPercussion = true;
        uint8_t priority = 128;
        uint8_t volume = 127;
        uint8_t effectiveVolume = 127;
        int8_t pan = 0;
        int8_t transpose = 0;
        int16_t detune = 0;
        uint8_t speed = 128;
        uint32_t tempoMicrosPerQuarter = 500000;
        uint16_t trackIndex = 0;
        uint16_t beatIndex = 1;
        uint16_t tickIndex = 0;
        uint16_t loopCounter = 0;
        uint16_t loopToBeat = 1;
        uint16_t loopToTick = 0;
        uint16_t loopFromBeat = 1;
        uint16_t loopFromTick = 0;
        uint16_t volChan = 0xFFFFu;
        bool pendingImmediateEvents = true;
        std::size_t nextEventHint = 0;
        std::array<PartState, 16> parts = {{}};
        std::array<MidiChannelState, 16> midiChannels = {{}};
        std::vector<HangingNote> hangingNotes;
        HookState hook;
        FadeState fade;
    };

    struct QueuedTrigger {
        uint16_t soundId = 0;
        uint8_t marker = 0;
        bool finalized = false;
        std::vector<CommandPacket> commands;
    };

    struct ScriptTrigger {
        uint16_t soundId = 0;
        uint8_t id = 0;
        uint16_t expire = 0;
        CommandPacket command;
    };

    struct DeferredCommand {
        uint32_t ticksLeft = 0;
        CommandPacket command;
    };

    struct ScanContext {
        bool active = false;
        uint16_t soundId = 0;
        std::array<uint16_t, 128> activeNotes = {{}};

        void reset() {
            active = false;
            soundId = 0;
            activeNotes.fill(0);
        }
    };

    struct RhythmState {
        uint8_t vol = 127;
        uint8_t poly = 1;
        uint8_t prio = 0;
    };

    ActiveSound *findActiveSound(uint16_t soundId);
    const ActiveSound *findActiveSound(uint16_t soundId) const;
    uint8_t getChannelVolume(uint16_t volChan) const;
    void refreshEffectiveVolume(ActiveSound *sound);
    int getSoundParam(const ActiveSound &sound, int param, uint8_t chan) const;
    int queryPartParam(const ActiveSound &sound, int param, uint8_t chan) const;
    int setSoundTranspose(ActiveSound *sound, uint8_t relative, int value);
    uint32_t ticksPerBeat(const ActiveSound &sound) const;
    uint32_t absoluteTick(const ActiveSound &sound) const;
    uint32_t trackEndTick(const ActiveSound &sound, uint16_t track) const;
    uint16_t normalizeTrackIndex(const ActiveSound &sound, uint16_t requestedTrack) const;
    void setAbsoluteTick(ActiveSound *sound, uint32_t absoluteTickValue);
    bool jumpTo(ActiveSound *sound, uint16_t track, uint16_t beat, uint16_t tick);
    int scanTo(ActiveSound *sound, uint16_t track, uint16_t beat, uint16_t tick);
    bool setLoop(ActiveSound *sound, uint16_t count, uint16_t toBeat, uint16_t toTick, uint16_t fromBeat, uint16_t fromTick);
    bool findNextControlTick(const ActiveSound &sound, uint16_t track, uint32_t afterTick, uint32_t *tick) const;
    std::vector<ImuseControlEvent> collectControlEvents(const ActiveSound &sound, uint16_t track, uint32_t tick) const;
    bool findNextTrackEventTick(const ActiveSound &sound, uint16_t track, uint32_t afterTick, uint32_t *tick) const;
    void resetMidiState(ActiveSound *sound);
    void clearHangingNotes(ActiveSound *sound);
    void cancelHangingNote(ActiveSound *sound, uint8_t channel, uint8_t note);
    void emitMidiMessage(uint16_t soundId, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2);
    void trackMidiMessage(ActiveSound *sound, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2);
    void flushActiveMidiState(uint16_t soundId);
    PartState *getActivePart(ActiveSound *sound, uint8_t channel);
    PartState *getPart(ActiveSound *sound, uint8_t channel);
    void initializePart(ActiveSound *sound, PartState *part, uint8_t channel);
    void clearPartActiveNotes(PartState *part);
    uint8_t computeEffectivePartVolume(const ActiveSound &sound, const PartState &part) const;
    int8_t computeEffectivePartPan(const ActiveSound &sound, const PartState &part) const;
    int8_t computeEffectivePartTranspose(const ActiveSound &sound, const PartState &part) const;
    uint8_t clampOutputNote(int note) const;
    bool clearToTransmit(ActiveSound *sound, PartState *part);
    void removeSuspendedPart(PartState *part);
    void suspendPart(PartState *part);
    int allocateOutputChannel(uint8_t priority);
    bool reassignChannelAndResumePart(uint8_t outputChannel);
    void reallocateMidiChannels();
    void releaseOutputChannel(PartState *part);
    uint8_t outputChannelForPart(const PartState &part) const;
    void emitPartMidiMessage(ActiveSound *sound, PartState *part, uint8_t statusClass, uint8_t data1, bool hasData2, uint8_t data2);
    void sendPartTranspose(ActiveSound *sound, PartState *part);
    void sendPartDetune(ActiveSound *sound, PartState *part);
    void sendPartPanPosition(ActiveSound *sound, PartState *part, uint8_t value);
    void sendPartEffectLevel(ActiveSound *sound, PartState *part, uint8_t value);
    void sendPartPolyphony(ActiveSound *sound, PartState *part);
    void applyPartPitchBendFactor(ActiveSound *sound, PartState *part);
    void applyPartPitchBend(ActiveSound *sound, PartState *part);
    void applyPartVolume(ActiveSound *sound, PartState *part);
    void applyPartPan(ActiveSound *sound, PartState *part);
    void applyPartModwheel(ActiveSound *sound, PartState *part);
    void applyPartSustain(ActiveSound *sound, PartState *part);
    void applyPartEffectLevel(ActiveSound *sound, PartState *part);
    void applyPartChorus(ActiveSound *sound, PartState *part);
    void applyPartPolyphony(ActiveSound *sound, PartState *part);
    void applyPartProgram(ActiveSound *sound, PartState *part);
    void applyPartAllState(ActiveSound *sound, PartState *part);
    void partAllNotesOff(ActiveSound *sound, PartState *part);
    void partOff(ActiveSound *sound, PartState *part);
    void partSetOnOff(ActiveSound *sound, uint8_t channel, bool on);
    void partSetVolume(ActiveSound *sound, uint8_t channel, uint8_t volume);
    void partSetPan(ActiveSound *sound, uint8_t channel, int8_t pan);
    void partSetPriority(ActiveSound *sound, uint8_t channel, int8_t priority);
    void partSetTranspose(ActiveSound *sound, uint8_t channel, int8_t transpose);
    void partSetProgram(ActiveSound *sound, uint8_t channel, uint8_t program);
    void partSetPitchBend(ActiveSound *sound, uint8_t channel, int16_t value);
    void partSetPitchBendFactor(ActiveSound *sound, uint8_t channel, uint8_t value);
    void partSetPolyphony(ActiveSound *sound, uint8_t channel, uint8_t value);
    void partSetDetune(ActiveSound *sound, uint8_t channel, int16_t value);
    void partSetModwheel(ActiveSound *sound, uint8_t channel, uint8_t value);
    void partSetSustain(ActiveSound *sound, uint8_t channel, bool on);
    void partSetEffectLevel(ActiveSound *sound, uint8_t channel, uint8_t value);
    void partSetChorus(ActiveSound *sound, uint8_t channel, uint8_t value);
    void partSetInstrument(ActiveSound *sound, uint8_t channel, uint16_t value);
    void partNoteOn(ActiveSound *sound, uint8_t channel, uint8_t sourceNote, uint8_t velocity);
    void partNoteOff(ActiveSound *sound, uint8_t channel, uint8_t sourceNote);
    void turnOffParts(ActiveSound *sound);
    void replayScanActiveNotes(ActiveSound *sound);
    bool dispatchChannelEvent(ActiveSound *sound, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2);
    void releaseSustainPedals(ActiveSound *sound);
    void scheduleSmartJumpNotes(ActiveSound *sound);
    bool findNextHangingTick(const ActiveSound &sound, uint32_t *tickDelta) const;
    void advanceHangingNotes(ActiveSound *sound, uint32_t deltaTicks);
    void emitExpiredHangingNotes(ActiveSound *sound);
    bool isScanActiveFor(uint16_t soundId) const;
    void beginScan(uint16_t soundId);
    void endScan(ActiveSound *sound);
    void trackScanNote(uint8_t status, uint8_t data1, bool hasData2, uint8_t data2);
    bool handleMidiEvent(uint16_t soundId, const MidiEvent &event);
    bool dispatchTrackTick(uint16_t soundId);
    bool executeControlEvent(uint16_t soundId, const ImuseControlEvent &event);
    bool maybeJump(ActiveSound *sound, uint8_t hookCommand, uint16_t track, uint16_t beat, uint16_t tick);
    int handleMarker(uint16_t soundId, uint8_t marker);
    int enqueueTrigger(uint16_t soundId, uint8_t marker);
    int enqueueCommand(uint16_t argc, const int16_t *args);
    int clearQueue();
    int queryQueue(uint16_t param) const;
    int setScriptTrigger(uint16_t soundId, uint8_t marker, const CommandPacket &command);
    int clearScriptTrigger(int soundId, int marker);
    void addDeferredCommand(uint32_t ticksLeft, const CommandPacket &command);
    void processDeferredCommands(uint32_t deltaTicks);
    int dispatchGlobalCommand(uint8_t cmd, uint16_t argc, const int16_t *args);
    int dispatchPlayerCommand(uint8_t cmd, uint16_t argc, const int16_t *args);

    const ResourceBank *_bank = nullptr;
    TargetProfile _profile = TargetProfile::GeneralMidi;
    uint16_t _masterVolume = 255;
    std::array<uint8_t, 8> _channelVolume = {{127, 127, 127, 127, 127, 127, 127, 127}};
    std::array<uint16_t, 8> _volchanTable = {{127, 127, 127, 127, 127, 127, 127, 127}};
    std::map<uint16_t, ActiveSound> _activeSounds;
    std::deque<QueuedTrigger> _triggerQueue;
    std::vector<ScriptTrigger> _scriptTriggers;
    std::vector<DeferredCommand> _deferredCommands;
    uint16_t _scriptTriggerEpoch = 0;
    MidiSink *_midiSink = nullptr;
    ScanContext _scan;
    CompatibilityProfile _compatibility = CompatibilityProfile::GenericV6;
    bool _nativeMt32Output = false;
    RhythmState _rhyState;
    std::vector<PartState *> _waitingPartsQueue;
    std::array<PartState *, 16> _physicalChannelOwners = {{}};
};

} // namespace imuse

#endif
