#include "MuntPreviewOutput.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "../../third_party/miniaudio/miniaudio.h"

#define MT32EMU_API_TYPE 1
#include <mt32emu.h>

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

struct MuntPreviewOutput::Impl {
    std::recursive_mutex mutex;
    mt32emu_context context = nullptr;
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
        if (!context) {
            std::memset(interleaved, 0, sizeof(float) * frameCount * 2);
            return;
        }
        mt32emu_render_float(context, interleaved, frameCount);
    }
};

MuntPreviewOutput::MuntPreviewOutput()
    : impl_(std::make_unique<Impl>()) {
}

MuntPreviewOutput::~MuntPreviewOutput() {
    stop();
}

bool MuntPreviewOutput::start(std::string* error) {
    stop();

    QDir appDir = QCoreApplication::applicationDirPath();
    QFileInfoList entries = appDir.entryInfoList(QDir::Files | QDir::Readable);

    std::vector<uint8_t> controlRom;
    std::vector<uint8_t> pcmRom;

    for (const QFileInfo& fileInfo : entries) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        QByteArray data = file.readAll();
        file.close();

        if (data.isEmpty()) {
            continue;
        }

        mt32emu_rom_info info;
        if (mt32emu_identify_rom_data(&info, reinterpret_cast<const mt32emu_bit8u*>(data.constData()), static_cast<size_t>(data.size()), nullptr) == MT32EMU_RC_OK) {
            if (info.control_rom_id && controlRom.empty()) {
                controlRom.assign(data.begin(), data.end());
            } else if (info.pcm_rom_id && pcmRom.empty()) {
                pcmRom.assign(data.begin(), data.end());
            }
        }

        if (!controlRom.empty() && !pcmRom.empty()) {
            break;
        }
    }

    if (controlRom.empty() || pcmRom.empty()) {
        SetError("Failed to find valid MUNT control and PCM ROMs in the executable directory.", error);
        return false;
    }

    mt32emu_report_handler_i reportHandler = { nullptr };
    impl_->context = mt32emu_create_context(reportHandler, nullptr);
    if (!impl_->context) {
        SetError("mt32emu_create_context() failed", error);
        return false;
    }

    mt32emu_add_rom_data(impl_->context, controlRom.data(), controlRom.size(), nullptr);
    mt32emu_add_rom_data(impl_->context, pcmRom.data(), pcmRom.size(), nullptr);
    mt32emu_set_stereo_output_samplerate(impl_->context, static_cast<double>(kSampleRate));

    if (mt32emu_open_synth(impl_->context) != MT32EMU_RC_OK) {
        mt32emu_free_context(impl_->context);
        impl_->context = nullptr;
        SetError("mt32emu_open_synth() failed", error);
        return false;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = kSampleRate;
    config.dataCallback = &Impl::DataCallback;
    config.pUserData = impl_.get();

    if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS) {
        mt32emu_free_context(impl_->context);
        impl_->context = nullptr;
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
            mt32emu_free_context(impl_->context);
            impl_->context = nullptr;
        }
        SetError("ma_device_start() failed", error);
        return false;
    }

    SetError("", error);
    return true;
}

void MuntPreviewOutput::stop() {
    if (!impl_) {
        return;
    }

    if (impl_->deviceInitialized) {
        ma_device_stop(&impl_->device);
        ma_device_uninit(&impl_->device);
        impl_->deviceInitialized = false;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->context) {
        mt32emu_free_context(impl_->context);
        impl_->context = nullptr;
    }
}

bool MuntPreviewOutput::isAvailable() const {
    return impl_ && impl_->deviceInitialized && impl_->context != nullptr;
}

void MuntPreviewOutput::onMidiMessage(uint16_t, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->context) {
        uint32_t msg = status | (data1 << 8);
        if (hasData2) {
            msg |= (data2 << 16);
        }
        mt32emu_play_msg(impl_->context, msg);
    }
}

void MuntPreviewOutput::onSysEx(uint16_t, ByteView message) {
    if (!impl_) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->context && !message.empty()) {
        std::vector<uint8_t> buffer;
        if (static_cast<uint8_t>(message.data()[0]) != 0xF0) {
            buffer.push_back(0xF0);
        }
        buffer.insert(buffer.end(), message.data(), message.data() + message.size());
        if (static_cast<uint8_t>(buffer.back()) != 0xF7) {
            buffer.push_back(0xF7);
        }
        mt32emu_play_sysex(impl_->context, buffer.data(), static_cast<uint32_t>(buffer.size()));
    }
}

} // namespace imwrap::gui
