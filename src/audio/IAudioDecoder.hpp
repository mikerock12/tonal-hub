#pragma once
#include "AudioFrame.hpp"
#include <QString>
namespace tonal::audio
{
class IAudioDecoder
{
  public:
    virtual ~IAudioDecoder() = default;
    virtual bool open(const QString &path, QString &error) = 0;
    // 1: frame; 0: EOF; -1: controlled error
    virtual int read(AudioFrame &, QString &error) = 0;
    virtual bool seek(double seconds, QString &error) = 0;
    virtual AudioFormat format() const = 0;
    virtual double duration() const = 0;
    virtual double replayGainDb() const
    {
        return 0;
    }
    virtual bool hasReplayGain() const
    {
        return false;
    }
    virtual QString analysisPath() const
    {
        return {};
    }
};
} // namespace tonal::audio
