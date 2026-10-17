#pragma once
#include <QString>
#include <functional>
namespace tonal::audio
{
struct Loudness
{
    double lufs = -70, truePeak = 0;
    bool valid = false;
};
class LoudnessAnalyzer
{
  public:
    static Loudness analyze(const QString &path, const QString &cacheDir,
                            const std::function<bool()> &cancelled);
    static double gain(const Loudness &, double targetLufs, double ceilingDb);
};
} // namespace tonal::audio
