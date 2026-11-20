// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "../Platform.hpp"
#include "NvidiaFlow.hpp"
FB int fb_nvidia_flow_adapter(int index)
{
    using namespace fb;
    try
    {
        ComPtr<IDXGIFactory1> factory;
        ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 desc{};
        if (index < 0 || FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) ||
            FAILED(factory->EnumAdapters1(index, &adapter)) || FAILED(adapter->GetDesc1(&desc)) ||
            desc.VendorId != 0x10de)
            return 0;
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        check(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &level, 1,
                                D3D11_SDK_VERSION, &device, nullptr, &context), "Probe optical flow device");
        NvidiaFlow probe;
        probe.open(device.Get(), context.Get(), 1280, 720, 1);
        return 1;
    }
    catch (const std::exception &)
    {
        return 0;
    }
}
namespace
{
const wchar_t *dlssCaptureStatus()
{
    return L"DLSS Frame Generation requires game depth and engine motion vectors. Finished video does not "
           L"supply those resources.";
}
const wchar_t *smoothMotionCaptureStatus()
{
    return L"Smooth Motion is a driver feature for supported games. NVIDIA has not documented a "
           L"captured-frame inference API for this pipeline.";
}
} // namespace
FB void fb_nvidia_status(int mode, wchar_t *text, int length)
{
    fb::copyText(text, length, mode == 0 ? dlssCaptureStatus() : smoothMotionCaptureStatus());
}
