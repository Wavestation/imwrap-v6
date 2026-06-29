#include "OplSbi.h"

#include <algorithm>

namespace imwrap::gui {
namespace {

constexpr std::size_t kSbiSignatureSize = 4;
constexpr std::size_t kSbiTitleSize = 32;
constexpr std::size_t kSbiHeaderSize = kSbiSignatureSize + kSbiTitleSize;
constexpr std::size_t kSbiCoreRegistersSize = 11;
constexpr uint8_t kSbiSignatureByte3 = 0x1A;
constexpr uint8_t kSbiLegacySignatureByte3 = 0x1D;

void SetError(const char* message, std::string* error) {
    if (error) {
        *error = message ? message : "";
    }
}

} // namespace

bool DecodeSbi(const std::vector<uint8_t>& fileBytes, SbiTimbre* out, std::string* error) {
    if (!out) {
        SetError("output timbre pointer is null", error);
        return false;
    }
    if (fileBytes.size() < kSbiHeaderSize + kSbiCoreRegistersSize) {
        SetError("SBI file is too short", error);
        return false;
    }
    if (fileBytes[0] != 'S' || fileBytes[1] != 'B' || fileBytes[2] != 'I' ||
        (fileBytes[3] != kSbiSignatureByte3 && fileBytes[3] != kSbiLegacySignatureByte3)) {
        SetError("SBI signature is invalid", error);
        return false;
    }

    SbiTimbre timbre;
    const uint8_t* titleBytes = fileBytes.data() + kSbiSignatureSize;
    std::size_t titleLength = 0;
    while (titleLength < kSbiTitleSize && titleBytes[titleLength] != 0) {
        ++titleLength;
    }
    timbre.title.assign(reinterpret_cast<const char*>(titleBytes), titleLength);

    const uint8_t* inst = fileBytes.data() + kSbiHeaderSize;

    // Follow ScummVM's 30-byte iMUSE layout:
    // mod op (0..4), carrier op (5..9), feedback (10), then iMUSE-specific extra bytes.
    timbre.imuseInstrument.fill(0);
    timbre.imuseInstrument[0] = inst[0];
    timbre.imuseInstrument[5] = inst[1];
    timbre.imuseInstrument[1] = inst[2];
    timbre.imuseInstrument[6] = inst[3];
    timbre.imuseInstrument[2] = inst[4];
    timbre.imuseInstrument[7] = inst[5];
    timbre.imuseInstrument[3] = inst[6];
    timbre.imuseInstrument[8] = inst[7];
    timbre.imuseInstrument[4] = inst[8];
    timbre.imuseInstrument[9] = inst[9];
    timbre.imuseInstrument[10] = inst[10];

    *out = std::move(timbre);
    SetError("", error);
    return true;
}

bool EncodeSbi(const std::vector<uint8_t>& imuseBytes, const std::string& title, std::vector<uint8_t>* out, std::string* error) {
    if (!out) {
        SetError("output byte vector is null", error);
        return false;
    }
    if (imuseBytes.size() < kSbiCoreRegistersSize) {
        SetError("at least 11 AdLib bytes are required to export SBI", error);
        return false;
    }

    out->assign(kSbiHeaderSize + kSbiInstrumentSize, 0);
    (*out)[0] = 'S';
    (*out)[1] = 'B';
    (*out)[2] = 'I';
    (*out)[3] = kSbiSignatureByte3;

    const std::size_t titleLength = (std::min)(title.size(), kSbiTitleSize - 1);
    std::copy_n(title.data(), titleLength, reinterpret_cast<char*>(out->data() + kSbiSignatureSize));

    uint8_t* inst = out->data() + kSbiHeaderSize;
    inst[0] = imuseBytes[0];
    inst[1] = imuseBytes[5];
    inst[2] = imuseBytes[1];
    inst[3] = imuseBytes[6];
    inst[4] = imuseBytes[2];
    inst[5] = imuseBytes[7];
    inst[6] = imuseBytes[3];
    inst[7] = imuseBytes[8];
    inst[8] = imuseBytes[4];
    inst[9] = imuseBytes[9];
    inst[10] = imuseBytes[10];

    SetError("", error);
    return true;
}

bool ImuseAdlibHasExtendedData(const std::vector<uint8_t>& imuseBytes) {
    if (imuseBytes.size() <= kSbiCoreRegistersSize) {
        return false;
    }
    return std::any_of(imuseBytes.begin() + static_cast<std::ptrdiff_t>(kSbiCoreRegistersSize), imuseBytes.end(),
                       [](uint8_t value) { return value != 0; });
}

} // namespace imwrap::gui
