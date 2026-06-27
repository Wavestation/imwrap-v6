#pragma once

#include "SysExToolController.h"
#include "WinMidiOut.h"

#include "public.sdk/source/vst/vsteditcontroller.h"

#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace imwrap::vst3tool {

class SysExToolEditorView final : public Steinberg::Vst::EditorView,
                                  public SysExToolController::ParameterListener {
public:
    explicit SysExToolEditorView(SysExToolController& controller);
    ~SysExToolEditorView() override;

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API removed() SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) SMTG_OVERRIDE;

    void OnControllerParameterChanged(Steinberg::Vst::ParamID id,
                                      Steinberg::Vst::ParamValue value) override;

private:
#ifdef _WIN32
    struct NumericField {
        HWND label = nullptr;
        HWND edit = nullptr;
        Steinberg::Vst::ParamID paramId = 0;
        int minValue = 0;
        int maxValue = 0;
    };

    struct CheckboxField {
        HWND button = nullptr;
        Steinberg::Vst::ParamID paramId = 0;
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void EnsureWindowClassRegistered();

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void LayoutControls(int width, int height);
    void SyncFromController();
    void UpdateFieldVisibility();
    void UpdateGeneratedHex();
    void RefreshMidiOutDevices();
    void ApplySelectedMidiOutDevice();
    void UpdateRoutingDescription();
    void CommitParameter(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue valueNormalized);
    void TriggerSend();
    void ApplyAllPendingNumericFields(bool rewriteText);
    void CommitNumericField(const NumericField& field, bool rewriteText);
    void SyncNumericField(const NumericField& field, int value);
    void SyncCheckboxField(const CheckboxField& field, bool checked);
    void SetFieldVisible(const NumericField& field, bool visible);
    void SetFieldVisible(const CheckboxField& field, bool visible);
    void LayoutNumericField(const NumericField& field, int left, int top, int labelWidth,
                            int controlWidth);
    void LayoutCheckboxField(const CheckboxField& field, int left, int top, int width);
    NumericField CreateNumericField(int controlId, const wchar_t* label,
                                    Steinberg::Vst::ParamID paramId, int minValue, int maxValue);
    CheckboxField CreateCheckboxField(int controlId, const wchar_t* text,
                                      Steinberg::Vst::ParamID paramId);
    NumericField* FindNumericFieldByControlId(int controlId);
    CheckboxField* FindCheckboxFieldByControlId(int controlId);
    MessageType CurrentMessageType() const;
    std::pair<int, int> EffectiveRangeForField(const NumericField& field) const;
    bool TryReadInteger(HWND edit, int* value) const;
    static std::wstring FormatSysExHex(const SysExToolState& state);

    HWND hwnd_ = nullptr;
    HWND parentWindow_ = nullptr;
    HFONT uiFont_ = nullptr;
    HFONT monoFont_ = nullptr;
    bool isSyncing_ = false;
    bool sendTriggerState_ = false;

    HWND messageTypeLabel_ = nullptr;
    HWND messageTypeCombo_ = nullptr;
    HWND infoLabel_ = nullptr;
    HWND midiOutLabel_ = nullptr;
    HWND midiOutCombo_ = nullptr;
    HWND midiOutRefreshButton_ = nullptr;
    HWND generatedHexLabel_ = nullptr;
    HWND generatedHexDisplay_ = nullptr;
    HWND routingLabel_ = nullptr;
    HWND sendButton_ = nullptr;
    std::vector<int> midiOutDeviceIndices_;
    WinMidiOut directMidiOut_;

    NumericField partField_{};
    NumericField channelField_{};
    NumericField unknownField_{};
    CheckboxField partOnField_{};
    CheckboxField reverbField_{};
    NumericField priorityField_{};
    NumericField volumeField_{};
    NumericField panField_{};
    CheckboxField percussionField_{};
    NumericField transposeField_{};
    NumericField detuneField_{};
    NumericField pitchbendField_{};
    NumericField programField_{};
    NumericField paramField_{};
    NumericField paramValueField_{};
    NumericField hookCommandField_{};
    NumericField targetTrackField_{};
    NumericField targetBeatField_{};
    NumericField targetTickField_{};
    CheckboxField relativeField_{};
    NumericField hookValueField_{};
    NumericField loopCountField_{};
    NumericField loopToBeatField_{};
    NumericField loopToTickField_{};
    NumericField loopFromBeatField_{};
    NumericField loopFromTickField_{};
    NumericField instrumentField_{};
#endif

    SysExToolController& controller_;
};

} // namespace imwrap::vst3tool
