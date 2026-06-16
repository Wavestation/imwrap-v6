#include <iostream>
#include <string>
#include <vector>

#include "imuse/ImuseSequence.h"
#include "imuse/ResourceBank.h"

#include <iomanip>

namespace {

const char *VariantName(imuse::VariantKind kind) {
    switch (kind) {
    case imuse::VariantKind::Gmd:
        return "GMD";
    case imuse::VariantKind::Rol:
        return "ROL";
    default:
        return "NONE";
    }
}

void PrintMdhd(const imuse::MdhdData &mdhd) {
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

void PrintVariant(const imuse::SoundResource &sound, imuse::VariantKind kind) {
    if (!sound.hasVariant(kind)) {
        return;
    }

    const imuse::TargetProfile profile =
        (kind == imuse::VariantKind::Rol) ? imuse::TargetProfile::Mt32 : imuse::TargetProfile::GeneralMidi;

    imuse::SoundVariantView variant = sound.selectVariant(profile);
    if (!variant.valid() || variant.kind != kind) {
        return;
    }

    std::cout
        << "  variant " << VariantName(kind)
        << ": chunk_bytes=" << variant.chunkData.size()
        << " smf_bytes=" << variant.smfData.size()
        << "\n";
    PrintMdhd(variant.mdhd);

    imuse::ImuseSequence sequence;
    std::string error;
    if (!imuse::LoadImuseSequence(variant, &sequence, &error)) {
        std::cout << "    parse: failed (" << error << ")\n";
        return;
    }

    std::cout
        << "    smf: format=" << sequence.smf.format
        << " tracks=" << sequence.smf.trackCount
        << " ppqn=" << sequence.smf.ppqn()
        << " imuse_events=" << sequence.controlEvents.size()
        << "\n";

    std::cout << "    Raw SysEx events in SMF:\n";
    for (std::size_t trackIdx = 0; trackIdx < sequence.smf.tracks.size(); ++trackIdx) {
        const auto &track = sequence.smf.tracks[trackIdx];
        for (const auto &event : track.events) {
            if (event.type == imuse::MidiEventType::SysEx) {
                std::cout << "      [track " << trackIdx << " tick " << event.tick << "] Raw: ";
                for (uint8_t byte : event.payload) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
                }
                std::cout << std::dec << "\n";

                std::string err;
                imuse::ImuseControlEvent control;
                if (!imuse::DecodeImuseSysex(imuse::ByteView(event.payload.data(), event.payload.size()), &control, &err)) {
                    std::cout << "        -> Decode failed: " << (err.empty() ? "not a valid iMUSE SysEx header/manufacturer" : err) << "\n";
                } else {
                    std::cout << "        -> Decoded: " << imuse::DescribeImuseSysex(control) << "\n";
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
    imuse::ResourceBank bank;
    std::string error;
    if (!bank.openFromFile(path, &error)) {
        std::cerr << "imsprobe: " << error << "\n";
        return 1;
    }

    const imuse::ImsHeader &header = bank.header();
    std::cout
        << "IMS version " << header.versionMajor << "." << header.versionMinor
        << ", sounds=" << header.soundCount
        << ", flags=" << header.flags
        << "\n";

    const std::vector<uint16_t> ids = bank.soundIds();
    for (uint16_t id : ids) {
        imuse::SoundResource sound = bank.loadSound(id, &error);
        if (!sound.valid()) {
            std::cerr << "  sound " << id << ": " << error << "\n";
            return 1;
        }

        std::cout << "sound " << sound.soundId();
        if (!sound.name().empty()) {
            std::cout << " name=\"" << sound.name() << "\"";
        }
        std::cout << "\n";

        PrintVariant(sound, imuse::VariantKind::Gmd);
        PrintVariant(sound, imuse::VariantKind::Rol);
    }

    return 0;
}
