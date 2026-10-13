#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTranslator>
#include <QWindow>

#include "App.h"
#include "MediaControls.h"
#include "NetworkFactory.h"

int main(int argc, char *argv[])
{
    MediaControls::setAppUserModelId();

    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Tonal"));
    QGuiApplication::setApplicationName(QStringLiteral("Tonal"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));

    // Os textos do código são em português; o inglês vem de i18n/tonal_en.ts, compilado para dentro do .exe.
    // O idioma vale do início ao fim da execução: trocar pede para reiniciar (App::setLanguage).
    const QString language = App::startupLanguage();
    QTranslator translator;
    if (language == QLatin1String("en") && translator.load(QStringLiteral(":/i18n/tonal_en.qm")))
        QCoreApplication::installTranslator(&translator);
    // Datas e números do QML (Qt.locale()) no mesmo idioma.
    QLocale::setDefault(language == QLatin1String("en") ? QLocale(QLocale::English, QLocale::UnitedStates)
                                                        : QLocale(QLocale::Portuguese, QLocale::Brazil));

    // "Basic" é o estilo mais leve do Qt Quick Controls; o visual é todo nosso.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

#ifdef Q_OS_WIN
    QFont font(QStringLiteral("Segoe UI Variable Text"));
    if (!QFontInfo(font).exactMatch())
        font = QFont(QStringLiteral("Segoe UI"));
#else
    QFont font;
#endif
    font.setPointSizeF(10);
    font.setHintingPreference(QFont::PreferNoHinting);
    QGuiApplication::setFont(font);

    int code = 0;
    {
        NetworkFactory networkFactory;
        App backend(nullptr);
        App::setInstance(&backend);

        QQmlApplicationEngine engine;
        engine.setNetworkAccessManagerFactory(&networkFactory);
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                         &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
        engine.loadFromModule("Tonal", "Main");

        // Teclas de mídia e painel de mídia do Windows: precisam da janela principal.
        MediaControls media(backend.player());
        if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().value(0)))
            media.attach(window);

        code = app.exec();
    } // o App sai daqui: o librespot já parou quando a instância nova abrir

    if (App::restartRequested())
        QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
    return code;
}
