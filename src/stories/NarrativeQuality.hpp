#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>
namespace tonal::stories {
inline std::vector<std::string> narrativeWords(const std::string &text){
 std::vector<std::string> words;std::string word;
 for(unsigned char c:text){if(std::isalpha(c)||c>=128)word+=char(c<128?std::tolower(c):c);else if(!word.empty()){words.push_back(word);word.clear();}}
 if(!word.empty())words.push_back(word);return words;
}
inline std::string phrase(const std::vector<std::string> &words,size_t start,size_t size){std::string value;for(size_t n=start;n<start+size;++n){value+=words[n];value+=' ';}return value;}
// A conservative output-format gate, not a proof that an interpretation is true.
// Semantic quality is evaluated separately against reviewed lyric fixtures.
inline bool usefulNarrative(const std::string &text,const std::string &evidence={}){
 if(text.size()<40||text.size()>700||text.find('<')!=std::string::npos||text.find('>')!=std::string::npos)return false;
 auto words=narrativeWords(text);if(words.size()<8)return false;
 const std::unordered_set<std::string> filler{"a","o","as","os","um","uma","de","da","do","das","dos","em","no","na","nos","nas","e","ou","que","se","com","por","para","sobre","ao","the","a","an","of","and","or","to","in","is","it","this","that","letra","letras","texto","textos","trecho","trechos","conjunto","reúne","reune","aborda","trata","fala","expressa","temas","tema","sentimentos","emoções","contrastes","contrastam"};
 std::unordered_set<std::string> content;
 std::string normalized=phrase(words,0,words.size());
 for(auto banned:{"o ouvinte ","o usuário ","do usuário ","the listener ","seu humor ","sua saúde ","sem copiar ","em uma frase ","não fale ","main conflict or idea ","contrastam e contrastam "})if(normalized.find(banned)!=std::string::npos)return false;
 for(auto &word:words)if(word.size()>2&&!filler.count(word))content.insert(word);
 if(content.size()<5)return false;
 std::unordered_set<std::string> triples;
 for(size_t n=0;n+3<=words.size();++n)if(!triples.insert(phrase(words,n,3)).second)return false;
 auto source=narrativeWords(evidence);
 for(size_t n=0;n+8<=words.size();++n){auto quoted=phrase(words,n,8);for(size_t at=0;at+8<=source.size();++at)if(quoted==phrase(source,at,8))return false;}
 return true;
}
}
