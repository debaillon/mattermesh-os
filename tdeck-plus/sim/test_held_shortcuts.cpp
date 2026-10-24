#include <cassert>
#include <iostream>
#include <string>
#include "services/held_shortcuts.h"
#include "services/first_repeater.h"
int main(){
 device::HeldShortcuts held;std::string typed;auto flush=[&](){held.flush([&](char c){typed+=c;});};
 assert(held.press(0,'p',100));held.append('a');flush();assert(typed.empty());
 assert(!held.tick(1100));assert(!held.release(0,1100));flush();assert(typed=="pa");
 assert(!held.press(0,'p',1110));assert(held.press(0,'p',1200));
 assert(held.press(1,'o',1250));held.append('x');flush();assert(typed=="pa");
 assert(!held.tick(3199));assert(held.tick(3200)==1);assert(held.tick(9000)==2);flush();assert(typed=="pax");
 assert(!held.release(0,9100));assert(!held.release(1,9100));assert(!held.tick(12000));
 // A captured release beyond the threshold must trigger even without UI polling.
 assert(held.press(2,'I',12100));assert(held.release(2,14100)==4);flush();assert(typed=="pax");
 assert(held.press(3,'u',14200));assert(!held.release(3,16199));flush();assert(typed=="paxu");
 held.reset();assert(held.press(0,'p',0xfffffff0));assert(held.tick(0xfffffff0u+2000u)==1);flush();assert(typed=="paxu");
 held.reset();assert(held.press(0,'p',100));for(int i=0;i<127;i++)assert(held.append('x'));assert(!held.append('x'));assert(!held.waiting()&&held.count==0);
 held.append(' ',4);held.append('q');flush();assert(typed=="paxu");held.resolve(4,false);flush();assert(typed=="paxu q");
 device::FirstRepeater rpt;rpt.sent(42,1);assert(!rpt.visible(1,0));assert(!rpt.observe(41,"abcd",100));assert(!rpt.observe(42,"",100));assert(rpt.observe(42,"abcd",100));assert(rpt.visible(1,6099)&&!rpt.visible(1,6100)&&!rpt.visible(0,100));assert(!rpt.observe(42,"1122",200)&&rpt.hash=="abcd");
 rpt.sent(43,2);assert(!rpt.visible(1,101)&&!rpt.observed);assert(rpt.observe(43,"ab",0xfffffff0));assert(rpt.visible(2,0xfffffff0u+5999u)&&!rpt.visible(2,0xfffffff0u+6000u));
 std::cout<<"PASS: brief/long presses, captured releases, simultaneous holds, letter rollover order, one toggle per hold, cancellation/overflow, time rollover; first observed repeater matched to latest send/chat, no invented route, exact 6s expiry\n";
}
