#pragma once
#include "IAudioDecoder.hpp"
#include <functional>
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwrContext;
namespace tonal::audio
{
class FfmpegDecoder final : public IAudioDecoder
{
  public:
    explicit FfmpegDecoder(bool network = false, std::function<bool()> cancelled = {})
        : m_network(network), m_cancelled(std::move(cancelled)) {}
    ~FfmpegDecoder() override;
    bool open(const QString &, QString &) override;
    int read(AudioFrame &, QString &) override;
    bool seek(double, QString &) override;
    AudioFormat format() const override
    {
        return m_format;
    }
    double duration() const override
    {
        return m_duration;
    }
    double replayGainDb() const override
    {
        return m_gain;
    }
    bool hasReplayGain() const override
    {
        return m_hasGain;
    }
    QString analysisPath() const override
    {
        return m_network ? QString() : m_path;
    }

  private:
    bool m_network = false;
    std::function<bool()> m_cancelled;
    void close();
    AVFormatContext *m_demux = nullptr;
    AVCodecContext *m_codec = nullptr;
    AVFrame *m_frame = nullptr;
    AVPacket *m_packet = nullptr;
    SwrContext *m_swr = nullptr;
    int m_stream = -1;
    bool m_eof = false, m_hasGain = false;
    AudioFormat m_format;
    double m_duration = 0, m_gain = 0;
    QString m_path;
    int64_t m_pts = 0, m_seekTarget = -1;
};
} // namespace tonal::audio
