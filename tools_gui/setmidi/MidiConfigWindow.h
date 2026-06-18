#ifndef MIDICONFIGWINDOW_H
#define MIDICONFIGWINDOW_H

#include <windows.h>

#include <string>

struct AppStrings {
    std::wstring windowTitle;
    std::wstring driverLabel;
    std::wstring deviceLabel;
    std::wstring saveButton;
    std::wstring cancelButton;
    std::wstring driverFluid;
    std::wstring driverAdLib;
    std::wstring driverHardwareGm;
    std::wstring driverHardwareMt32;
    std::wstring noDevice;
    std::wstring targetLabel;
    std::wstring saveError;
};

class MidiConfigWindow {
public:
    explicit MidiConfigWindow(HINSTANCE instance);

    int Run(int showCommand);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass();
    bool CreateMainWindow(int showCommand);
    void CreateControls();
    void PopulateMidiDevices();
    void LoadConfig();
    void SaveConfig();
    void UpdateDeviceControls();

    std::wstring GetTargetConfigPath() const;
    AppStrings GetStrings() const;
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND driverLabel_ = nullptr;
    HWND driverCombo_ = nullptr;
    HWND deviceLabel_ = nullptr;
    HWND deviceCombo_ = nullptr;
    HWND targetLabel_ = nullptr;
    HWND saveButton_ = nullptr;
    HWND cancelButton_ = nullptr;

    AppStrings strings_;
    std::wstring configFilePath_;
};

#endif // MIDICONFIGWINDOW_H
