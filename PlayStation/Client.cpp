// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include <winsock2.h>
#include <windows.h>
#include <wincred.h>
#include <xinput.h>
#include <mmsystem.h>
#include <chiaki/session.h>
#include <chiaki/ffmpegdecoder.h>
#include <chiaki/opusdecoder.h>
extern "C"
{
#include <libswscale/swscale.h>
}
#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#define API extern "C" __declspec(dllexport)
namespace
{
struct Keys
{
    char registration[16]{};
    uint8_t morning[16]{};
    char login[5]{};
    ~Keys()
    {
        SecureZeroMemory(this, sizeof(*this));
    }
};
std::string target(const char *host)
{
    return std::string("FrameBridge/PS5/") + host;
}
bool load(const char *host, Keys &keys)
{
    PCREDENTIALA cred{};
    if (!CredReadA(target(host).c_str(), CRED_TYPE_GENERIC, 0, &cred))
        return false;
    bool ok = cred->CredentialBlobSize == sizeof(keys);
    if (ok)
        memcpy(&keys, cred->CredentialBlob, sizeof(keys));
    SecureZeroMemory(cred->CredentialBlob, cred->CredentialBlobSize);
    CredFree(cred);
    return ok;
}
bool initialize()
{
    static const auto result = chiaki_lib_init();
    return result == CHIAKI_ERR_SUCCESS;
}
struct Registration
{
    Keys keys;
    bool ok{};
};
void registered(ChiakiRegistEvent *event, void *user)
{
    if (event->type != CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS || !event->registered_host)
        return;
    auto &result = *static_cast<Registration *>(user);
    memcpy(result.keys.registration, event->registered_host->rp_regist_key, 16);
    memcpy(result.keys.morning, event->registered_host->rp_key, 16);
    result.ok = true;
}
struct Audio
{
    HWAVEOUT device{};
    unsigned channels{};
    struct Slot
    {
        WAVEHDR header{};
        std::vector<char> bytes;
    };
    std::array<Slot, 4> slots;
    void reset()
    {
        if (!device)
            return;
        waveOutReset(device);
        for (auto &s : slots)
        {
            waveOutUnprepareHeader(device, &s.header, sizeof(s.header));
            s.header = {};
        }
        waveOutClose(device);
        device = nullptr;
    }
    ~Audio()
    {
        reset();
    }
    static void settings(uint32_t channels, uint32_t rate, void *user)
    {
        auto &a = *static_cast<Audio *>(user);
        a.reset();
        if (!channels || channels > 8 || !rate)
            return;
        WAVEFORMATEX fmt{};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = channels;
        fmt.nSamplesPerSec = rate;
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = channels * 2;
        fmt.nAvgBytesPerSec = rate * fmt.nBlockAlign;
        a.channels = channels;
        waveOutOpen(&a.device, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL);
    }
    static void frame(int16_t *pcm, size_t samples, void *user)
    {
        auto &a = *static_cast<Audio *>(user);
        if (!a.device || samples > 8192)
            return;
        for (auto &s : a.slots)
        {
            if (s.header.dwFlags & WHDR_INQUEUE)
                continue;
            if (s.header.dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(a.device, &s.header, sizeof(s.header));
            s.bytes.resize(samples * a.channels * 2);
            memcpy(s.bytes.data(), pcm, s.bytes.size());
            s.header = {};
            s.header.lpData = s.bytes.data();
            s.header.dwBufferLength = s.bytes.size();
            if (waveOutPrepareHeader(a.device, &s.header, sizeof(s.header)) == MMSYSERR_NOERROR)
                waveOutWrite(a.device, &s.header, sizeof(s.header));
            break;
        }
    }
};
struct Stream
{
    ChiakiLog log{};
    ChiakiSession session{};
    ChiakiFfmpegDecoder decoder{};
    ChiakiOpusDecoder opus{};
    Audio audio;
    bool hasDecoder{}, hasSession{}, started{};
    Keys keys;
    std::mutex lock;
    std::condition_variable ready;
    AVFrame *latest{};
    double pts{};
    bool gap{};
    std::string failure;
    SwsContext *scale{};
    static void display(void *user, bool restricted)
    {
        if (!restricted)
            return;
        auto &s = *static_cast<Stream *>(user);
        std::lock_guard guard(s.lock);
        av_frame_free(&s.latest);
        s.failure =
            "The PS5 restricted Remote Play for this screen. Return to supported content and reconnect.";
        s.ready.notify_one();
    }
    static void video(ChiakiFfmpegDecoder *decoder, void *user)
    {
        auto &s = *static_cast<Stream *>(user);
        int32_t lost{};
        auto sample = chiaki_ffmpeg_decoder_pull_frame(decoder, &lost);
        if (!sample.frame)
            return;
        std::lock_guard guard(s.lock);
        s.gap = s.gap || lost || sample.recovered || s.latest;
        av_frame_free(&s.latest);
        s.latest = sample.frame;
        s.pts = sample.pts;
        s.ready.notify_one();
    }
    static void event(ChiakiEvent *e, void *user)
    {
        auto &s = *static_cast<Stream *>(user);
        if (e->type == CHIAKI_EVENT_LOGIN_PIN_REQUEST && !e->login_pin_request.pin_incorrect && *s.keys.login)
        {
            chiaki_session_set_login_pin(&s.session, reinterpret_cast<uint8_t *>(s.keys.login), 4);
            return;
        }
        std::lock_guard guard(s.lock);
        if (e->type == CHIAKI_EVENT_LOGIN_PIN_REQUEST)
            s.failure =
                "PS5 login passcode required or incorrect. Pair again with the console login passcode.";
        else if (e->type == CHIAKI_EVENT_QUIT && s.failure.empty())
            s.failure = chiaki_quit_reason_string(e->quit.reason);
        s.ready.notify_one();
    }
    void controller()
    {
        XINPUT_STATE x{};
        ChiakiControllerState state;
        chiaki_controller_state_set_idle(&state);
        if (XInputGetState(0, &x) == ERROR_SUCCESS)
        {
            const auto &g = x.Gamepad;
            const WORD buttons[] = {XINPUT_GAMEPAD_A,
                                    XINPUT_GAMEPAD_B,
                                    XINPUT_GAMEPAD_X,
                                    XINPUT_GAMEPAD_Y,
                                    XINPUT_GAMEPAD_DPAD_LEFT,
                                    XINPUT_GAMEPAD_DPAD_RIGHT,
                                    XINPUT_GAMEPAD_DPAD_UP,
                                    XINPUT_GAMEPAD_DPAD_DOWN,
                                    XINPUT_GAMEPAD_LEFT_SHOULDER,
                                    XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                    XINPUT_GAMEPAD_LEFT_THUMB,
                                    XINPUT_GAMEPAD_RIGHT_THUMB,
                                    XINPUT_GAMEPAD_START,
                                    XINPUT_GAMEPAD_BACK};
            for (unsigned i = 0; i < sizeof(buttons) / sizeof(*buttons); ++i)
                if (g.wButtons & buttons[i])
                    state.buttons |= 1u << i;
            state.left_x = g.sThumbLX;
            state.left_y = std::min(32767, -int(g.sThumbLY));
            state.right_x = g.sThumbRX;
            state.right_y = std::min(32767, -int(g.sThumbRY));
            state.l2_state = g.bLeftTrigger;
            state.r2_state = g.bRightTrigger;
        }
        chiaki_session_set_controller_state(&session, &state);
    }
    Stream()
    {
        chiaki_opus_decoder_init(&opus, &log);
    }
    ~Stream()
    {
        if (started)
        {
            chiaki_session_stop(&session);
            chiaki_session_join(&session);
        }
        if (hasSession)
            chiaki_session_fini(&session);
        if (hasDecoder)
            chiaki_ffmpeg_decoder_fini(&decoder);
        chiaki_opus_decoder_fini(&opus);
        av_frame_free(&latest);
        sws_freeContext(scale);
    }
};
} // namespace
API int ps_paired(const char *host)
{
    Keys k;
    return load(host, k);
}
API int ps_pair(const char *host, const uint8_t *account, unsigned pin, const char *login, char *error,
                int cap)
{
    if (!initialize())
    {
        snprintf(error, cap, "Could not initialize chiaki-ng.");
        return 0;
    }
    Registration result;
    ChiakiRegistInfo info{};
    info.target = CHIAKI_TARGET_PS5_1;
    info.host = host;
    info.pin = pin;
    memcpy(info.psn_account_id, account, 8);
    if (login && strlen(login) == 4)
        memcpy(result.keys.login, login, 4);
    ChiakiRegist registration{};
    ChiakiLog log{};
    auto status = chiaki_regist_start(&registration, &log, &info, registered, &result);
    if (status == CHIAKI_ERR_SUCCESS)
        chiaki_regist_fini(&registration); // joins the registration worker
    if (!result.ok)
    {
        snprintf(error, cap,
                 "Pairing failed. Check that the PS5 is awake, the account ID matches and the pairing code "
                 "is current.");
        return 0;
    }
    auto name = target(host);
    CREDENTIALA c{};
    c.Type = CRED_TYPE_GENERIC;
    c.TargetName = name.data();
    c.CredentialBlob = reinterpret_cast<BYTE *>(&result.keys);
    c.CredentialBlobSize = sizeof(result.keys);
    c.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteA(&c, 0))
    {
        snprintf(error, cap, "Windows could not store the PS5 registration.");
        return 0;
    }
    return 1;
}
API void *ps_open(const char *host, int fps, char *error, int cap)
{
    if (!initialize())
    {
        snprintf(error, cap, "Could not initialize chiaki-ng.");
        return nullptr;
    }
    auto s = std::make_unique<Stream>();
    if (!load(host, s->keys))
    {
        snprintf(error, cap, "Pair this PS5 first.");
        return nullptr;
    }
    ChiakiConnectInfo c{};
    c.ps5 = true;
    c.host = host;
    c.video_profile_auto_downgrade = true;
    c.enable_idr_on_fec_failure = true;
    c.packet_loss_max = 0.05;
    memcpy(c.regist_key, s->keys.registration, 16);
    memcpy(c.morning, s->keys.morning, 16);
    chiaki_connect_video_profile_preset(&c.video_profile, CHIAKI_VIDEO_RESOLUTION_PRESET_1080p,
                                        fps == 60 ? CHIAKI_VIDEO_FPS_PRESET_60 : CHIAKI_VIDEO_FPS_PRESET_30);
    c.video_profile.codec = CHIAKI_CODEC_H264;
    auto status = chiaki_ffmpeg_decoder_init(&s->decoder, &s->log, c.video_profile.codec, fps, nullptr,
                                             nullptr, Stream::video, s.get());
    if ((s->hasDecoder = status == CHIAKI_ERR_SUCCESS))
        status = chiaki_session_init(&s->session, &c, &s->log);
    if ((s->hasSession = s->hasDecoder && status == CHIAKI_ERR_SUCCESS))
    {
        ChiakiAudioSink sink{};
        chiaki_opus_decoder_set_cb(&s->opus, Audio::settings, Audio::frame, &s->audio);
        chiaki_opus_decoder_get_sink(&s->opus, &sink);
        chiaki_session_set_audio_sink(&s->session, &sink);
        ChiakiCtrlDisplaySink display{};
        display.user = s.get();
        display.cantdisplay_cb = Stream::display;
        chiaki_session_ctrl_set_display_sink(&s->session, &display);
        chiaki_session_set_event_cb(&s->session, Stream::event, s.get());
        chiaki_session_set_video_sample_cb(&s->session, chiaki_ffmpeg_decoder_video_sample_cb, &s->decoder);
        status = chiaki_session_start(&s->session);
        s->started = status == CHIAKI_ERR_SUCCESS;
    }
    if (!s->started)
    {
        snprintf(error, cap, "Open PS5 session: %s", chiaki_error_string(status));
        return nullptr;
    }
    return s.release();
}
API int ps_read(Stream *s, uint32_t *pixels, int capacity, int *w, int *h, double *pts, int *gap, char *error,
                int cap)
{
    if (s->started)
        s->controller();
    AVFrame *frame{};
    {
        std::unique_lock lock(s->lock);
        s->ready.wait_for(lock, std::chrono::milliseconds(16),
                          [&] { return s->latest || !s->failure.empty(); });
        if (!s->failure.empty())
        {
            snprintf(error, cap, "%s", s->failure.c_str());
            return -1;
        }
        frame = s->latest;
        s->latest = nullptr;
        if (!frame)
            return 0;
        *pts = s->pts;
        *gap = s->gap;
        s->gap = false;
    }
    *w = frame->width;
    *h = frame->height;
    int rows = 0;
    if (*w > 0 && *h > 0 && int64_t(*w) * (*h) <= capacity)
    {
        s->scale = sws_getCachedContext(s->scale, *w, *h, (AVPixelFormat)frame->format, *w, *h,
                                        AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
        uint8_t *dst[] = {reinterpret_cast<uint8_t *>(pixels), nullptr, nullptr, nullptr};
        int stride[] = {*w * 4, 0, 0, 0};
        if (s->scale)
            rows = sws_scale(s->scale, frame->data, frame->linesize, 0, *h, dst, stride);
    }
    av_frame_free(&frame);
    if (rows != *h || rows <= 0)
    {
        snprintf(error, cap, "Unable to convert the decoded PS5 frame.");
        return -1;
    }
    return 1;
}
API void ps_close(Stream *s)
{
    delete s;
}
