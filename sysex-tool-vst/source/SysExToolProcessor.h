#pragma once

#include "SysExToolModel.h"

#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/utility/ringbuffer.h"

#include <cstdint>
#include <vector>

namespace imwrap::vst3tool {

class SysExToolProcessor final : public Steinberg::Vst::AudioEffect {
public:
    SysExToolProcessor();

    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new SysExToolProcessor());
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs,
                                                     Steinberg::int32 numIns,
                                                     Steinberg::Vst::SpeakerArrangement* outputs,
                                                     Steinberg::int32 numOuts) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(
        Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::uint32 PLUGIN_API getTailSamples() SMTG_OVERRIDE;

private:
    struct PendingSendRequest {
        SysExToolState state;
    };

    bool ApplyParameterChanges(Steinberg::Vst::IParameterChanges* changes);
    void EmitDiagnosticPulse(Steinberg::Vst::ProcessData& data);
    void EmitDiagnosticProgramChange(Steinberg::Vst::IEventList* outputEvents) const;
    void EmitSysEx(Steinberg::Vst::IEventList* outputEvents) const;

    SysExToolState state_;
    bool sendLatched_ = false;
    bool pendingParameterSend_ = false;
    mutable std::vector<std::uint8_t> sysexScratch_;
    Steinberg::OneReaderOneWriter::RingBuffer<PendingSendRequest> pendingSendRequests_{8};
    Steinberg::int32 diagnosticPulseSamplesRemaining_ = 0;
    double diagnosticPulsePhase_ = 0.0;
};

} // namespace imwrap::vst3tool
