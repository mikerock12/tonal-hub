#pragma once

#include <cstdint>
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
};
} // namespace tonal::audio
