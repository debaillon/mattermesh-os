#pragma once
#include <Arduino.h>
#include <vector>
// EPUB reader: plain-text, paginated, no images. An EPUB is a ZIP archive (see epub.cpp for the minimal
// reader) containing XHTML chapters listed by META-INF/container.xml -> the OPF package file's spine.
// One book is open at a time; chapters are decompressed and stripped to plain text on demand, one at a
// time, and handed to the UI layer (epub_app.inc) which does the actual word-wrap/pagination with
// Components::lines() since that needs font metrics this layer doesn't have.
namespace device {
void epubBegin(bool sdReady);
std::vector<String> epubList(); // SD /books/*.epub, sorted
bool epubOpenBook(const String &filename); // parses container.xml + OPF, loads chapter 0
void epubCloseBook();
bool epubGoToChapter(int index); // loads the given spine chapter's plain text
int epubChapterCount();
int epubChapterIndex();
const String &epubText(); // current chapter's plain text
String epubBookTitle();
String epubLastError();
}
