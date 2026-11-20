// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "Sources.hpp"
namespace fb
{
namespace
{
struct Plugin
{
    HMODULE module{};
    int (*paired)(const char *){};
    int (*pair)(const char *, const uint8_t *, unsigned, const char *, char *, int){};
    void *(*open)(const char *, int, char *, int){};
    int (*read)(void *, uint32_t *, int, int *, int *, double *, int *, char *, int){};
    void (*close)(void *){};
    Plugin()
    {
        wchar_t path[32768]{};
        GetModuleFileNameW(GetModuleHandleW(L"FrameBridge.Engine.dll"), path, 32768);
        std::wstring file = path;
        file = file.substr(0, file.find_last_of(L"\\") + 1) + L"PlayStation\\FrameBridge.PlayStation.dll";
        module = LoadLibraryExW(file.c_str(), nullptr,
                                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module)
            return;
        paired = reinterpret_cast<decltype(paired)>(GetProcAddress(module, "ps_paired"));
        pair = reinterpret_cast<decltype(pair)>(GetProcAddress(module, "ps_pair"));
        open = reinterpret_cast<decltype(open)>(GetProcAddress(module, "ps_open"));
        read = reinterpret_cast<decltype(read)>(GetProcAddress(module, "ps_read"));
        close = reinterpret_cast<decltype(close)>(GetProcAddress(module, "ps_close"));
        if (!paired || !pair || !open || !read || !close)
        {
            FreeLibrary(module);
            module = nullptr;
        }
    }
};
Plugin &plugin()
{
    static Plugin p;
    return p;
}
struct PlayStation final : Source
{
    void *stream{};
    PlayStation(const Config &c, const std::wstring &host)
    {
        if (!plugin().module)
            throw std::runtime_error("Install the optional chiaki-ng module and restart FrameBridge.");
        rate = c.inputMilliFps == 60000 ? 60 : 30;
        char error[384]{};
        std::string address(host.begin(), host.end());
        stream = plugin().open(address.c_str(), int(rate), error, sizeof error);
        if (!stream)
            throw std::runtime_error(error);
    }
    ~PlayStation()
    {
        plugin().close(stream);
    }
    bool read(Frame &f, const std::atomic_bool &stop) override
    {
        f.size(1920, 1080);
        auto deadline = Clock::now() + std::chrono::seconds(25);
        while (!stop)
        {
            int w{}, h{}, gap{};
            char error[384]{};
            int result = plugin().read(stream, f.pixels.data(), int(f.pixels.size()), &w, &h, &f.pts, &gap,
                                       error, sizeof error);
            if (result < 0)
                throw std::runtime_error(error);
            if (result > 0)
            {
                f.size(w, h);
                f.discontinuity = gap != 0;
                return true;
            }
            if (Clock::now() > deadline)
                throw std::runtime_error("PS5 stopped delivering video. Check the console and reconnect.");
        }
        return false;
    }
};
} // namespace
std::unique_ptr<Source> playStationSource(const Config &c, const std::wstring &host)
{
    return std::make_unique<PlayStation>(c, host);
}
FB int fb_ps_available()
{
    return plugin().module != nullptr;
}
FB int fb_ps_paired(const char *host)
{
    return host && plugin().module && plugin().paired(host);
}
FB int fb_ps_pair(const char *host, const uint8_t *account, unsigned pin, const char *login, wchar_t *message,
                  int len)
{
    char error[384] = "Install the optional chiaki-ng module and restart FrameBridge.";
    int ok =
        host && account && plugin().module && plugin().pair(host, account, pin, login, error, sizeof error);
    copyText(message, len, ok ? L"Paired. Start processing to connect." : wide(error));
    return ok;
}
} // namespace fb
