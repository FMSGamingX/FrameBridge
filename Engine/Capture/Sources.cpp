// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "Sources.hpp"
namespace fb
{
static uint32_t rgb(int r, int g, int b)
{
    return 0xff000000u | (r << 16) | (g << 8) | b;
}
void pattern(Frame &f, double t)
{
    // Deterministic parallax grid, textured moving targets and calibration marks.
    // Cache the stationary field per worker and rebuild only when the size changes.
    struct Plate
    {
        int w = 0, h = 0;
        std::vector<uint32_t> pixels;
    };
    thread_local Plate plate;
    const int w = f.width, h = f.height;
    if (plate.w != w || plate.h != h)
    {
        plate.w = w;
        plate.h = h;
        plate.pixels.resize(size_t(w) * h);
        int step = std::max(12, w / 24);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                double u = double(x) / w, v = double(y) / h;
                double glow = std::max(0., 1. - std::hypot((u - .63) * 1.6, (v - .42) * 1.4));
                int grid = ((x % step == 0) || (y % step == 0)) ? 9 : 0;
                int dot = (x % step < 3 && y % step < 3) ? 22 : 0;
                plate.pixels[size_t(y) * w + x] =
                    rgb(10 + int(9 * glow) + grid + dot, 21 + int(17 * glow) + grid + dot,
                        34 + int(29 * glow) + grid + dot);
            }
    }
    std::copy(plate.pixels.begin(), plate.pixels.end(), f.pixels.begin());

    const auto disc = [&](double x, double y, double r, bool amber) {
        int cx = int(x * w), cy = int(y * h), rad = std::max(3, int(r * h));
        for (int dy = -rad; dy <= rad; dy++)
            for (int dx = -rad; dx <= rad; dx++)
            {
                int px = cx + dx, py = cy + dy;
                if (px < 0 || py < 0 || px >= w || py >= h)
                    continue;
                double a = double(dx) / rad, b = double(dy) / rad, q = a * a + b * b;
                if (q > 1)
                    continue;
                double z = sqrt(1 - q), light = std::clamp(.16 + .72 * z - .31 * a - .34 * b, 0., 1.);
                double line = std::cos(19 * (a * .65 + b * .17 + z * .75));
                double s = light * (line > .93 ? .70 : 1.);
                f.pixels[size_t(py) * w + px] =
                    amber ? rgb(96 + int(153 * s), 45 + int(142 * s), 52 + int(60 * s))
                          : rgb(22 + int(91 * s), 68 + int(178 * s), 99 + int(132 * s));
            }
    };
    disc(.45 + .22 * sin(t * .8), .48 + .14 * sin(t * .55), .237, false);
    disc(.76 + .07 * sin(t * 1.2), .39 + .16 * cos(t), .083, true);
    int x = int(std::fmod(t * 0.21 + .13, 1.) * (w - 1));
    for (int y = h * 83 / 100; y < h * 87 / 100; y++)
        for (int d = 0; d < std::max(2, w / 150) && x + d < w; d++)
            f.pixels[size_t(y) * w + x + d] = rgb(118, 236, 210);
}
struct PatternSource final : Source
{
    int w, h;
    int64_t tick = 0;
    PatternSource(int w, int h) : w(w), h(h) {}
    bool read(Frame &f, const std::atomic_bool &) override
    {
        f.size(w, h);
        f.pts = tick++ / rate;
        pattern(f, f.pts);
        return true;
    }
};
struct DesktopSource final : Source
{
    int x, y, w, h;
    HDC screen = nullptr, dc = nullptr;
    HBITMAP dib = nullptr;
    HGDIOBJ old = nullptr;
    void *bits = nullptr;
    int64_t tick = 0;
    DesktopSource(const Config &c) : x(c.desktopX), y(c.desktopY), w(c.desktopW), h(c.desktopH)
    {
        if (w <= 0 || h <= 0 || int64_t(w) * h > 16777216)
            throw std::runtime_error("Select an attached desktop display.");
        screen = GetDC(nullptr);
        dc = CreateCompatibleDC(screen);
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
        dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!screen || !dc || !dib)
        {
            release();
            throw std::runtime_error("Could not allocate the desktop capture surface.");
        }
        old = SelectObject(dc, dib);
    }
    void release()
    {
        if (old)
            SelectObject(dc, old);
        if (dib)
            DeleteObject(dib);
        if (dc)
            DeleteDC(dc);
        if (screen)
            ReleaseDC(nullptr, screen);
    }
    ~DesktopSource()
    {
        release();
    }
    bool read(Frame &f, const std::atomic_bool &) override
    {
        if (!BitBlt(dc, 0, 0, w, h, screen, x, y, SRCCOPY | CAPTUREBLT))
            throw std::runtime_error("Windows denied desktop capture.");
        GdiFlush();
        f.size(w, h);
        memcpy(f.pixels.data(), bits, f.pixels.size() * 4);
        f.pts = tick++ / rate;
        return true;
    }
};
std::unique_ptr<Source> openSource(const Config &c, const std::wstring &key)
{
    switch (Kind(c.kind))
    {
    case Kind::Pattern: {
        auto src = std::make_unique<PatternSource>(c.width, c.height);
        src->rate = c.inputMilliFps > 0 ? std::clamp(c.inputMilliFps / 1000., 1., 120.) : 30;
        return src;
    }
    case Kind::Desktop: {
        auto src = std::make_unique<DesktopSource>(c);
        src->rate = c.inputMilliFps > 0 ? std::clamp(c.inputMilliFps / 1000., 1., 120.) : 30;
        return src;
    }
    case Kind::File:
    case Kind::Device:
        return mediaSource(c, key);
    case Kind::PlayStation:
        return playStationSource(c,key);
    }
    throw std::runtime_error("Unknown input.");
}
} // namespace fb
FB void fb_still(int w, int h, uint32_t *out)
{
    try
    {
        fb::Frame f;
        f.size(w, h);
        fb::pattern(f, 0);
        memcpy(out, f.pixels.data(), f.pixels.size() * 4);
    }
    catch (...)
    {
    }
}
