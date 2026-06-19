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

#include "imuse/ImuseEngine.h"

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <chrono>

#include "imuse/SinkMidiChannel.h"

namespace imuse {
namespace {

constexpr int kDefaultVolChan = 0xFFFF;
constexpr uint16_t kInvalidTrackIndex = 0xFFFFu;

int ClampInt(int value, int minValue, int maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

int TransposeClamp(int value, int minValue, int maxValue) {
    if (minValue > value) {
        value += (minValue - value + 11) / 12 * 12;
    }
    if (maxValue < value) {
        value -= (value - maxValue + 11) / 12 * 12;
    }
    return value;
}

bool IsPartChannel(int value) {
    return value >= 0 && value <= 15;
}

CommandPacket MakeCommandPacket(uint16_t argc, const int16_t *args, uint16_t startIndex) {
    CommandPacket packet;
    if (!args || argc <= startIndex) {
        return packet;
    }

    const uint16_t copyCount = std::min<uint16_t>(static_cast<uint16_t>(packet.args.size()), argc - startIndex);
    packet.argc = copyCount;
    for (uint16_t i = 0; i < copyCount; ++i) {
        packet.args[i] = args[startIndex + i];
    }
    return packet;
}

bool DebugMidiEnabled() {
    static const bool enabled = std::getenv("IMUSE_DEBUG_MIDI") != nullptr;
    return enabled;
}

void DebugMidiPrintf(const char *fmt, ...) {
    if (!DebugMidiEnabled()) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    std::fprintf(stderr, "\n");
    va_end(args);
}

} // namespace

void ImuseEngine::HookState::reset() {
    jump = {{0xFF, 0xFF}};
    transpose = 0xFF;
    partOnOff.fill(0xFF);
    partVolume.fill(0xFF);
    partProgram.fill(0xFF);
    partTranspose.fill(0xFF);
}

void ImuseEngine::MidiChannelState::reset() {
    heldNotes.fill(0);
    soundingNotes.fill(0);
    sustainValue = 0;
}

bool ImuseEngine::MidiChannelState::hasActiveNotes() const {
    for (uint8_t value : soundingNotes) {
        if (value != 0) {
            return true;
        }
    }
    return false;
}

int ImuseEngine::HookState::queryParam(int param, uint8_t chan) const {
    switch (param) {
    case 18:
        return jump[0];
    case 19:
        return transpose;
    case 20:
        return chan < partOnOff.size() ? partOnOff[chan] : -1;
    case 21:
        return chan < partVolume.size() ? partVolume[chan] : -1;
    case 22:
        return chan < partProgram.size() ? partProgram[chan] : -1;
    case 23:
        return chan < partTranspose.size() ? partTranspose[chan] : -1;
    default:
        return -1;
    }
}

int ImuseEngine::HookState::set(uint8_t cls, uint8_t value, uint8_t chan) {
    switch (cls) {
    case 0:
        if (value != jump[0]) {
            jump[1] = jump[0];
            jump[0] = value;
        }
        return 0;
    case 1:
        transpose = value;
        return 0;
    case 2:
        if (chan < partOnOff.size()) {
            partOnOff[chan] = value;
            return 0;
        }
        if (chan == 16) {
            partOnOff.fill(value);
            return 0;
        }
        return -1;
    case 3:
        if (chan < partVolume.size()) {
            partVolume[chan] = value;
            return 0;
        }
        if (chan == 16) {
            partVolume.fill(value);
            return 0;
        }
        return -1;
    case 4:
        if (chan < partProgram.size()) {
            partProgram[chan] = value;
            return 0;
        }
        if (chan == 16) {
            partProgram.fill(value);
            return 0;
        }
        return -1;
    case 5:
        if (chan < partTranspose.size()) {
            partTranspose[chan] = value;
            return 0;
        }
        if (chan == 16) {
            partTranspose.fill(value);
            return 0;
        }
        return -1;
    default:
        return -1;
    }
}

ImuseEngine::ActiveSound *ImuseEngine::findActiveSound(uint16_t soundId) {
    auto it = _activeSounds.find(soundId);
    return it != _activeSounds.end() ? &it->second : nullptr;
}

const ImuseEngine::ActiveSound *ImuseEngine::findActiveSound(uint16_t soundId) const {
    auto it = _activeSounds.find(soundId);
    return it != _activeSounds.end() ? &it->second : nullptr;
}

uint8_t ImuseEngine::getChannelVolume(uint16_t volChan) const {
    if (volChan < _channelVolume.size()) {
        return _channelVolume[volChan];
    }
    return static_cast<uint8_t>(_masterVolume / 2);
}

void ImuseEngine::refreshEffectiveVolume(ActiveSound *sound) {
    if (!sound) {
        return;
    }
    const int channelVolume = getChannelVolume(sound->volChan);
    sound->effectiveVolume = static_cast<uint8_t>((channelVolume * (ClampInt(sound->volume, 0, 127) + 1)) >> 7);
}

int ImuseEngine::queryPartParam(const ActiveSound &sound, int param, uint8_t chan) const {
    if (chan >= sound.parts.size()) {
        return 129;
    }

    const PartState &part = sound.parts[chan];
    if (!part.present) {
        return 129;
    }

    switch (param) {
    case 14:
        return part.on ? 1 : 0;
    case 15:
        return part.volume;
    case 16:
        return 129;
    case 17:
        return static_cast<uint8_t>(part.transpose);
    default:
        return -1;
    }
}

int ImuseEngine::getSoundParam(const ActiveSound &sound, int param, uint8_t chan) const {
    switch (param) {
    case 0:
        return sound.priority;
    case 1:
        return sound.volume;
    case 2:
        return static_cast<uint8_t>(sound.pan);
    case 3:
        return static_cast<uint8_t>(sound.transpose);
    case 4:
        return static_cast<uint8_t>(sound.detune);
    case 5:
        return sound.speed;
    case 6:
        return sound.trackIndex;
    case 7:
        return sound.beatIndex;
    case 8:
        return sound.tickIndex;
    case 9:
        return sound.loopCounter;
    case 10:
        return sound.loopToBeat;
    case 11:
        return sound.loopToTick;
    case 12:
        return sound.loopFromBeat;
    case 13:
        return sound.loopFromTick;
    case 14:
    case 15:
    case 16:
    case 17:
        return queryPartParam(sound, param, chan);
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
        return sound.hook.queryParam(param, chan);
    default:
        return -1;
    }
}

int ImuseEngine::setSoundTranspose(ActiveSound *sound, uint8_t relative, int value) {
    if (!sound || value < -24 || value > 24 || relative > 1) {
        return -1;
    }

    if (relative) {
        value = TransposeClamp(sound->transpose + value, -7, 7);
    }

    sound->transpose = static_cast<int8_t>(value);
    for (std::size_t channelIndex = 0; channelIndex < sound->parts.size(); ++channelIndex) {
        if (PartState *part = getActivePart(sound, static_cast<uint8_t>(channelIndex))) {
            part->transposeEffective = computeEffectivePartTranspose(*sound, *part);
        }
    }
    return 0;
}

uint32_t ImuseEngine::ticksPerBeat(const ActiveSound &sound) const {
    return sound.sequence.smf.ppqn() ? sound.sequence.smf.ppqn() : 480u;
}

uint32_t ImuseEngine::absoluteTick(const ActiveSound &sound) const {
    return (static_cast<uint32_t>(sound.beatIndex > 0 ? sound.beatIndex - 1 : 0) * ticksPerBeat(sound)) + sound.tickIndex;
}

uint32_t ImuseEngine::trackEndTick(const ActiveSound &sound, uint16_t track) const {
    if (track >= sound.sequence.smf.tracks.size()) {
        return 0;
    }
    const SmfTrack &smfTrack = sound.sequence.smf.tracks[track];
    return smfTrack.events.empty() ? 0 : smfTrack.events.back().tick;
}

uint16_t ImuseEngine::normalizeTrackIndex(const ActiveSound &sound, uint16_t requestedTrack) const {
    const std::size_t trackCount = sound.sequence.smf.tracks.size();
    if (trackCount == 0) {
        return kInvalidTrackIndex;
    }

    if (requestedTrack < trackCount) {
        return requestedTrack;
    }

    // Some authoring files and old iMUSE-style commands use 1-based track numbers.
    if (requestedTrack > 0 && static_cast<std::size_t>(requestedTrack - 1) < trackCount) {
        return static_cast<uint16_t>(requestedTrack - 1);
    }

    return kInvalidTrackIndex;
}

void ImuseEngine::setAbsoluteTick(ActiveSound *sound, uint32_t absoluteTickValue) {
    if (!sound) {
        return;
    }
    const uint32_t tpb = ticksPerBeat(*sound);
    sound->beatIndex = static_cast<uint16_t>((absoluteTickValue / tpb) + 1);
    sound->tickIndex = static_cast<uint16_t>(absoluteTickValue % tpb);
}

bool ImuseEngine::jumpTo(ActiveSound *sound, uint16_t track, uint16_t beat, uint16_t tick) {
    if (!sound || beat == 0) {
        return false;
    }

    const uint16_t normalizedTrack = normalizeTrackIndex(*sound, track);
    if (normalizedTrack == kInvalidTrackIndex) {
        return false;
    }

    sound->trackIndex = normalizedTrack;
    sound->beatIndex = beat;
    sound->tickIndex = tick;
    sound->pendingImmediateEvents = true;
    return true;
}

int ImuseEngine::scanTo(ActiveSound *sound, uint16_t track, uint16_t beat, uint16_t tick) {
    if (!sound) {
        return -1;
    }

    const uint16_t normalizedTrack = normalizeTrackIndex(*sound, track);
    if (normalizedTrack == kInvalidTrackIndex) {
        return -1;
    }

    if (beat == 0) {
        beat = 1;
    }

    const uint16_t soundId = sound->soundId;
    const uint16_t originalTrack = sound->trackIndex;
    const uint32_t targetAbsoluteTick = (static_cast<uint32_t>(beat - 1) * ticksPerBeat(*sound)) + tick;
    const ActiveSound originalState = *sound;

    turnOffParts(sound);
    resetMidiState(sound);
    beginScan(soundId);

    bool ok = true;
    if (originalTrack != normalizedTrack) {
        const uint32_t currentAbsoluteTick = absoluteTick(*sound);
        const uint32_t currentEndTick = trackEndTick(*sound, originalTrack);
        if (currentEndTick > currentAbsoluteTick) {
            ok = advanceSound(soundId, currentEndTick - currentAbsoluteTick);
        }

        sound = findActiveSound(soundId);
        if (!ok || !sound) {
            _scan.reset();
            _activeSounds[soundId] = originalState;
            return -1;
        }

        sound->trackIndex = normalizedTrack;
        sound->beatIndex = 1;
        sound->tickIndex = 0;
        sound->pendingImmediateEvents = true;
        sound->loopCounter = 0;
    } else {
        sound->trackIndex = normalizedTrack;
        sound->beatIndex = 1;
        sound->tickIndex = 0;
        sound->pendingImmediateEvents = true;
    }

    ok = advanceSound(soundId, targetAbsoluteTick);
    sound = findActiveSound(soundId);
    if (!ok || !sound) {
        _scan.reset();
        _activeSounds[soundId] = originalState;
        return -1;
    }

    endScan(sound);
    if (originalTrack != normalizedTrack) {
        sound->loopCounter = 0;
    }
    return 0;
}

bool ImuseEngine::setLoop(ActiveSound *sound, uint16_t count, uint16_t toBeat, uint16_t toTick, uint16_t fromBeat, uint16_t fromTick) {
    if (!sound || toBeat + 1 >= fromBeat) {
        return false;
    }

    if (toBeat == 0) {
        toBeat = 1;
    }

    sound->loopCounter = 0;
    sound->loopToBeat = toBeat;
    sound->loopToTick = toTick;
    sound->loopFromBeat = fromBeat;
    sound->loopFromTick = fromTick;
    sound->loopCounter = count;
    return true;
}

bool ImuseEngine::findNextControlTick(const ActiveSound &sound, uint16_t track, uint32_t afterTick, uint32_t *tick) const {
    bool found = false;
    uint32_t best = 0;

    for (const ImuseControlEvent &event : sound.sequence.controlEvents) {
        if (event.trackIndex != track || event.tick <= afterTick) {
            continue;
        }
        if (!found || event.tick < best) {
            found = true;
            best = event.tick;
        }
    }

    if (found && tick) {
        *tick = best;
    }
    return found;
}

std::vector<ImuseControlEvent> ImuseEngine::collectControlEvents(const ActiveSound &sound, uint16_t track, uint32_t tick) const {
    std::vector<ImuseControlEvent> events;
    for (const ImuseControlEvent &event : sound.sequence.controlEvents) {
        if (event.trackIndex == track && event.tick == tick) {
            events.push_back(event);
        }
    }
    return events;
}

bool ImuseEngine::findNextTrackEventTick(const ActiveSound &sound, uint16_t track, uint32_t afterTick, uint32_t *tick) const {
    if (track >= sound.sequence.smf.tracks.size()) {
        return false;
    }

    bool found = false;
    uint32_t best = 0;
    const SmfTrack &smfTrack = sound.sequence.smf.tracks[track];
    for (const MidiEvent &event : smfTrack.events) {
        if (event.tick <= afterTick) {
            continue;
        }
        if (!found || event.tick < best) {
            best = event.tick;
            found = true;
        }
    }

    if (found && tick) {
        *tick = best;
    }
    return found;
}

void ImuseEngine::resetMidiState(ActiveSound *sound) {
    if (!sound) {
        return;
    }

    for (MidiChannelState &channel : sound->midiChannels) {
        channel.reset();
    }
    clearHangingNotes(sound);
}

void ImuseEngine::clearHangingNotes(ActiveSound *sound) {
    if (!sound) {
        return;
    }
    sound->hangingNotes.clear();
}

void ImuseEngine::cancelHangingNote(ActiveSound *sound, uint8_t channel, uint8_t note) {
    if (!sound || sound->hangingNotes.empty()) {
        return;
    }

    auto best = sound->hangingNotes.end();
    for (auto it = sound->hangingNotes.begin(); it != sound->hangingNotes.end(); ++it) {
        if (it->channel != channel || it->note != note) {
            continue;
        }
        if (best == sound->hangingNotes.end() || it->ticksLeft < best->ticksLeft) {
            best = it;
        }
    }

    if (best != sound->hangingNotes.end()) {
        sound->hangingNotes.erase(best);
    }
}

void ImuseEngine::emitMidiMessage(uint16_t soundId, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
    if (_midiSink) {
        _midiSink->onMidiMessage(soundId, status, data1, hasData2, data2);
    }

    if (ActiveSound *sound = findActiveSound(soundId)) {
        trackMidiMessage(sound, status, data1, hasData2, data2);
    }
}

ImuseEngine::PartState *ImuseEngine::getActivePart(ActiveSound *sound, uint8_t channel) {
    if (!sound || channel >= sound->parts.size()) {
        return nullptr;
    }

    PartState &part = sound->parts[channel];
    return (part.initialized && part.present) ? &part : nullptr;
}

ImuseEngine::PartState *ImuseEngine::getPart(ActiveSound *sound, uint8_t channel) {
    if (!sound || channel >= sound->parts.size()) {
        return nullptr;
    }

    PartState &part = sound->parts[channel];
    if (!part.initialized) {
        initializePart(sound, &part, channel);
    }

    return &part;
}

void ImuseEngine::initializePart(ActiveSound *sound, PartState *part, uint8_t channel) {
    if (!sound || !part) {
        return;
    }

    removeSuspendedPart(part);
    releaseOutputChannel(part);

    *part = {};
    part->initialized = true;
    part->present = true;
    part->transmitted = false;
    part->on = true;
    part->percussion = sound->supportsPercussion && channel == 9;
    part->unassignedInstrument = true;
    part->channel = channel;
    part->ownerSoundId = sound->soundId;
    part->volume = 127;
    part->effectiveVolume = sound->effectiveVolume;
    part->program = 0;
    part->instrumentValue = 0;
    part->pitchBendFactor = 2;
    part->volControlSensitivity = 127;
    part->transposeEffective = static_cast<int8_t>(ClampInt(sound->transpose, -24, 24));
    part->detuneEffective = sound->detune;
    part->panEffective = static_cast<int8_t>(ClampInt(sound->pan, -64, 63));
    part->polyphony = 1;
    part->priorityEffective = sound->priority;
    part->effectLevel = _nativeMt32Output ? 127 : 64;
    part->instrument.setNativeMT32Mode(_nativeMt32Output);
    clearPartActiveNotes(part);
}

void ImuseEngine::clearPartActiveNotes(PartState *part) {
    if (!part) {
        return;
    }

    part->sourceNotes.fill(0);
    part->sourceOutputNotes.fill(0);
    part->sourceOutputChannels.fill(0);
    part->sourceVelocities.fill(0);
}

uint8_t ImuseEngine::computeEffectivePartVolume(const ActiveSound &sound, const PartState &part) const {
    uint16_t vol = static_cast<uint16_t>((part.volume + 1) * sound.effectiveVolume);
    vol = static_cast<uint16_t>((vol * (part.volControlSensitivity + 1)) >> 7);
    return static_cast<uint8_t>(ClampInt(vol >> 7, 0, 127));
}

int8_t ImuseEngine::computeEffectivePartPan(const ActiveSound &sound, const PartState &part) const {
    return static_cast<int8_t>(ClampInt(part.pan + sound.pan, -64, 63));
}

int8_t ImuseEngine::computeEffectivePartTranspose(const ActiveSound &sound, const PartState &part) const {
    return static_cast<int8_t>(ClampInt(TransposeClamp(part.transpose + sound.transpose, -24, 24), -24, 24));
}

uint8_t ImuseEngine::clampOutputNote(int note) const {
    return static_cast<uint8_t>(ClampInt(note, 0, 127));
}

bool ImuseEngine::clearToTransmit(ActiveSound *sound, PartState *part) {
    if (!sound || !part || !part->on) {
        return false;
    }

    if (part->percussion) {
        return false;
    }

    if (part->outputChannel >= 0) {
        return true;
    }

    if (part->instrument.isValid()) {
        reallocateMidiChannels();
    }

    return part->outputChannel >= 0;
}

void ImuseEngine::removeSuspendedPart(PartState *part) {
    if (!part) {
        return;
    }

    _waitingPartsQueue.erase(std::remove(_waitingPartsQueue.begin(), _waitingPartsQueue.end(), part), _waitingPartsQueue.end());
}

void ImuseEngine::suspendPart(PartState *part) {
    if (!part) {
        return;
    }

    removeSuspendedPart(part);
    auto it = _waitingPartsQueue.begin();
    while (it != _waitingPartsQueue.end() && (*it)->priorityEffective > part->priorityEffective) {
        ++it;
    }
    _waitingPartsQueue.insert(it, part);
}

int ImuseEngine::allocateOutputChannel(uint8_t priority) {
    // For MT-32: ScummVM uses channels 1-9 (PROP_CHANNEL_MASK=0x03FE), never channel 0.
    // MT-32 Parts 1-8 → channels 1-8, Rhythm → channel 9.
    const uint8_t firstChannel = (_profile == TargetProfile::Mt32) ? 1 : 0;
    const uint8_t lastChannel = (_profile == TargetProfile::Mt32) ? 8 : 15;
    for (uint8_t channel = firstChannel; channel <= lastChannel; ++channel) {
        if (channel == 9) {
            continue;
        }
        if (_physicalChannelOwners[channel] == nullptr) {
            return channel;
        }
    }

    PartState *best = nullptr;
    uint8_t bestChannel = 0xFF;
    uint8_t bestPriority = priority;
    for (uint8_t channel = firstChannel; channel <= lastChannel; ++channel) {
        if (channel == 9) {
            continue;
        }

        PartState *candidate = _physicalChannelOwners[channel];
        if (!candidate || candidate->percussion || candidate->priorityEffective >= bestPriority) {
            continue;
        }

        best = candidate;
        bestChannel = channel;
        bestPriority = candidate->priorityEffective;
    }

    if (!best) {
        return -1;
    }

    if (_logCallback) {
        _logCallback("iMUSE: Channel dispute! Part (sound=" + std::to_string(best->ownerSoundId) + 
                     ", channel=" + std::to_string(best->channel) + 
                     ", priority=" + std::to_string(best->priorityEffective) + 
                     ") is being stolen from physical channel " + std::to_string(bestChannel) + 
                     " by part with priority " + std::to_string(priority));
    }

    if (ActiveSound *owner = findActiveSound(best->ownerSoundId)) {
        if (best->pedal) {
            best->pedal = false;
            emitPartMidiMessage(owner, best, 0xB0, 64, true, 0);
        }
        emitPartMidiMessage(owner, best, 0xB0, 123, true, 0);
    }

    _physicalChannelOwners[bestChannel] = nullptr;
    best->outputChannel = -1;
    best->transmitted = false;
    clearPartActiveNotes(best);
    suspendPart(best);
    return bestChannel;
}

bool ImuseEngine::reassignChannelAndResumePart(uint8_t outputChannel) {
    while (!_waitingPartsQueue.empty()) {
        PartState *part = _waitingPartsQueue.front();
        _waitingPartsQueue.erase(_waitingPartsQueue.begin());
        if (!part || !part->present || !part->initialized || !part->on) {
            continue;
        }

        ActiveSound *sound = findActiveSound(part->ownerSoundId);
        if (!sound) {
            continue;
        }

        part->outputChannel = static_cast<int8_t>(outputChannel);
        part->transmitted = false;
        _physicalChannelOwners[outputChannel] = part;
        applyPartAllState(sound, part);
        return true;
    }

    return false;
}

void ImuseEngine::reallocateMidiChannels() {
    while (true) {
        PartState *best = nullptr;
        ActiveSound *bestSound = nullptr;
        uint8_t bestPriority = 0;

        for (auto &entry : _activeSounds) {
            ActiveSound &sound = entry.second;
            for (PartState &part : sound.parts) {
                if (!part.present || !part.initialized || part.percussion || !part.on || part.outputChannel >= 0) {
                    continue;
                }
                if (!part.instrument.isValid()) {
                    continue;
                }
                if (!best || part.priorityEffective >= bestPriority) {
                    best = &part;
                    bestSound = &sound;
                    bestPriority = part.priorityEffective;
                }
            }
        }

        if (!best || !bestSound) {
            return;
        }

        const int outputChannel = allocateOutputChannel(bestPriority);
        if (outputChannel < 0) {
            if (_logCallback) {
                _logCallback("iMUSE Warning: No physical channel available for Part (sound=" + std::to_string(bestSound->soundId) + 
                             ", channel=" + std::to_string(best->channel) + 
                             ", priority=" + std::to_string(best->priorityEffective) + ")");
            }
            return;
        }

        if (_logCallback) {
            _logCallback("iMUSE: Allocated physical channel " + std::to_string(outputChannel) + 
                         " to Part (sound=" + std::to_string(bestSound->soundId) + 
                         ", channel=" + std::to_string(best->channel) + 
                         ", priority=" + std::to_string(best->priorityEffective) + ")");
        }

        best->outputChannel = static_cast<int8_t>(outputChannel);
        best->transmitted = false;
        _physicalChannelOwners[outputChannel] = best;
        applyPartAllState(bestSound, best);
    }
}

void ImuseEngine::releaseOutputChannel(PartState *part) {
    if (!part || part->outputChannel < 0) {
        return;
    }

    const uint8_t outputChannel = static_cast<uint8_t>(part->outputChannel);
    if (_physicalChannelOwners[outputChannel] == part) {
        _physicalChannelOwners[outputChannel] = nullptr;
    }
    part->outputChannel = -1;
    part->transmitted = false;
}

uint8_t ImuseEngine::outputChannelForPart(const PartState &part) const {
    return part.percussion ? 9 : static_cast<uint8_t>(part.outputChannel);
}

void ImuseEngine::emitPartMidiMessage(ActiveSound *sound, PartState *part, uint8_t statusClass, uint8_t data1, bool hasData2, uint8_t data2) {
    if (!sound || !part) {
        return;
    }

    const uint8_t outputChannel = outputChannelForPart(*part);
    const uint8_t status = static_cast<uint8_t>((statusClass & 0xF0) | (outputChannel & 0x0F));
    emitMidiMessage(sound->soundId, status, data1, hasData2, data2);
}

void ImuseEngine::sendPartTranspose(ActiveSound *sound, PartState *part) {
    if (!sound || !part || part->percussion || part->outputChannel < 0 || !_midiSink) {
        return;
    }

    SinkMidiChannel channel(_midiSink, sound->soundId, static_cast<uint8_t>(part->outputChannel));
    channel.transpose(part->transposeEffective);
}

void ImuseEngine::sendPartDetune(ActiveSound *sound, PartState *part) {
    if (!sound || !part || part->percussion || part->outputChannel < 0 || !_midiSink) {
        return;
    }

    SinkMidiChannel channel(_midiSink, sound->soundId, static_cast<uint8_t>(part->outputChannel));
    channel.detune(part->detuneEffective);
}

void ImuseEngine::sendPartPanPosition(ActiveSound *sound, PartState *part, uint8_t value) {
    if (!sound || !part || part->percussion || part->outputChannel < 0 || !_midiSink) {
        return;
    }

    if (_nativeMt32Output) {
        value = static_cast<uint8_t>(127 - value);
    }

    SinkMidiChannel channel(_midiSink, sound->soundId, static_cast<uint8_t>(part->outputChannel));
    channel.panPosition(value);
}

void ImuseEngine::sendPartEffectLevel(ActiveSound *sound, PartState *part, uint8_t value) {
    if (!sound || !part || part->percussion || part->outputChannel < 0 || !_midiSink) {
        return;
    }

    SinkMidiChannel channel(_midiSink, sound->soundId, static_cast<uint8_t>(part->outputChannel));
    channel.effectLevel(value);
}

void ImuseEngine::sendPartPolyphony(ActiveSound *sound, PartState *part) {
    if (!sound || !part || part->percussion || part->outputChannel < 0 || !_midiSink) {
        return;
    }

    SinkMidiChannel channel(_midiSink, sound->soundId, static_cast<uint8_t>(part->outputChannel));
    channel.controlChange(17, part->polyphony);
}

void ImuseEngine::applyPartPitchBendFactor(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    if (part->percussion || !clearToTransmit(sound, part) || !_midiSink) {
        return;
    }

    SinkMidiChannel channel(_midiSink, sound->soundId, static_cast<uint8_t>(part->outputChannel));
    channel.pitchBendFactor(static_cast<uint8_t>(ClampInt(part->pitchBendFactor, 0, 12)));
    part->transmitted = true;
}

void ImuseEngine::applyPartPitchBend(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    if (part->percussion || !clearToTransmit(sound, part)) {
        return;
    }

    if (_compatibility == CompatibilityProfile::SamAndMax && part->pitchBendFactor == 0) {
        const int16_t fadeModifier = static_cast<int16_t>(((((part->pitchBend >= 0) ? 127 - part->volume : part->volume) + 1) * part->pitchBend) >> 7);
        const uint16_t vol = static_cast<uint16_t>((part->volume + fadeModifier + 1) * sound->effectiveVolume);
        part->effectiveVolume = static_cast<uint8_t>(ClampInt((vol * (part->volControlSensitivity + 1)) >> 14, 0, 127));
        emitPartMidiMessage(sound, part, 0xB0, 7, true, part->effectiveVolume);
        part->transmitted = true;
        return;
    }

    int bend = ClampInt(static_cast<int>(part->pitchBend) + 0x2000, 0, 0x3FFF);
    emitPartMidiMessage(sound,
                        part,
                        0xE0,
                        static_cast<uint8_t>(bend & 0x7F),
                        true,
                        static_cast<uint8_t>((bend >> 7) & 0x7F));
    part->transmitted = true;
}

void ImuseEngine::applyPartVolume(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    part->effectiveVolume = computeEffectivePartVolume(*sound, *part);
    if (part->percussion || !clearToTransmit(sound, part)) {
        return;
    }

    emitPartMidiMessage(sound, part, 0xB0, 7, true, part->effectiveVolume);
    part->transmitted = true;
}

void ImuseEngine::applyPartPan(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    part->panEffective = computeEffectivePartPan(*sound, *part);
    if (part->percussion || !clearToTransmit(sound, part)) {
        return;
    }

    sendPartPanPosition(sound, part, static_cast<uint8_t>(ClampInt(part->panEffective + 0x40, 0, 127)));
    part->transmitted = true;
}

void ImuseEngine::applyPartModwheel(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    if (part->percussion || !clearToTransmit(sound, part)) {
        return;
    }

    emitPartMidiMessage(sound, part, 0xB0, 1, true, part->modwheel);
    part->transmitted = true;
}

void ImuseEngine::applyPartSustain(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    if (part->percussion || !clearToTransmit(sound, part)) {
        return;
    }

    emitPartMidiMessage(sound, part, 0xB0, 64, true, part->pedal ? 1 : 0);
    part->transmitted = true;
}

void ImuseEngine::applyPartEffectLevel(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    if (part->percussion || !clearToTransmit(sound, part)) {
        return;
    }

    sendPartEffectLevel(sound, part, part->effectLevel);
    part->transmitted = true;
}

void ImuseEngine::applyPartChorus(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    if (part->percussion || !clearToTransmit(sound, part)) {
        return;
    }

    emitPartMidiMessage(sound, part, 0xB0, 93, true, part->chorus);
    part->transmitted = true;
}

void ImuseEngine::applyPartPolyphony(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    if (_compatibility != CompatibilityProfile::SamAndMax || part->percussion || !clearToTransmit(sound, part)) {
        return;
    }

    sendPartPolyphony(sound, part);
    part->transmitted = true;
}

void ImuseEngine::applyPartProgram(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    if (part->percussion || !clearToTransmit(sound, part) || !_midiSink) {
        return;
    }

    DebugMidiPrintf("part program sound=%u ch=%u bank=%u prog=%u", sound->soundId, part->channel, part->bank, part->program);
    SinkMidiChannel channel(_midiSink, sound->soundId, static_cast<uint8_t>(part->outputChannel));
    if (part->instrument.isValid()) {
        if (_profile == TargetProfile::Mt32 && part->outputChannel >= 1 && part->outputChannel <= 8) {
            if (!part->customRolandSysex.empty()) {
                // Custom Roland instrument: sendPartCustomSysex handles everything.
                sendPartCustomSysex(sound->soundId, part);
            }
            // Standard factory instruments: instrument.send() handles PC via the normal path.
            // (SysEx patch update is done in partSetProgram.)
            else {
                part->instrument.send(&channel);
            }
        } else {
            part->instrument.send(&channel);
        }
    }
    part->transmitted = true;
}

void ImuseEngine::sendMt32Sysex(uint16_t soundId, uint32_t addr, const uint8_t* data, size_t size) {
    if (!_midiSink) return;
    
    std::vector<uint8_t> sysex;
    sysex.reserve(size + 10);
    sysex.push_back(0x41);
    sysex.push_back(0x10);
    sysex.push_back(0x16);
    sysex.push_back(0x12);
    
    sysex.push_back((addr >> 14) & 0x7F);
    sysex.push_back((addr >> 7) & 0x7F);
    sysex.push_back(addr & 0x7F);
    
    for (size_t i = 0; i < size; ++i) {
        sysex.push_back(data[i]);
    }
    
    uint8_t checkSum = 0;
    for (size_t i = 4; i < sysex.size(); ++i) {
        checkSum -= sysex[i];
    }
    sysex.push_back(checkSum & 0x7F);
    sysex.push_back(0xF7);
    
    _midiSink->onSysEx(soundId, ByteView(sysex.data(), sysex.size()));
}

void ImuseEngine::sendPartCustomSysex(uint16_t soundId, PartState* part) {
    if (!part || part->customRolandSysex.empty() || !_midiSink) {
        return;
    }
    
    if (_profile == TargetProfile::Mt32 && part->outputChannel >= 1 && part->outputChannel <= 8) {
        if (part->customRolandSysex.size() >= 254) {
            size_t offset = (part->customRolandSysex.back() == 0xF7) ? 1 : 0;
            const uint8_t* timbreData = part->customRolandSysex.data() + 7;
            size_t timbreSize = part->customRolandSysex.size() - 8 - offset;
            
            if (timbreSize == 246) {
                // 1. Write the Timbre Data to the Memory Timbre slot corresponding to this physical channel
                uint32_t sysexTimbreAddrBase = 0x20000 + (part->outputChannel << 8);
                sendMt32Sysex(soundId, sysexTimbreAddrBase, timbreData, timbreSize);
                
                // 2. Set the Patch Memory for this channel to use the Memory Timbre
                part->currentTimbreIndex = 0xFF;
                uint32_t sysexPatchAddrBase = 0x14000 + (part->outputChannel << 3);
                uint8_t patchData[2] = { 0x02, static_cast<uint8_t>(part->outputChannel) };
                sendMt32Sysex(soundId, sysexPatchAddrBase, patchData, sizeof(patchData));
                
                // Update instrument program to match output channel!
                part->instrumentValue = static_cast<uint8_t>(part->outputChannel);
                part->instrument.program(part->outputChannel, 0, true);
                
                // 3. Send a standard Program Change to refresh the MT-32 state
                // Use channel-1 as dummy PC to force MT-32 to reload Patch Temp from the modified slot.
                // Clamp to [1..8]: for ch 1 use dummy=2, for ch 2-8 use dummy=ch-1.
                SinkMidiChannel channel(_midiSink, soundId, static_cast<uint8_t>(part->outputChannel));
                const uint8_t dummyPc = (part->outputChannel == 1) ? 2 : static_cast<uint8_t>(part->outputChannel - 1);
                channel.programChange(dummyPc);
                channel.programChange(static_cast<uint8_t>(part->outputChannel));
            }
        }
    }
}


void ImuseEngine::applyPartAllState(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    if (part->percussion) {
        return;
    }

    part->transposeEffective = computeEffectivePartTranspose(*sound, *part);
    part->detuneEffective = static_cast<int16_t>(ClampInt(part->detune + sound->detune, -128, 127));
    part->priorityEffective = static_cast<uint8_t>(ClampInt(part->priority + sound->priority, 0, 255));
    if (!clearToTransmit(sound, part) || !_midiSink) {
        return;
    }

    SinkMidiChannel channel(_midiSink, sound->soundId, static_cast<uint8_t>(part->outputChannel));
    channel.pitchBendFactor(part->pitchBendFactor);
    sendPartTranspose(sound, part);
    sendPartDetune(sound, part);
    channel.pitchBend(part->pitchBend);
    part->effectiveVolume = computeEffectivePartVolume(*sound, *part);
    channel.volume(part->effectiveVolume);
    channel.sustain(part->pedal);
    channel.modulationWheel(part->modwheel);
    part->panEffective = computeEffectivePartPan(*sound, *part);
    sendPartPanPosition(sound, part, static_cast<uint8_t>(ClampInt(part->panEffective + 0x40, 0, 127)));
    sendPartPolyphony(sound, part);
    if (part->instrument.isValid()) {
        if (_profile == TargetProfile::Mt32 && part->outputChannel >= 1 && part->outputChannel <= 8) {
            if (part->customRolandSysex.empty()) {
                // Standard factory instrument: configure patch memory to point to the correct factory timbre.
                // sendPartCustomSysex is not called for these, so we handle it here.
                part->currentTimbreIndex = part->program;
                uint32_t sysexPatchAddrBase = 0x14000 + (part->outputChannel << 3);
                uint8_t patchData[2] = { static_cast<uint8_t>(part->program >> 6), static_cast<uint8_t>(part->program & 0x3F) };
                sendMt32Sysex(sound->soundId, sysexPatchAddrBase, patchData, sizeof(patchData));
                part->instrumentValue = static_cast<uint8_t>(part->outputChannel);
                part->instrument.program(part->outputChannel, 0, sound->isMt32);
                // Dummy PC: for ch 1 use 2, for ch 2-8 use ch-1, to safely force MT-32 Patch Temp reload.
                const uint8_t dummyPc = (part->outputChannel == 1) ? 2 : static_cast<uint8_t>(part->outputChannel - 1);
                channel.programChange(dummyPc);
                channel.programChange(static_cast<uint8_t>(part->outputChannel));
            }
            // For custom Roland instruments: skip instrument.send() entirely.
            // sendPartCustomSysex() (called below) handles timbre upload, patch memory, and program change.
            // Calling instrument.send() here would emit a spurious program change (64+am) that
            // overwrites the correct patch selection before sendPartCustomSysex has a chance to fix it.
        } else {
            // Non-MT-32 or non-melodic: use standard instrument send path.
            part->instrument.send(&channel);
        }
    }
    sendPartEffectLevel(sound, part, part->effectLevel);
    channel.chorusLevel(part->chorus);
    channel.priority(part->priorityEffective);
    sendPartCustomSysex(sound->soundId, part);
    part->transmitted = true;
}

void ImuseEngine::partAllNotesOff(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    if (part->percussion || part->outputChannel < 0) {
        return;
    }

    emitPartMidiMessage(sound, part, 0xB0, 123, true, 0);
    clearPartActiveNotes(part);
    sound->hangingNotes.erase(
        std::remove_if(sound->hangingNotes.begin(), sound->hangingNotes.end(),
                       [part](const HangingNote &note) { return note.channel == static_cast<uint8_t>(part->outputChannel); }),
        sound->hangingNotes.end());
}

void ImuseEngine::partOff(ActiveSound *sound, PartState *part) {
    if (!sound || !part) {
        return;
    }

    if (part->outputChannel < 0) {
        clearPartActiveNotes(part);
        return;
    }

    if (part->pedal) {
        part->pedal = false;
        emitPartMidiMessage(sound, part, 0xB0, 64, true, 0);
    }
    partAllNotesOff(sound, part);
    const uint8_t outputChannel = static_cast<uint8_t>(part->outputChannel);
    releaseOutputChannel(part);
    if (!reassignChannelAndResumePart(outputChannel)) {
        _physicalChannelOwners[outputChannel] = nullptr;
    }
}

void ImuseEngine::partSetOnOff(ActiveSound *sound, uint8_t channel, bool on) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    if (part->on != on) {
        part->on = on;
        if (!on) {
            partOff(sound, part);
        }
        if (!part->percussion) {
            reallocateMidiChannels();
        }
    }
}

void ImuseEngine::partSetVolume(ActiveSound *sound, uint8_t channel, uint8_t volume) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->volume = static_cast<uint8_t>(ClampInt(volume, 0, 127));
    applyPartVolume(sound, part);
}

void ImuseEngine::partSetPan(ActiveSound *sound, uint8_t channel, int8_t pan) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->pan = static_cast<int8_t>(ClampInt(pan, -64, 63));
    applyPartPan(sound, part);
}

void ImuseEngine::partSetPriority(ActiveSound *sound, uint8_t channel, int8_t priority) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->priority = priority;
    part->priorityEffective = static_cast<uint8_t>(ClampInt(part->priority + sound->priority, 0, 255));
    if (!part->percussion) {
        reallocateMidiChannels();
    }
}

void ImuseEngine::partSetTranspose(ActiveSound *sound, uint8_t channel, int8_t transpose) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->transpose = static_cast<int8_t>(ClampInt(transpose, -24, 24));
    part->transposeEffective = computeEffectivePartTranspose(*sound, *part);
    sendPartTranspose(sound, part);
}

void ImuseEngine::partSetProgram(ActiveSound *sound, uint8_t channel, uint8_t program) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->bank = 0;
    part->program = program;
    
    uint8_t instrumentProgram = program;
    if (_profile == TargetProfile::Mt32) {
        if (part->outputChannel >= 1 && part->outputChannel <= 8) {
            if (part->currentTimbreIndex != program) {
                part->currentTimbreIndex = program;
                uint32_t sysexPatchAddrBase = 0x14000 + (part->outputChannel << 3);
                uint8_t patchData[2] = { static_cast<uint8_t>(program >> 6), static_cast<uint8_t>(program & 0x3F) };
                sendMt32Sysex(sound->soundId, sysexPatchAddrBase, patchData, sizeof(patchData));
                
                // Force MT-32 to reload the patch if it's currently active on this channel
                SinkMidiChannel midiCh(_midiSink, sound->soundId, static_cast<uint8_t>(part->outputChannel));
                // Dummy PC: for ch 1 use 2, for ch 2-8 use ch-1, to safely force MT-32 Patch Temp reload.
                const uint8_t dummyPc = (part->outputChannel == 1) ? 2 : static_cast<uint8_t>(part->outputChannel - 1);
                midiCh.programChange(dummyPc);
                midiCh.programChange(static_cast<uint8_t>(part->outputChannel));
            }
            instrumentProgram = static_cast<uint8_t>(part->outputChannel);
        }
    }

    part->instrumentValue = instrumentProgram;
    part->instrument.program(instrumentProgram, 0, sound->isMt32);
    part->unassignedInstrument = false;
    applyPartProgram(sound, part);
}

void ImuseEngine::partSetPitchBend(ActiveSound *sound, uint8_t channel, int16_t value) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->pitchBend = static_cast<int16_t>(ClampInt(value, -0x2000, 0x1FFF));
    applyPartPitchBend(sound, part);
}

void ImuseEngine::partSetPitchBendFactor(ActiveSound *sound, uint8_t channel, uint8_t value) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->pitchBendFactor = static_cast<uint8_t>(ClampInt(value, 0, 12));
    part->pitchBend = 0;
    applyPartPitchBendFactor(sound, part);
    applyPartPitchBend(sound, part);
}

void ImuseEngine::partSetPolyphony(ActiveSound *sound, uint8_t channel, uint8_t value) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->polyphony = value;
    applyPartPolyphony(sound, part);
}

void ImuseEngine::partSetDetune(ActiveSound *sound, uint8_t channel, int16_t value) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->detune = static_cast<int16_t>(ClampInt(value, -128, 127));
    part->detuneEffective = static_cast<int16_t>(ClampInt(part->detune + sound->detune, -128, 127));
    sendPartDetune(sound, part);
}

void ImuseEngine::partSetModwheel(ActiveSound *sound, uint8_t channel, uint8_t value) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->modwheel = value;
    applyPartModwheel(sound, part);
}

void ImuseEngine::partSetSustain(ActiveSound *sound, uint8_t channel, bool on) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->pedal = on;
    applyPartSustain(sound, part);
}

void ImuseEngine::partSetEffectLevel(ActiveSound *sound, uint8_t channel, uint8_t value) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->effectLevel = value;
    applyPartEffectLevel(sound, part);
}

void ImuseEngine::partSetChorus(ActiveSound *sound, uint8_t channel, uint8_t value) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->chorus = value;
    applyPartChorus(sound, part);
}

void ImuseEngine::partSetInstrument(ActiveSound *sound, uint8_t channel, uint16_t value) {
    PartState *part = getPart(sound, channel);
    if (!part) {
        return;
    }

    part->instrumentValue = value;
    part->bank = static_cast<uint8_t>(value >> 8);
    part->program = static_cast<uint8_t>(value & 0xFF);
    part->instrument.program(part->program, part->bank, sound->isMt32);
    part->unassignedInstrument = false;
    applyPartProgram(sound, part);
}

void ImuseEngine::partNoteOn(ActiveSound *sound, uint8_t channel, uint8_t sourceNote, uint8_t velocity) {
    PartState *part = getPart(sound, channel);
    if (!sound || !part || !part->on) {
        return;
    }

    if (!part->percussion && !part->transmitted) {
        applyPartAllState(sound, part);
    }

    if (!part->percussion && part->outputChannel < 0) {
        return;
    }

    uint8_t outputNote = sourceNote;
    if (!part->percussion) {
        outputNote = clampOutputNote(sourceNote + computeEffectivePartTranspose(*sound, *part));
    } else if (sourceNote < 35 && !_nativeMt32Output) {
        outputNote = Instrument::kGmRhythmMap[sourceNote];
    }

    part->sourceNotes[sourceNote] = 1;
    part->sourceOutputNotes[sourceNote] = outputNote;
    part->sourceOutputChannels[sourceNote] = outputChannelForPart(*part);
    part->sourceVelocities[sourceNote] = velocity;

    if (part->percussion) {
        const uint8_t physicalChannel = outputChannelForPart(*part);
        if (_rhyState.vol != part->effectiveVolume) {
            emitMidiMessage(sound->soundId, static_cast<uint8_t>(0xB0 | physicalChannel), 7, true, part->effectiveVolume);
        }
        if (_compatibility == CompatibilityProfile::SamAndMax) {
            if (_rhyState.prio != part->priorityEffective) {
                emitMidiMessage(sound->soundId, static_cast<uint8_t>(0xB0 | physicalChannel), 18, true, part->priorityEffective);
            }
            if (_rhyState.poly != part->polyphony) {
                emitMidiMessage(sound->soundId, static_cast<uint8_t>(0xB0 | physicalChannel), 17, true, part->polyphony);
            }
        }
        _rhyState = RhythmState{part->effectiveVolume, part->polyphony, part->priorityEffective};
    }

    emitPartMidiMessage(sound, part, 0x90, outputNote, true, velocity);
}

void ImuseEngine::partNoteOff(ActiveSound *sound, uint8_t channel, uint8_t sourceNote) {
    PartState *part = getPart(sound, channel);
    if (!sound || !part || !part->on) {
        return;
    }

    const uint8_t outputNote = part->sourceNotes[sourceNote] != 0
        ? part->sourceOutputNotes[sourceNote]
        : clampOutputNote(sourceNote + computeEffectivePartTranspose(*sound, *part));

    part->sourceNotes[sourceNote] = 0;
    part->sourceOutputNotes[sourceNote] = 0;
    part->sourceOutputChannels[sourceNote] = 0;
    part->sourceVelocities[sourceNote] = 0;
    if (!part->percussion && part->outputChannel < 0) {
        return;
    }
    emitPartMidiMessage(sound, part, 0x80, outputNote, true, 0);
}

void ImuseEngine::turnOffParts(ActiveSound *sound) {
    if (!sound) {
        return;
    }

    for (std::size_t channelIndex = 0; channelIndex < sound->parts.size(); ++channelIndex) {
        if (PartState *part = getActivePart(sound, static_cast<uint8_t>(channelIndex))) {
            if (part->outputChannel >= 0) {
                if (part->pedal) {
                    part->pedal = false;
                    emitPartMidiMessage(sound, part, 0xB0, 64, true, 0);
                }
                emitPartMidiMessage(sound, part, 0xB0, 123, true, 0);
                releaseOutputChannel(part);
            }
            clearPartActiveNotes(part);
        }
    }
    reallocateMidiChannels();
}

void ImuseEngine::replayScanActiveNotes(ActiveSound *sound) {
    if (!sound) {
        return;
    }

    for (std::size_t channelIndex = 0; channelIndex < 16; ++channelIndex) {
        const uint16_t mask = static_cast<uint16_t>(1u << channelIndex);
        bool hasActiveNotes = false;
        for (uint16_t noteMask : _scan.activeNotes) {
            if (noteMask & mask) {
                hasActiveNotes = true;
                break;
            }
        }
        if (!hasActiveNotes) {
            continue;
        }

        PartState *part = getPart(sound, static_cast<uint8_t>(channelIndex));
        if (!part) {
            continue;
        }

        for (std::size_t note = 0; note < _scan.activeNotes.size(); ++note) {
            if ((_scan.activeNotes[note] & mask) == 0) {
                continue;
            }

            partNoteOn(sound, static_cast<uint8_t>(channelIndex), static_cast<uint8_t>(note), 80);
        }
    }
}

bool ImuseEngine::dispatchChannelEvent(ActiveSound *sound, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
    if (!sound) {
        return false;
    }

    const uint8_t command = status & 0xF0;
    const uint8_t channel = status & 0x0F;

    switch (command >> 4) {
    case 0x8:
        if (isScanActiveFor(sound->soundId)) {
            trackScanNote(status, data1, hasData2, data2);
        } else {
            partNoteOff(sound, channel, data1);
        }
        return true;
    case 0x9:
        if (!hasData2 || data2 == 0) {
            if (isScanActiveFor(sound->soundId)) {
                trackScanNote(static_cast<uint8_t>(0x80 | channel), data1, true, 0);
            } else {
                partNoteOff(sound, channel, data1);
            }
            return true;
        }
        if (isScanActiveFor(sound->soundId)) {
            trackScanNote(status, data1, hasData2, data2);
        } else {
            partNoteOn(sound, channel, data1, data2);
        }
        return true;
    case 0xB: {
        PartState *part = (data1 == 123) ? getActivePart(sound, channel) : getPart(sound, channel);
        if (!part) {
            return true;
        }

        switch (data1) {
        case 0:
            part->bank = hasData2 ? data2 : 0;
            return true;
        case 1:
            partSetModwheel(sound, channel, data2);
            return true;
        case 7:
            partSetVolume(sound, channel, data2);
            return true;
        case 10:
            partSetPan(sound, channel, static_cast<int8_t>(ClampInt(static_cast<int>(data2) - 0x40, -64, 63)));
            return true;
        case 16:
            partSetPitchBendFactor(sound, channel, data2);
            return true;
        case 17:
            if (_compatibility == CompatibilityProfile::SamAndMax) {
                partSetPolyphony(sound, channel, data2);
            } else {
                partSetDetune(sound, channel, static_cast<int16_t>(static_cast<int>(data2) - 0x40));
            }
            return true;
        case 18:
            if (_compatibility == CompatibilityProfile::SamAndMax) {
                partSetPriority(sound, channel, static_cast<int8_t>(data2));
            } else {
                partSetPriority(sound, channel, static_cast<int8_t>(static_cast<int>(data2) - 0x40));
            }
            return true;
        case 64:
            partSetSustain(sound, channel, data2 != 0);
            return true;
        case 91:
            partSetEffectLevel(sound, channel, data2);
            return true;
        case 93:
            partSetChorus(sound, channel, data2);
            return true;
        case 120:
        case 123:
            partAllNotesOff(sound, part);
            return true;
        default:
            emitMidiMessage(sound->soundId, status, data1, hasData2, data2);
            return true;
        }
    }
    case 0xC:
        if (PartState *part = getPart(sound, channel)) {
            (void)part;
            partSetProgram(sound, channel, data1);
        }
        return true;
    case 0xD:
    case 0xA:
        emitMidiMessage(sound->soundId, status, data1, hasData2, data2);
        return true;
    case 0xE:
        if (hasData2) {
            partSetPitchBend(sound, channel, static_cast<int16_t>(((data2 << 7) | data1) - 0x2000));
        }
        return true;
    case 0xF:
        return true;
    default:
        return true;
    }
}

void ImuseEngine::trackMidiMessage(ActiveSound *sound, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
    if (!sound) {
        return;
    }

    const uint8_t statusClass = status & 0xF0;
    if (statusClass < 0x80 || statusClass > 0xE0) {
        return;
    }

    MidiChannelState &channel = sound->midiChannels[status & 0x0F];
    auto noteOff = [&](uint8_t note) {
        channel.heldNotes[note] = 0;
        if (!channel.sustainOn()) {
            channel.soundingNotes[note] = 0;
        }
    };

    switch (statusClass) {
    case 0x80:
        cancelHangingNote(sound, static_cast<uint8_t>(status & 0x0F), data1);
        noteOff(data1);
        break;
    case 0x90:
        if (!hasData2 || data2 == 0) {
            cancelHangingNote(sound, static_cast<uint8_t>(status & 0x0F), data1);
            noteOff(data1);
            break;
        }
        cancelHangingNote(sound, static_cast<uint8_t>(status & 0x0F), data1);
        channel.heldNotes[data1] = 1;
        channel.soundingNotes[data1] = 1;
        break;
    case 0xB0:
        if (hasData2) {
            if (data1 == 64) {
                const bool wasSustainOn = channel.sustainOn();
                channel.sustainValue = data2;
                if (wasSustainOn && !channel.sustainOn()) {
                    for (std::size_t note = 0; note < channel.soundingNotes.size(); ++note) {
                        channel.soundingNotes[note] = channel.heldNotes[note];
                    }
                }
            } else if (data1 == 120 || data1 == 123) {
                channel.heldNotes.fill(0);
                channel.soundingNotes.fill(0);
            }
        }
        break;
    default:
        break;
    }
}

void ImuseEngine::flushActiveMidiState(uint16_t soundId) {
    ActiveSound *sound = findActiveSound(soundId);
    if (!sound) {
        return;
    }

    // 1. First, send individual Note Offs and Sustain Offs to the active physical channels
    if (_midiSink) {
        for (std::size_t channelIndex = 0; channelIndex < sound->midiChannels.size(); ++channelIndex) {
            MidiChannelState &channel = sound->midiChannels[channelIndex];
            if (channel.sustainOn()) {
                emitMidiMessage(soundId, static_cast<uint8_t>(0xB0 | channelIndex), 64, true, 0);
            }

            for (std::size_t note = 0; note < channel.soundingNotes.size(); ++note) {
                if (channel.soundingNotes[note] != 0) {
                    emitMidiMessage(soundId,
                                    static_cast<uint8_t>(0x80 | channelIndex),
                                    static_cast<uint8_t>(note),
                                    true,
                                    0);
                }
            }
        }
    }

    // 2. Remove all parts of this sound from the waiting queue first
    for (std::size_t channelIndex = 0; channelIndex < sound->parts.size(); ++channelIndex) {
        PartState &part = sound->parts[channelIndex];
        removeSuspendedPart(&part);
    }

    // 3. Now stop all active parts
    for (std::size_t channelIndex = 0; channelIndex < sound->parts.size(); ++channelIndex) {
        PartState &part = sound->parts[channelIndex];
        if (part.initialized) {
            partOff(sound, &part);
            part.pedal = false;
            part.transmitted = false;
            part.present = false;
            part.initialized = false;
            clearPartActiveNotes(&part);
        }
        removeSuspendedPart(&part);
        releaseOutputChannel(&part);
    }

    resetMidiState(sound);
}

void ImuseEngine::releaseSustainPedals(ActiveSound *sound) {
    if (!sound) {
        return;
    }

    for (std::size_t channelIndex = 0; channelIndex < sound->parts.size(); ++channelIndex) {
        if (PartState *part = getActivePart(sound, static_cast<uint8_t>(channelIndex))) {
            if (part->pedal) {
                part->pedal = false;
                applyPartSustain(sound, part);
            }
        }
    }
}

void ImuseEngine::scheduleSmartJumpNotes(ActiveSound *sound) {
    if (!sound || sound->trackIndex >= sound->sequence.smf.tracks.size()) {
        return;
    }

    struct PendingJumpNote {
        bool active = false;
        uint8_t physicalChannel = 0;
        uint8_t outputNote = 0;
    };

    std::array<std::array<PendingJumpNote, 128>, 16> remaining = {{}};
    bool hasActiveNotes = false;
    for (std::size_t channelIndex = 0; channelIndex < sound->parts.size(); ++channelIndex) {
        PartState *part = getActivePart(sound, static_cast<uint8_t>(channelIndex));
        if (!part || !part->on) {
            continue;
        }
        for (std::size_t note = 0; note < part->sourceNotes.size(); ++note) {
            if (part->sourceNotes[note] == 0) {
                continue;
            }
            remaining[channelIndex][note] = PendingJumpNote{
                true,
                part->sourceOutputChannels[note],
                part->sourceOutputNotes[note]
            };
            hasActiveNotes = true;
        }
    }

    if (!hasActiveNotes) {
        return;
    }

    const uint32_t currentTick = absoluteTick(*sound);
    const SmfTrack &track = sound->sequence.smf.tracks[sound->trackIndex];
    for (const MidiEvent &event : track.events) {
        if (event.tick <= currentTick || event.type != MidiEventType::Channel || !event.hasData1) {
            continue;
        }

        const uint8_t statusClass = event.status & 0xF0;
        if (statusClass != 0x80 && !(statusClass == 0x90 && event.hasData2 && event.data2 == 0)) {
            continue;
        }

        const uint8_t channel = event.status & 0x0F;
        const uint8_t note = event.data1;
        PendingJumpNote &pending = remaining[channel][note];
        if (!pending.active) {
            continue;
        }

        sound->hangingNotes.push_back(HangingNote{pending.physicalChannel, pending.outputNote, event.tick - currentTick});
        pending.active = false;

        bool hasPending = false;
        for (const auto &perChannel : remaining) {
            for (const PendingJumpNote &left : perChannel) {
                if (left.active) {
                    hasPending = true;
                    break;
                }
            }
            if (hasPending) {
                break;
            }
        }
        if (!hasPending) {
            return;
        }
    }

    for (std::size_t channelIndex = 0; channelIndex < remaining.size(); ++channelIndex) {
        for (std::size_t note = 0; note < remaining[channelIndex].size(); ++note) {
            const PendingJumpNote &pending = remaining[channelIndex][note];
            if (!pending.active) {
                continue;
            }
            MidiChannelState &channel = sound->midiChannels[pending.physicalChannel];
            channel.heldNotes[pending.outputNote] = 0;
            channel.soundingNotes[pending.outputNote] = 0;
            if (_midiSink) {
                _midiSink->onMidiMessage(sound->soundId,
                                         static_cast<uint8_t>(0x80 | pending.physicalChannel),
                                         pending.outputNote,
                                         true,
                                         0);
            }
        }
    }
}

bool ImuseEngine::findNextHangingTick(const ActiveSound &sound, uint32_t *tickDelta) const {
    uint32_t best = 0;
    bool found = false;
    for (const HangingNote &note : sound.hangingNotes) {
        if (note.ticksLeft == 0) {
            continue;
        }
        if (!found || note.ticksLeft < best) {
            best = note.ticksLeft;
            found = true;
        }
    }
    if (found && tickDelta) {
        *tickDelta = best;
    }
    return found;
}

void ImuseEngine::advanceHangingNotes(ActiveSound *sound, uint32_t deltaTicks) {
    if (!sound || deltaTicks == 0) {
        return;
    }

    for (HangingNote &note : sound->hangingNotes) {
        if (note.ticksLeft == 0) {
            continue;
        }
        note.ticksLeft = (note.ticksLeft <= deltaTicks) ? 0 : (note.ticksLeft - deltaTicks);
    }
}

void ImuseEngine::emitExpiredHangingNotes(ActiveSound *sound) {
    if (!sound || sound->hangingNotes.empty()) {
        return;
    }

    std::vector<HangingNote> pending;
    pending.reserve(sound->hangingNotes.size());

    for (const HangingNote &note : sound->hangingNotes) {
        if (note.ticksLeft != 0) {
            pending.push_back(note);
            continue;
        }

        MidiChannelState &channel = sound->midiChannels[note.channel];
        channel.heldNotes[note.note] = 0;
        channel.soundingNotes[note.note] = 0;
        if (_midiSink) {
            _midiSink->onMidiMessage(sound->soundId,
                                     static_cast<uint8_t>(0x80 | note.channel),
                                     note.note,
                                     true,
                                     0);
        }
    }

    sound->hangingNotes = std::move(pending);
}

bool ImuseEngine::isScanActiveFor(uint16_t soundId) const {
    return _scan.active && _scan.soundId == soundId;
}

void ImuseEngine::beginScan(uint16_t soundId) {
    _scan.reset();
    _scan.active = true;
    _scan.soundId = soundId;
}

void ImuseEngine::endScan(ActiveSound *sound) {
    if (!sound || !isScanActiveFor(sound->soundId)) {
        _scan.reset();
        return;
    }

    std::array<uint8_t, 16> sustainValues = {{}};
    for (std::size_t channelIndex = 0; channelIndex < sound->midiChannels.size(); ++channelIndex) {
        sustainValues[channelIndex] = sound->midiChannels[channelIndex].sustainValue;
    }

    resetMidiState(sound);
    for (std::size_t channelIndex = 0; channelIndex < sound->midiChannels.size(); ++channelIndex) {
        sound->midiChannels[channelIndex].sustainValue = sustainValues[channelIndex];
    }

    for (PartState &part : sound->parts) {
        if (part.initialized) {
            clearPartActiveNotes(&part);
        }
    }

    replayScanActiveNotes(sound);
    _scan.reset();
}

void ImuseEngine::trackScanNote(uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
    const uint8_t channel = status & 0x0F;
    const uint8_t statusClass = status & 0xF0;
    const uint16_t mask = static_cast<uint16_t>(1u << channel);

    auto noteOff = [&](uint8_t note) {
        _scan.activeNotes[note] &= static_cast<uint16_t>(~mask);
    };

    switch (statusClass) {
    case 0x80:
        noteOff(data1);
        break;
    case 0x90:
        if (!hasData2 || data2 == 0) {
            noteOff(data1);
            break;
        }
        _scan.activeNotes[data1] |= mask;
        break;
    case 0xB0:
        if (hasData2 && (data1 == 120 || data1 == 123)) {
            for (uint16_t &noteMask : _scan.activeNotes) {
                noteMask &= static_cast<uint16_t>(~mask);
            }
        }
        break;
    default:
        break;
    }
}

bool ImuseEngine::handleMidiEvent(uint16_t soundId, const MidiEvent &event) {
    ActiveSound *sound = findActiveSound(soundId);
    if (!sound) {
        return false;
    }

    switch (event.type) {
    case MidiEventType::Channel:
        if (event.hasData1) {
            return dispatchChannelEvent(sound, event.status, event.data1, event.hasData2, event.data2);
        }
        return true;
    case MidiEventType::Meta:
        if (event.metaType == 0x51 && event.payload.size() == 3) {
            sound->tempoMicrosPerQuarter =
                (static_cast<uint32_t>(event.payload[0]) << 16) |
                (static_cast<uint32_t>(event.payload[1]) << 8) |
                static_cast<uint32_t>(event.payload[2]);
            if (_midiSink) {
                _midiSink->onTempoChange(soundId, sound->tempoMicrosPerQuarter);
            }
        }
        return true;
    case MidiEventType::SysEx: {
        ImuseControlEvent controlEvent;
        if (DecodeImuseSysex(ByteView(event.payload.data(), event.payload.size()), &controlEvent, nullptr)) {
            controlEvent.tick = event.tick;
            controlEvent.trackIndex = sound->trackIndex;
            return executeControlEvent(soundId, controlEvent);
        } else if (_midiSink) {
            if (event.payload.size() > 6 && event.payload[0] == 0x41) {
                int logicalPart = event.payload[1] & 0x0F;
                
                PartState *matchedPart = nullptr;
                if (logicalPart >= 0) {
                    for (auto &p : sound->parts) {
                        if (p.present && p.channel == logicalPart) {
                            matchedPart = &p;
                            break;
                        }
                    }
                }
                
                if (matchedPart) {
                    matchedPart->customRolandSysex.assign(event.payload.begin(), event.payload.end());
                    
                    // If the part is already allocated to a physical channel, we can send it immediately
                    if (matchedPart->outputChannel >= 1 && matchedPart->outputChannel <= 8) {
                        sendPartCustomSysex(soundId, matchedPart);
                    }
                } else {
                    _midiSink->onSysEx(soundId, ByteView(event.payload.data(), event.payload.size()));
                }
            } else {
                _midiSink->onSysEx(soundId, ByteView(event.payload.data(), event.payload.size()));
            }
        }
        return true;
    }
    case MidiEventType::System:
        return true;
    }

    return true;
}

bool ImuseEngine::dispatchTrackTick(uint16_t soundId) {
    ActiveSound *sound = findActiveSound(soundId);
    if (!sound || sound->trackIndex >= sound->sequence.smf.tracks.size()) {
        return false;
    }

    const uint32_t tick = absoluteTick(*sound);
    const SmfTrack &track = sound->sequence.smf.tracks[sound->trackIndex];
    std::vector<MidiEvent> atTick;
    for (const MidiEvent &event : track.events) {
        if (event.tick == tick) {
            atTick.push_back(event);
        }
    }

    for (const MidiEvent &event : atTick) {
        if (!handleMidiEvent(soundId, event)) {
            return false;
        }
        if (!findActiveSound(soundId)) {
            return false;
        }
    }
    return true;
}

bool ImuseEngine::maybeJump(ActiveSound *sound, uint8_t hookCommand, uint16_t track, uint16_t beat, uint16_t tick) {
    if (!sound) {
        return false;
    }

    if (hookCommand && sound->hook.jump[0] != 0 && sound->hook.jump[0] != hookCommand) {
        return false;
    }

    if (hookCommand != 0 && hookCommand < 0x80) {
        sound->hook.jump[0] = sound->hook.jump[1];
        sound->hook.jump[1] = 0xFF;
    }

    if (beat == 0 || normalizeTrackIndex(*sound, track) == kInvalidTrackIndex) {
        return false;
    }

    if (isScanActiveFor(sound->soundId)) {
        return jumpTo(sound, track, beat, tick);
    }

    ActiveSound snapshot = *sound;
    scheduleSmartJumpNotes(sound);
    if (!jumpTo(sound, track, beat, tick)) {
        *sound = std::move(snapshot);
        return false;
    }

    releaseSustainPedals(sound);
    return true;
}

bool ImuseEngine::executeControlEvent(uint16_t soundId, const ImuseControlEvent &event) {
    ActiveSound *sound = findActiveSound(soundId);
    if (!sound) {
        return false;
    }

    switch (event.type) {
    case ImuseSysexType::AllocatePart:
        if (event.hasPart && event.part < sound->parts.size()) {
            PartState *part = getPart(sound, event.part);
            if (part) {
                part->present = true;
                part->on = event.partOn;
                part->effectLevel = event.reverb ? 127 : 0;
                part->volume = event.volume;
                part->transpose = event.transpose;
                part->transposeEffective = computeEffectivePartTranspose(*sound, *part);
                part->pan = static_cast<int8_t>(ClampInt(static_cast<int>(event.pan), -64, 63));
                part->panEffective = computeEffectivePartPan(*sound, *part);
                part->priority = static_cast<int8_t>(ClampInt(static_cast<int>(event.priority) - 0x40, -128, 127));
                part->priorityEffective = static_cast<uint8_t>(ClampInt(part->priority + sound->priority, 0, 255));
                part->percussion = event.percussion;
                part->detune = event.detune;
                part->detuneEffective = static_cast<int16_t>(ClampInt(part->detune + sound->detune, -128, 127));
                part->pitchBendFactor = event.pitchbendFactor;
                
                partSetProgram(sound, event.part, event.program);
                if (part->outputChannel < 0) {
                    reallocateMidiChannels();
                } else {
                    applyPartAllState(sound, part);
                }
            }
        }
        return true;
    case ImuseSysexType::ShutdownPart:
        if (event.hasPart && event.part < sound->parts.size()) {
            PartState &part = sound->parts[event.part];
            partOff(sound, &part);
            removeSuspendedPart(&part);
            releaseOutputChannel(&part);
            sound->parts[event.part] = {};
        }
        return true;
    case ImuseSysexType::StartSong:
        for (PartState &part : sound->parts) {
            partOff(sound, &part);
            removeSuspendedPart(&part);
            releaseOutputChannel(&part);
            part = {};
        }
        return true;
    case ImuseSysexType::AdlibPartInstrument:
        if (event.hasPart && event.part < sound->parts.size()) {
            PartState *part = getPart(sound, event.part);
            if (part) {
                part->present = true;
                part->initialized = true;
                if (event.decodedBytes.size() >= 11) {
                    // Full OPL2 instrument data: load into part and send to sink
                    part->instrument.adlib(event.decodedBytes.data());
                    applyPartProgram(sound, part);
                } else if (!event.decodedBytes.empty() && _midiSink) {
                    // Fallback: only a program number, send as program change
                    SinkMidiChannel ch(_midiSink, sound->soundId,
                                       static_cast<uint8_t>(part->outputChannel));
                    ch.programChange(event.decodedBytes[0]);
                }
            }
        }
        return true;
    case ImuseSysexType::AdlibGlobalInstrument:
        if (event.decodedBytes.size() >= 11 && _midiSink) {
            // Broadcast OPL2 instrument data to every active part of this sound
            for (std::size_t i = 0; i < sound->parts.size(); ++i) {
                PartState *part = getActivePart(sound, static_cast<uint8_t>(i));
                if (part && part->outputChannel >= 0) {
                    part->instrument.adlib(event.decodedBytes.data());
                    applyPartProgram(sound, part);
                }
            }
        }
        return true;
    case ImuseSysexType::ParameterAdjust:
        return true;
    case ImuseSysexType::HookJump:
        return maybeJump(sound, event.hookCommand, event.targetTrack, event.targetBeat, event.targetTick);
    case ImuseSysexType::HookGlobalTranspose:
        if (event.hookCommand && sound->hook.transpose != 0 && sound->hook.transpose != event.hookCommand) {
            return true;
        }
        if (event.hookCommand != 0 && event.hookCommand < 0x80) {
            sound->hook.transpose = 0xFF;
        }
        return setSoundTranspose(sound, event.relative ? 1 : 0, event.signedValue) == 0;
    case ImuseSysexType::HookPartOnOff:
        if (event.hasChannel && event.channel < sound->parts.size()) {
            if (event.hookCommand && sound->hook.partOnOff[event.channel] != 0 && sound->hook.partOnOff[event.channel] != event.hookCommand) {
                return true;
            }
            if (event.hookCommand != 0 && event.hookCommand < 0x80) {
                sound->hook.partOnOff[event.channel] = 0xFF;
            }
            partSetOnOff(sound, event.channel, event.value != 0);
        }
        return true;
    case ImuseSysexType::HookSetVolume:
        if (event.hasChannel && event.channel < sound->parts.size()) {
            if (event.hookCommand && sound->hook.partVolume[event.channel] != 0 && sound->hook.partVolume[event.channel] != event.hookCommand) {
                return true;
            }
            if (event.hookCommand != 0 && event.hookCommand < 0x80) {
                sound->hook.partVolume[event.channel] = 0xFF;
            }
            partSetVolume(sound, event.channel, event.value);
        }
        return true;
    case ImuseSysexType::HookSetProgram:
        if (event.hasChannel && event.channel < sound->parts.size()) {
            if (event.hookCommand && sound->hook.partProgram[event.channel] != 0 && sound->hook.partProgram[event.channel] != event.hookCommand) {
                return true;
            }
            if (event.hookCommand != 0 && event.hookCommand < 0x80) {
                sound->hook.partProgram[event.channel] = 0xFF;
            }
            partSetProgram(sound, event.channel, event.value);
        }
        return true;
    case ImuseSysexType::HookSetTranspose:
        if (event.hasChannel && event.channel < sound->parts.size()) {
            if (event.hookCommand && sound->hook.partTranspose[event.channel] != 0 && sound->hook.partTranspose[event.channel] != event.hookCommand) {
                return true;
            }
            if (event.hookCommand != 0 && event.hookCommand < 0x80) {
                sound->hook.partTranspose[event.channel] = 0xFF;
            }
            int value = event.signedValue;
            if (event.relative) {
                const PartState *part = getPart(sound, event.channel);
                if (part) {
                    value = TransposeClamp(part->transpose + value, -7, 7);
                }
            }
            partSetTranspose(sound, event.channel, static_cast<int8_t>(ClampInt(value, -24, 24)));
        }
        return true;
    case ImuseSysexType::Marker:
        for (unsigned char marker : event.markerText) {
            handleMarker(soundId, static_cast<uint8_t>(marker));
        }
        return true;
    case ImuseSysexType::SetLoop:
        return setLoop(sound, event.loopCount, event.loopToBeat, event.loopToTick, event.loopFromBeat, event.loopFromTick);
    case ImuseSysexType::ClearLoop:
        sound->loopCounter = 0;
        return true;
    case ImuseSysexType::SetInstrument:
        if (event.hasChannel && event.channel < sound->parts.size()) {
            partSetInstrument(sound, event.channel, event.instrument);
        }
        return true;
    case ImuseSysexType::Unknown:
        return true;
    }

    return true;
}

int ImuseEngine::enqueueTrigger(uint16_t soundId, uint8_t marker) {
    QueuedTrigger trigger;
    trigger.soundId = soundId;
    trigger.marker = marker;
    _triggerQueue.push_back(trigger);
    return 0;
}

int ImuseEngine::enqueueCommand(uint16_t argc, const int16_t *args) {
    if (_triggerQueue.empty() || !args || argc < 3) {
        return -1;
    }

    QueuedTrigger &trigger = _triggerQueue.back();
    if (trigger.finalized) {
        return -1;
    }

    if (args[2] == -1) {
        trigger.finalized = true;
        return 0;
    }

    CommandPacket packet = MakeCommandPacket(argc, args, 2);
    if (packet.argc == 0) {
        return -1;
    }

    trigger.commands.push_back(packet);
    return 0;
}

int ImuseEngine::clearQueue() {
    _triggerQueue.clear();
    return 0;
}

int ImuseEngine::queryQueue(uint16_t param) const {
    switch (param) {
    case 0:
        return static_cast<int>(_triggerQueue.size());
    case 1:
        return _triggerQueue.empty() ? -1 : 1;
    case 2:
        return _triggerQueue.empty() ? 0xFF : _triggerQueue.front().soundId;
    default:
        return -1;
    }
}

int ImuseEngine::setScriptTrigger(uint16_t soundId, uint8_t marker, const CommandPacket &command) {
    for (ScriptTrigger &trigger : _scriptTriggers) {
        if (trigger.soundId == soundId && trigger.id == marker && trigger.command.argc > 0 && command.argc > 0 && trigger.command.args[0] == command.args[0]) {
            trigger.expire = ++_scriptTriggerEpoch;
            trigger.command = command;
            return 0;
        }
    }

    ScriptTrigger trigger;
    trigger.soundId = soundId;
    trigger.id = marker;
    trigger.expire = ++_scriptTriggerEpoch;
    trigger.command = command;
    _scriptTriggers.push_back(trigger);
    return 0;
}

int ImuseEngine::clearScriptTrigger(int soundId, int marker) {
    const std::size_t before = _scriptTriggers.size();
    _scriptTriggers.erase(
        std::remove_if(_scriptTriggers.begin(), _scriptTriggers.end(),
                       [soundId, marker](const ScriptTrigger &trigger) {
                           const bool soundMatches = (soundId < 0) || (trigger.soundId == static_cast<uint16_t>(soundId));
                           const bool markerMatches = (marker < 0) || (trigger.id == static_cast<uint8_t>(marker));
                           return soundMatches && markerMatches;
                       }),
        _scriptTriggers.end());
    return before != _scriptTriggers.size() ? 0 : -1;
}

int ImuseEngine::checkScriptTrigger(uint16_t soundId, int marker) const {
    int count = 0;
    for (const ScriptTrigger &trigger : _scriptTriggers) {
        if (trigger.soundId == soundId && trigger.id != 0 && (marker < 0 || trigger.id == static_cast<uint8_t>(marker))) {
            ++count;
        }
    }
    return count;
}

int ImuseEngine::fireAllScriptTriggers(uint16_t soundId) {
    std::vector<CommandPacket> commands;
    for (const ScriptTrigger &trigger : _scriptTriggers) {
        if (trigger.soundId == soundId && trigger.id != 0) {
            commands.push_back(trigger.command);
        }
    }

    _scriptTriggers.erase(
        std::remove_if(_scriptTriggers.begin(), _scriptTriggers.end(),
                       [soundId](const ScriptTrigger &trigger) { return trigger.soundId == soundId && trigger.id != 0; }),
        _scriptTriggers.end());

    int count = 0;
    for (const CommandPacket &command : commands) {
        doCommand(command.argc, command.args.data());
        ++count;
    }
    return count > 0 ? 0 : -1;
}

void ImuseEngine::addDeferredCommand(uint32_t ticksLeft, const CommandPacket &command) {
    if (command.argc == 0) {
        return;
    }

    DeferredCommand deferred;
    deferred.ticksLeft = ticksLeft;
    deferred.command = command;
    _deferredCommands.push_back(deferred);
}

void ImuseEngine::processDeferredCommands(uint32_t deltaTicks) {
    std::vector<CommandPacket> ready;
    std::vector<DeferredCommand> pending;
    pending.reserve(_deferredCommands.size());

    for (DeferredCommand &deferred : _deferredCommands) {
        if (deferred.ticksLeft <= deltaTicks) {
            ready.push_back(deferred.command);
        } else {
            deferred.ticksLeft -= deltaTicks;
            pending.push_back(deferred);
        }
    }

    _deferredCommands = std::move(pending);
    for (const CommandPacket &command : ready) {
        doCommand(command.argc, command.args.data());
    }
}

int ImuseEngine::handleMarker(uint16_t soundId, uint8_t marker) {
    std::vector<CommandPacket> commandsToRun;

    if (_markerCallback) {
        _markerCallback(soundId, marker);
    }

    if (!_triggerQueue.empty()) {
        const QueuedTrigger &trigger = _triggerQueue.front();
        if (trigger.soundId == soundId && trigger.marker == marker) {
            commandsToRun.insert(commandsToRun.end(), trigger.commands.begin(), trigger.commands.end());
            _triggerQueue.pop_front();
        }
    }

    for (const ScriptTrigger &trigger : _scriptTriggers) {
        if (trigger.soundId == soundId && trigger.id == marker) {
            commandsToRun.push_back(trigger.command);
        }
    }
    clearScriptTrigger(soundId, marker);

    for (const CommandPacket &command : commandsToRun) {
        doCommand(command.argc, command.args.data());
    }

    return commandsToRun.empty() ? -1 : 0;
}

bool ImuseEngine::startSound(uint16_t soundId) {
    if (_logCallback) {
        _logCallback("ImuseEngine::startSound(" + std::to_string(soundId) + ")");
    }
    if (isSoundActive(soundId)) {
        stopSound(soundId);
    }
    if (!_bank || !_bank->hasSound(soundId)) {
        return false;
    }

    SoundResource resource = _bank->loadSound(soundId, nullptr);
    if (!resource.valid()) {
        return false;
    }

    ActiveSound active;
    active.soundId = soundId;
    active.variant = resource.selectVariant(_profile);
    if (!active.variant.valid()) {
        return false;
    }
    if (!LoadImuseSequence(active.variant, &active.sequence, nullptr)) {
        return false;
    }

    active.isMidi = true;
    active.isMt32 = (_profile == TargetProfile::Mt32);
    active.supportsPercussion = true;
    active.priority = active.variant.mdhd.priority;
    active.volume = static_cast<uint8_t>(ClampInt(active.variant.mdhd.volume, 0, 127));
    active.pan = static_cast<int8_t>(ClampInt(active.variant.mdhd.pan, -128, 127));
    active.transpose = active.variant.mdhd.transpose;
    active.detune = active.variant.mdhd.detune;
    active.speed = active.variant.mdhd.speed;
    active.volChan = kDefaultVolChan;
    active.hook.reset();
    active.pendingImmediateEvents = true;
    resetMidiState(&active);
    refreshEffectiveVolume(&active);

    _activeSounds[soundId] = active;
    return true;
}

void ImuseEngine::stopSound(uint16_t soundId) {
    if (_logCallback) {
        _logCallback("ImuseEngine::stopSound(" + std::to_string(soundId) + ")");
    }
    flushActiveMidiState(soundId);
    _activeSounds.erase(soundId);
    if (_activeSounds.empty() && _midiSink) {
        _midiSink->onAllNotesOff();
    }
    fireAllScriptTriggers(soundId);
}

void ImuseEngine::stopAllSounds() {
    std::vector<uint16_t> ids;
    ids.reserve(_activeSounds.size());
    for (const auto &entry : _activeSounds) {
        ids.push_back(entry.first);
    }
    for (uint16_t id : ids) {
        flushActiveMidiState(id);
    }
    _activeSounds.clear();
    if (_midiSink) {
        _midiSink->onAllNotesOff();
    }
    for (uint16_t id : ids) {
        fireAllScriptTriggers(id);
    }
}

bool ImuseEngine::isSoundActive(uint16_t soundId) const {
    return _activeSounds.find(soundId) != _activeSounds.end();
}

int ImuseEngine::getSoundStatus(uint16_t soundId) const {
    if (isSoundActive(soundId)) {
        return 1;
    }

    for (const DeferredCommand &deferred : _deferredCommands) {
        if (deferred.command.argc >= 2 && static_cast<uint16_t>(deferred.command.args[0]) == 0x0008 && static_cast<uint16_t>(deferred.command.args[1]) == soundId) {
            return 2;
        }
    }

    for (const QueuedTrigger &trigger : _triggerQueue) {
        for (const CommandPacket &command : trigger.commands) {
            if (command.argc >= 2 && static_cast<uint16_t>(command.args[0]) == 0x0008 && static_cast<uint16_t>(command.args[1]) == soundId) {
                return 2;
            }
        }
    }

    return 0;
}

std::vector<uint16_t> ImuseEngine::activeSoundIds() const {
    std::vector<uint16_t> ids;
    ids.reserve(_activeSounds.size());
    for (const auto &entry : _activeSounds) {
        ids.push_back(entry.first);
    }
    return ids;
}

bool ImuseEngine::getPlaybackLocation(uint16_t soundId, uint16_t *track, uint16_t *beat, uint16_t *tick) const {
    const ActiveSound *sound = findActiveSound(soundId);
    if (!sound) {
        return false;
    }

    if (track) {
        *track = sound->trackIndex;
    }
    if (beat) {
        *beat = sound->beatIndex;
    }
    if (tick) {
        *tick = sound->tickIndex;
    }
    return true;
}

uint32_t ImuseEngine::currentTempoMicrosPerQuarter() const {
    if (_activeSounds.empty()) {
        return 500000;
    }

    const ActiveSound &sound = _activeSounds.begin()->second;
    return sound.tempoMicrosPerQuarter ? sound.tempoMicrosPerQuarter : 500000;
}

double ImuseEngine::transportTicksPerSecond() const {
    if (_activeSounds.empty()) {
        return 960.0;
    }

    const ActiveSound &sound = _activeSounds.begin()->second;
    const uint32_t tempo = sound.tempoMicrosPerQuarter ? sound.tempoMicrosPerQuarter : 500000;
    const double base = (static_cast<double>(ticksPerBeat(sound)) * 1000000.0) / static_cast<double>(tempo);
    const double speedScale = static_cast<double>(sound.speed ? sound.speed : 128) / 128.0;
    return base * speedScale;
}

void ImuseEngine::advanceAll(uint32_t deltaTicks) {
    processDeferredCommands(deltaTicks);

    std::vector<uint16_t> ids;
    ids.reserve(_activeSounds.size());
    for (const auto &entry : _activeSounds) {
        ids.push_back(entry.first);
    }

    for (uint16_t soundId : ids) {
        advanceSound(soundId, deltaTicks);
    }
}

bool ImuseEngine::advanceSound(uint16_t soundId, uint32_t deltaTicks) {
    ActiveSound *sound = findActiveSound(soundId);
    if (!sound) {
        return false;
    }

    uint32_t remaining = deltaTicks;
    while (remaining > 0 || (sound && sound->pendingImmediateEvents)) {
        sound = findActiveSound(soundId);
        if (!sound) {
            return false;
        }

        if (sound->pendingImmediateEvents) {
            sound->pendingImmediateEvents = false;
            if (!dispatchTrackTick(soundId)) {
                return findActiveSound(soundId) != nullptr;
            }
            continue;
        }

        const uint32_t currentTick = absoluteTick(*sound);
        const uint32_t endTick = trackEndTick(*sound, sound->trackIndex);
        const uint32_t loopTick = (sound->loopFromBeat > 0) ? ((static_cast<uint32_t>(sound->loopFromBeat - 1) * ticksPerBeat(*sound)) + sound->loopFromTick) : 0;

        uint32_t nextTrackEventTick = 0;
        uint32_t nextHangingTick = 0;
        const bool hasTrackEvent = findNextTrackEventTick(*sound, sound->trackIndex, currentTick, &nextTrackEventTick);
        const bool hasLoop = sound->loopCounter > 0 && sound->loopFromBeat > 0 && loopTick >= currentTick;
        const bool hasHanging = findNextHangingTick(*sound, &nextHangingTick);

        if (!hasTrackEvent && !hasLoop && !hasHanging && currentTick >= endTick) {
            if (isScanActiveFor(soundId)) {
                break;
            }
            stopSound(soundId);
            return false;
        }

        uint32_t step = remaining;
        bool stopForLoop = false;
        bool stopForTrackEvent = false;
        bool stopForHanging = false;

        if (hasLoop) {
            const uint32_t loopDelta = loopTick - currentTick;
            if (loopDelta <= step) {
                step = loopDelta;
                stopForLoop = true;
            }
        }

        if (hasTrackEvent) {
            const uint32_t eventDelta = nextTrackEventTick - currentTick;
            if (eventDelta < step || (!stopForLoop && eventDelta == step)) {
                step = eventDelta;
                stopForLoop = false;
                stopForTrackEvent = true;
            }
        } else if (!hasLoop && endTick > currentTick && (endTick - currentTick) < step) {
            step = endTick - currentTick;
        }

        if (hasHanging && nextHangingTick <= step) {
            if (nextHangingTick < step || (!stopForTrackEvent && !stopForLoop)) {
                step = nextHangingTick;
                stopForLoop = false;
                stopForTrackEvent = false;
            }
            stopForHanging = true;
        }

        if (step > 0) {
            advanceHangingNotes(sound, step);
            setAbsoluteTick(sound, currentTick + step);
            remaining -= step;
        }

        sound = findActiveSound(soundId);
        if (!sound) {
            return false;
        }

        if (stopForHanging) {
            emitExpiredHangingNotes(sound);
            if (!findActiveSound(soundId)) {
                return false;
            }
        }

        if (stopForLoop && sound->loopCounter > 0) {
            sound->loopCounter--;
            if (isScanActiveFor(soundId)) {
                jumpTo(sound, sound->trackIndex, sound->loopToBeat, sound->loopToTick);
            } else {
                maybeJump(sound, 0, sound->trackIndex, sound->loopToBeat, sound->loopToTick);
            }
            continue;
        }

        if (stopForTrackEvent) {
            if (!dispatchTrackTick(soundId)) {
                return findActiveSound(soundId) != nullptr;
            }
            continue;
        }

        if (step == 0) {
            if (!hasTrackEvent && !hasLoop && !hasHanging) {
                if (isScanActiveFor(soundId)) {
                    break;
                }
                stopSound(soundId);
                return false;
            }
            break;
        }
    }

    return findActiveSound(soundId) != nullptr;
}

int32_t ImuseEngine::doCommand(const CommandPacket &packet) {
    return doCommand(packet.argc, packet.args.data());
}

int ImuseEngine::dispatchGlobalCommand(uint8_t cmd, uint16_t argc, const int16_t *args) {
    switch (cmd) {
    case 6:
        if (argc < 2 || args[1] < 0 || args[1] > 127) {
            return -1;
        }
        _masterVolume = static_cast<uint16_t>((args[1] << 1) | (args[1] ? 0 : 1));
        for (auto &entry : _activeSounds) {
            refreshEffectiveVolume(&entry.second);
            for (PartState &part : entry.second.parts) {
                if (part.initialized) {
                    applyPartVolume(&entry.second, &part);
                }
            }
        }
        return 0;
    case 7:
        return _masterVolume / 2;
    case 8:
        return (argc >= 2 && startSound(static_cast<uint16_t>(args[1]))) ? 0 : -1;
    case 9:
        if (argc >= 2) {
            stopSound(static_cast<uint16_t>(args[1]));
            return 0;
        }
        return -1;
    case 10:
    case 11:
        stopAllSounds();
        return 0;
    case 13:
        return (argc >= 2) ? getSoundStatus(static_cast<uint16_t>(args[1])) : -1;
    case 16:
        if (argc >= 3 && args[2] >= 0) {
            ActiveSound *sound = findActiveSound(static_cast<uint16_t>(args[1]));
            if (!sound) {
                return -1;
            }
            sound->volChan = static_cast<uint16_t>(args[2]);
            refreshEffectiveVolume(sound);
            return 0;
        }
        return -1;
    case 17:
        if (argc >= 5) {
            if (args[4] == 0) {
                return clearScriptTrigger(args[1], args[3]);
            }
            const CommandPacket command = MakeCommandPacket(argc, args, 4);
            return setScriptTrigger(static_cast<uint16_t>(args[1]), static_cast<uint8_t>(args[3]), command);
        }
        if (argc >= 3 && args[1] >= 0 && args[1] < static_cast<int>(_channelVolume.size()) && args[2] >= 0 && args[2] <= 127) {
            _channelVolume[args[1]] = static_cast<uint8_t>(args[2]);
            for (auto &entry : _activeSounds) {
                refreshEffectiveVolume(&entry.second);
                for (PartState &part : entry.second.parts) {
                    if (part.initialized) {
                        applyPartVolume(&entry.second, &part);
                    }
                }
            }
            return 0;
        }
        return -1;
    case 18:
        if (argc >= 4) {
            return checkScriptTrigger(static_cast<uint16_t>(args[1]), args[3]);
        }
        if (argc >= 3 && args[1] >= 0 && args[1] < static_cast<int>(_volchanTable.size()) && args[2] >= 0) {
            _volchanTable[args[1]] = static_cast<uint16_t>(args[2]);
            return 0;
        }
        return -1;
    case 19:
        return (argc >= 4) ? clearScriptTrigger(args[1], args[3]) : -1;
    case 20:
        if (argc < 3 || args[1] < 0) {
            return -1;
        }
        addDeferredCommand(static_cast<uint32_t>(args[1]), MakeCommandPacket(argc, args, 2));
        return 0;
    default:
        return -1;
    }
}

int ImuseEngine::dispatchPlayerCommand(uint8_t cmd, uint16_t argc, const int16_t *args) {
    if (argc < 2) {
        return -1;
    }

    ActiveSound *sound = findActiveSound(static_cast<uint16_t>(args[1]));
    if (!sound) {
        return -1;
    }

    switch (cmd) {
    case 0:
        return (argc >= 4) ? getSoundParam(*sound, args[2], static_cast<uint8_t>(args[3])) : -1;
    case 1:
        if (argc < 3 || args[2] < 0 || args[2] > 255) {
            return -1;
        }
        sound->priority = static_cast<uint8_t>(args[2]);
        for (std::size_t channelIndex = 0; channelIndex < sound->parts.size(); ++channelIndex) {
            if (PartState *part = getActivePart(sound, static_cast<uint8_t>(channelIndex))) {
                part->priorityEffective = static_cast<uint8_t>(ClampInt(part->priority + sound->priority, 0, 255));
            }
        }
        return 0;
    case 2:
        if (argc < 3 || args[2] < 0 || args[2] > 127) {
            return -1;
        }
        sound->volume = static_cast<uint8_t>(args[2]);
        refreshEffectiveVolume(sound);
        for (PartState &part : sound->parts) {
            if (part.initialized) {
                applyPartVolume(sound, &part);
            }
        }
        return 0;
    case 3:
        if (argc < 3) {
            return -1;
        }
        sound->pan = static_cast<int8_t>(ClampInt(args[2], -128, 127));
        for (PartState &part : sound->parts) {
            if (part.initialized) {
                applyPartPan(sound, &part);
            }
        }
        return 0;
    case 4:
        return (argc >= 4) ? setSoundTranspose(sound, static_cast<uint8_t>(args[2]), args[3]) : -1;
    case 5:
        if (argc < 3) {
            return -1;
        }
        sound->detune = static_cast<int16_t>(args[2]);
        for (PartState &part : sound->parts) {
            if (part.initialized) {
                part.detuneEffective = static_cast<int16_t>(ClampInt(part.detune + sound->detune, -128, 127));
            }
        }
        return 0;
    case 6:
        if (argc < 3 || args[2] < 0 || args[2] > 255) {
            return -1;
        }
        sound->speed = static_cast<uint8_t>(args[2]);
        return 0;
    case 7:
        if (argc < 5 || args[3] == 0 || normalizeTrackIndex(*sound, static_cast<uint16_t>(args[2])) == kInvalidTrackIndex) {
            return -1;
        }
        return maybeJump(sound, 0, static_cast<uint16_t>(args[2]), static_cast<uint16_t>(args[3]), static_cast<uint16_t>(args[4])) ? 0 : -1;
    case 8:
        if (argc < 5 || normalizeTrackIndex(*sound, static_cast<uint16_t>(args[2])) == kInvalidTrackIndex) {
            return -1;
        }
        return scanTo(sound, static_cast<uint16_t>(args[2]), static_cast<uint16_t>(args[3]), static_cast<uint16_t>(args[4]));
    case 9:
        return (argc >= 7 && setLoop(sound, static_cast<uint16_t>(args[2]), static_cast<uint16_t>(args[3]), static_cast<uint16_t>(args[4]), static_cast<uint16_t>(args[5]), static_cast<uint16_t>(args[6]))) ? 0 : -1;
    case 10:
        sound->loopCounter = 0;
        return 0;
    case 11:
        if (argc < 4 || !IsPartChannel(args[2])) {
            return -1;
        }
        partSetOnOff(sound, static_cast<uint8_t>(args[2]), args[3] != 0);
        return 0;
    case 12:
    case 20:
        if (argc < 5 || args[2] < 0 || args[2] > 255 || args[3] < 0 || args[3] > 255 || args[4] < 0 || args[4] > 16) {
            return -1;
        }
        return sound->hook.set(static_cast<uint8_t>(args[2]), static_cast<uint8_t>(args[3]), static_cast<uint8_t>(args[4]));
    case 13:
        if (argc < 4) {
            return -1;
        }
        sound->fade.active = args[3] != 0;
        sound->fade.param = 1;
        sound->fade.target = args[2];
        sound->fade.time = args[3];
        if (args[3] == 0) {
            sound->volume = static_cast<uint8_t>(ClampInt(args[2], 0, 127));
            refreshEffectiveVolume(sound);
        }
        return 0;
    case 14:
        return (argc >= 3) ? enqueueTrigger(static_cast<uint16_t>(args[1]), static_cast<uint8_t>(args[2])) : -1;
    case 15:
        return enqueueCommand(argc, args);
    case 16:
        return clearQueue();
    case 19:
        return (argc >= 4) ? getSoundParam(*sound, args[2], static_cast<uint8_t>(args[3])) : -1;
    case 22:
        if (argc < 4 || !IsPartChannel(args[2]) || args[3] < 0 || args[3] > 127) {
            return -1;
        }
        partSetVolume(sound, static_cast<uint8_t>(args[2]), static_cast<uint8_t>(args[3]));
        return 0;
    case 23:
        return (argc >= 3) ? queryQueue(static_cast<uint16_t>(args[2])) : -1;
    default:
        return -1;
    }
}

int32_t ImuseEngine::doCommand(uint16_t argc, const int16_t *args) {
    if (!args || argc < 1) {
        return -1;
    }

    const uint16_t opcode = static_cast<uint16_t>(args[0]);
    const uint8_t param = static_cast<uint8_t>(opcode >> 8);
    const uint8_t cmd = static_cast<uint8_t>(opcode & 0xFFu);

    if (param == 0) {
        return dispatchGlobalCommand(cmd, argc, args);
    }
    if (param == 1) {
        return dispatchPlayerCommand(cmd, argc, args);
    }
    return -1;
}

void ImuseEngine::setTargetProfile(TargetProfile profile) {
    if (_profile != profile) {
        _profile = profile;
        if (_profile == TargetProfile::Mt32 && _midiSink) {
            initMt32();
        }
    }
}

void ImuseEngine::setMidiSink(MidiSink *sink) {
    _midiSink = sink;
    if (_profile == TargetProfile::Mt32 && _midiSink) {
        initMt32();
    }
}

void ImuseEngine::initMt32() {
    if (!_midiSink) return;
    
    // 1. Reset MT-32
    sendMt32Sysex(0, 0x1FC000, nullptr, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    
    // 2. System Area init (initSysex1)
    static const uint8_t initSysex1[] = {
        0x40, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x64
    };
    sendMt32Sysex(0, 0x40000, initSysex1, sizeof(initSysex1));
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    
    // 3. Percussion mapping (initSysex2)
    static const uint8_t initSysex2[] = {
        0x40, 0x64, 0x07, 0x00, 0x4a, 0x64, 0x06, 0x00, 0x41, 0x64, 0x07, 0x00,
        0x4b, 0x64, 0x08, 0x00, 0x45, 0x64, 0x06, 0x00, 0x44, 0x64, 0x0b, 0x00,
        0x51, 0x64, 0x05, 0x00, 0x43, 0x64, 0x08, 0x00, 0x50, 0x64, 0x07, 0x00,
        0x42, 0x64, 0x03, 0x00, 0x4c, 0x64, 0x07, 0x00
    };
    sendMt32Sysex(0, 0xC090, initSysex2, sizeof(initSysex2));
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    
    // 4. Pitch bend range (2 semitones = 0x10) for all 128 patches
    const uint8_t pbRange = 0x10;
    for (int i = 0; i < 128; ++i) {
        sendMt32Sysex(0, 0x014004 + (i << 3), &pbRange, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    // 5. Reset all 16 MIDI channels
    for (uint8_t i = 0; i < 16; ++i) {
        emitMidiMessage(0, 0xC0 | i, 0, false, 0);       // PC 0
        emitMidiMessage(0, 0xB0 | i, 64, true, 0);      // Sustain off
        emitMidiMessage(0, 0xB0 | i, 123, true, 0);     // All Notes Off
        emitMidiMessage(0, 0xB0 | i, 10, true, 0x3F);    // Pan center
        emitMidiMessage(0, 0xE0 | i, 0, true, 0x40);     // Pitch bend center
    }

    // 6. Display welcome message (padded/truncated to 20 chars)
    std::string msg = _welcomeMessage;
    if (msg.size() < 20) {
        msg.append(20 - msg.size(), ' ');
    } else if (msg.size() > 20) {
        msg = msg.substr(0, 20);
    }
    sendMt32Sysex(0, 0x80000, reinterpret_cast<const uint8_t*>(msg.c_str()), 20);
}

} // namespace imuse
