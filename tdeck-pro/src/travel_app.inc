// Travel app (section 27): ask a question about a place, offline - the UI for services/pack_sd.h's
// knowledge-pack search (pack_reader.cpp has the actual BM25 logic, host-tested in sim/test_pack_reader.cpp).
// Modeled on cli_app.inc (type a line, get scrollable text): a query line, a scrollable results list, and a
// scrollable passage read screen. Included by main.cpp.
static int travelView=0,travelFocus=0,travelOffset=0,travelScroll=0;
static std::string travelInput,travelPackName,travelStatus,travelPassageTitle,travelPassageText,travelAttribution;
static bool travelPackOpen=false;
static std::vector<mm::TravelHit> travelHits;
static std::string travelBaseName(const char *path){std::string s=path;size_t slash=s.find_last_of('/');return slash==std::string::npos?s:s.substr(slash+1);}
// The pack to open is whichever single folder sits under /packs/ - "swap the SD card's pack to change the
// place" (see Travel LLM/CONVERSATION_EXPORT.md's design goal), so there is deliberately no picker UI yet;
// the first pack found wins if there happens to be more than one.
static void travelFindAndOpenPack(){
 if(device::packIsOpen())return;
 if(!sdReady){travelStatus="No SD card";return;}
 File dir=SD.open("/packs");
 if(!dir||!dir.isDirectory()){travelStatus="No /packs folder on the SD card";if(dir)dir.close();return;}
 std::string foundId;
 for(File f=dir.openNextFile();f;f=dir.openNextFile()){if(f.isDirectory()&&foundId.empty())foundId=travelBaseName(f.name());f.close();}
 dir.close();
 if(foundId.empty()){travelStatus="No pack found in /packs";return;}
 device::PackInfo info;std::string error;
 if(!device::packOpen(foundId.c_str(),info,error)){travelStatus="Error: "+error;return;}
 travelPackName=info.name.empty()?info.id:info.name;travelAttribution=info.attribution;
 travelStatus="Ask a question about "+travelPackName;travelPackOpen=true;
}
void travelAppOpen(){
 notice="";travelInput.clear();travelHits.clear();travelView=0;travelFocus=travelOffset=travelScroll=0;travelPassageTitle=travelPassageText="";
 travelFindAndOpenPack();
}
void travelAppClose(){device::packClose();travelPackOpen=false;}
static void travelHitsFrom(const std::vector<pack::SearchHit> &results){
 travelHits.clear();
 for(auto &h:results){auto p=device::packPassage(h.passageId);char score[16];snprintf(score,sizeof(score),"%.1f",h.score);mm::TravelHit hit;hit.passageId=h.passageId;hit.title=p.title.empty()?"(untitled)":p.title;hit.detail=score;travelHits.push_back(hit);}
}
static void travelHitsFromNearby(const std::vector<pack::NearbyHit> &results){
 travelHits.clear();
 for(auto &h:results){auto p=device::packPassage(h.passageId);char km[16];if(h.distanceKm<1)snprintf(km,sizeof(km),"%.0f m",h.distanceKm*1000);else snprintf(km,sizeof(km),"%.1f km",h.distanceKm);mm::TravelHit hit;hit.passageId=h.passageId;hit.title=p.title.empty()?"(untitled)":p.title;hit.detail=km;travelHits.push_back(hit);}
}
void travelSearch(){
 if(!travelPackOpen||travelInput.empty())return;
 travelHitsFrom(device::packSearch(travelInput,20));travelView=1;travelFocus=travelOffset=0;notice=travelHits.empty()?"No results":"";render();
}
void travelNear(){
 if(!travelPackOpen)return;
 auto gps=device::locationState();
 if(!gps.configured){notice="No GPS fix yet";render();return;}
 travelHitsFromNearby(device::packNearby(gps.latitude,gps.longitude,5.0,20));travelView=1;travelFocus=travelOffset=0;notice=travelHits.empty()?"Nothing within 5 km":"";render();
}
void travelOpenPassage(uint32_t id){
 auto p=device::packPassage(id);travelPassageTitle=p.title;travelPassageText=p.text;travelView=2;travelScroll=0;render();
}
bool travelAppKey(char c){
 if(travelView==0){
  if(c=='\n'){travelSearch();return true;}
  if(c=='\b'){if(travelInput.empty()){back();return true;}travelInput.erase(mm::utf8PrevStart(travelInput,travelInput.size()));notice="";repaintSettled();return true;}
  if(uint8_t(c)>=32&&uint8_t(c)<=126&&travelInput.size()<120){travelInput+=c;notice="";repaintSettled();return true;}
  return true;
 }
 if(travelView==1){
  if(c=='a'||c=='\b'){travelView=0;render();return true;}
  if((c=='w'||c=='k'||c=='s'||c=='j')&&!travelHits.empty()){travelFocus=(travelFocus+(c=='s'||c=='j'?1:int(travelHits.size())-1))%int(travelHits.size());if(travelFocus<travelOffset)travelOffset=travelFocus;else if(travelFocus>=travelOffset+mm::TravelRows)travelOffset=travelFocus-mm::TravelRows+1;render();return true;}
  if((c=='\n'||c=='d')&&travelFocus<int(travelHits.size()))travelOpenPassage(travelHits[travelFocus].passageId);
  return true;
 }
 // travelView==2: reading a passage.
 if(c=='a'||c=='\b'){travelView=1;render();return true;}
 if(c=='s'||c=='j'){travelScroll+=4;render();}else if(c=='w'||c=='k'){travelScroll=std::max(0,travelScroll-4);render();}
 return true;
}
bool travelAppTap(int x,int y){
 if(travelView==0){
  if(mm::TravelSearch.contains(x,y)){travelSearch();return true;}
  if(mm::TravelNear.contains(x,y)){travelNear();return true;}
  return false;
 }
 if(travelView==1){
  for(int row=0;row<mm::TravelRows&&travelOffset+row<int(travelHits.size());row++)if(mm::travelRowRect(row).contains(x,y)){travelOpenPassage(travelHits[travelOffset+row].passageId);return true;}
  return false;
 }
 return false;
}
void travelAppCompose(mm::State &v){
 v.travelView=travelView;v.travelInput=travelInput;v.travelPackName=travelPackName;v.travelStatus=travelStatus;v.travelPackOpen=travelPackOpen;
 v.travelHits=travelHits;v.travelFocus=travelFocus;v.travelOffset=travelOffset;v.travelScroll=travelScroll;
 v.travelPassageTitle=travelPassageTitle;v.travelPassageText=travelPassageText;v.travelAttribution=travelAttribution;
}
