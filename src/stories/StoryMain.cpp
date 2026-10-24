#include "LocalStoryModel.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#endif
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
#ifdef _WIN32
 SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);
#endif
 if(app.arguments().size()!=2)return 2;QFile in;if(!in.open(stdin,QIODevice::ReadOnly))return 3;auto bytes=in.read(32769);if(bytes.size()>32768)return 4;
 auto object=QJsonDocument::fromJson(bytes).object();auto rows=object["texts"].toArray();std::vector<std::string> texts;for(auto v:rows)texts.push_back(v.toString().toUtf8().toStdString());
 auto result=tonal::stories::generate(app.arguments()[1].toUtf8().toStdString(),texts,object["deadlineMs"].toInt(45000));
 auto output=QJsonDocument(QJsonObject{{"summary",QString::fromUtf8(result.text)},{"error",QString::fromUtf8(result.error)},{"tokens",result.tokens},{"elapsedMs",double(result.elapsedMs)},{"engine","Qwen2.5 0.5B Q4_K_M / llama.cpp"}}).toJson(QJsonDocument::Compact);fwrite(output.data(),1,output.size(),stdout);fflush(stdout);return result.error.empty()?0:1;
}
