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

#ifndef IMUSE_INSTRUMENT_STATE_H
#define IMUSE_INSTRUMENT_STATE_H

#include <cstdint>
#include <vector>

namespace imuse {

enum class InstrumentKind {
    None,
    Program,
    ImuseSetInstrument,
    RolandSysEx,
    AdlibPart,
    AdlibGlobal
};

struct InstrumentState {
    InstrumentKind kind = InstrumentKind::None;
    uint8_t program = 0;
    uint8_t bank = 0;
    uint16_t imuseInstrument = 0;
    std::vector<uint8_t> rawData;
    bool dirty = false;

    void clear() {
        kind = InstrumentKind::None;
        program = 0;
        bank = 0;
        imuseInstrument = 0;
        rawData.clear();
        dirty = false;
    }
};

} // namespace imuse

#endif
