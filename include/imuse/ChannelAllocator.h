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

#ifndef IMUSE_CHANNEL_ALLOCATOR_H
#define IMUSE_CHANNEL_ALLOCATOR_H

#include <array>
#include <cstdint>

#include "imuse/Export.h"
#include "imuse/ImsFormat.h"

namespace imuse {

struct ChannelAssignment {
    bool valid = false;
    uint8_t logicalChannel = 0;
    uint8_t deviceChannel = 0;
    bool percussion = false;
};

class IMUSE_API ChannelAllocator {
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
