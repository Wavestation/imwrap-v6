#include "MidiConfigWindow.h"

#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <string_view>
#include <vector>

namespace {

constexpr wchar_t kWindowClassName[] = L"IMWrapSetMidiWindow";
constexpr int kWindowWidth = 520;
constexpr int kWindowHeight = 250;

constexpr int kDriverLabelId = 1001;
constexpr int kDriverComboId = 1002;
constexpr int kDeviceLabelId = 1003;
constexpr int kDeviceComboId = 1004;
constexpr int kTargetLabelId = 1005;
constexpr int kSaveButtonId = 1006;
constexpr int kCancelButtonId = 1007;

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool EndsWith(std::wstring_view value, std::wstring_view suffix) {
    return value.size() >= suffix.size() &&
        value.substr(value.size() - suffix.size()) == suffix;
}

bool IsBundledUtilityExecutable(const std::filesystem::path &path) {
    std::wstring baseName = ToLower(path.stem().wstring());
    if (EndsWith(baseName, L"-x32") || EndsWith(baseName, L"-x64")) {
        baseName.erase(baseName.size() - 4);
    }

    static const std::array<std::wstring_view, 6> bundledBaseNames = {
        L"setmidi",
        L"imwrap_sysex_gui",
        L"imwrap_player_gui",
        L"imwrap_packer_gui",
        L"imwrappack",
        L"imsprobe"
    };

    return std::find(bundledBaseNames.begin(), bundledBaseNames.end(), baseName) != bundledBaseNames.end();
}

int FindComboIndexByItemData(HWND comboBox, LPARAM value) {
    const LRESULT count = SendMessageW(comboBox, CB_GETCOUNT, 0, 0);
    for (int index = 0; index < static_cast<int>(count); ++index) {
        if (SendMessageW(comboBox, CB_GETITEMDATA, static_cast<WPARAM>(index), 0) == value) {
            return index;
        }
    }
    return -1;
}

int FindComboIndexByText(HWND comboBox, const std::wstring &text) {
    const LRESULT count = SendMessageW(comboBox, CB_GETCOUNT, 0, 0);
    std::vector<wchar_t> buffer(512, L'\0');

    for (int index = 0; index < static_cast<int>(count); ++index) {
        const LRESULT length = SendMessageW(comboBox, CB_GETLBTEXTLEN, static_cast<WPARAM>(index), 0);
        if (length < 0) {
            continue;
        }

        buffer.assign(static_cast<size_t>(length) + 1U, L'\0');
        SendMessageW(comboBox, CB_GETLBTEXT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(buffer.data()));
        if (text == buffer.data()) {
            return index;
        }
    }

    return -1;
}

std::wstring GetSelectedComboText(HWND comboBox) {
    const LRESULT selection = SendMessageW(comboBox, CB_GETCURSEL, 0, 0);
    if (selection < 0) {
        return L"";
    }

    const LRESULT length = SendMessageW(comboBox, CB_GETLBTEXTLEN, static_cast<WPARAM>(selection), 0);
    if (length < 0) {
        return L"";
    }

    std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1U, L'\0');
    SendMessageW(comboBox, CB_GETLBTEXT, static_cast<WPARAM>(selection), reinterpret_cast<LPARAM>(buffer.data()));
    return std::wstring(buffer.data());
}

} // namespace

MidiConfigWindow::MidiConfigWindow(HINSTANCE instance)
    : instance_(instance),
      strings_(GetStrings()),
      configFilePath_(GetTargetConfigPath()) {
}

int MidiConfigWindow::Run(int showCommand) {
    if (!RegisterWindowClass()) {
        return 1;
    }

    if (!CreateMainWindow(showCommand)) {
        return 1;
    }

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

AppStrings MidiConfigWindow::GetStrings() const {
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_FRENCH:
        return {
            L"Configuration MIDI iMWrap",
            L"Pilote MIDI:",
            L"Peripherique MIDI:",
            L"Enregistrer",
            L"Annuler",
            L"FluidSynth (Synthese logicielle)",
            L"AdLib (Emulation FM OPL3)",
            L"Hardware General MIDI (Port physique/virtuel)",
            L"Hardware Roland MT-32 (Port physique/virtuel)",
            L"Aucun (desactive)",
            L"Fichier de configuration cible:",
            L"Impossible d'enregistrer le fichier de configuration."
        };
    case LANG_SPANISH:
        return {
            L"Configuracion MIDI iMWrap",
            L"Controlador MIDI:",
            L"Dispositivo MIDI:",
            L"Guardar",
            L"Cancelar",
            L"FluidSynth (Sintetizador por software)",
            L"AdLib (Emulacion FM OPL3)",
            L"Hardware General MIDI (Puerto fisico/virtual)",
            L"Hardware Roland MT-32 (Puerto fisico/virtual)",
            L"Ninguno (desactivado)",
            L"Archivo de configuracion de destino:",
            L"No se pudo guardar el archivo de configuracion."
        };
    default:
        return {
            L"iMWrap MIDI Configurator",
            L"MIDI Driver:",
            L"MIDI Device:",
            L"Save",
            L"Cancel",
            L"FluidSynth (Software Synthesizer)",
            L"AdLib (FM OPL3 Emulation)",
            L"Hardware General MIDI (Physical/Virtual Port)",
            L"Hardware Roland MT-32 (Physical/Virtual Port)",
            L"None (Disabled)",
            L"Target configuration file:",
            L"Could not save the configuration file."
        };
    }
}

std::wstring MidiConfigWindow::GetTargetConfigPath() const {
    std::vector<wchar_t> modulePath(MAX_PATH, L'\0');
    DWORD copied = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    while (copied >= modulePath.size()) {
        modulePath.resize(modulePath.size() * 2U, L'\0');
        copied = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    }

    std::filesystem::path applicationPath(std::wstring(modulePath.data(), copied));
    std::filesystem::path applicationDir = applicationPath.parent_path();

    std::vector<std::filesystem::path> executables;
    for (const auto &entry : std::filesystem::directory_iterator(applicationDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::wstring extension = ToLower(entry.path().extension().wstring());
        if (extension == L".exe") {
            executables.push_back(entry.path());
        }
    }

    std::sort(executables.begin(), executables.end(), [](const auto &left, const auto &right) {
        return ToLower(left.filename().wstring()) < ToLower(right.filename().wstring());
    });

    for (const auto &exePath : executables) {
        if (!IsBundledUtilityExecutable(exePath)) {
            return (applicationDir / (exePath.stem().wstring() + L".imc")).wstring();
        }
    }

    return (applicationDir / L"imwrap.imc").wstring();
}

bool MidiConfigWindow::RegisterWindowClass() {
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &MidiConfigWindow::WindowProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    windowClass.hIcon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;

    return RegisterClassExW(&windowClass) != 0;
}

bool MidiConfigWindow::CreateMainWindow(int showCommand) {
    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClassName,
        strings_.windowTitle.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        instance_,
        this
    );

    if (hwnd_ == nullptr) {
        return false;
    }

    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);
    return true;
}

void MidiConfigWindow::CreateControls() {
    const HFONT defaultFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    driverLabel_ = CreateWindowExW(0, L"STATIC", strings_.driverLabel.c_str(), WS_CHILD | WS_VISIBLE,
        20, 20, 460, 20, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kDriverLabelId)), instance_, nullptr);
    driverCombo_ = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        20, 42, 460, 220, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kDriverComboId)), instance_, nullptr);

    deviceLabel_ = CreateWindowExW(0, L"STATIC", strings_.deviceLabel.c_str(), WS_CHILD | WS_VISIBLE,
        20, 78, 460, 20, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kDeviceLabelId)), instance_, nullptr);
    deviceCombo_ = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        20, 100, 460, 200, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kDeviceComboId)), instance_, nullptr);

    const std::wstring targetText = strings_.targetLabel + L" " + std::filesystem::path(configFilePath_).filename().wstring();
    targetLabel_ = CreateWindowExW(0, L"STATIC", targetText.c_str(), WS_CHILD | WS_VISIBLE,
        20, 145, 460, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTargetLabelId)), instance_, nullptr);

    cancelButton_ = CreateWindowExW(0, L"BUTTON", strings_.cancelButton.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        290, 185, 90, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCancelButtonId)), instance_, nullptr);
    saveButton_ = CreateWindowExW(0, L"BUTTON", strings_.saveButton.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        390, 185, 90, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSaveButtonId)), instance_, nullptr);

    for (HWND control : { driverLabel_, driverCombo_, deviceLabel_, deviceCombo_, targetLabel_, cancelButton_, saveButton_ }) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(defaultFont), TRUE);
    }

    const int fluidIndex = static_cast<int>(SendMessageW(driverCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(strings_.driverFluid.c_str())));
    SendMessageW(driverCombo_, CB_SETITEMDATA, static_cast<WPARAM>(fluidIndex), 0);

    const int adlibIndex = static_cast<int>(SendMessageW(driverCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(strings_.driverAdLib.c_str())));
    SendMessageW(driverCombo_, CB_SETITEMDATA, static_cast<WPARAM>(adlibIndex), 1);

    const int hardwareGmIndex = static_cast<int>(SendMessageW(driverCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(strings_.driverHardwareGm.c_str())));
    SendMessageW(driverCombo_, CB_SETITEMDATA, static_cast<WPARAM>(hardwareGmIndex), 2);

    const int hardwareMt32Index = static_cast<int>(SendMessageW(driverCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(strings_.driverHardwareMt32.c_str())));
    SendMessageW(driverCombo_, CB_SETITEMDATA, static_cast<WPARAM>(hardwareMt32Index), 3);

    SendMessageW(driverCombo_, CB_SETCURSEL, 0, 0);
}

void MidiConfigWindow::PopulateMidiDevices() {
    SendMessageW(deviceCombo_, CB_RESETCONTENT, 0, 0);
    SendMessageW(deviceCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(strings_.noDevice.c_str()));
    SendMessageW(deviceCombo_, CB_SETCURSEL, 0, 0);

    const UINT deviceCount = midiOutGetNumDevs();
    for (UINT deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
        MIDIOUTCAPSW caps = {};
        if (midiOutGetDevCapsW(deviceIndex, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            SendMessageW(deviceCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(caps.szPname));
        }
    }
}

void MidiConfigWindow::LoadConfig() {
    const UINT driverType = GetPrivateProfileIntW(L"MIDI", L"Driver", 0, configFilePath_.c_str());
    const int driverIndex = FindComboIndexByItemData(driverCombo_, static_cast<LPARAM>(driverType));
    if (driverIndex >= 0) {
        SendMessageW(driverCombo_, CB_SETCURSEL, static_cast<WPARAM>(driverIndex), 0);
    }

    std::array<wchar_t, 512> buffer = {};
    GetPrivateProfileStringW(L"MIDI", L"Device", L"", buffer.data(), static_cast<DWORD>(buffer.size()), configFilePath_.c_str());
    const std::wstring deviceName(buffer.data());
    if (!deviceName.empty()) {
        const int deviceIndex = FindComboIndexByText(deviceCombo_, deviceName);
        if (deviceIndex >= 0) {
            SendMessageW(deviceCombo_, CB_SETCURSEL, static_cast<WPARAM>(deviceIndex), 0);
        }
    }

    UpdateDeviceControls();
}

void MidiConfigWindow::SaveConfig() {
    const int selectedDriverIndex = static_cast<int>(SendMessageW(driverCombo_, CB_GETCURSEL, 0, 0));
    if (selectedDriverIndex < 0) {
        return;
    }

    const int driverType = static_cast<int>(SendMessageW(driverCombo_, CB_GETITEMDATA, static_cast<WPARAM>(selectedDriverIndex), 0));
    const std::wstring driverText = std::to_wstring(driverType);
    const BOOL driverSaved = WritePrivateProfileStringW(L"MIDI", L"Driver", driverText.c_str(), configFilePath_.c_str());

    std::wstring deviceName;
    if (driverType == 2 || driverType == 3) {
        deviceName = GetSelectedComboText(deviceCombo_);
        if (deviceName == strings_.noDevice) {
            deviceName.clear();
        }
    }

    const BOOL deviceSaved = WritePrivateProfileStringW(L"MIDI", L"Device", deviceName.c_str(), configFilePath_.c_str());
    if (!driverSaved || !deviceSaved) {
        MessageBoxW(hwnd_, strings_.saveError.c_str(), strings_.windowTitle.c_str(), MB_OK | MB_ICONERROR);
        return;
    }

    DestroyWindow(hwnd_);
}

void MidiConfigWindow::UpdateDeviceControls() {
    const int selectedDriverIndex = static_cast<int>(SendMessageW(driverCombo_, CB_GETCURSEL, 0, 0));
    const int driverType = selectedDriverIndex >= 0
        ? static_cast<int>(SendMessageW(driverCombo_, CB_GETITEMDATA, static_cast<WPARAM>(selectedDriverIndex), 0))
        : 0;

    const BOOL isHardwareDriver = (driverType == 2 || driverType == 3) ? TRUE : FALSE;
    EnableWindow(deviceLabel_, isHardwareDriver);
    EnableWindow(deviceCombo_, isHardwareDriver);
}

LRESULT CALLBACK MidiConfigWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    MidiConfigWindow *window = nullptr;

    if (message == WM_NCCREATE) {
        const auto *createStruct = reinterpret_cast<const CREATESTRUCTW *>(lParam);
        window = static_cast<MidiConfigWindow *>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
    } else {
        window = reinterpret_cast<MidiConfigWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window != nullptr) {
        return window->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT MidiConfigWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateControls();
        PopulateMidiDevices();
        LoadConfig();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kDriverComboId:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                UpdateDeviceControls();
            }
            return 0;

        case kSaveButtonId:
            if (HIWORD(wParam) == BN_CLICKED) {
                SaveConfig();
            }
            return 0;

        case kCancelButtonId:
            if (HIWORD(wParam) == BN_CLICKED) {
                DestroyWindow(hwnd_);
            }
            return 0;

        default:
            break;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd_);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}
