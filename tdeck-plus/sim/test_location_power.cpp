#include <cassert>
#include <cstdio>
#include <iostream>
#include <Preferences.h>
#include "board/board.h"
#include <TinyGPS++.h>
#include "services/location.h"
#include "services/gps_position_policy.h"
namespace device {bool timeSyncGPS(uint32_t,bool,bool){return false;}}
// Plus: the module is fed by BOARD_POWERON (no power switch), so "off" is a UBX-RXM-PMREQ backup request sent at both bauds.
static std::string pmreq(){const uint8_t f[]={0xB5,0x62,0x02,0x41,16,0,0,0,0,0,0,0,0,0,2,0,0,0,8,0,0,0};std::string s((const char*)f,sizeof(f));uint8_t a=0,b=0;for(size_t i=2;i<s.size();i++){a+=uint8_t(s[i]);b+=a;}return s+char(a)+char(b);}
static size_t sleeps(){size_t n=0,at=0;auto f=pmreq();while((at=SerialGPS.written.find(f,at))!=std::string::npos){n++;at+=f.size();}SerialGPS.written.clear();return n;}
int main(){using namespace device;locationBegin();assert(locationState().available&&locationState().enabled&&SerialGPS.active&&SerialGPS.baud==38400);
 assert(SerialGPS.written==std::string(8,char(0xFF)));SerialGPS.written.clear();// ON: wake bytes (RX edge) out of backup
 assert(locationSet(45.9,7.6));auto before=locationState();assert(locationEnabled(false));assert(!locationState().enabled&&!locationState().fix&&!locationState().data&&!SerialGPS.active&&locationState().baud==0&&sleeps()==2);
 assert(locationState().latitude==before.latitude&&locationState().longitude==before.longitude&&locationState().configured);
 SerialGPS.bytes=100;unsigned reads=SerialGPS.reads,begins=SerialGPS.begins;fakeMillis+=60000;locationLoop();assert(SerialGPS.reads==reads&&SerialGPS.begins==begins);
 // Startup honors OFF: the UART only opens to send the backup request at 38400 and 9600, then closes; cached coordinates survive.
 locationBegin();assert(!locationState().enabled&&SerialGPS.begins==begins+2&&!SerialGPS.active&&sleeps()==2);
 assert(locationEnabled(true));assert(SerialGPS.active&&SerialGPS.baud==38400&&!locationState().fix);
 fakeMillis+=5001;locationLoop();assert(locationState().baud==9600&&SerialGPS.baud==9600);
 SerialGPS.bytes=4;locationLoop();assert(locationState().data&&SerialGPS.reads==reads+4);
 fakePrefsFailure=true;assert(!locationEnabled(false));assert(locationState().enabled&&SerialGPS.active);fakePrefsFailure=false;
 assert(locationEnabled(false));assert(locationSet(45,9));assert(locationState().configured&&locationState().latitude==45);
 assert(locationEnabled(true));fakeGpsValid=fakeGpsUpdated=true;fakeGpsAge=100;fakeGpsLatitude=45.934123;fakeGpsLongitude=7.629456;locationLoop();auto live=locationState();
 assert(live.fix&&live.latitude==fakeGpsLatitude&&live.longitude==fakeGpsLongitude&&live.fixAgeMs==100);
 assert(gpsCoordinateMessage(live.available,live.enabled,live.fix,live.fixAgeMs,live.latitude,live.longitude)=="GPS: 45.934123, 7.629456");
 fakeGpsAge=15000;assert(!locationState().fix);locationLoop();assert(!locationState().fix);
 assert(locationEnabled(false));assert(locationState().fixAgeMs==UINT32_MAX&&!locationState().fix);fakeGpsValid=false;
 // Counts update from real checksum-valid GGA/GSV while no position exists.
 assert(locationEnabled(true));fakeGpsValid=false;std::string text="GPGSV,1,1,07,01,40,100,30";unsigned checksum=0;for(char c:text)checksum^=uint8_t(c);char tail[8];snprintf(tail,sizeof(tail),"*%02X\r\n",checksum);SerialGPS.inject("$"+text+tail);locationLoop();assert(!locationState().fix&&locationState().nmeaFresh&&locationState().satellitesVisible==7&&locationState().satellites==7);
 fakeMillis+=15001;locationLoop();assert(!locationState().nmeaFresh&&!locationState().satellites);
 assert(locationEnabled(false));auto saved=fakePrefs;assert(locationTimeLease(true));assert(locationState().enabled&&!locationState().userEnabled&&locationState().timeLease&&fakePrefs==saved);
 assert(locationTimeLease(false)&&!locationState().enabled&&fakePrefs==saved);
 assert(locationTimeLease(true));assert(locationEnabled(true));assert(locationTimeLease(false)&&locationState().enabled&&locationState().userEnabled);
 assert(locationEnabled(false));assert(locationTimeLease(true));unsigned runningBegins=SerialGPS.begins,runningEnds=SerialGPS.ends;
 assert(locationToggleUser()&&locationState().userEnabled&&locationState().enabled);
 assert(SerialGPS.begins==runningBegins&&SerialGPS.ends==runningEnds); // Pin ON without restarting acquisition.
 assert(locationTimeLease(false)&&locationState().enabled);
 std::cout<<"PASS: GPS power/UART OFF, polling stops, cached position survives, persisted OFF startup, fresh ON/baud fallback, live fix replaces cache/fresh age/reject stale and OFF, sleep request on OFF (both bauds), NVS failure rollback\n";
}
