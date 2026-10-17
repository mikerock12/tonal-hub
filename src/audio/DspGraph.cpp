#include "DspGraph.hpp"
#include "FfmpegUtils.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}
namespace tonal::audio
{
DspGraph::~DspGraph()
{
    avfilter_graph_free(&m_graph);
}
bool DspGraph::configure(const AudioFormat &f, const AudioSettings &s, double gain, QString &error)
{
    avfilter_graph_free(&m_graph);
    m_source = m_sink = nullptr;
    m_format = f;
    m_target = s;
    m_gain = m_targetGain = s.enabled ? gain : 0;
    m_bass = s.enabled ? s.bass : 0;
    m_compressorMix = s.enabled && s.compressor ? 1 : 0;
    m_mix = m_targetMix = s.effectsActive(gain) ? 1.f : 0.f;
    m_ramp = 0;
    m_eqPresent = {};
    m_nextPts = 0;
    for (int i = 0; i < 10; ++i)
        m_eq[i] = s.enabled && s.eqEnabled ? s.eq[i] : 0;
    auto layout = avLayout(f);
    char name[256];
    av_channel_layout_describe(&layout, name, sizeof(name));
    av_channel_layout_uninit(&layout);
    m_graph = avfilter_graph_alloc();
    if (!m_graph)
    {
        error = "Sem memoria DSP";
        return false;
    }
    m_graph->nb_threads = 1;
    AVFilterContext *previous = nullptr;
    auto add =
        [&](const char *filter, const QByteArray &id, const QString &args, AVFilterContext **save = nullptr)
    {
        AVFilterContext *node = nullptr;
        const auto *implementation = avfilter_get_by_name(filter);
        if (!implementation)
        {
            error = QString("Filtro FFmpeg indisponível: %1").arg(filter);
            return false;
        }
        int ret = avfilter_graph_create_filter(&node, implementation, id.constData(),
                                               args.isEmpty() ? nullptr : args.toUtf8().constData(), nullptr,
                                               m_graph);
        if (ret >= 0 && previous)
            ret = avfilter_link(previous, 0, node, 0);
        if (ret < 0)
        {
            error = avError(ret);
            return false;
        }
        previous = node;
        if (save)
            *save = node;
        return true;
    };
    if (!add("abuffer", "in",
             QString("time_base=1/%1:sample_rate=%1:sample_fmt=flt:channel_layout=%2")
                 .arg(f.sampleRate)
                 .arg(name),
             &m_source))
        return false;
    if (!add("volume", "gain", QString("volume=%1dB").arg(m_gain, 0, 'f', 7)))
        return false;
    for (int i = 0; i < 10; ++i)
        if (AudioSettings::frequencies[i] < f.sampleRate * .49)
        {
            if (!add("equalizer", QByteArray("eq") + QByteArray::number(i),
                     QString("f=%1:t=q:w=1.4:g=%2").arg(AudioSettings::frequencies[i]).arg(m_eq[i])))
                return false;
            m_eqPresent[i] = true;
        }
    if (!add("bass", "bass", QString("g=%1:f=110:t=q:w=0.707").arg(m_bass)))
        return false;
    if (!add("acompressor", "compressor",
             QString("threshold=0.125:ratio=2:attack=20:release=250:makeup=1:mix=%1").arg(m_compressorMix)))
        return false;
    // Constant 5ms lookahead. Keep the graph warm during exact dry bypass.
    // Sliders use runtime commands with a 30ms ramp, not graph reconstruction.
    if (!add("alimiter", "limiter",
             QString("limit=%1:level=0:attack=5:release=50:latency=0")
                 .arg(std::pow(10., s.ceilingDb / 20), 0, 'f', 7)))
        return false;
    if (!add("aformat", "format",
             QString("sample_fmts=flt:sample_rates=%1:channel_layouts=%2").arg(f.sampleRate).arg(name)))
        return false;
    if (!add("abuffersink", "out", {}, &m_sink))
        return false;
    const int ret = avfilter_graph_config(m_graph, nullptr);
    if (ret < 0)
    {
        error = avError(ret);
        return false;
    }
    return true;
}
void DspGraph::update(const AudioSettings &s, double gain)
{
    m_target = s;
    m_targetGain = s.enabled ? gain : 0;
    m_targetMix = s.effectsActive(gain) ? 1.f : 0.f;
    m_ramp = std::max(1, m_format.sampleRate * 30 / 1000);
}
bool DspGraph::commands(int frames, QString &error)
{
    if (!m_ramp)
        return true;
    const double a = std::min(1., double(frames) / m_ramp);
    auto send = [&](const QByteArray &id, const char *command, const QString &value)
    {
        const int ret = avfilter_graph_send_command(m_graph, id.constData(), command,
                                                    value.toUtf8().constData(), nullptr, 0, 0);
        if (ret < 0)
        {
            error = avError(ret);
            return false;
        }
        return true;
    };
    auto ramp = [&](double &v, double target, const QByteArray &id, const char *key, bool db = false)
    {
        if (v == target)
            return true;
        v += (target - v) * a;
        return send(id, key, QString::number(v, 'f', 7) + (db ? "dB" : ""));
    };
    if (!ramp(m_gain, m_targetGain, "gain", "volume", true))
        return false;
    for (int i = 0; i < 10; ++i)
        if (m_eqPresent[i] && !ramp(m_eq[i], m_target.enabled && m_target.eqEnabled ? m_target.eq[i] : 0,
                                    QByteArray("eq") + QByteArray::number(i), "gain"))
            return false;
    if (!ramp(m_bass, m_target.enabled ? m_target.bass : 0, "bass", "gain"))
        return false;
    if (!ramp(m_compressorMix, m_target.enabled && m_target.compressor ? 1 : 0, "compressor", "mix"))
        return false;
    if (!send("limiter", "limit", QString::number(std::pow(10., m_target.ceilingDb / 20), 'f', 7)))
        return false;
    m_ramp = std::max(0, m_ramp - frames);
    return true;
}
bool DspGraph::process(const AudioFrame &in, AudioFrame &out, QString &error)
{
    out.format = in.format;
    out.pts = in.pts;
    out.samples.clear();
    const auto channels = in.format.channels.size();
    for (int offset = 0; offset < in.frames(); offset += 256)
    {
        const int count = std::min(256, in.frames() - offset);
        if (!commands(count, error))
            return false;
        AVFrame *frame = av_frame_alloc();
        if (!frame)
        {
            error = "Sem memoria PCM";
            return false;
        }
        frame->format = AV_SAMPLE_FMT_FLT;
        frame->sample_rate = in.format.sampleRate;
        frame->nb_samples = count;
        frame->ch_layout = avLayout(in.format);
        frame->pts = in.pts + offset;
        int ret = av_frame_get_buffer(frame, 0);
        if (ret >= 0)
        {
            std::memcpy(frame->data[0], in.samples.data() + size_t(offset) * channels,
                        size_t(count) * channels * sizeof(float));
            ret = av_buffersrc_add_frame_flags(m_source, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        }
        av_frame_free(&frame);
        if (ret < 0)
        {
            error = avError(ret);
            return false;
        }
        AudioFrame wet;
        if (!receive(wet, error))
            return false;
        if (wet.frames() != count)
        {
            error = "DSP alterou o tamanho do bloco durante streaming";
            return false;
        }
        for (int f = 0; f < count; ++f)
        {
            const float step = 1.f / std::max(1, in.format.sampleRate * 30 / 1000);
            if (m_mix < m_targetMix)
                m_mix = std::min(m_targetMix, m_mix + step);
            else if (m_mix > m_targetMix)
                m_mix = std::max(m_targetMix, m_mix - step);
            for (size_t c = 0; c < channels; ++c)
            {
                const auto i = size_t(f) * channels + c;
                const float dry = in.samples[(size_t(offset) + f) * channels + c];
                out.samples.push_back(m_mix == 0   ? dry
                                      : m_mix == 1 ? wet.samples[i]
                                                   : dry * (1 - m_mix) + wet.samples[i] * m_mix);
            }
        }
    }
    m_nextPts = in.pts + in.frames();
    return true;
}
bool DspGraph::drain(AudioFrame &out, QString &error)
{
    out.format = m_format;
    out.samples.clear();
    if (m_mix > 0)
    {
        AudioFrame zeros;
        zeros.format = m_format;
        zeros.pts = m_nextPts;
        zeros.samples.resize(size_t(m_format.sampleRate * 5 / 1000) * m_format.channels.size());
        if (!process(zeros, out, error))
            return false;
    }
    const int ret = av_buffersrc_add_frame_flags(m_source, nullptr, 0);
    if (ret < 0 && ret != AVERROR_EOF)
    {
        error = avError(ret);
        return false;
    }
    AudioFrame tail;
    if (!receive(tail, error))
        return false;
    out.samples.insert(out.samples.end(), tail.samples.begin(), tail.samples.end());
    return true;
}
bool DspGraph::receive(AudioFrame &out, QString &error)
{
    out.samples.clear();
    out.format = m_format;
    AVFrame *f = av_frame_alloc();
    if (!f)
    {
        error = "Sem memoria DSP";
        return false;
    }
    int ret = 0;
    bool first = true;
    while ((ret = av_buffersink_get_frame(m_sink, f)) >= 0)
    {
        if (first)
        {
            out.pts = f->pts;
            first = false;
        }
        auto p = reinterpret_cast<const float *>(f->data[0]);
        out.samples.insert(out.samples.end(), p, p + size_t(f->nb_samples) * f->ch_layout.nb_channels);
        av_frame_unref(f);
    }
    av_frame_free(&f);
    if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
    {
        error = avError(ret);
        return false;
    }
    return true;
}
} // namespace tonal::audio
