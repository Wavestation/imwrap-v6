#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace imwrap::gui {

constexpr std::size_t kImuseAdlibInstrumentSize = 30;
constexpr std::size_t kSbiInstrumentSize = 16;

struct SbiTimbre {
    std::string title;
    std::array<uint8_t, kImuseAdlibInstrumentSize> imuseInstrument = {{}};
};

bool DecodeSbi(const std::vector<uint8_t>& fileBytes, SbiTimbre* out, std::string* error = nullptr);
bool EncodeSbi(const std::vector<uint8_t>& imuseBytes, const std::string& title, std::vector<uint8_t>* out, std::string* error = nullptr);
bool ImuseAdlibHasExtendedData(const std::vector<uint8_t>& imuseBytes);

} // namespace imwrap::gui
