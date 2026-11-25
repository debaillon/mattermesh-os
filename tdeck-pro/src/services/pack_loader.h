#pragma once
#include "pack_reader.h"
// Parses a pack's three small JSON sidecar files (FORMAT.md sections 5 and 7) into the Tokenizer/aliases/bm25
// PackReader::open() needs. Needs ArduinoJson (portable, no Arduino.h dependency - see pack_reader.h's header
// comment), so it is kept separate from pack_reader.cpp, which has none; both still compile and test on a
// host (sim/test_pack_reader.cpp) exactly as on the device. The caller reads each file's bytes itself (SD on
// the device, plain file I/O on a host) and passes the text in - this file does no file I/O of its own.
namespace pack {
struct ManifestInfo {
 std::string id,name,language;
 uint32_t passageCount=0,termCount=0,geoCount=0;
 Bm25Params bm25;
};
// false (with `error` set) if manifest.json's format_version isn't 1, or any of the three documents fails
// to parse.
bool loadManifest(const std::string &manifestJson,ManifestInfo &out,std::string &error);
bool loadTokenizer(const std::string &tokenizerJson,Tokenizer &out,std::string &error);
bool loadAliases(const std::string &aliasesJson,std::unordered_map<std::string,std::vector<std::string>> &out,std::string &error);
}
