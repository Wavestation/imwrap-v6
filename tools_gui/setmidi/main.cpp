#include <windows.h>
#include <commctrl.h>

#include "MidiConfigWindow.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    MidiConfigWindow window(instance);
    return window.Run(showCommand);
}
