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

/*
 * imuse-v6 Adventure Game Studio Plugin
 * Copyright (C) 2026 various contributors
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef THIS_IS_THE_PLUGIN
#define THIS_IS_THE_PLUGIN
#endif
#include "agsplugin.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <fluidsynth.h>
#include <adlmidi.h>
#include "imuse/ImuseEngine.h"
#include "imuse/ResourceBank.h"

#include <mutex>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
#elif defined(__APPLE__)
#include <CoreMIDI/CoreMIDI.h>
#endif

namespace {

enum class DriverType : int {
    FluidSynth = 0,
    AdLib = 1,
    HardwareGM = 2,
    HardwareMT32 = 3
};

struct AgsFluidSynthMidiSink final : imuse::MidiSink {
    fluid_synth_t *synth = nullptr;

    void onMidiMessage(uint16_t, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) override {
        if (!synth) {
            return;
        }

        const int channel = status & 0x0F;
        switch (status & 0xF0) {
        case 0x80:
            fluid_synth_noteoff(synth, channel, data1);
            break;
        case 0x90:
            if (!hasData2 || data2 == 0) {
                fluid_synth_noteoff(synth, channel, data1);
            } else {
                fluid_synth_noteon(synth, channel, data1, data2);
            }
            break;
        case 0xA0:
            if (hasData2) {
                fluid_synth_key_pressure(synth, channel, data1, data2);
            }
            break;
        case 0xB0:
            if (hasData2) {
                fluid_synth_cc(synth, channel, data1, data2);
            }
            break;
        case 0xC0:
            fluid_synth_program_change(synth, channel, data1);
            break;
        case 0xD0:
            fluid_synth_channel_pressure(synth, channel, data1);
            break;
        case 0xE0:
            if (hasData2) {
                const int bend = (static_cast<int>(data2) << 7) | data1;
                fluid_synth_pitch_bend(synth, channel, bend);
            }
            break;
        default:
            break;
        }
    }

    void onAllNotesOff() override {
        if (!synth) {
            return;
        }

        for (int channel = 0; channel < 16; ++channel) {
            fluid_synth_cc(synth, channel, 64, 0);
            fluid_synth_cc(synth, channel, 123, 0);
            fluid_synth_cc(synth, channel, 120, 0);
        }
    }
};

struct AgsAdlibMidiSink final : imuse::MidiSink {
    ADL_MIDIPlayer *player = nullptr;

    void onMidiMessage(uint16_t, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) override {
        if (!player) {
            return;
        }

        const int channel = status & 0x0F;
        switch (status & 0xF0) {
        case 0x80:
            adl_rt_noteOff(player, channel, data1);
            break;
        case 0x90:
            if (!hasData2 || data2 == 0) {
                adl_rt_noteOff(player, channel, data1);
            } else {
                adl_rt_noteOn(player, channel, data1, data2);
            }
            break;
        case 0xA0:
            if (hasData2) {
                adl_rt_noteAfterTouch(player, channel, data1, data2);
            }
            break;
        case 0xB0:
            if (hasData2) {
                adl_rt_controllerChange(player, channel, data1, data2);
            }
            break;
        case 0xC0:
            adl_rt_patchChange(player, channel, data1);
            break;
        case 0xD0:
            adl_rt_channelAfterTouch(player, channel, data1);
            break;
        case 0xE0:
            if (hasData2) {
                adl_rt_pitchBendML(player, channel, data2, data1);
            }
            break;
        default:
            break;
        }
    }

    void onSysEx(uint16_t, imuse::ByteView message) override {
        if (!player) {
            return;
        }

        std::vector<uint8_t> sysex(message.size() + 2);
        sysex[0] = 0xF0;
        std::memcpy(sysex.data() + 1, message.data(), message.size());
        sysex[message.size() + 1] = 0xF7;
        adl_rt_systemExclusive(player, sysex.data(), sysex.size());
    }

    void onCustomInstrument(uint16_t /*soundId*/, uint8_t channel, uint32_t type, imuse::ByteView data) override {
        if (!player) {
            return;
        }

        if ((type == 'ADL ') && data.size() >= 11) {
            // Map iMUSE 30-byte OPL2 instrument to libADLMIDI ADL_Instrument.
            //
            // iMUSE ADL byte layout (from ScummVM imuse/drivers/adlib.cpp):
            //  Bytes  0-4 : Modulator operator  (avekf, ksl_tl, ar_dr, sl_rr, waveform)
            //  Bytes  5-9 : Carrier   operator  (avekf, ksl_tl, ar_dr, sl_rr, waveform)
            //  Byte  10   : Feedback + connection register (C0)
            //  Bytes 11+  : Flags / extra (ignored for basic 2-op melodic)
            ADL_Instrument ins;
            std::memset(&ins, 0, sizeof(ins));
            ins.version        = ADLMIDI_InstrumentVersion;
            ins.inst_flags     = ADLMIDI_Ins_2op;  // Standard OPL2 2-operator melodic
            ins.fb_conn1_C0    = data.data()[10];

            // Operator 0 = Modulator
            ins.operators[0].avekf_20   = data.data()[0];
            ins.operators[0].ksl_l_40   = data.data()[1];
            ins.operators[0].atdec_60   = data.data()[2];
            ins.operators[0].susrel_80  = data.data()[3];
            ins.operators[0].waveform_E0 = data.data()[4];

            // Operator 1 = Carrier
            ins.operators[1].avekf_20   = data.data()[5];
            ins.operators[1].ksl_l_40   = data.data()[6];
            ins.operators[1].atdec_60   = data.data()[7];
            ins.operators[1].susrel_80  = data.data()[8];
            ins.operators[1].waveform_E0 = data.data()[9];

            // Use a dedicated custom iMUSE ADL bank (MSB=0x7D, LSB=channel)
            // This avoids clobbering the main GM bank.
            ADL_BankId bankId;
            bankId.msb       = 0x7D;  // iMUSE manufacturer ID as MSB
            bankId.lsb       = channel;
            bankId.percussive = 0;

            ADL_Bank bank;
            if (adl_getBank(player, &bankId, ADLMIDI_Bank_Create, &bank) == 0) {
                // Slot 0 in this per-channel bank holds the custom timbre
                adl_setInstrument(player, &bank, 0, &ins);
            }

            // Switch the channel to the custom bank and select slot 0
            adl_rt_bankChangeMSB(player, static_cast<ADL_UInt8>(channel), 0x7D);
            adl_rt_bankChangeLSB(player, static_cast<ADL_UInt8>(channel), static_cast<ADL_UInt8>(channel));
            adl_rt_patchChange(player, static_cast<int>(channel), 0);
        }
    }

    void onAllNotesOff() override {
        if (player) {
            adl_rt_resetState(player);
        }
    }
};

struct HardwareMidiOutSink final : imuse::MidiSink {
#if defined(_WIN32)
    HMIDIOUT hMidiOut = nullptr;
#elif defined(__APPLE__)
    MIDIClientRef client = 0;
    MIDIPortRef port = 0;
    MIDIEndpointRef destination = 0;
#endif
    bool active = false;

    bool open(int deviceIndex) {
        close();
#if defined(_WIN32)
        if (midiOutOpen(&hMidiOut, static_cast<UINT>(deviceIndex), 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            active = true;
            return true;
        }
#elif defined(__APPLE__)
        if (MIDIClientCreate(CFSTR("iMUSE Plugin Client"), nullptr, nullptr, &client) == noErr) {
            if (MIDIOutputPortCreate(client, CFSTR("iMUSE Output Port"), &port) == noErr) {
                ItemCount count = MIDIGetNumberOfDestinations();
                if (count > 0 && deviceIndex < static_cast<int>(count)) {
                    destination = MIDIGetDestination(static_cast<CFIndex>(deviceIndex));
                    active = true;
                    return true;
                }
            }
        }
        close();
#endif
        return false;
    }

    void close() {
        active = false;
#if defined(_WIN32)
        if (hMidiOut) {
            midiOutReset(hMidiOut);
            midiOutClose(hMidiOut);
            hMidiOut = nullptr;
        }
#elif defined(__APPLE__)
        if (port) {
            MIDIPortDispose(port);
            port = 0;
        }
        if (client) {
            MIDIClientDispose(client);
            client = 0;
        }
        destination = 0;
#endif
    }

    void onMidiMessage(uint16_t, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) override {
        if (!active) {
            return;
        }
#if defined(_WIN32)
        DWORD msg = status | (data1 << 8) | (hasData2 ? (data2 << 16) : 0);
        midiOutShortMsg(hMidiOut, msg);
#elif defined(__APPLE__)
        if (!port || !destination) {
            return;
        }
        Byte packetBuf[512];
        MIDIPacketList *packetList = (MIDIPacketList *)packetBuf;
        MIDIPacket *packet = MIDIPacketListInit(packetList);
        Byte data[3] = { status, data1, hasData2 ? data2 : (Byte)0 };
        packet = MIDIPacketListAdd(packetList, sizeof(packetBuf), packet, 0, hasData2 ? 3 : 2, data);
        if (packet) {
            MIDISend(port, destination, packetList);
        }
#endif
    }

    void onSysEx(uint16_t, imuse::ByteView message) override {
        if (!active) {
            return;
        }
#if defined(_WIN32)
        MIDIHDR hdr;
        std::vector<char> buf(message.size() + 2);
        buf[0] = 0xF0;
        std::memcpy(buf.data() + 1, message.data(), message.size());
        buf[message.size() + 1] = 0xF7;

        hdr.lpData = buf.data();
        hdr.dwBufferLength = static_cast<DWORD>(buf.size());
        hdr.dwFlags = 0;

        midiOutPrepareHeader(hMidiOut, &hdr, sizeof(hdr));
        midiOutLongMsg(hMidiOut, &hdr, sizeof(hdr));
        while (!(hdr.dwFlags & MHDR_DONE)) {
            Sleep(1);
        }
        midiOutUnprepareHeader(hMidiOut, &hdr, sizeof(hdr));
#elif defined(__APPLE__)
        if (!port || !destination) {
            return;
        }
        std::vector<Byte> data(message.size() + 2);
        data[0] = 0xF0;
        std::memcpy(data.data() + 1, message.data(), message.size());
        data[message.size() + 1] = 0xF7;

        Byte packetBuf[2048 + sizeof(MIDIPacketList)];
        MIDIPacketList *packetList = (MIDIPacketList *)packetBuf;
        MIDIPacket *packet = MIDIPacketListInit(packetList);
        packet = MIDIPacketListAdd(packetList, sizeof(packetBuf), packet, 0, data.size(), data.data());
        if (packet) {
            MIDISend(port, destination, packetList);
        }
#endif
    }

    void onAllNotesOff() override {
        if (!active) {
            return;
        }
        for (uint8_t chan = 0; chan < 16; ++chan) {
            onMidiMessage(0, 0xB0 | chan, 120, true, 0); // All Sound Off
            onMidiMessage(0, 0xB0 | chan, 123, true, 0); // All Notes Off
            onMidiMessage(0, 0xB0 | chan, 121, true, 0); // Reset All Controllers
        }
    }
};

int GetMidiOutDeviceCount() {
#if defined(_WIN32)
    return static_cast<int>(midiOutGetNumDevs());
#elif defined(__APPLE__)
    return static_cast<int>(MIDIGetNumberOfDestinations());
#else
    return 0;
#endif
}

std::string GetMidiOutDeviceName(int index) {
#if defined(_WIN32)
    MIDIOUTCAPSA caps;
    if (midiOutGetDevCapsA(static_cast<UINT_PTR>(index), &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
        return caps.szPname;
    }
#elif defined(__APPLE__)
    MIDIEndpointRef dest = MIDIGetDestination(static_cast<ItemCount>(index));
    if (dest) {
        CFStringRef nameRef = nullptr;
        if (MIDIObjectGetStringProperty(dest, kMIDIPropertyName, &nameRef) == noErr && nameRef) {
            char nameBuf[256];
            if (CFStringGetCString(nameRef, nameBuf, sizeof(nameBuf), kCFStringEncodingUTF8)) {
                CFRelease(nameRef);
                return nameBuf;
            }
            CFRelease(nameRef);
        }
    }
#endif
    return "Unknown MIDI Device";
}

// Global variables for the plugin state
std::mutex g_Mutex;
imuse::ResourceBank g_Bank;
imuse::ImuseEngine g_Engine(&g_Bank);

AgsFluidSynthMidiSink g_FluidMidiSink;
AgsAdlibMidiSink g_AdlMidiSink;
HardwareMidiOutSink g_HardwareMidiSink;

fluid_settings_t *g_FluidSettings = nullptr;
fluid_synth_t *g_FluidSynth = nullptr;
ADL_MIDIPlayer *g_AdlPlayer = nullptr;

DriverType g_DriverType = DriverType::FluidSynth;

ma_device g_AudioDevice;
bool g_AudioDeviceInitialized = false;
double g_TickAccumulator = 0.0;

IAGSEngine *g_AgsEngine = nullptr;

const char *g_iMuseScriptHeader =
    "#define IMUSE_PLUGIN_VERSION 101\r\n"
    "#define IMUSE_DRIVER_FLUIDSYNTH    0\r\n"
    "#define IMUSE_DRIVER_ADLIB         1\r\n"
    "#define IMUSE_DRIVER_HARDWARE_GM   2\r\n"
    "#define IMUSE_DRIVER_HARDWARE_MT32 3\r\n"
    "\r\n"
    "import void iMuse_LoadBank(const string filename);\r\n"
    "import void iMuse_LoadSoundFont(const string filename);\r\n"
    "import void iMuse_SetDriver(int driverType, const string deviceOrPath);\r\n"
    "import int  iMuse_GetMIDIDeviceCount();\r\n"
    "import const string iMuse_GetMIDIDeviceName(int index);\r\n"
    "import void iMuse_StartSound(int soundId);\r\n"
    "import void iMuse_StopSound(int soundId);\r\n"
    "import void iMuse_StopAllSounds();\r\n"
    "import int  iMuse_IsSoundActive(int soundId);\r\n"
    "import void iMuse_SetHook(int soundId, int hookClass, int hookValue, int hookChannel);\r\n"
    "import void iMuse_SetMasterVolume(int volume);\r\n"
    "import void iMuse_SetNativeMt32(int enabled);\r\n"
    "import void iMuse_SetSoundVolume(int soundId, int volume);\r\n"
    "import void iMuse_SetSoundPan(int soundId, int pan);\r\n"
    "import void iMuse_SetSoundTranspose(int soundId, int relative, int transpose);\r\n"
    "import void iMuse_SetSoundSpeed(int soundId, int speed);\r\n"
    "import void iMuse_SetSoundPriority(int soundId, int priority);\r\n"
    "import void iMuse_SetPartVolume(int soundId, int channel, int volume);\r\n"
    "import void iMuse_SetPartOnOff(int soundId, int channel, int onOff);\r\n"
    "import void iMuse_Jump(int soundId, int track, int beat, int tick);\r\n"
    "import void iMuse_Scan(int soundId, int track, int beat, int tick);\r\n"
    "import void iMuse_SetLoop(int soundId, int count, int toBeat, int toTick, int fromBeat, int fromTick);\r\n"
    "import void iMuse_ClearLoop(int soundId);\r\n"
    "import void iMuse_Fade(int soundId, int targetVolume, int timeInTicks);\r\n"
    "import void iMuse_SetNativeMt32(int enabled);\r\n"
    "import int  iMuse_GetPlaybackTrack(int soundId);\r\n"
    "import int  iMuse_GetPlaybackBeat(int soundId);\r\n"
    "import int  iMuse_GetPlaybackTick(int soundId);\r\n"
    "import int  iMuse_GetSoundStatus(int soundId);\r\n"
    "import int  iMuse_GetActiveSoundCount();\r\n"
    "import int  iMuse_GetActiveSoundId(int index);\r\n"
    "import int  iMuse_GetTempo();\r\n"
    "import void iMuse_SetCompatibilityProfile(int profile);\r\n"
    "import int  iMuse_GetCompatibilityProfile();\r\n"
    "import void iMuse_RegisterRolandTimbreMapping(const string name, int gmProgram);\r\n"
    "import void iMuse_ClearRolandTimbreMappings();\r\n";

void CleanupCurrentDriver() {
    g_Engine.setMidiSink(nullptr);
    g_FluidMidiSink.synth = nullptr;
    g_AdlMidiSink.player = nullptr;

    if (g_FluidSynth) {
        delete_fluid_synth(g_FluidSynth);
        g_FluidSynth = nullptr;
    }
    if (g_FluidSettings) {
        delete_fluid_settings(g_FluidSettings);
        g_FluidSettings = nullptr;
    }

    if (g_AdlPlayer) {
        adl_close(g_AdlPlayer);
        g_AdlPlayer = nullptr;
    }

    g_HardwareMidiSink.close();
}

void AudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pDevice;
    (void)pInput;
    float *pOutputFloat = static_cast<float*>(pOutput);

    std::lock_guard<std::mutex> lock(g_Mutex);

    double elapsed = static_cast<double>(frameCount) / 44100.0;
    double ticksPerSecond = g_Engine.transportTicksPerSecond();
    double exactTicks = (ticksPerSecond * elapsed) + g_TickAccumulator;
    uint32_t wholeTicks = static_cast<uint32_t>(exactTicks);
    g_TickAccumulator = exactTicks - wholeTicks;

    if (wholeTicks > 0) {
        g_Engine.advanceAll(wholeTicks);
    }

    if (g_DriverType == DriverType::FluidSynth && g_FluidSynth) {
        fluid_synth_write_float(g_FluidSynth,
                                static_cast<int>(frameCount),
                                pOutputFloat,
                                0,
                                2,
                                pOutputFloat,
                                1,
                                2);
    } else if (g_DriverType == DriverType::AdLib && g_AdlPlayer) {
        ADLMIDI_AudioFormat format;
        format.type = ADLMIDI_SampleType_F32;
        format.containerSize = sizeof(float);
        format.sampleOffset = sizeof(float) * 2;

        adl_generateFormat(g_AdlPlayer, static_cast<int>(frameCount),
                           reinterpret_cast<ADL_UInt8*>(pOutputFloat),
                           reinterpret_cast<ADL_UInt8*>(pOutputFloat + 1),
                           &format);
    } else {
        std::memset(pOutputFloat, 0, frameCount * 2 * sizeof(float));
    }
}

} // namespace

// AGS script exported functions
void __stdcall Ags_iMuse_LoadBank(const char *filename) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (!filename || filename[0] == '\0') {
        return;
    }

    std::string error;
    bool loaded = false;

    if (g_AgsEngine && g_AgsEngine->version >= 28) {
        IAGSStream *stream = g_AgsEngine->OpenFileStream(filename, 1, 1);
        if (stream) {
            int64_t len = stream->GetLength();
            if (len > 0) {
                std::vector<uint8_t> bytes(static_cast<size_t>(len));
                size_t readBytes = stream->Read(bytes.data(), static_cast<size_t>(len));
                if (readBytes > 0) {
                    bytes.resize(readBytes);
                    if (g_Bank.openFromMemory(std::move(bytes), &error)) {
                        loaded = true;
                    }
                }
            }
            stream->Dispose();
        }
    }

    if (!loaded) {
        std::string resolvedPath = filename;
        if (g_AgsEngine && g_AgsEngine->version >= 27) {
            size_t needed = g_AgsEngine->ResolveFilePath(filename, nullptr, 0);
            if (needed > 0) {
                std::vector<char> buf(needed);
                g_AgsEngine->ResolveFilePath(filename, buf.data(), needed);
                resolvedPath = buf.data();
            }
        }
        g_Bank.openFromFile(resolvedPath, &error);
    }
}

void __stdcall Ags_iMuse_SetDriver(int driverType, const char *deviceOrPath) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    CleanupCurrentDriver();

    g_DriverType = static_cast<DriverType>(driverType);

    switch (g_DriverType) {
    case DriverType::FluidSynth: {
        g_Engine.setNativeMt32Output(false);
        g_Engine.setTargetProfile(imuse::TargetProfile::GeneralMidi);

        if (deviceOrPath && deviceOrPath[0] != '\0') {
            std::string resolvedPath = deviceOrPath;
            if (g_AgsEngine && g_AgsEngine->version >= 27) {
                size_t needed = g_AgsEngine->ResolveFilePath(deviceOrPath, nullptr, 0);
                if (needed > 0) {
                    std::vector<char> buf(needed);
                    g_AgsEngine->ResolveFilePath(deviceOrPath, buf.data(), needed);
                    resolvedPath = buf.data();
                }
            }

            g_FluidSettings = new_fluid_settings();
            if (g_FluidSettings) {
                fluid_settings_setnum(g_FluidSettings, "synth.sample-rate", 44100.0);
                g_FluidSynth = new_fluid_synth(g_FluidSettings);
                if (g_FluidSynth) {
                    if (fluid_synth_sfload(g_FluidSynth, resolvedPath.c_str(), 1) != FLUID_FAILED) {
                        g_FluidMidiSink.synth = g_FluidSynth;
                        g_Engine.setMidiSink(&g_FluidMidiSink);
                        g_FluidMidiSink.onAllNotesOff();
                    } else {
                        delete_fluid_synth(g_FluidSynth);
                        g_FluidSynth = nullptr;
                        delete_fluid_settings(g_FluidSettings);
                        g_FluidSettings = nullptr;
                    }
                } else {
                    delete_fluid_settings(g_FluidSettings);
                    g_FluidSettings = nullptr;
                }
            }
        }
        break;
    }
    case DriverType::AdLib: {
        // Emulated AdLib via libADLMIDI. Set target profile to Adlib to select ADL/ROL variants.
        g_Engine.setNativeMt32Output(false);
        g_Engine.setTargetProfile(imuse::TargetProfile::Adlib);

        g_AdlPlayer = adl_init(44100);
        if (g_AdlPlayer) {
            // Set Microsoft Windows 3.1 AdLib bank (bank 0) and OPL3 4 chips polyphony
            adl_setBank(g_AdlPlayer, 0);
            adl_setNumChips(g_AdlPlayer, 4);
            g_AdlMidiSink.player = g_AdlPlayer;
            g_Engine.setMidiSink(&g_AdlMidiSink);
            g_AdlMidiSink.onAllNotesOff();
        }
        break;
    }
    case DriverType::HardwareGM: {
        g_Engine.setNativeMt32Output(false);
        g_Engine.setTargetProfile(imuse::TargetProfile::GeneralMidi);

        int deviceIndex = 0;
        if (deviceOrPath && deviceOrPath[0] != '\0') {
            deviceIndex = std::atoi(deviceOrPath);
        }

        if (g_HardwareMidiSink.open(deviceIndex)) {
            g_Engine.setMidiSink(&g_HardwareMidiSink);
            g_HardwareMidiSink.onAllNotesOff();
        }
        break;
    }
    case DriverType::HardwareMT32: {
        g_Engine.setNativeMt32Output(true);
        g_Engine.setTargetProfile(imuse::TargetProfile::Mt32);

        int deviceIndex = 0;
        if (deviceOrPath && deviceOrPath[0] != '\0') {
            deviceIndex = std::atoi(deviceOrPath);
        }

        if (g_HardwareMidiSink.open(deviceIndex)) {
            g_Engine.setMidiSink(&g_HardwareMidiSink);
            g_HardwareMidiSink.onAllNotesOff();
        }
        break;
    }
    }
}

void __stdcall Ags_iMuse_LoadSoundFont(const char *filename) {
    Ags_iMuse_SetDriver(static_cast<int>(DriverType::FluidSynth), filename);
}

int __stdcall Ags_iMuse_GetMIDIDeviceCount() {
    return GetMidiOutDeviceCount();
}

const char * __stdcall Ags_iMuse_GetMIDIDeviceName(int index) {
    static std::string lastDeviceName;
    std::lock_guard<std::mutex> lock(g_Mutex);
    lastDeviceName = GetMidiOutDeviceName(index);
    return lastDeviceName.c_str();
}

void __stdcall Ags_iMuse_StartSound(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Engine.startSound(static_cast<uint16_t>(soundId));
}

void __stdcall Ags_iMuse_StopSound(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Engine.stopSound(static_cast<uint16_t>(soundId));
}

void __stdcall Ags_iMuse_StopAllSounds() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Engine.stopAllSounds();
}

int __stdcall Ags_iMuse_IsSoundActive(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return g_Engine.isSoundActive(static_cast<uint16_t>(soundId)) ? 1 : 0;
}

void __stdcall Ags_iMuse_SetHook(int soundId, int hookClass, int hookValue, int hookChannel) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    const int16_t command[] = {
        static_cast<int16_t>(0x010C),
        static_cast<int16_t>(soundId),
        static_cast<int16_t>(hookClass),
        static_cast<int16_t>(hookValue),
        static_cast<int16_t>(hookChannel)
    };
    g_Engine.doCommand(5, command);
}

void __stdcall Ags_iMuse_SetMasterVolume(int volume) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[2] = {0x0006, static_cast<int16_t>(volume)};
    g_Engine.doCommand(2, args);
}

void __stdcall Ags_iMuse_SetSoundVolume(int soundId, int volume) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[3] = {0x0102, static_cast<int16_t>(soundId), static_cast<int16_t>(volume)};
    g_Engine.doCommand(3, args);
}

void __stdcall Ags_iMuse_SetSoundPan(int soundId, int pan) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[3] = {0x0103, static_cast<int16_t>(soundId), static_cast<int16_t>(pan)};
    g_Engine.doCommand(3, args);
}

void __stdcall Ags_iMuse_SetSoundTranspose(int soundId, int relative, int transpose) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[4] = {0x0104, static_cast<int16_t>(soundId), static_cast<int16_t>(relative), static_cast<int16_t>(transpose)};
    g_Engine.doCommand(4, args);
}

void __stdcall Ags_iMuse_SetSoundSpeed(int soundId, int speed) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[3] = {0x0106, static_cast<int16_t>(soundId), static_cast<int16_t>(speed)};
    g_Engine.doCommand(3, args);
}

void __stdcall Ags_iMuse_SetSoundPriority(int soundId, int priority) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[3] = {0x0101, static_cast<int16_t>(soundId), static_cast<int16_t>(priority)};
    g_Engine.doCommand(3, args);
}

void __stdcall Ags_iMuse_SetPartVolume(int soundId, int channel, int volume) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[4] = {0x0116, static_cast<int16_t>(soundId), static_cast<int16_t>(channel), static_cast<int16_t>(volume)};
    g_Engine.doCommand(4, args);
}

void __stdcall Ags_iMuse_SetPartOnOff(int soundId, int channel, int onOff) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[4] = {0x010B, static_cast<int16_t>(soundId), static_cast<int16_t>(channel), static_cast<int16_t>(onOff)};
    g_Engine.doCommand(4, args);
}

void __stdcall Ags_iMuse_Jump(int soundId, int track, int beat, int tick) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[5] = {0x0107, static_cast<int16_t>(soundId), static_cast<int16_t>(track), static_cast<int16_t>(beat), static_cast<int16_t>(tick)};
    g_Engine.doCommand(5, args);
}

void __stdcall Ags_iMuse_Scan(int soundId, int track, int beat, int tick) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[5] = {0x0108, static_cast<int16_t>(soundId), static_cast<int16_t>(track), static_cast<int16_t>(beat), static_cast<int16_t>(tick)};
    g_Engine.doCommand(5, args);
}

void __stdcall Ags_iMuse_SetLoop(int soundId, int count, int toBeat, int toTick, int fromBeat, int fromTick) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[7] = {0x0109, static_cast<int16_t>(soundId), static_cast<int16_t>(count), static_cast<int16_t>(toBeat), static_cast<int16_t>(toTick), static_cast<int16_t>(fromBeat), static_cast<int16_t>(fromTick)};
    g_Engine.doCommand(7, args);
}

void __stdcall Ags_iMuse_ClearLoop(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[2] = {0x010A, static_cast<int16_t>(soundId)};
    g_Engine.doCommand(2, args);
}

void __stdcall Ags_iMuse_Fade(int soundId, int targetVolume, int timeInTicks) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[4] = {0x010D, static_cast<int16_t>(soundId), static_cast<int16_t>(targetVolume), static_cast<int16_t>(timeInTicks)};
    g_Engine.doCommand(4, args);
}

void __stdcall Ags_iMuse_SetNativeMt32(int enabled) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Engine.setNativeMt32Output(enabled != 0);
    g_Engine.setTargetProfile(enabled ? imuse::TargetProfile::Mt32 : imuse::TargetProfile::GeneralMidi);
}

int __stdcall Ags_iMuse_GetPlaybackTrack(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    uint16_t track = 0, beat = 0, tick = 0;
    if (g_Engine.getPlaybackLocation(static_cast<uint16_t>(soundId), &track, &beat, &tick)) {
        return static_cast<int>(track);
    }
    return -1;
}

int __stdcall Ags_iMuse_GetPlaybackBeat(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    uint16_t track = 0, beat = 0, tick = 0;
    if (g_Engine.getPlaybackLocation(static_cast<uint16_t>(soundId), &track, &beat, &tick)) {
        return static_cast<int>(beat);
    }
    return -1;
}

int __stdcall Ags_iMuse_GetPlaybackTick(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    uint16_t track = 0, beat = 0, tick = 0;
    if (g_Engine.getPlaybackLocation(static_cast<uint16_t>(soundId), &track, &beat, &tick)) {
        return static_cast<int>(tick);
    }
    return -1;
}

int __stdcall Ags_iMuse_GetSoundStatus(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return g_Engine.getSoundStatus(static_cast<uint16_t>(soundId));
}

int __stdcall Ags_iMuse_GetActiveSoundCount() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return static_cast<int>(g_Engine.activeSoundIds().size());
}

int __stdcall Ags_iMuse_GetActiveSoundId(int index) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    std::vector<uint16_t> activeIds = g_Engine.activeSoundIds();
    if (index >= 0 && index < static_cast<int>(activeIds.size())) {
        return static_cast<int>(activeIds[index]);
    }
    return -1;
}

int __stdcall Ags_iMuse_GetTempo() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return static_cast<int>(g_Engine.currentTempoMicrosPerQuarter());
}

void __stdcall Ags_iMuse_SetCompatibilityProfile(int profile) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (profile == 1) {
        g_Engine.setCompatibilityProfile(imuse::ImuseEngine::CompatibilityProfile::SamAndMax);
    } else {
        g_Engine.setCompatibilityProfile(imuse::ImuseEngine::CompatibilityProfile::GenericV6);
    }
}

int __stdcall Ags_iMuse_GetCompatibilityProfile() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return g_Engine.compatibilityProfile() == imuse::ImuseEngine::CompatibilityProfile::SamAndMax ? 1 : 0;
}

void __stdcall Ags_iMuse_RegisterRolandTimbreMapping(const char *name, int gmProgram) {
    if (name) {
        imuse::RegisterRolandTimbreMapping(name, static_cast<uint8_t>(gmProgram));
    }
}

void __stdcall Ags_iMuse_ClearRolandTimbreMappings() {
    imuse::ClearRolandTimbreMappings();
}

// AGS plugin lifecycle exports
DLLEXPORT const char * AGS_GetPluginName(void) {
    return "iMUSE Classic v6 AGS Plugin";
}

DLLEXPORT int AGS_EditorStartup(IAGSEditor *lpEditor) {
    if (!lpEditor) {
        return -1;
    }
    lpEditor->RegisterScriptHeader(g_iMuseScriptHeader);
    return 0;
}

DLLEXPORT void AGS_EditorShutdown(void) {
    // Nothing special needed on editor shutdown.
}

DLLEXPORT void AGS_EditorProperties(HWND) {
    // No editor properties window needed.
}

DLLEXPORT int AGS_EditorSaveGame(char *, int) {
    return 0;
}

DLLEXPORT void AGS_EditorLoadGame(char *, int) {
    // Load editor state. Not used.
}

DLLEXPORT void AGS_EngineStartup(IAGSEngine *lpEngine) {
    g_AgsEngine = lpEngine;

    // Register script functions
    g_AgsEngine->RegisterScriptFunction("iMuse_LoadBank", (void*)Ags_iMuse_LoadBank);
    g_AgsEngine->RegisterScriptFunction("iMuse_LoadSoundFont", (void*)Ags_iMuse_LoadSoundFont);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetDriver", (void*)Ags_iMuse_SetDriver);
    g_AgsEngine->RegisterScriptFunction("iMuse_GetMIDIDeviceCount", (void*)Ags_iMuse_GetMIDIDeviceCount);
    g_AgsEngine->RegisterScriptFunction("iMuse_GetMIDIDeviceName", (void*)Ags_iMuse_GetMIDIDeviceName);
    g_AgsEngine->RegisterScriptFunction("iMuse_StartSound", (void*)Ags_iMuse_StartSound);
    g_AgsEngine->RegisterScriptFunction("iMuse_StopSound", (void*)Ags_iMuse_StopSound);
    g_AgsEngine->RegisterScriptFunction("iMuse_StopAllSounds", (void*)Ags_iMuse_StopAllSounds);
    g_AgsEngine->RegisterScriptFunction("iMuse_IsSoundActive", (void*)Ags_iMuse_IsSoundActive);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetHook", (void*)Ags_iMuse_SetHook);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetMasterVolume", (void*)Ags_iMuse_SetMasterVolume);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetNativeMt32", (void*)Ags_iMuse_SetNativeMt32);
    g_AgsEngine->RegisterScriptFunction("iMuse_GetPlaybackTrack", (void*)Ags_iMuse_GetPlaybackTrack);
    g_AgsEngine->RegisterScriptFunction("iMuse_GetPlaybackBeat", (void*)Ags_iMuse_GetPlaybackBeat);
    g_AgsEngine->RegisterScriptFunction("iMuse_GetPlaybackTick", (void*)Ags_iMuse_GetPlaybackTick);
    g_AgsEngine->RegisterScriptFunction("iMuse_GetSoundStatus", (void*)Ags_iMuse_GetSoundStatus);
    g_AgsEngine->RegisterScriptFunction("iMuse_GetActiveSoundCount", (void*)Ags_iMuse_GetActiveSoundCount);
    g_AgsEngine->RegisterScriptFunction("iMuse_GetActiveSoundId", (void*)Ags_iMuse_GetActiveSoundId);
    g_AgsEngine->RegisterScriptFunction("iMuse_GetTempo", (void*)Ags_iMuse_GetTempo);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetCompatibilityProfile", (void*)Ags_iMuse_SetCompatibilityProfile);
    g_AgsEngine->RegisterScriptFunction("iMuse_GetCompatibilityProfile", (void*)Ags_iMuse_GetCompatibilityProfile);
    g_AgsEngine->RegisterScriptFunction("iMuse_RegisterRolandTimbreMapping", (void*)Ags_iMuse_RegisterRolandTimbreMapping);
    g_AgsEngine->RegisterScriptFunction("iMuse_ClearRolandTimbreMappings", (void*)Ags_iMuse_ClearRolandTimbreMappings);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetSoundVolume", (void*)Ags_iMuse_SetSoundVolume);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetSoundPan", (void*)Ags_iMuse_SetSoundPan);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetSoundTranspose", (void*)Ags_iMuse_SetSoundTranspose);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetSoundSpeed", (void*)Ags_iMuse_SetSoundSpeed);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetSoundPriority", (void*)Ags_iMuse_SetSoundPriority);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetPartVolume", (void*)Ags_iMuse_SetPartVolume);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetPartOnOff", (void*)Ags_iMuse_SetPartOnOff);
    g_AgsEngine->RegisterScriptFunction("iMuse_Jump", (void*)Ags_iMuse_Jump);
    g_AgsEngine->RegisterScriptFunction("iMuse_Scan", (void*)Ags_iMuse_Scan);
    g_AgsEngine->RegisterScriptFunction("iMuse_SetLoop", (void*)Ags_iMuse_SetLoop);
    g_AgsEngine->RegisterScriptFunction("iMuse_ClearLoop", (void*)Ags_iMuse_ClearLoop);
    g_AgsEngine->RegisterScriptFunction("iMuse_Fade", (void*)Ags_iMuse_Fade);

    g_AgsEngine->RequestEventHook(AGSE_PRESCREENDRAW);

    // Initialize miniaudio
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 44100;
    config.dataCallback      = AudioCallback;
    config.pUserData         = nullptr;

    if (ma_device_init(nullptr, &config, &g_AudioDevice) == MA_SUCCESS) {
        g_AudioDeviceInitialized = true;
        ma_device_start(&g_AudioDevice);
    }
}

DLLEXPORT void AGS_EngineShutdown(void) {
    // Stop and uninitialize audio device
    if (g_AudioDeviceInitialized) {
        ma_device_stop(&g_AudioDevice);
        ma_device_uninit(&g_AudioDevice);
        g_AudioDeviceInitialized = false;
    }

    std::lock_guard<std::mutex> lock(g_Mutex);
    CleanupCurrentDriver();

    g_AgsEngine = nullptr;
}

DLLEXPORT intptr_t AGS_EngineOnEvent(int event, intptr_t) {
    if (event == AGSE_PRESCREENDRAW && g_AgsEngine) {
        std::lock_guard<std::mutex> lock(g_Mutex);
        
        // Process script triggers fired by the iMUSE engine
        for (uint16_t soundId : g_Engine.activeSoundIds()) {
            // fireAllScriptTriggers returns the last marker ID fired, or 0 if none.
            // This is a simplified way to poll triggers. A better way would be 
            // for fireAllScriptTriggers to return a list of fired triggers.
            int marker = g_Engine.fireAllScriptTriggers(soundId);
            if (marker > 0) {
                g_AgsEngine->QueueGameScriptFunction("iMuse_OnTrigger", 1, 2, soundId, marker);
            }
        }
    }
    return 0;
}

DLLEXPORT int AGS_EngineDebugHook(const char *, int, int) {
    return 0;
}

DLLEXPORT void AGS_EngineInitGfx(const char*, void*) {
    // No graphics initialization needed.
}

DLLEXPORT int AGS_PluginV2() {
    return 1;
}
