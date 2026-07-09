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

#ifndef IMWRAP_IMS_FORMAT_H
#define IMWRAP_IMS_FORMAT_H

#include <array>
#include <cstdint>
#include <string>

namespace imwrap {

struct ChunkHeader {
    std::array<char, 4> id = {{0, 0, 0, 0}};
    uint32_t size = 0;
};

struct ImsHeader {
    uint16_t versionMajor = 0;
    uint16_t versionMinor = 0;
    uint32_t soundCount = 0;
    uint32_t flags = 0;
    uint32_t reserved = 0;
};

enum class VariantKind : uint16_t {
    None = 0,
    Gmd  = 1u << 0,
    Rol  = 1u << 1,
    Adl  = 1u << 2
};

enum class TargetProfile {
    GeneralMidi,
    Mt32,
    Adlib
};

struct DirectoryEntry {
    uint16_t soundId = 0;
    uint16_t variantMask = 0;
    uint32_t soundOffset = 0;
    uint32_t soundSize = 0;
};

struct MdhdData {
    bool present = false;
    uint8_t priority = 128;
    uint8_t volume = 127;
    int8_t pan = 0;
    int8_t transpose = 0;
    int8_t detune = 0;
    uint8_t speed = 128;

    static MdhdData Defaults() { return MdhdData(); }
};

inline VariantKind operator|(VariantKind lhs, VariantKind rhs) {
    return static_cast<VariantKind>(static_cast<uint16_t>(lhs) | static_cast<uint16_t>(rhs));
}

inline bool HasVariant(uint16_t mask, VariantKind kind) {
    return (mask & static_cast<uint16_t>(kind)) != 0;
}

std::string FourCCToString(const std::array<char, 4> &id);

} // namespace imwrap

#endif
