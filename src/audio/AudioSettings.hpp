#pragma once
#include <QJsonObject>
#include <QString>
#include <array>

namespace tonal::audio
{
struct AudioSettings
{
    static constexpr std::array<int, 10> frequencies{31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    bool enabled = true;
    bool eqEnabled = false;
    std::array<double, 10> eq{};
    double bass = 0;
    bool compressor = false;
    bool normalization = true;
    double targetLufs = -16;
    double ceilingDb = -1; // sample peak; this is NOT a true-peak guarantee
    QString spatial = QStringLiteral("original");
    QString preset = QStringLiteral("Flat");
    QJsonObject json() const;
    static AudioSettings fromJson(const QJsonObject &json);
    static AudioSettings load(const QString &path);
    bool save(const QString &path) const;
    void validate();
    void applyPreset(const QString &name);
    bool effectsActive(double trackGainDb = 0) const;
};
} // namespace tonal::audio
