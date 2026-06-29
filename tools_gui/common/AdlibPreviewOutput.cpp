#include "AdlibPreviewOutput.h"

#define MINIAUDIO_IMPLEMENTATION
#include "../../third_party/miniaudio/miniaudio.h"
#include "../../third_party/libadlmidi/include/adlmidi.h"

#include <array>
#include <cstring>
#include <mutex>
#include <vector>

namespace imwrap::gui {
namespace {

constexpr uint32_t kSampleRate = 44100;

void SetError(const char* message, std::string* error) {
    if (error) {
        *error = message ? message : "";
    }
}

} // namespace

struct AdlibPreviewOutput::Impl {
    std::recursive_mutex mutex;
    ADL_MIDIPlayer* player = nullptr;
    ma_device device = {};
    bool deviceInitialized = false;

    static void DataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
        (void)input;
        auto* impl = static_cast<Impl*>(device ? device->pUserData : nullptr);
        float* out = static_cast<float*>(output);
        if (!out || frameCount == 0) {
            return;
        }
        if (!impl) {
            std::memset(out, 0, sizeof(float) * frameCount * 2);
            return;
        }
        impl->render(out, frameCount);
    }

    void render(float* interleaved, ma_uint32 frameCount) {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        if (!player) {
            std::memset(interleaved, 0, sizeof(float) * frameCount * 2);
            return;
        }

        ADLMIDI_AudioFormat format;
        format.type = ADLMIDI_SampleType_F32;
        format.containerSize = sizeof(float);
        format.sampleOffset = sizeof(float) * 2;

        adl_generateFormat(player,
                           static_cast<int>(frameCount),
                           reinterpret_cast<ADL_UInt8*>(interleaved),
                           reinterpret_cast<ADL_UInt8*>(interleaved + 1),
                           &format);
    }
};

AdlibPreviewOutput::AdlibPreviewOutput()
    : impl_(std::make_unique<Impl>()) {
}

AdlibPreviewOutput::~AdlibPreviewOutput() {
    stop();
}

bool AdlibPreviewOutput::start(std::string* error) {
    stop();

    ADL_MIDIPlayer* player = adl_init(static_cast<long>(kSampleRate));
    if (!player) {
        SetError("adl_init() failed", error);
        return false;
    }

    adl_setVolumeRangeModel(player, ADLMIDI_VolumeModel_DMX);
    adl_setBank(player, 0);
    adl_setNumChips(player, 1);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = kSampleRate;
    config.dataCallback = &Impl::DataCallback;
    config.pUserData = impl_.get();

    if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS) {
        adl_close(player);
        SetError("ma_device_init() failed", error);
        return false;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
        impl_->player = player;
        impl_->deviceInitialized = true;
    }

    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        ma_device_uninit(&impl_->device);
        {
            std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
            impl_->deviceInitialized = false;
            adl_close(impl_->player);
            impl_->player = nullptr;
        }
        SetError("ma_device_start() failed", error);
        return false;
    }

    SetError("", error);
    return true;
}

void AdlibPreviewOutput::stop() {
    if (!impl_) {
        return;
    }

    if (impl_->deviceInitialized) {
        ma_device_stop(&impl_->device);
        ma_device_uninit(&impl_->device);
        impl_->deviceInitialized = false;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->player) {
        adl_close(impl_->player);
        impl_->player = nullptr;
    }
}

bool AdlibPreviewOutput::isAvailable() const {
    return impl_ && impl_->player != nullptr && impl_->deviceInitialized;
}

void AdlibPreviewOutput::onMidiMessage(uint16_t, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (!impl_->player) {
        return;
    }

    const uint8_t messageType = status & 0xF0;
    const uint8_t channel = status & 0x0F;
    switch (messageType) {
    case 0x80:
        adl_rt_noteOff(impl_->player, channel, data1);
        break;
    case 0x90:
        if (!hasData2 || data2 == 0) {
            adl_rt_noteOff(impl_->player, channel, data1);
        } else {
            adl_rt_noteOn(impl_->player, channel, data1, data2);
        }
        break;
    case 0xA0:
        if (hasData2) {
            adl_rt_noteAfterTouch(impl_->player, channel, data1, data2);
        }
        break;
    case 0xB0:
        if (hasData2) {
            adl_rt_controllerChange(impl_->player, channel, data1, data2);
        }
        break;
    case 0xC0:
        adl_rt_patchChange(impl_->player, channel, data1);
        break;
    case 0xD0:
        adl_rt_channelAfterTouch(impl_->player, channel, data1);
        break;
    case 0xE0:
        if (hasData2) {
            adl_rt_pitchBendML(impl_->player, channel, data2, data1);
        }
        break;
    default:
        break;
    }
}

void AdlibPreviewOutput::onSysEx(uint16_t, ByteView message) {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (!impl_->player || message.empty()) {
        return;
    }

    std::vector<uint8_t> sysex;
    sysex.reserve(message.size() + 2);
    if (static_cast<uint8_t>(message.data()[0]) != 0xF0) {
        sysex.push_back(0xF0);
    }
    sysex.insert(sysex.end(), message.data(), message.data() + message.size());
    if (sysex.empty() || sysex.back() != 0xF7) {
        sysex.push_back(0xF7);
    }
    adl_rt_systemExclusive(impl_->player, sysex.data(), sysex.size());
}

void AdlibPreviewOutput::onCustomInstrument(uint16_t, uint8_t channel, uint32_t type, ByteView data) {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (!impl_->player || type != 'ADL ' || data.size() < 11) {
        return;
    }

    ADL_Instrument instrument;
    std::memset(&instrument, 0, sizeof(instrument));
    instrument.version = ADLMIDI_InstrumentVersion;
    instrument.inst_flags = ADLMIDI_Ins_2op;
    instrument.fb_conn1_C0 = static_cast<uint8_t>(data.data()[10]);

    instrument.operators[0].avekf_20 = static_cast<uint8_t>(data.data()[0]);
    instrument.operators[0].ksl_l_40 = static_cast<uint8_t>(data.data()[1]);
    instrument.operators[0].atdec_60 = static_cast<uint8_t>(data.data()[2]);
    instrument.operators[0].susrel_80 = static_cast<uint8_t>(data.data()[3]);
    instrument.operators[0].waveform_E0 = static_cast<uint8_t>(data.data()[4]);

    instrument.operators[1].avekf_20 = static_cast<uint8_t>(data.data()[5]);
    instrument.operators[1].ksl_l_40 = static_cast<uint8_t>(data.data()[6]);
    instrument.operators[1].atdec_60 = static_cast<uint8_t>(data.data()[7]);
    instrument.operators[1].susrel_80 = static_cast<uint8_t>(data.data()[8]);
    instrument.operators[1].waveform_E0 = static_cast<uint8_t>(data.data()[9]);

    ADL_BankId bankId;
    bankId.msb = 0x7D;
    bankId.lsb = channel;
    bankId.percussive = 0;

    ADL_Bank bank;
    if (adl_getBank(impl_->player, &bankId, ADLMIDI_Bank_Create, &bank) == 0) {
        adl_setInstrument(impl_->player, &bank, 0, &instrument);
    }

    adl_rt_bankChangeMSB(impl_->player, static_cast<ADL_UInt8>(channel), 0x7D);
    adl_rt_bankChangeLSB(impl_->player, static_cast<ADL_UInt8>(channel), static_cast<ADL_UInt8>(channel));
    adl_rt_patchChange(impl_->player, static_cast<int>(channel), 0);
}

void AdlibPreviewOutput::onAllNotesOff() {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->player) {
        adl_rt_resetState(impl_->player);
    }
}

} // namespace imwrap::gui
