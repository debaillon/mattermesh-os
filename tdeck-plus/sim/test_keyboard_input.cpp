#include <cassert>
#include <iostream>
#include <string>
#include "services/keyboard_modifiers.h"
#include "services/held_shortcuts.h"
#include "services/accent_key.h"
int main(){using namespace device;
 for(char c:std::string("TyP9_X")){assert(keyboardCase(c,true,true)==c);assert(keyboardCase(c,false,true)==c);}
 assert(keyboardCase('W',true,false)=='w'&&keyboardCase('S',true,false)=='s');
 KeyboardModifiers m;m.update(31,true);assert(m.shift()&&!m.symbols());m.update(30,true);assert(m.alt&&m.symbols());m.update(30,false);m.update(31,false);assert(!m.shift()&&!m.symbols());
 m.update(35,true);assert(m.leftShift&&m.shift());m.update(35,false);assert(!m.shift());// both Shift keys type capitals (the accent key is set aside only in chats, main.cpp)
 assert(accented('e',"perch",false)=="\xc3\xa9"&&accented('e',"",false)=="\xc3\x88"&&accented('e',"lui ",false)=="\xc3\xa8"&&accented('a',"citt",false)=="\xc3\xa0"&&accented('O',"per",true)=="\xc3\x92"&&accented('x',"",false).empty());
 m.update(32,true);assert(m.symbols());m.update(32,false);assert(!m.symbols());
 HeldShortcuts held;std::string typed;auto flush=[&](){held.flush([&](char c){typed+=c;});};
 // Real debounced FIFO edges can share one acquisition time after a slow I/O
 // operation. Preserve every letter instead of treating the second as bounce.
 for(int i=0;i<4;i++)for(int repeat=0;repeat<2;repeat++){assert(held.press(i,"POIU"[i],100,true));assert(!held.release(i,100));flush();}
 assert(typed=="PPOOIIUU");
 held.reset();assert(held.press(0,'p',200));assert(!held.release(0,210));assert(!held.press(0,'p',220));flush();assert(typed=="PPOOIIUUp");
 held.reset();assert(held.press(0,'p',1000,true));assert(held.release(0,3000)==1);flush();assert(typed=="PPOOIIUUp");
 std::cout<<"PASS: case-sensitive password/CLI/name editors, case-insensitive menus, Alt/Sym/Shift, repeated debounced FIFO letters and software bounce, long hold once\n";
}
