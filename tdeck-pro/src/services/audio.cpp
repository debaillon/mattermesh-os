#include "audio.h"
#include "ringtones.h"
#include <Preferences.h>
// Stub audio service for the T-Deck Pro MVP port: see audio.h. Preferences (volume/tones/notifications) are kept
// so Settings > Sound behaves normally and the real codec work can be dropped in later without a settings reset.
namespace device {
static Preferences prefs;static AudioState state;
void audioBegin(){prefs.begin("mmaudio",false);state.dictationConfigured=false;state.notifications=prefs.getBool("notify",true);state.volume=constrain(prefs.getInt("volume",20),0,100);state.messageTone=ringtoneIndex(prefs.getInt("tone",RINGTONE_MESSAGE_DEFAULT));state.mentionTone=ringtoneIndex(prefs.getInt("mtone",RINGTONE_MENTION_DEFAULT));state.ready=false;state.message="Audio not yet supported on this board";}
bool audioNotify(bool,bool){return false;}
bool audioPreview(int){return false;}
void audioRingtone(bool mention,int tone){tone=ringtoneIndex(tone);(mention?state.mentionTone:state.messageTone)=tone;prefs.putInt(mention?"mtone":"tone",tone);state.revision++;}
bool audioRecord(){return false;}
bool audioProbeRecord(){return false;}
void audioStop(){}
void audioCancel(){}
void audioNotifications(bool enabled){state.notifications=enabled;prefs.putBool("notify",enabled);state.revision++;}
void audioVolume(int value){state.volume=constrain(value,0,100);prefs.putInt("volume",state.volume);state.revision++;}
bool audioDiagnosticPcm(const uint8_t *&data,size_t &bytes){data=nullptr;bytes=0;return false;}
String audioDiagnosticResult(){return String("");}
uint32_t audioRevision(){return state.revision;}
bool audioBusy(){return false;}
AudioState audioState(){return state;}
bool takeTranscript(String &text){(void)text;return false;}
void audioLoop(){}
}
