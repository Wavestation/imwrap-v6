#ifndef IMUSE_MIDI_SINK_H
#define IMUSE_MIDI_SINK_H

#include <cstdint>

#include "imuse/ByteView.h"

namespace imuse {

class MidiSink {
public:
    virtual ~MidiSink() = default;

    virtual void onMidiMessage(uint16_t soundId, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) = 0;
    virtual void onSysEx(uint16_t soundId, ByteView message) {
        (void)soundId;
        (void)message;
    }
    virtual void onCustomInstrument(uint16_t soundId, uint8_t channel, uint32_t type, ByteView data) {
        (void)soundId;
        (void)channel;
        (void)type;
        (void)data;
    }
    virtual void onTempoChange(uint16_t soundId, uint32_t microsPerQuarter) {
        (void)soundId;
        (void)microsPerQuarter;
    }
    virtual void onAllNotesOff() {}
};

} // namespace imuse

#endif
