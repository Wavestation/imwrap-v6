#ifndef IMUSE_IMS_FORMAT_H
#define IMUSE_IMS_FORMAT_H

#include <array>
#include <cstdint>
#include <string>

namespace imuse {

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
    Rol  = 1u << 1
};

enum class TargetProfile {
    GeneralMidi,
    Mt32
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

} // namespace imuse

#endif
