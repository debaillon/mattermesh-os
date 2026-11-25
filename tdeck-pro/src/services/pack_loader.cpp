#include "pack_loader.h"
#include <ArduinoJson.h>
namespace pack {
namespace {
// Fold keys are always exactly one code point (the builder's contract) - just enough decoding to get it.
uint32_t firstCodepoint(const char *s){
 unsigned char c=s[0];
 if(c<0x80)return c;
 if((c&0xE0)==0xC0)return uint32_t(c&0x1F)<<6|(uint8_t(s[1])&0x3F);
 if((c&0xF0)==0xE0)return uint32_t(c&0x0F)<<12|uint32_t(uint8_t(s[1])&0x3F)<<6|(uint8_t(s[2])&0x3F);
 return uint32_t(c&0x07)<<18|uint32_t(uint8_t(s[1])&0x3F)<<12|uint32_t(uint8_t(s[2])&0x3F)<<6|(uint8_t(s[3])&0x3F);
}
// tokenizer.json in particular is hundreds of short key/value pairs (the Unicode fold table): ArduinoJson's
// per-entry slot overhead dominates over the raw text size for a shape like that, so a generous multiple of
// the source size (checked against the actual tokenizer.json this pack format produces) is safer than a
// tight estimate - these are one-time, pack-open-time allocations, not a steady-state cost.
size_t capacityFor(const std::string &json){return json.size()*8+8192;}
}
bool loadManifest(const std::string &manifestJson,ManifestInfo &out,std::string &error){
 DynamicJsonDocument doc(capacityFor(manifestJson));
 if(deserializeJson(doc,manifestJson)){error="manifest.json: parse error";return false;}
 if((doc["format_version"]|0)!=1){error="manifest.json: unsupported format_version";return false;}
 out.id=std::string(doc["id"]|"");out.name=std::string(doc["name"]|"");out.language=std::string(doc["language"]|"en");
 out.passageCount=doc["counts"]["passages"]|0;out.termCount=doc["counts"]["terms"]|0;out.geoCount=doc["counts"]["geo"]|0;
 out.bm25.k1=doc["bm25"]["k1"]|1.2;out.bm25.b=doc["bm25"]["b"]|0.75;
 return true;
}
bool loadTokenizer(const std::string &tokenizerJson,Tokenizer &out,std::string &error){
 DynamicJsonDocument doc(capacityFor(tokenizerJson));
 if(auto e=deserializeJson(doc,tokenizerJson)){error=std::string("tokenizer.json: ")+e.c_str();return false;}
 std::string mode=std::string(doc["mode"]|"latin-fold");
 if(mode!="latin-fold"&&mode!="ngram"){error="tokenizer.json: unknown mode";return false;}
 out.ngram=mode=="ngram";out.minLen=doc["min_len"]|2;
 out.stopwords.clear();for(JsonVariant v:doc["stopwords"].as<JsonArray>())out.stopwords.insert(std::string(v.as<const char*>()));
 out.fold.clear();for(JsonPair kv:doc["fold"].as<JsonObject>())out.fold[firstCodepoint(kv.key().c_str())]=std::string(kv.value().as<const char*>());
 return true;
}
bool loadAliases(const std::string &aliasesJson,std::unordered_map<std::string,std::vector<std::string>> &out,std::string &error){
 DynamicJsonDocument doc(capacityFor(aliasesJson));
 if(deserializeJson(doc,aliasesJson)){error="aliases.json: parse error";return false;}
 out.clear();
 for(JsonPair kv:doc.as<JsonObject>()){
  std::vector<std::string> values;for(JsonVariant v:kv.value().as<JsonArray>())values.push_back(std::string(v.as<const char*>()));
  out[std::string(kv.key().c_str())]=std::move(values);
 }
 return true;
}
}
