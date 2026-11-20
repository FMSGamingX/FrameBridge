// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "../Engine/Interpolation/Flow.hpp"
#include <cstdio>
#include "FlowFixture.hpp"
// Known translation. Verify the signed displacement and reconstruction against
// ground truth, including the distinction from a simple cross-fade.
int main()
{
    fb::Frame a, b, mid;
    fixture::translation(a, b);
    auto original = a.pixels;
    fb::BlockFlow f;
    f.generate(a, b, mid, 1, true);
    int correct = fixture::midpointMatches(a, mid), total = 3072;
    if (a.pixels != original || correct != total)
    {
        fprintf(stderr, "Midpoint accuracy: %d/%d\n", correct, total);
        return 1;
    }
    for (int y = 16; y < 64; ++y)
    {
        a.pixels[y * 96 + 41] = b.pixels[y * 96 + 41] = 0xffff00ff;
    }
    int retained[2]{};
    for (int mode = 0; mode < 2; ++mode)
    {
        f.generate(a, b, mid, 1, true, mode != 0);
        for (int y = 16; y < 64; ++y)
            retained[mode] += mid.pixels[y * 96 + 41] == 0xffff00ff;
    }
    printf("Fixed overlay on moving texture: classic %d/48, HUD-aware %d/48.\n", retained[0], retained[1]);
    if (retained[1] != 48 || retained[0] >= retained[1])
        return 4;
    f.generate(a, a, mid, 0, true);
    if (mid.pixels != a.pixels)
        return 2;
    a.size(1, 1);
    b.size(1, 1);
    a.pixels[0] = b.pixels[0] = 0xffaaccff;
    f.generate(a, b, mid, 4, true);
    if (mid.pixels != a.pixels)
        return 3;
    printf("CPU flow: %d/%d translated pixels exact; identical and single-pixel frames preserved.\n", correct,
           total);
}
