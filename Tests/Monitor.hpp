// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#pragma once
#include <windows.h>
namespace fbtest
{
inline bool secondary75(MONITORINFOEXW &result)
{
    result = {};
    EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
            MONITORINFOEXW info{};
            info.cbSize = sizeof(info);
            if (!GetMonitorInfoW(monitor, &info) || (info.dwFlags & MONITORINFOF_PRIMARY))
                return TRUE;
            DEVMODEW active{};
            active.dmSize = sizeof(active);
            if (!EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &active) ||
                active.dmDisplayFrequency > 76)
                return TRUE;
            DEVMODEW mode{};
            mode.dmSize = sizeof(mode);
            for (DWORD i = 0; EnumDisplaySettingsW(info.szDevice, i, &mode); i++)
            {
                if (mode.dmPelsWidth == active.dmPelsWidth && mode.dmPelsHeight == active.dmPelsHeight &&
                    mode.dmDisplayFrequency >= 74 && mode.dmDisplayFrequency <= 75)
                {
                    *reinterpret_cast<MONITORINFOEXW *>(data) = info;
                    return FALSE;
                }
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&result));
    return result.cbSize != 0;
}
} // namespace fbtest
