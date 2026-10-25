#include "music.h"
#include "audio_output.h"
#include <SD.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <Preferences.h>
#include <algorithm>
// MP3 playback via ESP8266Audio (see audio_output.h for why this library, not schreibfaul1/ESP32-audioI2S).
// Decode runs on its own task (pinned to core 0, matching how Fennek - a different open-source firmware for
// this exact board - runs its own player), not the main loop: calling this cooperatively from the main
// loop() fell behind real time under the rest of the UI/radio/touch work. Every AudioFileSourceSD/
// AudioGeneratorMP3 object is created, used and destroyed only on this one task; musicPlay()/musicPause()/
// etc. (called from the UI thread) just post a command and return, never touching those objects directly.
//
// Tried wrapping this in AudioFileSourceBuffer (a large PSRAM read-ahead buffer, the same role
// schreibfaul1/ESP32-audioI2S's setBufsize plays in Fennek) to smooth crackle from SD/e-paper/LoRa SPI-bus
// contention. Made it worse, confirmed on hardware: its fill() does one big blocking SD.read() per
// opportunistic top-up rather than many small ones, which stalls longer under contention, not less.
// Reverted to reading directly from AudioFileSourceSD.
namespace device {
enum class MusicCmd:uint8_t{Play,Pause,Resume,Stop,SetVolume};
struct MusicMsg{MusicCmd cmd;int arg;char path[160];};
static QueueHandle_t cmdQ=nullptr;static SemaphoreHandle_t stateMutex=nullptr;
static MusicState state;static Preferences prefs;static uint32_t lastTick=0;
static AudioFileSourceSD *src=nullptr;static AudioGeneratorMP3 *mp3=nullptr;
static bool endsWithMp3(const String &name){int n=name.length();return n>4&&name.substring(n-4).equalsIgnoreCase(".mp3");}
std::vector<String> musicList(){
 std::vector<String> out;if(!state.available)return out;
 File dir=SD.open("/music");if(!dir||!dir.isDirectory())return out;
 for(File f=dir.openNextFile();f;f=dir.openNextFile()){
  if(!f.isDirectory()){String name=String(f.name());int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);if(endsWithMp3(name))out.push_back(name);}
  f.close();
 }
 dir.close();std::sort(out.begin(),out.end(),[](const String &a,const String &b){return a.compareTo(b)<0;});
 return out;
}
static void cleanupPlayback(){if(mp3){mp3->stop();delete mp3;mp3=nullptr;}if(src){delete src;src=nullptr;}}
static void post(MusicCmd cmd,int arg=0,const char *path=nullptr){MusicMsg m;m.cmd=cmd;m.arg=arg;m.path[0]=0;if(path)strlcpy(m.path,path,sizeof(m.path));if(cmdQ)xQueueSend(cmdQ,&m,0);}
static void handle(const MusicMsg &m){
 switch(m.cmd){
  case MusicCmd::Play:{
   cleanupPlayback();
   src=new AudioFileSourceSD(m.path);
   if(!src->isOpen()){delete src;src=nullptr;xSemaphoreTake(stateMutex,portMAX_DELAY);state.message=String("Could not open ")+(strrchr(m.path,'/')?strrchr(m.path,'/')+1:m.path);state.playing=false;state.revision++;xSemaphoreGive(stateMutex);break;}
   mp3=new AudioGeneratorMP3();
   i2sOutput().SetGain(state.volume/100.0f);
   if(!mp3->begin(src,&i2sOutput())){delete mp3;mp3=nullptr;delete src;src=nullptr;xSemaphoreTake(stateMutex,portMAX_DELAY);state.message="Playback failed / not a valid MP3";state.playing=false;state.revision++;xSemaphoreGive(stateMutex);break;}
   lastTick=millis();
   xSemaphoreTake(stateMutex,portMAX_DELAY);
   {const char *slash=strrchr(m.path,'/');state.track=slash?slash+1:m.path;}
   state.playing=true;state.paused=false;state.elapsedSec=0;state.message="";state.revision++;
   xSemaphoreGive(stateMutex);
   break;}
  case MusicCmd::Pause: xSemaphoreTake(stateMutex,portMAX_DELAY);if(state.playing)state.paused=true;state.revision++;xSemaphoreGive(stateMutex); break;
  case MusicCmd::Resume: lastTick=millis();xSemaphoreTake(stateMutex,portMAX_DELAY);if(state.playing)state.paused=false;state.revision++;xSemaphoreGive(stateMutex); break;
  case MusicCmd::Stop: cleanupPlayback();xSemaphoreTake(stateMutex,portMAX_DELAY);state.playing=state.paused=false;state.revision++;xSemaphoreGive(stateMutex); break;
  case MusicCmd::SetVolume:{int v=constrain(m.arg,0,100);i2sOutput().SetGain(v/100.0f);xSemaphoreTake(stateMutex,portMAX_DELAY);state.volume=v;state.revision++;xSemaphoreGive(stateMutex);prefs.putInt("volume",v);break;}
 }
}
static void musicTask(void*){
 for(;;){
  MusicMsg m;while(xQueueReceive(cmdQ,&m,0)==pdTRUE)handle(m);
  bool active;xSemaphoreTake(stateMutex,portMAX_DELAY);active=state.playing&&!state.paused;xSemaphoreGive(stateMutex);
  if(!active){if(xQueueReceive(cmdQ,&m,pdMS_TO_TICKS(20))==pdTRUE)handle(m);continue;}
  bool ended=!mp3||!mp3->isRunning()||!mp3->loop();
  if(ended){cleanupPlayback();xSemaphoreTake(stateMutex,portMAX_DELAY);state.playing=state.paused=false;state.message="Playback finished";state.revision++;xSemaphoreGive(stateMutex);}
  else{uint32_t now=millis();if(uint32_t(now-lastTick)>=1000){uint32_t add=(now-lastTick)/1000;lastTick=now;xSemaphoreTake(stateMutex,portMAX_DELAY);state.elapsedSec+=add;state.revision++;xSemaphoreGive(stateMutex);}}
  vTaskDelay(1);
 }
}
void musicBegin(bool sdReady){
 prefs.begin("mmmusic",false);state.volume=constrain(prefs.getInt("volume",70),0,100);state.available=sdReady;
 i2sOutput().SetGain(state.volume/100.0f);
 stateMutex=xSemaphoreCreateMutex();cmdQ=xQueueCreate(8,sizeof(MusicMsg));
 xTaskCreatePinnedToCore(musicTask,"mmmusic",8192,nullptr,3,nullptr,0);
}
bool musicPlay(const String &filename){if(!state.available)return false;post(MusicCmd::Play,0,("/music/"+filename).c_str());return true;}
void musicPause(){post(MusicCmd::Pause);}
void musicResume(){post(MusicCmd::Resume);}
void musicStop(){post(MusicCmd::Stop);}
void musicVolume(int value){post(MusicCmd::SetVolume,value);}
void musicLoop(){} // playback is driven entirely by the dedicated task; nothing to do on the caller's thread.
MusicState musicState(){MusicState copy;xSemaphoreTake(stateMutex,portMAX_DELAY);copy=state;xSemaphoreGive(stateMutex);return copy;}
}
