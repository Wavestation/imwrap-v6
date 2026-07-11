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

#include <iostream>
#include <string>
#include <vector>

#include "imwrap/IMWrapSequence.h"
#include "imwrap/ResourceBank.h"

#include <iomanip>

namespace {

const char *VariantName(imwrap::VariantKind kind) {
    switch (kind) {
    case imwrap::VariantKind::Gmd:
        return "GMD";
    case imwrap::VariantKind::Rol:
        return "ROL";
    case imwrap::VariantKind::Adl:
        return "ADL";
    default:
        return "NONE";
    }
}

void PrintMdhd(const imwrap::MdhdData &mdhd) {
    if (!mdhd.present) {
        std::cout << "    mdhd: <defaults>\n";
        return;
    }

    std::cout
        << "    mdhd:"
        << " priority=" << static_cast<int>(mdhd.priority)
        << " volume=" << static_cast<int>(mdhd.volume)
        << " pan=" << static_cast<int>(mdhd.pan)
        << " transpose=" << static_cast<int>(mdhd.transpose)
        << " detune=" << static_cast<int>(mdhd.detune)
        << " speed=" << static_cast<int>(mdhd.speed)
        << "\n";
}

void PrintVariant(const imwrap::SoundResource &sound, imwrap::VariantKind kind) {
    if (!sound.hasVariant(kind)) {
        return;
    }

    imwrap::TargetProfile profile = imwrap::TargetProfile::GeneralMidi;
    if (kind == imwrap::VariantKind::Rol) {
        profile = imwrap::TargetProfile::Mt32;
    } else if (kind == imwrap::VariantKind::Adl) {
        profile = imwrap::TargetProfile::Adlib;
    }

    imwrap::SoundVariantView variant = sound.selectVariant(profile);
    if (!variant.valid() || variant.kind != kind) {
        return;
    }

    std::cout
        << "  variant " << VariantName(kind)
        << ": chunk_bytes=" << variant.chunkData.size()
        << " smf_bytes=" << variant.smfData.size()
        << "\n";
    PrintMdhd(variant.mdhd);

    imwrap::IMWrapSequence sequence;
    std::string error;
    if (!imwrap::LoadIMWrapSequence(variant, &sequence, &error)) {
        std::cout << "    parse: failed (" << error << ")\n";
        return;
    }

    std::cout
        << "    smf: format=" << sequence.smf.format
        << " tracks=" << sequence.smf.trackCount
        << " ppqn=" << sequence.smf.ppqn()
        << " imwrap_events=" << sequence.controlEvents.size()
        << "\n";

    std::cout << "    Raw SysEx events in SMF:\n";
    for (std::size_t trackIdx = 0; trackIdx < sequence.smf.tracks.size(); ++trackIdx) {
        const auto &track = sequence.smf.tracks[trackIdx];
        for (const auto &event : track.events) {
            if (event.type == imwrap::MidiEventType::SysEx) {
                std::cout << "      [track " << trackIdx << " tick " << event.tick << "] Raw: ";
                for (uint8_t byte : event.payload) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
                }
                std::cout << std::dec << "\n";

                std::string err;
                imwrap::IMWrapControlEvent control;
                if (!imwrap::DecodeIMWrapSysex(imwrap::ByteView(event.payload.data(), event.payload.size()), &control, &err)) {
                    std::cout << "        -> Decode failed: " << (err.empty() ? "not a valid iMUSE SysEx header/manufacturer" : err) << "\n";
                } else {
                    std::cout << "        -> Decoded: " << imwrap::DescribeIMWrapSysex(control) << "\n";
                }
            }
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: imsprobe path/to/file.ims\n";
        return 1;
    }

    const std::string path = argv[1];
    imwrap::ResourceBank bank;
    std::string error;
    if (!bank.openFromFile(path, &error)) {
        std::cerr << "imsprobe: " << error << "\n";
        return 1;
    }

    const imwrap::ImsHeader &header = bank.header();
    std::cout
        << "IMS version " << header.versionMajor << "." << header.versionMinor
        << ", sounds=" << header.soundCount
        << ", flags=" << header.flags
        << "\n";

    const std::vector<uint16_t> ids = bank.soundIds();
    for (uint16_t id : ids) {
        imwrap::SoundResource sound = bank.loadSound(id, &error);
        if (!sound.valid()) {
            std::cerr << "  sound " << id << ": " << error << "\n";
            return 1;
        }

        std::cout << "sound " << sound.soundId();
        if (!sound.name().empty()) {
            std::cout << " name=\"" << sound.name() << "\"";
        }
        std::cout << "\n";

        PrintVariant(sound, imwrap::VariantKind::Gmd);
        PrintVariant(sound, imwrap::VariantKind::Rol);
        PrintVariant(sound, imwrap::VariantKind::Adl);
    }

    return 0;
}
