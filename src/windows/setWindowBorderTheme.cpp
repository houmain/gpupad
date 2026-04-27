
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

enum DWMWINDOWATTRIBUTE : DWORD {
    DWMWA_USE_IMMERSIVE_DARK_MODE = 20,
    DWMWA_WINDOW_CORNER_PREFERENCE = 33,
};

enum DWM_WINDOW_CORNER_PREFERENCE : DWORD {
    DWMWCP_DEFAULT = 0,
    DWMWCP_DONOTROUND = 1,
    DWMWCP_ROUND = 2,
    DWMWCP_ROUNDSMALL = 3
};

void setWindowBorderTheme(void *hwnd, bool dark)
{
    if (auto dwmapi = LoadLibraryW(L"dwmapi.dll")) {
        using DwmSetWindowAttribute_t =
            HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
        if (auto DwmSetWindowAttribute =
                reinterpret_cast<DwmSetWindowAttribute_t>(
                    GetProcAddress(dwmapi, "DwmSetWindowAttribute"))) {
            const auto useDark = BOOL(dark ? TRUE : FALSE);

            DwmSetWindowAttribute(static_cast<HWND>(hwnd),
                DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

            auto corner = DWMWCP_DONOTROUND;
            DwmSetWindowAttribute(static_cast<HWND>(hwnd),
                DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        }
        FreeLibrary(dwmapi);
    }
}