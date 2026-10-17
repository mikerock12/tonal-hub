#pragma once
#include "AudioFrame.hpp"
#include <QString>
extern "C"
{
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
}
namespace tonal::audio
{
inline QString avError(int code)
{
    char b[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(code, b, sizeof(b));
    return QString::fromUtf8(b);
}
inline AVChannel avChannel(Channel c)
{
    switch (c)
    {
    case Channel::FL:
        return AV_CHAN_FRONT_LEFT;
    case Channel::FR:
        return AV_CHAN_FRONT_RIGHT;
    case Channel::FC:
        return AV_CHAN_FRONT_CENTER;
    case Channel::LFE:
        return AV_CHAN_LOW_FREQUENCY;
    case Channel::BL:
        return AV_CHAN_BACK_LEFT;
    case Channel::BR:
        return AV_CHAN_BACK_RIGHT;
    case Channel::SL:
        return AV_CHAN_SIDE_LEFT;
    case Channel::SR:
        return AV_CHAN_SIDE_RIGHT;
    default:
        return AV_CHAN_UNKNOWN;
    }
}
inline Channel channel(AVChannel c)
{
    switch (c)
    {
    case AV_CHAN_FRONT_LEFT:
        return Channel::FL;
    case AV_CHAN_FRONT_RIGHT:
        return Channel::FR;
    case AV_CHAN_FRONT_CENTER:
        return Channel::FC;
    case AV_CHAN_LOW_FREQUENCY:
        return Channel::LFE;
    case AV_CHAN_BACK_LEFT:
        return Channel::BL;
    case AV_CHAN_BACK_RIGHT:
        return Channel::BR;
    case AV_CHAN_SIDE_LEFT:
        return Channel::SL;
    case AV_CHAN_SIDE_RIGHT:
        return Channel::SR;
    default:
        return Channel::Unknown;
    }
}
inline AVChannelLayout avLayout(const AudioFormat &f)
{
    AVChannelLayout l{};
    uint64_t mask = 0;
    for (auto c : f.channels)
    {
        const auto a = avChannel(c);
        if (a < 0 || a >= 64)
            return l;
        mask |= uint64_t(1) << a;
    }
    av_channel_layout_from_mask(&l, mask);
    return l;
}
} // namespace tonal::audio
