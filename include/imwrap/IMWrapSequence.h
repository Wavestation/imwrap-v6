/* ==========================================================================
 *
 * iMWrap v6 - A modern iMWrap implementation attempt with Adventure Game Studio Companion Plugin
 *
 * This program is the legal property of Masami Komuro and few other contributors,
 * Please refer to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ========================================================================== */

#ifndef IMWRAP_IMWRAP_SEQUENCE_H
#define IMWRAP_IMWRAP_SEQUENCE_H

#include <string>
#include <vector>

#include "imwrap/Export.h"
#include "imwrap/IMWrapSysex.h"
#include "imwrap/ResourceBank.h"
#include "imwrap/SmfSequence.h"

namespace imwrap {

struct IMWrapSequence {
    SoundVariantView variant;
    SmfSequence smf;
    std::vector<IMWrapControlEvent> controlEvents;
};

IMWRAP_API bool LoadIMWrapSequence(const SoundVariantView &variant, IMWrapSequence *out, std::string *error = nullptr,
                                   IMWrapSysexDialect dialect = IMWrapSysexDialect::GenericV6);

} // namespace imwrap

#endif
