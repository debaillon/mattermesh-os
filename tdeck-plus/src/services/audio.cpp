#include "audio.h"
#include "ringtones.h"
#include "esp_codec.h"
#include <driver/gpio.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_reg.h>
#include <soc/gpio_sig_map.h>
#include <Preferences.h>
#include "../board/board.h"
#include "../board/plus_audio.h"
#include <atomic>
namespace device {
static plus::Audio codec;// ES8311 + MAX98357A speaker
static Preferences prefs;static AudioState state;static std::atomic<bool> done{false},cancel{false};static std::atomic<int> error{0};static int toneVolume=20,toneId=0;static std::atomic<uint32_t> transmitted{0};static uint32_t lastTone=0;
constexpr int RATE=16000;// ringtone sample rate
// The ESP32-S3 I2S keeps MCLK (4.096 MHz), BCLK and LRCK on the pins after
// codec.close(); their harmonics sit on GPS L1 (MIA-M10Q CW suppression 35-75,
// slow GPS fixes). Park the clock pins low while idle and put back the saved
// GPIO-matrix routing before each open. Input routing (RX loopback) is untouched.
static const int clockPins[3]={BOARD_ES8311_MCLK,BOARD_ES8311_SCLK,BOARD_ES8311_LRCK};static uint32_t clockRouting[3]={};static bool clocksParked=false;
static void i2sClocks(bool on){if(on!=clocksParked)return;for(int i=0;i<3;i++){volatile uint32_t *routing=(volatile uint32_t*)(GPIO_FUNC0_OUT_SEL_CFG_REG+4*clockPins[i]);if(on)*routing=clockRouting[i];else{clockRouting[i]=*routing;gpio_set_level(gpio_num_t(clockPins[i]),0);esp_rom_gpio_connect_out_signal(clockPins[i],SIG_GPIO_OUT_IDX,false,false);}}clocksParked=!on;}
static void worker(void*){
 // Feed both I2S slots: ES8311 sees valid PCM regardless of selected DAC slot.
 int16_t buffer[512];const int frames=ringtoneFrames(toneId);
 for(int start=0;start<frames&&!cancel.load();start+=256){int count=min(256,frames-start);for(int i=0;i<count;i++){int16_t sample=ringtoneSample(toneId,start+i,toneVolume);buffer[2*i]=buffer[2*i+1]=sample;}int code=codec.write((uint8_t*)buffer,count*4);if(code){error=code;break;}transmitted+=count*4;}
 // i2s_write queues DMA; do not cut PA power before the final silence drains.
 vTaskDelay(pdMS_TO_TICKS(35));
 done=true;vTaskDelete(nullptr);
}
void audioBegin(){prefs.begin("mmaudio",false);state.notifications=prefs.getBool("notify",true);state.volume=constrain(prefs.getInt("volume",20),0,100);state.messageTone=ringtoneIndex(prefs.getInt("tone",RINGTONE_MESSAGE_DEFAULT));state.mentionTone=ringtoneIndex(prefs.getInt("mtone",RINGTONE_MENTION_DEFAULT));codec.setPaParams(-1,3.3f);codec.setPins(BOARD_ES8311_MCLK,BOARD_ES8311_SCLK,BOARD_ES8311_LRCK,BOARD_ES8311_ASDOUT,BOARD_ES8311_DSDIN);state.ready=codec.begin(Wire,BOARD_I2C_ADDR_ES8311,CODEC_TYPE_ES8311);if(state.ready)i2sClocks(false);state.message=state.ready?"ES8311 ready":"ES8311 unavailable";state.revision++;}
static bool start(int tone){if(!state.ready||state.busy)return false;state.lastError=0;i2sClocks(true);if(codec.open(16,2,RATE)!=0){state.message="Audio stream failed";state.revision++;return false;}codec.setVolume(100);codec.setGain(12);codec.setMute(true);int muteError=codec.setOutputMute(false);if(muteError){codec.close();i2sClocks(false);state.message="Speaker mute control failed";state.lastError=muteError;state.revision++;return false;}toneId=ringtoneIndex(tone);toneVolume=state.volume;transmitted=0;error=0;done=false;cancel=false;state.busy=true;state.message="Notification tone";state.revision++;if(xTaskCreate(worker,"mmaudio",8192,nullptr,1,nullptr)!=pdPASS){codec.close();i2sClocks(false);state.busy=false;state.message="Audio task failed";state.revision++;return false;}return true;}
bool audioNotify(bool test,bool mention){if(!test&&(!state.notifications||state.volume==0||millis()-lastTone<2000))return false;if(!start(mention?state.mentionTone:state.messageTone))return false;lastTone=millis();return true;}
bool audioPreview(int tone){return start(tone);}
void audioRingtone(bool mention,int tone){tone=ringtoneIndex(tone);(mention?state.mentionTone:state.messageTone)=tone;prefs.putInt(mention?"mtone":"tone",tone);state.revision++;}
void audioCancel(){cancel=true;}
void audioNotifications(bool enabled){state.notifications=enabled;prefs.putBool("notify",enabled);state.revision++;}
void audioVolume(int value){state.volume=constrain(value,0,100);prefs.putInt("volume",state.volume);state.revision++;}
uint32_t audioRevision(){return state.revision;}
bool audioBusy(){return state.busy;}
AudioState audioState(){return state;}
void audioLoop(){
 if(!state.busy||!done.load())return;
 codec.setMute(true);codec.setOutputMute(true);codec.close();i2sClocks(false);state.txBytes=transmitted.load();state.playbacks++;
 state.busy=false;state.lastError=error.load();state.message=error.load()?"Audio failed ("+String(error.load())+")":"Playback complete";done=false;state.revision++;
}
}
