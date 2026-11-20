// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace fb
{
using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;
using Time = Clock::time_point;
inline double ms(Time a, Time b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}
inline void check(HRESULT hr, const char *op)
{
    if (SUCCEEDED(hr))
        return;
    char message[512]{}, code[16]{};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, hr, 0, message,
                   sizeof message, nullptr);
    snprintf(code, sizeof code, "%08lX", (unsigned long)hr);
    throw std::runtime_error(std::string(op) + " (0x" + code + "). " + message);
}
inline std::wstring wide(const char *text)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (n <= 1)
        return L"Video processing error.";
    std::wstring result(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), n);
    result.pop_back();
    return result;
}
struct Apartment
{
    Apartment()
    {
        check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Start video worker");
    }
    ~Apartment()
    {
        CoUninitialize();
    }
};
inline void copyText(wchar_t *dst, int cap, const std::wstring &text)
{
    if (cap > 0 && dst)
    {
        int n = (int)std::min<size_t>(cap - 1, text.size());
        std::copy_n(text.data(), n, dst);
        dst[n] = 0;
    }
}
struct Frame
{
    int width = 0, height = 0;
    double pts = 0;
    Time arrival{};
    bool discontinuity = false;
    std::vector<uint32_t> pixels;
    void size(int w, int h)
    {
        if (w < 1 || h < 1 || w > 8192 || h > 8192 || int64_t(w) * h > 16777216)
            throw std::runtime_error("Unsupported frame dimensions.");
        width = w;
        height = h;
        pixels.resize(size_t(w) * h);
    }
};
} // namespace fb
#define FB extern "C" __declspec(dllexport)
