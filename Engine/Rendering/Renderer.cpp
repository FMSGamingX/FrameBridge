// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "Renderer.hpp"
#include "OutputClient.hpp"
#include "../Interpolation/Flow.hpp"
#include "../AI/Inference.hpp"
namespace fb
{
struct Software final : Renderer
{
    HWND hwnd;
    Config config;
    OutputClient output;
    Frame mid;
    BlockFlow flow;
    std::unique_ptr<Inference> ai;
    Software(HWND window, const Config &c) : hwnd(window), config(c) { output.open(hwnd,c,-1); }
    void prepare(const Frame &a, const Frame &b, bool gen) override
    {
        if (!gen)
            return;
        if (ai)
            ai->generate(a, b, mid);
        else
            flow.generate(a, b, mid, config.quality, config.reduceArtifacts != 0,config.hudAware!=0);
    }
    bool present(const Frame &original, bool midpoint) override
    {
        const Frame &f = midpoint ? mid : original;

        return output.pixels(f);
    }
    std::wstring name() const override
    {
        return ai ? ai->name() : L"CPU · Direct3D output";
    }
};
std::unique_ptr<Renderer> gpuRenderer(HWND hwnd, const Config &c, int index, bool nvidia);
std::unique_ptr<Renderer> makeRenderer(HWND hwnd, const Config &c, std::wstring &notice,
                                       const std::wstring &model, bool nvidia)
{
    if (nvidia)
    {
        if (c.backend < 0)
            throw std::runtime_error("Select a compatible NVIDIA Optical Flow adapter.");
        return gpuRenderer(hwnd, c, c.backend, true);
    }
    auto create = [&](int index) -> std::unique_ptr<Renderer> {
        if (model.empty())
        {
            if (index >= 0)
                return gpuRenderer(hwnd, c, index, false);
            return std::make_unique<Software>(hwnd, c);
        }
        auto r = std::make_unique<Software>(hwnd, c);
        r->ai = openModel(model, index, c.quality);
        return r;
    };
    if (c.backend >= 0)
        return create(c.backend);
    if (c.backend == -1)
    {
        ComPtr<IDXGIFactory1> f;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&f))))
        {
            std::vector<std::pair<size_t, int>> adapters;
            for (int i = 0;; ++i)
            {
                ComPtr<IDXGIAdapter1> a;
                if (FAILED(f->EnumAdapters1(i, &a)))
                    break;
                DXGI_ADAPTER_DESC1 d{};
                if (SUCCEEDED(a->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
                    adapters.emplace_back(d.DedicatedVideoMemory, i);
            }
            std::stable_sort(adapters.begin(), adapters.end(),
                             [](auto a, auto b) { return a.first > b.first; });
            for (auto [mem, index] : adapters)
            {
                try
                {
                    return create(index);
                }
                catch (const std::exception &e)
                {
                    notice = L"GPU initialization failed: ";
                    notice += wide(e.what());
                }
            }
        }
        notice += L" Using CPU fallback.";
    }
    return create(-2);
}
} // namespace fb
