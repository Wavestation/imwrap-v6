#include "SysExToolProcessor.h"

#include "SysExToolIds.h"
#include "SysExToolStateIo.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "public.sdk/source/vst/vsteventshelper.h"

#include <cmath>
#include <cstring>

namespace imwrap::vst3tool {

namespace {

constexpr double kDiagnosticPulseFrequencyHz = 880.0;
constexpr double kDiagnosticPulseAmplitude = 0.05;
constexpr double kDiagnosticPulseDurationSeconds = 0.01;
constexpr double kTwoPi = 6.28318530717958647692;

template <typename SampleType>
void ClearOutputs(Steinberg::Vst::ProcessData& data) {
    for (Steinberg::int32 outputIndex = 0; outputIndex < data.numOutputs; ++outputIndex) {
        Steinberg::Vst::AudioBusBuffers& output = data.outputs[outputIndex];
        output.silenceFlags = 0;

        for (Steinberg::int32 channelIndex = 0; channelIndex < output.numChannels; ++channelIndex) {
            SampleType* channel = output.channelBuffers32
                                      ? reinterpret_cast<SampleType*>(output.channelBuffers32[channelIndex])
                                      : output.channelBuffers64
                                            ? reinterpret_cast<SampleType*>(
                                                  output.channelBuffers64[channelIndex])
                                            : nullptr;
            if (!channel) {
                continue;
            }

            for (Steinberg::int32 sampleIndex = 0; sampleIndex < data.numSamples; ++sampleIndex) {
                channel[sampleIndex] = static_cast<SampleType>(0);
            }
        }
    }
}

void SilenceOutputs(Steinberg::Vst::ProcessData& data) {
    if (data.numOutputs <= 0) {
        return;
    }

    switch (data.symbolicSampleSize) {
    case Steinberg::Vst::kSample32:
        ClearOutputs<Steinberg::Vst::Sample32>(data);
        break;
    case Steinberg::Vst::kSample64:
        ClearOutputs<Steinberg::Vst::Sample64>(data);
        break;
    default:
        break;
    }
}

} // namespace

SysExToolProcessor::SysExToolProcessor() {
    setControllerClass(Steinberg::FUID::fromTUID(SysExToolControllerUID));
}

Steinberg::tresult PLUGIN_API SysExToolProcessor::initialize(Steinberg::FUnknown* context) {
    const Steinberg::tresult result = AudioEffect::initialize(context);
    if (result != Steinberg::kResultTrue) {
        return result;
    }

    addAudioOutput(STR16("Audio Output"), Steinberg::Vst::SpeakerArr::kStereo);
    addEventInput(STR16("MIDI In"), 1);
    addEventOutput(STR16("MIDI Out"));
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API SysExToolProcessor::setBusArrangements(
    Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
    Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) {
    if (numIns == 0 && numOuts == 1 && outputs[0] == Steinberg::Vst::SpeakerArr::kStereo) {
        return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
    }

    return Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API SysExToolProcessor::canProcessSampleSize(
    Steinberg::int32 symbolicSampleSize) {
    if (symbolicSampleSize == Steinberg::Vst::kSample32 ||
        symbolicSampleSize == Steinberg::Vst::kSample64) {
        return Steinberg::kResultTrue;
    }

    return Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API SysExToolProcessor::process(Steinberg::Vst::ProcessData& data) {
    SilenceOutputs(data);
    EmitDiagnosticPulse(data);

    if (ApplyParameterChanges(data.inputParameterChanges)) {
        pendingParameterSend_ = true;
    }

    if (!data.outputEvents) {
        return Steinberg::kResultTrue;
    }

    PendingSendRequest request{};
    bool emittedQueuedSend = false;
    while (pendingSendRequests_.pop(request)) {
        state_ = request.state;
        diagnosticPulseSamplesRemaining_ = static_cast<Steinberg::int32>(
            std::max(1.0, processSetup.sampleRate * kDiagnosticPulseDurationSeconds));
        EmitDiagnosticProgramChange(data.outputEvents);
        EmitSysEx(data.outputEvents);
        emittedQueuedSend = true;
    }

    if (emittedQueuedSend) {
        pendingParameterSend_ = false;
    } else if (pendingParameterSend_) {
        diagnosticPulseSamplesRemaining_ = static_cast<Steinberg::int32>(
            std::max(1.0, processSetup.sampleRate * kDiagnosticPulseDurationSeconds));
        EmitDiagnosticProgramChange(data.outputEvents);
        EmitSysEx(data.outputEvents);
        pendingParameterSend_ = false;
    }

    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API SysExToolProcessor::notify(Steinberg::Vst::IMessage* message) {
    if (!message) {
        return Steinberg::kInvalidArgument;
    }

    if (const char* messageId = message->getMessageID();
        messageId && std::strcmp(messageId, kSendRequestMessageId) == 0) {
        if (Steinberg::Vst::IAttributeList* attributes = message->getAttributes()) {
            const void* payload = nullptr;
            Steinberg::uint32 payloadSize = 0;
            if (attributes->getBinary(kSendRequestStateKey, payload, payloadSize) ==
                    Steinberg::kResultTrue &&
                payload && payloadSize == sizeof(SysExToolState)) {
                PendingSendRequest request{};
                std::memcpy(&request.state, payload, sizeof(request.state));
                if (pendingSendRequests_.push(request)) {
                    return Steinberg::kResultTrue;
                }
            }
        }
    }

    return AudioEffect::notify(message);
}

Steinberg::tresult PLUGIN_API SysExToolProcessor::setState(Steinberg::IBStream* state) {
    if (!ReadState(state, &state_)) {
        return Steinberg::kResultFalse;
    }

    sendLatched_ = false;
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API SysExToolProcessor::getState(Steinberg::IBStream* state) {
    return WriteState(state, state_) ? Steinberg::kResultOk : Steinberg::kResultFalse;
}

Steinberg::uint32 PLUGIN_API SysExToolProcessor::getTailSamples() {
    return Steinberg::Vst::kInfiniteTail;
}

void SysExToolProcessor::EmitDiagnosticPulse(Steinberg::Vst::ProcessData& data) {
    if (diagnosticPulseSamplesRemaining_ <= 0 || data.numOutputs <= 0 || data.numSamples <= 0 ||
        processSetup.sampleRate <= 0.0) {
        return;
    }

    const Steinberg::int32 samplesToWrite =
        std::min(data.numSamples, diagnosticPulseSamplesRemaining_);
    const double phaseIncrement = kTwoPi * kDiagnosticPulseFrequencyHz / processSetup.sampleRate;

    for (Steinberg::int32 outputIndex = 0; outputIndex < data.numOutputs; ++outputIndex) {
        Steinberg::Vst::AudioBusBuffers& output = data.outputs[outputIndex];
        output.silenceFlags = 0;

        for (Steinberg::int32 channelIndex = 0; channelIndex < output.numChannels; ++channelIndex) {
            if (data.symbolicSampleSize == Steinberg::Vst::kSample32 && output.channelBuffers32) {
                auto* channel = output.channelBuffers32[channelIndex];
                if (!channel) {
                    continue;
                }
                double phase = diagnosticPulsePhase_;
                for (Steinberg::int32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex) {
                    channel[sampleIndex] += static_cast<Steinberg::Vst::Sample32>(
                        std::sin(phase) * kDiagnosticPulseAmplitude);
                    phase += phaseIncrement;
                }
            } else if (data.symbolicSampleSize == Steinberg::Vst::kSample64 &&
                       output.channelBuffers64) {
                auto* channel = output.channelBuffers64[channelIndex];
                if (!channel) {
                    continue;
                }
                double phase = diagnosticPulsePhase_;
                for (Steinberg::int32 sampleIndex = 0; sampleIndex < samplesToWrite; ++sampleIndex) {
                    channel[sampleIndex] += static_cast<Steinberg::Vst::Sample64>(
                        std::sin(phase) * kDiagnosticPulseAmplitude);
                    phase += phaseIncrement;
                }
            }
        }
    }

    diagnosticPulsePhase_ += phaseIncrement * static_cast<double>(samplesToWrite);
    while (diagnosticPulsePhase_ >= kTwoPi) {
        diagnosticPulsePhase_ -= kTwoPi;
    }
    diagnosticPulseSamplesRemaining_ -= samplesToWrite;
}

bool SysExToolProcessor::ApplyParameterChanges(Steinberg::Vst::IParameterChanges* changes) {
    if (!changes) {
        return false;
    }

    bool triggerSend = false;

    const Steinberg::int32 parameterCount = changes->getParameterCount();
    for (Steinberg::int32 parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex) {
        Steinberg::Vst::IParamValueQueue* queue = changes->getParameterData(parameterIndex);
        if (!queue) {
            continue;
        }

        const Steinberg::int32 pointCount = queue->getPointCount();
        if (pointCount <= 0) {
            continue;
        }

        switch (queue->getParameterId()) {
        case kSendId: {
            for (Steinberg::int32 pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
                Steinberg::Vst::ParamValue normalizedValue = 0.0;
                Steinberg::int32 sampleOffset = 0;
                if (queue->getPoint(pointIndex, sampleOffset, normalizedValue) !=
                    Steinberg::kResultTrue) {
                    continue;
                }

                const bool requested = DenormalizeBool(normalizedValue);
                if (requested != sendLatched_) {
                    triggerSend = true;
                }
                sendLatched_ = requested;
            }
            break;
        }
        default: {
            Steinberg::Vst::ParamValue normalizedValue = 0.0;
            Steinberg::int32 sampleOffset = 0;
            if (queue->getPoint(pointCount - 1, sampleOffset, normalizedValue) !=
                Steinberg::kResultTrue) {
                continue;
            }

            switch (queue->getParameterId()) {
        case kMessageTypeId:
            state_.messageType =
                static_cast<MessageType>(DenormalizeInt(normalizedValue, 0, kMessageTypeCount - 1));
            break;
        case kPartId: state_.part = DenormalizeInt(normalizedValue, 0, 15); break;
        case kChannelId: state_.channel = DenormalizeInt(normalizedValue, 0, 15); break;
        case kUnknownId: state_.unknown = DenormalizeInt(normalizedValue, 0, 127); break;
        case kPartOnId: state_.partOn = DenormalizeBool(normalizedValue); break;
        case kReverbId: state_.reverb = DenormalizeBool(normalizedValue); break;
        case kPriorityId: state_.priority = DenormalizeInt(normalizedValue, 0, 255); break;
        case kVolumeId: state_.volume = DenormalizeInt(normalizedValue, 0, 127); break;
        case kPanId: state_.pan = DenormalizeInt(normalizedValue, -64, 63); break;
        case kPercussionId: state_.percussion = DenormalizeBool(normalizedValue); break;
        case kTransposeId: state_.transpose = DenormalizeInt(normalizedValue, -127, 127); break;
        case kDetuneId: state_.detune = DenormalizeInt(normalizedValue, -128, 127); break;
        case kPitchbendFactorId:
            state_.pitchbendFactor = DenormalizeInt(normalizedValue, 0, 255);
            break;
        case kProgramId: state_.program = DenormalizeInt(normalizedValue, 0, 127); break;
        case kParamId: state_.param = DenormalizeInt(normalizedValue, 0, 65535); break;
        case kParamValueId: state_.paramValue = DenormalizeInt(normalizedValue, 0, 65535); break;
        case kHookCommandId: state_.hookCommand = DenormalizeInt(normalizedValue, 0, 255); break;
        case kTargetTrackId: state_.targetTrack = DenormalizeInt(normalizedValue, 0, 65535); break;
        case kTargetBeatId: state_.targetBeat = DenormalizeInt(normalizedValue, 0, 65535); break;
        case kTargetTickId: state_.targetTick = DenormalizeInt(normalizedValue, 0, 65535); break;
        case kRelativeId: state_.relative = DenormalizeBool(normalizedValue); break;
        case kHookValueId: state_.hookValue = DenormalizeInt(normalizedValue, -128, 255); break;
        case kMarkerValueId: state_.markerValue = DenormalizeInt(normalizedValue, 0, 127); break;
        case kLoopCountId: state_.loopCount = DenormalizeInt(normalizedValue, 0, 65535); break;
        case kLoopToBeatId: state_.loopToBeat = DenormalizeInt(normalizedValue, 0, 65535); break;
        case kLoopToTickId: state_.loopToTick = DenormalizeInt(normalizedValue, 0, 65535); break;
        case kLoopFromBeatId: state_.loopFromBeat = DenormalizeInt(normalizedValue, 0, 65535); break;
        case kLoopFromTickId: state_.loopFromTick = DenormalizeInt(normalizedValue, 0, 65535); break;
        case kInstrumentId: state_.instrument = DenormalizeInt(normalizedValue, 0, 65535); break;
        default:
            break;
        }
        } break;
        }
    }

    return triggerSend;
}

void SysExToolProcessor::EmitDiagnosticProgramChange(Steinberg::Vst::IEventList* outputEvents) const {
    const Steinberg::uint8 channel =
        static_cast<Steinberg::uint8>(state_.channel < 0 ? 0 : state_.channel > 15 ? 15
                                                                                   : state_.channel);
    const Steinberg::int8 program =
        static_cast<Steinberg::int8>(state_.program < 0 ? 0 : state_.program > 127 ? 127
                                                                                   : state_.program);

    Steinberg::Vst::Event event{};
    Steinberg::Vst::Helpers::initLegacyMIDICCOutEvent(
        event, Steinberg::Vst::kCtrlProgramChange, channel, program);
    event.sampleOffset = 0;
    event.flags = Steinberg::Vst::Event::kIsLive;
    outputEvents->addEvent(event);
}

void SysExToolProcessor::EmitSysEx(Steinberg::Vst::IEventList* outputEvents) const {
    sysexScratch_.clear();
    if (!BuildSysExBytes(state_, &sysexScratch_, nullptr) || sysexScratch_.empty()) {
        return;
    }

    Steinberg::Vst::Event event{};
    event.busIndex = 0;
    event.sampleOffset = 0;
    event.ppqPosition = 0;
    event.flags = Steinberg::Vst::Event::kIsLive;
    event.type = Steinberg::Vst::Event::kDataEvent;
    event.data.type = Steinberg::Vst::DataEvent::kMidiSysEx;
    event.data.size = static_cast<Steinberg::uint32>(sysexScratch_.size());
    event.data.bytes = sysexScratch_.data();
    outputEvents->addEvent(event);
}

} // namespace imwrap::vst3tool
