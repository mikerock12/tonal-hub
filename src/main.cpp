#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
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

    return app.exec();
}
