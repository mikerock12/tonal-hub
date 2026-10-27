#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>
#include <QJsonObject>
class Player;class LibrespotProcess;class SpotifyApi;

class JamController:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QVariantMap session READ session NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    JamController(Player*,LibrespotProcess*,SpotifyApi*,QObject* parent=nullptr);
    ~JamController() override;
    bool busy()const{return m_busy;}
    QVariantMap session()const{return m_session;}
    QString status()const{return m_status;}
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void start();
    Q_INVOKABLE void join(const QString &link);
    Q_INVOKABLE void leave();
    Q_INVOKABLE void copyInvite();
signals:
    void changed();
    void notice(const QString &text);
    void queueCapacityExceeded(bool value);
private:
    void request(QJsonObject);
    void response(const QJsonObject&);
    void fail(const QString&);
    void pollQueue();
    // Procura um Jam ativo sem abrir a janela: motor pronto ou janela ativada, no máximo a cada 5 min.
    void autoCheck();
    qint64 m_lastAutoCheck=0;
    Player *m_player;LibrespotProcess *m_engine;SpotifyApi *m_api;
    QProcess m_process;QTimer m_timeout,m_poll,m_idle;
    QByteArray m_buffer,m_command;
    QVariantMap m_session;QString m_status="Jam experimental • amigos participam pelo Spotify",m_op;
    QString m_queueAnchor;QStringList m_queueUris;
    bool m_busy=false,m_queueBusy=false,m_queueBaseline=false;
    quint64 m_epoch=0;
    qint64 m_quietUntil=0;
    qint64 m_cooldownUntil=0;
};
