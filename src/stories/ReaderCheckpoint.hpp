#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace tonal::stories {
// One private, disposable checkpoint per reader. Never part of a library backup.
// The native state is tied to the exact model/prompt/batch key and runtime v5.
inline constexpr size_t checkpointLimit=64*1024*1024;
#ifdef TONAL_STORY_ARM_FAST
inline constexpr char checkpointMagic[]="TonalF5c3f03b177";
#else
inline constexpr char checkpointMagic[]="TonalR5c3f03b177";
#endif
struct ReaderCheckpoint {
 uint32_t phase=0,track=0,cursor=0,generated=0;
 int32_t nextToken=-1;
 float reviewMargin=0;
 std::string draft;
 std::vector<uint8_t> state;
};
inline bool checkpointKey(const std::string &key){if(key.size()!=64)return false;for(char c:key)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
inline uint64_t checkpointHash(const uint8_t *data,size_t n){uint64_t value=14695981039346656037ULL;for(size_t i=0;i<n;++i){value^=data[i];value*=1099511628211ULL;}return value;}
inline bool validCheckpoint(const ReaderCheckpoint &v){return v.phase<=3&&v.track<8&&v.cursor<=768&&v.generated<=64&&(v.phase!=1||v.nextToken>=0)&&std::isfinite(v.reviewMargin)&&v.draft.size()<=700&&!v.state.empty()&&v.state.size()<=checkpointLimit-2048;}
inline std::vector<uint8_t> checkpointBytes(const ReaderCheckpoint &v,const std::string &key){
 if(!checkpointKey(key)||!validCheckpoint(v))return {};
 std::vector<uint8_t> out;out.insert(out.end(),checkpointMagic,checkpointMagic+16);out.insert(out.end(),key.begin(),key.end());
 auto number=[&](uint64_t n,int bytes){for(int i=0;i<bytes;++i)out.push_back(uint8_t(n>>(i*8)));};
 for(auto n:{v.phase,v.track,v.cursor,v.generated,uint32_t(v.draft.size())})number(n,4);
 number(uint32_t(v.nextToken),4);uint32_t margin;std::memcpy(&margin,&v.reviewMargin,4);number(margin,4);
 number(v.state.size(),8);out.insert(out.end(),v.draft.begin(),v.draft.end());out.insert(out.end(),v.state.begin(),v.state.end());number(checkpointHash(out.data(),out.size()),8);return out;
}
inline bool decodeCheckpoint(const std::vector<uint8_t> &data,const std::string &key,ReaderCheckpoint &value){
 constexpr size_t header=16+64+20+8+8;
 if(!checkpointKey(key)||data.size()<header+9||data.size()>checkpointLimit||std::memcmp(data.data(),checkpointMagic,16)||std::memcmp(data.data()+16,key.data(),64))return false;
 size_t at=80;auto number=[&](int n){uint64_t v=0;for(int i=0;i<n;++i)v|=uint64_t(data[at++])<<(i*8);return v;};
 ReaderCheckpoint v;v.phase=uint32_t(number(4));v.track=uint32_t(number(4));v.cursor=uint32_t(number(4));v.generated=uint32_t(number(4));auto length=number(4);v.nextToken=int32_t(number(4));uint32_t margin=uint32_t(number(4));std::memcpy(&v.reviewMargin,&margin,4);auto state=number(8);
 if(length>700||state>checkpointLimit-2048||header+length+state+8!=data.size())return false;
 size_t end=data.size()-8;at=end;const auto checksum=number(8);if(checksum!=checkpointHash(data.data(),end))return false;
 v.draft.assign(reinterpret_cast<const char*>(data.data()+header),size_t(length));v.state.assign(data.begin()+header+length,data.begin()+end);if(!validCheckpoint(v))return false;value=std::move(v);return true;
}
inline bool readCheckpoint(const std::string &path,const std::string &key,ReaderCheckpoint &value){
 if(path.empty())return false;std::error_code error;auto n=std::filesystem::file_size(std::filesystem::u8path(path),error);if(error||n>checkpointLimit||n<116)return false;
 std::ifstream in(std::filesystem::u8path(path),std::ios::binary);std::vector<uint8_t> data(size_t(n),0);if(!in.read(reinterpret_cast<char*>(data.data()),std::streamsize(n)))return false;return decodeCheckpoint(data,key,value);
}
inline bool writeCheckpoint(const std::string &path,const std::string &key,const ReaderCheckpoint &value){
 if(path.empty())return false;auto data=checkpointBytes(value,key);if(data.empty())return false;const auto file=std::filesystem::u8path(path),temporary=std::filesystem::u8path(path+".tmp");
 {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()))||!out.flush())return false;}
#ifdef _WIN32
 return MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
 return std::rename(temporary.c_str(),file.c_str())==0;
#endif
}
inline void clearCheckpoint(const std::string &path){if(path.empty())return;std::error_code error;std::filesystem::remove(std::filesystem::u8path(path),error);std::filesystem::remove(std::filesystem::u8path(path+".tmp"),error);}
}
