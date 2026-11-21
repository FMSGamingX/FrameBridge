// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "Platform.hpp"
#include "../Tests/Monitor.hpp"
using namespace fb;
FB int fb_test_display(wchar_t *name, int cap, RECT *bounds)
{
    MONITORINFOEXW info{};
    if (!bounds || !fbtest::secondary75(info))
        return 0;
    *bounds = info.rcMonitor;
    copyText(name, cap, info.szDevice);
    return 1;
}
FB int fb_version()
{
    return 2;
}
FB int fb_adapter(int index, wchar_t *name, int cap)
{
    ComPtr<IDXGIFactory1> f;
    ComPtr<IDXGIAdapter1> a;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&f))) || index < 0 || FAILED(f->EnumAdapters1(index, &a)))
        return 0;
    DXGI_ADAPTER_DESC1 d{};
    if (FAILED(a->GetDesc1(&d)))
        return 0;
    copyText(name, cap, d.Description);
    return (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? 2 : 1;
}
FB int fb_refresh(const wchar_t *display)
{
    // Query the active display path. EnumDisplaySettings can report the
    // desktop timing while DXGI owns an exclusive presentation surface.
    UINT32 paths = 0, modes = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &paths, &modes) == ERROR_SUCCESS)
    {
        std::vector<DISPLAYCONFIG_PATH_INFO> path(paths);
        std::vector<DISPLAYCONFIG_MODE_INFO> mode(modes);
        if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &paths, path.data(), &modes, mode.data(), nullptr) ==
            ERROR_SUCCESS)
        {
            for (UINT32 i = 0; i < paths; ++i)
            {
                DISPLAYCONFIG_SOURCE_DEVICE_NAME name{};
                name.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(name),
                               path[i].sourceInfo.adapterId, path[i].sourceInfo.id};
                if (DisplayConfigGetDeviceInfo(&name.header) != ERROR_SUCCESS ||
                    wcscmp(display, name.viewGdiDeviceName))
                    continue;
                const auto rate = path[i].targetInfo.refreshRate;
                if (rate.Denominator && rate.Numerator)
                    return int(std::lround(double(rate.Numerator) / rate.Denominator));
            }
        }
    }
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    return EnumDisplaySettingsW(display, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1
               ? int(dm.dmDisplayFrequency)
               : 0;
}
FB int fb_adapter_vendor(int index)
{
    ComPtr<IDXGIFactory1> factory;
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 desc{};
    if (index < 0 || FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) ||
        FAILED(factory->EnumAdapters1(index, &adapter)) || FAILED(adapter->GetDesc1(&desc)))
        return 0;
    return desc.VendorId;
}
FB int fb_adapter_info(int index, wchar_t *text, int cap)
{
    ComPtr<IDXGIFactory1> f;
    ComPtr<IDXGIAdapter1> a;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&f))) || FAILED(f->EnumAdapters1(index, &a)))
        return 0;
    DXGI_ADAPTER_DESC1 d{};
    a->GetDesc1(&d);
    LARGE_INTEGER ver{};
    std::wstring driver = L"unavailable";
    if (SUCCEEDED(a->CheckInterfaceSupport(__uuidof(IDXGIDevice), &ver)))
    {
        auto v = ver.QuadPart;
        driver = std::to_wstring((v >> 48) & 65535) + L"." + std::to_wstring((v >> 32) & 65535) + L"." +
                 std::to_wstring((v >> 16) & 65535) + L"." + std::to_wstring(v & 65535);
    }
    ComPtr<ID3D11Device> device;
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    bool ok = SUCCEEDED(D3D11CreateDevice(a.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &level, 1,
                                          D3D11_SDK_VERSION, &device, nullptr, nullptr));
    ComPtr<ID3D11VideoDevice> video;
    bool vp = ok && SUCCEEDED(device.As(&video));
    std::wstring result = L"Driver " + driver +
                          (ok ? L"   ·   D3D11 compute" : L"   ·   Compute unavailable") +
                          (vp ? L"   ·   Video device" : L"");
    copyText(text, cap, result);
    return ok ? 1 : 2;
}
