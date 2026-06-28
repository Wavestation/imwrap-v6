#include "ImsProject.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>

namespace imwrap::gui {
namespace {

std::string FileNameFromPath(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

std::string BaseNameFromPath(const std::string& path) {
    std::string fileName = FileNameFromPath(path);
    const std::size_t dot = fileName.find_last_of('.');
    if (dot == std::string::npos) {
        return fileName;
    }
    return fileName.substr(0, dot);
}

std::vector<uint8_t> ReadBinaryFile(const std::string& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) {
            *error = "failed to open MIDI file: " + path;
        }
        return {};
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

const char* VariantShortName(VariantKind kind) {
    switch (kind) {
    case VariantKind::Gmd:
        return "GMD";
    case VariantKind::Rol:
        return "ROL";
    case VariantKind::Adl:
        return "ADL";
    case VariantKind::None:
    default:
        return "NONE";
    }
}

const char* VariantDisplayName(VariantKind kind) {
    switch (kind) {
    case VariantKind::Gmd:
        return "General MIDI";
    case VariantKind::Rol:
        return "Roland MT-32";
    case VariantKind::Adl:
        return "AdLib";
    case VariantKind::None:
    default:
        return "Unknown";
    }
}

std::string FormatSoundLabel(const ProjectSound& sound) {
    std::ostringstream oss;
    oss << sound.id << ": " << sound.name;

    std::vector<std::string> presentVariants;
    for (const ProjectVariant& variant : sound.variants) {
        if (variant.includeVariant) {
            presentVariants.emplace_back(VariantShortName(variant.kind));
        }
    }

    if (!presentVariants.empty()) {
        oss << " [";
        for (std::size_t i = 0; i < presentVariants.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << presentVariants[i];
        }
        oss << "]";
    }

    return oss.str();
}

SmfTrack MergeTracksToFormat0(const SmfSequence& seq) {
    struct AbsoluteEvent {
        uint32_t tick = 0;
        uint32_t order = 0;
        MidiEvent event;
    };

    std::vector<AbsoluteEvent> allEvents;
    uint32_t order = 0;
    for (const SmfTrack& track : seq.tracks) {
        uint32_t currentTick = 0;
        for (const MidiEvent& event : track.events) {
            currentTick += event.delta;
            if (event.type == MidiEventType::Meta && event.metaType == 0x2F) {
                continue;
            }
            allEvents.push_back({currentTick, order++, event});
        }
    }

    std::stable_sort(allEvents.begin(), allEvents.end(), [](const AbsoluteEvent& lhs, const AbsoluteEvent& rhs) {
        if (lhs.tick != rhs.tick) {
            return lhs.tick < rhs.tick;
        }
        return lhs.order < rhs.order;
    });

    SmfTrack mergedTrack;
    uint32_t previousTick = 0;
    for (const AbsoluteEvent& absoluteEvent : allEvents) {
        MidiEvent event = absoluteEvent.event;
        event.delta = absoluteEvent.tick - previousTick;
        event.tick = absoluteEvent.tick;
        previousTick = absoluteEvent.tick;
        mergedTrack.events.push_back(event);
    }

    MidiEvent endOfTrack;
    endOfTrack.type = MidiEventType::Meta;
    endOfTrack.status = 0xFF;
    endOfTrack.metaType = 0x2F;
    endOfTrack.tick = previousTick;
    endOfTrack.delta = 0;
    mergedTrack.events.push_back(endOfTrack);

    return mergedTrack;
}

bool ImportMidiFileToVariant(const std::string& path, ProjectVariant* variant, std::string* error) {
    if (!variant) {
        if (error) {
            *error = "target variant is null";
        }
        return false;
    }

    std::vector<uint8_t> data = ReadBinaryFile(path, error);
    if (data.empty()) {
        return false;
    }

    SmfSequence seq;
    if (!SmfParser::Parse(ByteView(data.data(), data.size()), &seq, error)) {
        return false;
    }

    if (variant->tracks.empty()) {
        variant->division = seq.division;
    }

    const std::string baseName = BaseNameFromPath(path);
    const std::string fileName = FileNameFromPath(path);

    if (seq.format == 1) {
        const SmfTrack mergedTrack = MergeTracksToFormat0(seq);
        ProjectTrack track;
        track.name = baseName + " (Merged)";
        track.sourceFileName = fileName;
        track.events = mergedTrack.events;
        variant->tracks.push_back(std::move(track));
        return true;
    }

    int trackIndex = 0;
    for (const SmfTrack& sourceTrack : seq.tracks) {
        ProjectTrack track;
        track.name = (seq.tracks.size() == 1)
            ? baseName
            : (baseName + " (T" + std::to_string(trackIndex) + ")");
        track.sourceFileName = fileName;
        track.events = sourceTrack.events;
        variant->tracks.push_back(std::move(track));
        ++trackIndex;
    }

    return true;
}

void ImsProjectDocument::clear() {
    sounds_.clear();
}

bool ImsProjectDocument::loadFromFile(const std::string& path, std::string* error) {
    ResourceBank bank;
    if (!bank.openFromFile(path, error)) {
        return false;
    }
    return loadFromBank(bank, error);
}

bool ImsProjectDocument::loadFromMemory(std::vector<uint8_t> bytes, std::string* error) {
    ResourceBank bank;
    if (!bank.openFromMemory(std::move(bytes), error)) {
        return false;
    }
    return loadFromBank(bank, error);
}

bool ImsProjectDocument::loadFromBank(const ResourceBank& bank, std::string* error) {
    std::vector<ProjectSound> loadedSounds;

    const std::vector<VariantKind> variantKinds = {
        VariantKind::Gmd,
        VariantKind::Rol,
        VariantKind::Adl
    };

    for (uint16_t soundId : bank.soundIds()) {
        SoundResource resource = bank.loadSound(soundId, error);
        if (!resource.valid()) {
            return false;
        }

        ProjectSound sound;
        sound.id = soundId;
        sound.name = resource.name();

        for (VariantKind kind : variantKinds) {
            if (!resource.hasVariant(kind)) {
                continue;
            }

            const SoundVariantView view = resource.variant(kind);
            ProjectVariant variant;
            variant.kind = kind;
            variant.includeVariant = true;
            variant.includeMdhd = view.mdhd.present;
            variant.mdhd = view.mdhd;

            SmfSequence sequence;
            if (!SmfParser::Parse(view.smfData, &sequence, error)) {
                return false;
            }

            variant.division = sequence.division;
            int trackIndex = 0;
            for (const SmfTrack& sourceTrack : sequence.tracks) {
                ProjectTrack track;
                track.name = "Track " + std::to_string(trackIndex++);
                track.sourceFileName = "Imported";
                track.events = sourceTrack.events;
                variant.tracks.push_back(std::move(track));
            }

            sound.variants.push_back(std::move(variant));
        }

        loadedSounds.push_back(std::move(sound));
    }

    sounds_ = std::move(loadedSounds);
    return true;
}

bool ImsProjectDocument::buildBankBytes(std::vector<uint8_t>* outBytes, std::string* error) const {
    if (!outBytes) {
        if (error) {
            *error = "output buffer is null";
        }
        return false;
    }

    std::vector<SoundBankInput> inputs;
    inputs.reserve(sounds_.size());

    for (const ProjectSound& sound : sounds_) {
        SoundBankInput input;
        input.soundId = sound.id;
        input.name = sound.name;

        for (const ProjectVariant& variant : sound.variants) {
            if (!variant.includeVariant) {
                continue;
            }

            VariantSource source;
            source.kind = variant.kind;
            source.includeMdhd = variant.includeMdhd;
            source.mdhd = variant.mdhd;

            SmfSequence sequence;
            sequence.format = 2;
            sequence.division = variant.division;
            sequence.trackCount = static_cast<uint16_t>(variant.tracks.size());
            sequence.tracks.reserve(variant.tracks.size());

            for (const ProjectTrack& track : variant.tracks) {
                SmfTrack smfTrack;
                smfTrack.events = track.events;
                sequence.tracks.push_back(std::move(smfTrack));
            }

            if (!SmfSerializer::Serialize(sequence, &source.smfData, error)) {
                return false;
            }

            input.variants.push_back(std::move(source));
        }

        inputs.push_back(std::move(input));
    }

    ImsWriter writer;
    return writer.build(inputs, outBytes, error);
}

bool ImsProjectDocument::saveToFile(const std::string& path, std::string* error) const {
    std::vector<SoundBankInput> inputs;
    inputs.reserve(sounds_.size());

    for (const ProjectSound& sound : sounds_) {
        SoundBankInput input;
        input.soundId = sound.id;
        input.name = sound.name;

        for (const ProjectVariant& variant : sound.variants) {
            if (!variant.includeVariant) {
                continue;
            }

            VariantSource source;
            source.kind = variant.kind;
            source.includeMdhd = variant.includeMdhd;
            source.mdhd = variant.mdhd;

            SmfSequence sequence;
            sequence.format = 2;
            sequence.division = variant.division;
            sequence.trackCount = static_cast<uint16_t>(variant.tracks.size());
            sequence.tracks.reserve(variant.tracks.size());

            for (const ProjectTrack& track : variant.tracks) {
                SmfTrack smfTrack;
                smfTrack.events = track.events;
                sequence.tracks.push_back(std::move(smfTrack));
            }

            if (!SmfSerializer::Serialize(sequence, &source.smfData, error)) {
                return false;
            }

            input.variants.push_back(std::move(source));
        }

        inputs.push_back(std::move(input));
    }

    ImsWriter writer;
    return writer.writeFile(path, inputs, error);
}

ProjectSound* ImsProjectDocument::findSound(uint16_t soundId) {
    for (ProjectSound& sound : sounds_) {
        if (sound.id == soundId) {
            return &sound;
        }
    }
    return nullptr;
}

const ProjectSound* ImsProjectDocument::findSound(uint16_t soundId) const {
    for (const ProjectSound& sound : sounds_) {
        if (sound.id == soundId) {
            return &sound;
        }
    }
    return nullptr;
}

ProjectVariant* ImsProjectDocument::findVariant(ProjectSound* sound, VariantKind kind) {
    if (!sound) {
        return nullptr;
    }
    for (ProjectVariant& variant : sound->variants) {
        if (variant.kind == kind) {
            return &variant;
        }
    }
    return nullptr;
}

const ProjectVariant* ImsProjectDocument::findVariant(const ProjectSound* sound, VariantKind kind) const {
    if (!sound) {
        return nullptr;
    }
    for (const ProjectVariant& variant : sound->variants) {
        if (variant.kind == kind) {
            return &variant;
        }
    }
    return nullptr;
}

ProjectVariant* ImsProjectDocument::ensureVariant(ProjectSound* sound, VariantKind kind) {
    if (!sound) {
        return nullptr;
    }

    if (ProjectVariant* variant = findVariant(sound, kind)) {
        return variant;
    }

    ProjectVariant variant;
    variant.kind = kind;
    variant.includeVariant = true;
    sound->variants.push_back(std::move(variant));
    return &sound->variants.back();
}

} // namespace imwrap::gui
