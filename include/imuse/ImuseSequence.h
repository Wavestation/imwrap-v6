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

#ifndef IMUSE_IMUSE_SEQUENCE_H
#define IMUSE_IMUSE_SEQUENCE_H

#include <string>
#include <vector>

#include "imuse/Export.h"
#include "imuse/ImuseSysex.h"
#include "imuse/ResourceBank.h"
#include "imuse/SmfSequence.h"

namespace imuse {

struct ImuseSequence {
    SoundVariantView variant;
    SmfSequence smf;
    std::vector<ImuseControlEvent> controlEvents;
};

IMUSE_API bool LoadImuseSequence(const SoundVariantView &variant, ImuseSequence *out, std::string *error = nullptr);

} // namespace imuse

#endif
