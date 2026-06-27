#pragma once

#include "SysExToolModel.h"

#include "public.sdk/source/vst/vsteditcontroller.h"

#include <vector>

namespace imwrap::vst3tool {

class SysExToolController final : public Steinberg::Vst::EditController {
public:
    class ParameterListener {
    public:
        virtual ~ParameterListener() = default;
        virtual void OnControllerParameterChanged(Steinberg::Vst::ParamID id,
                                                  Steinberg::Vst::ParamValue value) = 0;
    };

    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IEditController*>(new SysExToolController());
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag,
                                                     Steinberg::Vst::ParamValue value)
        SMTG_OVERRIDE;

    SysExToolState GetCurrentState() const;
    int GetMidiOutDeviceIndex() const;
    void SetMidiOutDeviceIndex(int deviceIndex);
    void SendTriggerRequest(const SysExToolState& state);
    void AddParameterListener(ParameterListener* listener);
    void RemoveParameterListener(ParameterListener* listener);

private:
    SysExToolState CaptureState() const;
    void ApplyState(const SysExToolState& state);
    void NotifyParameterChanged(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue value);

    int midiOutDeviceIndex_ = -1;
    std::vector<ParameterListener*> parameterListeners_;
};

} // namespace imwrap::vst3tool
