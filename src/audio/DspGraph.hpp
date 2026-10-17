#pragma once
#include "AudioFrame.hpp"
#include "AudioSettings.hpp"
#include <QString>
#include <memory>
struct AVFilterGraph;
struct AVFilterContext;
namespace tonal::audio
{
class DspGraph
{
  public:
    DspGraph() = default;
    ~DspGraph();
    bool configure(const AudioFormat &, const AudioSettings &, double gainDb, QString &error);
    void update(const AudioSettings &, double gainDb);
    bool process(const AudioFrame &in, AudioFrame &out, QString &error);
    bool drain(AudioFrame &out, QString &error);

  private:
    bool receive(AudioFrame &, QString &);
    bool commands(int frames, QString &error);
    AVFilterGraph *m_graph = nullptr;
    AVFilterContext *m_source = nullptr, *m_sink = nullptr;
    AudioFormat m_format;
    AudioSettings m_target;
    std::array<double, 10> m_eq{};
    std::array<bool, 10> m_eqPresent{};
    double m_gain = 0, m_targetGain = 0, m_bass = 0, m_compressorMix = 0;
    float m_mix = 0, m_targetMix = 0;
    int m_ramp = 0;
    int64_t m_nextPts = 0;
};
} // namespace tonal::audio
