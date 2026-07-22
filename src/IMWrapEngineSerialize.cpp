#include "imwrap/IMWrapEngine.h"
#include "imwrap/ResourceBank.h"
#include "imwrap/IMWrapSequence.h"
#include <iostream>

namespace imwrap {

extern bool LoadIMWrapSequence(const SoundVariantView &variant, IMWrapSequence *seq, std::string *error,
                               IMWrapSysexDialect dialect);

namespace {

const uint32_t kMagic = 0x53574D49; // "IMWS"
const uint32_t kVersion = 3;

template <typename T>
bool writeVal(std::ostream &os, const T &val) {
    os.write(reinterpret_cast<const char *>(&val), sizeof(T));
    return os.good();
}

template <typename T>
bool readVal(std::istream &is, T &val) {
    is.read(reinterpret_cast<char *>(&val), sizeof(T));
    return is.good();
}

bool writeStr(std::ostream &os, const std::string &str) {
    uint32_t len = static_cast<uint32_t>(str.length());
    if (!writeVal(os, len)) return false;
    if (len > 0) {
        os.write(str.data(), len);
    }
    return os.good();
}

bool readStr(std::istream &is, std::string &str, uint32_t maxLen = 1048576) {
    uint32_t len = 0;
    if (!readVal(is, len)) return false;
    if (len > maxLen) {
        is.setstate(std::ios::failbit);
        return false;
    }
    if (len > 0) {
        str.resize(len);
        is.read(&str[0], len);
    } else {
        str.clear();
    }
    return is.good();
}

} // namespace

bool IMWrapEngine::Serialize(std::ostream &os) const {
    if (!writeVal(os, kMagic)) return false;
    if (!writeVal(os, kVersion)) return false;

    if (!writeVal(os, _masterVolume)) return false;
    
    // v3
    uint8_t targetProfileRaw = static_cast<uint8_t>(_profile);
    if (!writeVal(os, targetProfileRaw)) return false;
    uint8_t compatibilityProfileRaw = static_cast<uint8_t>(_compatibility);
    if (!writeVal(os, compatibilityProfileRaw)) return false;
    if (!writeVal(os, _nativeMt32Output)) return false;

    if (!writeStr(os, _welcomeMessage)) return false;

    uint32_t activeSoundCount = static_cast<uint32_t>(_activeSounds.size());
    if (!writeVal(os, activeSoundCount)) return false;
    
    for (const auto &pair : _activeSounds) {
        if (!writeVal(os, pair.first)) return false; // soundId
        const ActiveSound &snd = pair.second;
        if (!writeVal(os, snd.soundId)) return false;
        if (!writeVal(os, snd.isMidi)) return false;
        if (!writeVal(os, snd.isMt32)) return false;
        if (!writeVal(os, snd.supportsPercussion)) return false;
        if (!writeVal(os, snd.priority)) return false;
        if (!writeVal(os, snd.volume)) return false;
        if (!writeVal(os, snd.effectiveVolume)) return false;
        if (!writeVal(os, snd.pan)) return false;
        if (!writeVal(os, snd.transpose)) return false;
        if (!writeVal(os, snd.detune)) return false;
        if (!writeVal(os, snd.speed)) return false;
        if (!writeVal(os, snd.tempoMicrosPerQuarter)) return false;
        if (!writeVal(os, snd.trackIndex)) return false;
        if (!writeVal(os, snd.beatIndex)) return false;
        if (!writeVal(os, snd.tickIndex)) return false;
        if (!writeVal(os, snd.loopCounter)) return false;
        if (!writeVal(os, snd.loopToBeat)) return false;
        if (!writeVal(os, snd.loopToTick)) return false;
        if (!writeVal(os, snd.loopFromBeat)) return false;
        if (!writeVal(os, snd.loopFromTick)) return false;
        if (!writeVal(os, snd.volChan)) return false;
        if (!writeVal(os, snd.tickAccumulator)) return false;
        if (!writeVal(os, snd.pendingImmediateEvents)) return false;
        if (!writeVal(os, static_cast<uint32_t>(snd.nextEventHint))) return false;

        for (int i = 0; i < 16; ++i) {
            const PartState &part = snd.parts[i];
            if (!writeVal(os, part.initialized)) return false;
            if (!writeVal(os, part.present)) return false;
            if (!writeVal(os, part.transmitted)) return false;
            if (!writeVal(os, part.on)) return false;
            if (!writeVal(os, part.percussion)) return false;
            bool dummyUnassigned = false;
            if (!writeVal(os, dummyUnassigned)) return false;
            if (!writeVal(os, part.channel)) return false;
            if (!writeVal(os, part.volume)) return false;
            if (!writeVal(os, part.effectiveVolume)) return false;
            if (!writeVal(os, part.program)) return false;
            if (!writeVal(os, part.instrumentValue)) return false;
            if (!writeVal(os, part.pitchBend)) return false;
            if (!writeVal(os, part.pitchBendFactor)) return false;
            if (!writeVal(os, part.volControlSensitivity)) return false;
            if (!writeVal(os, part.transpose)) return false;
            if (!writeVal(os, part.transposeEffective)) return false;
            if (!writeVal(os, part.detune)) return false;
            if (!writeVal(os, part.detuneEffective)) return false;
            if (!writeVal(os, part.pan)) return false;
            if (!writeVal(os, part.panEffective)) return false;
            if (!writeVal(os, part.polyphony)) return false;
            if (!writeVal(os, part.modwheel)) return false;
            if (!writeVal(os, part.pedal)) return false;
            if (!writeVal(os, part.priority)) return false;
            if (!writeVal(os, part.priorityEffective)) return false;
            if (!writeVal(os, part.effectLevel)) return false;
            if (!writeVal(os, part.chorus)) return false;
            if (!writeVal(os, part.bank)) return false;
            if (!writeVal(os, part.outputChannel)) return false;
            if (!writeVal(os, part.ownerSoundId)) return false;
            if (!writeVal(os, part.currentTimbreIndex)) return false;
            
            os.write(reinterpret_cast<const char*>(part.sourceNotes.data()), part.sourceNotes.size());
            os.write(reinterpret_cast<const char*>(part.sourceOutputNotes.data()), part.sourceOutputNotes.size());
            os.write(reinterpret_cast<const char*>(part.sourceOutputChannels.data()), part.sourceOutputChannels.size());
            os.write(reinterpret_cast<const char*>(part.sourceVelocities.data()), part.sourceVelocities.size());
            
            uint32_t sysexLen = static_cast<uint32_t>(part.customRolandSysex.size());
            if (!writeVal(os, sysexLen)) return false;
            if (sysexLen > 0) {
                os.write(reinterpret_cast<const char*>(part.customRolandSysex.data()), sysexLen);
            }
        }
        
        for (int i = 0; i < 16; ++i) {
            const MidiChannelState &mc = snd.midiChannels[i];
            os.write(reinterpret_cast<const char*>(mc.heldNotes.data()), mc.heldNotes.size());
            os.write(reinterpret_cast<const char*>(mc.soundingNotes.data()), mc.soundingNotes.size());
            if (!writeVal(os, mc.sustainValue)) return false;
        }

        uint32_t hangingCount = 0; // TODO: Serialize hanging notes if needed later
        if (!writeVal(os, hangingCount)) return false;

        os.write(reinterpret_cast<const char*>(snd.hook.jump.data()), snd.hook.jump.size());
        if (!writeVal(os, snd.hook.transpose)) return false;
        os.write(reinterpret_cast<const char*>(snd.hook.partOnOff.data()), snd.hook.partOnOff.size());
        os.write(reinterpret_cast<const char*>(snd.hook.partVolume.data()), snd.hook.partVolume.size());
        os.write(reinterpret_cast<const char*>(snd.hook.partProgram.data()), snd.hook.partProgram.size());
        os.write(reinterpret_cast<const char*>(snd.hook.partTranspose.data()), snd.hook.partTranspose.size());

        if (!writeVal(os, snd.fade.active)) return false;
        if (!writeVal(os, snd.fade.param)) return false;
        if (!writeVal(os, snd.fade.target)) return false;
        if (!writeVal(os, snd.fade.time)) return false;
    }
    
    uint32_t tqCount = static_cast<uint32_t>(_triggerQueue.size());
    if (!writeVal(os, tqCount)) return false;
    for (const auto &tq : _triggerQueue) {
        if (!writeVal(os, tq.soundId)) return false;
        if (!writeVal(os, tq.marker)) return false;
        if (!writeVal(os, tq.finalized)) return false;
        uint32_t cmdCount = static_cast<uint32_t>(tq.commands.size());
        if (!writeVal(os, cmdCount)) return false;
        if (cmdCount > 0) {
            os.write(reinterpret_cast<const char*>(tq.commands.data()), cmdCount * sizeof(CommandPacket));
        }
    }
    
    uint32_t stCount = static_cast<uint32_t>(_scriptTriggers.size());
    if (!writeVal(os, stCount)) return false;
    if (stCount > 0) {
        os.write(reinterpret_cast<const char*>(_scriptTriggers.data()), stCount * sizeof(ScriptTrigger));
    }
    
    uint32_t dcCount = static_cast<uint32_t>(_deferredCommands.size());
    if (!writeVal(os, dcCount)) return false;
    if (dcCount > 0) {
        os.write(reinterpret_cast<const char*>(_deferredCommands.data()), dcCount * sizeof(DeferredCommand));
    }
    
    if (!writeVal(os, _rhyState.vol)) return false;
    if (!writeVal(os, _rhyState.poly)) return false;
    if (!writeVal(os, _rhyState.prio)) return false;
    
    return os.good();
}

IMWrapEngine::DeserializeResult IMWrapEngine::Deserialize(std::istream &is) {
    uint32_t magic = 0;
    if (!readVal(is, magic)) return DeserializeResult::IOError;
    if (magic != kMagic) return DeserializeResult::InvalidFormat;

    uint32_t version = 0;
    if (!readVal(is, version)) return DeserializeResult::IOError;
    if (version > kVersion) return DeserializeResult::UnsupportedVersion;

    uint16_t masterVolume = 255;
    uint8_t targetProfileRaw = static_cast<uint8_t>(TargetProfile::GeneralMidi);
    uint8_t compatibilityProfileRaw = static_cast<uint8_t>(CompatibilityProfile::GenericV6);
    bool nativeMt32Output = false;
    std::string welcomeMessage;

    if (version >= 2) {
        if (!readVal(is, masterVolume)) return DeserializeResult::IOError;
    }
    
    if (version >= 3) {
        if (!readVal(is, targetProfileRaw)) return DeserializeResult::IOError;
        if (!readVal(is, compatibilityProfileRaw)) return DeserializeResult::IOError;
        if (!readVal(is, nativeMt32Output)) return DeserializeResult::IOError;
    }

    if (!readStr(is, welcomeMessage)) return DeserializeResult::CorruptedData;

    TargetProfile localProfile = (version >= 3) ? static_cast<TargetProfile>(targetProfileRaw) : _profile;
    CompatibilityProfile localCompat = (version >= 3) ? static_cast<CompatibilityProfile>(compatibilityProfileRaw) : _compatibility;

    uint32_t activeSoundCount = 0;
    if (!readVal(is, activeSoundCount)) return DeserializeResult::IOError;
    if (activeSoundCount > 256) return DeserializeResult::CorruptedData;

    std::map<uint16_t, ActiveSound> tempActiveSounds;
    bool hasMissingResources = false;

    for (uint32_t s = 0; s < activeSoundCount; ++s) {
        uint16_t keyId = 0;
        if (!readVal(is, keyId)) return DeserializeResult::IOError;
        
        ActiveSound snd;
        if (!readVal(is, snd.soundId)) return DeserializeResult::IOError;
        if (!readVal(is, snd.isMidi)) return DeserializeResult::IOError;
        if (!readVal(is, snd.isMt32)) return DeserializeResult::IOError;
        if (!readVal(is, snd.supportsPercussion)) return DeserializeResult::IOError;
        if (!readVal(is, snd.priority)) return DeserializeResult::IOError;
        if (!readVal(is, snd.volume)) return DeserializeResult::IOError;
        if (!readVal(is, snd.effectiveVolume)) return DeserializeResult::IOError;
        if (!readVal(is, snd.pan)) return DeserializeResult::IOError;
        if (!readVal(is, snd.transpose)) return DeserializeResult::IOError;
        if (!readVal(is, snd.detune)) return DeserializeResult::IOError;
        if (!readVal(is, snd.speed)) return DeserializeResult::IOError;
        if (!readVal(is, snd.tempoMicrosPerQuarter)) return DeserializeResult::IOError;
        if (!readVal(is, snd.trackIndex)) return DeserializeResult::IOError;
        if (!readVal(is, snd.beatIndex)) return DeserializeResult::IOError;
        if (!readVal(is, snd.tickIndex)) return DeserializeResult::IOError;
        if (!readVal(is, snd.loopCounter)) return DeserializeResult::IOError;
        if (!readVal(is, snd.loopToBeat)) return DeserializeResult::IOError;
        if (!readVal(is, snd.loopToTick)) return DeserializeResult::IOError;
        if (!readVal(is, snd.loopFromBeat)) return DeserializeResult::IOError;
        if (!readVal(is, snd.loopFromTick)) return DeserializeResult::IOError;
        if (!readVal(is, snd.volChan)) return DeserializeResult::IOError;
        if (!readVal(is, snd.tickAccumulator)) return DeserializeResult::IOError;
        if (!readVal(is, snd.pendingImmediateEvents)) return DeserializeResult::IOError;
        uint32_t nextHint = 0;
        if (!readVal(is, nextHint)) return DeserializeResult::IOError;
        snd.nextEventHint = nextHint;

        for (int i = 0; i < 16; ++i) {
            PartState &part = snd.parts[i];
            if (!readVal(is, part.initialized)) return DeserializeResult::IOError;
            if (!readVal(is, part.present)) return DeserializeResult::IOError;
            if (!readVal(is, part.transmitted)) return DeserializeResult::IOError;
            if (!readVal(is, part.on)) return DeserializeResult::IOError;
            if (!readVal(is, part.percussion)) return DeserializeResult::IOError;
            bool dummyUnassigned = false;
            if (!readVal(is, dummyUnassigned)) return DeserializeResult::IOError;
            if (!readVal(is, part.channel)) return DeserializeResult::IOError;
            if (!readVal(is, part.volume)) return DeserializeResult::IOError;
            if (!readVal(is, part.effectiveVolume)) return DeserializeResult::IOError;
            if (!readVal(is, part.program)) return DeserializeResult::IOError;
            if (!readVal(is, part.instrumentValue)) return DeserializeResult::IOError;
            if (!readVal(is, part.pitchBend)) return DeserializeResult::IOError;
            if (!readVal(is, part.pitchBendFactor)) return DeserializeResult::IOError;
            if (!readVal(is, part.volControlSensitivity)) return DeserializeResult::IOError;
            if (!readVal(is, part.transpose)) return DeserializeResult::IOError;
            if (!readVal(is, part.transposeEffective)) return DeserializeResult::IOError;
            if (!readVal(is, part.detune)) return DeserializeResult::IOError;
            if (!readVal(is, part.detuneEffective)) return DeserializeResult::IOError;
            if (!readVal(is, part.pan)) return DeserializeResult::IOError;
            if (!readVal(is, part.panEffective)) return DeserializeResult::IOError;
            if (!readVal(is, part.polyphony)) return DeserializeResult::IOError;
            if (!readVal(is, part.modwheel)) return DeserializeResult::IOError;
            if (!readVal(is, part.pedal)) return DeserializeResult::IOError;
            if (!readVal(is, part.priority)) return DeserializeResult::IOError;
            if (!readVal(is, part.priorityEffective)) return DeserializeResult::IOError;
            if (!readVal(is, part.effectLevel)) return DeserializeResult::IOError;
            if (!readVal(is, part.chorus)) return DeserializeResult::IOError;
            if (!readVal(is, part.bank)) return DeserializeResult::IOError;
            if (!readVal(is, part.outputChannel)) return DeserializeResult::IOError;
            if (!readVal(is, part.ownerSoundId)) return DeserializeResult::IOError;
            if (!readVal(is, part.currentTimbreIndex)) return DeserializeResult::IOError;
            
            is.read(reinterpret_cast<char*>(part.sourceNotes.data()), part.sourceNotes.size());
            is.read(reinterpret_cast<char*>(part.sourceOutputNotes.data()), part.sourceOutputNotes.size());
            is.read(reinterpret_cast<char*>(part.sourceOutputChannels.data()), part.sourceOutputChannels.size());
            is.read(reinterpret_cast<char*>(part.sourceVelocities.data()), part.sourceVelocities.size());
            
            uint32_t sysexLen = 0;
            if (!readVal(is, sysexLen)) return DeserializeResult::IOError;
            if (sysexLen > 65536) return DeserializeResult::CorruptedData;
            if (sysexLen > 0) {
                part.customRolandSysex.resize(sysexLen);
                is.read(reinterpret_cast<char*>(part.customRolandSysex.data()), sysexLen);
            }
            if (is.fail()) return DeserializeResult::IOError;
        }
        
        for (int i = 0; i < 16; ++i) {
            MidiChannelState &mc = snd.midiChannels[i];
            is.read(reinterpret_cast<char*>(mc.heldNotes.data()), mc.heldNotes.size());
            is.read(reinterpret_cast<char*>(mc.soundingNotes.data()), mc.soundingNotes.size());
            if (!readVal(is, mc.sustainValue)) return DeserializeResult::IOError;
        }

        uint32_t hangingCount = 0;
        if (!readVal(is, hangingCount)) return DeserializeResult::IOError;
        if (hangingCount > 65536) return DeserializeResult::CorruptedData;
        // Skip hanging notes
        if (hangingCount > 0) {
            is.ignore(hangingCount * sizeof(HangingNote));
        }

        is.read(reinterpret_cast<char*>(snd.hook.jump.data()), snd.hook.jump.size());
        if (!readVal(is, snd.hook.transpose)) return DeserializeResult::IOError;
        is.read(reinterpret_cast<char*>(snd.hook.partOnOff.data()), snd.hook.partOnOff.size());
        is.read(reinterpret_cast<char*>(snd.hook.partVolume.data()), snd.hook.partVolume.size());
        is.read(reinterpret_cast<char*>(snd.hook.partProgram.data()), snd.hook.partProgram.size());
        is.read(reinterpret_cast<char*>(snd.hook.partTranspose.data()), snd.hook.partTranspose.size());

        if (!readVal(is, snd.fade.active)) return DeserializeResult::IOError;
        if (!readVal(is, snd.fade.param)) return DeserializeResult::IOError;
        if (!readVal(is, snd.fade.target)) return DeserializeResult::IOError;
        if (!readVal(is, snd.fade.time)) return DeserializeResult::IOError;
        snd.fade.currentVolume = static_cast<double>(snd.volume);

        if (is.fail()) return DeserializeResult::IOError;

        if (_bank && _bank->hasSound(snd.soundId)) {
            SoundResource resource = _bank->loadSound(snd.soundId, nullptr);
            auto variant = resource.selectVariant(localProfile);
            if (variant.valid()) {
                snd.variant = variant;
                const IMWrapSysexDialect dialect =
                    (localCompat == CompatibilityProfile::Snm) ? IMWrapSysexDialect::Snm : IMWrapSysexDialect::GenericV6;
                if (LoadIMWrapSequence(snd.variant, &snd.sequence, nullptr, dialect)) {
                    snd.isMidi = true;
                    snd.isMt32 = (localProfile == TargetProfile::Mt32);
                    snd.supportsPercussion = true;
                    tempActiveSounds[keyId] = std::move(snd);
                } else {
                    hasMissingResources = true;
                }
            } else {
                hasMissingResources = true;
            }
        } else {
            hasMissingResources = true;
        }
    }
    
    uint32_t tqCount = 0;
    if (!readVal(is, tqCount)) return DeserializeResult::IOError;
    if (tqCount > 4096) return DeserializeResult::CorruptedData;
    std::deque<QueuedTrigger> tempTriggerQueue(tqCount);
    for (uint32_t i = 0; i < tqCount; ++i) {
        auto &tq = tempTriggerQueue[i];
        if (!readVal(is, tq.soundId)) return DeserializeResult::IOError;
        if (!readVal(is, tq.marker)) return DeserializeResult::IOError;
        if (!readVal(is, tq.finalized)) return DeserializeResult::IOError;
        uint32_t cmdCount = 0;
        if (!readVal(is, cmdCount)) return DeserializeResult::IOError;
        if (cmdCount > 4096) return DeserializeResult::CorruptedData;
        if (cmdCount > 0) {
            tq.commands.resize(cmdCount);
            is.read(reinterpret_cast<char*>(tq.commands.data()), cmdCount * sizeof(CommandPacket));
        }
        if (is.fail()) return DeserializeResult::IOError;
    }
    
    uint32_t stCount = 0;
    if (!readVal(is, stCount)) return DeserializeResult::IOError;
    if (stCount > 4096) return DeserializeResult::CorruptedData;
    std::vector<ScriptTrigger> tempScriptTriggers;
    if (stCount > 0) {
        tempScriptTriggers.resize(stCount);
        is.read(reinterpret_cast<char*>(tempScriptTriggers.data()), stCount * sizeof(ScriptTrigger));
    }
    if (is.fail()) return DeserializeResult::IOError;
    
    uint32_t dcCount = 0;
    if (!readVal(is, dcCount)) return DeserializeResult::IOError;
    if (dcCount > 4096) return DeserializeResult::CorruptedData;
    std::vector<DeferredCommand> tempDeferredCommands;
    if (dcCount > 0) {
        tempDeferredCommands.resize(dcCount);
        is.read(reinterpret_cast<char*>(tempDeferredCommands.data()), dcCount * sizeof(DeferredCommand));
    }
    if (is.fail()) return DeserializeResult::IOError;
    
    RhythmState tempRhyState;
    if (!readVal(is, tempRhyState.vol)) return DeserializeResult::IOError;
    if (!readVal(is, tempRhyState.poly)) return DeserializeResult::IOError;
    if (!readVal(is, tempRhyState.prio)) return DeserializeResult::IOError;
    
    // Check if everything was read successfully before committing
    if (!is.good()) return DeserializeResult::IOError;

    // Transaction Success! Apply state.
    stopAllSounds();
    _activeSounds.clear();
    _triggerQueue.clear();
    _scriptTriggers.clear();
    _deferredCommands.clear();
    _waitingPartsQueue.clear();
    for(auto &owner : _physicalChannelOwners) {
        owner = nullptr;
    }

    _masterVolume = masterVolume;
    if (version >= 3) {
        _profile = static_cast<TargetProfile>(targetProfileRaw);
        _compatibility = static_cast<CompatibilityProfile>(compatibilityProfileRaw);
        _nativeMt32Output = nativeMt32Output;
    }
    _welcomeMessage = std::move(welcomeMessage);
    _activeSounds = std::move(tempActiveSounds);
    _triggerQueue = std::move(tempTriggerQueue);
    _scriptTriggers = std::move(tempScriptTriggers);
    _deferredCommands = std::move(tempDeferredCommands);
    _rhyState = tempRhyState;

    // Fix pointers now that things are moved
    for (auto &pair : _activeSounds) {
        ActiveSound *activeSoundPtr = &pair.second;
        for (int i = 0; i < 16; ++i) {
            PartState &part = activeSoundPtr->parts[i];
            if (part.initialized && part.outputChannel >= 0 && part.outputChannel < 16) {
                _physicalChannelOwners[part.outputChannel] = &part;
            }
        }
    }
    
    for (auto &pair : _activeSounds) {
        ActiveSound *activeSoundPtr = &pair.second;
        for (int i = 0; i < 16; ++i) {
            PartState &part = activeSoundPtr->parts[i];
            if (part.initialized && part.outputChannel >= 0) {
                applyPartAllState(activeSoundPtr, &part);
            }
        }
    }

    return hasMissingResources ? DeserializeResult::SuccessWithMissingResources : DeserializeResult::Success;
}

} // namespace imwrap
