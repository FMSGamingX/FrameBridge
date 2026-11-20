// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#pragma once
#include "../Platform.hpp"
namespace fb
{
struct Inference
{
    virtual ~Inference() = default;
    virtual void generate(const Frame &a, const Frame &b, Frame &middle) = 0;
    virtual std::wstring name() const = 0;
};
std::unique_ptr<Inference> openModel(const std::wstring &path, int adapter, int quality);
} // namespace fb
