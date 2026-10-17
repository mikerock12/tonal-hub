#include "IamfDecoderAdapter.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtEndian>
#include <cstring>
#include <limits>
#include <vector>
#ifdef TONAL_WITH_IAMF
extern "C"
{
#include <IAMF_decoder.h>
}
#endif
namespace tonal::audio
{
bool IamfDecoderAdapter::available()
{
#ifdef TONAL_WITH_IAMF
    return true;
#else
    return false;
#endif
}
bool IamfDecoderAdapter::renderBinaural(const QString &path, const QString &wav, QString &error,
                                        const std::function<bool()> &cancelled)
{
#ifndef TONAL_WITH_IAMF
    Q_UNUSED(path)
    Q_UNUSED(wav)
    Q_UNUSED(cancelled)
    error = "IAMF indisponível: decoder/OAR não incluídos nesta compilação";
    return false;
#else
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        error = file.errorString();
        return false;
    }
    auto handle = IAMF_decoder_open();
    if (!handle)
    {
        error = "Falha ao criar decoder IAMF";
        return false;
    }
    struct Cleanup
    {
        IAMF_DecoderHandle handle;
        ~Cleanup()
        {
            IAMF_decoder_close(handle);
        }
    } cleanup{handle};
    if (IAMF_decoder_output_layout_set_binaural(handle) != IAMF_OK ||
        IAMF_decoder_set_bit_depth(handle, 32) != IAMF_OK ||
        IAMF_decoder_set_sampling_rate(handle, 48000) != IAMF_OK)
    {
        error = "Decoder sem renderização binaural 48 kHz";
        return false;
    }
    // One normalization/limiter only: shared DSP controls sample peaks; OAR retains mix metadata.
    IAMF_decoder_set_normalization_loudness(handle, 0);
    IAMF_decoder_peak_limiter_enable(handle, 0);
    IAMF_decoder_enable_head_tracking(handle, 0);
    QByteArray pending;
    bool configured = false;
    std::vector<int32_t> pcm;
    uint64_t bytes = 0;
    QSaveFile out(wav);
    if (!out.open(QIODevice::WriteOnly))
    {
        error = out.errorString();
        return false;
    }
    out.write(QByteArray(44, '\0'));
    int drains = 0;
    while (!cancelled())
    {
        if (pending.size() < 65536 && !file.atEnd())
            pending += file.read(65536);
        if (!configured)
        {
            uint32_t used = 0;
            const int ret =
                IAMF_decoder_configure(handle, reinterpret_cast<const uint8_t *>(pending.constData()),
                                       uint32_t(pending.size()), &used);
            if (used > uint32_t(pending.size()))
            {
                error = "Decoder IAMF consumiu tamanho inválido";
                return false;
            }
            pending.remove(0, used);
            if (ret == IAMF_ERR_BUFFER_TOO_SMALL)
            {
                if (file.atEnd() || pending.size() > 4 * 1024 * 1024)
                {
                    error = "Descritores IAMF truncados ou excessivos";
                    return false;
                }
                pending += file.read(65536);
                continue;
            }
            if (ret != IAMF_OK)
            {
                error = QString("IAMF: perfil/codec/descritor não suportado (%1)").arg(ret);
                return false;
            }
            auto *info = IAMF_decoder_get_stream_info(handle);
            if (!info || info->max_frame_size == 0 || info->max_frame_size > 65536)
            {
                if (info)
                    IAMF_decoder_free_stream_info(info);
                error = "Frame IAMF inválido";
                return false;
            }
            pcm.resize(size_t(info->max_frame_size) * 2);
            IAMF_decoder_free_stream_info(info);
            configured = true;
        }
        const bool drain = pending.isEmpty() && file.atEnd();
        uint32_t used = 0;
        const int ret = IAMF_decoder_decode(
            handle, drain ? nullptr : reinterpret_cast<const uint8_t *>(pending.constData()),
            drain ? 0 : int(pending.size()), &used, pcm.data());
        if (used > uint32_t(pending.size()))
        {
            error = "Tamanho IAMF inválido";
            return false;
        }
        pending.remove(0, used);
        if (ret > 0)
        {
            if (size_t(ret) * 2 > pcm.size())
            {
                error = "Frame IAMF excessivo";
                return false;
            }
            const qint64 n = qint64(ret) * 8;
            if (bytes + uint64_t(n) > std::numeric_limits<uint32_t>::max() - 36)
            {
                error = "IAMF muito longo para cache WAV";
                return false;
            }
            if (out.write(reinterpret_cast<const char *>(pcm.data()), n) != n)
            {
                error = out.errorString();
                return false;
            }
            bytes += n;
        }
        if (drain)
        {
            if (ret <= 0 || ++drains > 64)
                break;
            continue;
        }
        if (ret < 0 && ret != IAMF_ERR_BUFFER_TOO_SMALL && ret != IAMF_ERR_PENDING)
        {
            error = QString("IAMF inválido ou incompatível (%1)").arg(ret);
            return false;
        }
        if (!used)
        {
            if (file.atEnd())
            {
                if (!pending.isEmpty())
                {
                    error = "IAMF truncado: pacote final incompleto";
                    return false;
                }
                break;
            }
            if (pending.size() > 4 * 1024 * 1024)
            {
                error = "Pacote IAMF excessivo";
                return false;
            }
            pending += file.read(65536);
        }
    }
    if (cancelled())
    {
        error = "Renderização IAMF cancelada";
        return false;
    }
    if (!bytes)
    {
        error = "IAMF não produziu áudio binaural";
        return false;
    }
    QByteArray header(44, '\0');
    auto put16 = [&](int off, quint16 v) { qToLittleEndian(v, header.data() + off); };
    auto put32 = [&](int off, quint32 v) { qToLittleEndian(v, header.data() + off); };
    std::memcpy(header.data(), "RIFF", 4);
    put32(4, quint32(bytes + 36));
    std::memcpy(header.data() + 8, "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1);
    put16(22, 2);
    put32(24, 48000);
    put32(28, 48000 * 8);
    put16(32, 8);
    put16(34, 32);
    std::memcpy(header.data() + 36, "data", 4);
    put32(40, quint32(bytes));
    if (!out.seek(0) || out.write(header) != 44 || !out.commit())
    {
        error = out.errorString();
        return false;
    }
    return true;
#endif
}
bool IamfDecoderAdapter::open(const QString &path, QString &error)
{
    if (!available())
    {
        error = "IAMF indisponível nesta compilação";
        return false;
    }
    QFileInfo f(path);
    const auto id = f.canonicalFilePath() + ":" + QString::number(f.size()) + ":" +
                    QString::number(f.lastModified().toMSecsSinceEpoch()) +
                    ":libiamf-b276f437-oar-ca14b543-binaural-v1";
    const auto dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/iamf-binaural";
    QDir().mkpath(dir);
    const auto output =
        dir + "/" + QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).toHex() + ".wav";
    if (!QFileInfo::exists(output) && !renderBinaural(path, output, error, m_cancelled))
        return false;
    return m_pcm.open(output, error);
}
int IamfDecoderAdapter::read(AudioFrame &frame, QString &error)
{
    const int ret = m_pcm.read(frame, error);
    frame.format.binaural = true;
    return ret;
}
} // namespace tonal::audio
