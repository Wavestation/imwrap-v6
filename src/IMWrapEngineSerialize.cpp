#include "imwrap/IMWrapEngine.h"
#include "imwrap/ResourceBank.h"
#include "imwrap/IMWrapSequence.h"
#include <iostream>

namespace imwrap {

extern bool LoadIMWrapSequence(const SoundVariantView &variant, IMWrapSequence *seq, std::string *error,
                               IMWrapSysexDialect dialect);

namespace {

template <typename T>
void writeVal(std::ostream &os, const T &val) {
    os.write(reinterpret_cast<const char *>(&val), sizeof(T));
}

template <typename T>
void readVal(std::istream &is, T &val) {
    is.read(reinterpret_cast<char *>(&val), sizeof(T));
}

void writeStr(std::ostream &os, const std::string &str) {
    uint32_t len = static_cast<uint32_t>(str.length());
    writeVal(os, len);
    if (len > 0) {
        os.write(str.data(), len);
    }
}

void readStr(std::istream &is, std::string &str) {
    uint32_t len = 0;
    readVal(is, len);
    if (len > 0) {
        str.resize(len);
        is.read(&str[0], len);
    } else {
        str.clear();
    }
}

} // namespace

bool IMWrapEngine::Serialize(std::ostream &os) const {
    writeVal(os, _nativeMt32Output);
    writeStr(os, _welcomeMessage);

    // Serialize _activeSounds
    uint32_t activeSoundCount = static_cast<uint32_t>(_activeSounds.size());
    writeVal(os, activeSoundCount);
    for (const auto &pair : _activeSounds) {
        writeVal(os, pair.first); // soundId
        const ActiveSound &snd = pair.second;
        writeVal(os, snd.soundId);
        writeVal(os, snd.isMidi);
        writeVal(os, snd.isMt32);
        writeVal(os, snd.supportsPercussion);
        writeVal(os, snd.priority);
        writeVal(os, snd.volume);
        writeVal(os, snd.effectiveVolume);
        writeVal(os, snd.pan);
        writeVal(os, snd.transpose);
        writeVal(os, snd.detune);
        writeVal(os, snd.speed);
        writeVal(os, snd.tempoMicrosPerQuarter);
        writeVal(os, snd.trackIndex);
        writeVal(os, snd.beatIndex);
        writeVal(os, snd.tickIndex);
        writeVal(os, snd.loopCounter);
        writeVal(os, snd.loopToBeat);
        writeVal(os, snd.loopToTick);
        writeVal(os, snd.loopFromBeat);
        writeVal(os, snd.loopFromTick);
        writeVal(os, snd.volChan);
        writeVal(os, snd.pendingImmediateEvents);
        writeVal(os, static_cast<uint32_t>(snd.nextEventHint));

        // Serialize parts
        for (int i = 0; i < 16; ++i) {
            const PartState &part = snd.parts[i];
            writeVal(os, part.initialized);
            writeVal(os, part.present);
            writeVal(os, part.transmitted);
            writeVal(os, part.on);
            writeVal(os, part.percussion);
            writeVal(os, part.unassignedInstrument);
            writeVal(os, part.channel);
            writeVal(os, part.volume);
            writeVal(os, part.effectiveVolume);
            writeVal(os, part.program);
            writeVal(os, part.instrumentValue);
            writeVal(os, part.pitchBend);
            writeVal(os, part.pitchBendFactor);
            writeVal(os, part.volControlSensitivity);
            writeVal(os, part.transpose);
            writeVal(os, part.transposeEffective);
            writeVal(os, part.detune);
            writeVal(os, part.detuneEffective);
            writeVal(os, part.pan);
            writeVal(os, part.panEffective);
            writeVal(os, part.polyphony);
            writeVal(os, part.modwheel);
            writeVal(os, part.pedal);
            writeVal(os, part.priority);
            writeVal(os, part.priorityEffective);
            writeVal(os, part.effectLevel);
            writeVal(os, part.chorus);
            writeVal(os, part.bank);
            writeVal(os, part.outputChannel);
            writeVal(os, part.ownerSoundId);
            writeVal(os, part.currentTimbreIndex);
            
            // Note arrays
            os.write(reinterpret_cast<const char*>(part.sourceNotes.data()), part.sourceNotes.size());
            os.write(reinterpret_cast<const char*>(part.sourceOutputNotes.data()), part.sourceOutputNotes.size());
            os.write(reinterpret_cast<const char*>(part.sourceOutputChannels.data()), part.sourceOutputChannels.size());
            os.write(reinterpret_cast<const char*>(part.sourceVelocities.data()), part.sourceVelocities.size());
            
            // Custom sysex
            uint32_t sysexLen = static_cast<uint32_t>(part.customRolandSysex.size());
            writeVal(os, sysexLen);
            if (sysexLen > 0) {
                os.write(reinterpret_cast<const char*>(part.customRolandSysex.data()), sysexLen);
            }
        }
        
        // MidiChannels
        for (int i = 0; i < 16; ++i) {
            const MidiChannelState &mc = snd.midiChannels[i];
            os.write(reinterpret_cast<const char*>(mc.heldNotes.data()), mc.heldNotes.size());
            os.write(reinterpret_cast<const char*>(mc.soundingNotes.data()), mc.soundingNotes.size());
            writeVal(os, mc.sustainValue);
        }

        uint32_t hangingCount = 0;
        writeVal(os, hangingCount);

        // Sound HookState
        os.write(reinterpret_cast<const char*>(snd.hook.jump.data()), snd.hook.jump.size());
        writeVal(os, snd.hook.transpose);
        os.write(reinterpret_cast<const char*>(snd.hook.partOnOff.data()), snd.hook.partOnOff.size());
        os.write(reinterpret_cast<const char*>(snd.hook.partVolume.data()), snd.hook.partVolume.size());
        os.write(reinterpret_cast<const char*>(snd.hook.partProgram.data()), snd.hook.partProgram.size());
        os.write(reinterpret_cast<const char*>(snd.hook.partTranspose.data()), snd.hook.partTranspose.size());

        // FadeState
        writeVal(os, snd.fade.active);
        writeVal(os, snd.fade.param);
        writeVal(os, snd.fade.target);
        writeVal(os, snd.fade.time);
    }
    
    // TriggerQueue
    uint32_t tqCount = static_cast<uint32_t>(_triggerQueue.size());
    writeVal(os, tqCount);
    for (const auto &tq : _triggerQueue) {
        writeVal(os, tq.soundId);
        writeVal(os, tq.marker);
        writeVal(os, tq.finalized);
        uint32_t cmdCount = static_cast<uint32_t>(tq.commands.size());
        writeVal(os, cmdCount);
        if (cmdCount > 0) {
            os.write(reinterpret_cast<const char*>(tq.commands.data()), cmdCount * sizeof(CommandPacket));
        }
    }
    
    // ScriptTriggers
    uint32_t stCount = static_cast<uint32_t>(_scriptTriggers.size());
    writeVal(os, stCount);
    if (stCount > 0) {
        os.write(reinterpret_cast<const char*>(_scriptTriggers.data()), stCount * sizeof(ScriptTrigger));
    }
    
    // DeferredCommands
    uint32_t dcCount = static_cast<uint32_t>(_deferredCommands.size());
    writeVal(os, dcCount);
    if (dcCount > 0) {
        os.write(reinterpret_cast<const char*>(_deferredCommands.data()), dcCount * sizeof(DeferredCommand));
    }
    
    // RhythmState
    writeVal(os, _rhyState.vol);
    writeVal(os, _rhyState.poly);
    writeVal(os, _rhyState.prio);
    
    return true;
}

bool IMWrapEngine::Deserialize(std::istream &is) {
    stopAllSounds();
    _activeSounds.clear();
    _triggerQueue.clear();
    _scriptTriggers.clear();
    _deferredCommands.clear();
    _waitingPartsQueue.clear();
    for(auto &owner : _physicalChannelOwners) {
        owner = nullptr;
    }

    readVal(is, _nativeMt32Output);
    readStr(is, _welcomeMessage);

    uint32_t activeSoundCount = 0;
    readVal(is, activeSoundCount);
    for (uint32_t s = 0; s < activeSoundCount; ++s) {
        uint16_t keyId = 0;
        readVal(is, keyId);
        ActiveSound snd;
        readVal(is, snd.soundId);
        readVal(is, snd.isMidi);
        readVal(is, snd.isMt32);
        readVal(is, snd.supportsPercussion);
        readVal(is, snd.priority);
        readVal(is, snd.volume);
        readVal(is, snd.effectiveVolume);
        readVal(is, snd.pan);
        readVal(is, snd.transpose);
        readVal(is, snd.detune);
        readVal(is, snd.speed);
        readVal(is, snd.tempoMicrosPerQuarter);
        readVal(is, snd.trackIndex);
        readVal(is, snd.beatIndex);
        readVal(is, snd.tickIndex);
        readVal(is, snd.loopCounter);
        readVal(is, snd.loopToBeat);
        readVal(is, snd.loopToTick);
        readVal(is, snd.loopFromBeat);
        readVal(is, snd.loopFromTick);
        readVal(is, snd.volChan);
        readVal(is, snd.pendingImmediateEvents);
        uint32_t nextHint = 0;
        readVal(is, nextHint);
        snd.nextEventHint = nextHint;

        for (int i = 0; i < 16; ++i) {
            PartState &part = snd.parts[i];
            readVal(is, part.initialized);
            readVal(is, part.present);
            readVal(is, part.transmitted);
            readVal(is, part.on);
            readVal(is, part.percussion);
            readVal(is, part.unassignedInstrument);
            readVal(is, part.channel);
            readVal(is, part.volume);
            readVal(is, part.effectiveVolume);
            readVal(is, part.program);
            readVal(is, part.instrumentValue);
            readVal(is, part.pitchBend);
            readVal(is, part.pitchBendFactor);
            readVal(is, part.volControlSensitivity);
            readVal(is, part.transpose);
            readVal(is, part.transposeEffective);
            readVal(is, part.detune);
            readVal(is, part.detuneEffective);
            readVal(is, part.pan);
            readVal(is, part.panEffective);
            readVal(is, part.polyphony);
            readVal(is, part.modwheel);
            readVal(is, part.pedal);
            readVal(is, part.priority);
            readVal(is, part.priorityEffective);
            readVal(is, part.effectLevel);
            readVal(is, part.chorus);
            readVal(is, part.bank);
            readVal(is, part.outputChannel);
            readVal(is, part.ownerSoundId);
            readVal(is, part.currentTimbreIndex);
            
            is.read(reinterpret_cast<char*>(part.sourceNotes.data()), part.sourceNotes.size());
            is.read(reinterpret_cast<char*>(part.sourceOutputNotes.data()), part.sourceOutputNotes.size());
            is.read(reinterpret_cast<char*>(part.sourceOutputChannels.data()), part.sourceOutputChannels.size());
            is.read(reinterpret_cast<char*>(part.sourceVelocities.data()), part.sourceVelocities.size());
            
            uint32_t sysexLen = 0;
            readVal(is, sysexLen);
            if (sysexLen > 0) {
                part.customRolandSysex.resize(sysexLen);
                is.read(reinterpret_cast<char*>(part.customRolandSysex.data()), sysexLen);
            }
        }
        
        for (int i = 0; i < 16; ++i) {
            MidiChannelState &mc = snd.midiChannels[i];
            is.read(reinterpret_cast<char*>(mc.heldNotes.data()), mc.heldNotes.size());
            is.read(reinterpret_cast<char*>(mc.soundingNotes.data()), mc.soundingNotes.size());
            readVal(is, mc.sustainValue);
        }

        uint32_t hangingCount = 0;
        readVal(is, hangingCount);

        is.read(reinterpret_cast<char*>(snd.hook.jump.data()), snd.hook.jump.size());
        readVal(is, snd.hook.transpose);
        is.read(reinterpret_cast<char*>(snd.hook.partOnOff.data()), snd.hook.partOnOff.size());
        is.read(reinterpret_cast<char*>(snd.hook.partVolume.data()), snd.hook.partVolume.size());
        is.read(reinterpret_cast<char*>(snd.hook.partProgram.data()), snd.hook.partProgram.size());
        is.read(reinterpret_cast<char*>(snd.hook.partTranspose.data()), snd.hook.partTranspose.size());

        readVal(is, snd.fade.active);
        readVal(is, snd.fade.param);
        readVal(is, snd.fade.target);
        readVal(is, snd.fade.time);

        if (_bank && _bank->hasSound(snd.soundId)) {
            SoundResource resource = _bank->loadSound(snd.soundId, nullptr);
            auto variant = resource.selectVariant(_profile);
            if (variant.valid()) {
                snd.variant = variant;
                const IMWrapSysexDialect dialect =
                    (_compatibility == CompatibilityProfile::Snm) ? IMWrapSysexDialect::Snm : IMWrapSysexDialect::GenericV6;
                LoadIMWrapSequence(snd.variant, &snd.sequence, nullptr, dialect);
                
                for (int i = 0; i < 16; ++i) {
                    PartState &part = snd.parts[i];
                    if (part.initialized && part.outputChannel >= 0 && part.outputChannel < 16) {
                        _physicalChannelOwners[part.outputChannel] = &part;
                    }
                }

                _activeSounds[keyId] = std::move(snd);
                
                ActiveSound *activeSoundPtr = &_activeSounds[keyId];
                for (int i = 0; i < 16; ++i) {
                    PartState &part = activeSoundPtr->parts[i];
                    if (part.initialized && part.outputChannel >= 0) {
                        applyPartAllState(activeSoundPtr, &part);
                    }
                }
            }
        }
    }
    
    uint32_t tqCount = 0;
    readVal(is, tqCount);
    _triggerQueue.resize(tqCount);
    for (uint32_t i = 0; i < tqCount; ++i) {
        auto &tq = _triggerQueue[i];
        readVal(is, tq.soundId);
        readVal(is, tq.marker);
        readVal(is, tq.finalized);
        uint32_t cmdCount = 0;
        readVal(is, cmdCount);
        if (cmdCount > 0) {
            tq.commands.resize(cmdCount);
            is.read(reinterpret_cast<char*>(tq.commands.data()), cmdCount * sizeof(CommandPacket));
        }
    }
    
    uint32_t stCount = 0;
    readVal(is, stCount);
    if (stCount > 0) {
        _scriptTriggers.resize(stCount);
        is.read(reinterpret_cast<char*>(_scriptTriggers.data()), stCount * sizeof(ScriptTrigger));
    }
    
    uint32_t dcCount = 0;
    readVal(is, dcCount);
    if (dcCount > 0) {
        _deferredCommands.resize(dcCount);
        is.read(reinterpret_cast<char*>(_deferredCommands.data()), dcCount * sizeof(DeferredCommand));
    }
    
    readVal(is, _rhyState.vol);
    readVal(is, _rhyState.poly);
    readVal(is, _rhyState.prio);
    
    return true;
}

} // namespace imwrap
