#include "timezone_sync.h"
#include "location.h"
#include "weather_ca.h"
#include "../board/device_model.h"
#include "ui/build_info.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <math.h>
#include <time.h>
// One free lookup (Open-Meteo, the same provider and root CA weather.cpp already trusts) turns a GPS fix
// into a UTC offset; applied as a fixed-offset TZ string ("LT-1" style - see timezone_sync.h on why this is
// a fixed offset, not a full zone) via setenv/tzset, replacing the MM_TZ default time_sync.cpp sets at boot.
namespace device {
static Preferences prefs;
static double resolvedLat=1000,resolvedLon=1000; // sentinel: nothing resolved yet this boot
static uint32_t lastAttempt=0;static volatile bool attempting=false;
struct TzResult{bool ok=false;int offsetSeconds=0;};
static TzResult fetchOffset(double lat,double lon){
 TzResult r;WiFiClientSecure client;HTTPClient http;
 client.setCACert(WEATHER_ROOT_CA);client.setHandshakeTimeout(6);client.setTimeout(6);http.setConnectTimeout(5000);http.setTimeout(6000);
 String url="https://api.open-meteo.com/v1/forecast?latitude="+String(lat,4)+"&longitude="+String(lon,4)+"&timezone=auto&forecast_days=0";
 if(http.begin(client,url)){http.setUserAgent("MatterMeshOS/" MM_VERSION " " MM_DEVICE_TOKEN " tz-lookup");
  if(http.GET()==200){DynamicJsonDocument json(512);if(!deserializeJson(json,http.getString())&&!json["utc_offset_seconds"].isNull()){r.offsetSeconds=json["utc_offset_seconds"];r.ok=true;}}
  http.end();}
 return r;
}
static void applyOffset(int offsetSeconds){
 int hours=offsetSeconds/3600,minutes=abs(offsetSeconds%3600)/60;char tz[24];
 // POSIX TZ offset sign is inverted from common "UTC+N" usage (std offset = hours to ADD to local to reach
 // UTC); "LT", not "UTC", as the zone label so that inversion never reads as a mislabeled UTC value.
 if(minutes)snprintf(tz,sizeof(tz),"LT%+d:%02u",-hours,minutes);else snprintf(tz,sizeof(tz),"LT%+d",-hours);
 setenv("TZ",tz,1);tzset();prefs.putInt("offset",offsetSeconds);
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
}
