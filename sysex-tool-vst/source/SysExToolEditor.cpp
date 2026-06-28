#include "SysExToolEditor.h"

#include "SysExToolIds.h"

#include "pluginterfaces/gui/iplugview.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cwctype>
#include <cwchar>
#include <mutex>
#include <string>
#include <vector>

namespace imwrap::vst3tool {
namespace {

constexpr wchar_t kEditorWindowClassName[] = L"IMWrapSysExToolEditor";
constexpr UINT kSyncStateMessage = WM_APP + 17;

constexpr int kMessageTypeLabelId = 2000;
constexpr int kMessageTypeComboId = 2001;
constexpr int kSendButtonId = 2002;
constexpr int kGeneratedHexDisplayId = 2003;
constexpr int kMidiOutLabelId = 2004;
constexpr int kMidiOutComboId = 2005;
constexpr int kMidiOutRefreshButtonId = 2006;

constexpr int kPartEditId = 2100;
constexpr int kChannelEditId = 2101;
constexpr int kUnknownEditId = 2102;
constexpr int kPartOnCheckId = 2103;
constexpr int kReverbCheckId = 2104;
constexpr int kPriorityEditId = 2105;
constexpr int kVolumeEditId = 2106;
constexpr int kPanEditId = 2107;
constexpr int kPercussionCheckId = 2108;
constexpr int kTransposeEditId = 2109;
constexpr int kDetuneEditId = 2110;
constexpr int kPitchbendEditId = 2111;
constexpr int kProgramEditId = 2112;
constexpr int kParamEditId = 2113;
constexpr int kParamValueEditId = 2114;
constexpr int kHookCommandEditId = 2115;
constexpr int kTargetTrackEditId = 2116;
constexpr int kTargetBeatEditId = 2117;
constexpr int kTargetTickEditId = 2118;
constexpr int kRelativeCheckId = 2119;
constexpr int kHookValueEditId = 2120;
constexpr int kMarkerValueEditId = 2127;
constexpr int kLoopCountEditId = 2121;
constexpr int kLoopToBeatEditId = 2122;
constexpr int kLoopToTickEditId = 2123;
constexpr int kLoopFromBeatEditId = 2124;
constexpr int kLoopFromTickEditId = 2125;
constexpr int kInstrumentEditId = 2126;

constexpr std::array<const wchar_t*, kMessageTypeCount> kMessageTypeNames = {
    L"Allocate Part (0x00)",
    L"Shutdown Part (0x01)",
    L"Start Song (0x02)",
    L"Parameter Adjust (0x21)",
    L"Hook Jump (0x30)",
    L"Hook Global Transpose (0x31)",
    L"Hook Part On/Off (0x32)",
    L"Hook Set Volume (0x33)",
    L"Hook Set Program (0x34)",
    L"Hook Set Transpose (0x35)",
    L"Marker (0x40)",
    L"Set Loop (0x50)",
    L"Clear Loop (0x51)",
    L"Set Instrument (0x60)",
};

Steinberg::ViewRect MakeDefaultEditorRect() {
    return Steinberg::ViewRect(0, 0, 900, 560);
}

bool IsSignedHookValueType(MessageType type) {
    return type == MessageType::HookGlobalTranspose || type == MessageType::HookSetTranspose;
}

int ClampValue(int value, int minValue, int maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

void SetControlFont(HWND control, HFONT font) {
    if (control && font) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

} // namespace

SysExToolEditorView::SysExToolEditorView(SysExToolController& controller)
    : Steinberg::Vst::EditorView(&controller, nullptr), controller_(controller) {
    setRect(MakeDefaultEditorRect());
#ifdef _WIN32
    controller_.AddParameterListener(this);
#endif
}

SysExToolEditorView::~SysExToolEditorView() {
#ifdef _WIN32
    controller_.RemoveParameterListener(this);
    if (monoFont_ && monoFont_ != uiFont_) {
        DeleteObject(monoFont_);
        monoFont_ = nullptr;
    }
#endif
}

Steinberg::tresult PLUGIN_API SysExToolEditorView::isPlatformTypeSupported(
    Steinberg::FIDString type) {
#ifdef _WIN32
    return (type && std::strcmp(type, Steinberg::kPlatformTypeHWND) == 0)
               ? Steinberg::kResultTrue
               : Steinberg::kResultFalse;
#else
    (void)type;
    return Steinberg::kResultFalse;
#endif
}

Steinberg::tresult PLUGIN_API SysExToolEditorView::attached(void* parent,
                                                            Steinberg::FIDString type) {
#ifndef _WIN32
    (void)parent;
    (void)type;
    return Steinberg::kResultFalse;
#else
    if (isPlatformTypeSupported(type) != Steinberg::kResultTrue) {
        return Steinberg::kResultFalse;
    }

    const Steinberg::tresult result = Steinberg::Vst::EditorView::attached(parent, type);
    if (result != Steinberg::kResultOk) {
        return result;
    }

    EnsureWindowClassRegistered();

    parentWindow_ = reinterpret_cast<HWND>(parent);
    const Steinberg::ViewRect& viewRect = getRect();
    hwnd_ = CreateWindowExW(0, kEditorWindowClassName, L"",
                            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0,
                            viewRect.getWidth(), viewRect.getHeight(), parentWindow_, nullptr,
                            GetModuleHandleW(nullptr), this);
    if (!hwnd_) {
        parentWindow_ = nullptr;
        Steinberg::Vst::EditorView::removed();
        return Steinberg::kResultFalse;
    }

    SyncFromController();
    return Steinberg::kResultOk;
#endif
}

Steinberg::tresult PLUGIN_API SysExToolEditorView::removed() {
#ifdef _WIN32
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    parentWindow_ = nullptr;
#endif
    return Steinberg::Vst::EditorView::removed();
}

Steinberg::tresult PLUGIN_API SysExToolEditorView::onSize(Steinberg::ViewRect* newSize) {
    const Steinberg::tresult result = Steinberg::Vst::EditorView::onSize(newSize);
#ifdef _WIN32
    if (result == Steinberg::kResultTrue && hwnd_) {
        const Steinberg::ViewRect& viewRect = getRect();
        SetWindowPos(hwnd_, nullptr, 0, 0, viewRect.getWidth(), viewRect.getHeight(),
                     SWP_NOZORDER | SWP_NOACTIVATE);
        LayoutControls(viewRect.getWidth(), viewRect.getHeight());
    }
#endif
    return result;
}

void SysExToolEditorView::OnControllerParameterChanged(Steinberg::Vst::ParamID,
                                                       Steinberg::Vst::ParamValue) {
#ifdef _WIN32
    if (hwnd_ && IsWindow(hwnd_)) {
        PostMessageW(hwnd_, kSyncStateMessage, 0, 0);
    }
#endif
}

#ifdef _WIN32

LRESULT CALLBACK SysExToolEditorView::WindowProc(HWND hwnd, UINT message, WPARAM wParam,
                                                 LPARAM lParam) {
    SysExToolEditorView* view = nullptr;
    if (message == WM_NCCREATE) {
        const auto* createStruct = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        view = static_cast<SysExToolEditorView*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        if (view) {
            view->hwnd_ = hwnd;
        }
    } else {
        view = reinterpret_cast<SysExToolEditorView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (view) {
        return view->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void SysExToolEditorView::EnsureWindowClassRegistered() {
    static std::once_flag registered;
    std::call_once(registered, [] {
        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = &SysExToolEditorView::WindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        windowClass.lpszClassName = kEditorWindowClassName;
        RegisterClassExW(&windowClass);
    });
}

LRESULT SysExToolEditorView::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateControls();
        return 0;

    case WM_SIZE:
        LayoutControls(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_COMMAND: {
        const int controlId = LOWORD(wParam);
        const int notification = HIWORD(wParam);

        if (controlId == kMessageTypeComboId && notification == CBN_SELCHANGE) {
            if (!isSyncing_) {
                const int selection =
                    static_cast<int>(SendMessageW(messageTypeCombo_, CB_GETCURSEL, 0, 0));
                if (selection >= 0 && selection < kMessageTypeCount) {
                    CommitParameter(kMessageTypeId, NormalizeInt(selection, 0, kMessageTypeCount - 1));
                    UpdateFieldVisibility();
                    UpdateGeneratedHex();
                }
            }
            return 0;
        }

        if (controlId == kSendButtonId && notification == BN_CLICKED) {
            TriggerSend();
            return 0;
        }

        if (controlId == kMidiOutComboId && notification == CBN_SELCHANGE) {
            if (!isSyncing_) {
                ApplySelectedMidiOutDevice();
            }
            return 0;
        }

        if (controlId == kMidiOutRefreshButtonId && notification == BN_CLICKED) {
            RefreshMidiOutDevices();
            return 0;
        }

        if (CheckboxField* field = FindCheckboxFieldByControlId(controlId)) {
            if (notification == BN_CLICKED && !isSyncing_) {
                const bool checked =
                    SendMessageW(field->button, BM_GETCHECK, 0, 0) == BST_CHECKED;
                CommitParameter(field->paramId, NormalizeBool(checked));
                UpdateGeneratedHex();
            }
            return 0;
        }

        if (NumericField* field = FindNumericFieldByControlId(controlId)) {
            if (!isSyncing_ && (notification == EN_CHANGE || notification == EN_KILLFOCUS)) {
                CommitNumericField(*field, notification == EN_KILLFOCUS);
                UpdateGeneratedHex();
            }
            return 0;
        }
        break;
    }

    case kSyncStateMessage:
        SyncFromController();
        return 0;

    case WM_SETFOCUS:
        if (messageTypeCombo_) {
            SetFocus(messageTypeCombo_);
        }
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

void SysExToolEditorView::CreateControls() {
    uiFont_ = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    if (!monoFont_) {
        monoFont_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                FIXED_PITCH | FF_MODERN, L"Consolas");
        if (!monoFont_) {
            monoFont_ = uiFont_;
        }
    }

    messageTypeLabel_ = CreateWindowExW(0, L"STATIC", L"Message Type:", WS_CHILD | WS_VISIBLE, 0, 0,
                                        0, 0, hwnd_,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMessageTypeLabelId)),
                                        GetModuleHandleW(nullptr), nullptr);
    messageTypeCombo_ = CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0, 0, 0, 240, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMessageTypeComboId)),
        GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* item : kMessageTypeNames) {
        SendMessageW(messageTypeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    }

    partField_ = CreateNumericField(kPartEditId, L"Part:", kPartId, 0, 15);
    channelField_ = CreateNumericField(kChannelEditId, L"Channel:", kChannelId, 0, 15);
    unknownField_ = CreateNumericField(kUnknownEditId, L"Unknown:", kUnknownId, 0, 127);
    partOnField_ = CreateCheckboxField(kPartOnCheckId, L"Part On", kPartOnId);
    reverbField_ = CreateCheckboxField(kReverbCheckId, L"Reverb", kReverbId);
    priorityField_ = CreateNumericField(kPriorityEditId, L"Priority:", kPriorityId, 0, 255);
    volumeField_ = CreateNumericField(kVolumeEditId, L"Volume:", kVolumeId, 0, 127);
    panField_ = CreateNumericField(kPanEditId, L"Pan (-64..63):", kPanId, -64, 63);
    percussionField_ = CreateCheckboxField(kPercussionCheckId, L"Percussion", kPercussionId);
    transposeField_ =
        CreateNumericField(kTransposeEditId, L"Transpose:", kTransposeId, -127, 127);
    detuneField_ = CreateNumericField(kDetuneEditId, L"Detune:", kDetuneId, -128, 127);
    pitchbendField_ =
        CreateNumericField(kPitchbendEditId, L"Pitchbend Factor:", kPitchbendFactorId, 0, 255);
    programField_ = CreateNumericField(kProgramEditId, L"Program:", kProgramId, 0, 127);
    paramField_ = CreateNumericField(kParamEditId, L"Parameter Number:", kParamId, 0, 65535);
    paramValueField_ =
        CreateNumericField(kParamValueEditId, L"Parameter Value:", kParamValueId, 0, 65535);
    hookCommandField_ =
        CreateNumericField(kHookCommandEditId, L"Hook Command:", kHookCommandId, 0, 255);
    targetTrackField_ =
        CreateNumericField(kTargetTrackEditId, L"Target Track:", kTargetTrackId, 0, 65535);
    targetBeatField_ =
        CreateNumericField(kTargetBeatEditId, L"Target Beat:", kTargetBeatId, 0, 65535);
    targetTickField_ =
        CreateNumericField(kTargetTickEditId, L"Target Tick:", kTargetTickId, 0, 65535);
    relativeField_ = CreateCheckboxField(kRelativeCheckId, L"Relative", kRelativeId);
    hookValueField_ =
        CreateNumericField(kHookValueEditId, L"Hook Value:", kHookValueId, -128, 255);
    markerValueField_ =
        CreateNumericField(kMarkerValueEditId, L"Marker Value:", kMarkerValueId, 0, 127);
    loopCountField_ =
        CreateNumericField(kLoopCountEditId, L"Loop Count:", kLoopCountId, 0, 65535);
    loopToBeatField_ =
        CreateNumericField(kLoopToBeatEditId, L"Loop To Beat:", kLoopToBeatId, 0, 65535);
    loopToTickField_ =
        CreateNumericField(kLoopToTickEditId, L"Loop To Tick:", kLoopToTickId, 0, 65535);
    loopFromBeatField_ =
        CreateNumericField(kLoopFromBeatEditId, L"Loop From Beat:", kLoopFromBeatId, 0, 65535);
    loopFromTickField_ =
        CreateNumericField(kLoopFromTickEditId, L"Loop From Tick:", kLoopFromTickId, 0, 65535);
    instrumentField_ =
        CreateNumericField(kInstrumentEditId, L"Instrument ID:", kInstrumentId, 0, 65535);

    infoLabel_ = CreateWindowExW(
        0, L"STATIC",
        L"This VSTi emits an iMWrap SysEx event on its MIDI/event output when you click Send.",
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
    midiOutLabel_ = CreateWindowExW(0, L"STATIC", L"Direct Windows MIDI Out:", WS_CHILD | WS_VISIBLE,
                                    0, 0, 0, 0, hwnd_,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMidiOutLabelId)),
                                    GetModuleHandleW(nullptr), nullptr);
    midiOutCombo_ = CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0, 0, 0, 260, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMidiOutComboId)),
        GetModuleHandleW(nullptr), nullptr);
    midiOutRefreshButton_ = CreateWindowExW(
        0, L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0,
        hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMidiOutRefreshButtonId)),
        GetModuleHandleW(nullptr), nullptr);
    sendButton_ = CreateWindowExW(
        0, L"BUTTON", L"Send SysEx Event", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        0, 0, 0, 0, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSendButtonId)), GetModuleHandleW(nullptr),
        nullptr);
    generatedHexLabel_ = CreateWindowExW(0, L"STATIC", L"Generated SysEx", WS_CHILD | WS_VISIBLE,
                                         0, 0, 0, 0, hwnd_, nullptr, GetModuleHandleW(nullptr),
                                         nullptr);
    generatedHexDisplay_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 0, 0,
        0, 0, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kGeneratedHexDisplayId)),
        GetModuleHandleW(nullptr), nullptr);
    routingLabel_ = CreateWindowExW(
        0, L"STATIC",
        L"Use the host routing for this instrument track if you want to capture or forward the SysEx output.",
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);

    SetControlFont(messageTypeLabel_, uiFont_);
    SetControlFont(messageTypeCombo_, uiFont_);
    SetControlFont(infoLabel_, uiFont_);
    SetControlFont(midiOutLabel_, uiFont_);
    SetControlFont(midiOutCombo_, uiFont_);
    SetControlFont(midiOutRefreshButton_, uiFont_);
    SetControlFont(sendButton_, uiFont_);
    SetControlFont(generatedHexLabel_, uiFont_);
    SetControlFont(generatedHexDisplay_, monoFont_);
    SetControlFont(routingLabel_, uiFont_);

    RefreshMidiOutDevices();

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);
    LayoutControls(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
}

void SysExToolEditorView::LayoutControls(int width, int height) {
    if (!hwnd_) {
        return;
    }

    const int margin = 12;
    const int rowHeight = 24;
    const int rowGap = 6;
    const int labelWidth = 132;
    const bool stackedLayout = width < 760;

    const int leftColumnWidth =
        stackedLayout ? width - (margin * 2) : (width - (margin * 3)) / 2;
    const int rightColumnLeft = stackedLayout ? margin : margin + leftColumnWidth + margin;
    const int rightColumnWidth =
        stackedLayout ? width - (margin * 2) : width - rightColumnLeft - margin;

    int currentTop = margin;
    MoveWindow(messageTypeLabel_, margin, currentTop + 4, labelWidth, rowHeight, TRUE);
    MoveWindow(messageTypeCombo_, margin + labelWidth + 8, currentTop,
               std::max(120, leftColumnWidth - labelWidth - 8), 240, TRUE);
    currentTop += rowHeight + rowGap;

    const auto layoutVisibleNumeric = [&](const NumericField& field) {
        if (field.edit && IsWindowVisible(field.edit)) {
            LayoutNumericField(field, margin, currentTop, labelWidth,
                               std::max(120, leftColumnWidth - labelWidth - 8));
            currentTop += rowHeight + rowGap;
        }
    };

    const auto layoutVisibleCheckbox = [&](const CheckboxField& field) {
        if (field.button && IsWindowVisible(field.button)) {
            LayoutCheckboxField(field, margin + labelWidth + 8, currentTop,
                                std::max(120, leftColumnWidth - labelWidth - 8));
            currentTop += rowHeight + rowGap;
        }
    };

    layoutVisibleNumeric(partField_);
    layoutVisibleNumeric(channelField_);
    layoutVisibleNumeric(unknownField_);
    layoutVisibleCheckbox(partOnField_);
    layoutVisibleCheckbox(reverbField_);
    layoutVisibleNumeric(priorityField_);
    layoutVisibleNumeric(volumeField_);
    layoutVisibleNumeric(panField_);
    layoutVisibleCheckbox(percussionField_);
    layoutVisibleNumeric(transposeField_);
    layoutVisibleNumeric(detuneField_);
    layoutVisibleNumeric(pitchbendField_);
    layoutVisibleNumeric(programField_);
    layoutVisibleNumeric(paramField_);
    layoutVisibleNumeric(paramValueField_);
    layoutVisibleNumeric(hookCommandField_);
    layoutVisibleNumeric(targetTrackField_);
    layoutVisibleNumeric(targetBeatField_);
    layoutVisibleNumeric(targetTickField_);
    layoutVisibleCheckbox(relativeField_);
    layoutVisibleNumeric(hookValueField_);
    layoutVisibleNumeric(markerValueField_);
    layoutVisibleNumeric(loopCountField_);
    layoutVisibleNumeric(loopToBeatField_);
    layoutVisibleNumeric(loopToTickField_);
    layoutVisibleNumeric(loopFromBeatField_);
    layoutVisibleNumeric(loopFromTickField_);
    layoutVisibleNumeric(instrumentField_);

    const int outputTop = stackedLayout ? currentTop + margin : margin;
    const int outputHeight = std::max(180, height - outputTop - margin);

    MoveWindow(infoLabel_, rightColumnLeft, outputTop, rightColumnWidth, 42, TRUE);
    MoveWindow(midiOutLabel_, rightColumnLeft, outputTop + 48, rightColumnWidth, 20, TRUE);
    const int refreshButtonWidth = 78;
    const int comboWidth = std::max(120, rightColumnWidth - refreshButtonWidth - 8);
    MoveWindow(midiOutCombo_, rightColumnLeft, outputTop + 72, comboWidth, 240, TRUE);
    MoveWindow(midiOutRefreshButton_, rightColumnLeft + comboWidth + 8, outputTop + 72,
               refreshButtonWidth, 24, TRUE);
    MoveWindow(sendButton_, rightColumnLeft, outputTop + 106, std::min(180, rightColumnWidth), 28,
               TRUE);
    MoveWindow(generatedHexLabel_, rightColumnLeft, outputTop + 142, rightColumnWidth, 22, TRUE);

    const int routingHeight = 40;
    const int generatedTop = outputTop + 168;
    const int generatedHeight =
        std::max(90, outputHeight - (generatedTop - outputTop) - routingHeight - 8);
    MoveWindow(generatedHexDisplay_, rightColumnLeft, generatedTop, rightColumnWidth,
               generatedHeight, TRUE);
    MoveWindow(routingLabel_, rightColumnLeft, generatedTop + generatedHeight + 8, rightColumnWidth,
               routingHeight, TRUE);
}

void SysExToolEditorView::SyncFromController() {
    if (!hwnd_) {
        return;
    }

    const SysExToolState state = controller_.GetCurrentState();
    isSyncing_ = true;

    SendMessageW(messageTypeCombo_, CB_SETCURSEL,
                 static_cast<WPARAM>(static_cast<int>(state.messageType)), 0);
    if (midiOutCombo_) {
        int selection = 0;
        for (std::size_t index = 0; index < midiOutDeviceIndices_.size(); ++index) {
            if (midiOutDeviceIndices_[index] == state.midiOutDeviceIndex) {
                selection = static_cast<int>(index);
                break;
            }
        }
        SendMessageW(midiOutCombo_, CB_SETCURSEL, static_cast<WPARAM>(selection), 0);
    }

    SyncNumericField(partField_, state.part);
    SyncNumericField(channelField_, state.channel);
    SyncNumericField(unknownField_, state.unknown);
    SyncCheckboxField(partOnField_, state.partOn);
    SyncCheckboxField(reverbField_, state.reverb);
    SyncNumericField(priorityField_, state.priority);
    SyncNumericField(volumeField_, state.volume);
    SyncNumericField(panField_, state.pan);
    SyncCheckboxField(percussionField_, state.percussion);
    SyncNumericField(transposeField_, state.transpose);
    SyncNumericField(detuneField_, state.detune);
    SyncNumericField(pitchbendField_, state.pitchbendFactor);
    SyncNumericField(programField_, state.program);
    SyncNumericField(paramField_, state.param);
    SyncNumericField(paramValueField_, state.paramValue);
    SyncNumericField(hookCommandField_, state.hookCommand);
    SyncNumericField(targetTrackField_, state.targetTrack);
    SyncNumericField(targetBeatField_, state.targetBeat);
    SyncNumericField(targetTickField_, state.targetTick);
    SyncCheckboxField(relativeField_, state.relative);
    SyncNumericField(hookValueField_, state.hookValue);
    SyncNumericField(markerValueField_, state.markerValue);
    SyncNumericField(loopCountField_, state.loopCount);
    SyncNumericField(loopToBeatField_, state.loopToBeat);
    SyncNumericField(loopToTickField_, state.loopToTick);
    SyncNumericField(loopFromBeatField_, state.loopFromBeat);
    SyncNumericField(loopFromTickField_, state.loopFromTick);
    SyncNumericField(instrumentField_, state.instrument);

    isSyncing_ = false;
    ApplySelectedMidiOutDevice();
    UpdateFieldVisibility();
    UpdateGeneratedHex();
    UpdateRoutingDescription();
}

void SysExToolEditorView::UpdateFieldVisibility() {
    const MessageType type = CurrentMessageType();

    SetWindowTextW(unknownField_.label,
                   type == MessageType::AllocatePart ? L"Unknown Nibble:" : L"Unknown:");
    SetWindowTextW(hookValueField_.label,
                   IsSignedHookValueType(type) ? L"Signed Hook Value:" : L"Hook Value:");

    SetFieldVisible(partField_, false);
    SetFieldVisible(channelField_, false);
    SetFieldVisible(unknownField_, false);
    SetFieldVisible(partOnField_, false);
    SetFieldVisible(reverbField_, false);
    SetFieldVisible(priorityField_, false);
    SetFieldVisible(volumeField_, false);
    SetFieldVisible(panField_, false);
    SetFieldVisible(percussionField_, false);
    SetFieldVisible(transposeField_, false);
    SetFieldVisible(detuneField_, false);
    SetFieldVisible(pitchbendField_, false);
    SetFieldVisible(programField_, false);
    SetFieldVisible(paramField_, false);
    SetFieldVisible(paramValueField_, false);
    SetFieldVisible(hookCommandField_, false);
    SetFieldVisible(targetTrackField_, false);
    SetFieldVisible(targetBeatField_, false);
    SetFieldVisible(targetTickField_, false);
    SetFieldVisible(relativeField_, false);
    SetFieldVisible(hookValueField_, false);
    SetFieldVisible(markerValueField_, false);
    SetFieldVisible(loopCountField_, false);
    SetFieldVisible(loopToBeatField_, false);
    SetFieldVisible(loopToTickField_, false);
    SetFieldVisible(loopFromBeatField_, false);
    SetFieldVisible(loopFromTickField_, false);
    SetFieldVisible(instrumentField_, false);

    switch (type) {
    case MessageType::AllocatePart:
        SetFieldVisible(partField_, true);
        SetFieldVisible(unknownField_, true);
        SetFieldVisible(partOnField_, true);
        SetFieldVisible(reverbField_, true);
        SetFieldVisible(priorityField_, true);
        SetFieldVisible(volumeField_, true);
        SetFieldVisible(panField_, true);
        SetFieldVisible(percussionField_, true);
        SetFieldVisible(transposeField_, true);
        SetFieldVisible(detuneField_, true);
        SetFieldVisible(pitchbendField_, true);
        SetFieldVisible(programField_, true);
        break;
    case MessageType::ShutdownPart:
        SetFieldVisible(partField_, true);
        break;
    case MessageType::StartSong:
    case MessageType::ClearLoop:
        break;
    case MessageType::ParameterAdjust:
        SetFieldVisible(partField_, true);
        SetFieldVisible(unknownField_, true);
        SetFieldVisible(paramField_, true);
        SetFieldVisible(paramValueField_, true);
        break;
    case MessageType::HookJump:
        SetFieldVisible(unknownField_, true);
        SetFieldVisible(hookCommandField_, true);
        SetFieldVisible(targetTrackField_, true);
        SetFieldVisible(targetBeatField_, true);
        SetFieldVisible(targetTickField_, true);
        break;
    case MessageType::HookGlobalTranspose:
        SetFieldVisible(unknownField_, true);
        SetFieldVisible(hookCommandField_, true);
        SetFieldVisible(relativeField_, true);
        SetFieldVisible(hookValueField_, true);
        break;
    case MessageType::HookPartOnOff:
    case MessageType::HookSetVolume:
    case MessageType::HookSetProgram:
        SetFieldVisible(channelField_, true);
        SetFieldVisible(hookCommandField_, true);
        SetFieldVisible(hookValueField_, true);
        break;
    case MessageType::HookSetTranspose:
        SetFieldVisible(channelField_, true);
        SetFieldVisible(hookCommandField_, true);
        SetFieldVisible(relativeField_, true);
        SetFieldVisible(hookValueField_, true);
        break;
    case MessageType::Marker:
        SetFieldVisible(unknownField_, true);
        SetFieldVisible(markerValueField_, true);
        break;
    case MessageType::SetLoop:
        SetFieldVisible(unknownField_, true);
        SetFieldVisible(loopCountField_, true);
        SetFieldVisible(loopToBeatField_, true);
        SetFieldVisible(loopToTickField_, true);
        SetFieldVisible(loopFromBeatField_, true);
        SetFieldVisible(loopFromTickField_, true);
        break;
    case MessageType::SetInstrument:
        SetFieldVisible(channelField_, true);
        SetFieldVisible(instrumentField_, true);
        break;
    }

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);
    LayoutControls(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
}

void SysExToolEditorView::UpdateGeneratedHex() {
    if (!generatedHexDisplay_) {
        return;
    }

    const std::wstring hex = FormatSysExHex(controller_.GetCurrentState());
    SetWindowTextW(generatedHexDisplay_, hex.c_str());
}

void SysExToolEditorView::RefreshMidiOutDevices() {
    if (!midiOutCombo_) {
        return;
    }

    const int desiredDeviceIndex = controller_.GetMidiOutDeviceIndex();
    midiOutDeviceIndices_.clear();

    const bool previousSyncState = isSyncing_;
    isSyncing_ = true;
    SendMessageW(midiOutCombo_, CB_RESETCONTENT, 0, 0);
    SendMessageW(midiOutCombo_, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"Disabled (VST3 output only)"));
    midiOutDeviceIndices_.push_back(-1);

    const std::vector<std::wstring> deviceNames = directMidiOut_.ListDeviceNames();
    for (std::size_t index = 0; index < deviceNames.size(); ++index) {
        std::wstring label = std::to_wstring(index);
        label += L": ";
        label += deviceNames[index];
        SendMessageW(midiOutCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        midiOutDeviceIndices_.push_back(static_cast<int>(index));
    }

    int selection = 0;
    for (std::size_t index = 0; index < midiOutDeviceIndices_.size(); ++index) {
        if (midiOutDeviceIndices_[index] == desiredDeviceIndex) {
            selection = static_cast<int>(index);
            break;
        }
    }
    SendMessageW(midiOutCombo_, CB_SETCURSEL, static_cast<WPARAM>(selection), 0);
    isSyncing_ = previousSyncState;
    ApplySelectedMidiOutDevice();
}

void SysExToolEditorView::ApplySelectedMidiOutDevice() {
    if (!midiOutCombo_) {
        return;
    }

    const int selection = static_cast<int>(SendMessageW(midiOutCombo_, CB_GETCURSEL, 0, 0));
    int deviceIndex = -1;
    if (selection >= 0 && selection < static_cast<int>(midiOutDeviceIndices_.size())) {
        deviceIndex = midiOutDeviceIndices_[selection];
    }

    if (!directMidiOut_.OpenDevice(deviceIndex)) {
        deviceIndex = -1;
        directMidiOut_.CloseDevice();
    }

    controller_.SetMidiOutDeviceIndex(deviceIndex);
    UpdateRoutingDescription();
}

void SysExToolEditorView::UpdateRoutingDescription() {
    if (!routingLabel_) {
        return;
    }

    if (controller_.GetMidiOutDeviceIndex() >= 0) {
        SetWindowTextW(
            routingLabel_,
            L"Direct Windows MIDI Out is enabled. Route that port into a DP MIDI track to record the SysEx.");
    } else {
        SetWindowTextW(
            routingLabel_,
            L"Select a Windows MIDI or loopMIDI port above if DP should record the SysEx on a MIDI track.");
    }
}

void SysExToolEditorView::CommitParameter(Steinberg::Vst::ParamID id,
                                          Steinberg::Vst::ParamValue valueNormalized) {
    controller_.beginEdit(id);
    controller_.setParamNormalized(id, valueNormalized);
    controller_.performEdit(id, valueNormalized);
    controller_.endEdit(id);
    controller_.setDirty(true);
}

void SysExToolEditorView::TriggerSend() {
    ApplyAllPendingNumericFields(true);
    const SysExToolState state = controller_.GetCurrentState();
    std::vector<std::uint8_t> directBytes;
    if (controller_.GetMidiOutDeviceIndex() >= 0 &&
        BuildSysExBytes(state, &directBytes, nullptr)) {
        directMidiOut_.SendSysEx(directBytes);
    }
    controller_.SendTriggerRequest(state);

    sendTriggerState_ = !sendTriggerState_;
    const Steinberg::Vst::ParamValue triggerValue = sendTriggerState_ ? 1.0 : 0.0;
    controller_.beginEdit(kSendId);
    controller_.setParamNormalized(kSendId, triggerValue);
    controller_.performEdit(kSendId, triggerValue);
    controller_.endEdit(kSendId);
}

void SysExToolEditorView::ApplyAllPendingNumericFields(bool rewriteText) {
    for (NumericField* field : std::array<NumericField*, 21>{
             &partField_,        &channelField_,      &unknownField_,     &priorityField_,
             &volumeField_,      &panField_,          &transposeField_,   &detuneField_,
             &pitchbendField_,   &programField_,      &paramField_,       &paramValueField_,
             &hookCommandField_, &targetTrackField_,  &targetBeatField_,  &targetTickField_,
             &hookValueField_,   &markerValueField_,  &loopCountField_,   &loopToBeatField_,
             &loopToTickField_,
         }) {
        CommitNumericField(*field, rewriteText);
    }

    for (NumericField* field : std::array<NumericField*, 3>{&loopFromBeatField_, &loopFromTickField_,
                                                            &instrumentField_}) {
        CommitNumericField(*field, rewriteText);
    }
}

void SysExToolEditorView::CommitNumericField(const NumericField& field, bool rewriteText) {
    if (!field.edit || isSyncing_) {
        return;
    }

    int value = 0;
    if (!TryReadInteger(field.edit, &value)) {
        if (rewriteText) {
            SyncFromController();
        }
        return;
    }

    const auto [minValue, maxValue] = EffectiveRangeForField(field);
    const int clampedValue = ClampValue(value, minValue, maxValue);
    if (rewriteText || clampedValue != value) {
        const bool previousSyncState = isSyncing_;
        isSyncing_ = true;
        SetWindowTextW(field.edit, std::to_wstring(clampedValue).c_str());
        isSyncing_ = previousSyncState;
    }

    CommitParameter(field.paramId, NormalizeInt(clampedValue, minValue, maxValue));
}

void SysExToolEditorView::SyncNumericField(const NumericField& field, int value) {
    if (field.edit) {
        SetWindowTextW(field.edit, std::to_wstring(value).c_str());
    }
}

void SysExToolEditorView::SyncCheckboxField(const CheckboxField& field, bool checked) {
    if (field.button) {
        SendMessageW(field.button, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

void SysExToolEditorView::SetFieldVisible(const NumericField& field, bool visible) {
    if (field.label) {
        ShowWindow(field.label, visible ? SW_SHOW : SW_HIDE);
    }
    if (field.edit) {
        ShowWindow(field.edit, visible ? SW_SHOW : SW_HIDE);
    }
}

void SysExToolEditorView::SetFieldVisible(const CheckboxField& field, bool visible) {
    if (field.button) {
        ShowWindow(field.button, visible ? SW_SHOW : SW_HIDE);
    }
}

void SysExToolEditorView::LayoutNumericField(const NumericField& field, int left, int top,
                                             int labelWidth, int controlWidth) {
    MoveWindow(field.label, left, top + 4, labelWidth, 20, TRUE);
    MoveWindow(field.edit, left + labelWidth + 8, top, controlWidth, 24, TRUE);
}

void SysExToolEditorView::LayoutCheckboxField(const CheckboxField& field, int left, int top,
                                              int width) {
    MoveWindow(field.button, left, top, width, 24, TRUE);
}

SysExToolEditorView::NumericField SysExToolEditorView::CreateNumericField(
    int controlId, const wchar_t* label, Steinberg::Vst::ParamID paramId, int minValue,
    int maxValue) {
    NumericField field;
    field.paramId = paramId;
    field.minValue = minValue;
    field.maxValue = maxValue;

    field.label = CreateWindowExW(0, L"STATIC", label, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_,
                                  nullptr, GetModuleHandleW(nullptr), nullptr);
    field.edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0,
                                 hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                 GetModuleHandleW(nullptr), nullptr);

    SetControlFont(field.label, uiFont_);
    SetControlFont(field.edit, uiFont_);
    return field;
}

SysExToolEditorView::CheckboxField SysExToolEditorView::CreateCheckboxField(
    int controlId, const wchar_t* text, Steinberg::Vst::ParamID paramId) {
    CheckboxField field;
    field.paramId = paramId;
    field.button = CreateWindowExW(0, L"BUTTON", text,
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0,
                                   0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
                                   GetModuleHandleW(nullptr), nullptr);
    SetControlFont(field.button, uiFont_);
    return field;
}

SysExToolEditorView::NumericField* SysExToolEditorView::FindNumericFieldByControlId(
    int controlId) {
    for (NumericField* field : std::array<NumericField*, 24>{
             &partField_,        &channelField_,       &unknownField_,      &priorityField_,
             &volumeField_,      &panField_,           &transposeField_,    &detuneField_,
             &pitchbendField_,   &programField_,       &paramField_,        &paramValueField_,
             &hookCommandField_, &targetTrackField_,   &targetBeatField_,   &targetTickField_,
             &hookValueField_,   &markerValueField_,   &loopCountField_,    &loopToBeatField_,
             &loopToTickField_,  &loopFromBeatField_,  &loopFromTickField_, &instrumentField_,
         }) {
        if (field->edit && GetDlgCtrlID(field->edit) == controlId) {
            return field;
        }
    }

    return nullptr;
}

SysExToolEditorView::CheckboxField* SysExToolEditorView::FindCheckboxFieldByControlId(
    int controlId) {
    for (CheckboxField* field :
         std::array<CheckboxField*, 4>{&partOnField_, &reverbField_, &percussionField_,
                                       &relativeField_}) {
        if (field->button && GetDlgCtrlID(field->button) == controlId) {
            return field;
        }
    }

    return nullptr;
}

MessageType SysExToolEditorView::CurrentMessageType() const {
    if (!messageTypeCombo_) {
        return controller_.GetCurrentState().messageType;
    }

    const int selection = static_cast<int>(SendMessageW(messageTypeCombo_, CB_GETCURSEL, 0, 0));
    if (selection < 0 || selection >= kMessageTypeCount) {
        return controller_.GetCurrentState().messageType;
    }

    return static_cast<MessageType>(selection);
}

std::pair<int, int> SysExToolEditorView::EffectiveRangeForField(const NumericField& field) const {
    if (field.paramId == kUnknownId && CurrentMessageType() == MessageType::AllocatePart) {
        return {0, 15};
    }

    if (field.paramId == kHookValueId && IsSignedHookValueType(CurrentMessageType())) {
        return {-128, 127};
    }

    return {field.minValue, field.maxValue};
}

bool SysExToolEditorView::TryReadInteger(HWND edit, int* value) const {
    if (!edit || !value) {
        return false;
    }

    const int textLength = GetWindowTextLengthW(edit);
    if (textLength <= 0) {
        return false;
    }

    std::vector<wchar_t> buffer(static_cast<std::size_t>(textLength) + 1, L'\0');
    GetWindowTextW(edit, buffer.data(), textLength + 1);

    wchar_t* end = nullptr;
    const long parsedValue = std::wcstol(buffer.data(), &end, 10);
    if (end == buffer.data()) {
        return false;
    }

    while (*end != L'\0') {
        if (!iswspace(*end)) {
            return false;
        }
        ++end;
    }

    *value = static_cast<int>(parsedValue);
    return true;
}

std::wstring SysExToolEditorView::FormatSysExHex(const SysExToolState& state) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!BuildSysExBytes(state, &bytes, &error)) {
        return L"Error: " + std::wstring(error.begin(), error.end());
    }

    std::wstring text = L"F0";
    wchar_t chunk[8] = {};
    for (std::uint8_t byte : bytes) {
        std::swprintf(chunk, sizeof(chunk) / sizeof(chunk[0]), L" %02X", byte);
        text += chunk;
    }
    text += L" F7";
    return text;
}

#endif

} // namespace imwrap::vst3tool
