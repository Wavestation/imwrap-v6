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

/*
 * imwrap-v6 Adventure Game Studio Plugin
 * Copyright (C) 2026 various contributors
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#define AGS_EngineStartup agsimwrap_AGS_EngineStartup
#define AGS_EngineShutdown agsimwrap_AGS_EngineShutdown
#define AGS_EngineOnEvent agsimwrap_AGS_EngineOnEvent
#define AGS_EngineDebugHook agsimwrap_AGS_EngineDebugHook
#define AGS_EngineInitGfx agsimwrap_AGS_EngineInitGfx

extern "C" {
EM_JS(void, JS_WebMIDI_Init, (int deviceIndex), {
    if (navigator.requestMIDIAccess) {
        navigator.requestMIDIAccess({ sysex: true }).then(function(midiAccess) {
            window.imwrapMidiAccess = midiAccess;
            var outputs = midiAccess.outputs.values();
            window.imwrapMidiOutput = null;
            var count = 0;
            for (var output = outputs.next(); output && !output.done; output = outputs.next()) {
                if (count === deviceIndex) {
                    window.imwrapMidiOutput = output.value;
                    break;
                }
                count++;
            }
            if (!window.imwrapMidiOutput) {
                // Fallback to first if index out of bounds
                var outputs2 = midiAccess.outputs.values();
                for (var out2 = outputs2.next(); out2 && !out2.done; out2 = outputs2.next()) {
                    window.imwrapMidiOutput = out2.value;
                    break;
                }
            }
            if (window.imwrapMidiOutput) {
                console.log("iMWrap WebMIDI: Connected to " + window.imwrapMidiOutput.name);
            } else {
                console.warn("iMWrap WebMIDI: No MIDI output ports found.");
            }
        }, function(msg) {
            console.error("iMWrap WebMIDI: Failed to get MIDI access - " + msg);
        });
    } else {
        console.warn("iMWrap WebMIDI: WebMIDI API is not supported in this browser.");
    }
});

EM_JS(void, JS_WebMIDI_SendShortMsg, (unsigned int msg), {
    if (window.imwrapMidiOutput) {
        var status = msg & 0xFF;
        var data1 = (msg >> 8) & 0xFF;
        var data2 = (msg >> 16) & 0xFF;
        // Optimization: some messages don't have data2
        if ((status & 0xF0) === 0xC0 || (status & 0xF0) === 0xD0) {
            window.imwrapMidiOutput.send([status, data1]);
        } else {
            window.imwrapMidiOutput.send([status, data1, data2]);
        }
    }
});

EM_JS(void, JS_WebMIDI_SendSysEx, (int dataPtr, int size), {
    if (window.imwrapMidiOutput) {
        var arr = new Uint8Array(size);
        for (var i = 0; i < size; i++) {
            arr[i] = Module.HEAPU8[dataPtr + i];
        }
        window.imwrapMidiOutput.send(arr);
    }
});
}
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#elif defined(__APPLE__)
#include <CoreMIDI/CoreMIDI.h>
#endif

#ifndef THIS_IS_THE_PLUGIN
#define THIS_IS_THE_PLUGIN
#endif
#include "agsplugin.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <fluidsynth.h>
#include <adlmidi.h>
#include "imwrap/IMWrapEngine.h"
#include "imwrap/ResourceBank.h"

#include <mutex>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <cstdlib>
#include <ctime>

void IMWrapLog(const char* msg);

namespace {

enum class IMWrapDriverType : int {
    FluidSynth = 0,
    AdLib = 1,
    HardwareGM = 2,
    HardwareMT32 = 3
};

struct AgsFluidSynthMidiSink final : imwrap::MidiSink {
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

struct AgsAdlibMidiSink final : imwrap::MidiSink {
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

    void onSysEx(uint16_t, imwrap::ByteView message) override {
        if (!player) {
            return;
        }

        std::vector<uint8_t> sysex(message.size() + 2);
        sysex[0] = 0xF0;
        std::memcpy(sysex.data() + 1, message.data(), message.size());
        sysex[message.size() + 1] = 0xF7;
        adl_rt_systemExclusive(player, sysex.data(), sysex.size());
    }

    void onCustomInstrument(uint16_t /*soundId*/, uint8_t channel, uint32_t type, imwrap::ByteView data) override {
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
struct HardwareMidiOutSink final : imwrap::MidiSink {
#if defined(_WIN32)
    HMIDIOUT hMidiOut = nullptr;

    struct PendingSysEx {
        MIDIHDR hdr;
        std::vector<char> buf;
    };
    std::vector<std::unique_ptr<PendingSysEx>> pendingSysExList;

    void cleanPendingSysEx() {
        if (!hMidiOut) return;
        auto it = pendingSysExList.begin();
        while (it != pendingSysExList.end()) {
            if ((*it)->hdr.dwFlags & MHDR_DONE) {
                midiOutUnprepareHeader(hMidiOut, &(*it)->hdr, sizeof(MIDIHDR));
                it = pendingSysExList.erase(it);
                IMWrapLog("iMWrap WinMM: SysEx transmission completed and header unprepared.");
            } else {
                ++it;
            }
        }
    }
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
        if (MIDIClientCreate(CFSTR("iMWrap Plugin Client"), nullptr, nullptr, &client) == noErr) {
            if (MIDIOutputPortCreate(client, CFSTR("iMWrap Output Port"), &port) == noErr) {
                ItemCount count = MIDIGetNumberOfDestinations();
                if (count > 0 && deviceIndex < static_cast<int>(count)) {
                    destination = MIDIGetDestination(static_cast<CFIndex>(deviceIndex));
                    active = true;
                    return true;
                }
            }
        }
        close();
#elif defined(__EMSCRIPTEN__)
        JS_WebMIDI_Init(deviceIndex);
        active = true;
        return true;
#endif
        return false;
    }

    void close() {
        active = false;
#if defined(_WIN32)
        if (hMidiOut) {
            midiOutReset(hMidiOut);
            for (auto &pending : pendingSysExList) {
                midiOutUnprepareHeader(hMidiOut, &pending->hdr, sizeof(MIDIHDR));
            }
            pendingSysExList.clear();
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
        cleanPendingSysEx();
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
#elif defined(__EMSCRIPTEN__)
        uint32_t msg = status | (data1 << 8) | (hasData2 ? (data2 << 16) : 0);
        JS_WebMIDI_SendShortMsg(msg);
#endif
    }

    void onSysEx(uint16_t, imwrap::ByteView message) override {
        if (!active) {
            return;
        }
#if defined(_WIN32)
        cleanPendingSysEx();

        auto pending = std::make_unique<PendingSysEx>();
        
        if (!message.empty() && static_cast<uint8_t>(message.data()[0]) == 0xF0) {
            pending->buf.assign(message.data(), message.data() + message.size());
        } else {
            pending->buf.push_back(static_cast<char>(0xF0));
            pending->buf.insert(pending->buf.end(), message.data(), message.data() + message.size());
        }
        
        if (pending->buf.empty() || static_cast<uint8_t>(pending->buf.back()) != 0xF7) {
            pending->buf.push_back(static_cast<char>(0xF7));
        }

        std::memset(&pending->hdr, 0, sizeof(MIDIHDR));
        pending->hdr.lpData = pending->buf.data();
        pending->hdr.dwBufferLength = static_cast<DWORD>(pending->buf.size());
        pending->hdr.dwFlags = 0;

        char infoBuf[256];
        std::snprintf(infoBuf, sizeof(infoBuf), "iMWrap WinMM: Queueing SysEx, size=%zu bytes.", pending->buf.size());
        IMWrapLog(infoBuf);

        MMRESULT prepRes = midiOutPrepareHeader(hMidiOut, &pending->hdr, sizeof(MIDIHDR));
        if (prepRes == MMSYSERR_NOERROR) {
            MMRESULT sendRes = midiOutLongMsg(hMidiOut, &pending->hdr, sizeof(MIDIHDR));
            if (sendRes == MMSYSERR_NOERROR) {
                pendingSysExList.push_back(std::move(pending));
            } else {
                char errBuf[256];
                std::snprintf(errBuf, sizeof(errBuf), "iMWrap WinMM Error: midiOutLongMsg failed (error %u).", sendRes);
                IMWrapLog(errBuf);
                midiOutUnprepareHeader(hMidiOut, &pending->hdr, sizeof(MIDIHDR));
            }
        } else {
            char errBuf[256];
            std::snprintf(errBuf, sizeof(errBuf), "iMWrap WinMM Error: midiOutPrepareHeader failed (error %u).", prepRes);
            IMWrapLog(errBuf);
        }
#elif defined(__APPLE__)
        if (!port || !destination) {
            return;
        }
        std::vector<Byte> data(message.size() + 2);
        data[0] = 0xF0;
        std::copy(message.begin(), message.end(), data.begin() + 1);
        data.back() = 0xF7;

        Byte packetBuf[1024];
        MIDIPacketList *packetList = (MIDIPacketList *)packetBuf;
        MIDIPacket *packet = MIDIPacketListInit(packetList);
        packet = MIDIPacketListAdd(packetList, sizeof(packetBuf), packet, 0, data.size(), data.data());
        if (packet) {
            MIDISend(port, destination, packetList);
        }
#elif defined(__EMSCRIPTEN__)
        std::vector<uint8_t> sysex;
        if (!message.empty() && static_cast<uint8_t>(message.data()[0]) == 0xF0) {
            sysex.assign(message.data(), message.data() + message.size());
        } else {
            sysex.push_back(0xF0);
            sysex.insert(sysex.end(), message.data(), message.data() + message.size());
        }
        if (sysex.empty() || static_cast<uint8_t>(sysex.back()) != 0xF7) {
            sysex.push_back(0xF7);
        }
        JS_WebMIDI_SendSysEx(reinterpret_cast<int>(sysex.data()), static_cast<int>(sysex.size()));
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
#elif defined(__EMSCRIPTEN__)
    return 1;
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
#elif defined(__EMSCRIPTEN__)
    return "WebMIDI Output";
#endif
    return "Unknown MIDI Device";
}
} // namespace

// Global variables for the plugin state
IAGSEngine *g_AgsEngine = nullptr;
std::mutex g_Mutex;
imwrap::ResourceBank g_Bank;
imwrap::IMWrapEngine g_Engine(&g_Bank);

struct PendingMarker {
    uint16_t soundId = 0;
    uint8_t marker = 0;
};

std::vector<PendingMarker> g_PendingMarkers;
PendingMarker g_LastMarkerData;
bool g_HasLastMarker = false;

AgsFluidSynthMidiSink g_FluidMidiSink;
AgsAdlibMidiSink g_AdlMidiSink;
HardwareMidiOutSink g_HardwareMidiSink;

fluid_settings_t *g_FluidSettings = nullptr;
fluid_synth_t *g_FluidSynth = nullptr;
std::string g_FluidTempSoundFontPath;
struct ADL_MIDIPlayer *g_AdlPlayer = nullptr;
IMWrapDriverType g_IMWrapDriverType = IMWrapDriverType::FluidSynth;
bool g_LoggingEnabled = false;
bool g_FluidDynLoading = false;
void IMWrapLog(const char* msg) {
#if defined(__EMSCRIPTEN__)
    printf("%s\n", msg);
#endif
    if (!g_LoggingEnabled) return;
    if (g_AgsEngine) {
        g_AgsEngine->PrintDebugConsole(msg);
    }
#if defined(_WIN32)
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
#endif
    FILE* f = fopen("imwrap_debug.log", "a");
    if (f) {
        time_t now = time(nullptr);
        struct tm* tm_info = localtime(&now);
        char timeBuffer[26];
        strftime(timeBuffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(f, "[%s] %s\n", timeBuffer, msg);
        fflush(f);
        fclose(f);
    }
}

namespace {
ma_device g_AudioDevice;
bool g_AudioDeviceInitialized = false;
double g_MicroAccumulator = 0.0;

const char *g_iMWrapScriptHeader =
    "#define IMWRAP_PLUGIN_VERSION 101\r\n"
    "#define IMWRAP_DRIVER_FLUIDSYNTH    0\r\n"
    "#define IMWRAP_DRIVER_ADLIB         1\r\n"
    "#define IMWRAP_DRIVER_HARDWARE_GM   2\r\n"
    "#define IMWRAP_DRIVER_HARDWARE_MT32 3\r\n"
    "\r\n"
    "#define IMWRAP_HOOK_JUMP            0\r\n"
    "#define IMWRAP_HOOK_TRANSPOSE       1\r\n"
    "#define IMWRAP_HOOK_PART_ONOFF      2\r\n"
    "#define IMWRAP_HOOK_PART_VOLUME     3\r\n"
    "#define IMWRAP_HOOK_PART_PROGRAM    4\r\n"
    "#define IMWRAP_HOOK_PART_TRANSPOSE  5\r\n"
    "\r\n"
    "#define IMWRAP_CMD_START_SOUND       8\r\n"
    "#define IMWRAP_CMD_STOP_SOUND        9\r\n"
    "#define IMWRAP_CMD_STOP_ALL_SOUNDS   10\r\n"
    "#define IMWRAP_CMD_SET_PRIORITY      257\r\n"
    "#define IMWRAP_CMD_SET_VOLUME        258\r\n"
    "#define IMWRAP_CMD_SET_PAN           259\r\n"
    "#define IMWRAP_CMD_SET_TRANSPOSE     260\r\n"
    "#define IMWRAP_CMD_SET_DETUNE        261\r\n"
    "#define IMWRAP_CMD_SET_SPEED         262\r\n"
    "#define IMWRAP_CMD_JUMP              263\r\n"
    "#define IMWRAP_CMD_SCAN              264\r\n"
    "#define IMWRAP_CMD_SET_LOOP          265\r\n"
    "#define IMWRAP_CMD_CLEAR_LOOP        266\r\n"
    "#define IMWRAP_CMD_SET_PART_ONOFF    267\r\n"
    "#define IMWRAP_CMD_SET_HOOK          268\r\n"
    "#define IMWRAP_CMD_ADD_FADER         269\r\n"
    "#define IMWRAP_CMD_ENQUEUE_TRIGGER   270\r\n"
    "#define IMWRAP_CMD_ENQUEUE_COMMAND   271\r\n"
    "#define IMWRAP_CMD_CLEAR_QUEUE       272\r\n"
    "#define IMWRAP_CMD_SET_PART_VOLUME   278\r\n"
    "\r\n"
    "import void iMWrap_LoadBank(const string filename);\r\n"
    "import void iMWrap_LoadSoundFont(const string filename);\r\n"
    "import void iMWrap_SetSFDynLoad(bool enable = false);\r\n"
    "import void iMWrap_SetDriver(int driverType, const string deviceOrPath);\r\n"
    "import int  iMWrap_GetMIDIDeviceCount();\r\n"
    "import String iMWrap_GetMIDIDeviceName(int index);\r\n"
    "import void iMWrap_StartSound(int soundId);\r\n"
    "import void iMWrap_StopSound(int soundId);\r\n"
    "import void iMWrap_StopAllSounds();\r\n"
    "import int  iMWrap_IsSoundActive(int soundId);\r\n"
    "import void iMWrap_SetHook(int soundId, int hookClass, int hookValue, int hookChannel);\r\n"
    "import void iMWrap_QueueTrigger(int soundId, int markerId);\r\n"
    "import void iMWrap_QueueCommand(int soundId, int cmd, int a1=0, int a2=0, int a3=0, int a4=0, int a5=0, int a6=0, int a7=0);\r\n"
    "import void iMWrap_CommitQueue(int soundId);\r\n"
    "import void iMWrap_ClearQueue();\r\n"
    "import void iMWrap_SetMasterVolume(int volume);\r\n"
    "import void iMWrap_SetNativeMt32(int enabled);\r\n"
    "import void iMWrap_SetSoundVolume(int soundId, int volume);\r\n"
    "import void iMWrap_SetSoundPan(int soundId, int pan);\r\n"
    "import void iMWrap_SetSoundTranspose(int soundId, int relative, int transpose);\r\n"
    "import void iMWrap_SetSoundSpeed(int soundId, int speed);\r\n"
    "import void iMWrap_SetSoundPriority(int soundId, int priority);\r\n"
    "import void iMWrap_SetPartVolume(int soundId, int channel, int volume);\r\n"
    "import void iMWrap_SetPartOnOff(int soundId, int channel, int onOff);\r\n"
    "import void iMWrap_Jump(int soundId, int track, int beat, int tick);\r\n"
    "import void iMWrap_Scan(int soundId, int track, int beat, int tick);\r\n"
    "import void iMWrap_SetLoop(int soundId, int count, int toBeat, int toTick, int fromBeat, int fromTick);\r\n"
    "import void iMWrap_ClearLoop(int soundId);\r\n"
    "import void iMWrap_Fade(int soundId, int targetVolume, int timeInTicks);\r\n"
    "import int  iMWrap_GetPlaybackTrack(int soundId);\r\n"
    "import int  iMWrap_GetPlaybackBeat(int soundId);\r\n"
    "import int  iMWrap_GetPlaybackTick(int soundId);\r\n"
    "import int  iMWrap_GetSoundStatus(int soundId);\r\n"
    "import int  iMWrap_GetActiveSoundCount();\r\n"
    "import int  iMWrap_GetActiveSoundId(int index);\r\n"
    "import int  iMWrap_GetTempo();\r\n"
    "import void iMWrap_SetCompatibilityProfile(int profile);\r\n"
    "import int  iMWrap_GetCompatibilityProfile();\r\n"
    "import void iMWrap_RegisterRolandTimbreMapping(const string name, int gmProgram);\r\n"
    "import void iMWrap_ClearRolandTimbreMappings();\r\n"
    "import void iMWrap_SetWelcomeMessage(const string message);\r\n"
    "import int  iMWrap_HasExternalConfig();\r\n"
    "import int  iMWrap_ApplyExternalConfig(const string fallbackSoundFont);\r\n"
    "import void iMWrap_EnableLog(int enabled);\r\n"
    "import int  iMWrap_PopMarker();\r\n"
    "import int  iMWrap_GetLastMarker();\r\n";

bool ResolveAgsPath(const char *scriptPath, std::string *resolvedPath) {
    if (!scriptPath || scriptPath[0] == '\0' || !resolvedPath) {
        return false;
    }

    *resolvedPath = scriptPath;
    if (g_AgsEngine && g_AgsEngine->version >= 27) {
        size_t needed = g_AgsEngine->ResolveFilePath(scriptPath, nullptr, 0);
        if (needed > 0) {
            std::vector<char> buf(needed);
            g_AgsEngine->ResolveFilePath(scriptPath, buf.data(), needed);
            *resolvedPath = buf.data();
        }
    }

    return true;
}

bool PathExists(const std::string &path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec);
}

void RemoveTempSoundFontFile() {
    if (g_FluidTempSoundFontPath.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::remove(std::filesystem::path(g_FluidTempSoundFontPath), ec);
    g_FluidTempSoundFontPath.clear();
}

bool ExtractAgsFileToTemp(const char *scriptPath,
                          const char *resolvedPath,
                          std::string *tempPath,
                          std::string *error) {
    if (!tempPath) {
        return false;
    }
    tempPath->clear();

    if (!g_AgsEngine || g_AgsEngine->version < 28) {
        if (error) {
            *error = "AGS file streaming is unavailable with this engine version";
        }
        return false;
    }

    IAGSStream *stream = g_AgsEngine->OpenFileStream(scriptPath, AGSSTREAM_FILE_OPEN, AGSSTREAM_MODE_READ);
    if (!stream) {
        if (error) {
            *error = "AGS could not open the packaged resource as a stream";
        }
        return false;
    }

    auto disposeStream = [&]() {
        if (stream) {
            stream->Dispose();
            stream = nullptr;
        }
    };

    std::error_code ec;
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        disposeStream();
        if (error) {
            *error = "unable to locate a temporary directory";
        }
        return false;
    }

    std::filesystem::path sourcePath = resolvedPath && resolvedPath[0] != '\0'
        ? std::filesystem::path(resolvedPath)
        : std::filesystem::path(scriptPath);
    std::string extension = sourcePath.has_extension() ? sourcePath.extension().string() : ".sf2";

    std::filesystem::path targetPath;
    bool haveTargetPath = false;
    for (int attempt = 0; attempt < 32; ++attempt) {
        std::filesystem::path candidate = tempDir / ("imwrap-" + std::to_string(std::time(nullptr)) +
                                                     "-" + std::to_string(std::rand()) + extension);
        if (!std::filesystem::exists(candidate, ec)) {
            targetPath = candidate;
            haveTargetPath = true;
            break;
        }
        ec.clear();
    }

    if (!haveTargetPath) {
        disposeStream();
        if (error) {
            *error = "unable to allocate a temporary file path";
        }
        return false;
    }

    std::ofstream out(targetPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        disposeStream();
        if (error) {
            *error = "unable to create the temporary SoundFont file";
        }
        return false;
    }

    std::vector<char> buffer(64 * 1024);
    size_t totalBytes = 0;
    while (!stream->EOS()) {
        size_t bytesRead = stream->Read(buffer.data(), buffer.size());
        if (bytesRead == 0) {
            if (stream->GetError()) {
                out.close();
                disposeStream();
                std::filesystem::remove(targetPath, ec);
                if (error) {
                    *error = "failed while reading the packaged SoundFont stream";
                }
                return false;
            }
            break;
        }

        out.write(buffer.data(), static_cast<std::streamsize>(bytesRead));
        if (!out) {
            out.close();
            disposeStream();
            std::filesystem::remove(targetPath, ec);
            if (error) {
                *error = "failed while writing the temporary SoundFont file";
            }
            return false;
        }

        totalBytes += bytesRead;
    }

    out.close();
    disposeStream();

    if (totalBytes == 0) {
        std::filesystem::remove(targetPath, ec);
        if (error) {
            *error = "the packaged SoundFont stream was empty";
        }
        return false;
    }

    *tempPath = targetPath.string();
    return true;
}

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
    RemoveTempSoundFontFile();

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

    double exactMicros = (static_cast<double>(frameCount) * 1000000.0) / 44100.0;
    exactMicros += g_MicroAccumulator;
    uint32_t deltaMicros = static_cast<uint32_t>(exactMicros);
    g_MicroAccumulator = exactMicros - deltaMicros;

    if (deltaMicros > 0) {
        g_Engine.advanceMicroseconds(deltaMicros);
    }

    if (g_IMWrapDriverType == IMWrapDriverType::FluidSynth && g_FluidSynth) {
        fluid_synth_write_float(g_FluidSynth,
                                static_cast<int>(frameCount),
                                pOutputFloat,
                                0,
                                2,
                                pOutputFloat,
                                1,
                                2);
    } else if (g_IMWrapDriverType == IMWrapDriverType::AdLib && g_AdlPlayer) {
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
void Ags_iMWrap_LoadBank(const char *filename) {
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
                        if (g_AgsEngine) {
                            IMWrapLog("iMWrap: Loaded bank from memory stream.");
                        }
                    }
                }
            }
            stream->Dispose();
        }
    }

    if (!loaded) {
        std::string resolvedPath = filename;
        ResolveAgsPath(filename, &resolvedPath);
        if (!g_Bank.openFromFile(resolvedPath, &error)) {
            if (g_AgsEngine && !error.empty()) {
                std::string msg = "iMWrap Error: " + error;
                IMWrapLog(msg.c_str());
            }
        } else if (g_AgsEngine) {
            std::string msg = "iMWrap: Loaded bank from file '" + resolvedPath + "'";
            IMWrapLog(msg.c_str());
        }
    } else if (g_AgsEngine && !error.empty()) {
        std::string msg = "iMWrap Error: " + error;
        IMWrapLog(msg.c_str());
    }
}

void FluidDummyLogCallback(int level, const char *message, void *data) {
    // Ignore all FluidSynth logs to prevent WebAssembly console lag
}

void Ags_iMWrap_SetDriver(int driverType, const char *deviceOrPath) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    CleanupCurrentDriver();

    g_IMWrapDriverType = static_cast<IMWrapDriverType>(driverType);

    switch (g_IMWrapDriverType) {
    case IMWrapDriverType::FluidSynth: {
        g_Engine.setNativeMt32Output(false);
        g_Engine.setTargetProfile(imwrap::TargetProfile::GeneralMidi);

        if (deviceOrPath && deviceOrPath[0] != '\0') {
            std::string resolvedPath = deviceOrPath;
            ResolveAgsPath(deviceOrPath, &resolvedPath);
            std::string loadPath = resolvedPath;
            std::string extractedTempPath;

            if (!PathExists(resolvedPath)) {
                std::string extractionError;
                if (ExtractAgsFileToTemp(deviceOrPath, resolvedPath.c_str(), &extractedTempPath, &extractionError)) {
                    loadPath = extractedTempPath;
                    if (g_AgsEngine) {
                        std::string msg = "iMWrap: Extracted packaged SoundFont to temporary file '" + loadPath + "'";
                        IMWrapLog(msg.c_str());
                    }
                } else if (g_AgsEngine && !extractionError.empty()) {
                    std::string msg = "iMWrap: SoundFont path '" + resolvedPath +
                                      "' is not available on disk; temporary extraction failed: " + extractionError;
                    IMWrapLog(msg.c_str());
                }
            }

            g_FluidSettings = new_fluid_settings();
            if (g_FluidSettings) {
                fluid_settings_setint(g_FluidSettings, "synth.lock-memory", 0);
                fluid_set_log_function(FLUID_DBG, nullptr, nullptr);
                fluid_set_log_function(FLUID_INFO, nullptr, nullptr);
                fluid_set_log_function(FLUID_WARN, nullptr, nullptr);
                fluid_set_log_function(FLUID_ERR, nullptr, nullptr);
                fluid_settings_setnum(g_FluidSettings, "synth.sample-rate", 44100.0);
                fluid_settings_setint(g_FluidSettings, "synth.dynamic-sample-loading", g_FluidDynLoading ? 1 : 0);
                g_FluidSynth = new_fluid_synth(g_FluidSettings);
                if (g_FluidSynth) {
                    if (fluid_synth_sfload(g_FluidSynth, loadPath.c_str(), 1) != FLUID_FAILED) {
                        g_FluidTempSoundFontPath = extractedTempPath;
                        g_FluidMidiSink.synth = g_FluidSynth;
                        g_Engine.setMidiSink(&g_FluidMidiSink);
                        g_FluidMidiSink.onAllNotesOff();
                        if (g_AgsEngine) {
                            std::string msg = "iMWrap: FluidSynth driver initialized with SoundFont '" + loadPath + "'";
                            IMWrapLog(msg.c_str());
                        }
                    } else {
                        if (!extractedTempPath.empty()) {
                            std::error_code ec;
                            std::filesystem::remove(std::filesystem::path(extractedTempPath), ec);
                        }
                        if (g_AgsEngine) {
                            std::string msg = "iMWrap Error: FluidSynth failed to load SoundFont '" + loadPath + "'";
                            IMWrapLog(msg.c_str());
                        }
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
    case IMWrapDriverType::AdLib: {
        // Emulated AdLib via libADLMIDI. Set target profile to Adlib to select ADL/ROL variants.
        g_Engine.setNativeMt32Output(false);
        g_Engine.setTargetProfile(imwrap::TargetProfile::Adlib);

        g_AdlPlayer = adl_init(44100);
        if (g_AdlPlayer) {
            // Set Microsoft Windows 3.1 AdLib bank (bank 0) and OPL3 4 chips polyphony
            adl_setBank(g_AdlPlayer, 0);
            adl_setNumChips(g_AdlPlayer, 4);
            g_AdlMidiSink.player = g_AdlPlayer;
            g_Engine.setMidiSink(&g_AdlMidiSink);
            g_AdlMidiSink.onAllNotesOff();
            if (g_AgsEngine) {
                IMWrapLog("iMWrap: AdLib driver initialized (OPL3, 4 chips).");
            }
        } else if (g_AgsEngine) {
            IMWrapLog("iMWrap Error: Failed to initialize AdLib player.");
        }
        break;
    }
    case IMWrapDriverType::HardwareGM: {
        g_Engine.setNativeMt32Output(false);
        g_Engine.setTargetProfile(imwrap::TargetProfile::GeneralMidi);

        int deviceIndex = 0;
        if (deviceOrPath && deviceOrPath[0] != '\0') {
            deviceIndex = std::atoi(deviceOrPath);
        }

        if (g_HardwareMidiSink.open(deviceIndex)) {
            g_Engine.setMidiSink(&g_HardwareMidiSink);
            g_HardwareMidiSink.onAllNotesOff();
            if (g_AgsEngine) {
                std::string msg = "iMWrap: Hardware GM driver initialized on port " + std::to_string(deviceIndex);
                IMWrapLog(msg.c_str());
            }
        } else if (g_AgsEngine) {
            std::string msg = "iMWrap Error: Failed to open Hardware GM MIDI port " + std::to_string(deviceIndex);
            IMWrapLog(msg.c_str());
        }
        break;
    }
    case IMWrapDriverType::HardwareMT32: {
        g_Engine.setNativeMt32Output(true);
        g_Engine.setTargetProfile(imwrap::TargetProfile::Mt32);

        int deviceIndex = 0;
        if (deviceOrPath && deviceOrPath[0] != '\0') {
            deviceIndex = std::atoi(deviceOrPath);
        }

        if (g_HardwareMidiSink.open(deviceIndex)) {
            g_Engine.setMidiSink(&g_HardwareMidiSink);
            g_HardwareMidiSink.onAllNotesOff();
            if (g_AgsEngine) {
                std::string msg = "iMWrap: Hardware MT-32 driver initialized on port " + std::to_string(deviceIndex);
                IMWrapLog(msg.c_str());
            }
        } else if (g_AgsEngine) {
            std::string msg = "iMWrap Error: Failed to open Hardware MT-32 MIDI port " + std::to_string(deviceIndex);
            IMWrapLog(msg.c_str());
        }
        break;
    }
    }
}

void Ags_iMWrap_LoadSoundFont(const char *filename) {
    Ags_iMWrap_SetDriver(static_cast<int>(IMWrapDriverType::FluidSynth), filename);
}

void Ags_iMWrap_SetSFDynLoad(int enable) {
    g_FluidDynLoading = (enable != 0);
}

int Ags_iMWrap_GetMIDIDeviceCount() {
    return GetMidiOutDeviceCount();
}

const char * Ags_iMWrap_GetMIDIDeviceName(int index) {
    std::string deviceName;
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        deviceName = GetMidiOutDeviceName(index);
    }
    if (g_AgsEngine) {
        const char* scriptString = g_AgsEngine->CreateScriptString(deviceName.c_str());
        if (scriptString) {
            return scriptString;
        } else {
            return g_AgsEngine->CreateScriptString("");
        }
    }
    static std::string lastDeviceName;
    lastDeviceName = deviceName;
    return lastDeviceName.c_str();
}

void Ags_iMWrap_StartSound(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Engine.startSound(static_cast<uint16_t>(soundId));
    if (g_AgsEngine) {
        std::string msg = "iMWrap: Playing sound ID " + std::to_string(soundId);
        IMWrapLog(msg.c_str());
    }
}

void Ags_iMWrap_StopSound(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Engine.stopSound(static_cast<uint16_t>(soundId));
    if (g_AgsEngine) {
        std::string msg = "iMWrap: Stopping sound ID " + std::to_string(soundId);
        IMWrapLog(msg.c_str());
    }
}

void Ags_iMWrap_StopAllSounds() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Engine.stopAllSounds();
    if (g_AgsEngine) {
        IMWrapLog("iMWrap: Stopping all sounds.");
    }
}

int Ags_iMWrap_IsSoundActive(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return g_Engine.isSoundActive(static_cast<uint16_t>(soundId)) ? 1 : 0;
}

void Ags_iMWrap_SetHook(int soundId, int hookClass, int hookValue, int hookChannel) {
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

void Ags_iMWrap_QueueTrigger(int soundId, int markerId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    const int16_t command[] = {
        static_cast<int16_t>(0x010E),
        static_cast<int16_t>(soundId),
        static_cast<int16_t>(markerId)
    };
    g_Engine.doCommand(3, command);
}

void Ags_iMWrap_QueueCommand(int soundId, int cmd, int a1, int a2, int a3, int a4, int a5, int a6, int a7) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    const int16_t args[10] = {
        static_cast<int16_t>(0x010F),
        static_cast<int16_t>(soundId),
        static_cast<int16_t>(cmd), static_cast<int16_t>(a1), static_cast<int16_t>(a2),
        static_cast<int16_t>(a3), static_cast<int16_t>(a4), static_cast<int16_t>(a5),
        static_cast<int16_t>(a6), static_cast<int16_t>(a7)
    };
    g_Engine.doCommand(10, args);
}

void Ags_iMWrap_CommitQueue(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    const int16_t args[] = {
        static_cast<int16_t>(0x010F),
        static_cast<int16_t>(soundId),
        -1
    };
    g_Engine.doCommand(3, args);
}

void Ags_iMWrap_ClearQueue() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    const int16_t args[] = {
        static_cast<int16_t>(0x0110),
        0
    };
    g_Engine.doCommand(2, args);
}

void Ags_iMWrap_SetMasterVolume(int volume) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[2] = {0x0006, static_cast<int16_t>(volume)};
    g_Engine.doCommand(2, args);
}

void Ags_iMWrap_SetSoundVolume(int soundId, int volume) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[3] = {0x0102, static_cast<int16_t>(soundId), static_cast<int16_t>(volume)};
    g_Engine.doCommand(3, args);
}

void Ags_iMWrap_SetSoundPan(int soundId, int pan) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[3] = {0x0103, static_cast<int16_t>(soundId), static_cast<int16_t>(pan)};
    g_Engine.doCommand(3, args);
}

void Ags_iMWrap_SetSoundTranspose(int soundId, int relative, int transpose) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[4] = {0x0104, static_cast<int16_t>(soundId), static_cast<int16_t>(relative), static_cast<int16_t>(transpose)};
    g_Engine.doCommand(4, args);
}

void Ags_iMWrap_SetSoundSpeed(int soundId, int speed) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[3] = {0x0106, static_cast<int16_t>(soundId), static_cast<int16_t>(speed)};
    g_Engine.doCommand(3, args);
}

void Ags_iMWrap_SetSoundPriority(int soundId, int priority) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[3] = {0x0101, static_cast<int16_t>(soundId), static_cast<int16_t>(priority)};
    g_Engine.doCommand(3, args);
}

void Ags_iMWrap_SetPartVolume(int soundId, int channel, int volume) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[4] = {0x0116, static_cast<int16_t>(soundId), static_cast<int16_t>(channel), static_cast<int16_t>(volume)};
    g_Engine.doCommand(4, args);
}

void Ags_iMWrap_SetPartOnOff(int soundId, int channel, int onOff) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[4] = {0x010B, static_cast<int16_t>(soundId), static_cast<int16_t>(channel), static_cast<int16_t>(onOff)};
    g_Engine.doCommand(4, args);
}

void Ags_iMWrap_Jump(int soundId, int track, int beat, int tick) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[5] = {0x0107, static_cast<int16_t>(soundId), static_cast<int16_t>(track), static_cast<int16_t>(beat), static_cast<int16_t>(tick)};
    g_Engine.doCommand(5, args);
}

void Ags_iMWrap_Scan(int soundId, int track, int beat, int tick) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[5] = {0x0108, static_cast<int16_t>(soundId), static_cast<int16_t>(track), static_cast<int16_t>(beat), static_cast<int16_t>(tick)};
    g_Engine.doCommand(5, args);
}

void Ags_iMWrap_SetLoop(int soundId, int count, int toBeat, int toTick, int fromBeat, int fromTick) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[7] = {0x0109, static_cast<int16_t>(soundId), static_cast<int16_t>(count), static_cast<int16_t>(toBeat), static_cast<int16_t>(toTick), static_cast<int16_t>(fromBeat), static_cast<int16_t>(fromTick)};
    g_Engine.doCommand(7, args);
}

void Ags_iMWrap_ClearLoop(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[2] = {0x010A, static_cast<int16_t>(soundId)};
    g_Engine.doCommand(2, args);
}

void Ags_iMWrap_Fade(int soundId, int targetVolume, int timeInTicks) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int16_t args[4] = {0x010D, static_cast<int16_t>(soundId), static_cast<int16_t>(targetVolume), static_cast<int16_t>(timeInTicks)};
    g_Engine.doCommand(4, args);
}

void Ags_iMWrap_SetNativeMt32(int enabled) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Engine.setNativeMt32Output(enabled != 0);
    g_Engine.setTargetProfile(enabled ? imwrap::TargetProfile::Mt32 : imwrap::TargetProfile::GeneralMidi);
}

int Ags_iMWrap_GetPlaybackTrack(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    uint16_t track = 0, beat = 0, tick = 0;
    if (g_Engine.getPlaybackLocation(static_cast<uint16_t>(soundId), &track, &beat, &tick)) {
        return static_cast<int>(track);
    }
    return -1;
}

int Ags_iMWrap_GetPlaybackBeat(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    uint16_t track = 0, beat = 0, tick = 0;
    if (g_Engine.getPlaybackLocation(static_cast<uint16_t>(soundId), &track, &beat, &tick)) {
        return static_cast<int>(beat);
    }
    return -1;
}

int Ags_iMWrap_GetPlaybackTick(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    uint16_t track = 0, beat = 0, tick = 0;
    if (g_Engine.getPlaybackLocation(static_cast<uint16_t>(soundId), &track, &beat, &tick)) {
        return static_cast<int>(tick);
    }
    return -1;
}

int Ags_iMWrap_GetSoundStatus(int soundId) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return g_Engine.getSoundStatus(static_cast<uint16_t>(soundId));
}

int Ags_iMWrap_GetActiveSoundCount() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return static_cast<int>(g_Engine.activeSoundIds().size());
}

int Ags_iMWrap_GetActiveSoundId(int index) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    std::vector<uint16_t> activeIds = g_Engine.activeSoundIds();
    if (index >= 0 && index < static_cast<int>(activeIds.size())) {
        return static_cast<int>(activeIds[index]);
    }
    return -1;
}

int Ags_iMWrap_GetTempo() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return static_cast<int>(g_Engine.currentTempoMicrosPerQuarter());
}

void Ags_iMWrap_SetCompatibilityProfile(int profile) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (profile == 1) {
        g_Engine.setCompatibilityProfile(imwrap::IMWrapEngine::CompatibilityProfile::Snm);
    } else {
        g_Engine.setCompatibilityProfile(imwrap::IMWrapEngine::CompatibilityProfile::GenericV6);
    }
}

int Ags_iMWrap_GetCompatibilityProfile() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return g_Engine.compatibilityProfile() == imwrap::IMWrapEngine::CompatibilityProfile::Snm ? 1 : 0;
}

void Ags_iMWrap_RegisterRolandTimbreMapping(const char *name, int gmProgram) {
    if (name) {
        imwrap::RegisterRolandTimbreMapping(name, static_cast<uint8_t>(gmProgram));
    }
}

void Ags_iMWrap_ClearRolandTimbreMappings() {
    imwrap::ClearRolandTimbreMappings();
}

void Ags_iMWrap_SetWelcomeMessage(const char *message) {
    if (message) {
        g_Engine.setWelcomeMessage(message);
    }
}

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#endif

namespace {

std::string GetConfigFilePath() {
#if defined(_WIN32)
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string s(path);
    size_t dot = s.find_last_of('.');
    if (dot != std::string::npos) {
        return s.substr(0, dot) + ".imc";
    }
    return s + ".imc";
#elif defined(__APPLE__)
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        std::string s(path);
        size_t dot = s.find_last_of('.');
        if (dot != std::string::npos) {
            return s.substr(0, dot) + ".imc";
        }
        return s + ".imc";
    }
    return "imwrap.imc";
#else
    return "imwrap.imc";
#endif
}

struct IMWrapConfig {
    int driverType = -1; // -1 means not configured
    std::string deviceName;
};

inline std::string trim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

IMWrapConfig LoadIMWrapConfig(const std::string &path) {
    IMWrapConfig cfg;
    std::ifstream f(path);
    if (!f.is_open()) return cfg;

    std::string line;
    std::string currentSection;
    while (std::getline(f, line)) {
        // Strip comments
        size_t comment = line.find(';');
        if (comment != std::string::npos) line = line.substr(0, comment);
        comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);

        line = trim(line);
        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            currentSection = trim(line.substr(1, line.size() - 2));
            std::transform(currentSection.begin(), currentSection.end(), currentSection.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            continue;
        }

        if (currentSection == "midi") {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = trim(line.substr(0, eq));
            std::string val = trim(line.substr(eq + 1));

            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c){ return std::tolower(c); });

            if (key == "driver") {
                try {
                    cfg.driverType = std::stoi(val);
                } catch (...) {
                    cfg.driverType = -1;
                }
            } else if (key == "device") {
                cfg.deviceName = val;
            }
        }
    }
    return cfg;
}

bool OpenHardwareMidi(const std::string &deviceOrPath) {
    int count = GetMidiOutDeviceCount();
    // Try opening by name first
    for (int i = 0; i < count; ++i) {
        if (GetMidiOutDeviceName(i) == deviceOrPath) {
            return g_HardwareMidiSink.open(i);
        }
    }
    // Try opening by index
    try {
        if (!deviceOrPath.empty() && std::all_of(deviceOrPath.begin(), deviceOrPath.end(), ::isdigit)) {
            int index = std::stoi(deviceOrPath);
            if (index >= 0 && index < count) {
                return g_HardwareMidiSink.open(index);
            }
        }
    } catch (...) {
    }
    return false;
}

} // namespace

int Ags_iMWrap_HasExternalConfig() {
    std::string configPath = GetConfigFilePath();
    IMWrapConfig cfg = LoadIMWrapConfig(configPath);
    return (cfg.driverType >= 0 && cfg.driverType <= 3) ? 1 : 0;
}

int Ags_iMWrap_ApplyExternalConfig(const char *fallbackSoundFont) {
    std::string configPath = GetConfigFilePath();
    IMWrapConfig cfg = LoadIMWrapConfig(configPath);
    if (cfg.driverType < 0 || cfg.driverType > 3) {
        return 0;
    }

    std::string sfPath = fallbackSoundFont ? fallbackSoundFont : "";
    if (cfg.driverType == static_cast<int>(IMWrapDriverType::FluidSynth)) {
        Ags_iMWrap_SetDriver(cfg.driverType, sfPath.c_str());
        return 1;
    } else if (cfg.driverType == static_cast<int>(IMWrapDriverType::AdLib)) {
        Ags_iMWrap_SetDriver(cfg.driverType, "");
        return 1;
    } else if (cfg.driverType == static_cast<int>(IMWrapDriverType::HardwareGM) ||
               cfg.driverType == static_cast<int>(IMWrapDriverType::HardwareMT32)) {
        
        std::lock_guard<std::mutex> lock(g_Mutex);
        CleanupCurrentDriver();

        g_IMWrapDriverType = static_cast<IMWrapDriverType>(cfg.driverType);

        if (g_IMWrapDriverType == IMWrapDriverType::HardwareGM) {
            g_Engine.setNativeMt32Output(false);
            g_Engine.setTargetProfile(imwrap::TargetProfile::GeneralMidi);
        } else {
            g_Engine.setNativeMt32Output(true);
            g_Engine.setTargetProfile(imwrap::TargetProfile::Mt32);
        }

        // Try opening the specific hardware device name, fallback to 0 if fails/empty
        bool opened = false;
        if (!cfg.deviceName.empty()) {
            opened = OpenHardwareMidi(cfg.deviceName);
        }
        if (!opened) {
            opened = g_HardwareMidiSink.open(0);
        }

        if (opened) {
            g_Engine.setMidiSink(&g_HardwareMidiSink);
            g_HardwareMidiSink.onAllNotesOff();
            if (g_AgsEngine) {
                std::string msg = "iMWrap: Hardware driver initialized from external config with device: " + cfg.deviceName;
                IMWrapLog(msg.c_str());
            }
            return 1;
        } else {
            if (g_AgsEngine) {
                IMWrapLog("iMWrap Error: Failed to open hardware MIDI port from external config.");
            }
            return 0;
        }
    }
    return 0;
}

void Ags_iMWrap_EnableLog(int enabled) {
    g_LoggingEnabled = (enabled != 0);
}

int Ags_iMWrap_PopMarker() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (g_PendingMarkers.empty()) return -1;
    auto pm = g_PendingMarkers.front();
    g_PendingMarkers.erase(g_PendingMarkers.begin());
    return (pm.soundId << 8) | pm.marker;
}

int Ags_iMWrap_GetLastMarker() {
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (!g_HasLastMarker) return -1;
    g_HasLastMarker = false;
    return (g_LastMarkerData.soundId << 8) | g_LastMarkerData.marker;
}

// AGS plugin lifecycle exports
DLLEXPORT const char * AGS_GetPluginName(void) {
    return "iMWrap v6 AGS Plugin";
}

DLLEXPORT int AGS_EditorStartup(IAGSEditor *lpEditor) {
    if (!lpEditor) {
        return -1;
    }
    lpEditor->RegisterScriptHeader(g_iMWrapScriptHeader);
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
    if (g_AgsEngine) {
        IMWrapLog("iMWrap Plugin: Initialized.");
    }

    g_Engine.setLogCallback([](const std::string& str) {
        IMWrapLog(str.c_str());
    });
    g_Engine.setMarkerCallback([](uint16_t soundId, uint8_t marker) {
        g_PendingMarkers.push_back({soundId, marker});
        g_LastMarkerData.soundId = soundId;
        g_LastMarkerData.marker = marker;
        g_HasLastMarker = true;
    });

    // Register script functions
    g_AgsEngine->RegisterScriptFunction("iMWrap_LoadBank", (void*)Ags_iMWrap_LoadBank);
    g_AgsEngine->RegisterScriptFunction("iMWrap_LoadSoundFont", (void*)Ags_iMWrap_LoadSoundFont);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetSFDynLoad", (void*)Ags_iMWrap_SetSFDynLoad);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetDriver", (void*)Ags_iMWrap_SetDriver);
    g_AgsEngine->RegisterScriptFunction("iMWrap_GetMIDIDeviceCount", (void*)Ags_iMWrap_GetMIDIDeviceCount);
    g_AgsEngine->RegisterScriptFunction("iMWrap_GetMIDIDeviceName", (void*)Ags_iMWrap_GetMIDIDeviceName);
    g_AgsEngine->RegisterScriptFunction("iMWrap_StartSound", (void*)Ags_iMWrap_StartSound);
    g_AgsEngine->RegisterScriptFunction("iMWrap_StopSound", (void*)Ags_iMWrap_StopSound);
    g_AgsEngine->RegisterScriptFunction("iMWrap_StopAllSounds", (void*)Ags_iMWrap_StopAllSounds);
    g_AgsEngine->RegisterScriptFunction("iMWrap_IsSoundActive", (void*)Ags_iMWrap_IsSoundActive);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetHook", (void*)Ags_iMWrap_SetHook);
    g_AgsEngine->RegisterScriptFunction("iMWrap_QueueTrigger", (void*)Ags_iMWrap_QueueTrigger);
    g_AgsEngine->RegisterScriptFunction("iMWrap_QueueCommand", (void*)Ags_iMWrap_QueueCommand);
    g_AgsEngine->RegisterScriptFunction("iMWrap_CommitQueue", (void*)Ags_iMWrap_CommitQueue);
    g_AgsEngine->RegisterScriptFunction("iMWrap_ClearQueue", (void*)Ags_iMWrap_ClearQueue);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetMasterVolume", (void*)Ags_iMWrap_SetMasterVolume);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetNativeMt32", (void*)Ags_iMWrap_SetNativeMt32);
    g_AgsEngine->RegisterScriptFunction("iMWrap_GetPlaybackTrack", (void*)Ags_iMWrap_GetPlaybackTrack);
    g_AgsEngine->RegisterScriptFunction("iMWrap_GetPlaybackBeat", (void*)Ags_iMWrap_GetPlaybackBeat);
    g_AgsEngine->RegisterScriptFunction("iMWrap_GetPlaybackTick", (void*)Ags_iMWrap_GetPlaybackTick);
    g_AgsEngine->RegisterScriptFunction("iMWrap_GetSoundStatus", (void*)Ags_iMWrap_GetSoundStatus);
    g_AgsEngine->RegisterScriptFunction("iMWrap_GetActiveSoundCount", (void*)Ags_iMWrap_GetActiveSoundCount);
    g_AgsEngine->RegisterScriptFunction("iMWrap_GetActiveSoundId", (void*)Ags_iMWrap_GetActiveSoundId);
    g_AgsEngine->RegisterScriptFunction("iMWrap_GetTempo", (void*)Ags_iMWrap_GetTempo);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetCompatibilityProfile", (void*)Ags_iMWrap_SetCompatibilityProfile);
    g_AgsEngine->RegisterScriptFunction("iMWrap_GetCompatibilityProfile", (void*)Ags_iMWrap_GetCompatibilityProfile);
    g_AgsEngine->RegisterScriptFunction("iMWrap_RegisterRolandTimbreMapping", (void*)Ags_iMWrap_RegisterRolandTimbreMapping);
    g_AgsEngine->RegisterScriptFunction("iMWrap_ClearRolandTimbreMappings", (void*)Ags_iMWrap_ClearRolandTimbreMappings);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetWelcomeMessage", (void*)Ags_iMWrap_SetWelcomeMessage);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetSoundVolume", (void*)Ags_iMWrap_SetSoundVolume);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetSoundPan", (void*)Ags_iMWrap_SetSoundPan);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetSoundTranspose", (void*)Ags_iMWrap_SetSoundTranspose);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetSoundSpeed", (void*)Ags_iMWrap_SetSoundSpeed);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetSoundPriority", (void*)Ags_iMWrap_SetSoundPriority);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetPartVolume", (void*)Ags_iMWrap_SetPartVolume);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetPartOnOff", (void*)Ags_iMWrap_SetPartOnOff);
    g_AgsEngine->RegisterScriptFunction("iMWrap_Jump", (void*)Ags_iMWrap_Jump);
    g_AgsEngine->RegisterScriptFunction("iMWrap_Scan", (void*)Ags_iMWrap_Scan);
    g_AgsEngine->RegisterScriptFunction("iMWrap_SetLoop", (void*)Ags_iMWrap_SetLoop);
    g_AgsEngine->RegisterScriptFunction("iMWrap_ClearLoop", (void*)Ags_iMWrap_ClearLoop);
    g_AgsEngine->RegisterScriptFunction("iMWrap_Fade", (void*)Ags_iMWrap_Fade);
    g_AgsEngine->RegisterScriptFunction("iMWrap_HasExternalConfig", (void*)Ags_iMWrap_HasExternalConfig);
    g_AgsEngine->RegisterScriptFunction("iMWrap_ApplyExternalConfig", (void*)Ags_iMWrap_ApplyExternalConfig);
    g_AgsEngine->RegisterScriptFunction("iMWrap_EnableLog", (void*)Ags_iMWrap_EnableLog);
    g_AgsEngine->RegisterScriptFunction("iMWrap_PopMarker", (void*)Ags_iMWrap_PopMarker);
    g_AgsEngine->RegisterScriptFunction("iMWrap_GetLastMarker", (void*)Ags_iMWrap_GetLastMarker);

    g_AgsEngine->RequestEventHook(AGSE_PRESCREENDRAW);
    g_AgsEngine->RequestEventHook(AGSE_SAVEGAME);
    g_AgsEngine->RequestEventHook(AGSE_RESTOREGAME);

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

#if defined(__EMSCRIPTEN__)
    int jsChoice = EM_ASM_INT({
        if (typeof window.imwrapMidiChoice !== 'undefined') {
            return window.imwrapMidiChoice;
        }
        return 0;
    });

    if (jsChoice == 1) {
        Ags_iMWrap_SetDriver(static_cast<int>(IMWrapDriverType::HardwareGM), "");
    } else if (jsChoice == 2) {
        Ags_iMWrap_SetDriver(static_cast<int>(IMWrapDriverType::HardwareMT32), "");
    } else {
        // Explicitly initialize FluidSynth to ensure the driver is active
        Ags_iMWrap_SetDriver(static_cast<int>(IMWrapDriverType::FluidSynth), "");
    }
#endif
}

DLLEXPORT void AGS_EngineShutdown(void) {
    // Stop and uninitialize audio device
    if (g_AudioDeviceInitialized) {
        ma_device_stop(&g_AudioDevice);
        ma_device_uninit(&g_AudioDevice);
        g_AudioDeviceInitialized = false;
    }

    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Engine.setMarkerCallback({});
    g_PendingMarkers.clear();
    g_HasLastMarker = false;
    CleanupCurrentDriver();

    g_AgsEngine = nullptr;
}

#include <sstream>

DLLEXPORT intptr_t AGS_EngineOnEvent(int event, intptr_t data) {
    if (event == AGSE_SAVEGAME && g_AgsEngine) {
        std::stringstream ss(std::ios::binary | std::ios::out);
        {
            std::lock_guard<std::mutex> lock(g_Mutex);
            g_Engine.Serialize(ss);
        }
        std::string dataStr = ss.str();
        int32_t size = static_cast<int32_t>(dataStr.size());
        g_AgsEngine->FWrite(&size, sizeof(size), static_cast<int32_t>(data));
        if (size > 0) {
            g_AgsEngine->FWrite(const_cast<char*>(dataStr.data()), size, static_cast<int32_t>(data));
        }
    }
    else if (event == AGSE_RESTOREGAME && g_AgsEngine) {
        int32_t size = 0;
        g_AgsEngine->FRead(&size, sizeof(size), static_cast<int32_t>(data));
        if (size > 0) {
            std::vector<char> buf(size);
            g_AgsEngine->FRead(buf.data(), size, static_cast<int32_t>(data));
            std::string dataStr(buf.begin(), buf.end());
            std::stringstream ss(dataStr, std::ios::binary | std::ios::in);
            std::lock_guard<std::mutex> lock(g_Mutex);
            g_Engine.Deserialize(ss);
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
