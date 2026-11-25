#include <cassert>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include "sfall_ini.h"
#include "memory.h"
using namespace fallout;
namespace fallout { int debugPrint(const char*, ...) { return 0; } unsigned int getTicks() { return 0; } }
static void write(const char* path, const char* content) { FILE* f=fopen(path,"wb"); assert(f); assert(fputs(content,f)>=0); assert(fclose(f)==0); }
static ProgramValue str(Program* p,const char* s) { ProgramValue v; v.opcode=VALUE_TYPE_DYNAMIC_STRING; std::string copy(s); v.integerValue=programPushString(p,copy.data()); return v; }
static std::string text(Program* p,ProgramValue v) { assert(v.isString()); return programGetString(p,v.opcode,v.integerValue); }
int main() {
 Program p{}; assert(sfallArraysInit()); mkdir("mods",0700);
 write("mods/Test.ini","[Ammo]\nPid=123\nzero=0\nname=Energy Cell\n[Other]\nkey=abc\n");
 write("fallback.ini","[Root]\nvalue=ok\n"); write("ddraw.ini","[Main]\nvalue=root\n"); write("mods/ddraw.ini","[Main]\nvalue=wrong\n");
 sfall_ini_set_base_path("mods\\");
 ArrayId a=sfall_ini_get_section(&p,"Test.ini","ammo"); assert(LenArray(a)==3); assert(GetArrayKey(a,-1,&p).asInt()==1);
 assert(text(&p,GetArray(a,str(&p,"Pid"),&p))=="123"); assert(text(&p,GetArray(a,str(&p,"zero"),&p))=="0"); assert(text(&p,GetArray(a,str(&p,"name"),&p))=="Energy Cell"); assert(GetArrayKey(a,LenArray(a),&p).asInt()==0);
 ArrayId names=sfall_ini_get_sections(&p,"Test.ini"); assert(LenArray(names)==2); assert(text(&p,GetArray(names,ProgramValue(0),&p))=="Ammo"); assert(text(&p,GetArray(names,ProgramValue(1),&p))=="Other");
 assert(LenArray(sfall_ini_get_section(&p,"missing.ini","Ammo"))==0); assert(LenArray(sfall_ini_get_section(&p,"Test.ini","NoSection"))==0); assert(LenArray(sfall_ini_get_sections(&p,"missing.ini"))==0); assert(LenArray(sfall_ini_get_sections(&p,"../fallback.ini"))==0); assert(LenArray(sfall_ini_get_sections(&p,"sdmc:/fallback.ini"))==0); assert(LenArray(sfall_ini_get_section(&p,"fallback.ini","Root"))==1);
 ArrayId system=sfall_ini_get_section(&p,"ddraw.ini","Main"); assert(text(&p,GetArray(system,str(&p,"value"),&p))=="root"); sfall_ini_set_base_path(nullptr); assert(LenArray(sfall_ini_get_sections(&p,"mods\\Test.ini"))==2);
 char output[16]; std::string maxFile(63,'x'),maxSection(32,'s'); assert(sfall_ini_get_string((maxFile+"|"+maxSection+"|key").c_str(),output,sizeof(output))); assert(output[0]=='\0'); assert(!sfall_ini_get_string((maxFile+"x|"+maxSection+"|key").c_str(),output,sizeof(output)));
 DeleteAllTempArrays(); assert(LenArray(a)==-1); sfallArraysExit(); free(p.dynamicStrings); puts("PASS Sfall INI maps/lists, path lookup, missing files, bounds, temporary cleanup, maximum triplet lengths");
}
