#pragma once

#include <memory>
#include <string>

#include "imwrap/MidiSink.h"

namespace imwrap::gui {

class AdlibPreviewOutput final : public MidiSink {
public:
    AdlibPreviewOutput();
    ~AdlibPreviewOutput() override;

    bool start(std::string* error = nullptr);
    void stop();
    bool isAvailable() const override;

    void onMidiMessage(uint16_t soundId, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) override;
    void onSysEx(uint16_t soundId, ByteView message) override;
    void onCustomInstrument(uint16_t soundId, uint8_t channel, uint32_t type, ByteView data) override;
    void onAllNotesOff() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace imwrap::gui
