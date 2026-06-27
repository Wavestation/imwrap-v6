#include "SysExToolStateIo.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"

namespace imwrap::vst3tool {
namespace {

constexpr Steinberg::int32 kStateVersion = 2;

bool ReadBool(Steinberg::IBStreamer& streamer, bool* value) {
    Steinberg::int32 raw = 0;
    if (!streamer.readInt32(raw)) {
        return false;
    }

    *value = raw != 0;
    return true;
}

bool WriteBool(Steinberg::IBStreamer& streamer, bool value) {
    return streamer.writeInt32(value ? 1 : 0);
}

} // namespace

bool ReadState(Steinberg::IBStream* stream, SysExToolState* out) {
    if (!stream || !out) {
        return false;
    }

    Steinberg::IBStreamer streamer(stream, kLittleEndian);

    Steinberg::int32 version = 0;
    if (!streamer.readInt32(version) || version < 1 || version > kStateVersion) {
        return false;
    }

    Steinberg::int32 messageType = 0;
    if (!streamer.readInt32(messageType)) {
        return false;
    }

    SysExToolState state;
    state.messageType = static_cast<MessageType>(messageType);

    const bool ok = (version >= 2 ? streamer.readInt32(state.midiOutDeviceIndex)
                                  : ((state.midiOutDeviceIndex = -1), true)) &&
                    streamer.readInt32(state.part) &&
           streamer.readInt32(state.channel) &&
           streamer.readInt32(state.unknown) &&
           ReadBool(streamer, &state.partOn) &&
           ReadBool(streamer, &state.reverb) &&
           ReadBool(streamer, &state.percussion) &&
           ReadBool(streamer, &state.relative) &&
           streamer.readInt32(state.priority) &&
           streamer.readInt32(state.volume) &&
           streamer.readInt32(state.pan) &&
           streamer.readInt32(state.transpose) &&
           streamer.readInt32(state.detune) &&
           streamer.readInt32(state.pitchbendFactor) &&
           streamer.readInt32(state.program) &&
           streamer.readInt32(state.param) &&
           streamer.readInt32(state.paramValue) &&
           streamer.readInt32(state.hookCommand) &&
           streamer.readInt32(state.targetTrack) &&
           streamer.readInt32(state.targetBeat) &&
           streamer.readInt32(state.targetTick) &&
           streamer.readInt32(state.hookValue) &&
           streamer.readInt32(state.loopCount) &&
           streamer.readInt32(state.loopToBeat) &&
           streamer.readInt32(state.loopToTick) &&
           streamer.readInt32(state.loopFromBeat) &&
           streamer.readInt32(state.loopFromTick) &&
           streamer.readInt32(state.instrument);
    if (!ok) {
        return false;
    }

    *out = state;
    return true;
}

bool WriteState(Steinberg::IBStream* stream, const SysExToolState& state) {
    if (!stream) {
        return false;
    }

    Steinberg::IBStreamer streamer(stream, kLittleEndian);
    return streamer.writeInt32(kStateVersion) &&
           streamer.writeInt32(static_cast<Steinberg::int32>(state.messageType)) &&
           streamer.writeInt32(state.midiOutDeviceIndex) &&
           streamer.writeInt32(state.part) &&
           streamer.writeInt32(state.channel) &&
           streamer.writeInt32(state.unknown) &&
           WriteBool(streamer, state.partOn) &&
           WriteBool(streamer, state.reverb) &&
           WriteBool(streamer, state.percussion) &&
           WriteBool(streamer, state.relative) &&
           streamer.writeInt32(state.priority) &&
           streamer.writeInt32(state.volume) &&
           streamer.writeInt32(state.pan) &&
           streamer.writeInt32(state.transpose) &&
           streamer.writeInt32(state.detune) &&
           streamer.writeInt32(state.pitchbendFactor) &&
           streamer.writeInt32(state.program) &&
           streamer.writeInt32(state.param) &&
           streamer.writeInt32(state.paramValue) &&
           streamer.writeInt32(state.hookCommand) &&
           streamer.writeInt32(state.targetTrack) &&
           streamer.writeInt32(state.targetBeat) &&
           streamer.writeInt32(state.targetTick) &&
           streamer.writeInt32(state.hookValue) &&
           streamer.writeInt32(state.loopCount) &&
           streamer.writeInt32(state.loopToBeat) &&
           streamer.writeInt32(state.loopToTick) &&
           streamer.writeInt32(state.loopFromBeat) &&
           streamer.writeInt32(state.loopFromTick) &&
           streamer.writeInt32(state.instrument);
}

} // namespace imwrap::vst3tool
