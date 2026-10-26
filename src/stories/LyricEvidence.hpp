#pragma once
#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_set>
namespace tonal::stories {
// Compact repeated chorus lines without adding new lyrical content. Bounded
// input comes from the reader; no network or model calls happen here.
inline std::string compactEvidence(const std::string &text){
 std::istringstream input(text);std::string line,result;std::unordered_set<std::string> seen;
 while(std::getline(input,line)){
  auto first=line.find_first_not_of(" \t\r");if(first==std::string::npos)continue;
  line=line.substr(first,line.find_last_not_of(" \t\r")-first+1);
  if(line=="[…]"||!seen.insert(line).second)continue;
  if(!result.empty())result+='\n';result+=line;
 }
 return result;
}
inline std::string boundedEvidence(const std::string &text,size_t limit=3000){
 auto value=compactEvidence(text);if(value.size()<=limit)return value;
 if(limit<32)return {};
 const auto part=(limit-24)/3;
 auto slice=[&](size_t start,size_t length){while(start<value.size()&&(static_cast<unsigned char>(value[start])&0xc0)==0x80)++start;
  size_t end=std::min(value.size(),start+length);while(end>start&&end<value.size()&&(static_cast<unsigned char>(value[end])&0xc0)==0x80)--end;return value.substr(start,end-start);};
 return slice(0,part)+"\n[…]\n"+slice(value.size()/2,part)+"\n[…]\n"+slice(value.size()-part,part);
}
}
