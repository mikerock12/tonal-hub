#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace tonal::queue {
struct Alignment {std::vector<int> matches;int suffix=0;};
// Align confirmed service occurrences with a known prepared queue. Keep the
// unobserved tail only after an actual match; a different selection replaces it.
// At most 500 x 101 short cells. Equal alternatives prefer earlier occurrences.
inline Alignment align(const std::vector<std::string>& old,const std::vector<std::string>& seen){
 Alignment out;out.suffix=int(old.size());out.matches.assign(seen.size(),-1);
 if(old.size()>500||seen.size()>101)return out;
 const size_t n=old.size(),m=seen.size();std::vector<uint16_t> dp((n+1)*(m+1));
 auto cell=[&](size_t i,size_t j)->uint16_t&{return dp[i*(m+1)+j];};
 for(size_t i=n;i-->0;)for(size_t j=m;j-->0;)cell(i,j)=old[i]==seen[j]?1+cell(i+1,j+1):std::max(cell(i+1,j),cell(i,j+1));
 size_t i=0,j=0;bool matched=false;
 while(i<n&&j<m){if(old[i]==seen[j]&&cell(i,j)==1+cell(i+1,j+1)){out.matches[j]=int(i);out.suffix=int(i+1);matched=true;++i;++j;}
  else if(cell(i,j+1)>=cell(i+1,j))++j;else ++i;}
 if(!matched)out.suffix=int(n);
 // Reordered occurrences can sit outside the ordered subsequence. Associate
 // their metadata too, without moving the boundary of the unobserved tail.
 std::vector<bool> used(n);for(int index:out.matches)if(index>=0)used[index]=true;
 for(size_t at=0;at<m;++at)if(out.matches[at]<0)for(size_t prior=0;prior<n;++prior)if(!used[prior]&&old[prior]==seen[at]){out.matches[at]=int(prior);used[prior]=true;break;}
 return out;
}
}
