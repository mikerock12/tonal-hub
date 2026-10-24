#include "PersonalDatabase.h"
#include <QFileInfo>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
#include <QUuid>
#include <functional>

namespace {
bool database(const QString &path,const std::function<bool(QSqlDatabase &)> &fn){
  auto name=QUuid::createUuid().toString();bool ok=false;
  {auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(path);db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
    if(db.open()) {QSqlQuery q(db);q.exec("PRAGMA journal_mode=WAL");q.exec("PRAGMA synchronous=FULL");
      bool schema=q.exec("CREATE TABLE IF NOT EXISTS settings(id TEXT PRIMARY KEY,json TEXT NOT NULL)")&&
        q.exec("CREATE TABLE IF NOT EXISTS tracks(id TEXT PRIMARY KEY,provider TEXT,title TEXT,artist TEXT,genre TEXT,json TEXT NOT NULL)")&&
        q.exec("CREATE TABLE IF NOT EXISTS listens(id TEXT PRIMARY KEY,track_id TEXT,started_at INTEGER,listened_ms INTEGER,qualified INTEGER,json TEXT NOT NULL)")&&
        q.exec("CREATE TABLE IF NOT EXISTS favorites(id TEXT PRIMARY KEY,position INTEGER,json TEXT NOT NULL)")&&
        q.exec("CREATE TABLE IF NOT EXISTS playlists(id TEXT PRIMARY KEY,name TEXT,position INTEGER,json TEXT NOT NULL)")&&
        q.exec("CREATE TABLE IF NOT EXISTS playlist_items(playlist_id TEXT,position INTEGER,track_id TEXT,PRIMARY KEY(playlist_id,position))")&&
        q.exec("CREATE INDEX IF NOT EXISTS listens_date ON listens(started_at)")&&q.exec("CREATE INDEX IF NOT EXISTS tracks_artist ON tracks(artist)");
      if(schema){q.exec("PRAGMA user_version=1");ok=fn(db);}db.close();}}
  QSqlDatabase::removeDatabase(name);return ok;
}
QByteArray json(const QJsonObject &o){return QJsonDocument(o).toJson(QJsonDocument::Compact);}
QMap<QString,QJsonObject> rows(const QJsonObject &data,const QString &table){
  QMap<QString,QJsonObject> out;
  if(table=="tracks"){auto all=data["tracks"].toObject();for(auto it=all.begin();it!=all.end();++it)out[it.key()]=it.value().toObject();return out;}
  if(table=="settings"){auto value=data;for(auto k:{"tracks","events","favorites","playlists"})value.remove(k);out["preferences"]=value;return out;}
  auto array=data[table=="listens"?"events":table].toArray();int n=0;
  for(auto v:array){QJsonObject o=table=="favorites"?QJsonObject{{"key",v},{"position",n}}:v.toObject();if(table=="playlists")o["position"]=n;
    QString id=table=="favorites"?v.toString():table=="playlists"?o["id"].toString():o["session"].toString();if(id.isEmpty())id=QString::number(n)+"|"+o["key"].toString();out[id]=o;n++;}return out;
}
}
namespace personal {
QJsonObject load(const QString &path){
  QJsonObject result;if(!QFileInfo::exists(path))return result;
  database(path,[&](auto &db){QSqlQuery q(db);q.exec("SELECT json FROM settings WHERE id='preferences'");if(q.next())result=QJsonDocument::fromJson(q.value(0).toByteArray()).object();
    QJsonObject tracks;q.exec("SELECT id,json FROM tracks WHERE id IN (SELECT id FROM tracks ORDER BY rowid DESC LIMIT 3000) OR id IN (SELECT id FROM favorites) OR id IN (SELECT track_id FROM playlist_items)");while(q.next())tracks[q.value(0).toString()]=QJsonDocument::fromJson(q.value(1).toByteArray()).object();result["tracks"]=tracks;
    for(auto table:{"listens","favorites","playlists"}){QJsonArray list;QString order=QString(table)=="listens"?"started_at,id":"position";q.exec(QString(table)=="listens"?"SELECT json FROM (SELECT id,started_at,json FROM listens ORDER BY started_at DESC,id DESC LIMIT 10000) ORDER BY started_at,id":"SELECT json FROM "+QString(table)+" ORDER BY "+order);while(q.next()){auto row=QJsonDocument::fromJson(q.value(0).toByteArray()).object();if(QString(table)=="favorites")list.append(row["key"]);else {row.remove("position");list.append(row);}}result[QString(table)=="listens"?"events":table]=list;}return true;});return result;
}
bool save(const QString &path,const QJsonObject &data,QJsonObject &previous){
  bool ok=database(path,[&](auto &db){if(!db.transaction())return false;bool success=true;QJsonObject oldData=previous;
    if(data.value("libraryEpoch")!=previous.value("libraryEpoch")){for(auto table:{"settings","tracks","listens","favorites","playlists","playlist_items"}){QSqlQuery q(db);success=q.exec("DELETE FROM "+QString(table))&&success;}oldData={};}
    else if(data.value("historyEpoch")!=previous.value("historyEpoch")){QSqlQuery q(db);success=q.exec("DELETE FROM listens")&&success;oldData["events"]=QJsonArray();}
    for(const auto &table:QStringList{"settings","tracks","listens","favorites","playlists"}){
      auto next=rows(data,table),old=rows(oldData,table);
      for(auto it=old.begin();it!=old.end();++it)if(table!="listens"&&!next.contains(it.key())){QSqlQuery q(db);q.prepare("DELETE FROM "+table+" WHERE id=?"+(table=="tracks"?" AND NOT EXISTS(SELECT 1 FROM listens WHERE track_id=tracks.id)":""));q.addBindValue(it.key());success=q.exec()&&success;}
      for(auto it=next.begin();it!=next.end();++it){if(old.value(it.key())==it.value())continue;auto row=it.value();QStringList columns{"id","json"};QVariantList values{it.key(),json(row)};
        auto add=[&](QString k,QVariant v){columns<<k;values<<v;};
        if(table=="tracks"){for(auto k:{"provider","title","artist","genre"})add(k,row[k].toString());}
        if(table=="listens"){add("track_id",row["key"].toString());add("started_at",qint64(row["at"].toDouble()));add("listened_ms",qint64(row["ms"].toDouble()));add("qualified",row["play"].toBool());}
        if(table=="favorites"||table=="playlists")add("position",row["position"].toInt());if(table=="playlists")add("name",row["name"].toString());
        QStringList placeholders;for(int i=0;i<columns.size();i++)placeholders<<"?";QSqlQuery q(db);q.prepare("INSERT OR REPLACE INTO "+table+" ("+columns.join(',')+") VALUES ("+placeholders.join(',')+")");for(auto value:values)q.addBindValue(value);success=q.exec()&&success;
      }
    }
    if(data["playlists"]!=oldData["playlists"]){QSqlQuery clear(db);success=clear.exec("DELETE FROM playlist_items")&&success;for(auto v:data["playlists"].toArray()){auto p=v.toObject();int pos=0;for(auto t:p["tracks"].toArray()){QSqlQuery q(db);q.prepare("INSERT INTO playlist_items VALUES (?,?,?)");q.addBindValue(p["id"].toString());q.addBindValue(pos++);q.addBindValue(t.toString());success=q.exec()&&success;}}}
    if(success)return db.commit();db.rollback();return false;});if(ok)previous=data;return ok;
}
}

namespace personal {
QJsonObject historySnapshot(const QString &path,const QJsonObject &current,qint64 cutoff){
 QJsonObject result=current;QJsonObject tracks=current["tracks"].toObject();QJsonArray events=current["events"].toArray();
 const bool ok=database(path,[&](auto &db){
  QSqlQuery q(db);if(!q.exec("CREATE TEMP TABLE active_sessions(id TEXT PRIMARY KEY)"))return false;
  q.prepare("INSERT OR IGNORE INTO active_sessions VALUES(?)");int index=0;
  for(auto v:events){auto e=v.toObject();QString id=e["session"].toString();if(id.isEmpty())id=QString::number(index)+"|"+e["key"].toString();q.bindValue(0,id);if(!q.exec())return false;++index;}
  q.exec("SELECT id,json FROM tracks WHERE id IN (SELECT track_id FROM listens WHERE id IN (SELECT id FROM active_sessions))");
  while(q.next()){QString id=q.value(0).toString();if(!tracks.contains(id))tracks[id]=QJsonDocument::fromJson(q.value(1).toByteArray()).object();}
  q.prepare("SELECT l.track_id,MIN(l.started_at),SUM(l.listened_ms),SUM(l.qualified),t.json FROM listens l JOIN tracks t ON t.id=l.track_id WHERE l.started_at>=? AND l.started_at<=? AND NOT EXISTS(SELECT 1 FROM active_sessions a WHERE a.id=l.id) GROUP BY l.track_id,date(l.started_at/1000,'unixepoch','localtime')");
  q.addBindValue(cutoff);q.addBindValue(QDateTime::currentMSecsSinceEpoch());if(!q.exec())return false;
  while(q.next()){QString key=q.value(0).toString();if(!tracks.contains(key))tracks[key]=QJsonDocument::fromJson(q.value(4).toByteArray()).object();events.append(QJsonObject{{"key",key},{"at",q.value(1).toDouble()},{"ms",q.value(2).toDouble()},{"plays",q.value(3).toInt()}});}
  return true;
 });if(!ok)result["historyError"]=true;result["tracks"]=tracks;result["events"]=events;return result;
}
QJsonObject fullSnapshot(const QString &path){
 QJsonObject result;
 database(path,[&](auto &db){QSqlQuery q(db);q.exec("SELECT json FROM settings WHERE id='preferences'");if(q.next())result=QJsonDocument::fromJson(q.value(0).toByteArray()).object();QJsonObject tracks;q.exec("SELECT id,json FROM tracks");while(q.next())tracks[q.value(0).toString()]=QJsonDocument::fromJson(q.value(1).toByteArray()).object();result["tracks"]=tracks;
 for(auto table:{"listens","favorites","playlists"}){QJsonArray list;QString order=QString(table)=="listens"?"started_at,id":"position";q.exec("SELECT json FROM "+QString(table)+" ORDER BY "+order);while(q.next()){auto row=QJsonDocument::fromJson(q.value(0).toByteArray()).object();if(QString(table)=="favorites")list.append(row["key"]);else{row.remove("position");list.append(row);}}result[QString(table)=="listens"?"events":table]=list;}return true;});return result;
}
}
