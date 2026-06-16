#ifndef IMUSE_SMF_SEQUENCE_H
#define IMUSE_SMF_SEQUENCE_H

#include <cstdint>
#include <string>
#include <vector>

#include "imuse/ByteView.h"

namespace imuse {

enum class MidiEventType {
    Channel,
    Meta,
    SysEx,
    System
};

struct MidiEvent {
    MidiEventType type = MidiEventType::Channel;
    uint32_t tick = 0;
    uint32_t delta = 0;
    uint8_t status = 0;
    uint8_t data1 = 0;
    uint8_t data2 = 0;
    bool hasData1 = false;
    bool hasData2 = false;
    uint8_t metaType = 0;
    std::vector<uint8_t> payload;
};

struct SmfTrack {
    std::vector<MidiEvent> events;
};

struct SmfSequence {
    uint16_t format = 0;
    uint16_t trackCount = 0;
    uint16_t division = 0;
    std::vector<SmfTrack> tracks;

    bool isType2() const { return format == 2; }
    uint16_t ppqn() const { return division; }
};

class SmfParser {
public:
    static bool Parse(ByteView data, SmfSequence *out, std::string *error = nullptr);
};

} // namespace imuse

#endif
