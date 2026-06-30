#include "AdlibPreviewOutput.h"

#define MINIAUDIO_IMPLEMENTATION
#include "../../third_party/miniaudio/miniaudio.h"

#include "imwrap/ScummAdlibSink.h"

#include <cstring>
#include <mutex>

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
    imwrap::ScummAdlibSink synth;
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
        if (!synth.isAvailable()) {
            std::memset(interleaved, 0, sizeof(float) * frameCount * 2);
            return;
        }
        synth.render(interleaved, frameCount);
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

    if (!impl_->synth.start(kSampleRate, error)) {
        return false;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = kSampleRate;
    config.dataCallback = &Impl::DataCallback;
    config.pUserData = impl_.get();

    if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS) {
        impl_->synth.stop();
        SetError("ma_device_init() failed", error);
        return false;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
        impl_->deviceInitialized = true;
    }

    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        ma_device_uninit(&impl_->device);
        {
            std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
            impl_->deviceInitialized = false;
            impl_->synth.stop();
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
    impl_->synth.stop();
}

bool AdlibPreviewOutput::isAvailable() const {
    return impl_ && impl_->deviceInitialized && impl_->synth.isAvailable();
}

void AdlibPreviewOutput::onMidiMessage(uint16_t, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    impl_->synth.onMidiMessage(0, status, data1, hasData2, data2);
}

void AdlibPreviewOutput::onSysEx(uint16_t, ByteView message) {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    impl_->synth.onSysEx(0, message);
}

void AdlibPreviewOutput::onCustomInstrument(uint16_t, uint8_t channel, uint32_t type, ByteView data) {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    impl_->synth.onCustomInstrument(0, channel, type, data);
}

void AdlibPreviewOutput::onAllNotesOff() {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    impl_->synth.onAllNotesOff();
}

} // namespace imwrap::gui
