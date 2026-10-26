#pragma once
#include <string>
#include <vector>
namespace tonal::stories {
struct Reading {
 int track=0;
 bool accepted=false,pending=true,reviewed=false;
 std::string meaning;
};
struct Result {
 std::string text,error,notice;
 std::vector<Reading> readings;
 int tokens=0,total=0,qualified=0;
 bool complete=false;
 bool resumable=false;
 bool resumed=false;
 int progressTrack=0,progressDone=0,progressTotal=0;
 std::string progressStage;
 long long elapsedMs=0;
};
Result generate(const std::string &model,const std::vector<std::string> &texts,int deadlineMs=90000,const std::vector<std::string> &known={},const std::string &checkpointPath={},const std::string &batchKey={});
std::string reportJson(const Result &result);
}
