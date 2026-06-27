#include "WinMidiOut.h"

#include <cstring>

namespace imwrap::vst3tool {

WinMidiOut::WinMidiOut() = default;

WinMidiOut::~WinMidiOut() {
    CloseDevice();
}

std::vector<std::wstring> WinMidiOut::ListDeviceNames() const {
    std::vector<std::wstring> names;
#if defined(_WIN32)
    const UINT deviceCount = midiOutGetNumDevs();
    names.reserve(deviceCount);
    for (UINT index = 0; index < deviceCount; ++index) {
        MIDIOUTCAPSW caps{};
        if (midiOutGetDevCapsW(static_cast<UINT_PTR>(index), &caps, sizeof(caps)) ==
            MMSYSERR_NOERROR) {
            names.emplace_back(caps.szPname);
        } else {
            names.emplace_back(L"Unknown MIDI Device");
        }
    }
#endif
    return names;
}

bool WinMidiOut::OpenDevice(int deviceIndex) {
#if defined(_WIN32)
    if (deviceIndex < 0) {
        CloseDevice();
        return true;
    }

    if (midiOut_ && openDeviceIndex_ == deviceIndex) {
        return true;
    }

    CloseDevice();
    if (midiOutOpen(&midiOut_, static_cast<UINT>(deviceIndex), 0, 0, CALLBACK_NULL) !=
        MMSYSERR_NOERROR) {
        midiOut_ = nullptr;
        openDeviceIndex_ = -1;
        return false;
    }

    openDeviceIndex_ = deviceIndex;
    return true;
#else
    (void)deviceIndex;
    return false;
#endif
}

void WinMidiOut::CloseDevice() {
#if defined(_WIN32)
    if (!midiOut_) {
        openDeviceIndex_ = -1;
        pendingSysEx_.clear();
        return;
    }

    midiOutReset(midiOut_);
    for (PendingSysEx& pending : pendingSysEx_) {
        midiOutUnprepareHeader(midiOut_, &pending.header, sizeof(MIDIHDR));
    }
    pendingSysEx_.clear();
    midiOutClose(midiOut_);
    midiOut_ = nullptr;
    openDeviceIndex_ = -1;
#endif
}

int WinMidiOut::GetOpenDeviceIndex() const {
#if defined(_WIN32)
    return openDeviceIndex_;
#else
    return -1;
#endif
}

bool WinMidiOut::SendSysEx(const std::vector<std::uint8_t>& data) {
#if defined(_WIN32)
    if (!midiOut_ || data.empty()) {
        return false;
    }

    CleanPendingSysEx();

    PendingSysEx pending;
    if (data.front() == 0xF0) {
        pending.buffer.assign(reinterpret_cast<const char*>(data.data()),
                              reinterpret_cast<const char*>(data.data() + data.size()));
    } else {
        pending.buffer.push_back(static_cast<char>(0xF0));
        pending.buffer.insert(pending.buffer.end(), reinterpret_cast<const char*>(data.data()),
                              reinterpret_cast<const char*>(data.data() + data.size()));
    }

    if (pending.buffer.empty() ||
        static_cast<std::uint8_t>(pending.buffer.back()) != static_cast<std::uint8_t>(0xF7)) {
        pending.buffer.push_back(static_cast<char>(0xF7));
    }

    std::memset(&pending.header, 0, sizeof(MIDIHDR));
    pending.header.lpData = pending.buffer.data();
    pending.header.dwBufferLength = static_cast<DWORD>(pending.buffer.size());

    if (midiOutPrepareHeader(midiOut_, &pending.header, sizeof(MIDIHDR)) != MMSYSERR_NOERROR) {
        return false;
    }

    if (midiOutLongMsg(midiOut_, &pending.header, sizeof(MIDIHDR)) != MMSYSERR_NOERROR) {
        midiOutUnprepareHeader(midiOut_, &pending.header, sizeof(MIDIHDR));
        return false;
    }

    pendingSysEx_.push_back(std::move(pending));
    return true;
#else
    (void)data;
    return false;
#endif
}

#if defined(_WIN32)
void WinMidiOut::CleanPendingSysEx() {
    if (!midiOut_) {
        pendingSysEx_.clear();
        return;
    }

    auto it = pendingSysEx_.begin();
    while (it != pendingSysEx_.end()) {
        if (it->header.dwFlags & MHDR_DONE) {
            midiOutUnprepareHeader(midiOut_, &it->header, sizeof(MIDIHDR));
            it = pendingSysEx_.erase(it);
        } else {
            ++it;
        }
    }
}
#endif

} // namespace imwrap::vst3tool
