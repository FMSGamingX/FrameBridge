// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "Channel.hpp"
#include "ShaderBytecode.hpp"
namespace fb
{
struct Presentation
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;
    ComPtr<IDXGISwapChain> chain;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps, originalShader;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11Buffer> constants;
    struct Surface
    {
        unsigned generation{};
        ComPtr<ID3D11Texture2D> data;
        ComPtr<ID3D11ShaderResourceView> view;
        ComPtr<IDXGIKeyedMutex> mutex;
    } surfaces[2];
    HWND hwnd;
    Config config;
    int vw{}, vh{}, w{}, h{};
    Presentation(Packet &p) : hwnd(p.window), config(p.config)
    {
        const auto &c = config;
        ComPtr<IDXGIFactory1> f;
        ComPtr<IDXGIAdapter1> a;
        check(CreateDXGIFactory1(IID_PPV_ARGS(&f)), "Open output DXGI");
        if (p.adapter >= 0)
            check(f->EnumAdapters1(p.adapter, &a), "Open producing adapter");
        HRESULT result = D3D11CreateDevice(a.Get(), a ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                                           nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &ctx);
        if (FAILED(result) && p.adapter < 0)
            result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                                       D3D11_SDK_VERSION, &device, nullptr, &ctx);
        check(result, "Create presentation device");
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        if (c.exclusive)
        {
            sd.BufferDesc.Width = c.outputW;
            sd.BufferDesc.Height = c.outputH;
            sd.BufferDesc.RefreshRate = {UINT(c.rateNum), UINT(c.rateDen)};
        }
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.OutputWindow = hwnd;
        sd.Windowed = TRUE;
        sd.SwapEffect = c.exclusive ? DXGI_SWAP_EFFECT_DISCARD : DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.Flags = c.exclusive ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;
        check(f->CreateSwapChain(device.Get(), &sd, &chain), "Create output swap chain");
        f->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        check(device->CreateVertexShader(vertexCode, sizeof vertexCode, nullptr, &vs),
              "Create output geometry");
        check(device->CreatePixelShader(pixelCode, sizeof pixelCode, nullptr, &ps),
              "Create packed frame shader");
        check(device->CreatePixelShader(nvOriginalCode, sizeof nvOriginalCode, nullptr, &originalShader),
              "Create BGRA shader");
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = 16;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        check(device->CreateBuffer(&bd, nullptr, &constants), "Create output dimensions");
        if (c.exclusive)
        {
            ComPtr<IDXGIOutput> output;
            check(chain->GetContainingOutput(&output), "Find fullscreen monitor");
            DXGI_MODE_DESC requested{};
            requested.Width = c.outputW;
            requested.Height = c.outputH;
            requested.RefreshRate = {UINT(c.rateNum), UINT(c.rateDen)};
            requested.Format = sd.BufferDesc.Format;
            DXGI_MODE_DESC match{};
            check(output->FindClosestMatchingMode(&requested, &match, device.Get()), "Validate monitor mode");
            if (match.Width != requested.Width || match.Height != requested.Height ||
                uint64_t(match.RefreshRate.Numerator) * requested.RefreshRate.Denominator !=
                    uint64_t(requested.RefreshRate.Numerator) * match.RefreshRate.Denominator)
                throw std::runtime_error("The selected monitor mode is no longer available.");
            HRESULT fullscreen = DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
            for (int attempt = 0; attempt < 8 && fullscreen == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE; ++attempt)
            {
                if (attempt)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                fullscreen = chain->SetFullscreenState(TRUE, output.Get());
            }
            if (fullscreen == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE && GetForegroundWindow() != hwnd)
                throw std::runtime_error(
                    "Windows requires the output window to have foreground focus for exclusive fullscreen.");
            check(fullscreen, "Enter exclusive fullscreen");
            HRESULT resized = chain->ResizeTarget(&match);
            if (FAILED(resized))
            {
                chain->SetFullscreenState(FALSE, nullptr);
                check(resized, "Apply fullscreen resolution");
            }
        }
    }
    ~Presentation()
    {
        if (config.exclusive && chain)
            chain->SetFullscreenState(FALSE, nullptr);
    }
    bool frame(Packet &p)
    {
        if (p.slot < 0 || p.slot > 1 || p.width < 1 || p.height < 1 || p.width > 8192 || p.height > 8192 ||
            int64_t(p.width) * p.height > 16777216)
            throw std::runtime_error("Invalid output frame dimensions or format.");
        auto &s = surfaces[p.slot];
        bool resized = w != p.width || h != p.height;
        w = p.width;
        h = p.height;
        if (resized)
        {
            int values[]{w, h, 0, 0};
            ctx->UpdateSubresource(constants.Get(), 0, nullptr, values, 0, 0);
        }
        if (p.adapter < 0)
        {
            if (resized || !s.data)
            {
                D3D11_TEXTURE2D_DESC d{};
                d.Width = w;
                d.Height = h;
                d.MipLevels = d.ArraySize = d.SampleDesc.Count = 1;
                d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                check(device->CreateTexture2D(&d, nullptr, s.data.ReleaseAndGetAddressOf()),
                      "Create upload surface");
                check(
                    device->CreateShaderResourceView(s.data.Get(), nullptr, s.view.ReleaseAndGetAddressOf()),
                    "Bind CPU output");
            }
            ctx->UpdateSubresource(s.data.Get(), 0, nullptr, &p + 1, w * 4, 0);
            return draw(s.view.Get(), true);
        }
        if (s.generation != p.generation)
        {
            s = Surface{};
            check(device->OpenSharedResource(p.texture, IID_PPV_ARGS(&s.data)), "Open GPU frame");
            check(s.data.As(&s.mutex), "Open GPU consumer mutex");
            check(device->CreateShaderResourceView(s.data.Get(), nullptr, &s.view), "Bind shared frame");
            s.generation = p.generation;
        }
        if (s.mutex->AcquireSync(1, 3000) != S_OK)
            throw std::runtime_error("GPU consumer timeout.");
        bool shown = draw(s.view.Get(), p.slot == 1);
        check(s.mutex->ReleaseSync(0), "Return GPU frame");
        return shown;
    }
    bool draw(ID3D11ShaderResourceView *view, bool bgra)
    {
        RECT r{};
        GetClientRect(hwnd, &r);
        if (r.right < 1 || r.bottom < 1 || !IsWindowVisible(hwnd))
            return false;
        if (vw != r.right || vh != r.bottom)
        {
            ctx->OMSetRenderTargets(0, nullptr, nullptr);
            target.Reset();
            vw = r.right;
            vh = r.bottom;
            check(chain->ResizeBuffers(0, vw, vh, DXGI_FORMAT_UNKNOWN,
                                       config.exclusive ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0),
                  "Resize output surface");
            ComPtr<ID3D11Texture2D> back;
            check(chain->GetBuffer(0, IID_PPV_ARGS(&back)), "Access back buffer");
            check(device->CreateRenderTargetView(back.Get(), nullptr, &target), "Bind back buffer");
        }
        float scale = std::min(float(vw) / w, float(vh) / h);
        D3D11_VIEWPORT vp{(vw - w * scale) / 2, (vh - h * scale) / 2, w * scale, h * scale, 0, 1};
        float black[]{0, 0, 0, 1};
        ctx->ClearRenderTargetView(target.Get(), black);
        ctx->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
        ctx->RSSetViewports(1, &vp);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(vs.Get(), nullptr, 0);
        bool nvOriginal = bgra;
        ctx->PSSetShader(nvOriginal ? originalShader.Get() : ps.Get(), nullptr, 0);
        ctx->PSSetConstantBuffers(0, 1, constants.GetAddressOf());
        ctx->PSSetShaderResources(nvOriginal ? 4 : 0, 1, &view);
        ctx->Draw(3, 0);
        ID3D11ShaderResourceView *empty = nullptr;
        ctx->PSSetShaderResources(nvOriginal ? 4 : 0, 1, &empty);
        HRESULT hr = chain->Present(config.vsync ? 1 : 0, 0);
        if (hr == DXGI_STATUS_MODE_CHANGED)
        {
            vw = vh = 0;
            return false;
        }
        check(hr, "Present output frame");
        return hr == S_OK;
    }
};
} // namespace fb
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR name, int)
{
    using namespace fb;
    SetProcessDPIAware();
    try
    {
        Channel channel;
        channel.open(name, false, false);
        auto &p = *channel.packet;
        Handle parent;
        parent.set(OpenProcess(SYNCHRONIZE, FALSE, p.producer));
        DWORD owner{};
        GetWindowThreadProcessId(p.window, &owner);
        if (owner != p.producer || p.protocol != outputProtocol)
            return 2;
        HANDLE waits[]{parent.value, channel.ready.value};
        try
        {
            std::unique_ptr<Presentation> output;
            while (WaitForMultipleObjects(2, waits, FALSE, INFINITE) == WAIT_OBJECT_0 + 1 && !p.stop)
            {
                if (!output)
                    output = std::make_unique<Presentation>(p);
                if (p.width > 0)
                    p.presented = output->frame(p);
                SetEvent(channel.done.value);
            }
        }
        catch (const std::exception &e)
        {
            snprintf(p.error, sizeof p.error, "%s", e.what());
            SetEvent(channel.done.value);
            return 1;
        }
    }
    catch (...)
    {
        return 2;
    }
    return 0;
}
