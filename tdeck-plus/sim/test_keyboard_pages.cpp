#include <cassert>
#include <iostream>
#include "ui/renderer.h"
#include "ui/fast_canvas.h"
#include "services/keyboard_layout.h"
// Keyboard languages and Accent key pages, accent bar: every language and accent mode fits without being cut.
int main(){mm::FastCanvas16 frame(mm::W,mm::H);mm::Components c(frame);mm::State s;
 const int lh=c.lineHeight(mm::Font::ChatMeta),tw=mm::W-2*mm::Margin-4;
 const int keyboardRows=(mm::H-2-(mm::KeyboardHelpY+lh))/lh;
 int introLines=int(c.lines(mm::AccentKeyHowTo,tw,mm::Font::ChatBody).size());
 const int accentRows=(mm::H-2-(mm::AccentInfoY+18+introLines*c.lineHeight(mm::Font::ChatBody)+3+lh+1))/lh;
 for(int lang=0;lang<mm_kbd::LangCount;lang++){
  for(uint32_t mask:{1u,1u|(1u<<lang)})for(int mode=0;mode<3;mode++){s.kbdMask=mask;s.kbdLang=lang;s.accentMode=mode;
   assert(c.width(mm::keyboardHowto(s),mm::Font::ChatMeta)<=tw);
   assert(int(c.lines(mm::keyboardHelp(s,lang),tw,mm::Font::ChatMeta).size())<=keyboardRows);}
  assert(int(c.lines(mm_kbd::accentHelp(mm_kbd::Lang(lang)),tw,mm::Font::ChatMeta).size())<=accentRows);
  // composer bar (ChatTitle) and the Quick Reply title line (ChatMeta beside "Quick Reply")
  const int quick=mm::QuickReplyPanel.w-68-c.width("Quick Reply  ",mm::Font::ChatTitle);
  for(int taps=0;taps<=4;taps++){s.kbdLang=lang;s.accentTaps=taps;assert(c.width(mm::accentBarText(s),mm::Font::ChatTitle)<=mm::W-2*mm::Margin-8);assert(c.width(mm::accentBarText(s),mm::Font::ChatMeta)<=quick);}
 }
 using mm_kbd::Lang;
 assert(mm_kbd::accentBar(Lang::Italian,1,"  ")=="\xc3\xa0  \xc3\xa8  \xc3\xac  \xc3\xb2  \xc3\xb9");// à è ì ò ù
 assert(mm_kbd::accentBar(Lang::Italian,2,"  ")=="\xc3\xa9");// two taps: é
 assert(mm_kbd::accentBar(Lang::Russian,1,"  ")=="\xd0\xb6  \xd0\xb1  \xd1\x85  \xd1\x91");// ж б х ё
 assert(mm_kbd::accentBar(Lang::Russian,0,"  ")==mm_kbd::accentBar(Lang::Russian,1,"  "));// held = one tap
 assert(mm_kbd::accentBar(Lang::English,1,"  ").empty());s.kbdLang=int(Lang::English);s.accentTaps=1;assert(mm::accentBarText(s).empty());// no bar
 assert(mm_kbd::firstRow(0,8,3)==0&&mm_kbd::firstRow(5,8,3)==3&&mm_kbd::firstRow(9,8,3)==5&&mm_kbd::firstRow(2,0,3)==0);
 assert(mm_kbd::rows("").size()==size_t(mm_kbd::LangCount)&&mm_kbd::rows("ukr").size()==1);
 std::cout<<"PASS: Keyboard languages / Accent key help fits for every language and accent mode; accent bar per language and taps (composer and Quick Reply)\n";
}
