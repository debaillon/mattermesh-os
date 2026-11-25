#pragma once
#include "pack_reader.h"
// Device-side binding for pack_reader.h: opens a pack from /packs/<id>/ on the SD card and exposes
// search/nearby/passage for a UI app to call. The pure search logic lives in pack_reader.cpp (host-tested,
// see sim/test_pack_reader.cpp); this file only supplies the SD-backed pack::IO and the JSON sidecar reads.
// Runs synchronously on whichever task calls it - like epub.cpp's zip reads, it does not take the global SPI
// mutex (services/spi_lock.h) itself; that is only needed for access from a separate background task, which
// this service does not have.
namespace device {
struct PackInfo {std::string id,name,language,attribution;uint32_t passageCount=0;bool hasGeo=false;};
// false (with `error` set) if the folder or any required file is missing or malformed. Closes whatever pack
// was open first - only one pack is open at a time.
bool packOpen(const char *id,PackInfo &info,std::string &error);
void packClose();
bool packIsOpen();
std::vector<pack::SearchHit> packSearch(const std::string &query,int k=5);
std::vector<pack::NearbyHit> packNearby(double lat,double lon,double radiusKm=2.0,int k=10);
pack::Passage packPassage(uint32_t id);
}
