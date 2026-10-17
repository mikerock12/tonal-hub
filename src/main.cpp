#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTranslator>
#include <QWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDir>
#include <QCryptographicHash>
#include <QJsonDocument>

#include "App.h"
#include "MediaControls.h"
#include "NetworkFactory.h"

int main(int argc, char *argv[])
{
    MediaControls::setAppUserModelId();

    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Tonal"));
    QGuiApplication::setApplicationName(QStringLiteral("Tonal"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Tonalify"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));
    const bool audioSmokeTest=app.arguments().contains(QStringLiteral("--audio-smoke-test"));
    QLocalServer instance;
    const bool isolated=audioSmokeTest||app.arguments().contains(QStringLiteral("--local-only"));
    if(!isolated){
        QString name="Tonalify-"+QCryptographicHash::hash(QDir::homePath().toUtf8(),QCryptographicHash::Sha256).toHex().left(16);
        QLocalSocket existing;existing.connectToServer(name);
        if(existing.waitForConnected(150)){existing.write("{\"action\":\"activate\"}");existing.waitForBytesWritten(500);return 0;}
        instance.setSocketOptions(QLocalServer::UserAccessOption);QLocalServer::removeServer(name);
        if(!instance.listen(name))return 3;
    }
    if(audioSmokeTest){QCoreApplication::setOrganizationName(QStringLiteral("TonalSmokeTests"));QCoreApplication::setApplicationName(QStringLiteral("AudioUi"));}

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
        QObject::connect(&instance,&QLocalServer::newConnection,&app,[&]{
            while(auto *socket=instance.nextPendingConnection()){
                auto buffer=std::make_shared<QByteArray>();
                auto handle=[&,socket,buffer]{
                    if(socket->property("handled").toBool())return;
                    buffer->append(socket->readAll());if(buffer->size()>4096){socket->abort();return;}
                    auto doc=QJsonDocument::fromJson(*buffer);if(!doc.isObject())return;
                    socket->setProperty("handled",true);
                    if(doc.object()["action"]=="pause"){backend.player()->pause();}
                    else if(auto *window=qobject_cast<QWindow*>(engine.rootObjects().value(0))){window->showNormal();window->requestActivate();}
                    socket->disconnectFromServer();
                };
                QObject::connect(socket,&QLocalSocket::readyRead,&app,handle);
                if(socket->bytesAvailable())QTimer::singleShot(0,socket,handle);
                QObject::connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
                QTimer::singleShot(2000,socket,[socket]{socket->abort();socket->deleteLater();});
            }
        });

        // Teclas de mídia e painel de mídia do Windows: precisam da janela principal.
        MediaControls media(backend.player());
        if (!audioSmokeTest) {
            if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().value(0)))
                media.attach(window);
        }
        const int fileArgument=app.arguments().indexOf(QStringLiteral("--local-file"));
        if(fileArgument>=0&&fileArgument+1<app.arguments().size()){
            const auto url=QUrl::fromLocalFile(app.arguments().at(fileArgument+1));
            QTimer::singleShot(0,&backend,[&backend,url]{backend.audio()->openFile(url);});
        }
        if(audioSmokeTest){
            backend.audio()->reset();
            // Wait for asynchronous device discovery instead of assuming that
            // HRTF is available 600 ms after startup on a busy test machine.
            auto *poll=new QTimer(&app);poll->setInterval(100);
            auto elapsed=std::make_shared<QElapsedTimer>();elapsed->start();
            QObject::connect(poll,&QTimer::timeout,&app,[&,poll,elapsed,clicked=false]() mutable {
                auto *root=engine.rootObjects().value(0);auto *panel=root?root->property("audioEffectsController").value<QObject*>():nullptr;
                auto *button=root?root->findChild<QObject*>(QStringLiteral("hrtfMode")):nullptr;
                if(!clicked&&button&&button->property("enabled").toBool()){clicked=true;QMetaObject::invokeMethod(button,"clicked");return;}
                const int bands=panel?panel->property("instantiatedBandCount").toInt():0;
                const bool hrtf=button&&button->property("enabled").toBool()&&button->property("checked").toBool()&&backend.audio()->hrtfActive();
                const bool ok=panel&&panel->property("opened").toBool()&&bands==10&&backend.audio()->positionMs()>100&&hrtf;
                if(ok||elapsed->elapsed()>9000){poll->stop();qInfo()<<"AUDIO_UI_SMOKE"<<ok<<"EQ bands"<<bands<<"position"<<backend.audio()->positionMs()<<"stereo HRTF selection"<<hrtf;app.exit(ok?0:2);}
            });poll->start();
        }

        code = app.exec();
    } // o App sai daqui: o librespot já parou quando a instância nova abrir

    if (App::restartRequested())
        QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
    return code;
}
