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

#ifndef IMUSE_RESOURCE_BANK_H
#define IMUSE_RESOURCE_BANK_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "imuse/ByteView.h"
#include "imuse/ImsFormat.h"

namespace imuse {

struct SoundVariantView {
    VariantKind kind = VariantKind::None;
    ByteView chunkData;
    ByteView smfData;
    MdhdData mdhd;

    bool valid() const { return !smfData.empty(); }
};

class SoundResource {
public:
    bool valid() const { return _valid; }
    uint16_t soundId() const { return _soundId; }
    uint16_t variantMask() const { return _variantMask; }
    const std::string &name() const { return _name; }

    bool hasVariant(VariantKind kind) const;
    SoundVariantView selectVariant(TargetProfile profile) const;

private:
    friend class ResourceBank;

    bool _valid = false;
    uint16_t _soundId = 0;
    uint16_t _variantMask = 0;
    std::string _name;
    SoundVariantView _gmd;
    SoundVariantView _rol;
};

class ResourceBank {
public:
    bool openFromFile(const std::string &path, std::string *error = nullptr);
    bool openFromMemory(std::vector<uint8_t> data, std::string *error = nullptr);

    bool isOpen() const { return !_bytes.empty(); }
    const ImsHeader &header() const { return _header; }

    bool hasSound(uint16_t soundId) const;
    SoundResource loadSound(uint16_t soundId, std::string *error = nullptr) const;
    std::vector<uint16_t> soundIds() const;

private:
    bool parse(std::string *error);

    std::vector<uint8_t> _bytes;
    ImsHeader _header;
    std::map<uint16_t, DirectoryEntry> _directory;
};

} // namespace imuse

#endif
