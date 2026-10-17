#include "FfmpegDecoder.hpp"
#include "FfmpegUtils.hpp"
#include <QFileInfo>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <set>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/replaygain.h>
#include <libswresample/swresample.h>
}
namespace tonal::audio
{
FfmpegDecoder::~FfmpegDecoder()
{
    close();
}
void FfmpegDecoder::close()
{
    swr_free(&m_swr);
    av_packet_free(&m_packet);
    av_frame_free(&m_frame);
    avcodec_free_context(&m_codec);
    avformat_close_input(&m_demux);
    m_eof = false;
    m_hasGain = false;
    m_stream = -1;
    m_pts = 0;
    m_seekTarget = -1;
}
bool FfmpegDecoder::open(const QString &path, QString &error)
{
    close();
    if ((!m_network && !QFileInfo(path).isFile()) ||
        (m_network && (QUrl(path).scheme() != "https" || !QUrl(path).userInfo().isEmpty())))
    {
        error = "Arquivo local não encontrado";
        return false;
    }
    m_demux = avformat_alloc_context();
    if (!m_demux) { error = "Sem memória para demux"; return false; }
    m_demux->interrupt_callback = {[](void *opaque) -> int {
        auto *self = static_cast<FfmpegDecoder *>(opaque);
        return self->m_cancelled && self->m_cancelled();
    }, this};
    AVDictionary *options = nullptr;
    av_dict_set(&options, "protocol_whitelist", m_network ? "async,https,tls,tcp" : "file", 0);
    if (m_network) {
        av_dict_set(&options, "rw_timeout", "8000000", 0);
        av_dict_set(&options, "tls_verify", "1", 0);
        av_dict_set(&options, "reconnect", "1", 0);
        av_dict_set(&options, "reconnect_on_network_error", "1", 0);
        av_dict_set(&options, "reconnect_on_http_error", "429,5xx", 0);
        av_dict_set(&options, "reconnect_streamed", "1", 0);
        av_dict_set(&options, "reconnect_delay_max", "5", 0);
        av_dict_set(&options, "reconnect_delay_total_max", "25", 0);
    }
    const auto input = (m_network ? "async:" + path : path).toUtf8();
    int ret = avformat_open_input(&m_demux, input.constData(), nullptr, &options);
    av_dict_free(&options);
    if (ret >= 0)
        ret = avformat_find_stream_info(m_demux, nullptr);
    if (ret < 0)
    {
        error = avError(ret);
        close();
        return false;
    }
    // An IAMF element is not a conventional mix. Never silently play only one substream.
    std::set<unsigned> immersive;
    for (unsigned i = 0; i < m_demux->nb_stream_groups; ++i)
    {
        auto *g = m_demux->stream_groups[i];
        if (g->type == AV_STREAM_GROUP_PARAMS_IAMF_AUDIO_ELEMENT ||
            g->type == AV_STREAM_GROUP_PARAMS_IAMF_MIX_PRESENTATION)
            for (unsigned j = 0; j < g->nb_streams; ++j)
                immersive.insert(g->streams[j]->index);
    }
    for (unsigned i = 0; i < m_demux->nb_streams; ++i)
        if (m_demux->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !immersive.count(i))
        {
            m_stream = int(i);
            break;
        }
    if (m_stream < 0)
    {
        error = "IAMF no contêiner: sem trilha convencional alternativa. Use um bitstream .iamf compatível.";
        close();
        return false;
    }
    const auto *par = m_demux->streams[m_stream]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec)
    {
        error = "Decoder não disponível nesta compilação FFmpeg";
        close();
        return false;
    }
    m_codec = avcodec_alloc_context3(codec);
    if (!m_codec)
    {
        error = "Sem memória para decoder";
        close();
        return false;
    }
    ret = avcodec_parameters_to_context(m_codec, par);
    if (ret >= 0)
        ret = avcodec_open2(m_codec, codec, nullptr);
    if (ret < 0)
    {
        error = avError(ret);
        close();
        return false;
    }
    // Legacy PCM WAV often omits a channel mask. Mono/stereo have unambiguous
    // conventions; multichannel layouts must still be explicitly identified.
    if (m_codec->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC && m_codec->ch_layout.nb_channels <= 2)
    {
        const int n = m_codec->ch_layout.nb_channels;
        av_channel_layout_uninit(&m_codec->ch_layout);
        av_channel_layout_default(&m_codec->ch_layout, n);
    }
    if (m_codec->ch_layout.order != AV_CHANNEL_ORDER_NATIVE)
    {
        error = "Layout de canais personalizado não suportado; recusada reprodução com ordem ambígua";
        close();
        return false;
    }
    if (m_codec->sample_rate < 8000 || m_codec->sample_rate > 384000 || m_codec->ch_layout.nb_channels < 1 ||
        m_codec->ch_layout.nb_channels > 8)
    {
        error = "Taxa/layout de áudio não suportado";
        close();
        return false;
    }
    m_format.sampleRate = m_codec->sample_rate;
    m_format.channels.clear();
    m_format.binaural = false;
    for (int i = 0; i < m_codec->ch_layout.nb_channels; ++i)
    {
        const auto c = channel(av_channel_layout_channel_from_index(&m_codec->ch_layout, i));
        if (c == Channel::Unknown)
        {
            error = "Ordem de canais não suportada; arquivo preservado";
            close();
            return false;
        }
        m_format.channels.push_back(c);
    }
    m_duration = m_demux->duration == AV_NOPTS_VALUE ? 0 : double(m_demux->duration) / AV_TIME_BASE;
    ret = swr_alloc_set_opts2(&m_swr, &m_codec->ch_layout, AV_SAMPLE_FMT_FLT, m_codec->sample_rate,
                              &m_codec->ch_layout, m_codec->sample_fmt, m_codec->sample_rate, 0, nullptr);
    if (ret >= 0)
        ret = swr_init(m_swr);
    if (ret < 0)
    {
        error = avError(ret);
        close();
        return false;
    }
    auto *tag = av_dict_get(m_demux->streams[m_stream]->metadata, "replaygain_track_gain", nullptr, 0);
    if (!tag)
        tag = av_dict_get(m_demux->metadata, "replaygain_track_gain", nullptr, 0);
    if (tag)
    {
        bool ok = false;
        QString value = QString::fromUtf8(tag->value);
        value.remove("dB", Qt::CaseInsensitive);
        m_gain = value.trimmed().toDouble(&ok);
        m_hasGain = ok && std::isfinite(m_gain);
    }
    if (!m_hasGain)
    {
        const auto *side =
            av_packet_side_data_get(par->coded_side_data, par->nb_coded_side_data, AV_PKT_DATA_REPLAYGAIN);
        if (side && side->size >= sizeof(AVReplayGain))
        {
            const auto *rg = reinterpret_cast<const AVReplayGain *>(side->data);
            if (rg->track_gain != INT32_MIN)
            {
                m_gain = rg->track_gain / 100000.;
                m_hasGain = true;
            }
        }
    }
    m_path = path;
    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();
    if (!m_frame || !m_packet)
    {
        error = "Sem memória para decoder";
        close();
        return false;
    }
    return true;
}
int FfmpegDecoder::read(AudioFrame &out, QString &error)
{
    if (!m_codec)
    {
        error = "Decoder fechado";
        return -1;
    }
    for (;;)
    {
        int ret = avcodec_receive_frame(m_codec, m_frame);
        if (ret >= 0)
        {
            if (m_frame->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC && m_frame->ch_layout.nb_channels <= 2)
            {
                const int n = m_frame->ch_layout.nb_channels;
                av_channel_layout_uninit(&m_frame->ch_layout);
                av_channel_layout_default(&m_frame->ch_layout, n);
            }
            if (m_frame->sample_rate != m_format.sampleRate ||
                av_channel_layout_compare(&m_frame->ch_layout, &m_codec->ch_layout) != 0)
            {
                error = "Mudança de formato dentro da faixa não suportada";
                return -1;
            }
            out.format = m_format;
            const int count = swr_get_out_samples(m_swr, m_frame->nb_samples);
            out.samples.resize(size_t(count) * m_format.channels.size());
            uint8_t *dst = reinterpret_cast<uint8_t *>(out.samples.data());
            const auto **src = const_cast<const uint8_t **>(m_frame->extended_data);
            ret = swr_convert(m_swr, &dst, count, src, m_frame->nb_samples);
            int64_t pts = m_frame->best_effort_timestamp;
            if (pts != AV_NOPTS_VALUE)
                m_pts = av_rescale_q(pts, m_demux->streams[m_stream]->time_base,
                                     AVRational{1, m_format.sampleRate});
            av_frame_unref(m_frame);
            if (ret < 0)
            {
                error = avError(ret);
                return -1;
            }
            out.pts = m_pts;
            m_pts += ret;
            out.samples.resize(size_t(ret) * m_format.channels.size());
            if (m_seekTarget >= 0)
            {
                const auto skip = std::clamp<int64_t>(m_seekTarget - out.pts, 0, ret);
                out.samples.erase(out.samples.begin(), out.samples.begin() + skip * m_format.channels.size());
                out.pts += skip;
                if (out.samples.empty())
                    continue;
                m_seekTarget = -1;
            }
            if (ret)
                return 1;
            continue;
        }
        if (ret == AVERROR_EOF)
            return 0;
        if (ret != AVERROR(EAGAIN))
        {
            error = avError(ret);
            return -1;
        }
        if (m_eof)
        {
            error = "Decoder não drenou corretamente";
            return -1;
        }
        do
        {
            ret = av_read_frame(m_demux, m_packet);
            if (ret >= 0 && m_packet->stream_index != m_stream)
                av_packet_unref(m_packet);
            else
                break;
        } while (true);
        if (ret == AVERROR_EOF)
        {
            m_eof = true;
            ret = avcodec_send_packet(m_codec, nullptr);
        }
        else if (ret >= 0)
        {
            ret = avcodec_send_packet(m_codec, m_packet);
            av_packet_unref(m_packet);
        }
        if (ret < 0)
        {
            error = avError(ret);
            return -1;
        }
    }
}
bool FfmpegDecoder::seek(double seconds, QString &error)
{
    if (!m_demux || !std::isfinite(seconds) || seconds < 0)
        return false;
    int ret = av_seek_frame(m_demux, -1, int64_t(seconds * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
    if (ret < 0)
    {
        error = avError(ret);
        return false;
    }
    avcodec_flush_buffers(m_codec);
    swr_close(m_swr);
    ret = swr_init(m_swr);
    m_eof = false;
    m_pts = m_seekTarget = int64_t(seconds * m_format.sampleRate);
    if (ret < 0)
    {
        error = avError(ret);
        return false;
    }
    return true;
}
} // namespace tonal::audio
