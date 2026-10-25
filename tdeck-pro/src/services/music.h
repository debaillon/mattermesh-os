#pragma once
#include <Arduino.h>
#include <vector>
namespace device {
struct MusicState {bool playing=false,paused=false,available=false;String track;uint32_t elapsedSec=0;int volume=70;uint32_t revision=0;String message;};
// /music/*.mp3 on the SD card, name only (no path), sorted.
std::vector<String> musicList();
void musicBegin(bool sdReady);
bool musicPlay(const String &filename);
void musicPause();
void musicResume();
void musicStop();
void musicVolume(int value);
void musicLoop();
MusicState musicState();
// Shared I2S engine (one physical I2S0 peripheral => one owner): audio.cpp's notification tones also play
// through this, as a WAV file written once to SD - see music.cpp's header comment and audio.cpp.
bool enginePlayTone(const String &absolutePath,int volumePercent);
bool engineToneBusy();
}
