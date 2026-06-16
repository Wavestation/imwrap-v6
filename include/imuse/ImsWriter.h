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

#ifndef IMUSE_IMS_WRITER_H
#define IMUSE_IMS_WRITER_H

#include <cstdint>
#include <string>
#include <vector>

#include "imuse/ImsFormat.h"

namespace imuse {

struct VariantSource {
    VariantKind kind = VariantKind::None;
    std::string sourcePath;
    bool includeMdhd = false;
    MdhdData mdhd = MdhdData::Defaults();
};

struct SoundBankInput {
    uint16_t soundId = 0;
    std::string name;
    std::vector<VariantSource> variants;
};

class ImsWriter {
public:
    bool build(const std::vector<SoundBankInput> &sounds, std::vector<uint8_t> *outBytes, std::string *error = nullptr) const;
    bool writeFile(const std::string &outputPath, const std::vector<SoundBankInput> &sounds, std::string *error = nullptr) const;
};

} // namespace imuse

#endif
