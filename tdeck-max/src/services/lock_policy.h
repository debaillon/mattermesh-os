#pragma once
#include <stdint.h>
namespace device {
constexpr uint32_t LOCK_HOLD_MS=2000,LOCK_DEBOUNCE_MS=30,LOCK_PROGRESS_DELAY_MS=500;
constexpr uint32_t AUTO_LOCK_IDLE_MS=60000;
inline bool autoLockDue(uint32_t now,uint32_t lastActivity,bool locked){return !locked&&uint32_t(now-lastActivity)>=AUTO_LOCK_IDLE_MS;}
constexpr uint32_t LOCKED_CPU_FREQUENCY=40,NORMAL_CPU_FREQUENCY=240;
constexpr int SPACE_KEYCODE=33;
struct LockKey {
 bool down=false,consumed=false;uint32_t started=0,lastRelease=0;int progress=0;
 bool press(uint32_t now,bool hardwareDebounced=false){if(down||(!hardwareDebounced&&uint32_t(now-lastRelease)<LOCK_DEBOUNCE_MS))return false;down=true;consumed=false;started=now;progress=0;return true;}
 bool release(uint32_t now,bool locked){if(!down)return false;bool shortSpace=!consumed&&!locked;down=false;progress=0;lastRelease=now;return shortSpace;}
 bool tick(uint32_t now,bool locked){if(locked||!down||consumed)return false;uint32_t elapsed=now-started,limit=LOCK_HOLD_MS;progress=elapsed<LOCK_PROGRESS_DELAY_MS?0:1+int((elapsed-LOCK_PROGRESS_DELAY_MS)*5/(limit-LOCK_PROGRESS_DELAY_MS));if(elapsed<limit)return false;consumed=true;progress=0;return true;}
};
}
