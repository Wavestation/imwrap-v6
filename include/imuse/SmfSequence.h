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

#ifndef IMUSE_SMF_SEQUENCE_H
#define IMUSE_SMF_SEQUENCE_H

#include <cstdint>
#include <string>
#include <vector>

#include "imuse/Export.h"
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

class IMUSE_API SmfParser {
public:
    static bool Parse(ByteView data, SmfSequence *out, std::string *error = nullptr);
};

class IMUSE_API SmfSerializer {
public:
    static bool Serialize(const SmfSequence &seq, std::vector<uint8_t> *outBytes, std::string *error = nullptr);
};

} // namespace imuse

#endif
