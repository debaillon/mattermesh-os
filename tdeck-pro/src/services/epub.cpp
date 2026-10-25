#include "epub.h"
#include <SD.h>
#include <algorithm>
#include <vector>
// Real zlib inflate, vendored inside the PNGdec library we already depend on for map tiles (its src/ dir
// is on the global include path because map_tiles.cpp includes <PNGdec.h>). This copy is patched to use a
// caller-supplied buffer instead of malloc/free (see PNGdec.h's ucZLIB field and png.inl's DecodePNG) - we
// mirror that exact pattern below instead of adding a second compression library to the project.
#include "zlib.h"
#include "zutil.h"
#include "inftrees.h"
#include "inflate.h"
// Minimal ZIP reader: EPUB files are ZIP archives, read by filename lookup through the central directory
// (no full in-memory index - each lookup is a linear scan, fine for the handful of lookups per book: the
// container, the OPF, and one chapter at a time). Entries are DEFLATE (method 8) or stored (method 0).
namespace device {
struct ZipLoc{uint32_t offset=0,compSize=0,uncompSize=0;uint16_t method=0;};
static File zipFile;
static String opfDir,bookTitleStr,lastError;
static std::vector<String> chapterHrefs;
static String currentChapterText;
static int currentChapterIdx=-1;
static bool sdAvailable=false;

static bool findEOCD(File &f,uint32_t &cdOffset,uint16_t &numEntries){
 uint32_t fsize=f.size();
 uint32_t windows[3]={22u,512u,uint32_t(std::min<uint32_t>(fsize,65557u))};
 for(uint32_t window:windows){
  if(window>fsize||window<22)continue;
  uint32_t start=fsize-window;
  std::vector<uint8_t> buf(window);
  f.seek(start);if(f.read(buf.data(),window)!=window)continue;
  for(int i=int(window)-22;i>=0;i--){
   if(buf[i]==0x50&&buf[i+1]==0x4b&&buf[i+2]==0x05&&buf[i+3]==0x06){
    numEntries=buf[i+10]|(buf[i+11]<<8);
    cdOffset=uint32_t(buf[i+16])|(uint32_t(buf[i+17])<<8)|(uint32_t(buf[i+18])<<16)|(uint32_t(buf[i+19])<<24);
    return true;
   }
  }
 }
 return false;
}
static bool zipFind(File &zf,const String &name,ZipLoc &out){
 uint32_t cdOffset;uint16_t numEntries;
 if(!findEOCD(zf,cdOffset,numEntries))return false;
 zf.seek(cdOffset);
 for(uint16_t i=0;i<numEntries;i++){
  uint8_t h[46];if(zf.read(h,46)!=46)return false;
  if(!(h[0]==0x50&&h[1]==0x4b&&h[2]==0x01&&h[3]==0x02))return false;
  uint16_t method=h[10]|(h[11]<<8);
  uint32_t compSize=h[20]|(h[21]<<8)|(h[22]<<16)|(uint32_t(h[23])<<24);
  uint32_t uncompSize=h[24]|(h[25]<<8)|(h[26]<<16)|(uint32_t(h[27])<<24);
  uint16_t nameLen=h[28]|(h[29]<<8),extraLen=h[30]|(h[31]<<8),commentLen=h[32]|(h[33]<<8);
  uint32_t localOffset=h[42]|(h[43]<<8)|(h[44]<<16)|(uint32_t(h[45])<<24);
  String entryName;entryName.reserve(nameLen);
  for(uint16_t k=0;k<nameLen;k++){int ch=zf.read();entryName+=char(ch<0?0:ch);}
  if(entryName==name){out.offset=localOffset;out.compSize=compSize;out.uncompSize=uncompSize;out.method=method;return true;}
  zf.seek(zf.position()+extraLen+commentLen);
 }
 return false;
}
static bool inflateRaw(const uint8_t *src,uint32_t srcLen,uint8_t *dst,uint32_t dstLen){
 static uint8_t zlibBuf[32768+sizeof(struct inflate_state)]; // see the header comment above: no malloc/free
 z_stream strm;strm.zalloc=(alloc_func)0;strm.zfree=(free_func)0;strm.opaque=(voidpf)0;
 auto *state=(struct inflate_state FAR*)zlibBuf;
 strm.state=(struct internal_state FAR*)state;
 state->window=&zlibBuf[sizeof(struct inflate_state)];
 if(inflateInit2(&strm,-15)!=Z_OK)return false;
 strm.next_in=const_cast<uint8_t*>(src);strm.avail_in=srcLen;
 strm.next_out=dst;strm.avail_out=dstLen;
 int ret=inflate(&strm,Z_FINISH,0); // PNGdec's fork adds a check_crc arg; skip it, ZIP has its own CRC32
 inflateEnd(&strm);
 return ret==Z_STREAM_END;
}
static bool zipReadEntry(File &zf,const ZipLoc &loc,String &outText){
 zf.seek(loc.offset);
 uint8_t lh[30];if(zf.read(lh,30)!=30)return false;
 if(!(lh[0]==0x50&&lh[1]==0x4b&&lh[2]==0x03&&lh[3]==0x04))return false;
 uint16_t nameLen=lh[26]|(lh[27]<<8),extraLen=lh[28]|(lh[29]<<8);
 zf.seek(zf.position()+nameLen+extraLen);
 std::vector<uint8_t> comp(loc.compSize?loc.compSize:1);
 if(loc.compSize&&zf.read(comp.data(),loc.compSize)!=loc.compSize)return false;
 std::vector<uint8_t> raw(loc.uncompSize+1);
 if(loc.method==0){if(loc.uncompSize)memcpy(raw.data(),comp.data(),loc.uncompSize);}
 else if(loc.method==8){if(!inflateRaw(comp.data(),loc.compSize,raw.data(),loc.uncompSize))return false;}
 else return false;
 raw[loc.uncompSize]=0;
 outText=String(reinterpret_cast<char*>(raw.data()));
 return true;
}
// Tiny attribute/tag scanners: EPUBs are well-formed XML in practice, so substring search for name="..." (or
// name='...') is enough - no need for a real XML parser for the handful of attributes/elements we read.
static String attrOf(const String &tag,const String &name){
 String needle=name+"=\"";int a=tag.indexOf(needle);
 if(a<0){needle=name+"='";a=tag.indexOf(needle);if(a<0)return "";a+=needle.length();int b=tag.indexOf('\'',a);return b<0?String(""):tag.substring(a,b);}
 a+=needle.length();int b=tag.indexOf('"',a);return b<0?String(""):tag.substring(a,b);
}
static String dirOf(const String &path){int slash=path.lastIndexOf('/');return slash<0?String(""):path.substring(0,slash);}
static String joinPath(const String &dir,const String &rel){if(dir.isEmpty()||rel.startsWith("/"))return rel.startsWith("/")?rel.substring(1):rel;return dir+"/"+rel;}
static std::vector<std::pair<String,String>> parseManifest(const String &opf){
 std::vector<std::pair<String,String>> items;
 int manifestEnd=opf.indexOf("</manifest>");if(manifestEnd<0)return items;
 int pos=opf.indexOf("<manifest");if(pos<0)return items;
 while(true){
  int itemPos=opf.indexOf("<item",pos);if(itemPos<0||itemPos>=manifestEnd)break;
  int tagEnd=opf.indexOf(">",itemPos);if(tagEnd<0)break;
  String tag=opf.substring(itemPos,tagEnd+1);
  String id=attrOf(tag,"id"),href=attrOf(tag,"href");
  if(id.length()&&href.length())items.push_back({id,href});
  pos=tagEnd+1;
 }
 return items;
}
static std::vector<String> parseSpine(const String &opf,const std::vector<std::pair<String,String>> &manifest){
 std::vector<String> hrefs;
 int spineEnd=opf.indexOf("</spine>");if(spineEnd<0)return hrefs;
 int pos=opf.indexOf("<spine");if(pos<0)return hrefs;
 while(true){
  int refPos=opf.indexOf("<itemref",pos);if(refPos<0||refPos>=spineEnd)break;
  int tagEnd=opf.indexOf(">",refPos);if(tagEnd<0)break;
  String tag=opf.substring(refPos,tagEnd+1);
  String idref=attrOf(tag,"idref");
  for(auto &kv:manifest)if(kv.first==idref){hrefs.push_back(kv.second);break;}
  pos=tagEnd+1;
 }
 return hrefs;
}
// XHTML -> plain text: strip tags, turn block-level closes/<br> into newlines, decode the entities actually
// seen in EPUB prose, collapse whitespace. No images, no styling - matches the reader's plain-text scope.
static bool isBlockClose(String t){
 t.trim();if(!t.length()||t[0]!='/')return false;String name=t.substring(1);int sp=name.indexOf(' ');if(sp>0)name=name.substring(0,sp);name.toLowerCase();
 return name=="p"||name=="div"||name=="li"||name=="h1"||name=="h2"||name=="h3"||name=="h4"||name=="h5"||name=="h6"||name=="blockquote"||name=="tr";
}
static bool isSelfBreak(String t){t.trim();t.toLowerCase();return t=="br"||t=="br/"||t.startsWith("br ");}
static String htmlToText(const String &rawHtml){
 int bodyStart=rawHtml.indexOf("<body"); // drop <head> (its <title> is prose-looking text, not chapter content)
 String html=bodyStart<0?rawHtml:rawHtml.substring(bodyStart);
 String out;out.reserve(html.length());
 String tag;bool inTag=false,inSkip=false;
 for(size_t i=0;i<html.length();i++){
  char c=html[i];
  if(!inTag&&c=='<'){inTag=true;tag="";continue;}
  if(inTag){
   if(c=='>'){inTag=false;String lower=tag;lower.toLowerCase();
    if(lower.startsWith("script")||lower.startsWith("style"))inSkip=true;
    else if(lower=="/script"||lower=="/style")inSkip=false;
    if(isBlockClose(tag)||isSelfBreak(tag))out+='\n';
    continue;}
   tag+=c;continue;
  }
  if(inSkip)continue;
  out+=c;
 }
 out.replace("&amp;","&");out.replace("&lt;","<");out.replace("&gt;",">");out.replace("&quot;","\"");out.replace("&apos;","'");
 out.replace("&nbsp;"," ");out.replace("&#39;","'");out.replace("&#8217;","'");out.replace("&#8216;","'");out.replace("&#8220;","\"");
 out.replace("&#8221;","\"");out.replace("&#8212;","-");out.replace("&#8211;","-");out.replace("&mdash;","-");out.replace("&ndash;","-");
 String collapsed;collapsed.reserve(out.length());bool lastSpace=false;int newlineRun=0;
 for(size_t i=0;i<out.length();i++){char c=out[i];
  if(c=='\n'){newlineRun++;if(newlineRun<=2)collapsed+='\n';lastSpace=false;continue;}
  newlineRun=0;
  if(c==' '||c=='\t'||c=='\r'){if(!lastSpace){collapsed+=' ';lastSpace=true;}continue;}
  lastSpace=false;collapsed+=c;
 }
 collapsed.trim();return collapsed;
}
static bool endsWithEpub(const String &name){int n=name.length();return n>5&&name.substring(n-5).equalsIgnoreCase(".epub");}
std::vector<String> epubList(){
 std::vector<String> out;if(!sdAvailable)return out;
 File dir=SD.open("/books");if(!dir||!dir.isDirectory())return out;
 for(File f=dir.openNextFile();f;f=dir.openNextFile()){
  if(!f.isDirectory()){String name=String(f.name());int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);if(endsWithEpub(name))out.push_back(name);}
  f.close();
 }
 dir.close();std::sort(out.begin(),out.end(),[](const String &a,const String &b){return a.compareTo(b)<0;});
 return out;
}
void epubBegin(bool sdReady){sdAvailable=sdReady;}
void epubCloseBook(){if(zipFile)zipFile.close();chapterHrefs.clear();currentChapterIdx=-1;currentChapterText="";bookTitleStr="";opfDir="";}
bool epubGoToChapter(int index){
 if(index<0||index>=int(chapterHrefs.size())){lastError="No such chapter";return false;}
 ZipLoc loc;
 if(!zipFind(zipFile,chapterHrefs[index],loc)){lastError="Missing chapter in EPUB";return false;}
 String raw;
 if(!zipReadEntry(zipFile,loc,raw)){lastError="Could not decompress chapter";return false;}
 currentChapterText=htmlToText(raw);currentChapterIdx=index;lastError="";
 return true;
}
bool epubOpenBook(const String &filename){
 epubCloseBook();
 if(!sdAvailable){lastError="No SD card";return false;}
 zipFile=SD.open("/books/"+filename,FILE_READ);
 if(!zipFile){lastError="Could not open "+filename;return false;}
 ZipLoc containerLoc;
 if(!zipFind(zipFile,"META-INF/container.xml",containerLoc)){lastError="Not a valid EPUB";zipFile.close();return false;}
 String containerXml;
 if(!zipReadEntry(zipFile,containerLoc,containerXml)){lastError="Could not read container.xml";zipFile.close();return false;}
 int rfPos=containerXml.indexOf("<rootfile ");String opfPath; // not "<rootfile" alone: that also matches the <rootfiles> wrapper tag
 if(rfPos>=0){int tagEnd=containerXml.indexOf(">",rfPos);if(tagEnd>=0)opfPath=attrOf(containerXml.substring(rfPos,tagEnd+1),"full-path");}
 if(opfPath.isEmpty()){lastError="No OPF rootfile found";zipFile.close();return false;}
 ZipLoc opfLoc;
 if(!zipFind(zipFile,opfPath,opfLoc)){lastError="Could not find OPF file";zipFile.close();return false;}
 String opfXml;
 if(!zipReadEntry(zipFile,opfLoc,opfXml)){lastError="Could not read OPF file";zipFile.close();return false;}
 opfDir=dirOf(opfPath);
 auto manifest=parseManifest(opfXml);
 auto spineHrefs=parseSpine(opfXml,manifest);
 if(spineHrefs.empty()){lastError="EPUB has no readable chapters";zipFile.close();return false;}
 chapterHrefs.clear();for(auto &href:spineHrefs)chapterHrefs.push_back(joinPath(opfDir,href));
 int titleStart=opfXml.indexOf("<dc:title");
 if(titleStart>=0){int tagEnd=opfXml.indexOf(">",titleStart);int closeStart=tagEnd>=0?opfXml.indexOf("</dc:title>",tagEnd):-1;
  bookTitleStr=(tagEnd>=0&&closeStart>=0)?opfXml.substring(tagEnd+1,closeStart):filename;}
 else bookTitleStr=filename;
 return epubGoToChapter(0);
}
int epubChapterCount(){return int(chapterHrefs.size());}
int epubChapterIndex(){return currentChapterIdx;}
const String &epubText(){return currentChapterText;}
String epubBookTitle(){return bookTitleStr;}
String epubLastError(){return lastError;}
}
