// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "../Platform.hpp"
#include <pdh.h>
#include <pdhmsg.h>
namespace
{
struct Meter
{
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    std::wstring prefix;
    Meter()
    {
        prefix = L"pid_" + std::to_wstring(GetCurrentProcessId()) + L"_";
        if (PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS)
        {
            if (PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter) ==
                ERROR_SUCCESS)
                PdhCollectQueryData(query);
        }
    }
    ~Meter()
    {
        if (query)
            PdhCloseQuery(query);
    }
    double sample()
    {
        if (!counter || PdhCollectQueryData(query) != ERROR_SUCCESS)
            return -1;
        DWORD bytes = 0, n = 0;
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bytes, &n, nullptr) != PDH_MORE_DATA)
            return -1;
        std::vector<BYTE> data(bytes);
        auto items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(data.data());
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bytes, &n, items) != ERROR_SUCCESS)
            return -1;
        double max = -1;
        for (DWORD i = 0; i < n; i++)
            if (items[i].szName && std::wstring_view(items[i].szName).starts_with(prefix) &&
                items[i].FmtValue.CStatus <= PDH_CSTATUS_NEW_DATA)
                max = std::max(max, items[i].FmtValue.doubleValue);
        return max;
    }
};
} // namespace
FB double fb_gpu_usage()
{
    try
    {
        static Meter meter;
        return meter.sample();
    }
    catch (...)
    {
        return -1;
    }
}
