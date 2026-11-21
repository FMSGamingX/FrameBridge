// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
#include "Rendering/Renderer.hpp"
#include <deque>
#include <mmsystem.h>
namespace fb
{
// One producer and one presentation worker. Only the capture queue can drop originals.
// Readers use source timestamps for file playback; live devices supply their own clock.
struct Stats
{
    int state = 1, width = 0, height = 0, activeGeneration = 0;
    int64_t inputs = 0, outputs = 0, generated = 0, dropped = 0, processed = 0, jobs = 0;
    double workMs = 0, generationMs = 0, latencyMs = 0, rate = 0, gpuTotalMs = 0;
    int64_t gpuJobs = 0;
    double pacingErrorMs = 0;
    int64_t intervals = 0;
    int hdr = 0;
};
struct Session
{
    Config config;
    bool nvidia = false;
    std::wstring model, key, backend = L"Starting", message;
    HWND hwnd;
    std::mutex lock;
    std::condition_variable cv;
    std::atomic_bool stop{false};
    std::atomic_int refresh;
    bool ended = false, rendererReady = false;
    std::shared_ptr<Frame> latest;
    std::deque<std::shared_ptr<Frame>> pending;
    std::vector<std::shared_ptr<Frame>> pool;
    Stats stats;
    std::thread producer, consumer;
    Session(HWND h, const Config &c, const wchar_t *k)
        : config(c), key(k ? k : L""), hwnd(h), refresh(c.refresh)
    {
    }
    ~Session()
    {
        cancel();
        if (producer.joinable())
            producer.join();
        if (consumer.joinable())
            consumer.join();
    }
    void cancel()
    {
        std::lock_guard g(lock);
        stop = true;
        cv.notify_all();
    }
    void fail(const std::exception &e)
    {
        std::lock_guard g(lock);
        message = wide(e.what());
        stats.state = 4;
        stop = true;
        cv.notify_all();
    }
    bool until(Time at)
    {
        std::unique_lock g(lock);
        cv.wait_until(g, at, [&] { return stop.load(); });
        return !stop;
    }
    void capture()
    {
        try
        {
            {
                std::unique_lock g(lock);
                cv.wait(g, [&] { return rendererReady || stop; });
                if (stop)
                    return;
            }
            Apartment com;
            MediaRuntime mf;
            auto src = openSource(config, key);
            Time epoch{}, sampleEpoch = Clock::now();
            int64_t tick = 0;
            double first = 0, last = 0;
            bool pacedCapture = config.kind == int(Kind::Pattern) || config.kind == int(Kind::Desktop);
            while (!stop)
            {
                std::shared_ptr<Frame> f;
                {
                    std::lock_guard g(lock);
                    for (auto &slot : pool)
                        if (slot.use_count() == 1)
                        {
                            f = slot;
                            break;
                        }
                    if (!f)
                    {
                        f = std::make_shared<Frame>();
                        pool.push_back(f);
                    }
                }
                if (pacedCapture)
                {
                    auto due = sampleEpoch + std::chrono::duration_cast<Clock::duration>(
                                                 std::chrono::duration<double>(tick++ / src->rate));
                    if (!until(due))
                        break;
                }
                f->discontinuity = false;
                if (!src->read(*f, stop))
                    break;
                if (epoch == Time{} || f->pts < last || f->pts - last > 2 || f->discontinuity)
                {
                    epoch = Clock::now();
                    first = f->pts;
                    f->discontinuity = true;
                }
                last = f->pts;
                if (config.kind == int(Kind::File))
                {
                    Time due = epoch + std::chrono::duration_cast<Clock::duration>(
                                           std::chrono::duration<double>(f->pts - first));
                    if (!until(due))
                        break;
                }
                f->arrival = Clock::now();
                {
                    std::unique_lock g(lock);
                    // A file can pause when presentation stalls. Live sources keep the drop-oldest policy.
                    if (config.kind == int(Kind::File) && !config.generate)
                    {
                        cv.wait(g, [&] { return stop || pending.size() < size_t(config.buffer); });
                        if (stop)
                            break;
                    }
                    stats.hdr = src->hdr;
                    stats.rate = src->rate;
                    stats.width = f->width;
                    stats.height = f->height;
                    ++stats.inputs;
                    if (pending.size() >= size_t(config.buffer))
                    {
                        pending.pop_front();
                        ++stats.dropped;
                        if (pending.empty())
                            f->discontinuity = true;
                        else
                            pending.front()->discontinuity = true;
                    }
                    pending.push_back(f);
                }
                cv.notify_all();
            }
        }
        catch (const std::exception &e)
        {
            fail(e);
        }
        {
            std::lock_guard g(lock);
            ended = true;
        }
        cv.notify_all();
    }
    void render()
    {
        timeBeginPeriod(1);
        try
        {
            Apartment com;
            std::wstring notice;
            auto r = makeRenderer(hwnd, config, notice, model, nvidia);
            {
                std::lock_guard g(lock);
                rendererReady = true;
                cv.notify_all();
                backend = r->name();
                if (!notice.empty() && stats.state != 4)
                    message = notice;
            }
            std::shared_ptr<Frame> prev;
            Time timeline{}, lastPresent{};
            double firstPts = 0, estimateMs = 1;
            while (!stop)
            {
                std::shared_ptr<Frame> cur;
                double rate;
                {
                    std::unique_lock g(lock);
                    cv.wait(g, [&] { return stop || ended || !pending.empty(); });
                    if (stop || pending.empty())
                        break;
                    cur = std::move(pending.front());
                    pending.pop_front();
                    if (config.kind == int(Kind::File) && !config.generate)
                        cv.notify_all();
                    rate = stats.rate;
                }
                bool eligible =
                    config.generate && rate > 0 && rate <= 60.1 && rate * 2 <= refresh.load() + 1.25;
                double dt = prev ? cur->pts - prev->pts : 0;
                bool mid = eligible && prev && !cur->discontinuity && prev->width == cur->width &&
                           prev->height == cur->height && dt > 0 && dt < 1.65 / rate;
                Time start = Clock::now();
                r->prepare(mid ? *prev : *cur, *cur, mid);
                Time prepared = Clock::now();
                auto delay = [](double seconds) {
                    return std::chrono::duration_cast<Clock::duration>(
                        std::chrono::duration<double>(seconds));
                };
                // Keep deadlines tied to source PTS. A bounded processing allowance
                // positions the midpoint and original on an evenly spaced timeline.
                if (!prev || cur->discontinuity ||
                    std::abs(ms(timeline + delay(cur->pts - firstPts), cur->arrival)) > 150)
                {
                    timeline = cur->arrival;
                    firstPts = cur->pts;
                }
                Time midpointDue = timeline + delay(cur->pts - firstPts);
                double half = rate > 0 ? .5 / rate : 0;
                if (eligible)
                    midpointDue += delay(config.buffer > 1 ? half : std::min(half, estimateMs * .001));
                Time due = midpointDue + (eligible ? delay(half) : Clock::duration::zero());
                bool midShown = false;
                Time midEnd{};
                if (mid && (prepared < due || config.buffer > 1))
                {
                    if (!until(midpointDue))
                        break;
                    midShown = r->present(*cur, true);
                    midEnd = Clock::now();
                    if (midShown)
                    {
                        std::lock_guard g(lock);
                        ++stats.outputs;
                        ++stats.generated;
                    }
                    if (config.buffer > 1 && Clock::now() > midpointDue + delay(.002))
                        due = Clock::now() + delay(half);
                }
                if (mid)
                    estimateMs =
                        std::clamp(.9 * estimateMs + .1 * (ms(start, prepared) + 1.), 1., half * 1000);
                if (!until(due))
                    break;
                bool shown = r->present(*cur, false);
                Time finish = Clock::now();
                {
                    std::lock_guard g(lock);
                    if (cur->discontinuity)
                        lastPresent = {};
                    auto record = [&](Time t) {
                        if (lastPresent != Time{} && rate > 0)
                        {
                            stats.pacingErrorMs +=
                                std::abs(ms(lastPresent, t) - 1000 / (rate * (eligible ? 2 : 1)));
                            ++stats.intervals;
                        }
                        lastPresent = t;
                    };
                    if (midShown)
                        record(midEnd);
                    if (shown)
                        record(finish);
                    double gpu = r->gpuMilliseconds();
                    if (gpu >= 0)
                    {
                        stats.gpuTotalMs += gpu;
                        ++stats.gpuJobs;
                    }
                    latest = cur;
                    if (stats.state != 4)
                        stats.state = 2;
                    stats.outputs += int(shown);
                    stats.dropped += int(mid && !midShown) + int(!shown);
                    stats.activeGeneration = eligible;
                    stats.jobs += int(mid);
                    stats.workMs += ms(start, prepared);
                    stats.generationMs += mid ? ms(start, prepared) : 0;
                    stats.latencyMs += ms(cur->arrival, finish);
                    ++stats.processed;
                }
                prev = std::move(cur);
            }
            std::lock_guard g(lock);
            if (stats.state != 4)
                stats.state = 3;
        }
        catch (const std::exception &e)
        {
            fail(e);
        }
        timeEndPeriod(1);
    }
};
} // namespace fb
using namespace fb;
static void *startSession(HWND hwnd, const Config *c, const wchar_t *key, const wchar_t *model, bool nvidia)
{
    try
    {
        if (!IsWindow(hwnd) || !c || c->buffer < 1 || c->buffer > 3)
            return nullptr;
        auto s = std::make_unique<Session>(hwnd, *c, key);
        s->nvidia = nvidia;
        if (model)
            s->model = model;
        s->consumer = std::thread(&Session::render, s.get());
        s->producer = std::thread(&Session::capture, s.get());
        return s.release();
    }
    catch (...)
    {
        return nullptr;
    }
}
FB void *fb_start_nvidia_flow(HWND hwnd, const Config *c, const wchar_t *key)
{
    return startSession(hwnd, c, key, nullptr, true);
}
FB void *fb_start_ai(HWND hwnd, const Config *c, const wchar_t *key, const wchar_t *model)
{
    return startSession(hwnd, c, key, model, false);
}
FB void *fb_start(HWND hwnd, const Config *c, const wchar_t *key)
{
    return fb_start_ai(hwnd, c, key, nullptr);
}
FB void fb_stats(Session *s, Stats *stats, wchar_t *backend, int bc, wchar_t *msg, int mc)
{
    if (!s)
        return;
    std::lock_guard g(s->lock);
    *stats = s->stats;
    copyText(backend, bc, s->backend);
    copyText(msg, mc, s->message);
}
FB void fb_cancel(Session *s)
{
    if (s)
        s->cancel();
}
FB void fb_close(Session *s)
{
    delete s;
}
FB int fb_preview(Session *s, uint32_t *out, int w, int h)
{
    if (!s || !out || w <= 0 || h <= 0 || w > 1024 || h > 1024)
        return 0;
    std::shared_ptr<Frame> f;
    {
        std::lock_guard g(s->lock);
        f = s->latest;
    }
    if (!f)
        return 0;
    double scale = std::min(double(w) / f->width, double(h) / f->height);
    int dw = std::max(1, int(f->width * scale)), dh = std::max(1, int(f->height * scale));
    std::fill_n(out, w * h, 0);
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++)
            out[(y + (h - dh) / 2) * w + x + (w - dw) / 2] =
                f->pixels[size_t(y * f->height / dh) * f->width + x * f->width / dw];
    return 1;
}
FB void fb_refresh_limit(Session *s, int hz)
{
    if (s)
        s->refresh = hz;
}
