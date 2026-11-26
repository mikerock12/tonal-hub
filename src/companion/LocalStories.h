#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

// Deterministic genre/mood summary. No model, network, inference or playback dependency.
class LocalStories : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
public:
 explicit LocalStories(const QString &root,QObject *parent=nullptr);
 QVariantMap state()const{return m_state;}
 void capture(const QVariantMap &,const QString &){}
 void analyze(const QVariantList &,int);
 void selectPeriod(int);
 void resetSummary();
 void setPlaying(bool){}
 Q_INVOKABLE void download();
 Q_INVOKABLE void cancel();
 Q_INVOKABLE void removeModel();
signals:
 void changed();
private:
 void removeLegacyFiles();
 QString m_root,m_summary,m_sources;
 QVariantList m_items;
 int m_days=7;
 QVariantMap m_state{{"ready",true},{"busy",false},{"downloading",false},{"summary",QString()},{"status",QStringLiteral("Resumo de gêneros e clima por regras locais, sem modelo.")}};
};
