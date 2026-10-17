#include "TinyMusicModel.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace tonal::muse {
namespace {
#include "MuseWeights.inc"
uint32_t hash(std::string_view text){uint32_t h=2166136261u;for(unsigned char b:text)h=(h^b)*16777619u;return h%kDimensions;}
}
const std::array<const char*,labelsCount>& labels(){return kLabels;}
std::array<float,labelsCount> analyze(std::string_view text){
  text=text.substr(0,12000);std::array<bool,kDimensions> seen{};std::vector<int> features;features.reserve(800);
  auto add=[&](const std::string &s){if(features.size()>=768)return;auto i=hash(s);if(!seen[i]){seen[i]=true;features.push_back(int(i));}};
  std::string previous;int neg=0,words=0;
  for(size_t start=0;start<text.size()&&words<1200;){
    while(start<text.size()&&text[start]==' ')++start;if(start>=text.size())break;
    auto end=text.find(' ',start);if(end==std::string_view::npos)end=text.size();std::string word(text.substr(start,end-start));start=end;++words;
    if(word=="not"||word=="never"||word=="without"||word=="nao"||word=="nunca"||word=="sem"||word=="sin"||word=="no")neg=3;
    const std::string prefix=neg?"n:":"w:";add(prefix+word);if(!previous.empty())add("b:"+previous+" "+word);
    if(word.size()>=5)for(size_t i=0;i+4<=word.size();++i)add(prefix+"#"+word.substr(i,4));
    previous=word;neg=std::max(0,neg-1);if(features.size()>=768)break;
  }
  std::sort(features.begin(),features.end());const float scale=1.f/std::sqrt(float(std::max(size_t(1),features.size())));std::array<float,labelsCount> scores{};
  for(int j=0;j<labelsCount;++j){float value=0;for(int i:features)value+=kWeights[j][i]*kScales[j];value=std::clamp(kBias[j]+value*scale,-20.f,20.f);scores[j]=1.f/(1.f+std::exp(-value));}
  return scores;
}
}
