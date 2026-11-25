#include "pack_sd.h"
#include "pack_loader.h"
#include <SD.h>
namespace device {
namespace {
class SdIO final: public pack::IO {
public:
 bool openFile(const String &path){f_=SD.open(path);return bool(f_);}
 void close(){if(f_)f_.close();}
 bool seek(uint32_t pos) override{return f_.seek(pos);}
 size_t read(void *buf,size_t len) override{return f_.read(reinterpret_cast<uint8_t*>(buf),len);}
 uint32_t size() override{return f_.size();}
private:
 File f_;
};
bool readWhole(const String &path,std::string &out){
 File f=SD.open(path);if(!f)return false;
 out.resize(f.size());
 if(out.size())f.read(reinterpret_cast<uint8_t*>(&out[0]),out.size());
 f.close();return true;
}
SdIO passagesIO,indexIO,geoIO;bool geoOpen=false;
pack::PackReader reader;bool isOpen=false;
}
bool packOpen(const char *id,PackInfo &info,std::string &error){
 packClose();
 String dir=String("/packs/")+id;
 std::string manifestJson,tokenizerJson,aliasesJson;
 if(!readWhole(dir+"/manifest.json",manifestJson)){error="missing manifest.json";return false;}
 if(!readWhole(dir+"/tokenizer.json",tokenizerJson)){error="missing tokenizer.json";return false;}
 if(!readWhole(dir+"/aliases.json",aliasesJson)){error="missing aliases.json";return false;}
 pack::ManifestInfo manifest;pack::Tokenizer tok;
 std::unordered_map<std::string,std::vector<std::string>> aliases;
 if(!pack::loadManifest(manifestJson,manifest,error))return false;
 if(!pack::loadTokenizer(tokenizerJson,tok,error))return false;
 if(!pack::loadAliases(aliasesJson,aliases,error))return false;
 if(!passagesIO.openFile(dir+"/passages.bin")){error="missing passages.bin";return false;}
 if(!indexIO.openFile(dir+"/index.bin")){passagesIO.close();error="missing index.bin";return false;}
 geoOpen=geoIO.openFile(dir+"/geo.bin"); // optional - a pack with no geo data just skips "near me"
 if(!reader.open(&passagesIO,&indexIO,geoOpen?&geoIO:nullptr,tok,aliases,manifest.bm25,error)){
  passagesIO.close();indexIO.close();if(geoOpen){geoIO.close();geoOpen=false;}
  return false;
 }
 info.id=manifest.id;info.name=manifest.name;info.language=manifest.language;info.passageCount=manifest.passageCount;info.hasGeo=geoOpen;
 readWhole(dir+"/ATTRIBUTION.txt",info.attribution); // optional - a pack missing it just shows no credit line
 isOpen=true;return true;
}
void packClose(){
 if(!isOpen)return;
 reader.close();passagesIO.close();indexIO.close();if(geoOpen){geoIO.close();geoOpen=false;}
 isOpen=false;
}
bool packIsOpen(){return isOpen;}
std::vector<pack::SearchHit> packSearch(const std::string &query,int k){return isOpen?reader.search(query,k):std::vector<pack::SearchHit>();}
std::vector<pack::NearbyHit> packNearby(double lat,double lon,double radiusKm,int k){return isOpen?reader.nearby(lat,lon,radiusKm,k):std::vector<pack::NearbyHit>();}
pack::Passage packPassage(uint32_t id){return isOpen?reader.passage(id):pack::Passage();}
}
