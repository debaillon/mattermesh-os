#pragma once
#include <Arduino.h>
namespace device {
constexpr int MICROPHONE_KEYCODE=34;// the MIC key types 0 on the Plus (no dictation)
// T-Deck Plus speaker (ES8311 + MAX98357A amplifier): notification tones and ringtone previews.
struct AudioState {bool ready=false,notifications=true,busy=false;int messageTone=0,mentionTone=4,volume=20,lastError=0;uint32_t revision=0,playbacks=0,txBytes=0;String message;};
uint32_t audioRevision();bool audioBusy();// cheap per-loop check (audioState() copies the message)
void audioBegin();void audioLoop();AudioState audioState();bool audioNotify(bool test=false,bool mention=false);bool audioPreview(int tone);void audioRingtone(bool mention,int tone);void audioNotifications(bool enabled);void audioVolume(int value);void audioCancel();
}
