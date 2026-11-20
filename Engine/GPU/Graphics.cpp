// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "../Rendering/Renderer.hpp"
#include "../Backends/NvidiaFlow.hpp"
#include "ShaderBytecode.hpp"
#include "../Rendering/OutputClient.hpp"
namespace fb
{
namespace
{
struct Texture
{
    ComPtr<ID3D11Texture2D> data;
    ComPtr<ID3D11ShaderResourceView> read;
    ComPtr<ID3D11UnorderedAccessView> write;
};
struct Gpu final : Renderer
{
    HWND hwnd;
    Config config;
    std::wstring adapter;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;
    OutputClient output;
    ComPtr<ID3D11ComputeShader> estimate, warp;
    ComPtr<ID3D11Buffer> constants;
    struct Timing
    {
        ComPtr<ID3D11Query> clock, begin, end;
        bool pending = false;
    };
    Timing timings[4];
    unsigned nextTiming = 0;
    double measured = -1;
    double gpuMilliseconds() override
    {
        return std::exchange(measured, -1.);
    }
    Texture input[2], vectors, reverseVectors, mid;
    std::unique_ptr<NvidiaFlow> optical;
    ComPtr<ID3D11SamplerState> sampler;
    bool useNvidia, resetHints = true;
    int current = 0, w = 0, h = 0;
    Gpu(HWND window, const Config &c, int index, bool nvidia = false) : hwnd(window), config(c), useNvidia(nvidia)
    {
        ComPtr<IDXGIFactory1> f;
        ComPtr<IDXGIAdapter1> a;
        check(CreateDXGIFactory1(IID_PPV_ARGS(&f)), "Open DXGI");
        check(f->EnumAdapters1(index, &a), "Find selected GPU");
        DXGI_ADAPTER_DESC1 desc{};
        check(a->GetDesc1(&desc), "Identify GPU");
        adapter = desc.Description;
        if (useNvidia && desc.VendorId != 0x10de)
            throw std::runtime_error("NVIDIA Optical Flow requires a compatible NVIDIA GPU.");
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            throw std::runtime_error("Software adapter cannot be selected for hardware processing.");
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        check(D3D11CreateDevice(a.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &level, 1, D3D11_SDK_VERSION,
                                &device, nullptr, &ctx),
              "Create Direct3D 11 compute device");
        check(device->CreateComputeShader(estimateCode, sizeof estimateCode, nullptr, &estimate),
              "Create motion shader");
        check(device->CreateComputeShader(useNvidia ? nvWarpCode : warpCode,
                                          useNvidia ? sizeof nvWarpCode : sizeof warpCode, nullptr, &warp),
              "Create warping shader");
        if (useNvidia)
        {
            D3D11_SAMPLER_DESC s{};
            s.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            s.ComparisonFunc = D3D11_COMPARISON_NEVER;
            check(device->CreateSamplerState(&s, &sampler), "Create midpoint sampler");
        }
        for (auto &slot : timings)
        {
            auto query = [&](D3D11_QUERY type, ComPtr<ID3D11Query> &dest) {
                D3D11_QUERY_DESC desc{type, 0};
                check(device->CreateQuery(&desc, &dest), "Create GPU timing query");
            };
            query(D3D11_QUERY_TIMESTAMP_DISJOINT, slot.clock);
            query(D3D11_QUERY_TIMESTAMP, slot.begin);
            query(D3D11_QUERY_TIMESTAMP, slot.end);
        }
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = 16;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        check(device->CreateBuffer(&bd, nullptr, &constants), "Create shader settings");

    }
    Texture texture(int x, int y, DXGI_FORMAT format, bool write)
    {
        Texture t;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = x;
        td.Height = y;
        td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
        td.Format = format;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | (write ? D3D11_BIND_UNORDERED_ACCESS : 0);
        check(device->CreateTexture2D(&td, nullptr, &t.data), "Allocate GPU frame");
        check(device->CreateShaderResourceView(t.data.Get(), nullptr, &t.read), "Create frame reader");
        if (write)
            check(device->CreateUnorderedAccessView(t.data.Get(), nullptr, &t.write), "Create frame writer");
        return t;
    }
    void prepare(const Frame &, const Frame &b, bool generate) override
    {
        if (w != b.width || h != b.height)
        {
            w = b.width;
            h = b.height;
            optical.reset();
            for (auto &t : input)
                t = texture(w, h, useNvidia ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R32_UINT, false);
            if (useNvidia)
            {
                optical = std::make_unique<NvidiaFlow>();
                optical->open(device.Get(), ctx.Get(), w, h, config.quality);
                int cols = (w + NvidiaFlow::grid - 1) / NvidiaFlow::grid;
                int rows = (h + NvidiaFlow::grid - 1) / NvidiaFlow::grid;
                vectors = texture(cols, rows, DXGI_FORMAT_R16G16_SINT, true);
                reverseVectors = texture(cols, rows, DXGI_FORMAT_R16G16_SINT, true);
                optical->bind(input[0].data.Get(), input[1].data.Get(), vectors.data.Get(), reverseVectors.data.Get());
            }
            else
                vectors = texture((w + 15) / 16, (h + 15) / 16, DXGI_FORMAT_R32G32B32A32_SINT, true);
            mid = texture(w, h, DXGI_FORMAT_R32_UINT, true);
            int values[]{w, h, std::clamp(config.quality + 1, 1, 5),
                         (config.reduceArtifacts ? 1 : 0) | (config.hudAware ? 2 : 0)};
            ctx->UpdateSubresource(constants.Get(), 0, nullptr, values, 0, 0);
        }
        current ^= 1;
        ctx->UpdateSubresource(input[current].data.Get(), 0, nullptr, b.pixels.data(), w * 4, 0);
        if (!generate)
        {
            resetHints = true;
            return;
        }
        // Poll older timestamps without stalling this frame on GPU completion.
        auto &timing = timings[nextTiming++ % 4];
        if (timing.pending)
        {
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT clock{};
            UINT64 a = 0, b = 0;
            if (ctx->GetData(timing.clock.Get(), &clock, sizeof clock, D3D11_ASYNC_GETDATA_DONOTFLUSH) ==
                    S_OK &&
                ctx->GetData(timing.begin.Get(), &a, sizeof a, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
                ctx->GetData(timing.end.Get(), &b, sizeof b, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK)
            {
                if (!clock.Disjoint && clock.Frequency && b >= a)
                    measured = 1000. * (b - a) / clock.Frequency;
                timing.pending = false;
            }
        }
        bool record = !timing.pending;
        if (record)
        {
            ctx->Begin(timing.clock.Get());
            ctx->End(timing.begin.Get());
        }
        ID3D11ShaderResourceView *reads[]{input[current ^ 1].read.Get(), input[current].read.Get(), nullptr};
        ctx->CSSetConstantBuffers(0, 1, constants.GetAddressOf());
        ID3D11UnorderedAccessView *empty = nullptr;
        if (useNvidia)
        {
            optical->execute(current, resetHints);
            resetHints = false;
            ID3D11ShaderResourceView *nvReads[]{reads[0], reads[1], vectors.read.Get(), reverseVectors.read.Get()};
            ctx->CSSetShaderResources(4, 4, nvReads);
            ctx->CSSetSamplers(0, 1, sampler.GetAddressOf());
        }
        else
        {
            ctx->CSSetShaderResources(0, 3, reads);
            ctx->CSSetUnorderedAccessViews(0, 1, vectors.write.GetAddressOf(), nullptr);
            ctx->CSSetShader(estimate.Get(), nullptr, 0);
            ctx->Dispatch((w + 127) / 128, (h + 127) / 128, 1);
            ctx->CSSetUnorderedAccessViews(0, 1, &empty, nullptr);
            ctx->CSSetShaderResources(2, 1, vectors.read.GetAddressOf());
        }
        ctx->CSSetUnorderedAccessViews(1, 1, mid.write.GetAddressOf(), nullptr);
        ctx->CSSetShader(warp.Get(), nullptr, 0);
        ctx->Dispatch((w + 15) / 16, (h + 15) / 16, 1);
        ctx->CSSetUnorderedAccessViews(1, 1, &empty, nullptr);
        if (record)
        {
            ctx->End(timing.end.Get());
            ctx->End(timing.clock.Get());
            timing.pending = true;
        }
        ID3D11ShaderResourceView *clear[8]{};
        ctx->CSSetShaderResources(0, 8, clear);
    }
    bool present(const Frame &, bool middle) override
    {

        return output.gpu(device.Get(),ctx.Get(),(middle ? mid : input[current]).data.Get());
    }
    ~Gpu()
    {
        if (ctx)
        {
            ctx->ClearState();
            ctx->Flush();
        }
    }
    std::wstring name() const override
    {
        return adapter + (useNvidia ? L" · NVIDIA Optical Flow" : L" · Direct3D 11");
    }
};
} // namespace
std::unique_ptr<Renderer> gpuRenderer(HWND hwnd, const Config &c, int index, bool nvidia)
{
    auto renderer = std::make_unique<Gpu>(hwnd, c, index, nvidia);
    renderer->output.open(hwnd, c, index);
    return renderer;
}
} // namespace fb
