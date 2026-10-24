#include "LocalStoryModel.hpp"
#include <iostream>
int main(){
 using tonal::stories::generate;
 auto empty=generate("missing-model.gguf",{},1000);
 auto excessive=generate("missing-model.gguf",std::vector<std::string>(9,"synthetic evidence"),1000);
 auto missing=generate("missing-model.gguf",{"Original synthetic song description about moving forward."},1000);
 if(empty.error.empty()||excessive.error.empty()||missing.error.empty()||!empty.text.empty()||!missing.text.empty())return 1;
 std::cout<<"STORY_GUARDS_PASS: missing weights, empty evidence, excessive batch; no invented narrative\n";
}
