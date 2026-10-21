#include "ThemeCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace {

const QString kSetting = QStringLiteral("appearance/theme");

QVariantMap merged(const QVariantMap &base, const QVariantMap &theme) {
  QVariantMap out = base;
  for (auto it = theme.cbegin(); it != theme.cend(); ++it) {
    if (it.value().typeId() == QMetaType::QVariantMap &&
        base.value(it.key()).typeId() == QMetaType::QVariantMap)
      out.insert(it.key(), merged(base.value(it.key()).toMap(), it.value().toMap()));
    else
      out.insert(it.key(), it.value());
  }
  return out;
}

} // namespace

ThemeCatalog::ThemeCatalog(QObject *parent, const QString &path)
    : QObject(parent) {
  // No corpo, e não na lista de inicialização: load() escreve em m_defaultId,
  // que é declarado depois de m_themes.
  m_themes = load(path, &m_defaultId);
  setThemeId(QSettings().value(kSetting, m_defaultId).toString());
}

QVariantList ThemeCatalog::load(const QString &path, QString *defaultId) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
  const QJsonArray list = root.value(QStringLiteral("themes")).toArray();
  const QString fallbackId = root.value(QStringLiteral("default")).toString();
  QVariantMap base;
  for (const QJsonValue &v : list)
    if (v.toObject().value(QStringLiteral("id")).toString() == fallbackId)
      base = v.toObject().toVariantMap();
  if (base.isEmpty() && !list.isEmpty())
    base = list.first().toObject().toVariantMap();
  if (defaultId)
    *defaultId = base.value(QStringLiteral("id")).toString();
  QVariantList themes;
  for (const QJsonValue &v : list) {
    const QVariantMap theme = v.toObject().toVariantMap();
    if (theme.value(QStringLiteral("id")).toString().isEmpty())
      continue;
    // O ajuste "android" é só do Java; no PC vale o tema principal.
    QVariantMap full = merged(base, theme);
    full.remove(QStringLiteral("android"));
    themes << full;
  }
  return themes;
}

QVariantMap ThemeCatalog::current() const {
  for (const QVariant &t : m_themes)
    if (t.toMap().value(QStringLiteral("id")).toString() == m_themeId)
      return t.toMap();
  return m_themes.isEmpty() ? QVariantMap() : m_themes.first().toMap();
}

void ThemeCatalog::setThemeId(const QString &id) {
  QString chosen = m_defaultId;
  for (const QVariant &t : m_themes)
    if (t.toMap().value(QStringLiteral("id")).toString() == id)
      chosen = id;
  if (chosen == m_themeId)
    return;
  m_themeId = chosen;
  QSettings().setValue(kSetting, m_themeId);
  emit changed();
}
