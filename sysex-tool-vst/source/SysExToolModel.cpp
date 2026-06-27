#include "SysExToolModel.h"

#include <algorithm>
#include <cmath>

namespace imwrap::vst3tool {
namespace {

template <typename T>
T Clamp(T value, T minValue, T maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

} // namespace

const char* MessageTypeName(MessageType type) {
    switch (type) {
    case MessageType::AllocatePart: return "Allocate Part";
    case MessageType::ShutdownPart: return "Shutdown Part";
    case MessageType::StartSong: return "Start Song";
    case MessageType::ParameterAdjust: return "Parameter Adjust";
    case MessageType::HookJump: return "Hook Jump";
    case MessageType::HookGlobalTranspose: return "Hook Global Transpose";
    case MessageType::HookPartOnOff: return "Hook Part On/Off";
    case MessageType::HookSetVolume: return "Hook Set Volume";
    case MessageType::HookSetProgram: return "Hook Set Program";
    case MessageType::HookSetTranspose: return "Hook Set Transpose";
    case MessageType::SetLoop: return "Set Loop";
    case MessageType::ClearLoop: return "Clear Loop";
    case MessageType::SetInstrument: return "Set Instrument";
    }

    return "Unknown";
}

double NormalizeBool(bool value) {
    return value ? 1.0 : 0.0;
}

bool DenormalizeBool(double normalized) {
    return normalized >= 0.5;
}

double NormalizeInt(int value, int minValue, int maxValue) {
    if (maxValue <= minValue) {
        return 0.0;
    }

    const int clamped = Clamp(value, minValue, maxValue);
    return static_cast<double>(clamped - minValue) / static_cast<double>(maxValue - minValue);
}

int DenormalizeInt(double normalized, int minValue, int maxValue) {
    if (maxValue <= minValue) {
        return minValue;
    }

    const double clamped = std::max(0.0, std::min(normalized, 1.0));
    const double scaled = static_cast<double>(minValue) +
                          (clamped * static_cast<double>(maxValue - minValue));
    return Clamp(static_cast<int>(std::lround(scaled)), minValue, maxValue);
}

bool BuildControlEvent(const SysExToolState& state, IMWrapControlEvent* out, std::string* error) {
    if (!out) {
        if (error) {
            *error = "output event pointer is null";
        }
        return false;
    }

    IMWrapControlEvent event;

    switch (state.messageType) {
    case MessageType::AllocatePart:
        event.type = IMWrapSysexType::AllocatePart;
        event.hasPart = true;
        event.part = static_cast<std::uint8_t>(Clamp(state.part, 0, 15));
        event.unknown = static_cast<std::uint8_t>(Clamp(state.unknown, 0, 15));
        event.partOn = state.partOn;
        event.reverb = state.reverb;
        event.priority = static_cast<std::uint8_t>(Clamp(state.priority, 0, 255));
        event.volume = static_cast<std::uint8_t>(Clamp(state.volume, 0, 127));
        event.pan = static_cast<std::int8_t>(Clamp(state.pan, 0, 127));
        event.percussion = state.percussion;
        event.transpose = static_cast<std::int8_t>(Clamp(state.transpose, -127, 127));
        event.detune = static_cast<std::int8_t>(Clamp(state.detune, -128, 127));
        event.pitchbendFactor = static_cast<std::uint8_t>(Clamp(state.pitchbendFactor, 0, 255));
        event.program = static_cast<std::uint8_t>(Clamp(state.program, 0, 127));
        break;

    case MessageType::ShutdownPart:
        event.type = IMWrapSysexType::ShutdownPart;
        event.hasPart = true;
        event.part = static_cast<std::uint8_t>(Clamp(state.part, 0, 15));
        break;

    case MessageType::StartSong:
        event.type = IMWrapSysexType::StartSong;
        break;

    case MessageType::ParameterAdjust:
        event.type = IMWrapSysexType::ParameterAdjust;
        event.hasPart = true;
        event.part = static_cast<std::uint8_t>(Clamp(state.part, 0, 15));
        event.unknown = static_cast<std::uint8_t>(Clamp(state.unknown, 0, 127));
        event.param = static_cast<std::uint16_t>(Clamp(state.param, 0, 65535));
        event.paramValue = static_cast<std::uint16_t>(Clamp(state.paramValue, 0, 65535));
        break;

    case MessageType::HookJump:
        event.type = IMWrapSysexType::HookJump;
        event.unknown = static_cast<std::uint8_t>(Clamp(state.unknown, 0, 127));
        event.hookCommand = static_cast<std::uint8_t>(Clamp(state.hookCommand, 0, 255));
        event.targetTrack = static_cast<std::uint16_t>(Clamp(state.targetTrack, 0, 65535));
        event.targetBeat = static_cast<std::uint16_t>(Clamp(state.targetBeat, 0, 65535));
        event.targetTick = static_cast<std::uint16_t>(Clamp(state.targetTick, 0, 65535));
        break;

    case MessageType::HookGlobalTranspose:
        event.type = IMWrapSysexType::HookGlobalTranspose;
        event.unknown = static_cast<std::uint8_t>(Clamp(state.unknown, 0, 127));
        event.hookCommand = static_cast<std::uint8_t>(Clamp(state.hookCommand, 0, 255));
        event.relative = state.relative;
        event.signedValue = static_cast<std::int8_t>(Clamp(state.hookValue, -128, 127));
        break;

    case MessageType::HookPartOnOff:
        event.type = IMWrapSysexType::HookPartOnOff;
        event.hasChannel = true;
        event.channel = static_cast<std::uint8_t>(Clamp(state.channel, 0, 15));
        event.hookCommand = static_cast<std::uint8_t>(Clamp(state.hookCommand, 0, 255));
        event.value = static_cast<std::uint8_t>(Clamp(state.hookValue, 0, 255));
        break;

    case MessageType::HookSetVolume:
        event.type = IMWrapSysexType::HookSetVolume;
        event.hasChannel = true;
        event.channel = static_cast<std::uint8_t>(Clamp(state.channel, 0, 15));
        event.hookCommand = static_cast<std::uint8_t>(Clamp(state.hookCommand, 0, 255));
        event.value = static_cast<std::uint8_t>(Clamp(state.hookValue, 0, 255));
        break;

    case MessageType::HookSetProgram:
        event.type = IMWrapSysexType::HookSetProgram;
        event.hasChannel = true;
        event.channel = static_cast<std::uint8_t>(Clamp(state.channel, 0, 15));
        event.hookCommand = static_cast<std::uint8_t>(Clamp(state.hookCommand, 0, 255));
        event.value = static_cast<std::uint8_t>(Clamp(state.hookValue, 0, 255));
        break;

    case MessageType::HookSetTranspose:
        event.type = IMWrapSysexType::HookSetTranspose;
        event.hasChannel = true;
        event.channel = static_cast<std::uint8_t>(Clamp(state.channel, 0, 15));
        event.hookCommand = static_cast<std::uint8_t>(Clamp(state.hookCommand, 0, 255));
        event.relative = state.relative;
        event.signedValue = static_cast<std::int8_t>(Clamp(state.hookValue, -128, 127));
        break;

    case MessageType::SetLoop:
        event.type = IMWrapSysexType::SetLoop;
        event.unknown = static_cast<std::uint8_t>(Clamp(state.unknown, 0, 127));
        event.loopCount = static_cast<std::uint16_t>(Clamp(state.loopCount, 0, 65535));
        event.loopToBeat = static_cast<std::uint16_t>(Clamp(state.loopToBeat, 0, 65535));
        event.loopToTick = static_cast<std::uint16_t>(Clamp(state.loopToTick, 0, 65535));
        event.loopFromBeat = static_cast<std::uint16_t>(Clamp(state.loopFromBeat, 0, 65535));
        event.loopFromTick = static_cast<std::uint16_t>(Clamp(state.loopFromTick, 0, 65535));
        break;

    case MessageType::ClearLoop:
        event.type = IMWrapSysexType::ClearLoop;
        break;

    case MessageType::SetInstrument:
        event.type = IMWrapSysexType::SetInstrument;
        event.hasChannel = true;
        event.channel = static_cast<std::uint8_t>(Clamp(state.channel, 0, 15));
        event.instrument = static_cast<std::uint16_t>(Clamp(state.instrument, 0, 65535));
        break;
    }

    *out = event;
    return true;
}

bool BuildSysExBytes(const SysExToolState& state, std::vector<std::uint8_t>* out, std::string* error) {
    IMWrapControlEvent event;
    if (!BuildControlEvent(state, &event, error)) {
        return false;
    }

    if (!EncodeIMWrapSysex(event, out)) {
        if (error) {
            *error = "failed to encode IMWrap SysEx event";
        }
        return false;
    }

    return true;
}

} // namespace imwrap::vst3tool
