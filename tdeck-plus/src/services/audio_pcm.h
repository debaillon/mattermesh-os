#pragma once
#include <stdint.h>
#include <math.h>
namespace device {
constexpr int SPEAKER_RATE=16000,SPEAKER_PEAK=16000;
inline int16_t speakerToneSample(int frame,int volume,bool test){
 const int warm=SPEAKER_RATE*30/1000,body=SPEAKER_RATE*(test?700:220)/1000;
 int at=frame-warm;if(at<0||at>=body||volume<=0)return 0;
 int amplitude=SPEAKER_PEAK*(volume>100?100:volume)/100;
 int ramp=SPEAKER_RATE*12/1000;float fade=1.0f;if(at<ramp)fade=float(at)/ramp;if(body-1-at<ramp)fade=float(body-1-at)/ramp;
 return int16_t(amplitude*fade*sinf(2*3.14159265f*880*at/SPEAKER_RATE));
}
}
