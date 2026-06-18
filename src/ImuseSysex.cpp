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

#include "imuse/ImuseSysex.h"

#include <sstream>

namespace imuse {
namespace {

bool DecodeNibbles(ByteView data, std::vector<uint8_t> *out) {
    if (!out) {
        return false;
    }

    out->clear();
    out->reserve(data.size() / 2);
    for (std::size_t i = 0; i + 1 < data.size(); i += 2) {
        out->push_back(static_cast<uint8_t>(((data.data()[i] & 0x0F) << 4) | (data.data()[i + 1] & 0x0F)));
    }
    return true;
}

uint16_t ReadU16BE(const std::vector<uint8_t> &bytes, std::size_t offset) {
    return static_cast<uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
}

ByteView TrimEnvelope(ByteView message) {
    std::size_t start = 0;
    std::size_t end = message.size();

    if (start < end && message.data()[start] == 0xF0) {
        ++start;
    }
    if (end > start && message.data()[end - 1] == 0xF7) {
        --end;
    }
    return message.subview(start, end - start);
}

const char *TypeName(ImuseSysexType type) {
    switch (type) {
    case ImuseSysexType::AllocatePart: return "allocate-part";
    case ImuseSysexType::ShutdownPart: return "shutdown-part";
    case ImuseSysexType::StartSong: return "start-song";
    case ImuseSysexType::AdlibPartInstrument: return "adlib-part";
    case ImuseSysexType::AdlibGlobalInstrument: return "adlib-global";
    case ImuseSysexType::ParameterAdjust: return "parameter-adjust";
    case ImuseSysexType::HookJump: return "hook-jump";
    case ImuseSysexType::HookGlobalTranspose: return "hook-global-transpose";
    case ImuseSysexType::HookPartOnOff: return "hook-part-onoff";
    case ImuseSysexType::HookSetVolume: return "hook-set-volume";
    case ImuseSysexType::HookSetProgram: return "hook-set-program";
    case ImuseSysexType::HookSetTranspose: return "hook-set-transpose";
    case ImuseSysexType::Marker: return "marker";
    case ImuseSysexType::SetLoop: return "set-loop";
    case ImuseSysexType::ClearLoop: return "clear-loop";
    case ImuseSysexType::SetInstrument: return "set-instrument";
    case ImuseSysexType::Unknown: return "unknown";
    }
    return "unknown";
}

} // namespace

bool DecodeImuseSysex(ByteView message, ImuseControlEvent *out, std::string *error) {
    if (!out) {
        if (error) {
            *error = "output control event pointer is null";
        }
        return false;
    }

    ByteView body = TrimEnvelope(message);
    if (body.size() < 2) {
        return false;
    }
    if (body.data()[0] != 0x7D) {
        return false;
    }

    ImuseControlEvent event;
    event.code = body.data()[1];
    event.rawMessage.assign(body.data(), body.data() + body.size());

    ByteView payload = body.subview(2, body.size() - 2);

    switch (event.code) {
    case 0x00: {
        if (payload.size() < 2) {
            if (error) {
                *error = "allocate-part payload is too small";
            }
            return false;
        }

        event.type = ImuseSysexType::AllocatePart;
        event.hasPart = true;
        event.part = payload.data()[0] & 0x0F;
        if (!DecodeNibbles(payload.subview(1, payload.size() - 1), &event.decodedBytes)) {
            if (error) {
                *error = "allocate-part nibble payload is invalid";
            }
            return false;
        }
        event.partOn = event.decodedBytes.size() > 0 ? (event.decodedBytes[0] & 0x01) != 0 : true;
        event.reverb = event.decodedBytes.size() > 0 ? (event.decodedBytes[0] & 0x02) != 0 : false;
        event.priority = event.decodedBytes.size() > 1 ? event.decodedBytes[1] : 90;
        event.volume = event.decodedBytes.size() > 2 ? event.decodedBytes[2] : 127;
        event.pan = event.decodedBytes.size() > 3 ? event.decodedBytes[3] : 64;
        event.percussion = event.decodedBytes.size() > 4 ? (event.decodedBytes[4] & 0x80) != 0 : false;
        
        uint8_t transpose7 = event.decodedBytes.size() > 4 ? (event.decodedBytes[4] & 0x7F) : 0;
        event.transpose = static_cast<int8_t>((transpose7 & 0x40) ? (transpose7 | 0x80) : transpose7);
        
        event.detune = event.decodedBytes.size() > 5 ? static_cast<int8_t>(event.decodedBytes[5]) : 0;
        event.pitchbendFactor = event.decodedBytes.size() > 6 ? event.decodedBytes[6] : 2;
        event.program = event.decodedBytes.size() > 7 ? event.decodedBytes[7] : 0;
        return (*out = std::move(event)), true;
    }
    case 0x01:
        if (payload.size() != 1) {
            if (error) {
                *error = "shutdown-part payload must contain exactly one byte";
            }
            return false;
        }
        event.type = ImuseSysexType::ShutdownPart;
        event.hasPart = true;
        event.part = payload.data()[0] & 0x0F;
        return (*out = std::move(event)), true;
    case 0x02:
        event.type = ImuseSysexType::StartSong;
        return (*out = std::move(event)), true;
    case 0x10:
        if (payload.size() < 2 || ((payload.size() - 2) % 2) != 0) {
            if (error) {
                *error = "adlib-part payload has an invalid size";
            }
            return false;
        }
        event.type = ImuseSysexType::AdlibPartInstrument;
        event.hasPart = true;
        event.part = payload.data()[0] & 0x0F;
        event.unknown = payload.data()[1] & 0x7F;
        if (!DecodeNibbles(payload.subview(2, payload.size() - 2), &event.decodedBytes)) {
            if (error) {
                *error = "adlib-part nibble payload is invalid";
            }
            return false;
        }
        return (*out = std::move(event)), true;
    case 0x11:
        if (payload.size() < 3 || ((payload.size() - 3) % 2) != 0) {
            if (error) {
                *error = "adlib-global payload has an invalid size";
            }
            return false;
        }
        event.type = ImuseSysexType::AdlibGlobalInstrument;
        event.unknown = payload.data()[0] & 0x7F;
        event.value = payload.data()[1] & 0x7F;
        event.program = payload.data()[2] & 0x7F;
        if (!DecodeNibbles(payload.subview(3, payload.size() - 3), &event.decodedBytes)) {
            if (error) {
                *error = "adlib-global nibble payload is invalid";
            }
            return false;
        }
        return (*out = std::move(event)), true;
    case 0x21: {
        if (payload.size() != 10) {
            if (error) {
                *error = "parameter-adjust payload must contain 10 bytes";
            }
            return false;
        }
        event.type = ImuseSysexType::ParameterAdjust;
        event.hasPart = true;
        event.part = payload.data()[0] & 0x0F;
        event.unknown = payload.data()[1] & 0x7F;
        if (!DecodeNibbles(payload.subview(2, payload.size() - 2), &event.decodedBytes) || event.decodedBytes.size() != 4) {
            if (error) {
                *error = "parameter-adjust nibble payload is invalid";
            }
            return false;
        }
        event.param = ReadU16BE(event.decodedBytes, 0);
        event.paramValue = ReadU16BE(event.decodedBytes, 2);
        return (*out = std::move(event)), true;
    }
    case 0x30: {
        if (payload.size() != 15) {
            if (error) {
                *error = "hook-jump payload must contain 15 bytes";
            }
            return false;
        }
        event.type = ImuseSysexType::HookJump;
        event.unknown = payload.data()[0] & 0x7F;
        if (!DecodeNibbles(payload.subview(1, payload.size() - 1), &event.decodedBytes) || event.decodedBytes.size() != 7) {
            if (error) {
                *error = "hook-jump nibble payload is invalid";
            }
            return false;
        }
        event.hookCommand = event.decodedBytes[0];
        event.targetTrack = ReadU16BE(event.decodedBytes, 1);
        event.targetBeat = ReadU16BE(event.decodedBytes, 3);
        event.targetTick = ReadU16BE(event.decodedBytes, 5);
        return (*out = std::move(event)), true;
    }
    case 0x31: {
        if (payload.size() != 7) {
            if (error) {
                *error = "hook-global-transpose payload must contain 7 bytes";
            }
            return false;
        }
        event.type = ImuseSysexType::HookGlobalTranspose;
        event.unknown = payload.data()[0] & 0x7F;
        if (!DecodeNibbles(payload.subview(1, payload.size() - 1), &event.decodedBytes) || event.decodedBytes.size() != 3) {
            if (error) {
                *error = "hook-global-transpose nibble payload is invalid";
            }
            return false;
        }
        event.hookCommand = event.decodedBytes[0];
        event.relative = event.decodedBytes[1] != 0;
        event.signedValue = static_cast<int8_t>(event.decodedBytes[2]);
        return (*out = std::move(event)), true;
    }
    case 0x32:
    case 0x33:
    case 0x34: {
        if (payload.size() != 5) {
            if (error) {
                *error = "hook part payload must contain 5 bytes";
            }
            return false;
        }
        event.type =
            (event.code == 0x32) ? ImuseSysexType::HookPartOnOff :
            (event.code == 0x33) ? ImuseSysexType::HookSetVolume :
                                   ImuseSysexType::HookSetProgram;
        event.hasChannel = true;
        event.channel = payload.data()[0] & 0x0F;
        if (!DecodeNibbles(payload.subview(1, payload.size() - 1), &event.decodedBytes) || event.decodedBytes.size() != 2) {
            if (error) {
                *error = "hook part nibble payload is invalid";
            }
            return false;
        }
        event.hookCommand = event.decodedBytes[0];
        event.value = event.decodedBytes[1];
        return (*out = std::move(event)), true;
    }
    case 0x35: {
        if (payload.size() != 7) {
            if (error) {
                *error = "hook-set-transpose payload must contain 7 bytes";
            }
            return false;
        }
        event.type = ImuseSysexType::HookSetTranspose;
        event.hasChannel = true;
        event.channel = payload.data()[0] & 0x0F;
        if (!DecodeNibbles(payload.subview(1, payload.size() - 1), &event.decodedBytes) || event.decodedBytes.size() != 3) {
            if (error) {
                *error = "hook-set-transpose nibble payload is invalid";
            }
            return false;
        }
        event.hookCommand = event.decodedBytes[0];
        event.relative = event.decodedBytes[1] != 0;
        event.signedValue = static_cast<int8_t>(event.decodedBytes[2]);
        return (*out = std::move(event)), true;
    }
    case 0x40:
        if (payload.empty()) {
            if (error) {
                *error = "marker payload is empty";
            }
            return false;
        }
        event.type = ImuseSysexType::Marker;
        event.unknown = payload.data()[0] & 0x7F;
        event.markerText.assign(reinterpret_cast<const char *>(payload.data() + 1), payload.size() - 1);
        return (*out = std::move(event)), true;
    case 0x50: {
        if (payload.size() != 21) {
            if (error) {
                *error = "set-loop payload must contain 21 bytes";
            }
            return false;
        }
        event.type = ImuseSysexType::SetLoop;
        event.unknown = payload.data()[0] & 0x7F;
        if (!DecodeNibbles(payload.subview(1, payload.size() - 1), &event.decodedBytes) || event.decodedBytes.size() != 10) {
            if (error) {
                *error = "set-loop nibble payload is invalid";
            }
            return false;
        }
        event.loopCount = ReadU16BE(event.decodedBytes, 0);
        event.loopToBeat = ReadU16BE(event.decodedBytes, 2);
        event.loopToTick = ReadU16BE(event.decodedBytes, 4);
        event.loopFromBeat = ReadU16BE(event.decodedBytes, 6);
        event.loopFromTick = ReadU16BE(event.decodedBytes, 8);
        return (*out = std::move(event)), true;
    }
    case 0x51:
        if (!payload.empty()) {
            event.rawMessage.assign(body.data(), body.data() + body.size());
        }
        event.type = ImuseSysexType::ClearLoop;
        return (*out = std::move(event)), true;
    case 0x60: {
        if (payload.size() != 5) {
            if (error) {
                *error = "set-instrument payload must contain 5 bytes";
            }
            return false;
        }
        event.type = ImuseSysexType::SetInstrument;
        event.hasChannel = true;
        event.channel = payload.data()[0] & 0x0F;
        event.instrument = static_cast<uint16_t>(((payload.data()[1] & 0x0F) << 12) |
                                                 ((payload.data()[2] & 0x0F) << 8) |
                                                 ((payload.data()[3] & 0x0F) << 4) |
                                                 (payload.data()[4] & 0x0F));
        return (*out = std::move(event)), true;
    }
    default:
        event.type = ImuseSysexType::Unknown;
        return (*out = std::move(event)), true;
    }
}

std::string DescribeImuseSysex(const ImuseControlEvent &event) {
    std::ostringstream oss;
    oss << TypeName(event.type);

    switch (event.type) {
    case ImuseSysexType::AllocatePart:
        oss << " part=" << static_cast<int>(event.part)
            << " pri=" << static_cast<int>(event.priority)
            << " vol=" << static_cast<int>(event.volume)
            << " pan=" << static_cast<int>(event.pan)
            << " program=" << static_cast<int>(event.program);
        break;
    case ImuseSysexType::ShutdownPart:
        oss << " part=" << static_cast<int>(event.part);
        break;
    case ImuseSysexType::ParameterAdjust:
        oss << " part=" << static_cast<int>(event.part)
            << " param=" << event.param
            << " value=" << event.paramValue;
        break;
    case ImuseSysexType::HookJump:
        oss << " cmd=" << static_cast<int>(event.hookCommand)
            << " track=" << event.targetTrack
            << " beat=" << event.targetBeat
            << " tick=" << event.targetTick;
        break;
    case ImuseSysexType::HookGlobalTranspose:
    case ImuseSysexType::HookSetTranspose:
        oss << " cmd=" << static_cast<int>(event.hookCommand)
            << " relative=" << (event.relative ? "yes" : "no")
            << " value=" << static_cast<int>(event.signedValue);
        break;
    case ImuseSysexType::HookPartOnOff:
    case ImuseSysexType::HookSetVolume:
    case ImuseSysexType::HookSetProgram:
        oss << " chan=" << static_cast<int>(event.channel)
            << " cmd=" << static_cast<int>(event.hookCommand)
            << " value=" << static_cast<int>(event.value);
        break;
    case ImuseSysexType::Marker:
        oss << " text=\"" << event.markerText << "\"";
        break;
    case ImuseSysexType::SetLoop:
        oss << " count=" << event.loopCount
            << " to=" << event.loopToBeat << ":" << event.loopToTick
            << " from=" << event.loopFromBeat << ":" << event.loopFromTick;
        break;
    case ImuseSysexType::SetInstrument:
        oss << " chan=" << static_cast<int>(event.channel)
            << " instrument=" << event.instrument;
        break;
    case ImuseSysexType::AdlibPartInstrument:
    case ImuseSysexType::AdlibGlobalInstrument:
        oss << " bytes=" << event.decodedBytes.size();
        break;
    case ImuseSysexType::StartSong:
    case ImuseSysexType::ClearLoop:
    case ImuseSysexType::Unknown:
        break;
    }

    return oss.str();
}

} // namespace imuse
