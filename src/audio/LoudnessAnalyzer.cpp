#include "LoudnessAnalyzer.hpp"
#include "FfmpegDecoder.hpp"
#include "FfmpegUtils.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
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
Loudness LoudnessAnalyzer::analyze(const QString &path, const QString &dir,
                                   const std::function<bool()> &cancelled)
{
    QFileInfo info(path);
    const auto identity = info.canonicalFilePath() + ":" + QString::number(info.size()) + ":" +
                          QString::number(info.lastModified().toMSecsSinceEpoch());
    const auto key = QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex();
    const auto cache = dir + "/" + key + ".json";
    QFile f(cache);
    Loudness result;
    if (f.open(QIODevice::ReadOnly))
    {
        const auto j = QJsonDocument::fromJson(f.readAll()).object();
        if (j.value("version").toInt() == 1)
        {
            result = {j.value("lufs").toDouble(-70), j.value("peak").toDouble(), true};
            if (std::isfinite(result.lufs) && std::isfinite(result.truePeak) && result.truePeak >= 0)
                return result;
        }
    }
    FfmpegDecoder decoder;
    QString error;
    if (!decoder.open(path, error))
        return {};
    AVFilterGraph *graph = avfilter_graph_alloc();
    if (!graph)
        return {};
    graph->nb_threads = 1;
    AVFilterContext *src = nullptr, *sink = nullptr;
    auto layout = avLayout(decoder.format());
    char name[256];
    av_channel_layout_describe(&layout, name, sizeof(name));
    av_channel_layout_uninit(&layout);
    const auto args = QString("sample_rate=%1:sample_fmt=flt:time_base=1/%1:channel_layout=%2")
                          .arg(decoder.format().sampleRate)
                          .arg(name)
                          .toUtf8();
    int ret = avfilter_graph_create_filter(&src, avfilter_get_by_name("abuffer"), "in", args.constData(),
                                           nullptr, graph);
    if (ret >= 0)
        ret = avfilter_graph_create_filter(&sink, avfilter_get_by_name("abuffersink"), "out", nullptr,
                                           nullptr, graph);
    AVFilterContext *meter = nullptr;
    if (ret >= 0)
        ret = avfilter_graph_create_filter(&meter, avfilter_get_by_name("ebur128"), "meter",
                                           "metadata=1:peak=true", nullptr, graph);
    if (ret >= 0)
        ret = avfilter_link(src, 0, meter, 0);
    if (ret >= 0)
        ret = avfilter_link(meter, 0, sink, 0);
    if (ret >= 0)
        ret = avfilter_graph_config(graph, nullptr);
    AVFrame *input = av_frame_alloc(), *out = av_frame_alloc();
    auto receive = [&]
    {
        int r = 0;
        while ((r = av_buffersink_get_frame(sink, out)) >= 0)
        {
            const auto *integrated = av_dict_get(out->metadata, "lavfi.r128.I", nullptr, 0),
                       *peak = av_dict_get(out->metadata, "lavfi.r128.true_peak", nullptr, 0);
            if (integrated)
            {
                result.lufs = QString::fromUtf8(integrated->value).toDouble();
                result.valid = std::isfinite(result.lufs);
            }
            if (peak)
                result.truePeak = QString::fromUtf8(peak->value).toDouble();
            av_frame_unref(out);
        }
        return r == AVERROR(EAGAIN) || r == AVERROR_EOF;
    };
    if (ret >= 0 && input && out)
    {
        AudioFrame frame;
        while (!cancelled() && (ret = decoder.read(frame, error)) > 0)
        {
            av_frame_unref(input);
            input->sample_rate = frame.format.sampleRate;
            input->format = AV_SAMPLE_FMT_FLT;
            input->ch_layout = avLayout(frame.format);
            input->nb_samples = frame.frames();
            input->pts = frame.pts;
            ret = av_frame_get_buffer(input, 0);
            if (ret < 0)
                break;
            std::memcpy(input->data[0], frame.samples.data(), frame.samples.size() * sizeof(float));
            ret = av_buffersrc_add_frame_flags(src, input, AV_BUFFERSRC_FLAG_KEEP_REF);
            if (ret < 0 || !receive())
                break;
        }
        if (ret == 0 && !cancelled())
        {
            ret = av_buffersrc_add_frame_flags(src, nullptr, 0);
            if (ret >= 0)
                receive();
        }
    }
    av_frame_free(&input);
    av_frame_free(&out);
    avfilter_graph_free(&graph);
    if (ret < 0 || cancelled() || !std::isfinite(result.truePeak))
        return {};
    if (result.valid)
    {
        QDir().mkpath(dir);
        QSaveFile saved(cache);
        if (saved.open(QIODevice::WriteOnly))
        {
            const auto bytes =
                QJsonDocument(QJsonObject{{"version", 1}, {"lufs", result.lufs}, {"peak", result.truePeak}})
                    .toJson();
            if (saved.write(bytes) == bytes.size())
                saved.commit();
        }
    }
    return result;
}
double LoudnessAnalyzer::gain(const Loudness &l, double target, double ceiling)
{
    if (!l.valid || l.lufs <= -69)
        return 0;
    double db = std::clamp(target - l.lufs, -24.0, 12.0);
    if (l.truePeak > 0)
        db = std::min(db, ceiling - 20 * std::log10(l.truePeak));
    return db;
}
} // namespace tonal::audio
