#include "ThemeCatalog.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>

// Loads the real resources/themes.json (embedded as :/themes/...), checks
// that every theme is complete for QML and that the choice persists.
static int checks = 0;
static void require(bool ok, const char *message) {
  if (!ok) {
    std::cerr << "FAIL " << message << "\n";
    std::exit(1);
  }
  ++checks;
}

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QTemporaryDir settingsDir;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
  QCoreApplication::setOrganizationName(QStringLiteral("TonalThemeChecks"));
  QCoreApplication::setApplicationName(QStringLiteral("ThemeCatalogChecks"));

  QString defaultId;
  const QVariantList themes = ThemeCatalog::load(QStringLiteral(":/themes/resources/themes.json"), &defaultId);
  require(themes.size() >= 5, "at least five themes");
  require(defaultId == QStringLiteral("tonal"), "Tonal Azul is the default");
  for (const QVariant &value : themes) {
    const QVariantMap theme = value.toMap();
    for (const char *key : {"id", "name", "bg", "panel", "card", "text", "textSecondary", "accent", "accentText", "onAccent"})
      require(!theme.value(QString::fromLatin1(key)).toString().isEmpty(), "theme token present after merge");
    const QVariantMap seek = theme.value(QStringLiteral("seek")).toMap();
    require(seek.value(QStringLiteral("fill")).toList().size() == 2, "seek fill has two stops");
    require(seek.value(QStringLiteral("thumbSize")).toInt() > 0, "thumb size present");
    require(!theme.contains(QStringLiteral("android")), "Android-only override hidden from the desktop");
  }

  {
    ThemeCatalog catalog;
    require(catalog.themeId() == defaultId, "first start uses the default theme");
    int changes = 0;
    QObject::connect(&catalog, &ThemeCatalog::changed, [&] { ++changes; });
    catalog.setThemeId(QStringLiteral("neon"));
    require(changes == 1 && catalog.current().value(QStringLiteral("id")) == QStringLiteral("neon"), "switching emits once");
    require(catalog.current().value(QStringLiteral("seek")).toMap().value(QStringLiteral("glow")).toBool(), "Neon thumb glows");
    catalog.setThemeId(QStringLiteral("neon"));
    require(changes == 1, "same theme does not notify again");
  }
  {
    ThemeCatalog reopened;
    require(reopened.themeId() == QStringLiteral("neon"), "choice persists across starts");
    reopened.setThemeId(QStringLiteral("missing-theme"));
    require(reopened.themeId() == defaultId, "unknown id falls back to the default");
  }
  std::cout << "THEME_CATALOG_PASS " << themes.size() << " themes, " << checks << " checks\n";
  return 0;
}
