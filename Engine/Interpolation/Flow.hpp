// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#pragma once
#include "../Platform.hpp"
namespace fb
{
// Each 16×16 output tile searches for a symmetric displacement in the two
// originals. Offsets are half-motion vectors centred at the requested midpoint.
class BlockFlow
{
    std::vector<uint8_t> lumaA, lumaB;

  public:
    void generate(const Frame &a, const Frame &b, Frame &out, int quality, bool protect,
                  bool hudAware = false);
};
} // namespace fb
