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

#include "imuse/ImsWriter.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>

namespace imuse {
namespace {

void AppendU16BE(std::vector<uint8_t> *out, uint16_t value) {
    out->push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out->push_back(static_cast<uint8_t>(value & 0xFF));
}

void AppendU32BE(std::vector<uint8_t> *out, uint32_t value) {
    out->push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out->push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out->push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out->push_back(static_cast<uint8_t>(value & 0xFF));
}

void PatchU32BE(std::vector<uint8_t> *out, std::size_t offset, uint32_t value) {
    (*out)[offset + 0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    (*out)[offset + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    (*out)[offset + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    (*out)[offset + 3] = static_cast<uint8_t>(value & 0xFF);
}

std::vector<uint8_t> ReadBinaryFile(const std::string &path, std::string *error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) {
            *error = "failed to open input file: " + path;
        }
        return {};
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool BeginsWithMThd(const std::vector<uint8_t> &bytes) {
    return bytes.size() >= 4 &&
           bytes[0] == 'M' &&
           bytes[1] == 'T' &&
           bytes[2] == 'h' &&
           bytes[3] == 'd';
}

std::vector<uint8_t> MakeChunk(const char id[4], const std::vector<uint8_t> &body) {
    std::vector<uint8_t> chunk;
    chunk.reserve(8 + body.size());
    chunk.insert(chunk.end(), id, id + 4);
    AppendU32BE(&chunk, static_cast<uint32_t>(body.size() + 8));
    chunk.insert(chunk.end(), body.begin(), body.end());
    return chunk;
}

std::vector<uint8_t> MakeMdhdChunk(const MdhdData &mdhd) {
    std::vector<uint8_t> body;
    body.reserve(8);
    body.push_back(0);
    body.push_back(0);
    body.push_back(mdhd.priority);
    body.push_back(mdhd.volume);
    body.push_back(static_cast<uint8_t>(mdhd.pan));
    body.push_back(static_cast<uint8_t>(mdhd.transpose));
    body.push_back(static_cast<uint8_t>(mdhd.detune));
    body.push_back(mdhd.speed);
    return MakeChunk("MDhd", body);
}

std::vector<uint8_t> MakeVariantChunk(const VariantSource &variant, std::string *error) {
    std::vector<uint8_t> midiBytes;
    if (!variant.smfData.empty()) {
        midiBytes = variant.smfData;
    } else {
        midiBytes = ReadBinaryFile(variant.sourcePath, error);
    }
    
    if (midiBytes.empty()) {
        return {};
    }
    if (!BeginsWithMThd(midiBytes)) {
        if (error) {
            *error = "input is not a MIDI file starting with MThd: " + (variant.sourcePath.empty() ? "memory buffer" : variant.sourcePath);
        }
        return {};
    }

    std::vector<uint8_t> body;
    if (variant.includeMdhd) {
        std::vector<uint8_t> mdhd = MakeMdhdChunk(variant.mdhd);
        body.insert(body.end(), mdhd.begin(), mdhd.end());
    }
    body.insert(body.end(), midiBytes.begin(), midiBytes.end());

    if (variant.kind == VariantKind::Gmd) {
        return MakeChunk("GMD ", body);
    }
    if (variant.kind == VariantKind::Rol) {
        return MakeChunk("ROL ", body);
    }
    if (variant.kind == VariantKind::Adl) {
        return MakeChunk("ADL ", body);
    }

    if (error) {
        *error = "unknown variant kind";
    }
    return {};
}

std::vector<uint8_t> MakeSoundChunk(const SoundBankInput &sound, uint16_t *variantMask, std::string *error) {
    std::vector<uint8_t> body;

    {
        std::vector<uint8_t> sidnBody;
        sidnBody.reserve(4);
        AppendU16BE(&sidnBody, sound.soundId);
        AppendU16BE(&sidnBody, 0);
        std::vector<uint8_t> sidnChunk = MakeChunk("SIDN", sidnBody);
        body.insert(body.end(), sidnChunk.begin(), sidnChunk.end());
    }

    if (!sound.name.empty()) {
        std::vector<uint8_t> nameBody(sound.name.begin(), sound.name.end());
        std::vector<uint8_t> nameChunk = MakeChunk("NAME", nameBody);
        body.insert(body.end(), nameChunk.begin(), nameChunk.end());
    }

    uint16_t mask = 0;
    for (const VariantSource &variant : sound.variants) {
        std::vector<uint8_t> variantChunk = MakeVariantChunk(variant, error);
        if (variantChunk.empty()) {
            return {};
        }
        mask |= static_cast<uint16_t>(variant.kind);
        body.insert(body.end(), variantChunk.begin(), variantChunk.end());
    }

    if (mask == 0) {
        if (error) {
            *error = "sound " + std::to_string(sound.soundId) + " has no variants";
        }
        return {};
    }

    if (variantMask) {
        *variantMask = mask;
    }

    return MakeChunk("SOUN", body);
}

} // namespace

bool ImsWriter::build(const std::vector<SoundBankInput> &sounds, std::vector<uint8_t> *outBytes, std::string *error) const {
    if (!outBytes) {
        if (error) {
            *error = "output buffer is null";
        }
        return false;
    }

    std::vector<SoundBankInput> sortedSounds = sounds;
    std::sort(sortedSounds.begin(), sortedSounds.end(), [](const SoundBankInput &lhs, const SoundBankInput &rhs) {
        return lhs.soundId < rhs.soundId;
    });

    for (std::size_t i = 1; i < sortedSounds.size(); ++i) {
        if (sortedSounds[i - 1].soundId == sortedSounds[i].soundId) {
            if (error) {
                *error = "duplicate sound id in input: " + std::to_string(sortedSounds[i].soundId);
            }
            return false;
        }
    }

    struct SoundArtifact {
        uint16_t soundId = 0;
        uint16_t variantMask = 0;
        std::vector<uint8_t> chunk;
    };

    std::vector<SoundArtifact> artifacts;
    artifacts.reserve(sortedSounds.size());
    for (const SoundBankInput &sound : sortedSounds) {
        SoundArtifact artifact;
        artifact.soundId = sound.soundId;
        artifact.chunk = MakeSoundChunk(sound, &artifact.variantMask, error);
        if (artifact.chunk.empty()) {
            return false;
        }
        artifacts.push_back(std::move(artifact));
    }

    const uint32_t ihdrChunkSize = 8 + 16;
    const uint32_t sdirChunkSize = 8 + static_cast<uint32_t>(artifacts.size() * 12);

    uint32_t soundOffset = 8 + ihdrChunkSize + sdirChunkSize;

    outBytes->clear();
    outBytes->reserve(soundOffset);

    outBytes->insert(outBytes->end(), {'I', 'M', 'S', 'B'});
    AppendU32BE(outBytes, 0);

    {
        std::vector<uint8_t> body;
        body.reserve(16);
        AppendU16BE(&body, 1);
        AppendU16BE(&body, 0);
        AppendU32BE(&body, static_cast<uint32_t>(artifacts.size()));
        AppendU32BE(&body, 0);
        AppendU32BE(&body, 0);
        std::vector<uint8_t> ihdr = MakeChunk("IHDR", body);
        outBytes->insert(outBytes->end(), ihdr.begin(), ihdr.end());
    }

    {
        std::vector<uint8_t> body;
        body.reserve(artifacts.size() * 12);
        uint32_t currentOffset = soundOffset;
        for (const SoundArtifact &artifact : artifacts) {
            AppendU16BE(&body, artifact.soundId);
            AppendU16BE(&body, artifact.variantMask);
            AppendU32BE(&body, currentOffset);
            AppendU32BE(&body, static_cast<uint32_t>(artifact.chunk.size()));
            currentOffset += static_cast<uint32_t>(artifact.chunk.size());
        }
        std::vector<uint8_t> sdir = MakeChunk("SDIR", body);
        outBytes->insert(outBytes->end(), sdir.begin(), sdir.end());
    }

    for (const SoundArtifact &artifact : artifacts) {
        outBytes->insert(outBytes->end(), artifact.chunk.begin(), artifact.chunk.end());
    }

    PatchU32BE(outBytes, 4, static_cast<uint32_t>(outBytes->size()));
    return true;
}

bool ImsWriter::writeFile(const std::string &outputPath, const std::vector<SoundBankInput> &sounds, std::string *error) const {
    std::vector<uint8_t> bytes;
    if (!build(sounds, &bytes, error)) {
        return false;
    }

    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error) {
            *error = "failed to open output file: " + outputPath;
        }
        return false;
    }

    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        if (error) {
            *error = "failed to write output file: " + outputPath;
        }
        return false;
    }

    return true;
}

} // namespace imuse
