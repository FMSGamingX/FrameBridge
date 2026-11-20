// FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
// Exercise the same compressed-packet callback and latest-frame handoff used by a registered session.
#include "Client.cpp"
#include <fstream>
#include <iterator>
int main(int argc, char **argv)
{
    if (argc != 2 || !initialize())
        return 1;
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<uint8_t> packet{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    size_t size = packet.size();
    if (!size)
        return 2;
    packet.resize(size + AV_INPUT_BUFFER_PADDING_SIZE);
    Stream s;
    s.hasDecoder = chiaki_ffmpeg_decoder_init(&s.decoder, &s.log, CHIAKI_CODEC_H264, 30, nullptr, nullptr,
                                              Stream::video, &s) == CHIAKI_ERR_SUCCESS;
    if (!s.hasDecoder)
        return 3;
    std::vector<uint32_t> pixels(320 * 180);
    int w{}, h{}, gap{};
    double pts{};
    char error[256]{};
    for (int i = 0; i < 12; i++)
    {
        if (!chiaki_ffmpeg_decoder_video_sample_cb(packet.data(), size, 0, false, &s.decoder))
            return 4;
        auto result = ps_read(&s, pixels.data(), int(pixels.size()), &w, &h, &pts, &gap, error, sizeof error);
        if (result < 0)
            return 5;
        if (result == 0)
            continue;
        if (w != 320 || h != 180 || !std::all_of(pixels.begin(), pixels.end(), [](uint32_t p) {
                return ((p >> 16) & 255) > 235 && ((p >> 8) & 255) < 10 && (p & 255) < 10;
            }))
            return 6;
        Stream::display(&s, true);
        if (ps_read(&s, pixels.data(), int(pixels.size()), &w, &h, &pts, &gap, error, sizeof error) != -1)
            return 7;
        puts("PS5 H.264 fixture decoded to 320 x 180 BGRA; restriction callback stopped delivery.");
        return 0;
    }
    return 8;
}
