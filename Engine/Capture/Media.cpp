// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "Sources.hpp"
#include <deque>
namespace fb
{
namespace
{
std::wstring attrString(IMFAttributes *a, REFGUID k)
{
    wchar_t *p = nullptr;
    UINT32 n = 0;
    check(a->GetAllocatedString(k, &p, &n), "Read device identity");
    std::wstring s(p, n);
    CoTaskMemFree(p);
    return s;
}
struct Devices
{
    IMFActivate **items = nullptr;
    UINT32 count = 0;
    Devices()
    {
        ComPtr<IMFAttributes> a;
        check(MFCreateAttributes(&a, 1), "Create discovery options");
        check(a->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID),
              "Select video inputs");
        check(MFEnumDeviceSources(a.Get(), &items, &count), "Discover video inputs");
    }
    ~Devices()
    {
        for (UINT32 i = 0; i < count; i++)
            items[i]->Release();
        CoTaskMemFree(items);
    }
};
ComPtr<IMFMediaSource> activate(const std::wstring &key)
{
    ComPtr<IMFAttributes> a;
    check(MFCreateAttributes(&a, 2), "Create camera options");
    check(a->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID),
          "Select capture");
    check(a->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, key.c_str()),
          "Select camera");
    ComPtr<IMFMediaSource> source;
    check(MFCreateDeviceSource(a.Get(), &source), "Open capture device");
    return source;
}
struct Result
{
    HRESULT hr = S_OK;
    DWORD flags = 0;
    LONGLONG stamp = 0;
    ComPtr<IMFSample> sample;
};
class Callback final : public IMFSourceReaderCallback
{
    std::atomic<ULONG> refs{1};

  public:
    std::mutex lock;
    std::condition_variable cv;
    Result result;
    bool ready = false;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **p) override
    {
        if (!p)
            return E_POINTER;
        *p = nullptr;
        if (id != IID_IUnknown && id != __uuidof(IMFSourceReaderCallback))
            return E_NOINTERFACE;
        *p = this;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++refs;
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG n = --refs;
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE OnReadSample(HRESULT hr, DWORD, DWORD flags, LONGLONG t, IMFSample *s) override
    {
        std::lock_guard g(lock);
        result = {hr, flags, t, s};
        ready = true;
        cv.notify_one();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnEvent(DWORD, IMFMediaEvent *) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnFlush(DWORD) override
    {
        return S_OK;
    }
};
struct SourceShutdown
{
    ComPtr<IMFMediaSource> s;
    ~SourceShutdown()
    {
        if (s)
            s->Shutdown();
    }
};
constexpr DWORD video = MF_SOURCE_READER_FIRST_VIDEO_STREAM;
struct Media final : Source
{
    SourceShutdown capture;
    ComPtr<Callback> callback;
    ComPtr<IMFSourceReader> reader;
    UINT32 w = 0, h = 0;
    LONG stride = 0;
    Media(const Config &c, const std::wstring &key)
    {
        ComPtr<IMFAttributes> a;
        check(MFCreateAttributes(&a, 3), "Create reader options");
        callback.Attach(new Callback);
        check(a->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, callback.Get()), "Configure asynchronous input");
        check(a->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE), "Enable RGB conversion");
        if (c.kind == int(Kind::Device))
        {
            capture.s = activate(key);
            check(MFCreateSourceReaderFromMediaSource(capture.s.Get(), a.Get(), &reader),
                  "Start capture reader");
            if (c.profile >= 0)
            {
                ComPtr<IMFMediaType> profile;
                check(reader->GetNativeMediaType(video, c.profile, &profile),
                      "Read selected capture profile");
                ComPtr<IMFSourceReaderEx> ex;
                check(reader.As(&ex), "Select a native capture profile");
                DWORD flags = 0;
                check(ex->SetNativeMediaType(video, profile.Get(), &flags), "Apply native capture profile");
            }
        }
        else
        {
            DWORD attrs = GetFileAttributesW(key.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
                throw std::runtime_error("Select an existing local video file.");
            check(MFCreateSourceReaderFromURL(key.c_str(), a.Get(), &reader), "Open local video");
        }
        check(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE), "Disable unused streams");
        check(reader->SetStreamSelection(video, TRUE), "Select video stream");
        ComPtr<IMFMediaType> rgb;
        check(MFCreateMediaType(&rgb), "Create RGB format");
        rgb->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        rgb->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        check(reader->SetCurrentMediaType(video, nullptr, rgb.Get()),
              "Negotiate RGB32 (check codec or capture profile)");
        format();
    }
    void format()
    {
        ComPtr<IMFMediaType> native;
        hdr = 0;
        if (SUCCEEDED(reader->GetNativeMediaType(video, MF_SOURCE_READER_CURRENT_TYPE_INDEX, &native)))
        {
            UINT32 transfer = 0;
            if (SUCCEEDED(native->GetUINT32(MF_MT_TRANSFER_FUNCTION, &transfer)) && transfer)
            {
                if (transfer == MFVideoTransFunc_2084 || transfer == MFVideoTransFunc_HLG)
                    throw std::runtime_error(
                        "HDR transfer function detected. Select an SDR input for this version.");
                hdr = 1;
            }
        }
        ComPtr<IMFMediaType> mt;
        check(reader->GetCurrentMediaType(video, &mt), "Read negotiated format");
        GUID sub{};
        check(mt->GetGUID(MF_MT_SUBTYPE, &sub), "Read color format");
        if (sub != MFVideoFormat_RGB32)
            throw std::runtime_error("The source changed to an unsupported pixel format.");
        check(MFGetAttributeSize(mt.Get(), MF_MT_FRAME_SIZE, &w, &h), "Read frame size");
        UINT32 n = 0, d = 0;
        rate =
            SUCCEEDED(MFGetAttributeRatio(mt.Get(), MF_MT_FRAME_RATE, &n, &d)) && n && d ? double(n) / d : 0;
        UINT32 raw;
        if (SUCCEEDED(mt->GetUINT32(MF_MT_DEFAULT_STRIDE, &raw)))
            stride = (LONG)raw;
        else
            check(MFGetStrideForBitmapInfoHeader(sub.Data1, w, &stride), "Read pixel stride");
    }
    bool read(Frame &f, const std::atomic_bool &stop) override
    {
        Time deadline = Clock::now() + std::chrono::seconds(8);
        while (!stop)
        {
            check(reader->ReadSample(video, 0, nullptr, nullptr, nullptr, nullptr), "Request video frame");
            Result r;
            {
                std::unique_lock g(callback->lock);
                while (!callback->ready && !stop)
                {
                    callback->cv.wait_for(g, std::chrono::milliseconds(40));
                    if (Clock::now() > deadline)
                        throw std::runtime_error("No frames arrived for eight seconds. Check input signal "
                                                 "and camera permissions.");
                }
                if (stop)
                    return false;
                r = std::move(callback->result);
                callback->ready = false;
            }
            check(r.hr, "Decode video frame");
            if (r.flags & MF_SOURCE_READERF_ERROR)
                throw std::runtime_error("The capture driver or decoder stopped.");
            if (r.flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
                format();
            if (!r.sample)
            {
                if (r.flags & MF_SOURCE_READERF_ENDOFSTREAM)
                    return false;
                continue;
            }
            f.size(w, h);
            f.pts = r.stamp / 10000000.;
            f.discontinuity = MFGetAttributeUINT32(r.sample.Get(), MFSampleExtension_Discontinuity, 0) != 0;
            ComPtr<IMFMediaBuffer> b;
            check(r.sample->ConvertToContiguousBuffer(&b), "Access decoded pixels");
            ComPtr<IMF2DBuffer> two;
            BYTE *p = nullptr;
            LONG pitch = stride;
            if (SUCCEEDED(b.As(&two)))
            {
                check(two->Lock2D(&p, &pitch), "Lock video plane");
                if (std::abs((int64_t)pitch) < w * 4ull)
                {
                    two->Unlock2D();
                    throw std::runtime_error("Invalid capture pitch.");
                }
            }
            else
            {
                DWORD len = 0;
                check(b->Lock(&p, nullptr, &len), "Lock decoded buffer");
                if (std::abs((int64_t)pitch) < w * 4ull ||
                    uint64_t(std::abs((int64_t)pitch)) * (h - 1) + w * 4ull > len)
                {
                    b->Unlock();
                    throw std::runtime_error("Invalid decoded frame layout.");
                }
                if (pitch < 0)
                    p += size_t(-pitch) * (h - 1);
            }
            for (UINT32 y = 0; y < h; y++)
                memcpy(f.pixels.data() + size_t(y) * w, p + ptrdiff_t(y) * pitch, w * 4);
            if (two)
                two->Unlock2D();
            else
                b->Unlock();
            return true;
        }
        return false;
    }
};
} // namespace
std::unique_ptr<Source> mediaSource(const Config &c, const std::wstring &key)
{
    return std::make_unique<Media>(c, key);
}
} // namespace fb
using namespace fb;
FB double fb_file_rate(const wchar_t *path)
{
    try
    {
        MediaRuntime runtime;
        ComPtr<IMFSourceReader> reader;
        ComPtr<IMFMediaType> format;
        if (FAILED(MFCreateSourceReaderFromURL(path, nullptr, &reader)) ||
            FAILED(reader->GetNativeMediaType(video, MF_SOURCE_READER_CURRENT_TYPE_INDEX, &format)))
            return 0;
        UINT32 n = 0, d = 0;
        return SUCCEEDED(MFGetAttributeRatio(format.Get(), MF_MT_FRAME_RATE, &n, &d)) && d ? double(n) / d
                                                                                           : 0;
    }
    catch (...)
    {
        return 0;
    }
}
FB int fb_device(int i, wchar_t *name, int nc, wchar_t *key, int kc)
{
    try
    {
        MediaRuntime runtime;
        Devices d;
        if (i < 0 || UINT32(i) >= d.count)
            return 0;
        auto *a = d.items[i];
        auto id = attrString(a, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK);
        if (int(id.size()) >= kc)
            return -1;
        copyText(name, nc, attrString(a, MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME));
        copyText(key, kc, id);
        return 1;
    }
    catch (...)
    {
        return -1;
    }
}
FB int fb_profiles(const wchar_t *key, wchar_t *text, int cap)
{
    try
    {
        MediaRuntime runtime;
        SourceShutdown source{activate(key)};
        ComPtr<IMFSourceReader> r;
        check(MFCreateSourceReaderFromMediaSource(source.s.Get(), nullptr, &r), "Read device formats");
        std::wstring lines;
        for (DWORD i = 0; i < 512; i++)
        {
            ComPtr<IMFMediaType> t;
            if (FAILED(r->GetNativeMediaType(video, i, &t)))
                break;
            UINT32 w = 0, h = 0, n = 0, d = 0;
            GUID sub{};
            MFGetAttributeSize(t.Get(), MF_MT_FRAME_SIZE, &w, &h);
            MFGetAttributeRatio(t.Get(), MF_MT_FRAME_RATE, &n, &d);
            t->GetGUID(MF_MT_SUBTYPE, &sub);
            wchar_t four[5]{};
            for (int j = 0; j < 4; j++)
            {
                int ch = (sub.Data1 >> (8 * j)) & 255;
                four[j] = (ch >= 32 && ch < 127) ? ch : L' ';
            }
            wchar_t line[150];
            swprintf(line, 150, L"%u|%u × %u   ·   %.2f fps   ·   %ls\n", i, w, h, d ? double(n) / d : 0,
                     four);
            lines += line;
        }
        if (int(lines.size()) >= cap)
            return -1;
        copyText(text, cap, lines);
        return 1;
    }
    catch (...)
    {
        return -1;
    }
}
