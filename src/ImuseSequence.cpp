#include "imuse/ImuseSequence.h"
#include <algorithm>

namespace imuse {

bool LoadImuseSequence(const SoundVariantView &variant, ImuseSequence *out, std::string *error) {
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

    ImuseSequence sequence;
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

            ImuseControlEvent control;
            if (!DecodeImuseSysex(ByteView(event.payload.data(), event.payload.size()), &control, nullptr)) {
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

} // namespace imuse
