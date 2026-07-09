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

#include "imwrap/IMWrapSequence.h"
#include <algorithm>

namespace imwrap {

bool LoadIMWrapSequence(const SoundVariantView &variant, IMWrapSequence *out, std::string *error,
                        IMWrapSysexDialect dialect) {
    if (!out) {
        if (error) {
            *error = "output sequence pointer is null";
        }
        return false;
    }

    if (!variant.valid()) {
        if (error) {
            *error = "variant is not playable";
        }
        return false;
    }

    IMWrapSequence sequence;
    sequence.variant = variant;
    if (!SmfParser::Parse(variant.smfData, &sequence.smf, error)) {
        return false;
    }

    for (std::size_t trackIndex = 0; trackIndex < sequence.smf.tracks.size(); ++trackIndex) {
        const SmfTrack &track = sequence.smf.tracks[trackIndex];
        for (const MidiEvent &event : track.events) {
            if (event.type != MidiEventType::SysEx) {
                continue;
            }

            IMWrapControlEvent control;
            if (!DecodeIMWrapSysex(ByteView(event.payload.data(), event.payload.size()), &control, dialect, nullptr)) {
                continue;
            }
            control.tick = event.tick;
            control.trackIndex = static_cast<uint16_t>(trackIndex);
            sequence.controlEvents.push_back(std::move(control));
        }
    }

    *out = std::move(sequence);
    return true;
}

} // namespace imwrap
