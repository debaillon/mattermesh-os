#pragma once
#include <string>
namespace device {
enum class RemapRow { Keep,Drop,Invalid };
// Rewrites the chat id of one /mattermesh/messages.tsv row after a channel catalog change.
inline RemapRow remapChatRow(std::string &row,const int *mapping,int count){
 bool deletion=row.rfind("D1\t",0)==0,read=row.rfind("V1\t",0)==0||row.rfind("I1\t",0)==0;
 if(!deletion&&!read&&row.rfind("R1\t",0)!=0&&row.rfind("R2\t",0)!=0)return RemapRow::Keep;
 int field=deletion||read?1:2;size_t a=0;
 for(int i=0;i<field;i++){auto end=row.find('\t',a);if(end==std::string::npos)return RemapRow::Invalid;a=end+1;}
 auto b=row.find('\t',a);if(b==std::string::npos||b==a)return RemapRow::Invalid;
 int id=0;for(size_t i=a;i<b;i++){if(row[i]<'0'||row[i]>'9'||id>=count)return RemapRow::Invalid;id=id*10+row[i]-'0';}
 if(id>=count)return RemapRow::Drop;if(mapping[id]<0)return RemapRow::Drop;
 row=row.substr(0,a)+std::to_string(mapping[id])+row.substr(b);
 return RemapRow::Keep;
}
}
