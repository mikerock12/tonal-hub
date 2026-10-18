#include "CoreCases.hpp"
#include <iostream>
int main(){try{playlistCoreCases();std::cout<<"PLAYLIST_CORE_PASS shared C++ parser/ranking/bounds\n";return 0;}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
