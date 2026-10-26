#include "JamController.h"
#include "JamState.h"
#include "Player.h"
#include "LibrespotProcess.h"
#include "SpotifyApi.h"
#include "playback/QueueMergeCore.hpp"
#include <QJsonArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QUrl>
#include <QClipboard>

JamController::JamController(Player*p,LibrespotProcess*e,SpotifyApi*a,QObject*parent):QObject(parent),m_player(p),m_engine(e),m_api(a){
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_timeout.setSingleShot(true);
    m_idle.setSingleShot(true);m_idle.setInterval(30000);connect(&m_idle,&QTimer::timeout,this,[this]{if(!m_busy&&!m_session.value("active").toBool())m_process.kill();});
    connect(&m_timeout,&QTimer::timeout,this,[this]{fail("A consulta Jam excedeu o tempo. Tente atualizar.");});
    connect(&m_process,&QProcess::started,this,[this]{m_process.write(m_command);});
    connect(&m_process,&QProcess::readyReadStandardError,this,[this]{m_process.readAllStandardError();});
    connect(&m_process,&QProcess::readyReadStandardOutput,this,[this]{
        m_buffer+=m_process.readAllStandardOutput();if(m_buffer.size()>600*1024){fail("Resposta Jam maior que o limite.");return;}
        int end;while((end=m_buffer.indexOf('\n'))>=0){auto bytes=m_buffer.left(end);m_buffer.remove(0,end+1);QJsonParseError error;auto value=QJsonDocument::fromJson(bytes,&error);if(error.error!=QJsonParseError::NoError||!value.isObject()){fail("Resposta Jam inválida.");return;}response(value.object());}
    });
    connect(&m_process,&QProcess::errorOccurred,this,[this](auto){if(m_busy)fail("Não foi possível iniciar o componente Jam. Atualize o instalador.");});
    connect(&m_process,&QProcess::finished,this,[this](int,QProcess::ExitStatus){if(m_busy)fail("A conexão Jam foi encerrada. Atualize para reconectar.");});
    m_poll.setInterval(5000);connect(&m_poll,&QTimer::timeout,this,[this]{if(!m_busy&&QGuiApplication::applicationState()==Qt::ApplicationActive)refresh();});
    connect(p,&Player::playbackRequested,this,[this]{m_queueBaseline=false;m_quietUntil=QDateTime::currentMSecsSinceEpoch()+3500;});
    connect(e,&LibrespotProcess::stateChanged,this,[this]{if(m_engine->state()==LibrespotProcess::Stopped||m_engine->state()==LibrespotProcess::NeedsLogin){++m_epoch;m_session.clear();m_poll.stop();m_process.kill();m_busy=false;m_timeout.stop();m_queueBaseline=false;emit changed();}});
}
JamController::~JamController(){m_process.kill();m_process.waitForFinished(1000);}
void JamController::fail(const QString &text){m_timeout.stop();m_busy=false;m_status=text;m_poll.stop();m_process.kill();emit changed();}
void JamController::request(QJsonObject command){
    if(m_busy)return;
    if(QDateTime::currentMSecsSinceEpoch()<m_cooldownUntil){m_status="Aguarde um pouco antes de consultar o Jam novamente.";emit changed();return;}
    m_idle.stop();
    if(!m_engine->nativeLoginAvailable()){m_status="Entre no motor Spotify antes de usar o Jam.";emit changed();return;}
    m_op=command.value("op").toString();
    if(m_op!="current"&&m_op!="leave"){
        const auto id=m_player->localDeviceId();if(!tonal::jam::atom(id,20,128)){m_status="Inicie o motor Tonalify deste PC para usar o Jam.";emit changed();return;}command["device"]=id;
    }
    m_command=QJsonDocument(command).toJson(QJsonDocument::Compact)+'\n';m_busy=true;m_status="Consultando Jam…";emit changed();m_timeout.start(45000);
    if(m_process.state()==QProcess::Running)m_process.write(m_command);
    else {m_buffer.clear();m_process.setProgram(QCoreApplication::applicationDirPath()+"/TonalJam.exe");m_process.setArguments({m_engine->sessionCachePath()});m_process.start();}
}
void JamController::refresh(){request({{"op","current"}});}
void JamController::start(){request({{"op","start"}});}
void JamController::copyInvite(){const auto link=m_session.value("link").toString();if(!link.isEmpty())QGuiApplication::clipboard()->setText(link);}
void JamController::join(const QString &value){
    QString token=value.trimmed();const QUrl u(token);
    if(u.scheme()=="https"&&u.host()=="open.spotify.com"&&u.userInfo().isEmpty()&&u.path().startsWith("/socialsession/"))token=u.path().mid(15);
    else if(token.startsWith("spotify:socialsession:"))token=token.mid(22);
    if(!tonal::jam::atom(token,6,512)){m_status="Cole um convite open.spotify.com/socialsession ou o código do Jam.";emit changed();return;}
    request({{"op","join"},{"token",token}});
}
void JamController::leave(){request({{"op",m_session.value("host").toBool()?"end":"leave"},{"session",m_session.value("id").toString()}});}
void JamController::response(const QJsonObject &reply){
    if(!m_busy)return;m_timeout.stop();m_busy=false;const auto op=m_op;const int code=reply.value("status").toInt();
    if(code==404&&op=="current"){++m_epoch;m_session.clear();m_queueBaseline=false;m_status="Nenhum Jam ativo nesta conta.";m_poll.stop();m_idle.start();emit changed();return;}
    if(code<200||code>=300){m_cooldownUntil=QDateTime::currentMSecsSinceEpoch()+(code==429?60000:code==503?15000:5000);m_status=QString("Jam indisponível nesta sessão (HTTP %1). O protocolo experimental pode variar.").arg(code);m_poll.stop();m_idle.start();emit changed();return;}
    if(op=="leave"||op=="end"){++m_epoch;m_session.clear();m_poll.stop();m_queueBaseline=false;m_status="Você saiu do Jam.";m_idle.start();emit changed();return;}
    auto parsed=tonal::jam::parse(reply.value("result").toObject(),reply.value("viewer").toString());
    if(!parsed){if(op!="current"){QTimer::singleShot(0,this,&JamController::refresh);return;}m_status="O Spotify retornou um formato Jam não reconhecido.";m_poll.stop();emit changed();return;}
    if(m_session.value("id")!=parsed->value("id")){++m_epoch;m_queueBaseline=false;}
    for(const auto &name:tonal::jam::joined(m_session,*parsed))emit notice(name+" entrou no Jam");
    m_session=*parsed;m_status="Jam ativo • alterações confirmadas aparecem no player";m_poll.start();emit changed();pollQueue();
}
void JamController::pollQueue(){
    if(m_queueBusy||!m_session.value("active").toBool())return;m_queueBusy=true;const auto epoch=m_epoch;
    m_api->get("/me/player/queue",{},[this,epoch](int status,const QJsonDocument &doc){
        m_queueBusy=false;if(epoch!=m_epoch||status!=200)return;
        const auto object=doc.object();const auto anchor=object.value("currently_playing").toObject().value("uri").toString();
        if(anchor==m_player->trackUri()&&!m_player->queueEditPending()&&QDateTime::currentMSecsSinceEpoch()>=m_quietUntil){
            const auto old=m_player->preparedQueue();QVariantList observed;
            auto row=[](const QJsonObject &t){const auto a=t.value("album").toObject();QStringList artists;QString artistId;for(const auto &v:t.value("artists").toArray()){const auto artist=v.toObject();artists<<artist.value("name").toString();if(artistId.isEmpty())artistId=artist.value("id").toString();}const auto images=a.value("images").toArray();return QVariantMap{{"uri",t.value("uri").toString()},{"id",t.value("uri").toString()},{"provider","Spotify"},{"title",t.value("name").toString()},{"artist",artists.join(", ")},{"artistId",artistId},{"album",a.value("name").toString()},{"albumId",a.value("id").toString()},{"year",a.value("release_date").toString().left(4).toInt()},{"durationMs",t.value("duration_ms").toInt()},{"image",images.isEmpty()?QString():images.first().toObject().value("url").toString()}};};
            observed<<row(object.value("currently_playing").toObject());
            for(const auto &v:object.value("queue").toArray()){if(observed.size()>=101)break;if(!v.toObject().value("uri").toString().isEmpty())observed<<row(v.toObject());}
            std::vector<std::string> before,after;for(const auto &v:old)before.push_back(v.toMap().value("uri").toString().toStdString());for(const auto &v:observed)after.push_back(v.toMap().value("uri").toString().toStdString());
            const auto alignment=tonal::queue::align(before,after);
            for(int i=0;i<observed.size();++i)if(alignment.matches[i]>=0){auto merged=observed[i].toMap();const auto known=old[alignment.matches[i]].toMap();for(auto it=known.cbegin();it!=known.cend();++it)if(it.key().startsWith("mix"))merged[it.key()]=it.value();observed[i]=merged;}
            for(int i=alignment.suffix;i<old.size();++i)if(std::find(alignment.matches.begin(),alignment.matches.end(),i)==alignment.matches.end())observed<<old[i];emit queueCapacityExceeded(observed.size()>500);
            if(observed.size()<=500){std::vector<std::string> merged;for(const auto &v:observed)merged.push_back(v.toMap().value("uri").toString().toStdString());if(merged!=before)m_player->reconcileJamQueue(observed);}
        }
        QStringList ids,names;for(const auto &v:object.value("queue").toArray()){if(ids.size()>=100)break;const auto t=v.toObject();const auto uri=t.value("uri").toString();if(uri.isEmpty())continue;ids<<uri;names<<t.value("name").toString().left(160);}
        const bool notify=m_queueBaseline&&anchor==m_queueAnchor&&QDateTime::currentMSecsSinceEpoch()>=m_quietUntil;
        if(notify&&ids!=m_queueUris){QStringList remaining=m_queueUris,added;for(int i=0;i<ids.size();++i){int at=remaining.indexOf(ids[i]);if(at>=0)remaining.removeAt(at);else if(added.size()<3)added<<names[i];}
            emit notice(added.isEmpty()?"A ordem ou as músicas da fila do Jam foram alteradas":"Adicionada à fila do Jam: "+added.join(" • "));
        }
        m_queueAnchor=anchor;m_queueUris=ids;m_queueBaseline=true;
    });
}
