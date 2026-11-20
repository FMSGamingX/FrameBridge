// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
// This test-only translation unit reads back the actual compute textures. No
// staging copies or diagnostic entry points are required in the shipped DLL.
#include "../Engine/GPU/Graphics.cpp"
#include "FlowFixture.hpp"
#include <cstdio>
fb::Frame readback(fb::Gpu &gpu, ID3D11Texture2D *source)
{
    D3D11_TEXTURE2D_DESC d{};
    source->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    d.BindFlags = 0;
    fb::ComPtr<ID3D11Texture2D> target;
    fb::check(gpu.device->CreateTexture2D(&d, nullptr, &target), "Create readback surface");
    gpu.ctx->CopyResource(target.Get(), source);
    D3D11_MAPPED_SUBRESOURCE m{};
    fb::check(gpu.ctx->Map(target.Get(), 0, D3D11_MAP_READ, 0, &m), "Read back compute result");
    fb::Frame out;
    out.size(d.Width, d.Height);
    for (int y = 0; y < out.height; y++)
        memcpy(out.pixels.data() + size_t(y) * out.width, static_cast<const char *>(m.pData) + y * m.RowPitch,
               out.width * 4);
    gpu.ctx->Unmap(target.Get(), 0);
    return out;
}
#include "Monitor.hpp"
extern "C" int fb_nvidia_flow_adapter(int index);
double panError(HWND window, int index, bool nvidia)
{
    // A 40-pixel color pan exceeds the Balanced block matcher's search range.
    fb::Frame a, b;
    a.size(384, 224);
    b.size(a.width, a.height);
    unsigned seed = 91;
    for (auto &p : a.pixels)
    {
        seed = seed * 1664525 + 1013904223;
        p = 0xff000000u | (seed >> 8);
    }
    for (int y = 0; y < a.height; ++y)
        for (int x = 0; x < a.width; ++x)
            b.pixels[y*a.width+x] = a.pixels[y*a.width+std::max(x-40,0)];
    fb::Config c{};
    c.quality = c.reduceArtifacts = 1;
    fb::Gpu gpu(window, c, index, nvidia);
    gpu.prepare(a, a, false);
    gpu.prepare(a, b, true);
    auto mid = readback(gpu, gpu.mid.data.Get());
    double error = 0;
    int samples = 0;
    for (int y = 32; y < a.height-32; ++y)
        for (int x = 64; x < a.width-64; ++x)
            for (int shift : {0,8,16})
            {
                int reference = (a.pixels[y*a.width+x-20] >> shift) & 255;
                int generated = (mid.pixels[y*a.width+x] >> shift) & 255;
                error += std::abs(reference-generated);
                ++samples;
            }
    return error/samples;
}
void verifyNvidia(HWND window, int index)
{
    fb::Frame a, b;
    fixture::translation(a, b);
    for (int quality = 0; quality < 3; ++quality)
    {
        fb::Config c{};
        c.quality = quality;
        c.reduceArtifacts = 1;
        fb::Gpu gpu(window, c, index, true);
        gpu.prepare(a, a, false);
        gpu.prepare(a, b, true);
        auto mid = readback(gpu, gpu.mid.data.Get());
        auto original = readback(gpu, gpu.input[gpu.current].data.Get());
        int matches = fixture::midpointMatches(a, mid);
        double error = 0, repeatError = 0;
        for (int y = 16; y < 64; ++y)
            for (int x = 16; x < 80; ++x)
            {
                int i = y * a.width + x;
                int expected = int(a.pixels[i - 4] & 255);
                error += std::abs(int(mid.pixels[i] & 255) - expected);
                repeatError += std::abs(int(a.pixels[i] & 255) - expected);
            }
        printf("NVIDIA Optical Flow preset %d: %d/3072 exact, midpoint MAE %.2f, repeated-frame MAE %.2f.\n",
               quality, matches, error/3072, repeatError/3072);
        if (error >= 0.15 * repeatError || original.pixels != b.pixels)
            throw std::runtime_error("NVIDIA optical flow motion or original-frame check failed.");
        gpu.prepare(b, b, true);
        if (readback(gpu, gpu.mid.data.Get()).pixels != b.pixels)
            throw std::runtime_error("NVIDIA optical flow changed a static frame.");
    }
    double classic = panError(window, index, false);
    double optical = panError(window, index, true);
    printf("40-pixel color pan, mean absolute channel error: traditional %.2f, NVIDIA %.2f.\n", classic, optical);
    if (optical >= classic)
        throw std::runtime_error("NVIDIA flow failed the wide color-pan regression.");
}
int main()
{
    MONITORINFOEXW info{};
    if (!fbtest::secondary75(info))
    {
        fprintf(stderr, "Secondary 75 Hz test monitor unavailable.\n");
        return 1;
    }
    HWND window =
        CreateWindowExW(0, L"STATIC", L"FrameBridge shader verification", WS_POPUP, info.rcMonitor.left + 16,
                        info.rcMonitor.top + 16, 128, 96, nullptr, nullptr, nullptr, nullptr);
    if (!window)
        return 1;
    try
    {
        fb::ComPtr<IDXGIFactory1> factory;
        fb::check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "Enumerate test GPUs");
        for (int i = 0;; i++)
        {
            fb::ComPtr<IDXGIAdapter1> adapter;
            if (FAILED(factory->EnumAdapters1(i, &adapter)))
                break;
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;
            if (fb_nvidia_flow_adapter(i))
                verifyNvidia(window, i);
            D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
            if (FAILED(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &level, 1,
                                         D3D11_SDK_VERSION, nullptr, nullptr, nullptr)))
                continue;
            fb::Config c{};
            c.quality = 1;
            c.reduceArtifacts = 1;
            c.hudAware = 1;
            fb::Gpu gpu(window, c, i);
            fb::Frame a, b;
            fixture::translation(a, b);
            gpu.prepare(a, a, false);
            gpu.prepare(a, b, true);
            auto mid = readback(gpu, gpu.mid.data.Get());
            auto original = readback(gpu, gpu.input[gpu.current].data.Get());
            int matches = fixture::midpointMatches(a, mid);
            printf("GPU adapter %d: %d/3072 translated pixels exact.\n", i, matches);
            if (matches != 3072 || original.pixels != b.pixels)
                throw std::runtime_error("GPU pixel verification failed.");
            for (int y = 16; y < 64; y++)
                a.pixels[y * 96 + 41] = b.pixels[y * 96 + 41] = 0xffff00ff;
            gpu.prepare(a, a, false);
            gpu.prepare(a, b, true);
            mid = readback(gpu, gpu.mid.data.Get());
            for (int y = 16; y < 64; y++)
                if (mid.pixels[y * 96 + 41] != 0xffff00ff)
                    throw std::runtime_error("Stationary GPU overlay moved.");
        }
    }
    catch (const std::exception &e)
    {
        fprintf(stderr, "%s\n", e.what());
        DestroyWindow(window);
        return 1;
    }
    DestroyWindow(window);
}
