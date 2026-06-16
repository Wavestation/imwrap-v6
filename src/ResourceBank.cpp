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

#include "imuse/ResourceBank.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <sstream>

namespace imuse {
namespace {

constexpr std::array<char, 4> kImsb = {{'I', 'M', 'S', 'B'}};
constexpr std::array<char, 4> kIhdr = {{'I', 'H', 'D', 'R'}};
constexpr std::array<char, 4> kSdir = {{'S', 'D', 'I', 'R'}};
constexpr std::array<char, 4> kSoun = {{'S', 'O', 'U', 'N'}};
constexpr std::array<char, 4> kSidn = {{'S', 'I', 'D', 'N'}};
constexpr std::array<char, 4> kName = {{'N', 'A', 'M', 'E'}};
constexpr std::array<char, 4> kGmd  = {{'G', 'M', 'D', ' '}};
constexpr std::array<char, 4> kRol  = {{'R', 'O', 'L', ' '}};
constexpr std::array<char, 4> kAdl  = {{'A', 'D', 'L', ' '}};
constexpr std::array<char, 4> kMdhd = {{'M', 'D', 'h', 'd'}};

uint16_t ReadU16BE(const uint8_t *data) {
    return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

uint32_t ReadU32BE(const uint8_t *data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

bool ParseChunk(ByteView data, std::size_t offset, ChunkHeader *header, ByteView *body) {
    if (!header || !body || offset > data.size() || data.size() - offset < 8) {
        return false;
    }

    const uint8_t *ptr = data.data() + offset;
    header->id = {{static_cast<char>(ptr[0]), static_cast<char>(ptr[1]), static_cast<char>(ptr[2]), static_cast<char>(ptr[3])}};
    header->size = ReadU32BE(ptr + 4);

    if (header->size < 8) {
        return false;
    }

    if (offset + header->size > data.size()) {
        return false;
    }

    *body = data.subview(offset + 8, header->size - 8);
    return true;
}

bool ParseMdhd(ByteView body, MdhdData *out) {
    if (!out || body.size() != 8) {
        return false;
    }

    out->present = true;
    out->priority = body.data()[2];
    out->volume = body.data()[3];
    out->pan = static_cast<int8_t>(body.data()[4]);
    out->transpose = static_cast<int8_t>(body.data()[5]);
    out->detune = static_cast<int8_t>(body.data()[6]);
    out->speed = body.data()[7];
    return true;
}

bool ParseVariant(ByteView variantData, VariantKind kind, SoundVariantView *out, std::string *error) {
    if (!out) {
        return false;
    }

    MdhdData mdhd = MdhdData::Defaults();
    std::size_t offset = 0;

    if (variantData.size() >= 8) {
        const uint8_t *ptr = variantData.data();
        if (ptr[0] == kMdhd[0] && ptr[1] == kMdhd[1] && ptr[2] == kMdhd[2] && ptr[3] == kMdhd[3]) {
            ChunkHeader header;
            ByteView body;
            if (!ParseChunk(variantData, 0, &header, &body)) {
                if (error) {
                    *error = "variant MDhd chunk malformed";
                }
                return false;
            }

            if (!ParseMdhd(body, &mdhd)) {
                if (error) {
                    *error = "invalid MDhd chunk";
                }
                return false;
            }

            offset = header.size;
        }
    }

    if (variantData.size() - offset < 4) {
        if (error) {
            *error = "variant too small to contain MThd";
        }
        return false;
    }

    const uint8_t *smfStart = variantData.data() + offset;
    if (!(smfStart[0] == 'M' && smfStart[1] == 'T' && smfStart[2] == 'h' && smfStart[3] == 'd')) {
        if (error) {
            *error = "variant does not begin with MThd after optional MDhd";
        }
        return false;
    }

    ByteView smfData = variantData.subview(offset, variantData.size() - offset);
    if (smfData.empty()) {
        if (error) {
            *error = "variant does not contain an MThd chunk";
        }
        return false;
    }

    out->kind = kind;
    out->chunkData = variantData;
    out->smfData = smfData;
    out->mdhd = mdhd;
    return true;
}

} // namespace

std::string FourCCToString(const std::array<char, 4> &id) {
    return std::string(id.begin(), id.end());
}

bool SoundResource::hasVariant(VariantKind kind) const {
    return HasVariant(_variantMask, kind);
}

SoundVariantView SoundResource::selectVariant(TargetProfile profile) const {
    if (!_valid) {
        return {};
    }

    if (profile == TargetProfile::Adlib) {
        if (_adl.valid()) {
            return _adl;
        }
        if (_rol.valid()) {
            return _rol;
        }
        return _gmd;
    }

    if (profile == TargetProfile::Mt32) {
        if (_rol.valid()) {
            return _rol;
        }
        if (_adl.valid()) {
            return _adl;
        }
        return _gmd;
    }

    if (_gmd.valid()) {
        return _gmd;
    }
    return _rol;
}

bool ResourceBank::openFromFile(const std::string &path, std::string *error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) {
            *error = "failed to open file: " + path;
        }
        return false;
    }

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return openFromMemory(std::move(bytes), error);
}

bool ResourceBank::openFromMemory(std::vector<uint8_t> data, std::string *error) {
    _bytes = std::move(data);
    _header = {};
    _directory.clear();
    return parse(error);
}

bool ResourceBank::hasSound(uint16_t soundId) const {
    return _directory.find(soundId) != _directory.end();
}

std::vector<uint16_t> ResourceBank::soundIds() const {
    std::vector<uint16_t> ids;
    ids.reserve(_directory.size());
    for (const auto &entry : _directory) {
        ids.push_back(entry.first);
    }
    return ids;
}

SoundResource ResourceBank::loadSound(uint16_t soundId, std::string *error) const {
    SoundResource result;

    auto it = _directory.find(soundId);
    if (it == _directory.end()) {
        if (error) {
            *error = "sound id not found";
        }
        return result;
    }

    const DirectoryEntry &dir = it->second;
    ByteView fileView(_bytes.data(), _bytes.size());

    ChunkHeader sounHeader;
    ByteView sounBody;
    if (!ParseChunk(fileView, dir.soundOffset, &sounHeader, &sounBody) || sounHeader.id != kSoun) {
        if (error) {
            *error = "directory points to an invalid SOUN chunk";
        }
        return result;
    }

    uint16_t sidn = 0;
    bool hasSidn = false;
    std::size_t offset = 0;

    while (offset < sounBody.size()) {
        ChunkHeader header;
        ByteView body;
        if (!ParseChunk(sounBody, offset, &header, &body)) {
            if (error) {
                *error = "malformed SOUN chunk";
            }
            return {};
        }

        if (header.id == kSidn) {
            if (body.size() != 4) {
                if (error) {
                    *error = "invalid SIDN chunk";
                }
                return {};
            }
            sidn = ReadU16BE(body.data());
            hasSidn = true;
        }
        else if (header.id == kName) {
            result._name.assign(reinterpret_cast<const char *>(body.data()), body.size());
        }
        else if (header.id == kGmd) {
            if (!ParseVariant(body, VariantKind::Gmd, &result._gmd, error)) {
                return {};
            }
            result._variantMask |= static_cast<uint16_t>(VariantKind::Gmd);
        }
        else if (header.id == kRol) {
            if (!ParseVariant(body, VariantKind::Rol, &result._rol, error)) {
                return {};
            }
            result._variantMask |= static_cast<uint16_t>(VariantKind::Rol);
        }
        else if (header.id == kAdl) {
            if (!ParseVariant(body, VariantKind::Adl, &result._adl, error)) {
                return {};
            }
            result._variantMask |= static_cast<uint16_t>(VariantKind::Adl);
        }

        offset += header.size;
    }

    if (!hasSidn) {
        if (error) {
            *error = "SOUN chunk is missing SIDN";
        }
        return {};
    }

    if (sidn != soundId) {
        if (error) {
            *error = "SIDN does not match directory sound id";
        }
        return {};
    }

    if (result._variantMask == 0) {
        if (error) {
            *error = "SOUN chunk has no playable variants";
        }
        return {};
    }

    result._valid = true;
    result._soundId = sidn;
    return result;
}

bool ResourceBank::parse(std::string *error) {
    if (_bytes.size() < 8) {
        if (error) {
            *error = "file too small to be a valid IMS bank";
        }
        return false;
    }

    ByteView fileView(_bytes.data(), _bytes.size());
    ChunkHeader rootHeader;
    ByteView rootBody;
    if (!ParseChunk(fileView, 0, &rootHeader, &rootBody) || rootHeader.id != kImsb) {
        if (error) {
            *error = "root chunk is not IMSB";
        }
        return false;
    }

    std::size_t offset = 0;
    bool haveHeader = false;
    bool haveDirectory = false;

    while (offset < rootBody.size()) {
        ChunkHeader header;
        ByteView body;
        if (!ParseChunk(rootBody, offset, &header, &body)) {
            if (error) {
                *error = "malformed chunk in IMSB body";
            }
            return false;
        }

        if (header.id == kIhdr) {
            if (body.size() != 16) {
                if (error) {
                    *error = "IHDR must contain exactly 16 bytes";
                }
                return false;
            }
            _header.versionMajor = ReadU16BE(body.data());
            _header.versionMinor = ReadU16BE(body.data() + 2);
            _header.soundCount = ReadU32BE(body.data() + 4);
            _header.flags = ReadU32BE(body.data() + 8);
            _header.reserved = ReadU32BE(body.data() + 12);
            haveHeader = true;
        }
        else if (header.id == kSdir) {
            if (body.size() % 12 != 0) {
                if (error) {
                    *error = "SDIR size is not a multiple of 12 bytes";
                }
                return false;
            }

            const std::size_t entryCount = body.size() / 12;
            for (std::size_t i = 0; i < entryCount; ++i) {
                const uint8_t *entry = body.data() + (i * 12);
                DirectoryEntry dir;
                dir.soundId = ReadU16BE(entry);
                dir.variantMask = ReadU16BE(entry + 2);
                dir.soundOffset = ReadU32BE(entry + 4);
                dir.soundSize = ReadU32BE(entry + 8);
                _directory[dir.soundId] = dir;
            }
            haveDirectory = true;
        }

        offset += header.size;
    }

    if (!haveHeader) {
        if (error) {
            *error = "IMSB is missing IHDR";
        }
        return false;
    }

    if (!haveDirectory) {
        if (error) {
            *error = "IMSB is missing SDIR";
        }
        return false;
    }

    if (_header.versionMajor != 1) {
        if (error) {
            std::ostringstream oss;
            oss << "unsupported IMS major version: " << _header.versionMajor;
            *error = oss.str();
        }
        return false;
    }

    if (_header.soundCount != _directory.size()) {
        if (error) {
            *error = "IHDR sound_count does not match SDIR entries";
        }
        return false;
    }

    for (const auto &pair : _directory) {
        const DirectoryEntry &dir = pair.second;
        if (dir.soundOffset + dir.soundSize > _bytes.size()) {
            if (error) {
                *error = "directory entry points outside the file";
            }
            return false;
        }
    }

    return true;
}

} // namespace imuse
