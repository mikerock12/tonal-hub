#include "LocalStories.h"
#include <QDir>
#include <QFile>
#include <QMap>
#include <algorithm>

LocalStories::LocalStories(const QString &root,QObject *parent):QObject(parent),m_root(root){removeLegacyFiles();}

void LocalStories::removeLegacyFiles(){
 for(const auto &name:{"Qwen3.5-2B-Q4_K_M.gguf","qwen2.5-0.5b-instruct-q4_k_m.gguf","verified.json","download.lock","resume-v5.bin","resume-v5.bin.tmp"})QFile::remove(m_root+"/"+name);
 QDir partials(m_root);for(const auto &name:partials.entryList({"download-*.part"},QDir::Files))QFile::remove(m_root+"/"+name);
 QDir summaries(m_root+"/summaries");for(const auto &name:summaries.entryList({"*.json"},QDir::Files))QFile::remove(summaries.absoluteFilePath(name));
}

void LocalStories::selectPeriod(int days){if(m_days==days)return;m_days=days;m_summary.clear();m_sources.clear();m_items.clear();m_state["summary"]={};m_state["sources"]={};m_state["items"]={};emit changed();}
void LocalStories::resetSummary(){m_summary.clear();m_sources.clear();m_items.clear();removeLegacyFiles();m_state["summary"]={};m_state["sources"]={};m_state["items"]={};m_state["status"]="Resumo de gêneros e clima por regras locais, sem modelo.";emit changed();}
void LocalStories::download(){m_state["status"]="Nenhum modelo ou download é necessário.";emit changed();}
void LocalStories::cancel(){m_state["status"]="Resumo local por regras; nenhuma inferência em andamento.";emit changed();}
void LocalStories::removeModel(){removeLegacyFiles();m_state["status"]="Os pesos antigos foram removidos. Este resumo não usa modelo.";emit changed();}

void LocalStories::analyze(const QVariantList &tracks,int days){
 m_days=days;m_summary.clear();m_sources.clear();m_items.clear();
 if(tracks.isEmpty()){m_state["status"]="Não há músicas ouvidas neste período.";m_state["summary"]={};m_state["sources"]={};m_state["items"]={};emit changed();return;}
 QMap<QString,int> genres,moods;QStringList labels;int counted=0;
 for(const auto &value:tracks){if(counted>=8)break;const auto track=value.toMap();++counted;
  QString genre=track.value("genre").toString().trimmed();if(genre.isEmpty())genre="Gênero não identificado";
  QString mood="clima não identificado",bestMood;double best=0;const auto tone=track.value("tone").toMap();for(const auto &name:{"intenso","contemplativo","afetivo","festivo"}){const double score=tone.value(name).toDouble();if(score>best){best=score;bestMood=name;}}
  if(!bestMood.isEmpty())mood=bestMood;
  genres[genre]++;if(!bestMood.isEmpty())moods[bestMood]++;
  m_items<<QVariantMap{{"title",track.value("title")},{"artist",track.value("artist")},{"accepted",true},{"pending",false},{"theme",genre+" • "+mood},{"meaning","Contribui para o resumo com o gênero e a pista de clima já registrados nesta biblioteca."}};
  labels<<track.value("title").toString()+" — "+track.value("artist").toString();
 }
 m_sources=labels.join(" • ");
 auto top=[](QMap<QString,int> counts){QList<QPair<QString,int>> rows;for(auto it=counts.begin();it!=counts.end();++it)rows.append({it.key(),it.value()});std::sort(rows.begin(),rows.end(),[](const auto &a,const auto &b){return a.second>b.second;});QStringList names;for(int i=0;i<std::min(2,int(rows.size()));++i)names<<rows[i].first;return names.join(" e ");};
 const QString topGenres=top(genres),topMoods=top(moods);
 m_summary="Neste período, os gêneros mais presentes são "+(topGenres.isEmpty()?"ainda não identificados":topGenres)+".";
 if(!topMoods.isEmpty())m_summary+=" As pistas de clima mais frequentes são "+topMoods+".";else m_summary+=" Ainda não há pistas de clima suficientes nestas faixas.";
 m_summary+=QString(" Resumo por regras de %1 %2, sem modelo de IA.").arg(counted).arg(counted==1?"faixa":"faixas");
 m_state["summary"]=m_summary;m_state["sources"]=m_sources;m_state["items"]=m_items;m_state["status"]=QString("Resumo local por regras • %1 %2 • sem download ou inferência.").arg(counted).arg(counted==1?"faixa":"faixas");m_state["modelBytes"]=0;m_state["legacyBytes"]=0;m_state["cacheBytes"]=0;m_state["engine"]="Regras locais • sem modelo";emit changed();
}
