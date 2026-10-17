#pragma once
#include "FfmpegDecoder.hpp"
#include "IAudioDecoder.hpp"
#include <functional>
namespace tonal::audio
{
// libiamf at the locked revision invokes OAR itself. Its PCM is FINAL binaural.
// Render once into an atomic cache, then reuse FFmpeg's reliable PCM seek path.
class IamfDecoderAdapter final : public IAudioDecoder
{
  public:
    explicit IamfDecoderAdapter(std::function<bool()> cancelled = [] { return false; })
        : m_cancelled(std::move(cancelled))
    {
    }
    static bool available();
    static bool renderBinaural(
        const QString &input, const QString &wav, QString &error,
        const std::function<bool()> &cancelled = [] { return false; });
    bool open(const QString &, QString &) override;
    int read(AudioFrame &frame, QString &error) override;
    bool seek(double seconds, QString &error) override
    {
        return m_pcm.seek(seconds, error);
    }
    AudioFormat format() const override
    {
        auto f = m_pcm.format();
        f.binaural = true;
        return f;
    }
    double duration() const override
    {
        return m_pcm.duration();
    }
    QString analysisPath() const override
    {
        return m_pcm.analysisPath();
    }

  private:
    FfmpegDecoder m_pcm;
    std::function<bool()> m_cancelled;
};
} // namespace tonal::audio
