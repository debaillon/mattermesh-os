#pragma once
#include <stdint.h>
#include <time.h>
namespace device {
enum class TimeSource {None,WiFi,GPS,Manual};
struct TimeSyncState {bool acquiring=true,gpsWaiting=false,gpsTemporary=false,timedOut=false;const char *message="Acquiring time";TimeSource source=TimeSource::None;uint32_t revision=0,epoch=0;};
// clock: system time at boot. A valid one (kept across a soft reset) skips the
// automatic Wi-Fi/GPS acquisition; timeSyncRequest() still forces one.
void timeSyncBegin(uint32_t clock=0);void timeSyncRequest();void timeSyncLoop();TimeSyncState timeSyncState();
bool timeSyncNetwork(uint32_t epoch,uint32_t micros=0);
bool timeSyncGPS(uint32_t epoch,bool validTime,bool fresh);
// Clock set by hand (Settings > Date / time): kept until an NTP or GPS time replaces it; no GPS lease is started for it.
bool timeSyncManual(uint32_t epoch);
// Local-time breakdown of a UTC epoch: applies timezone_sync.cpp's resolved GPS offset directly by
// arithmetic when one is available, otherwise falls back to localtime_r() under the MM_TZ compiled default
// set once at boot. Every UI/CLI call site that needs local wall-clock fields should go through this rather
// than calling localtime_r() itself (see timezone_sync.h for why).
struct tm localBreakdown(time_t utc);
}
