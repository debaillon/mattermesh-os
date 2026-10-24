#pragma once
// T-Deck Plus keyboard: an ESP32-C3 at 0x55 scans a 5x7 matrix. With the raw command (0x03, keyboard firmware from
// 2025-06) it answers five column bytes, one bit per row; the changes become the TCA8418 key events of the T-Deck Max
// (same key caps and code numbers, bit 7 = pressed), so modifiers, holds and the lock keys work unchanged.
// Older keyboard firmware only returns typed characters: they are replayed as press/release pairs (no holds).
#include <Arduino.h>
#include <Wire.h>
#include "plus_board.h"
namespace plus {
class Keyboard {
 // Max TCA8418 code of each Plus matrix key [column][row] (LilyGO Keyboard_ESP32C3 layout).
 static uint8_t code(int col,int row){static const uint8_t codes[5][7]={{10,9,32,20,30,33,34},{8,19,18,1,28,29,35},{7,16,6,31,26,27,17},{4,15,5,21,25,24,14},{2,12,3,11,22,23,13}};return codes[col][row];}
 uint8_t address=BOARD_I2C_ADDR_KEYBOARD,state[5]={};uint8_t queue[32];uint8_t head=0,count=0;uint32_t polledAt=0;
 void push(uint8_t event){if(count<sizeof(queue)){queue[(head+count)%sizeof(queue)]=event;count++;}else overflow=true;}
 static int codeFor(char c,bool &shift,bool &sym){
  static const char letters[]="poiuytrewq\blkjhgfdsa\n$mnbvcxz";// codes 1..29 in order, as keys[][] in main.cpp
  static const char symbolKeys[]="@+-_)(321#\b\"';:/654*\n0.,!?987";
  shift=sym=false;if(c==' ')return 33;if(c=='\r'||c=='\n')return 21;if(c=='\b')return 11;
  for(int i=0;letters[i];i++)if(letters[i]==c)return i+1;
  if(c>='A'&&c<='Z'){int code=codeFor(char(c-'A'+'a'),shift,sym);shift=true;return code;}
  for(int i=0;symbolKeys[i];i++)if(symbolKeys[i]==c){sym=true;return i+1;}
  return 0;
 }
public:
 bool raw=false,overflow=false;uint32_t polls=0,errors=0;
 bool begin(uint8_t addr,TwoWire*){
  address=addr;Wire.beginTransmission(address);if(Wire.endTransmission()!=0)return false;
  Wire.beginTransmission(address);Wire.write(0x03);Wire.endTransmission();delay(5);
  uint8_t bytes[5];raw=Wire.requestFrom(address,uint8_t(5))==5;for(int i=0;i<5;i++){bytes[i]=Wire.available()?Wire.read():0xFF;if(bytes[i]&0x80)raw=false;}
  if(raw)memcpy(state,bytes,5);// keys already held at boot do not fire
  else{Wire.beginTransmission(address);Wire.write(0x04);Wire.endTransmission();}
  Serial.printf("PLUS_KEYBOARD ready=1 raw=%d\n",raw);return true;
 }
 bool matrix(uint8_t,uint8_t){return true;}
 void enableDebounce(){}void enableInterrupts(){}void flush(){count=0;}
 // Reads the controller at most every 8 ms; true when events are waiting.
 bool poll(){
  if(count)return true;uint32_t now=millis();if(now-polledAt<8)return false;polledAt=now;polls++;
  if(raw){
   uint8_t bytes[5];if(Wire.requestFrom(address,uint8_t(5))!=5){errors++;return false;}for(int i=0;i<5;i++)bytes[i]=Wire.read();
   for(int col=0;col<5;col++){if(bytes[col]&0x80){errors++;return count>0;}uint8_t changed=bytes[col]^state[col];
    for(int row=0;row<7;row++)if(changed&(1<<row))push(uint8_t(code(col,row)|((bytes[col]&(1<<row))?0x80:0)));state[col]=bytes[col];}
  }else{
   if(Wire.requestFrom(address,uint8_t(1))!=1){errors++;return false;}char c=char(Wire.read());if(!c)return false;
   bool shift,sym;int code=codeFor(c,shift,sym);if(!code)return false;
   if(shift)push(0x80|35);if(sym)push(0x80|32);push(uint8_t(0x80|code));push(uint8_t(code));if(sym)push(32);if(shift)push(35);
  }
  return count>0;
 }
 // TCA8418 register shims: INT_STAT reports a key event when the queue is not empty.
 uint8_t readRegister(uint8_t){poll();uint8_t status=count?1:0;if(overflow){status|=8;overflow=false;}return status;}
 void writeRegister(uint8_t,uint8_t){}
 int available(){return count;}
 int getEvent(){if(!count)return 0;uint8_t e=queue[head];head=(head+1)%sizeof(queue);count--;return e;}
};
// Trackball: each direction line toggles once per detent step, counted from pin-change interrupts. File-scope
// statics (the header is included by main.cpp only): IRAM handlers cannot be inline class members.
static volatile int16_t trackballX=0,trackballY=0;
static void IRAM_ATTR trackballUp(){trackballY--;}
static void IRAM_ATTR trackballDown(){trackballY++;}
static void IRAM_ATTR trackballLeft(){trackballX--;}
static void IRAM_ATTR trackballRight(){trackballX++;}
struct Trackball {
 static void begin(){
  int pins[4]={BOARD_TBOX_G01,BOARD_TBOX_G02,BOARD_TBOX_G03,BOARD_TBOX_G04};void(*handlers[4])()={trackballUp,trackballRight,trackballDown,trackballLeft};
  for(int i=0;i<4;i++){pinMode(pins[i],INPUT_PULLUP);attachInterrupt(digitalPinToInterrupt(pins[i]),handlers[i],CHANGE);}
  pinMode(BOARD_BOOT_PIN,INPUT_PULLUP);
 }
 static void take(int &x,int &y){noInterrupts();x=trackballX;y=trackballY;trackballX=trackballY=0;interrupts();}
};
}
