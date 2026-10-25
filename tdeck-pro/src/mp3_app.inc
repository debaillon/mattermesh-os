// MP3 player app (Settings > Apps > Music, section mm::MusicSection): browse /music/*.mp3 on the SD card and
// play one through services/music.*. Playback continues in the background if you leave this section; only the
// Stop button (or Backspace while the player view is showing) actually stops it.
int musicFocus=0,musicScroll=0;static std::vector<String> musicCache;static uint32_t musicCachedAt=0;
static std::vector<String> &musicTracks(){if(musicCache.empty()||uint32_t(millis()-musicCachedAt)>=4000){musicCache=device::musicList();musicCachedAt=millis();}return musicCache;}
void musicAppOpen(){musicFocus=musicScroll=0;musicCachedAt=0;}
void musicAppClose(){}
bool musicAppKey(char c){
 auto state=device::musicState();
 if(state.playing){
  if(c=='\n'){state.paused?device::musicResume():device::musicPause();repaintNow();return true;}
  if(c=='\b'){device::musicStop();repaintNow();return true;}
  if(c=='w'){device::musicVolume(state.volume+10);repaintNow();return true;}
  if(c=='s'){device::musicVolume(state.volume-10);repaintNow();return true;}
  return false;
 }
 auto &tracks=musicTracks();if(tracks.empty())return false;
 if(c=='j'||c=='k'){musicFocus=constrain(musicFocus+(c=='j'?1:-1),0,int(tracks.size())-1);
  if(musicFocus<musicScroll)musicScroll=musicFocus;else if(musicFocus>=musicScroll+mm::MusicRows)musicScroll=musicFocus-mm::MusicRows+1;
  repaintNow();return true;}
 if(c=='\n'){device::musicPlay(tracks[musicFocus]);repaintNow();return true;}
 return false;
}
bool musicAppTap(int x,int y){
 auto state=device::musicState();
 if(state.playing){
  for(int i=0;i<4;i++)if(mm::musicButtonRect(i).contains(x,y)){
   if(i==0)state.paused?device::musicResume():device::musicPause();
   else if(i==1)device::musicStop();
   else if(i==2)device::musicVolume(state.volume-10);
   else device::musicVolume(state.volume+10);
   repaintNow();return true;
  }
  return false;
 }
 auto &tracks=musicTracks();
 for(int row=0;row<mm::MusicRows&&musicScroll+row<int(tracks.size());row++)if(mm::musicRowRect(row).contains(x,y)){musicFocus=musicScroll+row;device::musicPlay(tracks[musicFocus]);repaintNow();return true;}
 return false;
}
void musicAppCompose(mm::State &v){
 auto state=device::musicState();
 v.musicPlaying=state.playing;v.musicPaused=state.paused;v.musicTrack=state.track.c_str();v.musicElapsedSec=state.elapsedSec;v.musicVolume=state.volume;v.musicMessage=state.message.c_str();
 if(!state.playing){auto &tracks=musicTracks();v.musicTracks.clear();for(int i=musicScroll;i<int(tracks.size())&&i<musicScroll+mm::MusicRows;i++)v.musicTracks.push_back(tracks[i].c_str());v.musicFocus=musicFocus-musicScroll;}
}
