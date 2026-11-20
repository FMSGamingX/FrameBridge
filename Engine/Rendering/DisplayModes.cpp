// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "../Platform.hpp"
#include <set>
#include <tuple>
using namespace fb;
struct SavedDisplay
{
    std::wstring device;
    DEVMODEW original{};
    bool changed = false;
};
FB void *fb_select_display(const wchar_t *device, int w, int h, int num, int den)
{
    try
    {
        if (!device || den <= 0)
            return nullptr;
        auto saved = std::make_unique<SavedDisplay>();
        saved->device = device;
        saved->original.dmSize = sizeof(DEVMODEW);
        if (!EnumDisplaySettingsW(device, ENUM_CURRENT_SETTINGS, &saved->original))
            return nullptr;
        double rate = double(num) / den;
        DEVMODEW candidate{}, best{};
        candidate.dmSize = sizeof(candidate);
        double distance = 1.0;
        for (DWORD i = 0; EnumDisplaySettingsW(device, i, &candidate); ++i)
        {
            double diff = std::abs(candidate.dmDisplayFrequency - rate);
            if (candidate.dmPelsWidth == UINT(w) && candidate.dmPelsHeight == UINT(h) &&
                candidate.dmBitsPerPel == saved->original.dmBitsPerPel && diff < distance)
            {
                best = candidate;
                distance = diff;
            }
        }
        if (!best.dmSize)
            return nullptr;
        const auto &old = saved->original;
        if (best.dmPelsWidth != old.dmPelsWidth || best.dmPelsHeight != old.dmPelsHeight ||
            best.dmDisplayFrequency != old.dmDisplayFrequency)
        {
            if (ChangeDisplaySettingsExW(device, &best, nullptr, CDS_FULLSCREEN, nullptr) !=
                DISP_CHANGE_SUCCESSFUL)
                return nullptr;
            saved->changed = true;
        }
        return saved.release();
    }
    catch (...)
    {
        return nullptr;
    }
}
FB int fb_restore_display(SavedDisplay *saved)
{
    std::unique_ptr<SavedDisplay> owner(saved);
    return !saved || !saved->changed ||
           ChangeDisplaySettingsExW(saved->device.c_str(), &saved->original, nullptr, CDS_FULLSCREEN,
                                    nullptr) == DISP_CHANGE_SUCCESSFUL;
}
FB int fb_display_modes(const wchar_t *display, int selected, wchar_t *out, int cap)
{
    try
    {
        ComPtr<IDXGIFactory1> factory;
        check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "Enumerate display modes");
        for (int index = 0;; index++)
        {
            ComPtr<IDXGIAdapter1> adapter;
            if (FAILED(factory->EnumAdapters1(index, &adapter)))
                break;
            if (selected >= 0 && selected != index)
                continue;
            for (UINT j = 0;; j++)
            {
                ComPtr<IDXGIOutput> output;
                if (FAILED(adapter->EnumOutputs(j, &output)))
                    break;
                DXGI_OUTPUT_DESC info{};
                if (FAILED(output->GetDesc(&info)) || !info.AttachedToDesktop ||
                    wcscmp(info.DeviceName, display))
                    continue;
                UINT n = 0;
                auto fmt = DXGI_FORMAT_B8G8R8A8_UNORM;
                check(output->GetDisplayModeList(fmt, 0, &n, nullptr), "Count display modes");
                std::vector<DXGI_MODE_DESC> modes(n);
                if (n)
                    check(output->GetDisplayModeList(fmt, 0, &n, modes.data()), "Read display modes");
                std::set<std::tuple<int, int, int, int>> unique;
                for (const auto &m : modes)
                    if (m.Width && m.Height && m.RefreshRate.Numerator && m.RefreshRate.Denominator &&
                        m.ScanlineOrdering != DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST &&
                        m.ScanlineOrdering != DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST)
                        unique.emplace(m.Width, m.Height, m.RefreshRate.Numerator, m.RefreshRate.Denominator);
                std::wstring lines;
                for (auto [w, h, num, den] : unique)
                {
                    wchar_t row[96];
                    swprintf(row, 96, L"%d|%d|%d|%d|%d\n", w, h, num, den, index);
                    lines += row;
                }
                if (int(lines.size()) >= cap)
                    return -1;
                copyText(out, cap, lines);
                return int(unique.size());
            }
        }
        return 0;
    }
    catch (...)
    {
        return -1;
    }
}
