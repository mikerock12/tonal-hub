#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

namespace personal {
// An old free-form sentence is never a valid structured report. Metadata is
// attached only after validating every index against this exact prepared batch.
inline QJsonObject storyReport(const QJsonObject &report,const QJsonArray &tracks){
 const auto rows=report.value("items").toArray();const int count=tracks.size();
 if(report.value("format").toString()!="lyric-reading-v4"||!report.value("complete").isBool()||report.value("schema").toInt()!=2||!report.value("error").toString().isEmpty()||
    count<1||count>8||report.value("total").toInt()!=count||rows.size()!=count)return {};
 QSet<int> seen;QJsonArray items;int accepted=0;bool pending=false;
 for(const auto &value:rows){
  if(!value.isObject())return {};auto item=value.toObject();const auto index=item.value("track");
  const int at=index.toInt(-1);const auto code=item.value("code").toString();
  const bool ok=item.value("accepted").toBool(),waiting=item.value("pending").toBool();pending|=waiting;
  if(!index.isDouble()||index.toDouble()!=at||at<0||at>=count||seen.contains(at)||code.size()!=1||
     (ok?code!="A":code!="L")||!item.value("pending").isBool()||(ok&&waiting)||(ok&&!item.value("reviewed").toBool())||!item.value("accepted").isBool()||
     item.value("theme").toString().trimmed().isEmpty()||item.value("theme").toString().size()>80||
     item.value("meaning").toString().trimmed().size()<20||item.value("meaning").toString().size()>512||item.value("meaning").toString().toUtf8().size()>700)return {};
  seen.insert(at);if(ok)++accepted;
  const auto track=tracks[at].toObject();
  items.append(QJsonObject{{"title",track.value("title").toString().left(240)},
   {"artist",track.value("artist").toString().left(160)}, {"accepted",ok},
   {"pending",waiting},{"theme",item.value("theme")},{"meaning",item.value("meaning")}});
 }
 if(report.value("complete").toBool()==pending||accepted==0||report.value("qualified").toInt()!=accepted)return {};
 const auto summary=report.value("summary").toString().trimmed();
 if(summary.size()<20||summary.size()>1400)return {};
 return {{"summary",summary},{"complete",!pending},{"items",items},{"qualified",accepted},{"total",count}};
}
}
