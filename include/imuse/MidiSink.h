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
