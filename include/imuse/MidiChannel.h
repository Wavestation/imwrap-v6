#ifndef IMUSE_MIDI_CHANNEL_H
#define IMUSE_MIDI_CHANNEL_H

#include <cstdint>

#include "imuse/ByteView.h"

namespace imuse {

class MidiChannel {
public:
    virtual ~MidiChannel() = default;

    virtual uint8_t number() const = 0;
    virtual void release() = 0;
    virtual void send(uint32_t packedMessage) = 0;

    virtual void noteOff(uint8_t note) = 0;
    virtual void noteOn(uint8_t note, uint8_t velocity) = 0;
    virtual void programChange(uint8_t program) = 0;
    virtual void pitchBend(int16_t bend) = 0;

    virtual void controlChange(uint8_t control, uint8_t value) = 0;
    virtual void modulationWheel(uint8_t value) { controlChange(0x01, value); }
    virtual void volume(uint8_t value) { controlChange(0x07, value); }
    virtual void panPosition(uint8_t value) { controlChange(0x0A, value); }
    virtual void pitchBendFactor(uint8_t value) = 0;
    virtual void transpose(int8_t value) { (void)value; }
    virtual void detune(int16_t value) { (void)value; }
    virtual void priority(uint8_t value) { (void)value; }
    virtual void sustain(bool value) { controlChange(0x40, value ? 1 : 0); }
    virtual void effectLevel(uint8_t value) { controlChange(0x5B, value); }
    virtual void chorusLevel(uint8_t value) { controlChange(0x5D, value); }
    virtual void bankSelect(uint8_t bank) { controlChange(0x00, bank); }
    virtual void allNotesOff() { controlChange(0x7B, 0); }

    virtual void sysExCustomInstrument(uint32_t type, ByteView data) = 0;
};

} // namespace imuse

#endif
