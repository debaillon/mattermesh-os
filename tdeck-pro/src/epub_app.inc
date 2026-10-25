// EPUB reader app (Settings > Apps > Books, section mm::BooksSection): browse /books/*.epub on the SD
// card, open one, and read it as paginated plain text. Pagination is page-of-chapter: the whole current
// chapter is word-wrapped once (mm::Components::lines(), since that needs font metrics this file doesn't
// have on its own - see main.cpp's scrollMessagePath for the same mm::Components(frame) trick) and sliced
// into screen-sized pages; crossing a chapter boundary loads the next/previous chapter and re-wraps it.
int booksFocus=0,booksScroll=0,bookCurrentPage=0;
static std::vector<String> booksCache;static uint32_t booksCachedAt=0;
static std::vector<std::string> bookPageLines;static int bookPageCount=1;static int bookPaginatedFor=-2;
static std::vector<String> &booksList(){if(booksCache.empty()||uint32_t(millis()-booksCachedAt)>=4000){booksCache=device::epubList();booksCachedAt=millis();}return booksCache;}
static int bookLinesPerPage(){mm::Components c(frame);return std::max(1,(mm::EpubTextBottom-mm::EpubTextY)/c.lineHeight(mm::Font::ChatBody));}
static void repaginateChapter(){
 mm::Components c(frame);
 bookPageLines=c.lines(std::string(device::epubText().c_str()),mm::W-2*mm::Margin,mm::Font::ChatBody);
 bookPageCount=std::max(1,(int(bookPageLines.size())+bookLinesPerPage()-1)/bookLinesPerPage());
 bookPaginatedFor=device::epubChapterIndex();
}
// A chapter can be opened from outside this app's own key/tap handlers (e.g. CLI diagnostics calling
// device::epubOpenBook/epubGoToChapter directly) and leave bookPageLines stale for the now-current chapter;
// compose() calls this first so the page shown always matches what's actually loaded.
static void ensurePaginated(){if(bookPaginatedFor!=device::epubChapterIndex()){repaginateChapter();bookCurrentPage=0;}}
// True once a chapter's wrapped lines amount to real prose, not just a handful of stray characters - EPUBs
// commonly open with a cover.xhtml or title page that is mostly/only an <img>, which this reader skips
// since it has no images to show; landing a reader on that blank page looks broken.
static bool chapterHasText(){size_t chars=0;for(auto &l:bookPageLines)for(char c:l)if(c!=' ')chars++;return chars>=20;}
void booksAppOpen(){booksFocus=booksScroll=0;booksCachedAt=0;}
void booksAppClose(){}
bool booksAppKey(char c){
 if(device::epubChapterIndex()>=0){
  if(c=='\b'){device::epubCloseBook();repaintNow();return true;}
  if(c=='l'){if(bookCurrentPage+1<bookPageCount)bookCurrentPage++;else if(device::epubChapterIndex()+1<device::epubChapterCount()&&device::epubGoToChapter(device::epubChapterIndex()+1)){repaginateChapter();bookCurrentPage=0;}repaintNow();return true;}
  if(c=='h'){if(bookCurrentPage>0)bookCurrentPage--;else if(device::epubChapterIndex()>0&&device::epubGoToChapter(device::epubChapterIndex()-1)){repaginateChapter();bookCurrentPage=bookPageCount-1;}repaintNow();return true;}
  return false;
 }
 auto &books=booksList();if(books.empty())return false;
 if(c=='j'||c=='k'){booksFocus=constrain(booksFocus+(c=='j'?1:-1),0,int(books.size())-1);
  if(booksFocus<booksScroll)booksScroll=booksFocus;else if(booksFocus>=booksScroll+mm::BooksRows)booksScroll=booksFocus-mm::BooksRows+1;
  repaintNow();return true;}
 if(c=='\n'){if(device::epubOpenBook(books[booksFocus])){repaginateChapter();bookCurrentPage=0;
  for(int guard=0;!chapterHasText()&&device::epubChapterIndex()+1<device::epubChapterCount()&&guard<8;guard++){if(!device::epubGoToChapter(device::epubChapterIndex()+1))break;repaginateChapter();}
  }repaintNow();return true;}
 return false;
}
bool booksAppTap(int x,int y){
 if(device::epubChapterIndex()>=0){
  if(mm::EpubPrevTap.contains(x,y))return booksAppKey('h');
  if(mm::EpubNextTap.contains(x,y))return booksAppKey('l');
  return booksAppKey('\b'); // middle tap: close the book, back to the list
 }
 auto &books=booksList();
 for(int row=0;row<mm::BooksRows&&booksScroll+row<int(books.size());row++)if(mm::booksRowRect(row).contains(x,y)){booksFocus=booksScroll+row;return booksAppKey('\n');}
 return false;
}
void booksAppCompose(mm::State &v){
 if(device::epubChapterIndex()>=0){
  ensurePaginated();
  v.epubOpen=true;v.epubTitle=device::epubBookTitle().c_str();v.epubMessage=device::epubLastError().c_str();
  v.epubPageLines.clear();int first=bookCurrentPage*bookLinesPerPage();
  for(int i=first;i<int(bookPageLines.size())&&i<first+bookLinesPerPage();i++)v.epubPageLines.push_back(bookPageLines[i]);
 }else{
  v.epubOpen=false;auto &books=booksList();v.epubBooks.clear();
  for(int i=booksScroll;i<int(books.size())&&i<booksScroll+mm::BooksRows;i++)v.epubBooks.push_back(books[i].c_str());
  v.epubFocus=booksFocus-booksScroll;v.epubMessage=device::epubLastError().c_str();
 }
}
