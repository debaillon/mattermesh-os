// Travel app (section 27), part of the OS. Included by renderer.cpp.
void Renderer::travelApp(const State &s){
 shell(s,"Travel",true,Icon::Menu);g.fillRect(ComposeButton.x,StatusH,ComposeButton.w,HeaderH-1,BG);
 if(s.travelView==0){
  auto lines=c.lines(s.travelStatus,TravelBody.w,Font::Body);
  int y=TravelBody.y;for(auto &line:lines){c.text(TravelBody.x,y,line,Font::Body);y+=Components::lineHeight(Font::Body);}
  if(!s.travelAttribution.empty())c.text(TravelBody.x,TravelBody.y+TravelBody.h-12,"Data licensed - see ATTRIBUTION.txt on the SD card",Font::Small,FG,TravelBody.w);
 }else if(s.travelView==1){
  int visible=TravelRows,maxScroll=std::max(0,int(s.travelHits.size())-visible),offset=std::min(s.travelOffset,maxScroll);
  for(int row=0;row<visible&&offset+row<int(s.travelHits.size());row++){auto &hit=s.travelHits[offset+row];c.menuRow(travelRowRect(row),Icon::Search,hit.title,hit.detail,s.travelFocus==offset+row);}
  if(s.travelHits.empty())c.text(TravelBody.x,TravelBody.y,s.notice.empty()?"No results":s.notice,Font::Body,FG,TravelBody.w);
  if(maxScroll){int height=visible*TravelRowH,thumb=std::max(10,height*visible/int(s.travelHits.size()));g.drawFastVLine(W-3,TravelBody.y,height,FG);g.fillRect(W-4,TravelBody.y+height*offset/int(s.travelHits.size()),3,thumb,FG);}
 }else{
  c.text(TravelBody.x,TravelBody.y,s.travelPassageTitle,Font::Bold,FG,TravelBody.w);
  int titleLines=int(c.lines(s.travelPassageTitle,TravelBody.w,Font::Bold).size()),bodyY=TravelBody.y+titleLines*Components::lineHeight(Font::Bold)+4;
  int bodyH=TravelBody.y+TravelBody.h-bodyY;
  std::vector<std::string> wrapped;for(auto &para:c.lines(s.travelPassageText,TravelBody.w,Font::ChatMeta))wrapped.push_back(para);
  int visible=bodyH/TravelLineH,maxScroll=std::max(0,int(wrapped.size())-visible),scroll=std::min(s.travelScroll,maxScroll);
  for(int i=0;i<visible&&scroll+i<int(wrapped.size());i++)c.text(TravelBody.x,bodyY+i*TravelLineH,wrapped[scroll+i],Font::ChatMeta,FG,TravelBody.w);
  if(maxScroll){int height=visible*TravelLineH,thumb=std::max(10,height*visible/int(wrapped.size()));g.drawFastVLine(W-3,bodyY,height,FG);g.fillRect(W-4,bodyY+height*scroll/int(wrapped.size()),3,thumb,FG);}
 }
 c.card(TravelBar,false,8);
 int textY=TravelInput.y+(TravelInput.h-13)/2; // matches Components::button()'s vertical centering
 if(s.travelView==0){
  int maxw=TravelInput.w-6;
  std::string shown=c.width(s.travelInput,Font::ChatMeta)>maxw?c.inputTail(s.travelInput,maxw,Font::ChatMeta):s.travelInput;
  c.text(TravelInput.x,textY,shown.empty()?"Ask a question...":shown,Font::ChatMeta);
  int cx=TravelInput.x+(shown.empty()?0:c.width(shown,Font::ChatMeta));g.fillRect(cx,textY-2,2,TravelLineH-1,FG);
 }else{
  std::string shown=c.fit(s.travelInput.empty()?"(no query)":s.travelInput,TravelInput.w-6,Font::ChatMeta);
  c.text(TravelInput.x,textY,shown,Font::ChatMeta);
 }
 c.button(TravelSearch,"SEARCH");c.button(TravelNear,"NEAR");
}
