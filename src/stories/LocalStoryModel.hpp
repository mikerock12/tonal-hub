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
 long long elapsedMs=0;
};
Result generate(const std::string &model,const std::vector<std::string> &texts,int deadlineMs=90000,const std::vector<std::string> &known={});
std::string reportJson(const Result &result);
}
