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

#include "imwrap/SmfSequence.h"

#include <array>

namespace imwrap {
namespace {

constexpr std::array<char, 4> kMthd = {{'M', 'T', 'h', 'd'}};
constexpr std::array<char, 4> kMtrk = {{'M', 'T', 'r', 'k'}};

uint16_t ReadU16BE(const uint8_t *data) {
    return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

uint32_t ReadU32BE(const uint8_t *data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

bool ReadVlq(ByteView data, std::size_t *offset, uint32_t *value) {
    if (!offset || !value) {
        return false;
    }

    uint32_t result = 0;
    int count = 0;
    while (*offset < data.size() && count < 4) {
        const uint8_t byte = data.data()[(*offset)++];
        result = (result << 7) | (byte & 0x7F);
        ++count;
        if ((byte & 0x80) == 0) {
            *value = result;
            return true;
        }
    }
    return false;
}

bool ParseTrack(ByteView trackData, SmfTrack *out, std::string *error) {
    if (!out) {
        return false;
    }

    uint32_t tick = 0;
    std::size_t offset = 0;
    uint8_t runningStatus = 0;

    while (offset < trackData.size()) {
        uint32_t delta = 0;
        if (!ReadVlq(trackData, &offset, &delta)) {
            if (error) {
                *error = "failed to read MIDI delta-time";
            }
            return false;
        }
        tick += delta;

        if (offset >= trackData.size()) {
            if (error) {
                *error = "unexpected end of track after delta-time";
            }
            return false;
        }

        const uint8_t first = trackData.data()[offset++];
        MidiEvent event;
        event.tick = tick;
        event.delta = delta;

        uint8_t status = first;
        bool consumedData1 = false;
        uint8_t impliedData1 = 0;

        if (first < 0x80) {
            if (runningStatus == 0) {
                if (error) {
                    *error = "running status used without prior channel status";
                }
                return false;
            }
            status = runningStatus;
            consumedData1 = true;
            impliedData1 = first;
        }

        if (status == 0xFF) {
            event.type = MidiEventType::Meta;
            event.status = status;
            if (offset >= trackData.size()) {
                if (error) {
                    *error = "meta event missing meta type";
                }
                return false;
            }
            event.metaType = trackData.data()[offset++];

            uint32_t length = 0;
            if (!ReadVlq(trackData, &offset, &length) || offset + length > trackData.size()) {
                if (error) {
                    *error = "meta event has invalid payload length";
                }
                return false;
            }
            event.payload.assign(trackData.data() + offset, trackData.data() + offset + length);
            offset += length;
            out->events.push_back(std::move(event));
            continue;
        }

        if (status == 0xF0 || status == 0xF7) {
            event.type = MidiEventType::SysEx;
            event.status = status;
            uint32_t length = 0;
            if (!ReadVlq(trackData, &offset, &length) || offset + length > trackData.size()) {
                if (error) {
                    *error = "SysEx event has invalid payload length";
                }
                return false;
            }
            event.payload.assign(trackData.data() + offset, trackData.data() + offset + length);
            offset += length;
            out->events.push_back(std::move(event));
            continue;
        }

        const uint8_t statusClass = status & 0xF0;
        if (statusClass >= 0x80 && statusClass <= 0xE0) {
            event.type = MidiEventType::Channel;
            event.status = status;
            runningStatus = status;

            if (consumedData1) {
                event.hasData1 = true;
                event.data1 = impliedData1;
            } else {
                if (offset >= trackData.size()) {
                    if (error) {
                        *error = "channel event missing first data byte";
                    }
                    return false;
                }
                event.hasData1 = true;
                event.data1 = trackData.data()[offset++];
            }

            if (statusClass != 0xC0 && statusClass != 0xD0) {
                if (offset >= trackData.size()) {
                    if (error) {
                        *error = "channel event missing second data byte";
                    }
                    return false;
                }
                event.hasData2 = true;
                event.data2 = trackData.data()[offset++];
            }

            out->events.push_back(std::move(event));
            continue;
        }

        event.type = MidiEventType::System;
        event.status = status;
        runningStatus = 0;

        int systemLength = 0;
        switch (status) {
        case 0xF1:
        case 0xF3:
            systemLength = 1;
            break;
        case 0xF2:
            systemLength = 2;
            break;
        default:
            systemLength = 0;
            break;
        }

        if (offset + static_cast<std::size_t>(systemLength) > trackData.size()) {
            if (error) {
                *error = "system event extends beyond track data";
            }
            return false;
        }

        if (systemLength > 0) {
            event.payload.assign(trackData.data() + offset, trackData.data() + offset + systemLength);
            offset += static_cast<std::size_t>(systemLength);
        }
        out->events.push_back(std::move(event));
    }

    return true;
}

} // namespace

bool SmfParser::Parse(ByteView data, SmfSequence *out, std::string *error) {
    if (!out) {
        if (error) {
            *error = "output sequence pointer is null";
        }
        return false;
    }

    if (data.size() < 14) {
        if (error) {
            *error = "SMF data is too small";
        }
        return false;
    }

    const uint8_t *ptr = data.data();
    if (!(ptr[0] == kMthd[0] && ptr[1] == kMthd[1] && ptr[2] == kMthd[2] && ptr[3] == kMthd[3])) {
        if (error) {
            *error = "SMF data does not start with MThd";
        }
        return false;
    }

    const uint32_t headerLength = ReadU32BE(ptr + 4);
    if (headerLength < 6 || 8 + headerLength > data.size()) {
        if (error) {
            *error = "SMF header length is invalid";
        }
        return false;
    }

    SmfSequence result;
    result.format = ReadU16BE(ptr + 8);
    result.trackCount = ReadU16BE(ptr + 10);
    result.division = ReadU16BE(ptr + 12);

    std::size_t offset = 8 + headerLength;
    result.tracks.reserve(result.trackCount);
    for (uint16_t trackIndex = 0; trackIndex < result.trackCount; ++trackIndex) {
        if (offset + 8 > data.size()) {
            if (error) {
                *error = "SMF track header is truncated";
            }
            return false;
        }

        const uint8_t *trackPtr = data.data() + offset;
        if (!(trackPtr[0] == kMtrk[0] && trackPtr[1] == kMtrk[1] && trackPtr[2] == kMtrk[2] && trackPtr[3] == kMtrk[3])) {
            if (error) {
                *error = "expected MTrk chunk";
            }
            return false;
        }

        const uint32_t trackLength = ReadU32BE(trackPtr + 4);
        offset += 8;
        if (offset + trackLength > data.size()) {
            if (error) {
                *error = "SMF track data extends beyond input size";
            }
            return false;
        }

        SmfTrack track;
        if (!ParseTrack(data.subview(offset, trackLength), &track, error)) {
            return false;
        }
        result.tracks.push_back(std::move(track));
        offset += trackLength;
    }

    *out = std::move(result);
    return true;
}

void WriteU16BE(std::vector<uint8_t> *out, uint16_t value) {
    out->push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out->push_back(static_cast<uint8_t>(value & 0xFF));
}

void WriteU32BE(std::vector<uint8_t> *out, uint32_t value) {
    out->push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out->push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out->push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out->push_back(static_cast<uint8_t>(value & 0xFF));
}

void WriteVlq(std::vector<uint8_t> *out, uint32_t value) {
    uint32_t buffer = value & 0x7F;
    while ((value >>= 7) > 0) {
        buffer <<= 8;
        buffer |= 0x80;
        buffer += (value & 0x7F);
    }
    while (true) {
        out->push_back(static_cast<uint8_t>(buffer & 0xFF));
        if (buffer & 0x80) {
            buffer >>= 8;
        } else {
            break;
        }
    }
}

bool SmfSerializer::Serialize(const SmfSequence &seq, std::vector<uint8_t> *outBytes, std::string *error) {
    if (!outBytes) return false;
    outBytes->clear();

    outBytes->insert(outBytes->end(), kMthd.begin(), kMthd.end());
    WriteU32BE(outBytes, 6);
    WriteU16BE(outBytes, seq.format);
    WriteU16BE(outBytes, static_cast<uint16_t>(seq.tracks.size()));
    WriteU16BE(outBytes, seq.division);

    for (const auto &track : seq.tracks) {
        outBytes->insert(outBytes->end(), kMtrk.begin(), kMtrk.end());
        std::size_t lengthOffset = outBytes->size();
        WriteU32BE(outBytes, 0); // placeholder for length
        std::size_t trackStart = outBytes->size();

        for (const auto &event : track.events) {
            WriteVlq(outBytes, event.delta);
            outBytes->push_back(event.status);

            if (event.type == MidiEventType::Channel) {
                if (event.hasData1) outBytes->push_back(event.data1);
                if (event.hasData2) outBytes->push_back(event.data2);
            } else if (event.type == MidiEventType::Meta) {
                outBytes->push_back(event.metaType);
                WriteVlq(outBytes, static_cast<uint32_t>(event.payload.size()));
                outBytes->insert(outBytes->end(), event.payload.begin(), event.payload.end());
            } else if (event.type == MidiEventType::SysEx || event.type == MidiEventType::System) {
                if (event.type == MidiEventType::SysEx) {
                    WriteVlq(outBytes, static_cast<uint32_t>(event.payload.size()));
                }
                outBytes->insert(outBytes->end(), event.payload.begin(), event.payload.end());
            }
        }

        uint32_t trackLength = static_cast<uint32_t>(outBytes->size() - trackStart);
        (*outBytes)[lengthOffset] = static_cast<uint8_t>((trackLength >> 24) & 0xFF);
        (*outBytes)[lengthOffset + 1] = static_cast<uint8_t>((trackLength >> 16) & 0xFF);
        (*outBytes)[lengthOffset + 2] = static_cast<uint8_t>((trackLength >> 8) & 0xFF);
        (*outBytes)[lengthOffset + 3] = static_cast<uint8_t>(trackLength & 0xFF);
    }

    return true;
}

} // namespace imwrap
