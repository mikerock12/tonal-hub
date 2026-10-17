#include "AudioSettings.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <cmath>

namespace tonal::audio
{
namespace
{
double clamp(double v, double lo, double hi, double fallback = 0)
{
    return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
}
} // namespace
void AudioSettings::validate()
{
    for (auto &v : eq)
        v = clamp(v, -12, 12);
    bass = clamp(bass, 0, 12);
    targetLufs = clamp(targetLufs, -24, -10, -16);
    ceilingDb = clamp(ceilingDb, -6, -0.1, -1);
    if (spatial != "original" && spatial != "hrtf" && spatial != "iamf")
        spatial = "original";
}
QJsonObject AudioSettings::json() const
{
    QJsonArray bands;
    for (auto v : eq)
        bands.append(v);
    return {{"schemaVersion", 1},
            {"enabled", enabled},
            {"eqEnabled", eqEnabled},
            {"eq", bands},
            {"bass", bass},
            {"compressor", compressor},
            {"normalization", normalization},
            {"targetLufs", targetLufs},
            {"ceilingDb", ceilingDb},
            {"spatial", spatial},
            {"preset", preset}};
}
AudioSettings AudioSettings::fromJson(const QJsonObject &j)
{
    AudioSettings s;
    if (j.value("schemaVersion").toInt(1) != 1)
        return s;
    s.enabled = j.value("enabled").toBool(true);
    s.eqEnabled = j.value("eqEnabled").toBool(false);
    const auto a = j.value("eq").toArray();
    for (int i = 0; i < std::min(10, int(a.size())); ++i)
        s.eq[i] = a[i].toDouble();
    s.bass = j.value("bass").toDouble();
    s.compressor = j.value("compressor").toBool();
    s.normalization = j.value("normalization").toBool(true);
    s.targetLufs = j.value("targetLufs").toDouble(-16);
    s.ceilingDb = j.value("ceilingDb").toDouble(-1);
    s.spatial = j.value("spatial").toString("original");
    s.preset = j.value("preset").toString("Personalizada");
    s.validate();
    return s;
}
AudioSettings AudioSettings::load(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return fromJson(QJsonDocument::fromJson(f.readAll()).object());
}
bool AudioSettings::save(const QString &path) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    const auto data = QJsonDocument(json()).toJson();
    return f.write(data) == data.size() && f.commit();
}
bool AudioSettings::effectsActive(double gain) const
{
    return enabled && ((eqEnabled && std::any_of(eq.begin(), eq.end(), [](double v) { return v != 0; })) ||
                       bass > 0 || compressor || std::abs(gain) > 0.001);
}
void AudioSettings::applyPreset(const QString &name)
{
    eq = {};
    bass = 0;
    eqEnabled = true;
    compressor = false;
    preset = name;
    if (name == "Rock")
    {
        eq = {0, 2, 1, -1, -1, 0, 1, 2, 1, 0};
        bass = 2;
    }
    else if (name == "Pop")
        eq = {-1, 1, 2, 1, 0, 1, 2, 1, 0, -1};
    else if (name == "Clássica")
        eq = {0, 0, 0, 0, -1, -1, 0, 1, 1, 0};
    else if (name == "Eletrônica")
    {
        eq = {2, 2, 1, 0, -1, 0, 1, 2, 1, 0};
        bass = 3;
    }
    else if (name == "Vocal")
        eq = {-2, -1, -1, 0, 1, 2, 2, 1, 0, -1};
    else
    {
        preset = "Flat";
        eqEnabled = false;
    }
}
} // namespace tonal::audio
