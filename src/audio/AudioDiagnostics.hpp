#pragma once
#include "AudioFrame.hpp"
#include <QVariantMap>
#include <QVariantList>
#include <array>
#include <algorithm>
#include <cmath>

namespace tonal::audio
{
// Worker-owned counters. Values describe PCM before the one master-volume stage.
struct AudioDiagnostics
{
    uint64_t invalidSamples = 0, overFullScale = 0, blocks = 0;
    double peak = 0, averageDspUs = 0;
    QVariantList waveform, spectrum;
    void visualize(const AudioFrame &frame) {
        waveform.clear(); spectrum.clear();
        const int channels=int(frame.format.channels.size()), frames=frame.frames();
        if(channels<1 || frames<2)return;
        constexpr int N=256;
        std::array<double,N> mono{};
        for(int i=0;i<N;i++) {
            int row=std::min(frames-1,i);double value=0;
            for(int ch=0;ch<channels;ch++)value+=frame.samples[row*channels+ch];
            mono[i]=std::isfinite(value)?value/channels:0;
        }
        for(int i=0;i<64;i++)waveform<<std::clamp(mono[i*4],-1.,1.);
        for(int band=0;band<16;band++) {
            const int bin=std::clamp(int(std::round(std::pow(128.,band/15.))),1,128);
            double re=0,im=0;
            for(int i=0;i<N;i++) {double window=.5-.5*std::cos(6.283185307179586*i/(N-1)),angle=6.283185307179586*bin*i/N;re+=mono[i]*window*std::cos(angle);im-=mono[i]*window*std::sin(angle);}
            double magnitude=std::hypot(re,im)*4/N;
            spectrum<<std::clamp((20*std::log10(std::max(1e-6,magnitude))+60)/60,0.,1.);
        }
    }
    void observe(const AudioFrame &frame, double dspUs)
    {
        peak = 0;
        for (float value : frame.samples)
        {
            if (!std::isfinite(value))
                ++invalidSamples;
            else
            {
                peak = std::max(peak, std::abs(double(value)));
                if (std::abs(value) > 1)
                    ++overFullScale;
            }
        }
        ++blocks;
        averageDspUs += (dspUs - averageDspUs) / std::min<uint64_t>(blocks, 128);
    }
    QVariantMap values(const AudioFormat &format, bool hrtf, int underruns) const
    {
        return {{"sampleRate", format.sampleRate},
                {"channels", int(format.channels.size())},
                {"binaural", format.binaural},
                {"hrtfActive", hrtf},
                {"underruns", underruns},
                {"peakDbfs", peak > 0 ? 20 * std::log10(peak) : -120},
                {"invalidSamples", double(invalidSamples)},
                {"overFullScale", double(overFullScale)},
                {"averageDspUs", averageDspUs}, {"waveform",waveform}, {"spectrum",spectrum}};
    }
};
} // namespace tonal::audio
