#include "SysExToolController.h"

#include "SysExToolEditor.h"
#include "SysExToolIds.h"
#include "SysExToolStateIo.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/vstparameters.h"

#include <algorithm>
#include <cstring>

namespace imwrap::vst3tool {
namespace {

using Steinberg::Vst::Parameter;
using Steinberg::Vst::ParameterInfo;
using Steinberg::Vst::RangeParameter;
using Steinberg::Vst::StringListParameter;

Parameter* AddBoolParameter(Steinberg::Vst::ParameterContainer& parameters,
                            const Steinberg::char16* title,
                            Steinberg::Vst::ParamID id,
                            bool defaultValue) {
    return parameters.addParameter(title, nullptr, 1, NormalizeBool(defaultValue),
                                   ParameterInfo::kCanAutomate, id);
}

Parameter* AddRangeParameter(Steinberg::Vst::ParameterContainer& parameters,
                             const Steinberg::char16* title,
                             Steinberg::Vst::ParamID id,
                             int minValue,
                             int maxValue,
                             int defaultValue) {
    return parameters.addParameter(new RangeParameter(
        title, id, nullptr, static_cast<Steinberg::Vst::ParamValue>(minValue),
        static_cast<Steinberg::Vst::ParamValue>(maxValue),
        static_cast<Steinberg::Vst::ParamValue>(defaultValue), maxValue - minValue,
        ParameterInfo::kCanAutomate));
}

double ReadNormalizedValue(const Steinberg::Vst::ParameterContainer& parameters,
                           Steinberg::Vst::ParamID id,
                           double fallback) {
    if (auto* parameter = parameters.getParameter(id)) {
        return parameter->getNormalized();
    }

    return fallback;
}

} // namespace

Steinberg::tresult PLUGIN_API SysExToolController::initialize(Steinberg::FUnknown* context) {
    const Steinberg::tresult result = EditController::initialize(context);
    if (result != Steinberg::kResultTrue) {
        return result;
    }

    AddBoolParameter(parameters, STR16("Send Trigger"), kSendId, false);

    auto* messageType = new StringListParameter(STR16("Message Type"), kMessageTypeId);
    messageType->appendString(STR16("Allocate Part"));
    messageType->appendString(STR16("Shutdown Part"));
    messageType->appendString(STR16("Start Song"));
    messageType->appendString(STR16("Parameter Adjust"));
    messageType->appendString(STR16("Hook Jump"));
    messageType->appendString(STR16("Hook Global Transpose"));
    messageType->appendString(STR16("Hook Part On/Off"));
    messageType->appendString(STR16("Hook Set Volume"));
    messageType->appendString(STR16("Hook Set Program"));
    messageType->appendString(STR16("Hook Set Transpose"));
    messageType->appendString(STR16("Marker"));
    messageType->appendString(STR16("Set Loop"));
    messageType->appendString(STR16("Clear Loop"));
    messageType->appendString(STR16("Set Instrument"));
    parameters.addParameter(messageType);

    AddRangeParameter(parameters, STR16("Part"), kPartId, 0, 15, 0);
    AddRangeParameter(parameters, STR16("Channel"), kChannelId, 0, 15, 0);
    AddRangeParameter(parameters, STR16("Unknown"), kUnknownId, 0, 127, 0);
    AddBoolParameter(parameters, STR16("Part On"), kPartOnId, true);
    AddBoolParameter(parameters, STR16("Reverb"), kReverbId, false);
    AddRangeParameter(parameters, STR16("Priority"), kPriorityId, 0, 255, 90);
    AddRangeParameter(parameters, STR16("Volume"), kVolumeId, 0, 127, 127);
    AddRangeParameter(parameters, STR16("Pan"), kPanId, -64, 63, 0);
    AddBoolParameter(parameters, STR16("Percussion"), kPercussionId, false);
    AddRangeParameter(parameters, STR16("Transpose"), kTransposeId, -127, 127, 0);
    AddRangeParameter(parameters, STR16("Detune"), kDetuneId, -128, 127, 0);
    AddRangeParameter(parameters, STR16("Pitchbend Factor"), kPitchbendFactorId, 0, 255, 2);
    AddRangeParameter(parameters, STR16("Program"), kProgramId, 0, 127, 0);
    AddRangeParameter(parameters, STR16("Parameter Number"), kParamId, 0, 65535, 0);
    AddRangeParameter(parameters, STR16("Parameter Value"), kParamValueId, 0, 65535, 0);
    AddRangeParameter(parameters, STR16("Hook Command"), kHookCommandId, 0, 255, 0);
    AddRangeParameter(parameters, STR16("Target Track"), kTargetTrackId, 0, 65535, 0);
    AddRangeParameter(parameters, STR16("Target Beat"), kTargetBeatId, 0, 65535, 0);
    AddRangeParameter(parameters, STR16("Target Tick"), kTargetTickId, 0, 65535, 0);
    AddBoolParameter(parameters, STR16("Relative"), kRelativeId, false);
    AddRangeParameter(parameters, STR16("Hook Value"), kHookValueId, -128, 255, 0);
    AddRangeParameter(parameters, STR16("Marker Value"), kMarkerValueId, 0, 127, 0);
    AddRangeParameter(parameters, STR16("Loop Count"), kLoopCountId, 0, 65535, 0);
    AddRangeParameter(parameters, STR16("Loop To Beat"), kLoopToBeatId, 0, 65535, 0);
    AddRangeParameter(parameters, STR16("Loop To Tick"), kLoopToTickId, 0, 65535, 0);
    AddRangeParameter(parameters, STR16("Loop From Beat"), kLoopFromBeatId, 0, 65535, 0);
    AddRangeParameter(parameters, STR16("Loop From Tick"), kLoopFromTickId, 0, 65535, 0);
    AddRangeParameter(parameters, STR16("Instrument"), kInstrumentId, 0, 65535, 0);

    return Steinberg::kResultTrue;
}

Steinberg::IPlugView* PLUGIN_API SysExToolController::createView(Steinberg::FIDString name) {
    if (name && std::strcmp(name, Steinberg::Vst::ViewType::kEditor) == 0) {
        return new SysExToolEditorView(*this);
    }

    return nullptr;
}

Steinberg::tresult PLUGIN_API SysExToolController::setComponentState(Steinberg::IBStream* state) {
    SysExToolState toolState;
    if (!ReadState(state, &toolState)) {
        return Steinberg::kResultFalse;
    }

    ApplyState(toolState);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API SysExToolController::setState(Steinberg::IBStream* state) {
    SysExToolState toolState;
    if (!ReadState(state, &toolState)) {
        return Steinberg::kResultFalse;
    }

    ApplyState(toolState);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API SysExToolController::getState(Steinberg::IBStream* state) {
    return WriteState(state, CaptureState()) ? Steinberg::kResultOk : Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API SysExToolController::setParamNormalized(
    Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value) {
    const Steinberg::tresult result = EditController::setParamNormalized(tag, value);
    if (result == Steinberg::kResultTrue) {
        NotifyParameterChanged(tag, value);
    }

    return result;
}

SysExToolState SysExToolController::GetCurrentState() const {
    return CaptureState();
}

int SysExToolController::GetMidiOutDeviceIndex() const {
    return midiOutDeviceIndex_;
}

void SysExToolController::SetMidiOutDeviceIndex(int deviceIndex) {
    midiOutDeviceIndex_ = deviceIndex;
}

void SysExToolController::SendTriggerRequest(const SysExToolState& state) {
    Steinberg::Vst::IMessage* message = allocateMessage();
    if (!message) {
        return;
    }

    message->setMessageID(kSendRequestMessageId);
    if (Steinberg::Vst::IAttributeList* attributes = message->getAttributes()) {
        attributes->setBinary(kSendRequestStateKey, &state, static_cast<Steinberg::uint32>(sizeof(state)));
        sendMessage(message);
    }
    message->release();
}

void SysExToolController::AddParameterListener(ParameterListener* listener) {
    if (!listener) {
        return;
    }

    if (std::find(parameterListeners_.begin(), parameterListeners_.end(), listener) ==
        parameterListeners_.end()) {
        parameterListeners_.push_back(listener);
    }
}

void SysExToolController::RemoveParameterListener(ParameterListener* listener) {
    parameterListeners_.erase(
        std::remove(parameterListeners_.begin(), parameterListeners_.end(), listener),
        parameterListeners_.end());
}

SysExToolState SysExToolController::CaptureState() const {
    SysExToolState state;
    state.messageType = static_cast<MessageType>(DenormalizeInt(
        ReadNormalizedValue(parameters, kMessageTypeId, 0.0), 0, kMessageTypeCount - 1));
    state.midiOutDeviceIndex = midiOutDeviceIndex_;
    state.part = DenormalizeInt(ReadNormalizedValue(parameters, kPartId, NormalizeInt(0, 0, 15)), 0, 15);
    state.channel = DenormalizeInt(ReadNormalizedValue(parameters, kChannelId, NormalizeInt(0, 0, 15)), 0, 15);
    state.unknown = DenormalizeInt(ReadNormalizedValue(parameters, kUnknownId, NormalizeInt(0, 0, 127)), 0, 127);
    state.partOn = DenormalizeBool(ReadNormalizedValue(parameters, kPartOnId, 1.0));
    state.reverb = DenormalizeBool(ReadNormalizedValue(parameters, kReverbId, 0.0));
    state.priority = DenormalizeInt(ReadNormalizedValue(parameters, kPriorityId, NormalizeInt(90, 0, 255)), 0, 255);
    state.volume = DenormalizeInt(ReadNormalizedValue(parameters, kVolumeId, NormalizeInt(127, 0, 127)), 0, 127);
    state.pan = DenormalizeInt(ReadNormalizedValue(parameters, kPanId, NormalizeInt(0, -64, 63)), -64, 63);
    state.percussion = DenormalizeBool(ReadNormalizedValue(parameters, kPercussionId, 0.0));
    state.transpose = DenormalizeInt(ReadNormalizedValue(parameters, kTransposeId, NormalizeInt(0, -127, 127)), -127, 127);
    state.detune = DenormalizeInt(ReadNormalizedValue(parameters, kDetuneId, NormalizeInt(0, -128, 127)), -128, 127);
    state.pitchbendFactor = DenormalizeInt(ReadNormalizedValue(parameters, kPitchbendFactorId, NormalizeInt(2, 0, 255)), 0, 255);
    state.program = DenormalizeInt(ReadNormalizedValue(parameters, kProgramId, NormalizeInt(0, 0, 127)), 0, 127);
    state.param = DenormalizeInt(ReadNormalizedValue(parameters, kParamId, 0.0), 0, 65535);
    state.paramValue = DenormalizeInt(ReadNormalizedValue(parameters, kParamValueId, 0.0), 0, 65535);
    state.hookCommand = DenormalizeInt(ReadNormalizedValue(parameters, kHookCommandId, 0.0), 0, 255);
    state.targetTrack = DenormalizeInt(ReadNormalizedValue(parameters, kTargetTrackId, 0.0), 0, 65535);
    state.targetBeat = DenormalizeInt(ReadNormalizedValue(parameters, kTargetBeatId, 0.0), 0, 65535);
    state.targetTick = DenormalizeInt(ReadNormalizedValue(parameters, kTargetTickId, 0.0), 0, 65535);
    state.relative = DenormalizeBool(ReadNormalizedValue(parameters, kRelativeId, 0.0));
    state.hookValue = DenormalizeInt(ReadNormalizedValue(parameters, kHookValueId, NormalizeInt(0, -128, 255)), -128, 255);
    state.markerValue = DenormalizeInt(ReadNormalizedValue(parameters, kMarkerValueId, 0.0), 0, 127);
    state.loopCount = DenormalizeInt(ReadNormalizedValue(parameters, kLoopCountId, 0.0), 0, 65535);
    state.loopToBeat = DenormalizeInt(ReadNormalizedValue(parameters, kLoopToBeatId, 0.0), 0, 65535);
    state.loopToTick = DenormalizeInt(ReadNormalizedValue(parameters, kLoopToTickId, 0.0), 0, 65535);
    state.loopFromBeat = DenormalizeInt(ReadNormalizedValue(parameters, kLoopFromBeatId, 0.0), 0, 65535);
    state.loopFromTick = DenormalizeInt(ReadNormalizedValue(parameters, kLoopFromTickId, 0.0), 0, 65535);
    state.instrument = DenormalizeInt(ReadNormalizedValue(parameters, kInstrumentId, 0.0), 0, 65535);
    return state;
}

void SysExToolController::ApplyState(const SysExToolState& state) {
    midiOutDeviceIndex_ = state.midiOutDeviceIndex;
    setParamNormalized(kSendId, 0.0);
    setParamNormalized(kMessageTypeId, NormalizeInt(static_cast<int>(state.messageType), 0, kMessageTypeCount - 1));
    setParamNormalized(kPartId, NormalizeInt(state.part, 0, 15));
    setParamNormalized(kChannelId, NormalizeInt(state.channel, 0, 15));
    setParamNormalized(kUnknownId, NormalizeInt(state.unknown, 0, 127));
    setParamNormalized(kPartOnId, NormalizeBool(state.partOn));
    setParamNormalized(kReverbId, NormalizeBool(state.reverb));
    setParamNormalized(kPriorityId, NormalizeInt(state.priority, 0, 255));
    setParamNormalized(kVolumeId, NormalizeInt(state.volume, 0, 127));
    setParamNormalized(kPanId, NormalizeInt(state.pan, -64, 63));
    setParamNormalized(kPercussionId, NormalizeBool(state.percussion));
    setParamNormalized(kTransposeId, NormalizeInt(state.transpose, -127, 127));
    setParamNormalized(kDetuneId, NormalizeInt(state.detune, -128, 127));
    setParamNormalized(kPitchbendFactorId, NormalizeInt(state.pitchbendFactor, 0, 255));
    setParamNormalized(kProgramId, NormalizeInt(state.program, 0, 127));
    setParamNormalized(kParamId, NormalizeInt(state.param, 0, 65535));
    setParamNormalized(kParamValueId, NormalizeInt(state.paramValue, 0, 65535));
    setParamNormalized(kHookCommandId, NormalizeInt(state.hookCommand, 0, 255));
    setParamNormalized(kTargetTrackId, NormalizeInt(state.targetTrack, 0, 65535));
    setParamNormalized(kTargetBeatId, NormalizeInt(state.targetBeat, 0, 65535));
    setParamNormalized(kTargetTickId, NormalizeInt(state.targetTick, 0, 65535));
    setParamNormalized(kRelativeId, NormalizeBool(state.relative));
    setParamNormalized(kHookValueId, NormalizeInt(state.hookValue, -128, 255));
    setParamNormalized(kMarkerValueId, NormalizeInt(state.markerValue, 0, 127));
    setParamNormalized(kLoopCountId, NormalizeInt(state.loopCount, 0, 65535));
    setParamNormalized(kLoopToBeatId, NormalizeInt(state.loopToBeat, 0, 65535));
    setParamNormalized(kLoopToTickId, NormalizeInt(state.loopToTick, 0, 65535));
    setParamNormalized(kLoopFromBeatId, NormalizeInt(state.loopFromBeat, 0, 65535));
    setParamNormalized(kLoopFromTickId, NormalizeInt(state.loopFromTick, 0, 65535));
    setParamNormalized(kInstrumentId, NormalizeInt(state.instrument, 0, 65535));
}

void SysExToolController::NotifyParameterChanged(Steinberg::Vst::ParamID id,
                                                 Steinberg::Vst::ParamValue value) {
    for (ParameterListener* listener : parameterListeners_) {
        if (listener) {
            listener->OnControllerParameterChanged(id, value);
        }
    }
}

} // namespace imwrap::vst3tool
