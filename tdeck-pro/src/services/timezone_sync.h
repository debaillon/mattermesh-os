#pragma once
// Auto-detects the local UTC offset from the device's GPS fix (see location.h) via a timezone lookup, and
// applies it as the process TZ (see time_sync.cpp, which otherwise hardcodes MM_TZ at boot). A fixed UTC
// offset, not a full IANA rule: ESP32 Arduino doesn't ship a zoneinfo database, so there's no DST calendar
// to follow automatically - the offset is simply re-resolved whenever the GPS fix moves meaningfully, which
// corrects itself across a DST transition the next time that happens rather than at the exact moment.
namespace device {
void timezoneSyncBegin();
void timezoneSyncLoop();
// Forces a fresh lookup attempt on the next timezoneSyncLoop() call, bypassing the "close enough to the
// last resolved fix" cache and the 10-minute rate limit - for a user-initiated "update now" action. Still
// needs a GPS fix and Wi-Fi to actually do anything; timezoneSyncReady() reports whether those are in place
// right now, so the caller can give honest feedback instead of a blind "requested".
void timezoneSyncRequest();
bool timezoneSyncReady();
// The raw UTC offset last resolved from GPS (or loaded from NVS at boot), if any. device::localBreakdown()
// uses this directly instead of setenv("TZ",...)+tzset(): confirmed on hardware that a second tzset() call
// with a different value doesn't reliably change localtime_r()'s output on this platform, so the offset is
// now applied by plain arithmetic rather than through the OS timezone mechanism. Returns false (seconds left
// untouched) if nothing has been resolved yet this boot, in which case the MM_TZ compiled default applies.
bool timezoneOffsetSeconds(int &seconds);
}
