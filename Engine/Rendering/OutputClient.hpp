// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#pragma once
#include "../../Output/Channel.hpp"
namespace fb
{
class OutputClient
{
    Channel channel;
    Handle worker;
    struct Slot
    {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<IDXGIKeyedMutex> mutex;
        HANDLE shared{};
        int width{}, height{};
        unsigned generation{};
    } slots[2];
    bool submit()
    {
        SetEvent(channel.ready.value);
        HANDLE waits[]{channel.done.value, worker.value};
        if (WaitForMultipleObjects(2, waits, FALSE, 10000) != WAIT_OBJECT_0)
            throw std::runtime_error("FrameBridge.Output stopped responding.");
        if (channel.packet->error[0])
            throw std::runtime_error(channel.packet->error);
        return channel.packet->presented;
    }

  public:
    void open(HWND hwnd, const Config &config, int adapter, FrameTransport transport)
    {
        if (worker.value)
            return;
        GUID guid{};
        check(CoCreateGuid(&guid), "Create output session");
        wchar_t id[40]{}, path[32768]{};
        StringFromGUID2(guid, id, 40);
        std::wstring name = std::wstring(L"Local\\FrameBridge-") + id;
        channel.open(name, true, transport == FrameTransport::Pixels);
        auto &p = *channel.packet;
        p.protocol = outputProtocol;
        p.window = hwnd;
        p.producer = GetCurrentProcessId();
        p.config = config;
        p.transport = transport;
        p.adapter = adapter;
        // The engine may also be hosted by the native test runner.
        HMODULE engine = GetModuleHandleW(L"FrameBridge.Engine.dll");
        GetModuleFileNameW(engine, path, 32768);
        std::wstring exe(path);
        exe = exe.substr(0, exe.find_last_of(L"\\/") + 1) + L"FrameBridge.Output.exe";
        auto cmd = L"\"" + exe + L"\" " + name;
        STARTUPINFOW si{sizeof(si)};
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
            check(HRESULT_FROM_WIN32(GetLastError()), "Launch FrameBridge.Output.exe");
        CloseHandle(pi.hThread);
        worker.set(pi.hProcess);
        submit(); // Wait for the presentation device before starting input capture.
    }
    ~OutputClient()
    {
        if (worker.value)
        {
            channel.packet->stop = true;
            SetEvent(channel.ready.value);
            if (WaitForSingleObject(worker.value, 1500) == WAIT_TIMEOUT)
                TerminateProcess(worker.value, 2);
        }
    }
    bool pixels(const Frame &f)
    {
        auto &p = *channel.packet;
        p.width = f.width;
        p.height = f.height;
        memcpy(&p + 1, f.pixels.data(), f.pixels.size() * 4);
        return submit();
    }
    bool gpu(ID3D11Device *device, ID3D11DeviceContext *ctx, ID3D11Texture2D *source)
    {
        D3D11_TEXTURE2D_DESC d{};
        source->GetDesc(&d);
        auto &p = *channel.packet;
        p.width = d.Width;
        p.height = d.Height;
        p.slot = d.Format == DXGI_FORMAT_B8G8R8A8_UNORM ? 1 : 0;
        auto &s = slots[p.slot];
        if (s.width != p.width || s.height != p.height)
        {
            s.mutex.Reset();
            s.texture.Reset();
            d.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            d.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
            check(device->CreateTexture2D(&d, nullptr, &s.texture), "Create shared GPU frame");
            ComPtr<IDXGIResource> resource;
            check(s.texture.As(&resource), "Access shared GPU frame");
            check(resource->GetSharedHandle(&s.shared), "Share GPU frame");
            check(s.texture.As(&s.mutex), "Open GPU producer mutex");
            s.width = p.width;
            s.height = p.height;
            ++s.generation;
        }
        p.texture = s.shared;
        p.generation = s.generation;
        if (s.mutex->AcquireSync(0, 3000) != S_OK)
            throw std::runtime_error("GPU producer mutex timed out.");
        ctx->CopyResource(s.texture.Get(), source);
        check(s.mutex->ReleaseSync(1), "Publish GPU frame");
        ctx->Flush();
        return submit();
    }
};
} // namespace fb
