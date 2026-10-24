#pragma once
// LilyGO T-Deck Plus (ESP32-S3FN16R8, ST7789 320x240 LCD, GT911 touch, ESP32-C3 keyboard, trackball, SX1262 868 MHz,
// MIA-M10Q GPS, ES7210 microphones). Pins from the LilyGO T-Deck utilities.h and the MeshCore lilygo_tdeck variant.
// Names follow TDeckMaxBoard.h so the shared code compiles unchanged; Max-only parts are -1 or unused addresses.
#include <Arduino.h>
#include <Wire.h>
#define BOARD_NAME "T-Deck-Plus"
#define SerialMon Serial
#define SerialGPS Serial2
#define BOARD_POWERON 10 // peripheral power (LCD, LoRa, GPS, keyboard): HIGH before any of them is used

#define BOARD_I2C_SDA 18
#define BOARD_I2C_SCL 8
#define BOARD_I2C_ADDR_TOUCH    0x5D // GT911 (the driver also probes 0x14)
#define BOARD_I2C_ADDR_KEYBOARD 0x55 // ESP32-C3 keyboard controller
#define BOARD_I2C_ADDR_ES8311   0x7E // not fitted (ES7210 at 0x40 is the microphone ADC)
#define BOARD_I2C_ADDR_BQ27220  0x7E // not fitted: battery read from the ADC
#define BOARD_I2C_ADDR_SY6970   0x7E // not fitted
#define BOARD_TOUCH_INT    16
#define BOARD_KEYBOARD_INT 46
#define BOARD_KEYBOARD_LED (-1) // keyboard light is driven by the C3 over I2C
#define BOARD_BOOT_PIN 0        // trackball click
#define BOARD_TBOX_G01 3        // trackball up
#define BOARD_TBOX_G02 2        // trackball right
#define BOARD_TBOX_G03 15       // trackball down
#define BOARD_TBOX_G04 1        // trackball left
#define BOARD_BAT_ADC 4         // battery through a 1:2 divider

#define BOARD_SPI_SCK  40
#define BOARD_SPI_MOSI 41
#define BOARD_SPI_MISO 38
#define BOARD_TFT_CS 12
#define BOARD_TFT_DC 11
#define BOARD_TFT_BL 42
#define BOARD_EPD_CS   BOARD_TFT_CS // shared code keeps every SPI chip select high by these names
#define BOARD_EPD_DC   BOARD_TFT_DC
#define BOARD_EPD_RST  (-1)
#define BOARD_EPD_BUSY (-1)
#define BOARD_EPD_BL   BOARD_TFT_BL
#define BOARD_SD_CS 39
#define BOARD_LORA_CS   9
#define BOARD_LORA_BUSY 13
#define BOARD_LORA_RST  17
#define BOARD_LORA_INT  45
#define BOARD_GPS_RXD 44
#define BOARD_GPS_TXD 43

#define BOARD_ES7210_MCLK 48
#define BOARD_ES7210_LRCK 21
#define BOARD_ES7210_SCK  47
#define BOARD_ES7210_DIN  14
#define BOARD_I2S_WS   5
#define BOARD_I2S_BCK  7
#define BOARD_I2S_DOUT 6
#define BOARD_ES8311_MCLK   BOARD_ES7210_MCLK
#define BOARD_ES8311_SCLK   BOARD_I2S_BCK
#define BOARD_ES8311_LRCK   BOARD_I2S_WS
#define BOARD_ES8311_DSDIN  BOARD_I2S_DOUT
#define BOARD_ES8311_ASDOUT BOARD_ES7210_DIN

namespace plus {
// The backlight driver counts pulses on GPIO42: 16 levels, each falling edge steps one level down (LilyGO setBrightness).
inline void backlight(uint8_t value){
 static uint8_t level=0;constexpr uint8_t steps=16;
 uint8_t target=value?uint8_t(max(1,(value*steps+127)/255)):0;
 if(target==level)return;
 pinMode(BOARD_TFT_BL,OUTPUT);
 if(!target){digitalWrite(BOARD_TFT_BL,LOW);delay(3);level=0;return;}
 if(!level){digitalWrite(BOARD_TFT_BL,HIGH);level=steps;delayMicroseconds(30);}
 int from=steps-level,to=steps-target,pulses=(steps+to-from)%steps;
 for(int i=0;i<pulses;i++){digitalWrite(BOARD_TFT_BL,LOW);digitalWrite(BOARD_TFT_BL,HIGH);}
 level=target;
}
// Keyboard light: brightness command 0x01 to the C3 controller (0..255).
inline void keyboardLight(uint8_t duty){Wire.beginTransmission(BOARD_I2C_ADDR_KEYBOARD);Wire.write(0x01);Wire.write(duty);Wire.endTransmission();}
// 32 samples 500 us apart (16 ms), the 8 lowest and 8 highest dropped (spikes, TX bursts), x2 for the 1:2 divider.
inline int batteryMillivolts(){uint16_t v[32];for(int i=0;i<32;i++){v[i]=analogReadMilliVolts(BOARD_BAT_ADC);delayMicroseconds(500);}
 for(int i=1;i<32;i++){uint16_t x=v[i];int j=i;while(j>0&&v[j-1]>x){v[j]=v[j-1];j--;}v[j]=x;}
 uint32_t sum=0;for(int i=8;i<24;i++)sum+=v[i];return int((sum*2+8)/16);}
}
