#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
// Reads a "knowledge pack" (see Travel LLM/packbuilder/FORMAT.md) and runs BM25 search over it - the exact
// seek-and-read access pattern and hashed-term index the Python packbuilder/packlib/reader.py implements, so
// results must match it byte-for-byte. Deliberately free of Arduino/SD/ESP-IDF includes: PackReader and
// Tokenizer only need the IO interface below, so this file and pack_reader.cpp compile and test on a host
// (see sim/test_pack_reader.cpp) exactly as they run on the device, which only supplies the IO implementation
// (pack_sd.h, backed by Arduino's File) and the JSON sidecar parsing (pack_loader.h, which needs ArduinoJson).
namespace pack {
// One pack file (passages.bin, index.bin or geo.bin), opened and owned by the caller.
class IO {
public:
 virtual ~IO()=default;
 virtual bool seek(uint32_t pos)=0;
 virtual size_t read(void *buf,size_t len)=0; // returns bytes actually read; short/zero on EOF or error
 virtual uint32_t size()=0;
};
struct Passage {
 uint32_t id=0;uint8_t kind=0;std::string title,text;bool hasGeo=false;double lat=0,lon=0;
};
struct SearchHit {uint32_t passageId;double score;};
struct NearbyHit {uint32_t passageId;double distanceKm;};
// Turns text into terms exactly as the builder does at index time (FORMAT.md section 1) - loaded from a
// pack's tokenizer.json via pack_loader.h, not constructed by hand (the fold table is Unicode-derived and
// belongs in the pack, so the firmware needs no Unicode library of its own).
struct Tokenizer {
 bool ngram=false;int minLen=2;
 std::unordered_map<uint32_t,std::string> fold; // code point -> replacement UTF-8 bytes (may be empty)
 std::unordered_set<std::string> stopwords; // already folded, as tokens() would produce them
 std::vector<std::string> tokens(const std::string &text) const;
};
uint32_t fnv1a32(const std::string &term);
struct Bm25Params {double k1=1.2,b=0.75;};
// Opens the binary files only; the caller already parsed the JSON sidecars (pack_loader.h) and the tokenizer
// and aliases are passed in. Does not take ownership of the IO pointers - the caller keeps them open for as
// long as the PackReader is used and closes them afterward.
class PackReader {
public:
 // false (with `error` set) on a header/version/count mismatch - geo may be null (no geo.bin in the pack).
 bool open(IO *passages,IO *index,IO *geo,Tokenizer tokenizer,
  std::unordered_map<std::string,std::vector<std::string>> aliases,Bm25Params bm25,std::string &error);
 // Drops the tokenizer's fold table and the alias map (the two things worth freeing promptly - a pack's own
 // SD-resident binary files cost nothing to "keep open" beyond the IO pointers themselves, which the caller
 // owns and closes). The caller forgets its IO pointers too; open() must be called again before reuse.
 void close(){*this=PackReader();}
 uint32_t numDocs() const {return numDocs_;}
 Passage passage(uint32_t id);
 // Best-first by score, ties broken by ascending passage id (matches packlib/reader.py).
 std::vector<SearchHit> search(const std::string &query,int k=5);
 // Nearest-first within radiusKm, empty if the pack has no geo.bin.
 std::vector<NearbyHit> nearby(double lat,double lon,double radiusKm=2.0,int k=10);
 std::vector<std::string> queryTerms(const std::string &query) const;
private:
 IO *p_=nullptr,*i_=nullptr,*g_=nullptr;
 uint32_t count_=0,offTab_=0;
 uint32_t numDocs_=0,numTerms_=0,totalTokens_=0,dirOff_=0,entOff_=0,dlOff_=0,postOff_=0;
 double avgdl_=1;
 Tokenizer tok_;
 std::unordered_map<std::string,std::vector<std::string>> aliases_;
 Bm25Params bm25_;
 bool lookup(uint32_t hash,uint32_t &postingsOff,uint32_t &df);
 std::vector<std::pair<uint32_t,uint8_t>> postings(uint32_t off,uint32_t df); // (docId, tf), ascending docId
 uint16_t doclen(uint32_t doc);
};
}
