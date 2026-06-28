#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "imwrap/ImsWriter.h"
#include "imwrap/ResourceBank.h"
#include "imwrap/SmfSequence.h"

namespace imwrap::gui {

struct ProjectTrack {
    std::string name;
    std::string sourceFileName;
    std::vector<MidiEvent> events;
};

struct ProjectVariant {
    VariantKind kind = VariantKind::None;
    bool includeVariant = false;
    bool includeMdhd = false;
    MdhdData mdhd = MdhdData::Defaults();
    uint16_t division = 480;
    std::vector<ProjectTrack> tracks;
};

struct ProjectSound {
    uint16_t id = 0;
    std::string name;
    std::vector<ProjectVariant> variants;
};

const char* VariantShortName(VariantKind kind);
const char* VariantDisplayName(VariantKind kind);
std::string FormatSoundLabel(const ProjectSound& sound);

SmfTrack MergeTracksToFormat0(const SmfSequence& seq);
bool ImportMidiFileToVariant(const std::string& path, ProjectVariant* variant, std::string* error = nullptr);

class ImsProjectDocument {
public:
    void clear();

    bool loadFromFile(const std::string& path, std::string* error = nullptr);
    bool loadFromMemory(std::vector<uint8_t> bytes, std::string* error = nullptr);
    bool buildBankBytes(std::vector<uint8_t>* outBytes, std::string* error = nullptr) const;
    bool saveToFile(const std::string& path, std::string* error = nullptr) const;

    bool empty() const { return sounds_.empty(); }

    std::vector<ProjectSound>& sounds() { return sounds_; }
    const std::vector<ProjectSound>& sounds() const { return sounds_; }

    ProjectSound* findSound(uint16_t soundId);
    const ProjectSound* findSound(uint16_t soundId) const;

    ProjectVariant* findVariant(ProjectSound* sound, VariantKind kind);
    const ProjectVariant* findVariant(const ProjectSound* sound, VariantKind kind) const;

    ProjectVariant* ensureVariant(ProjectSound* sound, VariantKind kind);

private:
    bool loadFromBank(const ResourceBank& bank, std::string* error);

    std::vector<ProjectSound> sounds_;
};

} // namespace imwrap::gui
