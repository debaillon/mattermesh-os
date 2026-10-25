#pragma once
#include <Arduino.h>
#include "dictation_policy.h"
namespace device {
constexpr int MICROPHONE_KEYCODE=34;
struct AudioState {bool ready=false,notifications=true,busy=false,recording=false,warming=false/* MIC held, codec settling: nothing kept yet */,dictationConfigured=false;int messageTone=0,mentionTone=4,volume=20,level=0,peak=0,lastError=0;uint32_t revision=0,samples=0,playbacks=0,txBytes=0;bool transcribing=false,limitReached=false;uint32_t captureMs=0,stopLatencyMs=0,transcribeMs=0,pcmBytes=0,internalBefore=0,psramBefore=0,internalMin=0,psramMin=0;String message,transcript;};
bool audioDiagnosticPcm(const uint8_t *&data,size_t &bytes);
String audioDiagnosticResult();
uint32_t audioRevision();
bool takeTranscript(String &text);
// Not yet ported to the T-Deck Pro: the PCM5102A DAC / PDM mic are different hardware from the Max's ES7210/ES8311
// codec this service was written against. Every call here is a safe no-op so the shared UI code keeps compiling
// and running; audioState().ready stays false and the Sound/Dictation pages show "unavailable".
void audioBegin();void audioLoop();AudioState audioState();bool audioBusy();bool audioNotify(bool test=false,bool mention=false);bool audioPreview(int tone);void audioRingtone(bool mention,int tone);void audioNotifications(bool enabled);void audioVolume(int value);bool audioRecord();bool audioProbeRecord();void audioStop();void audioCancel();
}
