#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Temas visuais do player. A lista vem de resources/themes.json, o mesmo
// arquivo copiado para os assets do Android, para PC e celular terem os
// mesmos temas. A escolha fica em QSettings ("appearance/theme").
class ThemeCatalog : public QObject {
  Q_OBJECT
  QML_ANONYMOUS
  Q_PROPERTY(QVariantList themes READ themes CONSTANT)
  Q_PROPERTY(QString themeId READ themeId WRITE setThemeId NOTIFY changed)
  Q_PROPERTY(QVariantMap current READ current NOTIFY changed)

public:
  explicit ThemeCatalog(QObject *parent = nullptr,
                        const QString &path = QStringLiteral(":/themes/resources/themes.json"));

  // Lê e normaliza o catálogo: todo tema recebe os campos que faltarem do
  // tema padrão, para um JSON incompleto nunca deixar cor indefinida no QML.
  static QVariantList load(const QString &path, QString *defaultId = nullptr);

  QVariantList themes() const { return m_themes; }
  QString themeId() const { return m_themeId; }
  QVariantMap current() const;
  void setThemeId(const QString &id);

signals:
  void changed();

private:
  QVariantList m_themes;
  QString m_defaultId;
  QString m_themeId;
};
