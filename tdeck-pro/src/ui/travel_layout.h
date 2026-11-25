#pragma once
#include "theme.h"
// Travel app (section 27): a knowledge-pack query line, a scrollable results list, and a scrollable passage
// read screen - see services/pack_sd.h.
namespace mm {
constexpr int TravelSection=27;
// Bottom bar: the query input plus two buttons, same strip CLI's bar uses.
constexpr Rect TravelBar={Margin-4,H-36,W-2*Margin+8,32};
constexpr int TravelButtonW=66,TravelButtonGap=6;
constexpr Rect TravelNear={TravelBar.x+TravelBar.w-4-TravelButtonW,TravelBar.y+2,TravelButtonW,TravelBar.h-4};
constexpr Rect TravelSearch={TravelNear.x-TravelButtonGap-TravelButtonW,TravelBar.y+2,TravelButtonW,TravelBar.h-4};
constexpr Rect TravelInput={Margin,TravelBar.y+2,TravelSearch.x-TravelButtonGap-Margin,TravelBar.h-4};
// Body: a status line (query view) or a scrollable list/passage (results/passage views).
constexpr Rect TravelBody={Margin,Top+4,W-2*Margin,TravelBar.y-Top-8};
constexpr int TravelRowH=34,TravelRows=TravelBody.h/TravelRowH;
inline Rect travelRowRect(int row){return {TravelBody.x,TravelBody.y+row*TravelRowH,TravelBody.w,TravelRowH-4};}
constexpr int TravelLineH=13; // passage body text, matches CliLineH
}
