#pragma once
#include "AudioFrame.hpp"
#include <AL/al.h>
#include <AL/alc.h>
#include <QString>
#include <chrono>
#include <vector>
namespace tonal::audio
{
class OpenALOutput
{
  public:
    ~OpenALOutput();
    // Loopback renders deterministically without opening the user's audio device.
    bool open(const AudioFormat &, bool requestHrtf, QString &error, bool loopback = false);
    bool setHrtf(bool requested, QString &error);
    std::vector<float> render(int frames);
    void close();
    bool enqueue(const AudioFrame &, QString &error);
    void collect();
    void clear();
    // Streaming producers must keep draining until their upstream pause takes
    // effect.
    void pause(bool, bool suspend = true);
    void setVolume(float value);
    int queued() const;
    double position() const;
    bool connected() const;
    bool hrtf() const
    {
        return m_hrtf;
    }
    bool hrtfSupported() const
    {
        return m_hrtfSupported;
    }
    int underruns() const
    {
        return m_underruns;
    }
    QString description() const;

  private:
    void rampTo(float gain);
    void updateGain();
    void silence();
    ALCdevice *m_device = nullptr;
    ALCcontext *m_context = nullptr;
    ALuint m_source = 0;
    AudioFormat m_format;
    ALenum m_alFormat = 0;
    std::vector<int> m_order;
    struct Buffer
    {
        ALuint id;
        int frames;
    };
    std::vector<Buffer> m_buffers;
    bool m_hrtf = false, m_hrtfSupported = false, m_paused = false, m_started = false;
    bool m_pauseRequested = false, m_ramping = false, m_loopback = false;
    bool m_suspendOnPause = true;
    float m_volume = 1, m_gain = 0, m_rampFrom = 0, m_rampTarget = 0;
    std::chrono::steady_clock::time_point m_rampStart;
    double m_renderTime = 0, m_rampRenderStart = 0;
    int m_underruns = 0;
    int64_t m_completed = 0, m_firstPts = 0;
};
} // namespace tonal::audio
