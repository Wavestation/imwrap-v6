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

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "imuse/ImsWriter.h"

namespace {

using imuse::MdhdData;
using imuse::SoundBankInput;
using imuse::VariantKind;
using imuse::VariantSource;

void PrintUsage() {
    std::cerr
        << "Usage:\n"
        << "  imusepack build output.ims [--name=80:Woodtick] [--mdhd=80:gmd:90:127:0:0:0:128] 80:gmd=file.mid [80:rol=file.mid...]\n";
}

bool SplitOnce(const std::string &value, char sep, std::string *lhs, std::string *rhs) {
    const std::size_t pos = value.find(sep);
    if (pos == std::string::npos) {
        return false;
    }
    *lhs = value.substr(0, pos);
    *rhs = value.substr(pos + 1);
    return true;
}

bool ParseVariantName(const std::string &value, VariantKind *kind) {
    if (value == "gmd") {
        *kind = VariantKind::Gmd;
        return true;
    }
    if (value == "rol") {
        *kind = VariantKind::Rol;
        return true;
    }
    return false;
}

bool ParseUnsigned16(const std::string &value, uint16_t *out) {
    char *end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed < 0 || parsed > 0xFFFF) {
        return false;
    }
    *out = static_cast<uint16_t>(parsed);
    return true;
}

bool ParseSigned8(const std::string &value, int8_t *out) {
    char *end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed < -128 || parsed > 127) {
        return false;
    }
    *out = static_cast<int8_t>(parsed);
    return true;
}

bool ParseUnsigned8(const std::string &value, uint8_t *out) {
    char *end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed < 0 || parsed > 255) {
        return false;
    }
    *out = static_cast<uint8_t>(parsed);
    return true;
}

bool ParseSpec(const std::string &spec, uint16_t *soundId, VariantKind *kind, std::string *path) {
    std::string lhs;
    std::string rhs;
    if (!SplitOnce(spec, '=', &lhs, &rhs)) {
        return false;
    }

    std::string soundPart;
    std::string variantPart;
    if (!SplitOnce(lhs, ':', &soundPart, &variantPart)) {
        return false;
    }

    return ParseUnsigned16(soundPart, soundId) &&
           ParseVariantName(variantPart, kind) &&
           ((*path = rhs), true);
}

bool ParseNameValue(const std::string &arg, uint16_t *soundId, std::string *name) {
    const std::string prefix = "--name=";
    if (arg.rfind(prefix, 0) != 0) {
        return false;
    }
    std::string soundPart;
    std::string valuePart;
    if (!SplitOnce(arg.substr(prefix.size()), ':', &soundPart, &valuePart)) {
        return false;
    }
    return ParseUnsigned16(soundPart, soundId) && ((*name = valuePart), true);
}

bool ParseMdhdValue(const std::string &arg, uint16_t *soundId, VariantKind *kind, MdhdData *mdhd) {
    const std::string prefix = "--mdhd=";
    if (arg.rfind(prefix, 0) != 0) {
        return false;
    }

    std::string payload = arg.substr(prefix.size());
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= payload.size()) {
        const std::size_t next = payload.find(':', start);
        if (next == std::string::npos) {
            parts.push_back(payload.substr(start));
            break;
        }
        parts.push_back(payload.substr(start, next - start));
        start = next + 1;
    }

    if (parts.size() != 8) {
        return false;
    }

    mdhd->present = true;
    return ParseUnsigned16(parts[0], soundId) &&
           ParseVariantName(parts[1], kind) &&
           ParseUnsigned8(parts[2], &mdhd->priority) &&
           ParseUnsigned8(parts[3], &mdhd->volume) &&
           ParseSigned8(parts[4], &mdhd->pan) &&
           ParseSigned8(parts[5], &mdhd->transpose) &&
           ParseSigned8(parts[6], &mdhd->detune) &&
           ParseUnsigned8(parts[7], &mdhd->speed);
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 4) {
        PrintUsage();
        return 1;
    }

    const std::string command = argv[1];
    if (command != "build") {
        PrintUsage();
        return 1;
    }

    const std::string outputPath = argv[2];
    std::map<uint16_t, SoundBankInput> sounds;
    std::map<std::pair<uint16_t, uint16_t>, MdhdData> mdhdOverrides;

    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];

        uint16_t soundId = 0;
        VariantKind kind = VariantKind::None;
        MdhdData mdhd = MdhdData::Defaults();
        std::string value;

        if (ParseNameValue(arg, &soundId, &value)) {
            sounds[soundId].soundId = soundId;
            sounds[soundId].name = value;
            continue;
        }

        if (ParseMdhdValue(arg, &soundId, &kind, &mdhd)) {
            mdhdOverrides[{soundId, static_cast<uint16_t>(kind)}] = mdhd;
            continue;
        }

        std::string path;
        if (!ParseSpec(arg, &soundId, &kind, &path)) {
            std::cerr << "Invalid argument: " << arg << "\n";
            PrintUsage();
            return 1;
        }

        SoundBankInput &sound = sounds[soundId];
        sound.soundId = soundId;

        VariantSource variant;
        variant.kind = kind;
        variant.sourcePath = path;

        auto mdhdIt = mdhdOverrides.find({soundId, static_cast<uint16_t>(kind)});
        if (mdhdIt != mdhdOverrides.end()) {
            variant.includeMdhd = true;
            variant.mdhd = mdhdIt->second;
        }

        sound.variants.push_back(variant);
    }

    std::vector<SoundBankInput> inputs;
    inputs.reserve(sounds.size());
    for (auto &entry : sounds) {
        for (VariantSource &variant : entry.second.variants) {
            auto mdhdIt = mdhdOverrides.find({entry.first, static_cast<uint16_t>(variant.kind)});
            if (mdhdIt != mdhdOverrides.end()) {
                variant.includeMdhd = true;
                variant.mdhd = mdhdIt->second;
            }
        }
        inputs.push_back(std::move(entry.second));
    }

    imuse::ImsWriter writer;
    std::string error;
    if (!writer.writeFile(outputPath, inputs, &error)) {
        std::cerr << "imusepack: " << error << "\n";
        return 1;
    }

    std::cout << "Wrote " << outputPath << " with " << inputs.size() << " sound(s).\n";
    return 0;
}
