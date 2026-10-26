#include "LocalStoryModel.hpp"
#include "NarrativeQuality.hpp"
#include "LyricEvidence.hpp"
#include "ReaderCheckpoint.hpp"
#include "CpuDispatch.hpp"
#include <iostream>
int main(){
 if(!tonal::stories::acceleratedReaderSupported(true,true,true,true,true,true,true))return 15;
 for(int missing=0;missing<7;++missing){bool flags[7]={true,true,true,true,true,true,true};flags[missing]=false;if(tonal::stories::acceleratedReaderSupported(flags[0],flags[1],flags[2],flags[3],flags[4],flags[5],flags[6]))return 16;}
 const std::string key(64,'a');tonal::stories::ReaderCheckpoint progress;progress.phase=2;progress.track=0;progress.cursor=32;progress.generated=20;progress.draft="Uma interpretação candidata ainda não conferida.";progress.state={1,2,3,4};
 auto encoded=tonal::stories::checkpointBytes(progress,key);tonal::stories::ReaderCheckpoint restored;
 if(!tonal::stories::decodeCheckpoint(encoded,key,restored)||restored.cursor!=32||restored.draft!=progress.draft||restored.state!=progress.state)return 11;
 if(tonal::stories::decodeCheckpoint(encoded,std::string(64,'b'),restored))return 12;
 encoded.back()^=1;if(tonal::stories::decodeCheckpoint(encoded,key,restored))return 13;
 progress.cursor=769;if(!tonal::stories::checkpointBytes(progress,key).empty())return 14;
 using tonal::stories::generate;
 if(tonal::stories::compactEvidence("  One line\nDifferent meaning\nOne line\n  Different meaning \nLast line")!="One line\nDifferent meaning\nLast line")return 9;
 std::string longEvidence="First verse matters.\n";for(int i=0;i<100;++i)longEvidence+="A repeated chorus with ação and 🎵.\n";longEvidence+="Final verse changes the meaning.";
 auto bounded=tonal::stories::boundedEvidence(longEvidence,120);
 if(bounded.size()>120||bounded.find("Final verse")==std::string::npos||bounded.find("First verse")==std::string::npos)return 10;
 using tonal::stories::usefulNarrative;
 if(usefulNarrative("O conjunto reúne letras que contrastam e contrastam."))return 2;
 if(usefulNarrative("A letra apresenta uma complexidade da vida e repete a complexidade da vida."))return 3;
 if(usefulNarrative("A noite passou por cima de mim, mas hoje abri as cortinas.","A noite passou por cima de mim, mas hoje abri as cortinas. Planto uma semente."))return 4;
 if(usefulNarrative("A letra aborda a dor da vida, sem copiar versos e sem falar do ouvinte."))return 5;
 if(!usefulNarrative("A lembrança de uma pessoa ausente mantém o narrador preso ao passado e dificulta seguir adiante."))return 6;

 const std::string note="A lembrança de uma pessoa ausente mantém o narrador preso ao passado e dificulta seguir adiante.";
 auto cached=generate("missing-model.gguf",{"Original short evidence without a matching quote."},1000,{note});
 if(!cached.error.empty()||cached.text!=note||cached.tokens!=0||cached.qualified!=1||!cached.complete)return 7;
 auto wrong=generate("missing-model.gguf",{"Original short evidence."},1000,{note,note});if(wrong.error.empty())return 8;
 auto empty=generate("missing-model.gguf",{},1000);
 auto excessive=generate("missing-model.gguf",std::vector<std::string>(9,"synthetic evidence"),1000);
 auto missing=generate("missing-model.gguf",{"Original synthetic song description about moving forward."},1000);
 if(empty.error.empty()||excessive.error.empty()||missing.error.empty()||!empty.text.empty()||!missing.text.empty())return 1;
 std::cout<<"STORY_GUARDS_PASS: missing weights, empty evidence, excessive batch; no invented narrative\n";
}
