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

#ifndef IMUSE_SINK_MIDI_CHANNEL_H
#define IMUSE_SINK_MIDI_CHANNEL_H

#include <cstdint>

#include "imuse/Export.h"
#include "imuse/MidiChannel.h"
#include "imuse/MidiSink.h"

namespace imuse {

class IMUSE_API SinkMidiChannel final : public MidiChannel {
public:
    SinkMidiChannel(MidiSink *sink, uint16_t soundId, uint8_t channel);

    uint8_t number() const override { return _channel; }
    void release() override {}
    void send(uint32_t packedMessage) override;

    void noteOff(uint8_t note) override;
    void noteOn(uint8_t note, uint8_t velocity) override;
    void programChange(uint8_t program) override;
    void pitchBend(int16_t bend) override;
    void controlChange(uint8_t control, uint8_t value) override;
    void pitchBendFactor(uint8_t value) override;
    void bankSelect(uint8_t bank) override;
    void sysExCustomInstrument(uint32_t type, ByteView data) override;

private:
    void sendShort(uint8_t status, uint8_t data1, bool hasData2, uint8_t data2);

    MidiSink *_sink = nullptr;
    uint16_t _soundId = 0;
    uint8_t _channel = 0;
};

} // namespace imuse

#endif
