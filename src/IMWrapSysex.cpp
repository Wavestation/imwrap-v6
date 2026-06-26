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

#include "imwrap/IMWrapSysex.h"

#include <sstream>

namespace imwrap {
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

std::vector<uint8_t> EncodeNibbles(const std::vector<uint8_t>& bytes) {
    std::vector<uint8_t> nibbles;
    nibbles.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        nibbles.push_back((b >> 4) & 0x0F);
        nibbles.push_back(b & 0x0F);
    }
    return nibbles;
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

const char *TypeName(IMWrapSysexType type) {
    switch (type) {
    case IMWrapSysexType::AllocatePart: return "allocate-part";
    case IMWrapSysexType::ShutdownPart: return "shutdown-part";
    case IMWrapSysexType::StartSong: return "start-song";
    case IMWrapSysexType::AdlibPartInstrument: return "adlib-part";
    case IMWrapSysexType::AdlibGlobalInstrument: return "adlib-global";
    case IMWrapSysexType::ParameterAdjust: return "parameter-adjust";
    case IMWrapSysexType::HookJump: return "hook-jump";
    case IMWrapSysexType::HookGlobalTranspose: return "hook-global-transpose";
    case IMWrapSysexType::HookPartOnOff: return "hook-part-onoff";
    case IMWrapSysexType::HookSetVolume: return "hook-set-volume";
    case IMWrapSysexType::HookSetProgram: return "hook-set-program";
    case IMWrapSysexType::HookSetTranspose: return "hook-set-transpose";
    case IMWrapSysexType::Marker: return "marker";
    case IMWrapSysexType::SetLoop: return "set-loop";
    case IMWrapSysexType::ClearLoop: return "clear-loop";
    case IMWrapSysexType::SetInstrument: return "set-instrument";
    case IMWrapSysexType::Unknown: return "unknown";
    }
    return "unknown";
}

} // namespace

bool DecodeIMWrapSysex(ByteView message, IMWrapControlEvent *out, std::string *error) {
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

    IMWrapControlEvent event;
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

        event.type = IMWrapSysexType::AllocatePart;
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
        event.pan = event.decodedBytes.size() > 3 ? static_cast<int8_t>(event.decodedBytes[3]) : 0;
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
        event.type = IMWrapSysexType::ShutdownPart;
        event.hasPart = true;
        event.part = payload.data()[0] & 0x0F;
        return (*out = std::move(event)), true;
    case 0x02:
        event.type = IMWrapSysexType::StartSong;
        return (*out = std::move(event)), true;
    case 0x10:
        if (payload.size() < 2 || ((payload.size() - 2) % 2) != 0) {
            if (error) {
                *error = "adlib-part payload has an invalid size";
            }
            return false;
        }
        event.type = IMWrapSysexType::AdlibPartInstrument;
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
        event.type = IMWrapSysexType::AdlibGlobalInstrument;
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
        event.type = IMWrapSysexType::ParameterAdjust;
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
        event.type = IMWrapSysexType::HookJump;
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
        event.type = IMWrapSysexType::HookGlobalTranspose;
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
            (event.code == 0x32) ? IMWrapSysexType::HookPartOnOff :
            (event.code == 0x33) ? IMWrapSysexType::HookSetVolume :
                                   IMWrapSysexType::HookSetProgram;
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
        event.type = IMWrapSysexType::HookSetTranspose;
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
        event.type = IMWrapSysexType::Marker;
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
        event.type = IMWrapSysexType::SetLoop;
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
        event.type = IMWrapSysexType::ClearLoop;
        return (*out = std::move(event)), true;
    case 0x60: {
        if (payload.size() != 5) {
            if (error) {
                *error = "set-instrument payload must contain 5 bytes";
            }
            return false;
        }
        event.type = IMWrapSysexType::SetInstrument;
        event.hasChannel = true;
        event.channel = payload.data()[0] & 0x0F;
        event.instrument = static_cast<uint16_t>(((payload.data()[1] & 0x0F) << 12) |
                                                 ((payload.data()[2] & 0x0F) << 8) |
                                                 ((payload.data()[3] & 0x0F) << 4) |
                                                 (payload.data()[4] & 0x0F));
        return (*out = std::move(event)), true;
    }
    default:
        event.type = IMWrapSysexType::Unknown;
        return (*out = std::move(event)), true;
    }
}

bool EncodeIMWrapSysex(const IMWrapControlEvent &event, std::vector<uint8_t> *out) {
    if (!out) return false;
    out->clear();
    
    std::vector<uint8_t> payload;
    switch (event.type) {
    case IMWrapSysexType::AllocatePart: {
        out->push_back(0x7D);
        out->push_back(0x00);
        out->push_back(event.part & 0x0F);
        
        std::vector<uint8_t> params;
        uint8_t b0 = static_cast<uint8_t>(((event.unknown & 0x0F) << 4) |
                                          (event.partOn ? 0x01 : 0) |
                                          (event.reverb ? 0x02 : 0));
        params.push_back(b0);
        params.push_back(event.priority);
        params.push_back(event.volume);
        params.push_back(static_cast<uint8_t>(event.pan));
        uint8_t b4 = (event.percussion ? 0x80 : 0) | (static_cast<uint8_t>(event.transpose) & 0x7F);
        params.push_back(b4);
        params.push_back(static_cast<uint8_t>(event.detune));
        params.push_back(event.pitchbendFactor);
        params.push_back(event.program);
        
        std::vector<uint8_t> nibs = EncodeNibbles(params);
        out->insert(out->end(), nibs.begin(), nibs.end());
        break;
    }
    case IMWrapSysexType::ShutdownPart:
        out->push_back(0x7D);
        out->push_back(0x01);
        out->push_back(event.part & 0x0F);
        break;
    case IMWrapSysexType::StartSong:
        out->push_back(0x7D);
        out->push_back(0x02);
        break;
    case IMWrapSysexType::AdlibPartInstrument: {
        out->push_back(0x7D);
        out->push_back(0x10);
        out->push_back(event.part & 0x0F);
        out->push_back(event.unknown & 0x7F);
        std::vector<uint8_t> nibs = EncodeNibbles(event.decodedBytes);
        out->insert(out->end(), nibs.begin(), nibs.end());
        break;
    }
    case IMWrapSysexType::AdlibGlobalInstrument: {
        out->push_back(0x7D);
        out->push_back(0x11);
        out->push_back(event.unknown & 0x7F);
        out->push_back(event.value & 0x7F);
        out->push_back(event.program & 0x7F);
        std::vector<uint8_t> nibs = EncodeNibbles(event.decodedBytes);
        out->insert(out->end(), nibs.begin(), nibs.end());
        break;
    }
    case IMWrapSysexType::ParameterAdjust: {
        out->push_back(0x7D);
        out->push_back(0x21);
        out->push_back(event.part & 0x0F);
        out->push_back(event.unknown & 0x7F);
        std::vector<uint8_t> nibs = EncodeNibbles({
            static_cast<uint8_t>((event.param >> 8) & 0xFF), static_cast<uint8_t>(event.param & 0xFF),
            static_cast<uint8_t>((event.paramValue >> 8) & 0xFF), static_cast<uint8_t>(event.paramValue & 0xFF)
        });
        out->insert(out->end(), nibs.begin(), nibs.end());
        break;
    }
    case IMWrapSysexType::HookJump: {
        out->push_back(0x7D);
        out->push_back(0x30);
        out->push_back(event.unknown & 0x7F);
        std::vector<uint8_t> nibs = EncodeNibbles({
            event.hookCommand,
            static_cast<uint8_t>((event.targetTrack >> 8) & 0xFF), static_cast<uint8_t>(event.targetTrack & 0xFF),
            static_cast<uint8_t>((event.targetBeat >> 8) & 0xFF), static_cast<uint8_t>(event.targetBeat & 0xFF),
            static_cast<uint8_t>((event.targetTick >> 8) & 0xFF), static_cast<uint8_t>(event.targetTick & 0xFF)
        });
        out->insert(out->end(), nibs.begin(), nibs.end());
        break;
    }
    case IMWrapSysexType::HookGlobalTranspose: {
        out->push_back(0x7D);
        out->push_back(0x31);
        out->push_back(event.unknown & 0x7F);
        std::vector<uint8_t> nibs = EncodeNibbles({
            event.hookCommand, static_cast<uint8_t>(event.relative ? 1 : 0), static_cast<uint8_t>(event.signedValue)
        });
        out->insert(out->end(), nibs.begin(), nibs.end());
        break;
    }
    case IMWrapSysexType::HookPartOnOff:
    case IMWrapSysexType::HookSetVolume:
    case IMWrapSysexType::HookSetProgram: {
        out->push_back(0x7D);
        out->push_back(event.type == IMWrapSysexType::HookPartOnOff ? 0x32 : (event.type == IMWrapSysexType::HookSetVolume ? 0x33 : 0x34));
        out->push_back(event.channel & 0x0F);
        std::vector<uint8_t> nibs = EncodeNibbles({event.hookCommand, event.value});
        out->insert(out->end(), nibs.begin(), nibs.end());
        break;
    }
    case IMWrapSysexType::HookSetTranspose: {
        out->push_back(0x7D);
        out->push_back(0x35);
        out->push_back(event.channel & 0x0F);
        std::vector<uint8_t> nibs = EncodeNibbles({
            event.hookCommand, static_cast<uint8_t>(event.relative ? 1 : 0), static_cast<uint8_t>(event.signedValue)
        });
        out->insert(out->end(), nibs.begin(), nibs.end());
        break;
    }
    case IMWrapSysexType::Marker: {
        out->push_back(0x7D);
        out->push_back(0x40);
        out->push_back(event.unknown & 0x7F);
        out->insert(out->end(), event.markerText.begin(), event.markerText.end());
        break;
    }
    case IMWrapSysexType::SetLoop: {
        out->push_back(0x7D);
        out->push_back(0x50);
        out->push_back(event.unknown & 0x7F);
        std::vector<uint8_t> nibs = EncodeNibbles({
            static_cast<uint8_t>((event.loopCount >> 8) & 0xFF), static_cast<uint8_t>(event.loopCount & 0xFF),
            static_cast<uint8_t>((event.loopToBeat >> 8) & 0xFF), static_cast<uint8_t>(event.loopToBeat & 0xFF),
            static_cast<uint8_t>((event.loopToTick >> 8) & 0xFF), static_cast<uint8_t>(event.loopToTick & 0xFF),
            static_cast<uint8_t>((event.loopFromBeat >> 8) & 0xFF), static_cast<uint8_t>(event.loopFromBeat & 0xFF),
            static_cast<uint8_t>((event.loopFromTick >> 8) & 0xFF), static_cast<uint8_t>(event.loopFromTick & 0xFF)
        });
        out->insert(out->end(), nibs.begin(), nibs.end());
        break;
    }
    case IMWrapSysexType::ClearLoop:
        out->push_back(0x7D);
        out->push_back(0x51);
        break;
    case IMWrapSysexType::SetInstrument:
        out->push_back(0x7D);
        out->push_back(0x60);
        out->push_back(event.channel & 0x0F);
        out->push_back(static_cast<uint8_t>((event.instrument >> 12) & 0x0F));
        out->push_back(static_cast<uint8_t>((event.instrument >> 8) & 0x0F));
        out->push_back(static_cast<uint8_t>((event.instrument >> 4) & 0x0F));
        out->push_back(static_cast<uint8_t>(event.instrument & 0x0F));
        break;
    default:
        if (!event.rawMessage.empty()) {
            out->assign(event.rawMessage.begin(), event.rawMessage.end());
        } else {
            return false;
        }
        break;
    }
    
    out->insert(out->begin(), 0xF0);
    out->push_back(0xF7);
    return true;
}

std::string DescribeIMWrapSysex(const IMWrapControlEvent &event) {
    std::ostringstream oss;
    oss << TypeName(event.type);

    switch (event.type) {
    case IMWrapSysexType::AllocatePart:
        oss << " part=" << static_cast<int>(event.part)
            << " pri=" << static_cast<int>(event.priority)
            << " vol=" << static_cast<int>(event.volume)
            << " pan=" << static_cast<int>(event.pan)
            << " program=" << static_cast<int>(event.program);
        break;
    case IMWrapSysexType::ShutdownPart:
        oss << " part=" << static_cast<int>(event.part);
        break;
    case IMWrapSysexType::ParameterAdjust:
        oss << " part=" << static_cast<int>(event.part)
            << " param=" << event.param
            << " value=" << event.paramValue;
        break;
    case IMWrapSysexType::HookJump:
        oss << " cmd=" << static_cast<int>(event.hookCommand)
            << " track=" << event.targetTrack
            << " beat=" << event.targetBeat
            << " tick=" << event.targetTick;
        break;
    case IMWrapSysexType::HookGlobalTranspose:
    case IMWrapSysexType::HookSetTranspose:
        oss << " cmd=" << static_cast<int>(event.hookCommand)
            << " relative=" << (event.relative ? "yes" : "no")
            << " value=" << static_cast<int>(event.signedValue);
        break;
    case IMWrapSysexType::HookPartOnOff:
    case IMWrapSysexType::HookSetVolume:
    case IMWrapSysexType::HookSetProgram:
        oss << " chan=" << static_cast<int>(event.channel)
            << " cmd=" << static_cast<int>(event.hookCommand)
            << " value=" << static_cast<int>(event.value);
        break;
    case IMWrapSysexType::Marker:
        oss << " text=\"" << event.markerText << "\"";
        break;
    case IMWrapSysexType::SetLoop:
        oss << " count=" << event.loopCount
            << " to=" << event.loopToBeat << ":" << event.loopToTick
            << " from=" << event.loopFromBeat << ":" << event.loopFromTick;
        break;
    case IMWrapSysexType::SetInstrument:
        oss << " chan=" << static_cast<int>(event.channel)
            << " instrument=" << event.instrument;
        break;
    case IMWrapSysexType::AdlibPartInstrument:
    case IMWrapSysexType::AdlibGlobalInstrument:
        oss << " bytes=" << event.decodedBytes.size();
        break;
    case IMWrapSysexType::StartSong:
    case IMWrapSysexType::ClearLoop:
    case IMWrapSysexType::Unknown:
        break;
    }

    return oss.str();
}


bool ParseIMWrapSysexDescription(const std::string &desc, IMWrapControlEvent *out) {
    if (!out) return false;
    
    // Set some defaults
    out->unknown = 0;
    out->hasPart = false;
    out->hasChannel = false;
    out->partOn = true;
    out->reverb = false;
    out->percussion = false;
    out->relative = false;
    out->priority = 90;
    out->volume = 127;
    out->pitchbendFactor = 2;
    out->program = 0;
    out->value = 0;
    out->pan = 0;
    out->transpose = 0;
    out->detune = 0;
    out->signedValue = 0;

    int p1, p2, p3, p4, p5, p6, p7;
    char s1[256];

    if (sscanf(desc.c_str(), "allocate-part part=%d pri=%d vol=%d pan=%d program=%d", &p1, &p2, &p3, &p4, &p5) == 5) {
        out->type = IMWrapSysexType::AllocatePart;
        out->hasPart = true; out->part = p1; out->priority = p2; out->volume = p3; out->pan = p4; out->program = p5;
        return true;
    }
    if (sscanf(desc.c_str(), "shutdown-part part=%d", &p1) == 1) {
        out->type = IMWrapSysexType::ShutdownPart;
        out->hasPart = true; out->part = p1;
        return true;
    }
    if (desc == "start-song") {
        out->type = IMWrapSysexType::StartSong;
        return true;
    }
    if (sscanf(desc.c_str(), "parameter-adjust part=%d param=%d value=%d", &p1, &p2, &p3) == 3) {
        out->type = IMWrapSysexType::ParameterAdjust;
        out->hasPart = true; out->part = p1; out->param = p2; out->paramValue = p3;
        return true;
    }
    if (sscanf(desc.c_str(), "hook-jump cmd=%d track=%d beat=%d tick=%d", &p1, &p2, &p3, &p4) == 4) {
        out->type = IMWrapSysexType::HookJump;
        out->hookCommand = p1; out->targetTrack = p2; out->targetBeat = p3; out->targetTick = p4;
        return true;
    }
    if (sscanf(desc.c_str(), "hook-global-transpose cmd=%d relative=%255s value=%d", &p1, s1, &p2) == 3) {
        out->type = IMWrapSysexType::HookGlobalTranspose;
        out->hookCommand = p1; out->relative = (std::string(s1) == "yes"); out->signedValue = p2;
        return true;
    }
    if (sscanf(desc.c_str(), "hook-set-transpose cmd=%d relative=%255s value=%d", &p1, s1, &p2) == 3) {
        out->type = IMWrapSysexType::HookSetTranspose;
        out->hookCommand = p1; out->relative = (std::string(s1) == "yes"); out->signedValue = p2;
        return true;
    }
    if (sscanf(desc.c_str(), "hook-part-onoff chan=%d cmd=%d value=%d", &p1, &p2, &p3) == 3) {
        out->type = IMWrapSysexType::HookPartOnOff;
        out->hasChannel = true; out->channel = p1; out->hookCommand = p2; out->value = p3;
        return true;
    }
    if (sscanf(desc.c_str(), "hook-set-volume chan=%d cmd=%d value=%d", &p1, &p2, &p3) == 3) {
        out->type = IMWrapSysexType::HookSetVolume;
        out->hasChannel = true; out->channel = p1; out->hookCommand = p2; out->value = p3;
        return true;
    }
    if (sscanf(desc.c_str(), "hook-set-program chan=%d cmd=%d value=%d", &p1, &p2, &p3) == 3) {
        out->type = IMWrapSysexType::HookSetProgram;
        out->hasChannel = true; out->channel = p1; out->hookCommand = p2; out->value = p3;
        return true;
    }
    if (desc.find("marker text=\"") == 0) {
        out->type = IMWrapSysexType::Marker;
        size_t start = desc.find_first_of('"');
        size_t end = desc.find_last_of('"');
        if (start != std::string::npos && end != std::string::npos && end > start) {
            out->markerText = desc.substr(start + 1, end - start - 1);
        }
        return true;
    }
    if (sscanf(desc.c_str(), "set-loop count=%d to=%d:%d from=%d:%d", &p1, &p2, &p3, &p4, &p5) == 5) {
        out->type = IMWrapSysexType::SetLoop;
        out->loopCount = p1; out->loopToBeat = p2; out->loopToTick = p3; out->loopFromBeat = p4; out->loopFromTick = p5;
        return true;
    }
    if (desc == "clear-loop") {
        out->type = IMWrapSysexType::ClearLoop;
        return true;
    }
    if (sscanf(desc.c_str(), "set-instrument chan=%d instrument=%d", &p1, &p2) == 2) {
        out->type = IMWrapSysexType::SetInstrument;
        out->hasChannel = true; out->channel = p1; out->instrument = p2;
        return true;
    }
    
    return false;
}

} // namespace imwrap
