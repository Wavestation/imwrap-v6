#ifndef IMUSE_IMUSE_SYSEX_H
#define IMUSE_IMUSE_SYSEX_H

#include <cstdint>
#include <string>
#include <vector>

#include "imuse/ByteView.h"

namespace imuse {

enum class ImuseSysexType {
    AllocatePart,
    ShutdownPart,
    StartSong,
    AdlibPartInstrument,
    AdlibGlobalInstrument,
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
    SetInstrument,
    Unknown
};

struct ImuseControlEvent {
    uint32_t tick = 0;
    uint16_t trackIndex = 0;
    ImuseSysexType type = ImuseSysexType::Unknown;
    uint8_t code = 0;

    bool hasPart = false;
    uint8_t part = 0;
    bool hasChannel = false;
    uint8_t channel = 0;

    uint8_t unknown = 0;
    uint8_t hookCommand = 0;

    bool partOn = false;
    bool reverb = false;
    bool percussion = false;
    bool relative = false;

    uint8_t priority = 0;
    uint8_t volume = 0;
    uint8_t pan = 0;
    uint8_t pitchbendFactor = 0;
    uint8_t program = 0;
    uint8_t value = 0;

    int8_t transpose = 0;
    int8_t detune = 0;
    int8_t signedValue = 0;

    uint16_t param = 0;
    uint16_t paramValue = 0;
    uint16_t targetTrack = 0;
    uint16_t targetBeat = 0;
    uint16_t targetTick = 0;
    uint16_t loopCount = 0;
    uint16_t loopToBeat = 0;
    uint16_t loopToTick = 0;
    uint16_t loopFromBeat = 0;
    uint16_t loopFromTick = 0;
    uint16_t instrument = 0;

    std::string markerText;
    std::vector<uint8_t> rawMessage;
    std::vector<uint8_t> decodedBytes;
};

bool DecodeImuseSysex(ByteView message, ImuseControlEvent *out, std::string *error = nullptr);
std::string DescribeImuseSysex(const ImuseControlEvent &event);

} // namespace imuse

#endif
