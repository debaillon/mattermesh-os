#pragma once
#include "lock_policy.h"
namespace device {
// P/O/I/U physical keycodes 1/2/3/4. Keep press order during key rollover.
struct HeldShortcuts {
 struct Entry {char character=0;int index=-1;bool waiting=false;};
 LockKey keys[4];Entry queue[128];int count=0;
 bool waiting()const{for(int i=0;i<count;i++)if(queue[i].waiting)return true;return false;}
 void reset(){for(auto &key:keys)key=LockKey();count=0;}
 bool append(char character,int index=-1){if(count==128){reset();return false;}queue[count].character=character;queue[count].index=index;queue[count++].waiting=index>=0;return true;}
 bool press(int index,char character,uint32_t now,bool hardwareDebounced=false){return index>=0&&index<4&&keys[index].press(now,hardwareDebounced)&&append(character,index);}
 void resolve(int index,bool consumed){for(int i=0;i<count;i++)if(queue[i].index==index&&queue[i].waiting){queue[i].waiting=false;if(consumed)queue[i].character=0;break;}}
 uint8_t tick(uint32_t now){uint8_t actions=0;for(int i=0;i<4;i++)if(keys[i].tick(now,false)){resolve(i,true);actions|=1<<i;}return actions;}
 uint8_t release(int index,uint32_t now){if(index<0||index>=4||!keys[index].down)return 0;uint8_t action=0;if(keys[index].tick(now,false))action=1<<index;bool consumed=keys[index].consumed;keys[index].release(now,false);resolve(index,consumed);return action;}
 int visible(uint32_t now)const{int index=-1;for(int i=0;i<4;i++)if(keys[i].down&&!keys[i].consumed&&keys[i].progress&&(index<0||uint32_t(now-keys[i].started)>uint32_t(now-keys[index].started)))index=i;return index;}
 template<class Emit>void flush(Emit emit){while(count&&!queue[0].waiting){char character=queue[0].character;for(int i=1;i<count;i++)queue[i-1]=queue[i];count--;if(character)emit(character);}}
};
}
