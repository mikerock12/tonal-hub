#pragma once
#include "insights/TinyMusicModel.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <cmath>

inline void testMuse(){
  using namespace tonal::muse;
  auto require=[](bool yes,const char* reason){if(!yes)throw std::runtime_error(reason);};
  auto love=analyze("love and affection a tender kiss loving you a warm embrace");
  require(love[0]>.60f,"music model failed affection fixture");
  auto neutral=analyze("album and artist metadata guitars bass and drums instrumental introduction");
  for(float p:neutral)require(std::isfinite(p)&&p>=0&&p<=1&&p<.60,"neutral text did not abstain");
  require(analyze("")==analyze(""),"model is not deterministic");
  std::string oversized(200000,'a');require(analyze(oversized)==analyze(std::string_view(oversized).substr(0,12000)),"model input is not bounded");
  const QStringList samples={
    "Your gentle touch makes our hearts feel close", "I miss the home where we spent our days", "A goodbye left tears and a heart in pieces", "We demand freedom and resist oppressive rulers",
    "After the storm I believe in a brighter tomorrow", "Friends dance and laugh at the party", "Dark shadows fill the night with fear", "In silence I question the meaning of my choices",
    "O carinho do seu abraço aquece meu coração", "Sinto falta da nossa casa e das antigas lembranças", "A despedida deixou tristeza e um coração partido", "Nossa luta exige liberdade contra a opressão",
    "Depois de cair acredito em um novo começo", "Os amigos cantam e dançam de alegria", "O perigo da noite me deixa inquieto", "No silêncio tento entender quem sou"
  };
  QJsonArray predictions;int accepted=0,correct=0;
  for(int i=0;i<samples.size();++i){QString normalized=samples[i].normalized(QString::NormalizationForm_D).toLower();normalized.remove(QRegularExpression("[\\x{0300}-\\x{036f}]"));normalized.replace(QRegularExpression("[^\\p{L}\\p{N}]+")," ");auto scores=analyze(normalized.trimmed().toStdString());QJsonObject row{{"expected",labels()[i%8]}};QJsonObject probabilities;int top=0;for(int j=0;j<labelsCount;++j){probabilities[labels()[j]]=double(scores[j]);if(scores[j]>scores[top])top=j;}bool abstained=scores[top]<.60f;if(!abstained){++accepted;if(top==i%8)++correct;}row["predicted"]=abstained?"abstained":labels()[top];row["scores"]=probabilities;predictions.append(row);}
  QElapsedTimer clock;clock.start();double checksum=0;for(int i=0;i<10000;++i)checksum+=analyze("love and affection a tender kiss loving you a warm embrace")[0];double micros=clock.nsecsElapsed()/1000./10000;require(checksum>0,"inference was not executed");
  QJsonObject report{{"model","TonalMuse Tiny v1"},{"weight_payload_bytes",32832},{"independent_synthetic_examples",samples.size()},{"accepted",accepted},{"correct_accepted",correct},{"abstained",samples.size()-accepted},{"precision_when_accepted",accepted?double(correct)/accepted:0},{"top1_recall_with_abstention",double(correct)/samples.size()},{"mean_inference_us",micros},{"predictions",predictions},{"limits","Small independently authored synthetic sanity set, not real-song accuracy or calibrated probabilities."}};
  auto args=QCoreApplication::arguments();int at=args.indexOf("--muse-report");if(at>=0){QFile f(args.value(at+1));require(f.open(QIODevice::WriteOnly),"cannot save model evaluation");f.write(QJsonDocument(report).toJson());}
  std::cout<<"MUSE_PASS: deterministic, bounded, neutral abstention; inference "<<micros<<" us; synthetic accepted "<<accepted<<"/"<<samples.size()<<", correct "<<correct<<"\n";
}
