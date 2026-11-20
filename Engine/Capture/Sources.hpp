// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#pragma once
#include "../Platform.hpp"
namespace fb
{
enum class Kind
{
    Pattern,
    File,
    Device,
    Desktop
};
struct Config
{
    int kind, width, height, profile, backend, quality, reduceArtifacts, generate, refresh, buffer;
    int desktopX, desktopY, desktopW, desktopH;
    int vsync, exclusive, outputW, outputH, rateNum, rateDen, inputMilliFps, hudAware;
};
struct Source
{
    double rate = 30;
    int hdr = 0;
    virtual ~Source() = default;
    virtual bool read(Frame &frame, const std::atomic_bool &stop) = 0;
};
struct MediaRuntime
{
    MediaRuntime()
    {
        check(MFStartup(MF_VERSION), "Initialize Windows Media Foundation");
    }
    ~MediaRuntime()
    {
        MFShutdown();
    }
};
std::unique_ptr<Source> openSource(const Config &c, const std::wstring &key);
std::unique_ptr<Source> mediaSource(const Config &c, const std::wstring &key);
void pattern(Frame &f, double seconds);
} // namespace fb
