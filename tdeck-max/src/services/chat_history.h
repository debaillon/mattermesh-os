#pragma once
#include <algorithm>
#include <stdint.h>
#include <vector>
namespace device {
inline int historyStep(int offset,int delta,int count){return std::max(0,std::min(std::max(0,count-1),offset+(delta>0?1:delta<0?-1:0)));}
inline uint32_t historyAnchor(const std::vector<uint32_t> &tokens,int offset){int index=int(tokens.size())-1-offset;return index>=0&&index<int(tokens.size())?tokens[index]:0;}
inline int historyRestore(const std::vector<uint32_t> &tokens,uint32_t anchor,int offset){if(anchor)for(int i=0;i<int(tokens.size());i++)if(tokens[i]==anchor)return int(tokens.size())-1-i;return std::max(0,std::min(std::max(0,int(tokens.size())-1),offset));}
}
