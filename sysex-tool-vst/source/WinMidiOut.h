#pragma once

#include <cstdint>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace imwrap::vst3tool {

class WinMidiOut {
public:
    WinMidiOut();
    ~WinMidiOut();

    WinMidiOut(const WinMidiOut&) = delete;
    WinMidiOut& operator=(const WinMidiOut&) = delete;

    std::vector<std::wstring> ListDeviceNames() const;
    bool OpenDevice(int deviceIndex);
    void CloseDevice();
    int GetOpenDeviceIndex() const;
    bool SendSysEx(const std::vector<std::uint8_t>& data);

private:
#if defined(_WIN32)
    struct PendingSysEx {
        MIDIHDR header{};
        std::vector<char> buffer;
    };

    void CleanPendingSysEx();

    HMIDIOUT midiOut_ = nullptr;
    int openDeviceIndex_ = -1;
    std::vector<PendingSysEx> pendingSysEx_;
#endif
};

} // namespace imwrap::vst3tool
