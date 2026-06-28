#pragma once

#include "imwrap/IMWrapSysex.h"

#include <cstdint>
#include <string>
#include <vector>

namespace imwrap::vst3tool {

enum class MessageType : std::int32_t {
    AllocatePart = 0,
    ShutdownPart,
    StartSong,
    ParameterAdjust,
    HookJump,
    HookGlobalTranspose,
    HookPartOnOff,
    HookSetVolume,
    HookSetProgram,
    HookSetTranspose,
    Marker,
    SetLoop,
    ClearLoop,
    SetInstrument
};

constexpr std::int32_t kMessageTypeCount = 14;

struct SysExToolState {
    MessageType messageType = MessageType::AllocatePart;
    int midiOutDeviceIndex = -1;
    int part = 0;
    int channel = 0;
    int unknown = 0;

    bool partOn = true;
    bool reverb = false;
    bool percussion = false;
    bool relative = false;

    int priority = 90;
    int volume = 127;
    int pan = 0;
    int transpose = 0;
    int detune = 0;
    int pitchbendFactor = 2;
    int program = 0;

    int param = 0;
    int paramValue = 0;
    int hookCommand = 0;
    int targetTrack = 0;
    int targetBeat = 0;
    int targetTick = 0;
    int hookValue = 0;
    int markerValue = 0;

    int loopCount = 0;
    int loopToBeat = 0;
    int loopToTick = 0;
    int loopFromBeat = 0;
    int loopFromTick = 0;

    int instrument = 0;
};

const char* MessageTypeName(MessageType type);

double NormalizeBool(bool value);
bool DenormalizeBool(double normalized);
double NormalizeInt(int value, int minValue, int maxValue);
int DenormalizeInt(double normalized, int minValue, int maxValue);

bool BuildControlEvent(const SysExToolState& state, IMWrapControlEvent* out, std::string* error);
bool BuildSysExBytes(const SysExToolState& state, std::vector<std::uint8_t>* out, std::string* error);

} // namespace imwrap::vst3tool
