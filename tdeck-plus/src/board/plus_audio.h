#pragma once
// T-Deck Plus audio behind the part of the EspCodec API that services/audio.cpp uses.
// Speaker: MAX98357A class-D amplifier on I2S1 (BCK 7, WS 5, DOUT 6), no codec registers: the ringtone level is
// already applied to the samples. The driver is installed on open() and removed on close(), so the pins are idle
// between sounds. (The ES7210 microphones are not used: no dictation on the Plus.)
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include "plus_board.h"
namespace plus {
class Audio {
 static constexpr i2s_port_t SpeakerPort=I2S_NUM_1;
 bool speakerOpen=false,outputMuted=false;uint32_t rate=16000;
 bool openSpeaker(){
  i2s_config_t config={};config.mode=i2s_mode_t(I2S_MODE_MASTER|I2S_MODE_TX);config.sample_rate=rate;config.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;config.communication_format=I2S_COMM_FORMAT_STAND_I2S;config.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count=6;config.dma_buf_len=256;config.use_apll=false;config.tx_desc_auto_clear=true;
  if(i2s_driver_install(SpeakerPort,&config,0,nullptr)!=ESP_OK)return false;
  i2s_pin_config_t pins={};pins.mck_io_num=I2S_PIN_NO_CHANGE;pins.bck_io_num=BOARD_I2S_BCK;pins.ws_io_num=BOARD_I2S_WS;pins.data_out_num=BOARD_I2S_DOUT;pins.data_in_num=I2S_PIN_NO_CHANGE;
  if(i2s_set_pin(SpeakerPort,&pins)!=ESP_OK){i2s_driver_uninstall(SpeakerPort);return false;}
  i2s_zero_dma_buffer(SpeakerPort);return true;
 }
public:
 void setPaParams(int,float){}void setPins(int,int,int,int,int){}
 bool begin(TwoWire &,uint8_t,int){Serial.println("PLUS_AUDIO speaker=1");return true;}// the speaker has no control bus
 int open(uint8_t,uint8_t,uint32_t sampleRate){close();rate=sampleRate;speakerOpen=openSpeaker();return speakerOpen?0:-1;}
 void close(){if(speakerOpen){i2s_driver_uninstall(SpeakerPort);speakerOpen=false;}}
 int write(uint8_t *buffer,size_t size){if(!speakerOpen)return -1;if(outputMuted)memset(buffer,0,size);size_t written=0;if(i2s_write(SpeakerPort,buffer,size,&written,portMAX_DELAY)!=ESP_OK)return -1;return 0;}
 void setVolume(uint8_t){}
 void setGain(float){}
 void setMute(bool){}
 int setOutputMute(bool muted){outputMuted=muted;return 0;}
};
}
