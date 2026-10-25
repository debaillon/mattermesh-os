#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
// A single mutex around every SD/e-paper SPI transaction that can race with the music engine's own SD
// reads: both the decode loop and the e-paper repaint happen continuously while a track plays, on the same
// physical bus, from different tasks. Modeled on Fennek (a different open-source firmware for this exact
// board)'s g_spiMutex/spiLock()/spiUnlock(), which wraps every SD and e-paper SPI call in its firmware; this
// port only wraps the two call sites that actually contend during playback (see music.cpp and main.cpp's
// paint path), not every SD access in the project - those are one-shot and don't race a continuous reader.
namespace device {
extern SemaphoreHandle_t spiMutex;
void spiLockBegin();
inline void spiLock(){xSemaphoreTake(spiMutex,portMAX_DELAY);}
inline void spiUnlock(){xSemaphoreGive(spiMutex);}
}
