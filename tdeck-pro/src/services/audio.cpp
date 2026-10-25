#include "audio.h"
#include "ringtones.h"
#include "music.h"
#include "spi_lock.h"
#include <SD.h>
#include <Preferences.h>
// Notification tones for the T-Deck Pro: the same synthesised waveforms as the T-Deck Max (services/
// ringtones.h), but played through music.cpp's shared Audio engine instead of a separate I2S setup of their
// own - matching Fennek (a different open-source firmware for this exact board), whose alarm clock plays its
// beep the same way. There is one physical I2S0 peripheral on this chip; two independently-managed I2S
// drivers fighting over it is what made tones and music interfere with each other before. Each tone is
// synthesised to a cached WAV file on SD the first time it's used (see ensureToneWav()), then played exactly
// like an MP3 track. Recording/dictation needs the PDM mic, not ported here, so those calls stay stubbed
// (see audio.h).
namespace device {
static Preferences prefs;static AudioState state;
static bool ensureToneWav(int tone,String &outPath){
 outPath="/tones/"+String(tone)+".wav";
 spiLock();bool exists=SD.exists(outPath);spiUnlock();
 if(exists)return true;
 spiLock();
 if(!SD.exists("/tones"))SD.mkdir("/tones");
 File f=SD.open(outPath,FILE_WRITE);
 if(!f){spiUnlock();return false;}
 int frames=ringtoneFrames(tone);uint32_t dataBytes=uint32_t(frames)*2; // mono, 16-bit
 uint8_t hdr[44]={};
 auto p32=[](uint8_t *d,uint32_t v){d[0]=v;d[1]=v>>8;d[2]=v>>16;d[3]=v>>24;};
 auto p16=[](uint8_t *d,uint16_t v){d[0]=v;d[1]=v>>8;};
 memcpy(hdr,"RIFF",4);p32(hdr+4,36+dataBytes);memcpy(hdr+8,"WAVE",4);
 memcpy(hdr+12,"fmt ",4);p32(hdr+16,16);p16(hdr+20,1);p16(hdr+22,1);p32(hdr+24,SPEAKER_RATE);p32(hdr+28,SPEAKER_RATE*2);p16(hdr+32,2);p16(hdr+34,16);
 memcpy(hdr+36,"data",4);p32(hdr+40,dataBytes);
 f.write(hdr,sizeof(hdr));
 int16_t buf[256];int i=0;
 while(i<frames){int n=0;while(n<256&&i<frames){buf[n++]=ringtoneSample(tone,i,100);i++;} // full-scale; playback volume applied by the engine, not baked in
  f.write(reinterpret_cast<const uint8_t*>(buf),n*2);}
 f.close();spiUnlock();
 return true;
}
static bool start(int tone){
 if(!state.ready||engineToneBusy()||musicState().playing)return false;
 String path;
 if(!ensureToneWav(ringtoneIndex(tone),path)){state.message="Tone generation failed";state.revision++;return false;}
 if(!enginePlayTone(path,state.volume)){state.message="Tone playback failed";state.revision++;return false;}
 state.busy=true;state.message="Playing tone...";state.revision++;
 return true;
}
void audioBegin(){prefs.begin("mmaudio",false);state.dictationConfigured=false;state.notifications=prefs.getBool("notify",true);state.volume=constrain(prefs.getInt("volume",20),0,100);state.messageTone=ringtoneIndex(prefs.getInt("tone",RINGTONE_MESSAGE_DEFAULT));state.mentionTone=ringtoneIndex(prefs.getInt("mtone",RINGTONE_MENTION_DEFAULT));state.ready=true;state.message="PCM5102A ready";}
static uint32_t lastTone=0;
bool audioNotify(bool test,bool mention){if(!test&&(!state.notifications||state.volume==0||millis()-lastTone<2000))return false;if(!start(mention?state.mentionTone:state.messageTone))return false;lastTone=millis();return true;}
bool audioPreview(int tone){return start(tone);}
void audioRingtone(bool mention,int tone){tone=ringtoneIndex(tone);(mention?state.mentionTone:state.messageTone)=tone;prefs.putInt(mention?"mtone":"tone",tone);state.revision++;}
bool audioRecord(){state.message="Recording not supported on this board";state.revision++;return false;}
bool audioProbeRecord(){return false;}
void audioStop(){}
void audioCancel(){}
void audioNotifications(bool enabled){state.notifications=enabled;prefs.putBool("notify",enabled);state.revision++;}
void audioVolume(int value){state.volume=constrain(value,0,100);prefs.putInt("volume",state.volume);state.revision++;}
bool audioDiagnosticPcm(const uint8_t *&data,size_t &bytes){data=nullptr;bytes=0;return false;}
String audioDiagnosticResult(){return String("");}
uint32_t audioRevision(){return state.revision;}
bool audioBusy(){return state.busy;}
AudioState audioState(){return state;}
bool takeTranscript(String &text){(void)text;return false;}
void audioLoop(){if(state.busy&&!engineToneBusy()){state.busy=false;state.revision++;}}
}
