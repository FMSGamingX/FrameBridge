// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#pragma once
#include "../Capture/Sources.hpp"
namespace fb
{
struct Renderer
{
    virtual ~Renderer() = default;
    virtual void prepare(const Frame &a, const Frame &b, bool generate) = 0;
    virtual bool present(const Frame &original, bool midpoint) = 0;
    virtual double gpuMilliseconds()
    {
        return -1;
    }
    virtual std::wstring name() const = 0;
};
std::unique_ptr<Renderer> makeRenderer(HWND hwnd, const Config &c, std::wstring &notice,
                                       const std::wstring &model, bool nvidia);
} // namespace fb
