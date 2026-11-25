#include "pack_reader.h"
#include "geo.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace pack {
namespace {
uint16_t rdU16(const uint8_t *b){return uint16_t(b[0])|uint16_t(b[1])<<8;}
uint32_t rdU32(const uint8_t *b){return uint32_t(b[0])|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;}
int32_t rdI32(const uint8_t *b){return int32_t(rdU32(b));}
struct Range{uint32_t lo,hi;};
const Range SEP_RANGES[]={
 {0x0080,0x00BF},{0x00D7,0x00D7},{0x00F7,0x00F7},{0x2000,0x2BFF},
 {0x3000,0x303F},{0xFE00,0xFE0F},{0xFEFF,0xFEFF},{0xFF00,0xFF0F},
 {0xFF1A,0xFF20},{0xFF3B,0xFF40},{0xFF5B,0xFF65},{0x1F000,0x1FAFF},
};
const Range CJK_RANGES[]={
 {0x3040,0x30FF},{0x3400,0x4DBF},{0x4E00,0x9FFF},{0xAC00,0xD7AF},{0xFF66,0xFF9F},
};
bool inRanges(uint32_t cp,const Range *ranges,size_t n){for(size_t i=0;i<n;i++)if(cp>=ranges[i].lo&&cp<=ranges[i].hi)return true;return false;}
// Decodes one UTF-8 code point at `pos`, sets `len` to its byte length (always >=1, even on a malformed
// lead byte, so a caller stepping pos+=len can never stall on bad input).
uint32_t decodeUtf8(const std::string &s,size_t pos,size_t &len){
 unsigned char c=s[pos];
 if(c<0x80){len=1;return c;}
 if((c&0xE0)==0xC0&&pos+1<s.size()){len=2;return uint32_t(c&0x1F)<<6|(uint8_t(s[pos+1])&0x3F);}
 if((c&0xF0)==0xE0&&pos+2<s.size()){len=3;return uint32_t(c&0x0F)<<12|uint32_t(uint8_t(s[pos+1])&0x3F)<<6|(uint8_t(s[pos+2])&0x3F);}
 if((c&0xF8)==0xF0&&pos+3<s.size()){len=4;return uint32_t(c&0x07)<<18|uint32_t(uint8_t(s[pos+1])&0x3F)<<12|uint32_t(uint8_t(s[pos+2])&0x3F)<<6|(uint8_t(s[pos+3])&0x3F);}
 len=1;return c;
}
size_t utf8Len(const std::string &s){size_t n=0;for(size_t i=0;i<s.size();){size_t len;decodeUtf8(s,i,len);i+=len;n++;}return n;}
}
uint32_t fnv1a32(const std::string &term){uint32_t h=0x811C9DC5;for(unsigned char c:term)h=(h^c)*0x01000193u;return h;}
std::vector<std::string> Tokenizer::tokens(const std::string &text) const{
 std::vector<std::string> out;
 std::string word;size_t wordCps=0;bool wordStarted=false;
 std::vector<std::string> cjk; // one entry per collected CJK code point, its raw UTF-8 bytes
 // Python's str.isdigit() also accepts non-ASCII Unicode digits; this pack format's fold table never
 // produces those, so an ASCII-only check matches every real pack without needing a digit-category table.
 auto allAsciiDigits=[](const std::string &w){if(w.empty())return false;for(unsigned char c:w)if(c<'0'||c>'9')return false;return true;};
 auto keep=[&](const std::string &w,size_t cps){
  if(int(cps)<minLen&&!allAsciiDigits(w))return false;
  return stopwords.find(w)==stopwords.end();
 };
 auto flushWord=[&](){
  if(wordStarted){if(!word.empty()&&keep(word,wordCps))out.push_back(word);word.clear();wordCps=0;wordStarted=false;}
 };
 auto flushCjk=[&](){
  if(!cjk.empty()){
   if(cjk.size()==1)out.push_back(cjk[0]);
   else for(size_t i=0;i+1<cjk.size();i++)out.push_back(cjk[i]+cjk[i+1]);
   cjk.clear();
  }
 };
 size_t pos=0;
 while(pos<text.size()){
  size_t clen;uint32_t cp=decodeUtf8(text,pos,clen);
  if(ngram&&inRanges(cp,CJK_RANGES,sizeof(CJK_RANGES)/sizeof(Range))){flushWord();cjk.push_back(text.substr(pos,clen));pos+=clen;continue;}
  flushCjk();
  bool present=true;std::string piece;
  if(cp<128){
   if((cp>='a'&&cp<='z')||(cp>='A'&&cp<='Z')||(cp>='0'&&cp<='9'))piece=std::string(1,char(cp>='A'&&cp<='Z'?cp+32:cp));
   else present=false;
  }else if(inRanges(cp,SEP_RANGES,sizeof(SEP_RANGES)/sizeof(Range))){
   present=false;
  }else{
   auto it=fold.find(cp);piece=it!=fold.end()?it->second:text.substr(pos,clen);
  }
  if(!present)flushWord();
  else{wordStarted=true;word+=piece;wordCps+=utf8Len(piece);}
  pos+=clen;
 }
 flushWord();flushCjk();
 return out;
}
bool PackReader::open(IO *passages,IO *index,IO *geo,Tokenizer tokenizer,
 std::unordered_map<std::string,std::vector<std::string>> aliases,Bm25Params bm25,std::string &error){
 p_=passages;i_=index;g_=geo;tok_=std::move(tokenizer);aliases_=std::move(aliases);bm25_=bm25;
 uint8_t hdr[32];
 if(!p_->seek(0)||p_->read(hdr,32)!=32){error="passages.bin: short read";return false;}
 if(memcmp(hdr,"PPAS",4)!=0||rdU16(hdr+4)!=1){error="passages.bin: bad header";return false;}
 count_=rdU32(hdr+8);offTab_=rdU32(hdr+12);
 uint8_t ihdr[36];
 if(!i_->seek(0)||i_->read(ihdr,36)!=36){error="index.bin: short read";return false;}
 if(memcmp(ihdr,"PIDX",4)!=0||rdU16(ihdr+4)!=1){error="index.bin: bad header";return false;}
 numDocs_=rdU32(ihdr+8);numTerms_=rdU32(ihdr+12);totalTokens_=rdU32(ihdr+16);
 dirOff_=rdU32(ihdr+20);entOff_=rdU32(ihdr+24);dlOff_=rdU32(ihdr+28);postOff_=rdU32(ihdr+32);
 if(numDocs_!=count_){error="passages/index count mismatch";return false;}
 avgdl_=numDocs_?double(totalTokens_)/numDocs_:1;
 if(g_){
  uint8_t ghdr[16];
  if(!g_->seek(0)||g_->read(ghdr,16)!=16||memcmp(ghdr,"PGEO",4)!=0||rdU16(ghdr+4)!=1)g_=nullptr; // optional: drop a bad/absent geo.bin rather than fail the whole pack
 }
 return true;
}
Passage PackReader::passage(uint32_t id){
 Passage out;out.id=id;
 if(id>=count_||!p_->seek(offTab_+4*id))return out;
 uint8_t pair[8];if(p_->read(pair,8)!=8)return out;
 uint32_t start=rdU32(pair),end=rdU32(pair+4);
 if(end<start||end-start<5||!p_->seek(start))return out;
 uint32_t len=end-start;
 std::vector<uint8_t> rec(len);
 if(p_->read(rec.data(),len)!=len)return out;
 uint8_t flags=rec[0];out.kind=rec[1];uint8_t titleLen=rec[2];uint16_t textLen=rdU16(&rec[3]);
 size_t pos=5;
 if(flags&1){
  if(pos+8>len)return out;
  out.hasGeo=true;out.lat=rdI32(&rec[pos])/1e6;out.lon=rdI32(&rec[pos+4])/1e6;pos+=8;
 }
 if(pos+titleLen+textLen>len)return out;
 out.title.assign(reinterpret_cast<char*>(&rec[pos]),titleLen);pos+=titleLen;
 out.text.assign(reinterpret_cast<char*>(&rec[pos]),textLen);
 return out;
}
bool PackReader::lookup(uint32_t hash,uint32_t &postingsOff,uint32_t &df){
 uint32_t bucket=hash>>16;
 if(!i_->seek(dirOff_+4*bucket))return false;
 uint8_t buf[8];if(i_->read(buf,8)!=8)return false;
 uint32_t lo=rdU32(buf),hi=rdU32(buf+4);
 if(hi<=lo||!i_->seek(entOff_+12*lo))return false;
 std::vector<uint8_t> raw(size_t(12)*(hi-lo));
 if(i_->read(raw.data(),raw.size())!=raw.size())return false;
 for(uint32_t k=0;k<hi-lo;k++){
  const uint8_t *e=&raw[12*k];
  if(rdU32(e)==hash){postingsOff=rdU32(e+4);df=rdU32(e+8);return true;}
 }
 return false;
}
std::vector<std::pair<uint32_t,uint8_t>> PackReader::postings(uint32_t off,uint32_t df){
 std::vector<std::pair<uint32_t,uint8_t>> out;out.reserve(df);
 if(!i_->seek(off))return out;
 std::vector<uint8_t> buf(size_t(df)*6); // 5-byte varint + 1-byte tf worst case per entry, matching the builder/reference reader
 size_t got=i_->read(buf.data(),buf.size());
 size_t pos=0;uint32_t doc=0;
 for(uint32_t k=0;k<df;k++){
  uint32_t delta=0,shift=0;
  for(;;){
   if(pos>=got)return out;
   uint8_t b=buf[pos++];delta|=uint32_t(b&0x7F)<<shift;
   if(!(b&0x80))break;
   shift+=7;
  }
  doc+=delta;
  if(pos>=got)return out;
  out.push_back({doc,buf[pos++]});
 }
 return out;
}
uint16_t PackReader::doclen(uint32_t doc){
 if(!i_->seek(dlOff_+2*doc))return 0;
 uint8_t b[2];if(i_->read(b,2)!=2)return 0;
 return rdU16(b);
}
std::vector<std::string> PackReader::queryTerms(const std::string &query) const{
 std::vector<std::string> terms;
 for(auto &t:tok_.tokens(query)){
  terms.push_back(t);
  auto it=aliases_.find(t);
  if(it!=aliases_.end())for(auto &a:it->second)terms.push_back(a);
 }
 std::vector<std::string> uniq;std::unordered_set<std::string> seen;
 for(auto &t:terms)if(seen.insert(t).second)uniq.push_back(t);
 return uniq;
}
std::vector<SearchHit> PackReader::search(const std::string &query,int k){
 std::unordered_map<uint32_t,double> scores;
 double n=numDocs_;
 for(auto &t:queryTerms(query)){
  uint32_t off,df;
  if(!lookup(fnv1a32(t),off,df))continue;
  double idf=std::log(1.0+(n-df+0.5)/(df+0.5));
  for(auto &pr:postings(off,df)){
   uint32_t doc=pr.first;uint8_t tf=pr.second;uint16_t dl=doclen(doc);
   double s=idf*tf*(bm25_.k1+1)/(tf+bm25_.k1*(1-bm25_.b+bm25_.b*dl/avgdl_));
   scores[doc]+=s;
  }
 }
 std::vector<SearchHit> ranked;ranked.reserve(scores.size());
 for(auto &kv:scores)ranked.push_back({kv.first,kv.second});
 std::sort(ranked.begin(),ranked.end(),[](const SearchHit &a,const SearchHit &b){return a.score!=b.score?a.score>b.score:a.passageId<b.passageId;});
 if(int(ranked.size())>k)ranked.resize(k);
 return ranked;
}
std::vector<NearbyHit> PackReader::nearby(double lat,double lon,double radiusKm,int k){
 std::vector<NearbyHit> out;
 if(!g_)return out;
 uint8_t cbuf[4];
 if(!g_->seek(8)||g_->read(cbuf,4)!=4)return out;
 uint32_t count=rdU32(cbuf);
 double dlat=radiusKm/111.0;
 int32_t loKey=int32_t((lat-dlat)*1e6); // truncating cast, matching Python's int(): a search bound, not a stored value
 double latHiBound=(lat+dlat)*1e6;
 auto entry=[&](uint32_t idx,int32_t &la,int32_t &lo2,uint32_t &doc){
  if(!g_->seek(16+12*idx))return false;
  uint8_t b[12];if(g_->read(b,12)!=12)return false;
  la=rdI32(b);lo2=rdI32(b+4);doc=rdU32(b+8);return true;
 };
 uint32_t lo=0,hi=count;
 while(lo<hi){
  uint32_t mid=lo+(hi-lo)/2;int32_t la,lo2;uint32_t doc;
  if(!entry(mid,la,lo2,doc))break;
  if(la<loKey)lo=mid+1;else hi=mid;
 }
 for(uint32_t idx=lo;idx<count;idx++){
  int32_t la,lo2;uint32_t doc;
  if(!entry(idx,la,lo2,doc))break;
  if(double(la)>latHiBound)break;
  double distKm=mm::geoDistanceMeters(lat,lon,la/1e6,lo2/1e6)/1000.0;
  if(distKm<=radiusKm)out.push_back({doc,distKm});
 }
 std::sort(out.begin(),out.end(),[](const NearbyHit &a,const NearbyHit &b){return a.distanceKm<b.distanceKm;});
 if(int(out.size())>k)out.resize(k);
 return out;
}
}
