#ifndef IMUSE_CHANNEL_ALLOCATOR_H
#define IMUSE_CHANNEL_ALLOCATOR_H

#include <array>
#include <cstdint>

#include "imuse/ImsFormat.h"

namespace imuse {

struct ChannelAssignment {
    bool valid = false;
    uint8_t logicalChannel = 0;
    uint8_t deviceChannel = 0;
    bool percussion = false;
};

class ChannelAllocator {
public:
    void setProfile(TargetProfile profile);
    TargetProfile profile() const { return _profile; }

    void reset();
    ChannelAssignment allocate(uint8_t logicalChannel, bool percussion);
    void release(uint8_t logicalChannel);
    ChannelAssignment assignment(uint8_t logicalChannel) const;

    bool reversePan() const;
    bool supportsRolandSysEx() const;
    bool supportsAdlibInstruments() const;

private:
    uint8_t mapLogicalToPhysical(uint8_t logicalChannel, bool percussion) const;

    TargetProfile _profile = TargetProfile::GeneralMidi;
    std::array<ChannelAssignment, 16> _assignments = {{}};
};

} // namespace imuse

#endif
