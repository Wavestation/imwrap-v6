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

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ImsProject.h"

namespace {

namespace project = imwrap::gui;

using imwrap::MdhdData;
using imwrap::SmfSequence;
using imwrap::SmfSerializer;
using imwrap::VariantKind;

struct BuildSpec {
    uint16_t soundId = 0;
    VariantKind kind = VariantKind::None;
    std::string path;
};

void PrintUsage() {
    std::cout
        << "Usage:\n"
        << "  imwrappack inspect bank.ims\n"
        << "  imwrappack create bank.ims\n"
        << "  imwrappack build output.ims [--name=80:Forest] [--mdhd=80:gmd:90:127:0:0:0:128] 80:gmd=file.mid [80:adl=file.mid...]\n"
        << "  imwrappack add-sound bank.ims soundId variant [--name=Forest] [--output=other.ims]\n"
        << "  imwrappack set-sound bank.ims soundId [--name=Forest] [--new-id=81] [--output=other.ims]\n"
        << "  imwrappack delete-sound bank.ims soundId [--output=other.ims]\n"
        << "  imwrappack add-variant bank.ims soundId variant [--output=other.ims]\n"
        << "  imwrappack delete-variant bank.ims soundId variant [--output=other.ims]\n"
        << "  imwrappack import-midi bank.ims soundId variant file.mid [more.mid ...] [--name=Forest] [--output=other.ims]\n"
        << "  imwrappack set-mdhd bank.ims soundId variant priority volume pan transpose detune speed [--output=other.ims]\n"
        << "  imwrappack clear-mdhd bank.ims soundId variant [--output=other.ims]\n"
        << "  imwrappack delete-track bank.ims soundId variant trackIndex [--output=other.ims]\n"
        << "  imwrappack move-track bank.ims soundId variant trackIndex up|down [--output=other.ims]\n"
        << "  imwrappack export-track bank.ims soundId variant trackIndex output.mid\n"
        << "\n"
        << "Notes:\n"
        << "  - Variants are: gmd, rol, adl\n"
        << "  - Track indices are zero-based\n"
        << "  - MIDI import follows the GUI rules: SMF0/2 tracks are imported as-is,\n"
        << "    SMF1 files are merged into a single format-0 style track.\n";
}

bool SplitOnce(const std::string& value, char sep, std::string* lhs, std::string* rhs) {
    const std::size_t pos = value.find(sep);
    if (pos == std::string::npos) {
        return false;
    }
    *lhs = value.substr(0, pos);
    *rhs = value.substr(pos + 1);
    return true;
}

bool ParseUnsigned16(const std::string& value, uint16_t* out) {
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed < 0 || parsed > 0xFFFF) {
        return false;
    }
    *out = static_cast<uint16_t>(parsed);
    return true;
}

bool ParseUnsigned8(const std::string& value, uint8_t* out) {
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed < 0 || parsed > 255) {
        return false;
    }
    *out = static_cast<uint8_t>(parsed);
    return true;
}

bool ParseSigned8(const std::string& value, int8_t* out) {
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed < -128 || parsed > 127) {
        return false;
    }
    *out = static_cast<int8_t>(parsed);
    return true;
}

bool ParseIndex(const std::string& value, std::size_t* out) {
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(value.c_str(), &end, 10);
    if (!end || *end != '\0') {
        return false;
    }
    *out = static_cast<std::size_t>(parsed);
    return true;
}

bool ParseVariantName(const std::string& value, VariantKind* kind) {
    if (value == "gmd") {
        *kind = VariantKind::Gmd;
        return true;
    }
    if (value == "rol") {
        *kind = VariantKind::Rol;
        return true;
    }
    if (value == "adl") {
        *kind = VariantKind::Adl;
        return true;
    }
    return false;
}

bool ParseBuildSpec(const std::string& spec, BuildSpec* out) {
    std::string lhs;
    if (!SplitOnce(spec, '=', &lhs, &out->path)) {
        return false;
    }

    std::string soundPart;
    std::string variantPart;
    if (!SplitOnce(lhs, ':', &soundPart, &variantPart)) {
        return false;
    }

    return ParseUnsigned16(soundPart, &out->soundId) &&
           ParseVariantName(variantPart, &out->kind);
}

bool ParseSoundNameMapping(const std::string& arg, uint16_t* soundId, std::string* name) {
    static const std::string prefix = "--name=";
    if (arg.rfind(prefix, 0) != 0) {
        return false;
    }
    std::string soundPart;
    return SplitOnce(arg.substr(prefix.size()), ':', &soundPart, name) &&
           ParseUnsigned16(soundPart, soundId);
}

bool ParseOutputOption(const std::string& arg, std::string* path) {
    static const std::string prefix = "--output=";
    if (arg.rfind(prefix, 0) != 0) {
        return false;
    }
    *path = arg.substr(prefix.size());
    return !path->empty();
}

bool ParseNewIdOption(const std::string& arg, uint16_t* soundId) {
    static const std::string prefix = "--new-id=";
    if (arg.rfind(prefix, 0) != 0) {
        return false;
    }
    return ParseUnsigned16(arg.substr(prefix.size()), soundId);
}

bool ParseMdhdMapping(const std::string& arg, uint16_t* soundId, VariantKind* kind, MdhdData* mdhd) {
    static const std::string prefix = "--mdhd=";
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

project::ProjectSound* FindSound(project::ImsProjectDocument* document, uint16_t soundId) {
    return document ? document->findSound(soundId) : nullptr;
}

const project::ProjectSound* FindSound(const project::ImsProjectDocument& document, uint16_t soundId) {
    return document.findSound(soundId);
}

project::ProjectVariant* FindVariant(project::ProjectSound* sound, VariantKind kind) {
    if (!sound) {
        return nullptr;
    }
    for (project::ProjectVariant& variant : sound->variants) {
        if (variant.kind == kind) {
            return &variant;
        }
    }
    return nullptr;
}

const project::ProjectVariant* FindVariant(const project::ProjectSound& sound, VariantKind kind) {
    for (const project::ProjectVariant& variant : sound.variants) {
        if (variant.kind == kind) {
            return &variant;
        }
    }
    return nullptr;
}

project::ProjectSound& EnsureSound(project::ImsProjectDocument* document, uint16_t soundId) {
    if (project::ProjectSound* sound = document->findSound(soundId)) {
        return *sound;
    }

    project::ProjectSound sound;
    sound.id = soundId;
    document->sounds().push_back(std::move(sound));
    return document->sounds().back();
}

project::ProjectVariant& EnsureVariant(project::ProjectSound* sound, VariantKind kind) {
    if (project::ProjectVariant* variant = FindVariant(sound, kind)) {
        return *variant;
    }

    project::ProjectVariant variant;
    variant.kind = kind;
    variant.includeVariant = true;
    sound->variants.push_back(std::move(variant));
    return sound->variants.back();
}

bool LoadDocument(const std::string& path, project::ImsProjectDocument* document, std::string* error) {
    if (!document) {
        if (error) {
            *error = "document pointer is null";
        }
        return false;
    }
    return document->loadFromFile(path, error);
}

bool SaveDocument(const std::string& inputPath, const std::string& outputOverride, const project::ImsProjectDocument& document, std::string* error) {
    const std::string targetPath = outputOverride.empty() ? inputPath : outputOverride;
    return document.saveToFile(targetPath, error);
}

std::string VariantTag(VariantKind kind) {
    switch (kind) {
    case VariantKind::Gmd:
        return "gmd";
    case VariantKind::Rol:
        return "rol";
    case VariantKind::Adl:
        return "adl";
    case VariantKind::None:
    default:
        return "none";
    }
}

void PrintDocumentSummary(const project::ImsProjectDocument& document) {
    std::cout << "Sounds: " << document.sounds().size() << "\n";
    for (const project::ProjectSound& sound : document.sounds()) {
        std::cout << "- " << project::FormatSoundLabel(sound) << "\n";
        for (const project::ProjectVariant& variant : sound.variants) {
            if (!variant.includeVariant) {
                continue;
            }

            std::cout
                << "    * " << VariantTag(variant.kind)
                << " tracks=" << variant.tracks.size()
                << " division=" << variant.division;

            if (variant.includeMdhd) {
                std::cout
                    << " mdhd=("
                    << static_cast<int>(variant.mdhd.priority) << ","
                    << static_cast<int>(variant.mdhd.volume) << ","
                    << static_cast<int>(variant.mdhd.pan) << ","
                    << static_cast<int>(variant.mdhd.transpose) << ","
                    << static_cast<int>(variant.mdhd.detune) << ","
                    << static_cast<int>(variant.mdhd.speed) << ")";
            }

            std::cout << "\n";

            for (std::size_t i = 0; i < variant.tracks.size(); ++i) {
                const project::ProjectTrack& track = variant.tracks[i];
                std::cout
                    << "        [" << i << "] "
                    << track.name
                    << " source=" << track.sourceFileName
                    << " events=" << track.events.size()
                    << "\n";
            }
        }
    }
}

int CommandInspect(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        PrintUsage();
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(args[0], &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    PrintDocumentSummary(document);
    return 0;
}

int CommandCreate(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        PrintUsage();
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!document.saveToFile(args[0], &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Created " << args[0] << "\n";
    return 0;
}

int CommandBuild(const std::vector<std::string>& args) {
    if (args.empty()) {
        PrintUsage();
        return 1;
    }

    const std::string outputPath = args[0];
    std::vector<BuildSpec> specs;
    std::map<uint16_t, std::string> soundNames;
    std::map<std::pair<uint16_t, uint16_t>, MdhdData> mdhdOverrides;

    for (std::size_t i = 1; i < args.size(); ++i) {
        uint16_t soundId = 0;
        VariantKind kind = VariantKind::None;
        MdhdData mdhd = MdhdData::Defaults();
        std::string value;
        BuildSpec spec;

        if (ParseSoundNameMapping(args[i], &soundId, &value)) {
            soundNames[soundId] = value;
            continue;
        }

        if (ParseMdhdMapping(args[i], &soundId, &kind, &mdhd)) {
            mdhdOverrides[{soundId, static_cast<uint16_t>(kind)}] = mdhd;
            continue;
        }

        if (!ParseBuildSpec(args[i], &spec)) {
            std::cerr << "imwrappack: invalid build argument: " << args[i] << "\n";
            return 1;
        }
        specs.push_back(std::move(spec));
    }

    project::ImsProjectDocument document;
    std::string error;

    for (const BuildSpec& spec : specs) {
        project::ProjectSound& sound = EnsureSound(&document, spec.soundId);
        auto nameIt = soundNames.find(spec.soundId);
        if (nameIt != soundNames.end()) {
            sound.name = nameIt->second;
        }

        project::ProjectVariant& variant = EnsureVariant(&sound, spec.kind);
        auto mdhdIt = mdhdOverrides.find({spec.soundId, static_cast<uint16_t>(spec.kind)});
        if (mdhdIt != mdhdOverrides.end()) {
            variant.includeMdhd = true;
            variant.mdhd = mdhdIt->second;
        }

        if (!project::ImportMidiFileToVariant(spec.path, &variant, &error)) {
            std::cerr << "imwrappack: " << error << "\n";
            return 1;
        }
    }

    for (const auto& entry : soundNames) {
        project::ProjectSound* sound = FindSound(&document, entry.first);
        if (!sound) {
            std::cerr << "imwrappack: --name references sound " << entry.first << " without any imported variant\n";
            return 1;
        }
        sound->name = entry.second;
    }

    for (const auto& entry : mdhdOverrides) {
        const uint16_t soundId = entry.first.first;
        const VariantKind kind = static_cast<VariantKind>(entry.first.second);
        project::ProjectSound* sound = FindSound(&document, soundId);
        project::ProjectVariant* variant = sound ? FindVariant(sound, kind) : nullptr;
        if (!variant) {
            std::cerr << "imwrappack: --mdhd references " << soundId << ":" << VariantTag(kind) << " without any imported variant\n";
            return 1;
        }
        variant->includeMdhd = true;
        variant->mdhd = entry.second;
    }

    if (!document.saveToFile(outputPath, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Wrote " << outputPath << " with " << document.sounds().size() << " sound(s).\n";
    return 0;
}

int CommandAddSound(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        PrintUsage();
        return 1;
    }

    const std::string inputPath = args[0];
    uint16_t soundId = 0;
    VariantKind kind = VariantKind::None;
    if (!ParseUnsigned16(args[1], &soundId) || !ParseVariantName(args[2], &kind)) {
        PrintUsage();
        return 1;
    }

    std::string outputPath;
    std::string name;
    for (std::size_t i = 3; i < args.size(); ++i) {
        if (ParseOutputOption(args[i], &outputPath)) {
            continue;
        }
        if (args[i].rfind("--name=", 0) == 0) {
            name = args[i].substr(std::strlen("--name="));
            continue;
        }
        std::cerr << "imwrappack: unknown option: " << args[i] << "\n";
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(inputPath, &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    if (FindSound(document, soundId)) {
        std::cerr << "imwrappack: sound " << soundId << " already exists\n";
        return 1;
    }

    project::ProjectSound& sound = EnsureSound(&document, soundId);
    sound.name = name;
    EnsureVariant(&sound, kind);

    if (!SaveDocument(inputPath, outputPath, document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Added sound " << soundId << " with variant " << VariantTag(kind) << "\n";
    return 0;
}

int CommandSetSound(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        PrintUsage();
        return 1;
    }

    const std::string inputPath = args[0];
    uint16_t soundId = 0;
    if (!ParseUnsigned16(args[1], &soundId)) {
        PrintUsage();
        return 1;
    }

    std::string outputPath;
    std::string newName;
    uint16_t newId = soundId;
    bool hasNewId = false;
    bool hasNewName = false;

    for (std::size_t i = 2; i < args.size(); ++i) {
        if (ParseOutputOption(args[i], &outputPath)) {
            continue;
        }
        if (ParseNewIdOption(args[i], &newId)) {
            hasNewId = true;
            continue;
        }
        if (args[i].rfind("--name=", 0) == 0) {
            newName = args[i].substr(std::strlen("--name="));
            hasNewName = true;
            continue;
        }
        std::cerr << "imwrappack: unknown option: " << args[i] << "\n";
        return 1;
    }

    if (!hasNewId && !hasNewName) {
        std::cerr << "imwrappack: set-sound requires --name and/or --new-id\n";
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(inputPath, &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    project::ProjectSound* sound = FindSound(&document, soundId);
    if (!sound) {
        std::cerr << "imwrappack: sound " << soundId << " not found\n";
        return 1;
    }

    if (hasNewId && newId != soundId && FindSound(document, newId)) {
        std::cerr << "imwrappack: sound " << newId << " already exists\n";
        return 1;
    }

    if (hasNewName) {
        sound->name = newName;
    }
    if (hasNewId) {
        sound->id = newId;
    }

    if (!SaveDocument(inputPath, outputPath, document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Updated sound " << soundId << "\n";
    return 0;
}

int CommandDeleteSound(const std::vector<std::string>& args) {
    if (args.size() < 2 || args.size() > 3) {
        PrintUsage();
        return 1;
    }

    const std::string inputPath = args[0];
    uint16_t soundId = 0;
    if (!ParseUnsigned16(args[1], &soundId)) {
        PrintUsage();
        return 1;
    }

    std::string outputPath;
    if (args.size() == 3 && !ParseOutputOption(args[2], &outputPath)) {
        std::cerr << "imwrappack: unknown option: " << args[2] << "\n";
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(inputPath, &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    auto& sounds = document.sounds();
    auto it = std::find_if(sounds.begin(), sounds.end(), [soundId](const project::ProjectSound& sound) {
        return sound.id == soundId;
    });
    if (it == sounds.end()) {
        std::cerr << "imwrappack: sound " << soundId << " not found\n";
        return 1;
    }
    sounds.erase(it);

    if (!SaveDocument(inputPath, outputPath, document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Deleted sound " << soundId << "\n";
    return 0;
}

int CommandAddVariant(const std::vector<std::string>& args) {
    if (args.size() < 3 || args.size() > 4) {
        PrintUsage();
        return 1;
    }

    const std::string inputPath = args[0];
    uint16_t soundId = 0;
    VariantKind kind = VariantKind::None;
    if (!ParseUnsigned16(args[1], &soundId) || !ParseVariantName(args[2], &kind)) {
        PrintUsage();
        return 1;
    }

    std::string outputPath;
    if (args.size() == 4 && !ParseOutputOption(args[3], &outputPath)) {
        std::cerr << "imwrappack: unknown option: " << args[3] << "\n";
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(inputPath, &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    project::ProjectSound* sound = FindSound(&document, soundId);
    if (!sound) {
        std::cerr << "imwrappack: sound " << soundId << " not found\n";
        return 1;
    }

    EnsureVariant(sound, kind).includeVariant = true;

    if (!SaveDocument(inputPath, outputPath, document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Added variant " << VariantTag(kind) << " to sound " << soundId << "\n";
    return 0;
}

int CommandDeleteVariant(const std::vector<std::string>& args) {
    if (args.size() < 3 || args.size() > 4) {
        PrintUsage();
        return 1;
    }

    const std::string inputPath = args[0];
    uint16_t soundId = 0;
    VariantKind kind = VariantKind::None;
    if (!ParseUnsigned16(args[1], &soundId) || !ParseVariantName(args[2], &kind)) {
        PrintUsage();
        return 1;
    }

    std::string outputPath;
    if (args.size() == 4 && !ParseOutputOption(args[3], &outputPath)) {
        std::cerr << "imwrappack: unknown option: " << args[3] << "\n";
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(inputPath, &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    project::ProjectSound* sound = FindSound(&document, soundId);
    if (!sound) {
        std::cerr << "imwrappack: sound " << soundId << " not found\n";
        return 1;
    }

    if (sound->variants.size() <= 1) {
        std::cerr << "imwrappack: cannot delete the last variant of a sound; use delete-sound instead\n";
        return 1;
    }

    auto& variants = sound->variants;
    auto it = std::find_if(variants.begin(), variants.end(), [kind](const project::ProjectVariant& variant) {
        return variant.kind == kind;
    });
    if (it == variants.end()) {
        std::cerr << "imwrappack: variant " << VariantTag(kind) << " not found on sound " << soundId << "\n";
        return 1;
    }
    variants.erase(it);

    if (!SaveDocument(inputPath, outputPath, document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Deleted variant " << VariantTag(kind) << " from sound " << soundId << "\n";
    return 0;
}

int CommandImportMidi(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        PrintUsage();
        return 1;
    }

    const std::string inputPath = args[0];
    uint16_t soundId = 0;
    VariantKind kind = VariantKind::None;
    if (!ParseUnsigned16(args[1], &soundId) || !ParseVariantName(args[2], &kind)) {
        PrintUsage();
        return 1;
    }

    std::string outputPath;
    std::string name;
    std::vector<std::string> midiPaths;
    for (std::size_t i = 3; i < args.size(); ++i) {
        if (ParseOutputOption(args[i], &outputPath)) {
            continue;
        }
        if (args[i].rfind("--name=", 0) == 0) {
            name = args[i].substr(std::strlen("--name="));
            continue;
        }
        midiPaths.push_back(args[i]);
    }

    if (midiPaths.empty()) {
        std::cerr << "imwrappack: import-midi requires at least one MIDI file\n";
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(inputPath, &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    project::ProjectSound& sound = EnsureSound(&document, soundId);
    if (!name.empty()) {
        sound.name = name;
    }

    project::ProjectVariant& variant = EnsureVariant(&sound, kind);
    for (const std::string& midiPath : midiPaths) {
        if (!project::ImportMidiFileToVariant(midiPath, &variant, &error)) {
            std::cerr << "imwrappack: " << error << "\n";
            return 1;
        }
    }

    if (!SaveDocument(inputPath, outputPath, document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Imported " << midiPaths.size() << " MIDI file(s) into " << soundId << ":" << VariantTag(kind) << "\n";
    return 0;
}

int CommandSetMdhd(const std::vector<std::string>& args) {
    if (args.size() < 9 || args.size() > 10) {
        PrintUsage();
        return 1;
    }

    const std::string inputPath = args[0];
    uint16_t soundId = 0;
    VariantKind kind = VariantKind::None;
    MdhdData mdhd = MdhdData::Defaults();
    if (!ParseUnsigned16(args[1], &soundId) ||
        !ParseVariantName(args[2], &kind) ||
        !ParseUnsigned8(args[3], &mdhd.priority) ||
        !ParseUnsigned8(args[4], &mdhd.volume) ||
        !ParseSigned8(args[5], &mdhd.pan) ||
        !ParseSigned8(args[6], &mdhd.transpose) ||
        !ParseSigned8(args[7], &mdhd.detune) ||
        !ParseUnsigned8(args[8], &mdhd.speed)) {
        PrintUsage();
        return 1;
    }
    mdhd.present = true;

    std::string outputPath;
    if (args.size() == 10 && !ParseOutputOption(args[9], &outputPath)) {
        std::cerr << "imwrappack: unknown option: " << args[9] << "\n";
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(inputPath, &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    project::ProjectSound* sound = FindSound(&document, soundId);
    if (!sound) {
        std::cerr << "imwrappack: sound " << soundId << " not found\n";
        return 1;
    }

    project::ProjectVariant& variant = EnsureVariant(sound, kind);
    variant.includeMdhd = true;
    variant.mdhd = mdhd;

    if (!SaveDocument(inputPath, outputPath, document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Updated MDhd for " << soundId << ":" << VariantTag(kind) << "\n";
    return 0;
}

int CommandClearMdhd(const std::vector<std::string>& args) {
    if (args.size() < 3 || args.size() > 4) {
        PrintUsage();
        return 1;
    }

    const std::string inputPath = args[0];
    uint16_t soundId = 0;
    VariantKind kind = VariantKind::None;
    if (!ParseUnsigned16(args[1], &soundId) || !ParseVariantName(args[2], &kind)) {
        PrintUsage();
        return 1;
    }

    std::string outputPath;
    if (args.size() == 4 && !ParseOutputOption(args[3], &outputPath)) {
        std::cerr << "imwrappack: unknown option: " << args[3] << "\n";
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(inputPath, &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    project::ProjectSound* sound = FindSound(&document, soundId);
    project::ProjectVariant* variant = sound ? FindVariant(sound, kind) : nullptr;
    if (!variant) {
        std::cerr << "imwrappack: variant " << soundId << ":" << VariantTag(kind) << " not found\n";
        return 1;
    }

    variant->includeMdhd = false;
    variant->mdhd = MdhdData::Defaults();

    if (!SaveDocument(inputPath, outputPath, document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Cleared MDhd for " << soundId << ":" << VariantTag(kind) << "\n";
    return 0;
}

project::ProjectVariant* RequireVariant(project::ImsProjectDocument* document, uint16_t soundId, VariantKind kind) {
    project::ProjectSound* sound = FindSound(document, soundId);
    return sound ? FindVariant(sound, kind) : nullptr;
}

int CommandDeleteTrack(const std::vector<std::string>& args) {
    if (args.size() < 4 || args.size() > 5) {
        PrintUsage();
        return 1;
    }

    const std::string inputPath = args[0];
    uint16_t soundId = 0;
    VariantKind kind = VariantKind::None;
    std::size_t trackIndex = 0;
    if (!ParseUnsigned16(args[1], &soundId) || !ParseVariantName(args[2], &kind) || !ParseIndex(args[3], &trackIndex)) {
        PrintUsage();
        return 1;
    }

    std::string outputPath;
    if (args.size() == 5 && !ParseOutputOption(args[4], &outputPath)) {
        std::cerr << "imwrappack: unknown option: " << args[4] << "\n";
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(inputPath, &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    project::ProjectVariant* variant = RequireVariant(&document, soundId, kind);
    if (!variant) {
        std::cerr << "imwrappack: variant " << soundId << ":" << VariantTag(kind) << " not found\n";
        return 1;
    }
    if (trackIndex >= variant->tracks.size()) {
        std::cerr << "imwrappack: track index " << trackIndex << " is out of range\n";
        return 1;
    }

    variant->tracks.erase(variant->tracks.begin() + static_cast<std::ptrdiff_t>(trackIndex));

    if (!SaveDocument(inputPath, outputPath, document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Deleted track " << trackIndex << " from " << soundId << ":" << VariantTag(kind) << "\n";
    return 0;
}

int CommandMoveTrack(const std::vector<std::string>& args) {
    if (args.size() < 5 || args.size() > 6) {
        PrintUsage();
        return 1;
    }

    const std::string inputPath = args[0];
    uint16_t soundId = 0;
    VariantKind kind = VariantKind::None;
    std::size_t trackIndex = 0;
    if (!ParseUnsigned16(args[1], &soundId) || !ParseVariantName(args[2], &kind) || !ParseIndex(args[3], &trackIndex)) {
        PrintUsage();
        return 1;
    }

    const std::string direction = args[4];
    std::string outputPath;
    if (args.size() == 6 && !ParseOutputOption(args[5], &outputPath)) {
        std::cerr << "imwrappack: unknown option: " << args[5] << "\n";
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(inputPath, &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    project::ProjectVariant* variant = RequireVariant(&document, soundId, kind);
    if (!variant) {
        std::cerr << "imwrappack: variant " << soundId << ":" << VariantTag(kind) << " not found\n";
        return 1;
    }
    if (trackIndex >= variant->tracks.size()) {
        std::cerr << "imwrappack: track index " << trackIndex << " is out of range\n";
        return 1;
    }

    if (direction == "up") {
        if (trackIndex == 0) {
            std::cerr << "imwrappack: track is already at the top\n";
            return 1;
        }
        std::swap(variant->tracks[trackIndex], variant->tracks[trackIndex - 1]);
    } else if (direction == "down") {
        if (trackIndex + 1 >= variant->tracks.size()) {
            std::cerr << "imwrappack: track is already at the bottom\n";
            return 1;
        }
        std::swap(variant->tracks[trackIndex], variant->tracks[trackIndex + 1]);
    } else {
        std::cerr << "imwrappack: direction must be 'up' or 'down'\n";
        return 1;
    }

    if (!SaveDocument(inputPath, outputPath, document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::cout << "Moved track " << trackIndex << " " << direction << " in " << soundId << ":" << VariantTag(kind) << "\n";
    return 0;
}

int CommandExportTrack(const std::vector<std::string>& args) {
    if (args.size() != 5) {
        PrintUsage();
        return 1;
    }

    const std::string inputPath = args[0];
    const std::string outputPath = args[4];
    uint16_t soundId = 0;
    VariantKind kind = VariantKind::None;
    std::size_t trackIndex = 0;
    if (!ParseUnsigned16(args[1], &soundId) || !ParseVariantName(args[2], &kind) || !ParseIndex(args[3], &trackIndex)) {
        PrintUsage();
        return 1;
    }

    project::ImsProjectDocument document;
    std::string error;
    if (!LoadDocument(inputPath, &document, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    const project::ProjectSound* sound = FindSound(document, soundId);
    const project::ProjectVariant* variant = sound ? FindVariant(*sound, kind) : nullptr;
    if (!variant) {
        std::cerr << "imwrappack: variant " << soundId << ":" << VariantTag(kind) << " not found\n";
        return 1;
    }
    if (trackIndex >= variant->tracks.size()) {
        std::cerr << "imwrappack: track index " << trackIndex << " is out of range\n";
        return 1;
    }

    SmfSequence sequence;
    sequence.format = 0;
    sequence.division = variant->division;
    sequence.tracks.push_back(imwrap::SmfTrack{variant->tracks[trackIndex].events});

    std::vector<uint8_t> bytes;
    if (!SmfSerializer::Serialize(sequence, &bytes, &error)) {
        std::cerr << "imwrappack: " << error << "\n";
        return 1;
    }

    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "imwrappack: failed to open output file: " << outputPath << "\n";
        return 1;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        std::cerr << "imwrappack: failed to write output file: " << outputPath << "\n";
        return 1;
    }

    std::cout << "Exported track " << trackIndex << " to " << outputPath << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    const std::string command = argv[1];
    const std::vector<std::string> args(argv + 2, argv + argc);

    if (command == "help" || command == "--help" || command == "-h") {
        PrintUsage();
        return 0;
    }
    if (command == "inspect" || command == "list") {
        return CommandInspect(args);
    }
    if (command == "create") {
        return CommandCreate(args);
    }
    if (command == "build") {
        return CommandBuild(args);
    }
    if (command == "add-sound") {
        return CommandAddSound(args);
    }
    if (command == "set-sound") {
        return CommandSetSound(args);
    }
    if (command == "delete-sound") {
        return CommandDeleteSound(args);
    }
    if (command == "add-variant") {
        return CommandAddVariant(args);
    }
    if (command == "delete-variant") {
        return CommandDeleteVariant(args);
    }
    if (command == "import-midi") {
        return CommandImportMidi(args);
    }
    if (command == "set-mdhd") {
        return CommandSetMdhd(args);
    }
    if (command == "clear-mdhd") {
        return CommandClearMdhd(args);
    }
    if (command == "delete-track") {
        return CommandDeleteTrack(args);
    }
    if (command == "move-track") {
        return CommandMoveTrack(args);
    }
    if (command == "export-track") {
        return CommandExportTrack(args);
    }

    PrintUsage();
    return 1;
}
