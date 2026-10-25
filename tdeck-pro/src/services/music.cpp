#include "music.h"
#include "spi_lock.h"
#include "../board/board.h"
#include <SD.h>
#include <Audio.h> // schreibfaul1/ESP32-audioI2S
#include <Preferences.h>
#include <algorithm>
// MP3 playback via schreibfaul1/ESP32-audioI2S, the same library and architecture Fennek (a different
// open-source firmware for this exact board, confirmed working) uses:
//  - One shared Audio object owns the one physical I2S0 peripheral for the whole firmware. Notification
//    tones (audio.cpp) play through this SAME engine, as a WAV file written once to SD and played the same
//    way as an MP3 - not a second, independent I2S setup, which would fight this one for the same hardware.
//  - A large PSRAM read-ahead buffer (setBufsize) absorbs the ~650ms an e-paper refresh can hold the shared
//    SPI bus for; earlier attempts without it, or with a smaller RAM-only buffer, made stutter worse, not
//    better, because a plain AudioFileSourceSD/AudioFileSourceBuffer read blocks directly on SD contention.
//  - Decode runs on its own task (pinned to core 0, priority above the main UI loop), reached only through a
//    command queue from the UI thread, matching music.cpp's earlier design.
//  - Every call into the Audio object that touches SD (connecttoFS, loop, stopSong, pauseResume,
//    setAudioPlayPosition) is wrapped in device::spiLock()/spiUnlock() - the actual fix for the crashes this
//    replaces: the previous version had no coordination at all with the e-paper repaint, which shares the
//    same SPI bus and can corrupt an in-flight SD transaction read from a different task.
namespace {
enum class EngineKind:uint8_t{Music,Tone};
enum class MusicCmd:uint8_t{Play,Pause,Resume,Stop,SetVolume};
struct MusicMsg{MusicCmd cmd;int arg;char path[200];EngineKind kind;};
QueueHandle_t cmdQ=nullptr;SemaphoreHandle_t stateMutex=nullptr;
device::MusicState state;Preferences prefs;uint32_t lastTick=0;
Audio s_audio;
EngineKind activeKind=EngineKind::Music;
volatile bool s_eof=false,toneBusy_=false;
int toPlayerVolume(int percent){return (std::max(0,std::min(100,percent))*21+50)/100;} // library range is 0..21
void handle(const MusicMsg &m){
 switch(m.cmd){
  case MusicCmd::Play:{
   device::spiLock();bool ok=s_audio.connecttoFS(SD,m.path);device::spiUnlock();
   activeKind=m.kind;
   if(!ok){
    if(m.kind==EngineKind::Music){xSemaphoreTake(stateMutex,portMAX_DELAY);state.message=String("Could not open ")+(strrchr(m.path,'/')?strrchr(m.path,'/')+1:m.path);state.playing=false;state.revision++;xSemaphoreGive(stateMutex);}
    else toneBusy_=false;
    break;
   }
   s_audio.setVolume(toPlayerVolume(m.arg));
   lastTick=millis();
   if(m.kind==EngineKind::Music){
    xSemaphoreTake(stateMutex,portMAX_DELAY);
    {const char *slash=strrchr(m.path,'/');state.track=slash?slash+1:m.path;}
    state.playing=true;state.paused=false;state.elapsedSec=0;state.message="";state.revision++;
    xSemaphoreGive(stateMutex);
   }else toneBusy_=true;
   break;}
  case MusicCmd::Pause:{
   xSemaphoreTake(stateMutex,portMAX_DELAY);bool toggle=activeKind==EngineKind::Music&&state.playing&&!state.paused;if(toggle)state.paused=true;state.revision++;xSemaphoreGive(stateMutex);
   if(toggle){device::spiLock();s_audio.pauseResume();device::spiUnlock();}
   break;}
  case MusicCmd::Resume:{
   xSemaphoreTake(stateMutex,portMAX_DELAY);bool toggle=activeKind==EngineKind::Music&&state.playing&&state.paused;if(toggle){state.paused=false;lastTick=millis();}state.revision++;xSemaphoreGive(stateMutex);
   if(toggle){device::spiLock();s_audio.pauseResume();device::spiUnlock();}
   break;}
  case MusicCmd::Stop:
   device::spiLock();s_audio.stopSong();device::spiUnlock();
   if(activeKind==EngineKind::Music){xSemaphoreTake(stateMutex,portMAX_DELAY);state.playing=state.paused=false;state.revision++;xSemaphoreGive(stateMutex);}
   else toneBusy_=false;
   break;
  case MusicCmd::SetVolume:{
   int v=std::max(0,std::min(100,m.arg));s_audio.setVolume(toPlayerVolume(v));
   if(activeKind==EngineKind::Music){xSemaphoreTake(stateMutex,portMAX_DELAY);state.volume=v;state.revision++;xSemaphoreGive(stateMutex);prefs.putInt("volume",v);}
   break;}
 }
}
void musicTask(void*){
 for(;;){
  MusicMsg m;while(xQueueReceive(cmdQ,&m,0)==pdTRUE)handle(m);
  bool active=activeKind==EngineKind::Music?(state.playing&&!state.paused):toneBusy_;
  if(!active){if(xQueueReceive(cmdQ,&m,pdMS_TO_TICKS(20))==pdTRUE)handle(m);continue;}
  device::spiLock();s_audio.loop();device::spiUnlock();
  if(activeKind==EngineKind::Music){uint32_t now=millis();if(uint32_t(now-lastTick)>=1000){uint32_t add=(now-lastTick)/1000;lastTick=now;xSemaphoreTake(stateMutex,portMAX_DELAY);state.elapsedSec+=add;state.revision++;xSemaphoreGive(stateMutex);}}
  if(s_eof){
   s_eof=false;
   if(activeKind==EngineKind::Music){xSemaphoreTake(stateMutex,portMAX_DELAY);state.playing=state.paused=false;state.message="Playback finished";state.revision++;xSemaphoreGive(stateMutex);}
   else toneBusy_=false;
  }
  vTaskDelay(1);
 }
}
bool endsWithMp3(const String &name){int n=name.length();return n>4&&name.substring(n-4).equalsIgnoreCase(".mp3");}
}
// ESP32-audioI2S weak callbacks (declared at global scope by the library); "mp3" in the name is historical -
// it fires for any local file connecttoFS() plays through to end of file, WAV tones included.
void audio_eof_mp3(const char*){s_eof=true;}
void audio_info(const char*){}
namespace device {
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
void musicBegin(bool sdReady){
 prefs.begin("mmmusic",false);state.volume=std::max(0,std::min(100,prefs.getInt("volume",70)));state.available=sdReady;
 s_audio.setPinout(BOARD_I2S_BCLK,BOARD_I2S_LRC,BOARD_I2S_DOUT);
 s_audio.setVolume(toPlayerVolume(state.volume));
 s_audio.setBufsize(-1,256*1024); // PSRAM read-ahead: rides out an e-paper refresh holding the SPI bus
 stateMutex=xSemaphoreCreateMutex();cmdQ=xQueueCreate(8,sizeof(MusicMsg));
 xTaskCreatePinnedToCore(musicTask,"mmmusic",8192,nullptr,4,nullptr,0);
}
bool musicPlay(const String &filename){if(!state.available)return false;MusicMsg m;m.cmd=MusicCmd::Play;m.arg=state.volume;strlcpy(m.path,("/music/"+filename).c_str(),sizeof(m.path));m.kind=EngineKind::Music;if(cmdQ)xQueueSend(cmdQ,&m,0);return true;}
void musicPause(){MusicMsg m{MusicCmd::Pause,0,"",EngineKind::Music};if(cmdQ)xQueueSend(cmdQ,&m,0);}
void musicResume(){MusicMsg m{MusicCmd::Resume,0,"",EngineKind::Music};if(cmdQ)xQueueSend(cmdQ,&m,0);}
void musicStop(){MusicMsg m{MusicCmd::Stop,0,"",EngineKind::Music};if(cmdQ)xQueueSend(cmdQ,&m,0);}
void musicVolume(int value){MusicMsg m{MusicCmd::SetVolume,value,"",EngineKind::Music};if(cmdQ)xQueueSend(cmdQ,&m,0);}
void musicLoop(){} // playback is driven entirely by the dedicated task; nothing to do on the caller's thread.
MusicState musicState(){MusicState copy;xSemaphoreTake(stateMutex,portMAX_DELAY);copy=state;xSemaphoreGive(stateMutex);return copy;}
bool enginePlayTone(const String &absolutePath,int volumePercent){
 if(!state.available||state.playing)return false; // never interrupt music, same as before
 MusicMsg m;m.cmd=MusicCmd::Play;m.arg=volumePercent;strlcpy(m.path,absolutePath.c_str(),sizeof(m.path));m.kind=EngineKind::Tone;
 if(!cmdQ)return false;
 xQueueSend(cmdQ,&m,0);return true;
}
bool engineToneBusy(){return toneBusy_;}
}
