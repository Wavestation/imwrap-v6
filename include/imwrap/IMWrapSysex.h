/* ==========================================================================
 *
 * iMWrap v6 - A modern iMWrap implementation attempt with Adventure Game Studio Companion Plugin
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

#ifndef IMWRAP_IMWRAP_SYSEX_H
#define IMWRAP_IMWRAP_SYSEX_H

#include <cstdint>
#include <string>
#include <vector>

#include "imwrap/Export.h"
#include "imwrap/ByteView.h"

namespace imwrap {

enum class IMWrapSysexType {
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

struct IMWrapControlEvent {
    uint32_t tick = 0;
    uint16_t trackIndex = 0;
    IMWrapSysexType type = IMWrapSysexType::Unknown;
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
    uint8_t pitchbendFactor = 0;
    uint8_t program = 0;
    uint8_t value = 0;

    int8_t pan = 0;
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

IMWRAP_API bool DecodeIMWrapSysex(ByteView message, IMWrapControlEvent *out, std::string *error = nullptr);
IMWRAP_API bool EncodeIMWrapSysex(const IMWrapControlEvent &event, std::vector<uint8_t> *out);
IMWRAP_API std::string DescribeIMWrapSysex(const IMWrapControlEvent &event);
IMWRAP_API bool ParseIMWrapSysexDescription(const std::string &desc, IMWrapControlEvent *out);

} // namespace imwrap

#endif
