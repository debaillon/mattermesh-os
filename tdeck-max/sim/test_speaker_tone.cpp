#include <cassert>
#include <iostream>
#include "services/audio_pcm.h"
int main(){using namespace device;int peak=0,nonzero=0;for(int i=0;i<5120;i++){int value=speakerToneSample(i,35,false);assert(speakerToneSample(i,0,false)==0);assert(value>=-5600&&value<=5600);if(value)nonzero++;if(abs(value)>peak)peak=abs(value);if(i<480||i>=4000)assert(value==0);}assert(peak>5500&&nonzero>3000);assert(speakerToneSample(480,100,false)==0);assert(speakerToneSample(11679,100,true)==0);std::cout<<"PASS: tone level, volume OFF, fade envelope, amplifier warmup and trailing silence\n";}
