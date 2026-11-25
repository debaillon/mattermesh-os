// Host test for services/pack_reader.cpp + pack_loader.cpp (the device-side Portugal/travel-pack search
// service). Loads the real pack fixtures/packs/portugal/ (built by the real Python packbuilder - see
// Travel LLM/packbuilder/gen_test_fixture.py, which also wrote fixtures/portugal_oracle.json by running the
// same queries through packlib/reader.py, the format's executable specification) through this exact
// production code path, and checks the results match that oracle: FORMAT.md requires the device reproduce
// the Python reader byte-for-byte, so the oracle - not a hand-picked expectation here - is the source of
// truth. Build: see sim/README.md.
#include "services/pack_reader.h"
#include "services/pack_loader.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
namespace {
int failures=0;
void check(bool ok,const std::string &what){if(!ok){failures++;printf("FAIL: %s\n",what.c_str());}}
std::string readFile(const std::string &path){
 std::ifstream f(path,std::ios::binary);
 if(!f)throw std::runtime_error("cannot open "+path);
 std::ostringstream ss;ss<<f.rdbuf();return ss.str();
}
// Host-only PackIO backed by plain POSIX file I/O, mirroring what pack_sd.cpp does with Arduino's File on
// the device - pack_reader.cpp only ever sees the IO interface, never this.
class FileIO final: public pack::IO {
public:
 explicit FileIO(const std::string &path):f_(fopen(path.c_str(),"rb")){if(!f_)throw std::runtime_error("cannot open "+path);}
 ~FileIO() override{if(f_)fclose(f_);}
 bool seek(uint32_t pos) override{return fseek(f_,long(pos),SEEK_SET)==0;}
 size_t read(void *buf,size_t len) override{return fread(buf,1,len,f_);}
 uint32_t size() override{long cur=ftell(f_);fseek(f_,0,SEEK_END);long end=ftell(f_);fseek(f_,cur,SEEK_SET);return uint32_t(end);}
private:
 FILE *f_;
};
std::string fixtures(const std::string &rel){return "sim/fixtures/"+rel;}
}
static void testFnvKnownVectors(){
 check(pack::fnv1a32("")==0x811C9DC5u,"fnv1a32(\"\")");
 check(pack::fnv1a32("a")==0xE40C292Cu,"fnv1a32(\"a\")");
 check(pack::fnv1a32("foobar")==0xBF9CF968u,"fnv1a32(\"foobar\")");
}
static pack::Tokenizer loadTok(const std::string &path){
 pack::Tokenizer tok;std::string error;
 if(!pack::loadTokenizer(readFile(path),tok,error))throw std::runtime_error(error);
 return tok;
}
static void expectTokens(const pack::Tokenizer &tok,const std::string &text,std::vector<std::string> want,const std::string &label){
 auto got=tok.tokens(text);
 check(got==want,label+" tokens() mismatch");
 if(got!=want){
  printf("  got:");for(auto &t:got)printf(" [%s]",t.c_str());printf("\n");
  printf("  want:");for(auto &t:want)printf(" [%s]",t.c_str());printf("\n");
 }
}
static void testLatinFoldTokenizer(const pack::Tokenizer &tok){
 // Same assertions as packbuilder/tests/test_packlib.py's TestPrimitives tokenizer cases.
 expectTokens(tok,"S\xc3\xa3o Jorge, Past\xc3\xa9is de BEL\xc3\x89M!",{"sao","jorge","pasteis","de","belem"},"accents+case");
 expectTokens(tok,"Caf\xc3\xa9",{"cafe"},"decomposed accent");
 expectTokens(tok,"Stra\xc3\x9f" "e",{"strasse"},"sharp s");
 expectTokens(tok,"\xc3\x86r\xc3\xb8 \xc5\x81\xc3\xb3" "d\xc5\xba",{"aero","lodz"},"extra fold table");
 // Unlike packbuilder/tests/test_packlib.py's bare Tokenizer() (no stopwords configured), this pack's real
 // tokenizer carries English stopwords, so "and" is correctly dropped here too.
 expectTokens(tok,"fish and\xe2\x80\x94" "chips \xe2\x80\x9cquoted\xe2\x80\x9d",{"fish","chips","quoted"},"separators");
 // Confirms digits bypass min_len and real stopwords ("the") from this pack's language ("en") are dropped.
 auto toks=tok.tokens("the Hotel 5 stars");
 check(toks==std::vector<std::string>({"hotel","5","stars"}),"stopword+digit handling");
}
static void testNgramTokenizer(){
 auto tok=loadTok(fixtures("ngram_tokenizer.json"));
 expectTokens(tok,"\xe6\x9d\xb1\xe4\xba\xac\xe3\x82\xbf\xe3\x83\xaf\xe3\x83\xbc sushi",
  {"\xe6\x9d\xb1\xe4\xba\xac","\xe4\xba\xac\xe3\x82\xbf","\xe3\x82\xbf\xe3\x83\xaf","\xe3\x83\xaf\xe3\x83\xbc","sushi"},"CJK bigrams");
 expectTokens(tok,"\xe4\xba\xac",{"\xe4\xba\xac"},"single CJK char");
}
struct Pack {
 FileIO passages,index,geo;
 pack::PackReader reader;
 explicit Pack(const std::string &dir):
  passages(dir+"/passages.bin"),index(dir+"/index.bin"),geo(dir+"/geo.bin"){
  pack::ManifestInfo manifest;pack::Tokenizer tok;
  std::unordered_map<std::string,std::vector<std::string>> aliases;std::string error;
  if(!pack::loadManifest(readFile(dir+"/manifest.json"),manifest,error))throw std::runtime_error(error);
  if(!pack::loadTokenizer(readFile(dir+"/tokenizer.json"),tok,error))throw std::runtime_error(error);
  if(!pack::loadAliases(readFile(dir+"/aliases.json"),aliases,error))throw std::runtime_error(error);
  if(!reader.open(&passages,&index,&geo,tok,aliases,manifest.bm25,error))throw std::runtime_error(error);
 }
};
static void testSearchAgainstOracle(pack::PackReader &reader){
 DynamicJsonDocument doc(65536);
 auto err=deserializeJson(doc,readFile(fixtures("portugal_oracle.json")));
 check(!err,"oracle JSON parse");
 for(JsonObject q:doc["queries"].as<JsonArray>()){
  std::string query=q["query"].as<const char*>();
  auto hits=reader.search(query,5);
  JsonArray want=q["hits"].as<JsonArray>();
  check(hits.size()==want.size(),"search(\""+query+"\") hit count ("+std::to_string(hits.size())+" vs "+std::to_string(want.size())+")");
  for(size_t i=0;i<hits.size()&&i<want.size();i++){
   uint32_t wantId=want[i]["id"];double wantScore=want[i]["score"];
   check(hits[i].passageId==wantId,"search(\""+query+"\")["+std::to_string(i)+"].id");
   check(std::fabs(hits[i].score-wantScore)<1e-6,"search(\""+query+"\")["+std::to_string(i)+"].score "+std::to_string(hits[i].score)+" vs "+std::to_string(wantScore));
  }
 }
}
static void testNearbyAgainstOracle(pack::PackReader &reader){
 DynamicJsonDocument doc(65536);
 deserializeJson(doc,readFile(fixtures("portugal_oracle.json")));
 for(JsonObject n:doc["nearby"].as<JsonArray>()){
  double lat=n["lat"],lon=n["lon"],radius=n["radius_km"];
  auto hits=reader.nearby(lat,lon,radius,10);
  JsonArray want=n["hits"].as<JsonArray>();
  std::string label="nearby("+std::to_string(lat)+","+std::to_string(lon)+")";
  check(hits.size()==want.size(),label+" hit count");
  for(size_t i=0;i<hits.size()&&i<want.size();i++){
   uint32_t wantId=want[i]["id"];double wantKm=want[i]["distance_km"];
   check(hits[i].passageId==wantId,label+"["+std::to_string(i)+"].id");
   check(std::fabs(hits[i].distanceKm-wantKm)<1e-3,label+"["+std::to_string(i)+"].distance_km");
  }
 }
}
int main(){
 try{
  testFnvKnownVectors();
  testNgramTokenizer();
  Pack pack(fixtures("packs/portugal"));
  check(pack.reader.numDocs()>0,"pack opened with passages");
  testLatinFoldTokenizer(loadTok(fixtures("packs/portugal/tokenizer.json")));
  testSearchAgainstOracle(pack.reader);
  testNearbyAgainstOracle(pack.reader);
 }catch(const std::exception &e){
  printf("FAIL: exception: %s\n",e.what());
  failures++;
 }
 if(failures)printf("%d check(s) FAILED\n",failures);
 else printf("All checks passed\n");
 return failures?1:0;
}
