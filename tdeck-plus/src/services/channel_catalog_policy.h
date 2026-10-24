#pragma once
#include <string>
#include <stdint.h>
namespace device {
enum class RemapRow { Keep,Drop,Invalid };
inline RemapRow remapChatRow(std::string &row,bool journal,const int *mapping,int count,const uint32_t *boundaries){
 bool deletion=journal&&row.rfind("D1\t",0)==0,read=journal&&(row.rfind("V1\t",0)==0||row.rfind("I1\t",0)==0);
 bool record=journal?(deletion||read||row.rfind("R1\t",0)==0||row.rfind("R2\t",0)==0):row.rfind("M1\t",0)==0;
 if(!record)return journal?RemapRow::Keep:RemapRow::Drop;
 int field=journal&&!deletion&&!read?2:1;size_t a=0;
 for(int i=0;i<field;i++){auto end=row.find('\t',a);if(end==std::string::npos)return RemapRow::Invalid;a=end+1;}
 auto b=row.find('\t',a);if(b==std::string::npos||b==a)return RemapRow::Invalid;
 int id=0;for(size_t i=a;i<b;i++){if(row[i]<'0'||row[i]>'9'||id>=count)return RemapRow::Invalid;id=id*10+row[i]-'0';}
 if(id>=count)return RemapRow::Drop;if(mapping[id]<0)return RemapRow::Drop;
 row=deletion?"D1\t"+std::to_string(mapping[id])+"\t"+std::to_string(boundaries[id])+"\tCLEAR":row.substr(0,a)+std::to_string(mapping[id])+row.substr(b);
 return RemapRow::Keep;
}
}
