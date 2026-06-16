#ifndef IMUSE_IMUSE_SEQUENCE_H
#define IMUSE_IMUSE_SEQUENCE_H

#include <string>
#include <vector>

#include "imuse/ImuseSysex.h"
#include "imuse/ResourceBank.h"
#include "imuse/SmfSequence.h"

namespace imuse {

struct ImuseSequence {
    SoundVariantView variant;
    SmfSequence smf;
    std::vector<ImuseControlEvent> controlEvents;
};

bool LoadImuseSequence(const SoundVariantView &variant, ImuseSequence *out, std::string *error = nullptr);

} // namespace imuse

#endif
