#pragma once

#include "SysExToolModel.h"

namespace Steinberg {
class IBStream;
}

namespace imwrap::vst3tool {

bool ReadState(Steinberg::IBStream* stream, SysExToolState* out);
bool WriteState(Steinberg::IBStream* stream, const SysExToolState& state);

} // namespace imwrap::vst3tool
