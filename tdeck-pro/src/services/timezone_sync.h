#pragma once
// Auto-detects the local UTC offset from the device's GPS fix (see location.h) via a timezone lookup, and
// applies it as the process TZ (see time_sync.cpp, which otherwise hardcodes MM_TZ at boot). A fixed UTC
// offset, not a full IANA rule: ESP32 Arduino doesn't ship a zoneinfo database, so there's no DST calendar
// to follow automatically - the offset is simply re-resolved whenever the GPS fix moves meaningfully, which
// corrects itself across a DST transition the next time that happens rather than at the exact moment.
namespace device {
void timezoneSyncBegin();
void timezoneSyncLoop();
}
