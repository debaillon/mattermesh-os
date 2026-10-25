#include "timezone_sync.h"
#include "location.h"
#include "../board/device_model.h"
#include "ui/build_info.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <math.h>
// One free lookup (Open-Meteo, the same provider weather.cpp already calls) turns a GPS fix into a UTC
// offset; device::localBreakdown() (time_sync.cpp) applies it by plain arithmetic rather than through
// setenv("TZ",...)+tzset() - confirmed on hardware that a second tzset() call with a different value
// doesn't reliably change localtime_r()'s output on this platform, so the OS timezone mechanism is only
// ever used once, for the MM_TZ compiled default at boot (time_sync.cpp).
namespace device {
static Preferences prefs;
static double resolvedLat=1000,resolvedLon=1000; // sentinel: nothing resolved yet this boot
static uint32_t lastAttempt=0;static volatile bool attempting=false;
static int resolvedOffsetSeconds=0;static bool haveOffset=false;
struct TzResult{bool ok=false;int offsetSeconds=0;};
static TzResult fetchOffset(double lat,double lon){
 TzResult r;WiFiClientSecure client;HTTPClient http;
 // setInsecure(): see weather.cpp's secure() for why - this host's cert chain reliably fails pinned-CA
 // validation on-device ("SSL - Memory allocation failed"), confirmed by direct test; not sensitive data.
 client.setInsecure();client.setHandshakeTimeout(6);client.setTimeout(6);http.setConnectTimeout(5000);http.setTimeout(6000);
 String url="https://api.open-meteo.com/v1/forecast?latitude="+String(lat,4)+"&longitude="+String(lon,4)+"&timezone=auto&forecast_days=0";
 if(http.begin(client,url)){http.setUserAgent("MatterMeshOS/" MM_VERSION " " MM_DEVICE_TOKEN " tz-lookup");
  if(http.GET()==200){DynamicJsonDocument json(512);if(!deserializeJson(json,http.getString())&&!json["utc_offset_seconds"].isNull()){r.offsetSeconds=json["utc_offset_seconds"];r.ok=true;}}
  http.end();}
 return r;
}
static void applyOffset(int seconds){
 resolvedOffsetSeconds=seconds;haveOffset=true;prefs.putInt("offset",seconds);
}
static void workerTask(void *arg){
 auto *coords=static_cast<double*>(arg);double lat=coords[0],lon=coords[1];delete[] coords;
 auto result=fetchOffset(lat,lon);
 if(result.ok){applyOffset(result.offsetSeconds);resolvedLat=lat;resolvedLon=lon;}
 attempting=false;vTaskDelete(nullptr);
}
void timezoneSyncBegin(){prefs.begin("mmtz",false);if(prefs.isKey("offset"))applyOffset(prefs.getInt("offset",0));}
void timezoneSyncLoop(){
 if(attempting||WiFi.status()!=WL_CONNECTED)return;
 auto loc=locationState();if(!loc.fix||!isfinite(loc.latitude)||!isfinite(loc.longitude))return;
 if(resolvedLat<999&&fabs(loc.latitude-resolvedLat)<0.5&&fabs(loc.longitude-resolvedLon)<0.5)return; // close enough to the last resolved fix
 uint32_t now=millis();if(lastAttempt&&uint32_t(now-lastAttempt)<600000)return; // at most once every 10 min
 lastAttempt=now;attempting=true;
 auto *coords=new double[2]{loc.latitude,loc.longitude};
 if(xTaskCreate(workerTask,"mmtzsync",6144,coords,1,nullptr)!=pdPASS){delete[] coords;attempting=false;}
}
void timezoneSyncRequest(){resolvedLat=resolvedLon=1000;lastAttempt=0;}
bool timezoneSyncReady(){return WiFi.status()==WL_CONNECTED&&locationState().fix;}
bool timezoneOffsetSeconds(int &seconds){if(haveOffset)seconds=resolvedOffsetSeconds;return haveOffset;}
}
