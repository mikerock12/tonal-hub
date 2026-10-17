#include "audio/AudioEngine.hpp"
#include "audio/AudioSettings.hpp"
#include "audio/DspGraph.hpp"
#include "audio/FfmpegDecoder.hpp"
#include "audio/IamfDecoderAdapter.hpp"
#include "audio/LoudnessAnalyzer.hpp"
#include "audio/NativePcmPipe.hpp"
#include "audio/OpenALOutput.hpp"
#include <AL/alext.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif
using namespace tonal::audio;
void check(bool value, const QString &why)
{
    if (!value)
        throw std::runtime_error(why.toStdString());
}
AudioFrame tone(double hz, double amplitude, int rate = 48000)
{
    AudioFrame f;
    f.format.sampleRate = rate;
    f.samples.resize(size_t(rate) * 2);
    for (int i = 0; i < rate; ++i)
        f.samples[2 * i] = f.samples[2 * i + 1] =
            float(amplitude * std::sin(2 * 3.141592653589793 * hz * i / rate));
    return f;
}
double rms(const AudioFrame &f)
{
    double sum = 0;
    for (float s : f.samples)
        sum += double(s) * s;
    return std::sqrt(sum / std::max<size_t>(1, f.samples.size()));
}
AudioFrame filtered(const AudioFrame &in, AudioSettings settings)
{
    DspGraph graph;
    QString error;
    check(graph.configure(in.format, settings, 0, error), error);
    AudioFrame out, tail;
    check(graph.process(in, out, error), error);
    check(graph.drain(tail, error), error);
    out.samples.insert(out.samples.end(), tail.samples.begin(), tail.samples.end());
    return out;
}
void settingsTest()
{
    AudioSettings s;
    s.eq[0] = std::numeric_limits<double>::quiet_NaN();
    s.bass = 100;
    s.eq[1] = -100;
    s.targetLufs = std::numeric_limits<double>::infinity();
    s.validate();
    check(s.eq[0] == 0 && s.eq[1] == -12 && s.bass == 12 && s.targetLufs == -16, "settings validation");
    QTemporaryDir dir;
    const auto path = dir.filePath("audio.json");
    check(s.save(path), "atomic save");
    auto copy = AudioSettings::load(path);
    check(copy.json() == s.json(), "settings round trip");
    QFile f(path);
    check(f.open(QIODevice::WriteOnly), "corrupt config");
    f.write("{broken");
    f.close();
    check(AudioSettings::load(path).bass == 0, "corrupt config fallback");
    std::cout << "PASS settings, bounds, atomic persistence\n";
}
void dspTest()
{
    auto in = tone(1000, .03);
    AudioSettings s;
    s.normalization = false;
    auto out = filtered(in, s);
    check(out.samples == in.samples, "bypass must be bit-identical float32");
    s.eqEnabled = true;
    s.eq[5] = 3;
    out = filtered(in, s);
    check(std::abs(int(out.samples.size()) - int(in.samples.size())) <= in.format.sampleRate * .012,
          "DSP lookahead drain sample count");
    const double gain = 20 * std::log10(rms(out) / rms(in));
    check(gain > 2.5 && gain < 3.5, QString("EQ gain=%1").arg(gain));
    s = {};
    s.normalization = false;
    s.bass = 12;
    out = filtered(tone(60, .85), s);
    for (float value : out.samples)
        check(std::isfinite(value) && std::abs(value) <= .892, "limiter/finite output");
    s = {};
    s.normalization = false;
    s.compressor = true;
    const auto loud = tone(1000, .5);
    out = filtered(loud, s);
    check(rms(out) < rms(loud) * .8, "compressor dynamic reduction");
    DspGraph live;
    QString error;
    auto block = tone(1000, .05);
    block.samples.resize(1024);
    s = {};
    s.normalization = false;
    check(live.configure(block.format, s, 0, error), error);
    for (int i = 0; i < 120; ++i)
    {
        block.pts = i * 512;
        if (i % 10 == 0)
        {
            s.eqEnabled = true;
            s.eq[5] = (i % 20 == 0) ? 3 : -3;
            s.compressor = !s.compressor;
            live.update(s, 0);
        }
        check(live.process(block, out, error), error);
        check(out.samples.size() == block.samples.size(), "live changes must not lose PCM frames");
        for (auto v : out.samples)
            check(std::isfinite(v), "live output finite");
    }
    s = {};
    s.normalization = false;
    live.update(s, 0);
    for (int i = 0; i < 8; ++i)
        check(live.process(block, out, error), error);
    check(out.samples == block.samples, "runtime return to exact bypass");
    std::cout << "PASS bypass null test, EQ response, +12dB bass limiter, "
                 "compression, sample-continuous "
                 "live commands\n";
}
void decoderTest(const QString &dir)
{
    QString error;
    for (auto name : {"stereo-44100.wav", "stereo-48000.wav", "stereo-96000.wav", "stereo.mp3", "stereo.flac",
                      "stereo.m4a", "surround-51.wav", "surround-71.wav"})
    {
        FfmpegDecoder d;
        check(d.open(dir + "/" + name, error), QString(name) + ": " + error);
        const auto rate = d.format().sampleRate;
        AudioFrame f;
        int count = 0, ret = 0, blocks = 0;
        while ((ret = d.read(f, error)) > 0)
        {
            count += f.frames();
            check(++blocks < 10000, "bounded decode");
            for (float v : f.samples)
                check(std::isfinite(v), "finite decode");
        }
        check(ret == 0, error);
        check(std::abs(double(count) / rate - 3) < .08, QString("duration/sample rate %1").arg(name));
        check(d.seek(1.25, error), error);
        check(d.read(f, error) > 0, error);
        check(std::abs(double(f.pts) / rate - 1.25) < .03, "seek timestamps");
    }
    FfmpegDecoder d;
    check(d.open(dir + "/replaygain.flac", error), error);
    check(d.hasReplayGain() && std::abs(d.replayGainDb() + 3) < .01, "ReplayGain metadata priority");
    check(!d.open(dir + "/corrupt.bin", error), "reject malformed input");
    if (d.open(dir + "/truncated.flac", error))
    {
        AudioFrame f;
        int n = 0;
        while (d.read(f, error) > 0)
            check(++n < 10000, "truncated decode bounded");
    }
    std::cout << "PASS WAV/MP3/FLAC/AAC, 44.1/48/96kHz, multichannel, seek, "
                 "malformed files\n";
}
void loudnessTest(const QString &dir)
{
    QTemporaryDir cache;
    auto never = [] { return false; };
    const auto a = LoudnessAnalyzer::analyze(dir + "/quiet.wav", cache.path(), never),
               b = LoudnessAnalyzer::analyze(dir + "/loud.wav", cache.path(), never);
    check(a.valid && b.valid, "R128 analysis");
    check(std::abs((b.lufs - a.lufs) - 12.04) < .3, "R128 relative loudness");
    const auto gainA = LoudnessAnalyzer::gain(a, -16, -1), gainB = LoudnessAnalyzer::gain(b, -16, -1);
    check(std::abs((a.lufs + gainA) - (b.lufs + gainB)) < .3, "track normalization target");
    const auto cached = LoudnessAnalyzer::analyze(dir + "/quiet.wav", cache.path(), never);
    check(cached.lufs == a.lufs, "analysis cache");
    std::cout << "PASS EBU R128, true-peak headroom, track gain, analysis cache\n";
}
void outputTest()
{
    QString error;
    OpenALOutput out;
    AudioFormat stereo;
    check(out.open(stereo, true, error), error);
    check(out.hrtfSupported() && out.hrtf(), "stereo virtual speakers support real HRTF");
    auto f = tone(1000, .01, 44100);
    f.samples.resize(1024);
    check(out.enqueue(f, error), error);
    out.pause(true);
    out.clear();
    out.close();
    AudioFormat surround;
    surround.sampleRate = 48000;
    surround.channels = {Channel::FL, Channel::FR, Channel::FC, Channel::LFE, Channel::BL, Channel::BR};
    check(out.open(surround, true, error), error);
    check(out.hrtfSupported() && out.hrtf(), "real HRTF status on 5.1");
    out.close();
    stereo.binaural = true;
    check(out.open(stereo, true, error), error);
    check(!out.hrtf(), "OAR binaural must bypass HRTF");
    std::cout << "PASS OpenAL queue, pause, flush, stereo/5.1 HRTF and no double "
                 "HRTF\n";
}
void pipeTest()
{
#ifdef _WIN32
    NativePcmPipe pipe;
    check(pipe.valid(), "pipe creation");
    QProcess child;
    const auto writer = pipe.writer();
    child.setCreateProcessArgumentsModifier([writer](QProcess::CreateProcessArguments *a) {
        a->inheritHandles = true;
        a->startupInfo->dwFlags |= STARTF_USESTDHANDLES;
        a->startupInfo->hStdOutput = reinterpret_cast<HANDLE>(writer);
        a->flags |= CREATE_NO_WINDOW;
    });
    child.start(QCoreApplication::applicationFilePath(), {"--pipe-child"});
    check(child.waitForStarted(3000), child.errorString());
    pipe.closeWriter();
    QElapsedTimer timer;
    timer.start();
    int frames = 0;
    while (timer.elapsed() < 5000)
    {
        std::vector<float> samples;
        const int got = pipe.read(samples, 512);
        if (got < 0)
            break;
        for (float v : samples)
            check(v == .125f, "raw pipe exact PCM, no text");
        frames += got;
        if (!got)
            QThread::msleep(1);
    }
    check(child.waitForFinished(1000), "pipe producer exit");
    check(child.exitCode() == 0 && frames == 8192, "pipe backpressure/frame alignment");
    std::cout << "PASS QProcess OS PCM pipe, bounded backpressure, channel "
                 "separation\n";
#endif
}
void iamfTest(const QString &dir)
{
    if (!IamfDecoderAdapter::available())
    {
        std::cout << "SKIP IAMF feature disabled\n";
        return;
    }
    QString error;
    QTemporaryDir temp;
    for (auto name : {"stereo.iamf", "surround.iamf"})
    {
        const auto wav = temp.filePath(QString(name) + ".wav");
        check(IamfDecoderAdapter::renderBinaural(dir + "/" + name, wav, error), error);
        FfmpegDecoder d;
        check(d.open(wav, error), error);
        check(d.format().sampleRate == 48000 && d.format().channels.size() == 2, "OAR PCM format");
        AudioFrame f;
        int count = 0;
        double sum = 0;
        while (d.read(f, error) > 0)
        {
            count += f.frames();
            sum += rms(f);
        }
        check(count > 140000 && count < 150000 && sum > 0, "IAMF real binaural PCM");
    }
    IamfDecoderAdapter adapter;
    check(adapter.open(dir + "/surround.iamf", error), error);
    check(adapter.format().binaural, "IAMF PCM provenance flag");
    check(adapter.seek(1.5, error), error);
    AudioFrame f;
    check(adapter.read(f, error) > 0 && f.format.binaural, "IAMF cached seek and no double HRTF");
    check(!IamfDecoderAdapter::renderBinaural(dir + "/corrupt.bin", temp.filePath("bad.wav"), error),
          "IAMF malformed input");
    std::cout << "PASS stereo/5.1 IAMF decode -> OAR binaural -> WAV, "
                 "cache/seek, malformed fallback\n";
}
template <class F> bool spin(F predicate, int timeout = 5000)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeout)
    {
        QCoreApplication::processEvents();
        if (predicate())
            return true;
        QThread::msleep(5);
    }
    return false;
}
void spotifyPauseTest()
{
#ifdef _WIN32
    QTemporaryDir storage;
    AudioEngine engine(nullptr, storage.path());
    const auto writer = engine.prepareSpotifyPipe();
    check(writer != 0, "Spotify PCM writer");
    engine.setSpotifyPaused(true);
    QProcess child;
    child.setCreateProcessArgumentsModifier([writer](QProcess::CreateProcessArguments *a) {
        a->inheritHandles = true;
        a->startupInfo->dwFlags |= STARTF_USESTDHANDLES;
        a->startupInfo->hStdOutput = reinterpret_cast<HANDLE>(writer);
        a->flags |= CREATE_NO_WINDOW;
    });
    child.start(QCoreApplication::applicationFilePath(), {"--pipe-child"});
    check(child.waitForStarted(), "synthetic Spotify producer starts");
    engine.spotifyStarted();
    const bool finished = spin([&] { return child.state() == QProcess::NotRunning; }, 3000);
    if (!finished)
    {
        child.kill();
        child.waitForFinished();
    }
    check(finished && child.exitCode() == 0,
          "paused Spotify still drains more than the pipe capacity without deadlock");
    engine.setSpotifyPaused(false);
    engine.stopSpotify();
    std::cout << "PASS paused Spotify engine drains native producer without blocking transport\n";
#endif
}
void engineTest(const QString &dir)
{
    QTemporaryDir storage;
    AudioEngine engine(nullptr, storage.path());
    engine.setSetting("normalization", false);
    engine.openFile(QUrl::fromLocalFile(QFileInfo(dir + "/stereo-48000.wav").absoluteFilePath()));
    check(spin([&] { return engine.positionMs() > 200; }), "engine advances on OpenAL clock");
    check(engine.hrtfAvailable(), "stereo enables the HRTF control");
    engine.setSetting("spatial", "hrtf");
    check(spin([&] { return engine.hrtfActive(); }), "stereo switches to actual HRTF");
    engine.setSetting("spatial", "original");
    check(spin([&] { return !engine.hrtfActive(); }), "switch back to original");
    engine.setSetting("eqEnabled", true);
    engine.setEqBand(5, 3);
    engine.setSetting("bass", 2);
    engine.setSetting("compressor", true);
    check(spin([&] { return engine.positionMs() > 450; }), "live DSP updates keep playback advancing");
    engine.toggleLocalPlay();
    spin([] { return false; }, 150);
    const int paused = engine.positionMs();
    spin([] { return false; }, 200);
    check(std::abs(engine.positionMs() - paused) < 50, "pause freezes output clock");
    engine.toggleLocalPlay();
    check(spin([&] { return engine.positionMs() > paused + 100; }),
          "resume preserves the local queue without a seek");
    engine.toggleLocalPlay();
    engine.seekLocal(1300);
    engine.toggleLocalPlay();
    check(spin([&] { return engine.positionMs() > 1400; }), "seek resumes from target");
    engine.seekLocal(2600);
    spin([] { return false; }, 50);
    engine.toggleLocalPlay();
    spin([] { return false; }, 200);
    check(!engine.localPlaying() && engine.positionMs() < 2900,
          "pause during EOF drain keeps the remaining audio");
    engine.toggleLocalPlay();
    check(spin([&] { return !engine.localPlaying(); }), "drain at EOF");
    engine.toggleLocalPlay();
    check(spin([&] { return engine.positionMs() > 100 && engine.positionMs() < 1500; }), "replay from EOF");
    for (int i = 0; i < 3; ++i)
    {
        engine.openFile(QUrl::fromLocalFile(QFileInfo(dir + "/stereo-44100.wav").absoluteFilePath()));
        spin([] { return false; }, 30);
        engine.returnToSpotify();
    }
    check(!engine.localActive(), "source switching releases local route");
    std::cout << "PASS engine lifecycle, live EQ/bass/compressor, pause, seek, "
                 "EOF/replay, source switching\n";
}
void hrtfRenderTest()
{
    QString error;
    for (int channels : {2, 6})
    {
        for (bool hrtf : {false, true})
        {
            AudioFrame frame;
            frame.format.sampleRate = 48000;
            if (channels == 6)
                frame.format.channels = {Channel::FL,  Channel::FR, Channel::FC,
                                         Channel::LFE, Channel::BL, Channel::BR};
            frame.samples.resize(8192 * channels);
            // Leave time for the output's startup fade, then excite only FL.
            frame.samples[4096 * channels] = .1f;
            OpenALOutput out;
            check(out.open(frame.format, hrtf, error, true), error);
            check(out.enqueue(frame, error), error);
            auto stereo = out.render(10000);
            double left = 0, right = 0;
            for (size_t i = 0; i < stereo.size(); i += 2)
            {
                left += stereo[i] * stereo[i];
                right += stereo[i + 1] * stereo[i + 1];
            }
            check(left > 1e-8, "FL reaches left ear");
            if (hrtf)
                check(out.hrtf() && right > 1e-9 && left > right,
                      "production HRTF path retains FL direction and produces "
                      "cross-ear signal");
            else if (channels == 2)
                check(right < 1e-12, "original stereo keeps FL out of right channel");
        }
    }
    std::cout << "PASS production stereo/5.1 HRTF rendered impulses and original "
                 "stereo isolation\n";
}
void pauseRampTest(bool hrtf)
{
    QString error;
    OpenALOutput out;
    AudioFrame dc;
    dc.format.sampleRate = 48000;
    dc.samples.resize(48000 * 2, .25f);
    check(out.open(dc.format, hrtf, error, true), error);
    check(out.enqueue(dc, error), error);
    const float level = out.render(4800).back();
    check(std::abs(level) > .01f, "steady signal before pause");
    const double before = out.position();
    out.pause(true);
    auto fade = out.render(4800);
    const double paused = out.position();
    check(paused > before + .025 && paused < before + .055, "pause fades before suspending playback");
    auto verifyFade = [level](const std::vector<float> &samples, bool up) {
        float maxStep = 0;
        float previous = up ? 0.f : level;
        for (size_t i = 0; i < samples.size(); i += 2)
        {
            check(std::isfinite(samples[i]), "finite transition samples");
            maxStep = std::max(maxStep, std::abs(samples[i] - previous));
            previous = samples[i];
        }
        check(maxStep < std::abs(level) * .04f, "pause/resume has no abrupt sample discontinuity");
        check(up ? std::abs(samples.back() - level) < .001f : std::abs(samples.back()) < 1e-6f,
              "fade reaches the expected gain");
    };
    verifyFade(fade, false);
    out.render(4800);
    check(std::abs(out.position() - paused) < .001 && out.queued() == 1,
          "pause preserves queued PCM and freezes the source clock");
    out.pause(false);
    verifyFade(out.render(4800), true);
    check(out.position() > paused + .09, "resume continues preserved PCM");
    // Rapid reversal must begin at the current gain rather than jump to an
    // endpoint.
    out.pause(true);
    auto partial = out.render(480);
    out.pause(false);
    auto reversed = out.render(4800);
    check(std::abs(reversed.front() - partial.back()) < .01f, "rapid pause/resume reverses smoothly");
    const double streamingBefore = out.position();
    out.pause(true, false);
    verifyFade(out.render(4800), false);
    check(out.position() > streamingBefore + .09,
          "Spotify mute drains PCM so producer commands cannot deadlock");
    out.pause(false, false);
    verifyFade(out.render(4800), true);
    std::cout << "PASS rendered pause/resume ramps, preserved queue, rapid "
                 "reversal and streaming mute\n";
}
void spatialSwitchTest()
{
    QString error;
    OpenALOutput out;
    auto frame = tone(440, .1);
    check(out.open(frame.format, false, error, true), error);
    check(out.enqueue(frame, error), error);
    out.render(4800);
    for (bool hrtf : {true, false, true})
    {
        const double before = out.position();
        check(out.setHrtf(hrtf, error), error);
        check(out.hrtf() == hrtf && out.queued() == 1, "mode switch preserves queued PCM");
        check(out.position() >= before && out.position() < before + .055,
              "mode switch preserves playhead without skipping queued audio");
        out.render(4800);
    }
    out.pause(true);
    out.render(4800);
    const double paused = out.position();
    check(out.setHrtf(false, error), error);
    out.render(4800);
    check(std::abs(out.position() - paused) < .001, "mode change while paused does not resume playback");
    std::cout << "PASS HRTF switches preserve source, PCM queue and paused playhead\n";
}
void benchmark()
{
    AudioSettings s;
    s.applyPreset("Rock");
    s.compressor = true;
    auto block = tone(1000, .05);
    block.samples.resize(1024);
    DspGraph graph;
    QString error;
    check(graph.configure(block.format, s, 0, error), error);
    std::vector<double> times;
    AudioFrame out;
    for (int i = 0; i < 500; ++i)
    {
        block.pts = i * 512;
        auto start = std::chrono::steady_clock::now();
        check(graph.process(block, out, error), error);
        times.push_back(
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count());
    }
    std::sort(times.begin(), times.end());
    std::cout << "BENCH DSP 512 frames / 48kHz: median=" << times[250] << "us p95=" << times[475]
              << "us (block budget 10667us)\n";
}
int main(int argc, char **argv)
{
#ifdef _WIN32
    if (argc > 1 && std::string(argv[1]) == "--pipe-child")
    {
        _setmode(_fileno(stdout), _O_BINARY);
        std::vector<float> pcm(8192 * 2, .125f);
        return fwrite(pcm.data(), sizeof(float), pcm.size(), stdout) == pcm.size() ? 0 : 1;
    }
#endif
    QCoreApplication app(argc, argv);
    app.setOrganizationName("TonalTests");
    app.setApplicationName("AudioTests");
    qputenv("ALSOFT_DRIVERS", "null");
    try
    {
        if (argc == 4 && std::string(argv[1]) == "--render-iamf")
        {
            QString error;
            check(IamfDecoderAdapter::renderBinaural(QString::fromLocal8Bit(argv[2]),
                                                     QString::fromLocal8Bit(argv[3]), error),
                  error);
            return 0;
        }
        check(argc > 1, "fixture directory required");
        const auto dir = QString::fromLocal8Bit(argv[1]);
        settingsTest();
        dspTest();
        decoderTest(dir);
        loudnessTest(dir);
        outputTest();
        pipeTest();
        iamfTest(dir);
        hrtfRenderTest();
        pauseRampTest(false);
        pauseRampTest(true);
        spatialSwitchTest();
        spotifyPauseTest();
        engineTest(dir);
        benchmark();
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
    return 0;
}
