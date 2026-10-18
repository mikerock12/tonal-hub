#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace tonal::mix {
struct Track { std::string id,artist,album,genres; int year=0; bool heard=false,artistHeard=false; unsigned themes=0; };
std::vector<int> order(int count,uint32_t seed);
// Nonnegative indices refer to source; -(n+1) refers to the external pool.
std::vector<int> plan(const std::vector<Track>& source,const std::vector<Track>& pool,int selected,bool random,bool discover,uint32_t seed,bool album=false);
std::vector<std::string> queries(const std::string& genres,int year);
}
