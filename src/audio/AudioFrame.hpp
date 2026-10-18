#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <vector>

namespace tonal::audio
{
enum class Channel
{
    FL,
    FR,
    FC,
    LFE,
    BL,
    BR,
    SL,
    SR,
    Unknown
};
struct AudioFormat
{
    int sampleRate = 44100;
    std::vector<Channel> channels{Channel::FL, Channel::FR};
    bool binaural = false;
    bool operator==(const AudioFormat &other) const
    {
        return sampleRate == other.sampleRate && channels == other.channels && binaural == other.binaural;
    }
};
struct AudioFrame
{
    AudioFormat format;
    int64_t pts = 0;            // frames in the source sample-rate time base
    std::vector<float> samples; // packed float32, in format.channels order
    int frames() const
    {
        return format.channels.empty() ? 0 : int(samples.size() / format.channels.size());
    }
    bool validPcm() const
    {
        // Float PCM may legitimately exceed 0 dBFS before limiting. Allow
        // ample headroom (+30 dB), but reject broken byte streams/NaN/Inf.
        return !format.channels.empty() && samples.size() % format.channels.size() == 0 &&
               std::all_of(samples.begin(), samples.end(), [](float v) {
                   return std::isfinite(v) && std::abs(v) <= 32.f;
               });
    }
};
} // namespace tonal::audio
