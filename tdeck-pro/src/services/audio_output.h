#pragma once
#include <AudioOutputI2S.h>
#include "../board/board.h"
// Single shared PCM5102A I2S output, used by both the notification-tone player (audio.cpp) and the MP3
// player (music.cpp): the ESP32 has one I2S peripheral per port, so only one AudioOutputI2S may own it.
//
// Tried switching this to schreibfaul1/ESP32-audioI2S (the library Fennek, a different open-source firmware
// for this exact board, uses successfully for its own music player) to get its large PSRAM read-ahead
// buffer. Confirmed decoding correctly (position advanced in near-real-time once the CPU-frequency fix
// below was in place) but produced no audible output at all, on both the tone and music paths, for a reason
// not tracked down; reverted rather than keep debugging an unfamiliar library's I2S setup blind. This
// library (ESP8266Audio) is proven audible on this hardware - a previewed tone and actual MP3 playback were
// both confirmed heard in testing. The fix that matters most is in main.cpp
// (requestedLockedCpu()/idleCpuLoop()): keeping the CPU off its power-saving 80 MHz clock while anything is
// actually playing, which is what let decode fall behind real time.
namespace device {
inline AudioOutputI2S &i2sOutput(){
 // The real cause of the long total-silence/chirp regression was a missing SetRate() call on the tone path
 // (see audio.cpp), not this constructor - APLL and dma_buf_count were red herrings chased before that was
 // found. Plain default dma_buf_count=8: raising it to 32 (and separately, wrapping the SD source in a
 // read-ahead buffer in music.cpp) was tried to smooth occasional crackle during MP3 playback and made it
 // worse both times - confirmed on hardware - so left at the default.
 static AudioOutputI2S out(0,AudioOutputI2S::EXTERNAL_I2S);
 static bool configured=false;
 if(!configured){configured=true;out.SetPinout(BOARD_I2S_BCLK,BOARD_I2S_LRC,BOARD_I2S_DOUT);}
 return out;
}
}
