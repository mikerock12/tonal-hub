#include "OpenALOutput.hpp"
#include <AL/alext.h>
#include <algorithm>
#include <numbers>
#include <thread>
namespace tonal::audio
{
OpenALOutput::~OpenALOutput()
{
    close();
}
bool OpenALOutput::open(const AudioFormat &format, bool requested, QString &error, bool loopback)
{
    close();
    m_format = format;
    const auto n = format.channels.size();
    if (n != 1 && n != 2 && n != 4 && n != 6 && n != 8)
    {
        error = "Layout de saída não suportado (mono, estéreo, quad, 5.1 ou 7.1)";
        return false;
    }
    m_loopback = loopback;
    if (loopback)
    {
        auto openLoopback = reinterpret_cast<LPALCLOOPBACKOPENDEVICESOFT>(
            alcGetProcAddress(nullptr, "alcLoopbackOpenDeviceSOFT"));
        m_device = openLoopback ? openLoopback(nullptr) : nullptr;
    }
    else
        m_device = alcOpenDevice(nullptr);
    if (!m_device)
    {
        error = "Nenhum dispositivo de áudio disponível";
        return false;
    }
    m_hrtfSupported = alcIsExtensionPresent(m_device, "ALC_SOFT_HRTF") == ALC_TRUE;
    const bool want = requested && !format.binaural;
    std::vector<ALCint> attrs;
    if (loopback)
        attrs = {ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT,
                 ALC_FLOAT_SOFT,           ALC_FREQUENCY,   format.sampleRate};
    if (m_hrtfSupported)
        attrs.insert(attrs.end(), {ALC_HRTF_SOFT, want ? ALC_TRUE : ALC_FALSE});
    attrs.push_back(0);
    m_context = alcCreateContext(m_device, attrs.data());
    if (!m_context || !alcMakeContextCurrent(m_context))
    {
        error = "Não foi possível criar a saída OpenAL";
        close();
        return false;
    }
    if (m_hrtfSupported)
    {
        ALCint active = 0;
        alcGetIntegerv(m_device, ALC_HRTF_SOFT, 1, &active);
        m_hrtf = active == ALC_TRUE;
    }
    if (format.binaural && m_hrtf)
    {
        error = "Backend não desativou HRTF: recusada espacialização duplicada";
        close();
        return false;
    }
    if (!alIsExtensionPresent("AL_EXT_FLOAT32") || (n > 2 && !alIsExtensionPresent("AL_EXT_MCFORMATS")))
    {
        error = "Dispositivo sem formato PCM float/multicanal necessário";
        close();
        return false;
    }
    std::vector<Channel> expected;
    if (n == 1)
    {
        expected = {Channel::FC};
        m_alFormat = AL_FORMAT_MONO_FLOAT32;
    }
    if (n == 2)
    {
        expected = {Channel::FL, Channel::FR};
        m_alFormat = AL_FORMAT_STEREO_FLOAT32;
    }
    if (n == 4)
    {
        expected = {Channel::FL, Channel::FR, Channel::BL, Channel::BR};
        m_alFormat = AL_FORMAT_QUAD32;
    }
    if (n == 6)
    {
        expected = {Channel::FL, Channel::FR, Channel::FC, Channel::LFE, Channel::BL, Channel::BR};
        m_alFormat = AL_FORMAT_51CHN32;
    }
    if (n == 8)
    {
        expected = {Channel::FL, Channel::FR, Channel::FC, Channel::LFE,
                    Channel::BL, Channel::BR, Channel::SL, Channel::SR};
        m_alFormat = AL_FORMAT_71CHN32;
    }
    m_order.clear();
    for (auto c : expected)
    {
        auto it = std::find(format.channels.begin(), format.channels.end(), c);
        // OpenAL 5.1 speaker slots carry the sole surround pair, including
        // FFmpeg 5.1(side).
        if (it == format.channels.end() && n == 6)
        {
            if (c == Channel::BL)
                it = std::find(format.channels.begin(), format.channels.end(), Channel::SL);
            if (c == Channel::BR)
                it = std::find(format.channels.begin(), format.channels.end(), Channel::SR);
        }
        if (it == format.channels.end())
        {
            error = "Ordem de canais incompatível com OpenAL";
            close();
            return false;
        }
        m_order.push_back(int(it - format.channels.begin()));
    }
    alGenSources(1, &m_source);
    alSourcei(m_source, AL_SOURCE_RELATIVE, AL_TRUE);
    // Non-mono sources at the listener retain each virtual speaker's direction.
    // Placing them in front with zero spread collapses the channels to one point.
    alSource3f(m_source, AL_POSITION, 0, 0, n == 1 ? -1.f : 0.f);
    alSourcef(m_source, AL_ROLLOFF_FACTOR, 0);
    alSourcef(m_source, AL_GAIN, 0);
    if (n == 2 && alIsExtensionPresent("AL_SOFT_direct_channels"))
        alSourcei(m_source, AL_DIRECT_CHANNELS_SOFT, want && m_hrtf ? AL_FALSE : AL_TRUE);
    if (n == 2 && alIsExtensionPresent("AL_EXT_STEREO_ANGLES"))
    {
        const ALfloat angles[] = {std::numbers::pi_v<float> / 6, -std::numbers::pi_v<float> / 6};
        alSourcefv(m_source, AL_STEREO_ANGLES, angles);
    }
    m_hrtfSupported = m_hrtfSupported && alIsExtensionPresent("AL_SOFT_source_spatialize");
    if (alIsExtensionPresent("AL_SOFT_source_spatialize"))
        alSourcei(m_source, AL_SOURCE_SPATIALIZE_SOFT, want && m_hrtf ? AL_TRUE : AL_FALSE);
    if (alGetError() != AL_NO_ERROR)
    {
        error = "Falha ao configurar fonte OpenAL";
        close();
        return false;
    }
    return true;
}
bool OpenALOutput::setHrtf(bool requested, QString &error)
{
    if (!m_device || !m_source)
        return false;
    const bool want = requested && !m_format.binaural;
    if (!want && !m_hrtf)
        return true;
    auto reset = reinterpret_cast<LPALCRESETDEVICESOFT>(alcGetProcAddress(m_device, "alcResetDeviceSOFT"));
    if (!reset || !m_hrtfSupported)
    {
        error = "Saída sem suporte à troca de HRTF";
        return false;
    }
    // Reset the device's renderer while retaining source, playhead and queued PCM.
    silence();
    std::vector<ALCint> attrs = {ALC_HRTF_SOFT, want ? ALC_TRUE : ALC_FALSE};
    if (m_loopback)
        attrs.insert(attrs.end(), {ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT,
                                   ALC_FLOAT_SOFT, ALC_FREQUENCY, m_format.sampleRate});
    attrs.push_back(0);
    if (!reset(m_device, attrs.data()))
    {
        error = "Não foi possível trocar o modo HRTF";
        return false;
    }
    ALCint active = ALC_FALSE;
    alcGetIntegerv(m_device, ALC_HRTF_SOFT, 1, &active);
    m_hrtf = active == ALC_TRUE;
    if (m_format.binaural && m_hrtf)
    {
        error = "Backend não desativou HRTF: recusada espacialização duplicada";
        return false;
    }
    if (m_format.channels.size() == 2 && alIsExtensionPresent("AL_SOFT_direct_channels"))
        alSourcei(m_source, AL_DIRECT_CHANNELS_SOFT, want && m_hrtf ? AL_FALSE : AL_TRUE);
    alSourcei(m_source, AL_SOURCE_SPATIALIZE_SOFT, want && m_hrtf ? AL_TRUE : AL_FALSE);
    rampTo(m_pauseRequested ? 0.f : 1.f);
    if (alGetError() != AL_NO_ERROR)
    {
        error = "Falha ao trocar a espacialização OpenAL";
        return false;
    }
    return true;
}
void OpenALOutput::clear()
{
    if (!m_source)
        return;
    silence();
    alSourceStop(m_source);
    ALint n = 0;
    alGetSourcei(m_source, AL_BUFFERS_QUEUED, &n);
    while (n-- > 0)
    {
        ALuint id;
        alSourceUnqueueBuffers(m_source, 1, &id);
        alDeleteBuffers(1, &id);
    }
    m_buffers.clear();
    m_completed = 0;
    m_firstPts = 0;
    m_started = false;
    m_gain = 0;
    m_ramping = false;
    alSourcef(m_source, AL_GAIN, 0);
}
void OpenALOutput::close()
{
    if (m_context)
    {
        alcMakeContextCurrent(m_context);
        clear();
        if (m_source)
            alDeleteSources(1, &m_source);
        m_source = 0;
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(m_context);
        m_context = nullptr;
    }
    if (m_device)
    {
        alcCloseDevice(m_device);
        m_device = nullptr;
    }
    m_hrtf = false;
    m_hrtfSupported = false;
    m_paused = false;
    m_pauseRequested = false;
    m_renderTime = 0;
}
void OpenALOutput::collect()
{
    if (!m_source)
        return;
    updateGain();
    ALint done = 0;
    alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &done);
    while (done-- > 0 && !m_buffers.empty())
    {
        ALuint id;
        alSourceUnqueueBuffers(m_source, 1, &id);
        m_completed += m_buffers.front().frames;
        m_buffers.erase(m_buffers.begin());
        alDeleteBuffers(1, &id);
    }
}
bool OpenALOutput::enqueue(const AudioFrame &pcm, QString &error)
{
    if (!pcm.validPcm())
    {
        error = "PCM inválido recusado na saída de áudio";
        return false;
    }
    if (pcm.samples.empty())
        return true;
    if (!m_source || !(pcm.format == m_format))
    {
        error = "Formato PCM mudou sem reconfigurar a saída";
        return false;
    }
    collect();
    if (!m_started && m_buffers.empty() && m_completed == 0)
        m_firstPts = pcm.pts;
    std::vector<float> data(pcm.samples.size());
    const auto channels = m_order.size();
    for (int f = 0; f < pcm.frames(); ++f)
        for (size_t c = 0; c < channels; ++c)
            data[size_t(f) * channels + c] = pcm.samples[size_t(f) * channels + m_order[c]];
    ALuint id;
    alGenBuffers(1, &id);
    alBufferData(id, m_alFormat, data.data(), ALsizei(data.size() * sizeof(float)), m_format.sampleRate);
    alSourceQueueBuffers(m_source, 1, &id);
    if (alGetError() != AL_NO_ERROR)
    {
        alDeleteBuffers(1, &id);
        error = "Falha ao enfileirar PCM";
        return false;
    }
    m_buffers.push_back({id, pcm.frames()});
    if (!m_pauseRequested || !m_suspendOnPause)
    {
        ALint state;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING)
        {
            if (m_started)
                ++m_underruns;
            alSourcePlay(m_source);
            m_started = true;
            if (!m_pauseRequested)
                rampTo(1);
        }
    }
    return true;
}
void OpenALOutput::pause(bool value, bool suspend)
{
    m_suspendOnPause = suspend;
    if (value == m_pauseRequested)
        return;
    updateGain();
    m_pauseRequested = value;
    if (!m_source)
        return;
    if (value)
        rampTo(0);
    else
    {
        if (m_paused && !m_buffers.empty())
            alSourcePlay(m_source);
        m_paused = false;
        rampTo(1);
    }
}
void OpenALOutput::setVolume(float value)
{
    m_volume = std::clamp(value, 0.f, 1.f);
    updateGain();
}
void OpenALOutput::rampTo(float gain)
{
    m_rampFrom = m_gain;
    m_rampTarget = gain;
    m_rampStart = std::chrono::steady_clock::now();
    m_rampRenderStart = m_renderTime;
    m_ramping = true;
}
void OpenALOutput::updateGain()
{
    if (!m_source)
        return;
    const double elapsed =
        m_loopback ? m_renderTime - m_rampRenderStart
                   : std::chrono::duration<double>(std::chrono::steady_clock::now() - m_rampStart).count();
    if (m_ramping)
    {
        const auto t = float(std::clamp(elapsed / .030, 0., 1.));
        // Smoothstep has zero slope at both ends of the 30 ms transition.
        m_gain = m_rampFrom + (m_rampTarget - m_rampFrom) * t * t * (3 - 2 * t);
        m_ramping = t < 1;
    }
    alSourcef(m_source, AL_GAIN, m_volume * m_gain);
    // Allow a mixer interval at zero gain before actually suspending the source.
    if (m_pauseRequested && m_suspendOnPause && !m_paused && !m_ramping && elapsed >= .045)
    {
        alSourcePause(m_source);
        m_paused = true;
    }
}
void OpenALOutput::silence()
{
    ALint state = 0;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING || m_gain == 0)
        return;
    rampTo(0);
    // Only the audio worker owns this output; UI and OpenAL's mixer never wait
    // here.
    for (int ms = 0; ms < 45; ++ms)
    {
        if (m_loopback)
            render(std::max(1, m_format.sampleRate / 1000));
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        updateGain();
    }
}
std::vector<float> OpenALOutput::render(int frames)
{
    if (!m_loopback || !m_device || frames <= 0)
        return {};
    auto renderSamples =
        reinterpret_cast<LPALCRENDERSAMPLESSOFT>(alcGetProcAddress(m_device, "alcRenderSamplesSOFT"));
    std::vector<float> samples(size_t(frames) * 2);
    // Use the same gain service as live playback, with a deterministic render
    // clock.
    int offset = 0;
    while (offset < frames)
    {
        updateGain();
        const int count = std::min(frames - offset, std::max(1, m_format.sampleRate / 1000));
        renderSamples(m_device, samples.data() + offset * 2, count);
        m_renderTime += double(count) / m_format.sampleRate;
        offset += count;
    }
    updateGain();
    return samples;
}
int OpenALOutput::queued() const
{
    return int(m_buffers.size());
}
double OpenALOutput::position() const
{
    if (!m_source)
        return 0;
    ALfloat offset = 0;
    alGetSourcef(m_source, AL_SEC_OFFSET, &offset);
    return double(m_firstPts + m_completed) / m_format.sampleRate + offset;
}
bool OpenALOutput::connected() const
{
    if (!m_device)
        return false;
    if (!alcIsExtensionPresent(m_device, "ALC_EXT_disconnect"))
        return true;
    ALCint v = ALC_TRUE;
    alcGetIntegerv(m_device, ALC_CONNECTED, 1, &v);
    return v == ALC_TRUE;
}
QString OpenALOutput::description() const
{
    return QString("%1 · PCM %2 canais · %3 Hz · OpenAL Soft")
        .arg(m_format.binaural ? "IAMF binaural / OAR"
             : m_hrtf          ? "HRTF ativo"
                               : "Original")
        .arg(m_format.channels.size())
        .arg(m_format.sampleRate);
}
} // namespace tonal::audio
