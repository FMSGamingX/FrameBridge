// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#pragma once
#include "../Engine/Platform.hpp"
namespace fixture
{
inline void translation(fb::Frame &a, fb::Frame &b)
{
    a.size(96, 80);
    b.size(96, 80);
    unsigned seed = 73;
    for (auto &p : a.pixels)
    {
        seed = seed * 1664525 + 1013904223;
        unsigned g = seed >> 24;
        p = 0xff000000u | g * 0x010101u;
    }
    for (int y = 0; y < 80; y++)
        for (int x = 0; x < 96; x++)
            b.pixels[y * 96 + x] = a.pixels[y * 96 + std::max(0, x - 8)];
}
inline int midpointMatches(const fb::Frame &a, const fb::Frame &mid)
{
    int correct = 0;
    for (int y = 16; y < 64; y++)
        for (int x = 16; x < 80; x++)
            correct += mid.pixels[y * 96 + x] == a.pixels[y * 96 + x - 4];
    return correct;
}
} // namespace fixture
