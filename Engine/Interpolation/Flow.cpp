// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "Flow.hpp"
namespace fb
{
void BlockFlow::generate(const Frame &a, const Frame &b, Frame &out, int quality, bool protect, bool hudAware)
{
    const int w = b.width, h = b.height, sw = (w + 1) / 2, sh = (h + 1) / 2;
    out.size(w, h);
    auto luma = [&](const Frame &f, std::vector<uint8_t> &result) {
        result.resize(size_t(sw) * sh);
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++)
            {
                uint32_t p = f.pixels[size_t(y * 2) * w + x * 2];
                result[y * sw + x] = ((p & 255) * 29 + ((p >> 8) & 255) * 150 + ((p >> 16) & 255) * 77) >> 8;
            }
    };
    luma(a, lumaA);
    luma(b, lumaB);
    const int radius = std::clamp(quality + 1, 1, 5);
    for (int y = 0; y < h; y += 16)
        for (int x = 0; x < w; x += 16)
        {
            int mx = 0, my = 0, best = INT_MAX;
            auto score = [&](int dx, int dy) {
                int sum = abs(dx) + abs(dy);
                for (int j = 1; j <= 7; j += 3)
                    for (int i = 1; i <= 7; i += 3)
                    {
                        int ax = std::clamp(x / 2 + i - dx, 0, sw - 1),
                            ay = std::clamp(y / 2 + j - dy, 0, sh - 1);
                        int bx = std::clamp(x / 2 + i + dx, 0, sw - 1),
                            by = std::clamp(y / 2 + j + dy, 0, sh - 1);
                        sum += abs(lumaA[ay * sw + ax] - lumaB[by * sw + bx]);
                    }
                if (sum < best)
                {
                    best = sum;
                    mx = dx;
                    my = dy;
                }
            };
            score(0, 0);
            if (best > 9)
            {
                for (int j = -radius; j <= radius; j++)
                    for (int i = -radius; i <= radius; i++)
                        score(i * 2, j * 2);
                const int cx = mx, cy = my;
                for (int j = -1; j <= 1; j++)
                    for (int i = -1; i <= 1; i++)
                        score(cx + i, cy + j);
            }
            mx *= 2;
            my *= 2;
            for (int yy = y; yy < std::min(y + 16, h); yy++)
                for (int xx = x; xx < std::min(x + 16, w); xx++)
                {
                    const int index = yy * w + xx;
                    const uint32_t pa =
                        a.pixels[std::clamp(yy - my, 0, h - 1) * w + std::clamp(xx - mx, 0, w - 1)];
                    const uint32_t pb =
                        b.pixels[std::clamp(yy + my, 0, h - 1) * w + std::clamp(xx + mx, 0, w - 1)];
                    // A moving tile can carry stationary HUD ink with it. Trust exact
                    // stationary RGB when the warped samples disagree about this pixel.
                    bool fixedOverlay = false;
                    const uint32_t center = a.pixels[index];
                    if (hudAware && ((center ^ b.pixels[index]) & 0xffffffu) == 0 &&
                        ((((pa ^ center) | (pb ^ center)) & 0xffffffu) != 0))
                    {
                        auto edge = [&](int n) {
                            auto contrast = [&](uint32_t v) {
                                return std::max({abs(int(v & 255) - int(center & 255)),
                                                 abs(int((v >> 8) & 255) - int((center >> 8) & 255)),
                                                 abs(int((v >> 16) & 255) - int((center >> 16) & 255))});
                            };
                            return contrast(a.pixels[n]) > 64 && contrast(b.pixels[n]) > 64;
                        };
                        auto ink = [&](int n) {
                            return n != index &&
                                   (((center ^ a.pixels[n]) | (center ^ b.pixels[n])) & 0xffffffu) == 0;
                        };
                        bool stroke =
                            ink(yy * w + std::max(xx - 1, 0)) || ink(yy * w + std::min(xx + 1, w - 1)) ||
                            ink(std::max(yy - 1, 0) * w + xx) || ink(std::min(yy + 1, h - 1) * w + xx);
                        fixedOverlay = stroke && (((pa ^ pb) & 0xf8f8f8u) != 0 ||
                                                  edge(yy * w + std::min(xx + 2, w - 1)) ||
                                                  edge(std::min(yy + 2, h - 1) * w + xx));
                    }
                    out.pixels[index] =
                        (fixedOverlay || (protect && best > 300))
                            ? a.pixels[index]
                            : (0xff000000u | (((pa & pb) + (((pa ^ pb) & 0xfefefeu) >> 1)) & 0xffffffu));
                }
        }
}
} // namespace fb
