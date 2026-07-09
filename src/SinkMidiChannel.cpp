/* ==========================================================================
 *
 * iMWrap v6 - A modern iMWrap implementation attempt with Adventure Game Studio Companion Plugin
 *
 * This program is the legal property of Masami Komuro and few other contributors,
 * Please refer to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ========================================================================== */

#include "imwrap/SinkMidiChannel.h"

namespace imwrap {

SinkMidiChannel::SinkMidiChannel(MidiSink *sink, uint16_t soundId, uint8_t channel)
    : _sink(sink), _soundId(soundId), _channel(channel) {}

void SinkMidiChannel::sendShort(uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
    if (_sink) {
        _sink->onMidiMessage(_soundId, status, data1, hasData2, data2);
    }
}

void SinkMidiChannel::send(uint32_t packedMessage) {
    const uint8_t status = static_cast<uint8_t>((packedMessage & 0xF0u) | _channel);
    const uint8_t data1 = static_cast<uint8_t>((packedMessage >> 8) & 0xFFu);
    const uint8_t data2 = static_cast<uint8_t>((packedMessage >> 16) & 0xFFu);
    sendShort(status, data1, true, data2);
}

void SinkMidiChannel::noteOff(uint8_t note) {
    sendShort(static_cast<uint8_t>(0x80 | _channel), note, true, 0);
}

void SinkMidiChannel::noteOn(uint8_t note, uint8_t velocity) {
    sendShort(static_cast<uint8_t>(0x90 | _channel), note, true, velocity);
}

void SinkMidiChannel::programChange(uint8_t program) {
    sendShort(static_cast<uint8_t>(0xC0 | _channel), program, false, 0);
}

void SinkMidiChannel::pitchBend(int16_t bend) {
    int value = static_cast<int>(bend) + 0x2000;
    if (value < 0) {
        value = 0;
    }
    if (value > 0x3FFF) {
        value = 0x3FFF;
    }
    sendShort(static_cast<uint8_t>(0xE0 | _channel),
              static_cast<uint8_t>(value & 0x7F),
              true,
              static_cast<uint8_t>((value >> 7) & 0x7F));
}

void SinkMidiChannel::controlChange(uint8_t control, uint8_t value) {
    sendShort(static_cast<uint8_t>(0xB0 | _channel), control, true, value);
}

void SinkMidiChannel::pitchBendFactor(uint8_t value) {
    controlChange(0x65, 0x00);
    controlChange(0x64, 0x00);
    controlChange(0x06, value);
    controlChange(0x26, 0x00);
    controlChange(0x65, 0x7F);
    controlChange(0x64, 0x7F);
}

void SinkMidiChannel::bankSelect(uint8_t bank) {
    controlChange(0x00, bank);
}

void SinkMidiChannel::sysExCustomInstrument(uint32_t type, ByteView data) {
    if (_sink) {
        _sink->onCustomInstrument(_soundId, _channel, type, data);
    }
}

} // namespace imwrap
