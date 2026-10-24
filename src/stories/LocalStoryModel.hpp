#pragma once
#include <string>
#include <vector>
namespace tonal::stories {
struct Result { std::string text,error;int tokens=0;long long elapsedMs=0; };
Result generate(const std::string &model,const std::vector<std::string> &texts,int deadlineMs=45000);
}
