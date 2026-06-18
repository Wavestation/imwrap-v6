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

#include "CImuseShim.h"

#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

#if defined(IMUSE_SHIM_WITH_FLUIDSYNTH)
#include <fluidsynth.h>
#endif
#if defined(IMUSE_SHIM_WITH_MT32EMU)
#include <mt32emu.h>
#include <c_interface/c_interface.h>
#endif
#if defined(IMUSE_SHIM_WITH_ADLIB)
#include <adlmidi.h>
#endif

#include "imuse/ImuseEngine.h"
#include "imuse/Instrument.h"
#include "imuse/ImuseSequence.h"
#include "imuse/ImuseSysex.h"
#include "imuse/ResourceBank.h"

struct ImuseBankHandle {
    imuse::ResourceBank bank;
    std::vector<uint16_t> soundIds;
};

struct ShimMidiSink final : imuse::MidiSink {
#if defined(IMUSE_SHIM_WITH_FLUIDSYNTH)
    fluid_synth_t *fluidSynth = nullptr;
#endif
#if defined(IMUSE_SHIM_WITH_MT32EMU)
    mt32emu_context mt32Context = nullptr;
#endif
#if defined(IMUSE_SHIM_WITH_ADLIB)
    ADL_MIDIPlayer *adlPlayer = nullptr;
#endif

    imuse_midi_message_callback_t midiCallback = nullptr;
    imuse_sysex_callback_t sysexCallback = nullptr;
    void *userData = nullptr;

    void onMidiMessage(uint16_t soundId, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) override {
        uint8_t finalData2 = hasData2 ? data2 : 0;

#if defined(IMUSE_SHIM_WITH_FLUIDSYNTH)
        if (fluidSynth) {
            const int channel = status & 0x0F;
            switch (status & 0xF0) {
            case 0x80:
                fluid_synth_noteoff(fluidSynth, channel, data1);
                break;
            case 0x90:
                if (!hasData2 || data2 == 0) {
                    fluid_synth_noteoff(fluidSynth, channel, data1);
                } else {
                    fluid_synth_noteon(fluidSynth, channel, data1, data2);
                }
                break;
            case 0xA0:
                if (hasData2) {
                    fluid_synth_key_pressure(fluidSynth, channel, data1, data2);
                }
                break;
            case 0xB0:
                if (hasData2) {
                    fluid_synth_cc(fluidSynth, channel, data1, data2);
                }
                break;
            case 0xC0:
                fluid_synth_program_change(fluidSynth, channel, data1);
                break;
            case 0xD0:
                fluid_synth_channel_pressure(fluidSynth, channel, data1);
                break;
            case 0xE0:
                if (hasData2) {
                    const int bend = (static_cast<int>(data2) << 7) | data1;
                    fluid_synth_pitch_bend(fluidSynth, channel, bend);
                }
                break;
            }
        }
#endif

#if defined(IMUSE_SHIM_WITH_MT32EMU)
        if (mt32Context) {
            uint32_t msg = status | (data1 << 8) | (finalData2 << 16);
            mt32emu_play_msg(mt32Context, msg);
        }
#endif

#if defined(IMUSE_SHIM_WITH_ADLIB)
        if (adlPlayer) {
            const int ch = status & 0x0F;
            switch (status & 0xF0) {
            case 0x80:
                adl_rt_noteOff(adlPlayer, ch, data1);
                break;
            case 0x90:
                if (!hasData2 || data2 == 0) {
                    adl_rt_noteOff(adlPlayer, ch, data1);
                } else {
                    adl_rt_noteOn(adlPlayer, ch, data1, data2);
                }
                break;
            case 0xA0:
                if (hasData2) { adl_rt_noteAfterTouch(adlPlayer, ch, data1, data2); }
                break;
            case 0xB0:
                if (hasData2) { adl_rt_controllerChange(adlPlayer, ch, data1, data2); }
                break;
            case 0xC0:
                adl_rt_patchChange(adlPlayer, ch, data1);
                break;
            case 0xD0:
                adl_rt_channelAfterTouch(adlPlayer, ch, data1);
                break;
            case 0xE0:
                if (hasData2) { adl_rt_pitchBendML(adlPlayer, ch, data2, data1); }
                break;
            }
        }
#endif

        if (midiCallback) {
            midiCallback(userData, soundId, status, data1, finalData2);
        }
    }

    void onSysEx(uint16_t soundId, imuse::ByteView message) override {
        // Construct the full SysEx message starting with 0xF0
        std::vector<uint8_t> fullMessage;
        fullMessage.reserve(message.size() + 1);
        fullMessage.push_back(0xF0);
        fullMessage.insert(fullMessage.end(), message.data(), message.data() + message.size());

#if defined(IMUSE_SHIM_WITH_MT32EMU)
        if (mt32Context) {
            mt32emu_play_sysex(mt32Context, fullMessage.data(), fullMessage.size());
        }
#endif

        if (sysexCallback) {
            sysexCallback(userData, soundId, fullMessage.data(), fullMessage.size());
        }
    }

    void onCustomInstrument(uint16_t soundId, uint8_t channel, uint32_t type, imuse::ByteView data) override {
        if (type == 0x524F4C20 || type == 'ROL ') { // 'ROL '
            // Convert to standard Roland MT-32 DT1 SysEx
            if (data.size() >= 7 && data.data()[0] == 0x41) {
                std::vector<uint8_t> sysex;
                sysex.reserve(data.size() + 2);
                sysex.push_back(0xF0);
                sysex.insert(sysex.end(), data.data(), data.data() + data.size());
                if (sysex.back() != 0xF7) {
                    sysex.push_back(0xF7);
                }

#if defined(IMUSE_SHIM_WITH_MT32EMU)
                if (mt32Context) {
                    mt32emu_play_sysex(mt32Context, sysex.data(), sysex.size());
                }
#endif

                if (sysexCallback) {
                    sysexCallback(userData, soundId, sysex.data(), sysex.size());
                }
            }
        } else if (type == 0x41444C20 || type == 'ADL ') { // 'ADL ' — OPL2 instrument (30 bytes)
#if defined(IMUSE_SHIM_WITH_ADLIB)
            if (!data.empty() && adlPlayer) {
                // Direct routing to libADLMIDI: map 30-byte iMUSE OPL2 data to ADL_Instrument
                ADL_Instrument ins;
                std::memset(&ins, 0, sizeof(ins));
                ins.version        = ADLMIDI_InstrumentVersion;
                ins.inst_flags     = ADLMIDI_Ins_2op;
                if (data.size() >= 11) {
                    ins.operators[0].avekf_20    = data.data()[0];
                    ins.operators[0].ksl_l_40    = data.data()[1];
                    ins.operators[0].atdec_60    = data.data()[2];
                    ins.operators[0].susrel_80   = data.data()[3];
                    ins.operators[0].waveform_E0 = data.data()[4];
                    ins.operators[1].avekf_20    = data.data()[5];
                    ins.operators[1].ksl_l_40    = data.data()[6];
                    ins.operators[1].atdec_60    = data.data()[7];
                    ins.operators[1].susrel_80   = data.data()[8];
                    ins.operators[1].waveform_E0 = data.data()[9];
                    ins.fb_conn1_C0              = data.data()[10];
                }
                ADL_BankId bankId;
                bankId.msb = 0x7D;
                bankId.lsb = channel;
                bankId.percussive = 0;
                ADL_Bank bank;
                if (adl_getBank(adlPlayer, &bankId, ADLMIDI_Bank_Create, &bank) == 0) {
                    adl_setInstrument(adlPlayer, &bank, 0, &ins);
                }
                adl_rt_bankChangeMSB(adlPlayer, static_cast<ADL_UInt8>(channel), 0x7D);
                adl_rt_bankChangeLSB(adlPlayer, static_cast<ADL_UInt8>(channel), static_cast<ADL_UInt8>(channel));
                adl_rt_patchChange(adlPlayer, static_cast<int>(channel), 0);
                return;
            }
#endif
            if (!data.empty() && sysexCallback) {
                // No adlPlayer: forward as framed envelope for external handling
                std::vector<uint8_t> envelope;
                envelope.reserve(data.size() + 5);
                envelope.push_back(0xF0);
                envelope.push_back(0x7D);
                envelope.push_back(0x10);
                envelope.push_back(channel);
                envelope.insert(envelope.end(), data.data(), data.data() + data.size());
                envelope.push_back(0xF7);
                sysexCallback(userData, soundId, envelope.data(), envelope.size());
            }
        }
    }

    void onAllNotesOff() override {
#if defined(IMUSE_SHIM_WITH_FLUIDSYNTH)
        if (fluidSynth) {
            for (int channel = 0; channel < 16; ++channel) {
                fluid_synth_cc(fluidSynth, channel, 64, 0);
                fluid_synth_cc(fluidSynth, channel, 123, 0);
                fluid_synth_cc(fluidSynth, channel, 120, 0);
            }
        }
#endif

#if defined(IMUSE_SHIM_WITH_MT32EMU)
        if (mt32Context) {
            for (uint8_t chan = 0; chan < 16; ++chan) {
                uint32_t msg1 = (0xB0 | chan) | (123 << 8) | (0 << 16);
                mt32emu_play_msg(mt32Context, msg1);
                uint32_t msg2 = (0xB0 | chan) | (120 << 8) | (0 << 16);
                mt32emu_play_msg(mt32Context, msg2);
            }
        }
#endif

#if defined(IMUSE_SHIM_WITH_ADLIB)
        if (adlPlayer) {
            adl_rt_resetState(adlPlayer);
        }
#endif
    }
};

struct ImuseEngineHandle {
    const ImuseBankHandle *bankHandle = nullptr;
    imuse::ImuseEngine engine;
    ShimMidiSink midiSink;
#if defined(IMUSE_SHIM_WITH_FLUIDSYNTH)
    fluid_settings_t *fluidSettings = nullptr;
    fluid_synth_t *fluidSynth = nullptr;
#endif
#if defined(IMUSE_SHIM_WITH_MT32EMU)
    mt32emu_context mt32Context = nullptr;
#endif
#if defined(IMUSE_SHIM_WITH_ADLIB)
    ADL_MIDIPlayer *adlPlayer = nullptr;
#endif
};

namespace {

imuse::TargetProfile ToProfile(ImuseAuthoringProfile profile) {
    if (profile == ImuseAuthoringProfileMt32) {
        return imuse::TargetProfile::Mt32;
    }
    if (profile == ImuseAuthoringProfileAdlib) {
        return imuse::TargetProfile::Adlib;
    }
    return imuse::TargetProfile::GeneralMidi;
}

void CopyString(const std::string &value, char *buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return;
    }

    const size_t copySize = value.size() < (bufferSize - 1) ? value.size() : (bufferSize - 1);
    std::memcpy(buffer, value.data(), copySize);
    buffer[copySize] = '\0';
}

bool LoadSequenceForSound(const ImuseBankHandle *handle, uint16_t soundId, ImuseAuthoringProfile profile, imuse::ImuseSequence *sequence) {
    if (!handle || !sequence || !handle->bank.hasSound(soundId)) {
        return false;
    }

    imuse::SoundResource sound = handle->bank.loadSound(soundId, nullptr);
    if (!sound.valid()) {
        return false;
    }

    imuse::SoundVariantView variant = sound.selectVariant(ToProfile(profile));
    if (!variant.valid()) {
        return false;
    }

    return imuse::LoadImuseSequence(variant, sequence, nullptr);
}

void DestroyFluidSynth(ImuseEngineHandle *handle) {
#if defined(IMUSE_SHIM_WITH_FLUIDSYNTH)
    if (!handle) {
        return;
    }

    handle->midiSink.fluidSynth = nullptr;

    if (handle->fluidSynth) {
        delete_fluid_synth(handle->fluidSynth);
        handle->fluidSynth = nullptr;
    }
    if (handle->fluidSettings) {
        delete_fluid_settings(handle->fluidSettings);
        handle->fluidSettings = nullptr;
    }
#else
    (void)handle;
#endif
}

bool CreateFluidSynth(ImuseEngineHandle *handle, const char *soundFontPath, std::string *error) {
#if defined(IMUSE_SHIM_WITH_FLUIDSYNTH)
    if (!handle || !soundFontPath || !soundFontPath[0]) {
        if (error) {
            *error = "invalid FluidSynth handle or soundfont path";
        }
        return false;
    }

    DestroyFluidSynth(handle);

    handle->fluidSettings = new_fluid_settings();
    if (!handle->fluidSettings) {
        if (error) {
            *error = "unable to create FluidSynth settings";
        }
        return false;
    }

    fluid_settings_setstr(handle->fluidSettings, "audio.driver", "coreaudio");
    fluid_settings_setnum(handle->fluidSettings, "synth.sample-rate", 44100.0);

    handle->fluidSynth = new_fluid_synth(handle->fluidSettings);
    if (!handle->fluidSynth) {
        if (error) {
            *error = "unable to create FluidSynth synth";
        }
        DestroyFluidSynth(handle);
        return false;
    }

    if (fluid_synth_sfload(handle->fluidSynth, soundFontPath, 1) == FLUID_FAILED) {
        if (error) {
            *error = "unable to load the selected soundfont";
        }
        DestroyFluidSynth(handle);
        return false;
    }

    handle->midiSink.fluidSynth = handle->fluidSynth;
    handle->midiSink.onAllNotesOff();
    return true;
#else
    (void)handle;
    (void)soundFontPath;
    if (error) {
        *error = "this build of imuse_shim was compiled without FluidSynth support";
    }
    return false;
#endif
}

} // namespace

ImuseBankHandle *imuse_bank_create(void) {
    return new ImuseBankHandle();
}

void imuse_bank_destroy(ImuseBankHandle *handle) {
    delete handle;
}

int imuse_bank_load(ImuseBankHandle *handle, const char *path, char *errorBuffer, size_t errorBufferSize) {
    if (!handle || !path) {
        CopyString("invalid bank handle or path", errorBuffer, errorBufferSize);
        return 0;
    }

    std::string error;
    if (!handle->bank.openFromFile(path, &error)) {
        CopyString(error, errorBuffer, errorBufferSize);
        return 0;
    }

    handle->soundIds = handle->bank.soundIds();
    CopyString("", errorBuffer, errorBufferSize);
    return 1;
}

size_t imuse_bank_sound_count(const ImuseBankHandle *handle) {
    return handle ? handle->soundIds.size() : 0;
}

uint16_t imuse_bank_sound_id_at(const ImuseBankHandle *handle, size_t index) {
    return (handle && index < handle->soundIds.size()) ? handle->soundIds[index] : 0;
}

int imuse_bank_sound_name(const ImuseBankHandle *handle, uint16_t soundId, char *buffer, size_t bufferSize) {
    if (!handle || !handle->bank.hasSound(soundId)) {
        CopyString("", buffer, bufferSize);
        return 0;
    }

    imuse::SoundResource sound = handle->bank.loadSound(soundId, nullptr);
    if (!sound.valid()) {
        CopyString("", buffer, bufferSize);
        return 0;
    }

    CopyString(sound.name(), buffer, bufferSize);
    return 1;
}

size_t imuse_bank_event_count(const ImuseBankHandle *handle, uint16_t soundId, ImuseAuthoringProfile profile) {
    imuse::ImuseSequence sequence;
    return LoadSequenceForSound(handle, soundId, profile, &sequence) ? sequence.controlEvents.size() : 0;
}

int imuse_bank_event_summary(const ImuseBankHandle *handle, uint16_t soundId, ImuseAuthoringProfile profile, size_t index, uint32_t *tick, uint16_t *track, char *buffer, size_t bufferSize) {
    imuse::ImuseSequence sequence;
    if (!LoadSequenceForSound(handle, soundId, profile, &sequence) || index >= sequence.controlEvents.size()) {
        CopyString("", buffer, bufferSize);
        return 0;
    }

    const imuse::ImuseControlEvent &event = sequence.controlEvents[index];
    if (tick) {
        *tick = event.tick;
    }
    if (track) {
        *track = event.trackIndex;
    }
    CopyString(imuse::DescribeImuseSysex(event), buffer, bufferSize);
    return 1;
}

size_t imuse_bank_track_count(const ImuseBankHandle *handle, uint16_t soundId, ImuseAuthoringProfile profile) {
    imuse::ImuseSequence sequence;
    if (!LoadSequenceForSound(handle, soundId, profile, &sequence)) {
        return 0;
    }
    return sequence.smf.tracks.size();
}

int imuse_bank_track_summary(const ImuseBankHandle *handle, uint16_t soundId, ImuseAuthoringProfile profile, size_t trackIndex, char *nameBuffer, size_t nameBufferSize, size_t *eventCount) {
    imuse::ImuseSequence sequence;
    if (!LoadSequenceForSound(handle, soundId, profile, &sequence) || trackIndex >= sequence.smf.tracks.size()) {
        if (nameBuffer && nameBufferSize > 0) {
            nameBuffer[0] = '\0';
        }
        if (eventCount) {
            *eventCount = 0;
        }
        return 0;
    }

    const imuse::SmfTrack &track = sequence.smf.tracks[trackIndex];
    if (eventCount) {
        *eventCount = track.events.size();
    }

    std::string trackName = "";
    for (const auto &event : track.events) {
        if (event.type == imuse::MidiEventType::Meta && event.metaType == 0x03) {
            trackName.assign(reinterpret_cast<const char *>(event.payload.data()), event.payload.size());
            break;
        }
    }

    if (trackName.empty()) {
        char defaultName[64];
        std::snprintf(defaultName, sizeof(defaultName), "Track %d", (int)trackIndex);
        trackName = defaultName;
    }

    CopyString(trackName, nameBuffer, nameBufferSize);
    return 1;
}

ImuseEngineHandle *imuse_engine_create(const ImuseBankHandle *bankHandle) {
    if (!bankHandle) {
        return nullptr;
    }

    ImuseEngineHandle *handle = new ImuseEngineHandle();
    handle->bankHandle = bankHandle;
    handle->engine.setResourceBank(&bankHandle->bank);
    handle->engine.setMidiSink(&handle->midiSink);
    return handle;
}

void imuse_engine_destroy(ImuseEngineHandle *handle) {
    if (!handle) {
        return;
    }

    DestroyFluidSynth(handle);
    imuse_engine_disable_mt32(handle);
    imuse_engine_disable_adlib(handle);
    delete handle;
}

void imuse_engine_set_profile(ImuseEngineHandle *handle, ImuseAuthoringProfile profile) {
    if (!handle) {
        return;
    }
    handle->engine.setTargetProfile(ToProfile(profile));
}

void imuse_engine_set_native_mt32_output(ImuseEngineHandle *handle, int enabled) {
    if (!handle) {
        return;
    }
    handle->engine.setNativeMt32Output(enabled != 0);
}

int imuse_engine_start_sound(ImuseEngineHandle *handle, uint16_t soundId) {
    return (handle && handle->engine.startSound(soundId)) ? 1 : 0;
}

void imuse_engine_stop_sound(ImuseEngineHandle *handle, uint16_t soundId) {
    if (!handle) {
        return;
    }
    handle->engine.stopSound(soundId);
}

void imuse_engine_stop_all(ImuseEngineHandle *handle) {
    if (!handle) {
        return;
    }
    handle->engine.stopAllSounds();
}

void imuse_engine_advance(ImuseEngineHandle *handle, uint32_t deltaTicks) {
    if (!handle) {
        return;
    }
    handle->engine.advanceAll(deltaTicks);
}

double imuse_engine_ticks_per_second(const ImuseEngineHandle *handle) {
    return handle ? handle->engine.transportTicksPerSecond() : 960.0;
}

int imuse_engine_get_sound_status(const ImuseEngineHandle *handle, uint16_t soundId) {
    return handle ? handle->engine.getSoundStatus(soundId) : 0;
}

size_t imuse_engine_active_sound_count(const ImuseEngineHandle *handle) {
    return handle ? handle->engine.activeSoundIds().size() : 0;
}

uint16_t imuse_engine_active_sound_id_at(const ImuseEngineHandle *handle, size_t index) {
    if (!handle) {
        return 0;
    }
    const std::vector<uint16_t> ids = handle->engine.activeSoundIds();
    return index < ids.size() ? ids[index] : 0;
}

int imuse_engine_get_location(const ImuseEngineHandle *handle, uint16_t soundId, uint16_t *track, uint16_t *beat, uint16_t *tick) {
    return (handle && handle->engine.getPlaybackLocation(soundId, track, beat, tick)) ? 1 : 0;
}

int imuse_engine_set_hook(ImuseEngineHandle *handle, uint16_t soundId, uint8_t hookClass, uint8_t value, uint8_t channel) {
    if (!handle) {
        return 0;
    }

    const int16_t command[] = {
        static_cast<int16_t>(0x010C),
        static_cast<int16_t>(soundId),
        static_cast<int16_t>(hookClass),
        static_cast<int16_t>(value),
        static_cast<int16_t>(channel)
    };

    return handle->engine.doCommand(5, command) == 0 ? 1 : 0;
}

int imuse_engine_enable_fluidsynth(ImuseEngineHandle *handle, const char *soundFontPath, char *errorBuffer, size_t errorBufferSize) {
    std::string error;
    if (!CreateFluidSynth(handle, soundFontPath, &error)) {
        CopyString(error, errorBuffer, errorBufferSize);
        return 0;
    }
    CopyString("", errorBuffer, errorBufferSize);
    return 1;
}

void imuse_engine_render_fluidsynth(ImuseEngineHandle *handle, uint32_t frameCount, float *left, float *right) {
    if (!left || !right || frameCount == 0) {
        return;
    }

#if defined(IMUSE_SHIM_WITH_FLUIDSYNTH)
    if (!handle || !handle->fluidSynth) {
        std::memset(left, 0, sizeof(float) * frameCount);
        std::memset(right, 0, sizeof(float) * frameCount);
        return;
    }

    fluid_synth_write_float(handle->fluidSynth,
                            static_cast<int>(frameCount),
                            left,
                            0,
                            1,
                            right,
                            0,
                            1);
#else
    (void)handle;
    std::memset(left, 0, sizeof(float) * frameCount);
    std::memset(right, 0, sizeof(float) * frameCount);
#endif
}

void imuse_engine_disable_fluidsynth(ImuseEngineHandle *handle) {
    DestroyFluidSynth(handle);
}

void imuse_engine_set_callbacks(ImuseEngineHandle *handle,
                                imuse_midi_message_callback_t midiCallback,
                                imuse_sysex_callback_t sysexCallback,
                                void *userData) {
    if (!handle) {
        return;
    }
    handle->midiSink.midiCallback = midiCallback;
    handle->midiSink.sysexCallback = sysexCallback;
    handle->midiSink.userData = userData;
}

int imuse_engine_enable_mt32(ImuseEngineHandle *handle, const char *romDir, char *errorBuffer, size_t errorBufferSize) {
#if defined(IMUSE_SHIM_WITH_MT32EMU)
    if (!handle || !romDir || !romDir[0]) {
        CopyString("invalid MT-32 handle or ROM directory", errorBuffer, errorBufferSize);
        return 0;
    }

    imuse_engine_disable_mt32(handle);

    mt32emu_report_handler_i reportHandler = { nullptr };
    mt32emu_context mt32Ctx = mt32emu_create_context(reportHandler, nullptr);
    if (!mt32Ctx) {
        CopyString("failed to create mt32emu context", errorBuffer, errorBufferSize);
        return 0;
    }

    try {
        std::filesystem::path dirPath(romDir);
        if (std::filesystem::exists(dirPath) && std::filesystem::is_directory(dirPath)) {
            for (const auto &entry : std::filesystem::directory_iterator(dirPath)) {
                if (entry.is_regular_file()) {
                    std::string pathStr = entry.path().string();
                    mt32emu_add_rom_file(mt32Ctx, pathStr.c_str());
                }
            }
        } else {
            CopyString("ROM directory does not exist or is not a directory", errorBuffer, errorBufferSize);
            mt32emu_free_context(mt32Ctx);
            return 0;
        }
    } catch (const std::exception &e) {
        CopyString(e.what(), errorBuffer, errorBufferSize);
        mt32emu_free_context(mt32Ctx);
        return 0;
    }

    mt32emu_set_stereo_output_samplerate(mt32Ctx, 44100.0);

    mt32emu_return_code rc = mt32emu_open_synth(mt32Ctx);
    if (rc != MT32EMU_RC_OK) {
        if (rc == MT32EMU_RC_MISSING_ROMS) {
            CopyString("mt32emu missing ROMs (make sure MT32_CONTROL.ROM and MT32_PCM.ROM are in the directory)", errorBuffer, errorBufferSize);
        } else {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "failed to open mt32emu synth (error code %d)", (int)rc);
            CopyString(buf, errorBuffer, errorBufferSize);
        }
        mt32emu_free_context(mt32Ctx);
        return 0;
    }

    handle->mt32Context = mt32Ctx;
    handle->midiSink.mt32Context = mt32Ctx;
    handle->midiSink.onAllNotesOff();

    CopyString("", errorBuffer, errorBufferSize);
    return 1;
#else
    (void)handle;
    (void)romDir;
    CopyString("this build of imuse_shim was compiled without MT-32 support", errorBuffer, errorBufferSize);
    return 0;
#endif
}

void imuse_engine_render_mt32(ImuseEngineHandle *handle, uint32_t frameCount, float *left, float *right) {
    if (!left || !right || frameCount == 0) {
        return;
    }

#if defined(IMUSE_SHIM_WITH_MT32EMU)
    if (!handle || !handle->mt32Context) {
        std::memset(left, 0, sizeof(float) * frameCount);
        std::memset(right, 0, sizeof(float) * frameCount);
        return;
    }

    std::vector<float> interleavedBuffer(frameCount * 2);
    mt32emu_render_float(handle->mt32Context, interleavedBuffer.data(), frameCount);

    for (uint32_t i = 0; i < frameCount; ++i) {
        left[i] = interleavedBuffer[i * 2];
        right[i] = interleavedBuffer[i * 2 + 1];
    }
#else
    (void)handle;
    std::memset(left, 0, sizeof(float) * frameCount);
    std::memset(right, 0, sizeof(float) * frameCount);
#endif
}

void imuse_engine_disable_mt32(ImuseEngineHandle *handle) {
#if defined(IMUSE_SHIM_WITH_MT32EMU)
    if (!handle) {
        return;
    }

    handle->midiSink.mt32Context = nullptr;

    if (handle->mt32Context) {
        mt32emu_close_synth(handle->mt32Context);
        mt32emu_free_context(handle->mt32Context);
        handle->mt32Context = nullptr;
    }
#else
    (void)handle;
#endif
}

void imuse_register_roland_timbre_mapping(const char *name, uint8_t gmProgram) {
    imuse::RegisterRolandTimbreMapping(name, gmProgram);
}

void imuse_clear_roland_timbre_mappings(void) {
    imuse::ClearRolandTimbreMappings();
}

int imuse_engine_enable_adlib(ImuseEngineHandle *handle, char *errorBuffer, size_t errorBufferSize) {
#if defined(IMUSE_SHIM_WITH_ADLIB)
    if (!handle) {
        CopyString("invalid engine handle", errorBuffer, errorBufferSize);
        return 0;
    }

    imuse_engine_disable_adlib(handle);

    ADL_MIDIPlayer *player = adl_init(44100);
    if (!player) {
        CopyString("adl_init() failed — libADLMIDI could not initialise the OPL emulator", errorBuffer, errorBufferSize);
        return 0;
    }

    // Use the "DMX" volume model (classic LucasArts iMUSE/Doom-era behaviour)
    adl_setVolumeRangeModel(player, ADLMIDI_VolumeModel_DMX);
    // Embedded bank 0 (OPL2-GM) as a fallback for non-custom timbres
    adl_setBank(player, 0);
    adl_setNumChips(player, 1);

    handle->adlPlayer = player;
    handle->midiSink.adlPlayer = player;

    CopyString("", errorBuffer, errorBufferSize);
    return 1;
#else
    (void)handle;
    CopyString("this build of imuse_shim was compiled without AdLib support", errorBuffer, errorBufferSize);
    return 0;
#endif
}

void imuse_engine_render_adlib(ImuseEngineHandle *handle, uint32_t frameCount, float *left, float *right) {
    if (!left || !right || frameCount == 0) {
        return;
    }

#if defined(IMUSE_SHIM_WITH_ADLIB)
    if (!handle || !handle->adlPlayer) {
        std::memset(left, 0, sizeof(float) * frameCount);
        std::memset(right, 0, sizeof(float) * frameCount);
        return;
    }

    // libADLMIDI generates interleaved stereo float32
    ADLMIDI_AudioFormat fmt;
    fmt.type          = ADLMIDI_SampleType_F32;
    fmt.containerSize = sizeof(float);
    fmt.sampleOffset  = sizeof(float) * 2;

    // Allocate a temporary interleaved buffer then deinterleave
    std::vector<float> interleaved(frameCount * 2, 0.0f);
    adl_generateFormat(handle->adlPlayer,
                       static_cast<int>(frameCount),
                       reinterpret_cast<ADL_UInt8 *>(interleaved.data()),
                       reinterpret_cast<ADL_UInt8 *>(interleaved.data() + 1),
                       &fmt);

    for (uint32_t i = 0; i < frameCount; ++i) {
        left[i]  = interleaved[i * 2];
        right[i] = interleaved[i * 2 + 1];
    }
#else
    (void)handle;
    std::memset(left, 0, sizeof(float) * frameCount);
    std::memset(right, 0, sizeof(float) * frameCount);
#endif
}

void imuse_engine_disable_adlib(ImuseEngineHandle *handle) {
#if defined(IMUSE_SHIM_WITH_ADLIB)
    if (!handle) {
        return;
    }

    handle->midiSink.adlPlayer = nullptr;

    if (handle->adlPlayer) {
        adl_close(handle->adlPlayer);
        handle->adlPlayer = nullptr;
    }
#else
    (void)handle;
#endif
}
