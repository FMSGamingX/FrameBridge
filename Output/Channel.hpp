// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#pragma once
#include "../Engine/Capture/Sources.hpp"
namespace fb
{
struct Handle
{
    HANDLE value{};
    ~Handle()
    {
        if (value)
            CloseHandle(value);
    }
    Handle() = default;
    Handle(const Handle &) = delete;
    void set(HANDLE h)
    {
        if (!h)
            check(HRESULT_FROM_WIN32(GetLastError()), "Open presentation IPC");
        value = h;
    }
};
inline constexpr uint32_t outputProtocol = 0x46420102;
enum class FrameTransport { Pixels, SharedTexture };
struct Packet
{
    uint32_t protocol;
    HWND window;
    DWORD producer;
    Config config;
    FrameTransport transport;
    int adapter, width, height, slot;
    unsigned generation;
    HANDLE texture;
    bool stop, presented;
    char error[512];
};
struct Channel
{
    Handle mapping, ready, done;
    Packet *packet{};
    ~Channel()
    {
        if (packet)
            UnmapViewOfFile(packet);
    }
    void open(const std::wstring &name, bool create, bool cpu)
    {
        mapping.set(create ? CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                                sizeof(Packet) + (cpu ? 64 * 1024 * 1024 : 0), name.c_str())
                           : OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str()));
        packet = static_cast<Packet *>(MapViewOfFile(mapping.value, FILE_MAP_ALL_ACCESS, 0, 0, 0));
        if (!packet)
            throw std::runtime_error("Cannot map presentation channel.");
        auto event = [&](const wchar_t *suffix) {
            auto key = name + suffix;
            return create ? CreateEventW(nullptr, FALSE, FALSE, key.c_str())
                          : OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, key.c_str());
        };
        ready.set(event(L"-submit"));
        done.set(event(L"-complete"));
    }
};
} // namespace fb
