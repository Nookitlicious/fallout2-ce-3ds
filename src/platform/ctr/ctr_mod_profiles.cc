#ifdef __3DS__
#include "ctr_mod_profiles.h"
#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include "color.h"
#include "art.h"
#include "db.h"
#include "obj_types.h"
#include <cstring>
#include "input.h"
#include "game.h"
#include "kb.h"
#include "mouse.h"
#include "palette.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"
#include "ctr_rectmap.h"
#include "ctr_input.h"
namespace fallout {
namespace {
const char* root = "sdmc:/3ds/fallout2";
const char* selection = "sdmc:/3ds/fallout2/mod-profile.txt";
std::string active = "vanilla";
const char* pending = "sdmc:/3ds/fallout2/mod-startup.pending";
const char* logPath = "sdmc:/3ds/fallout2/mod-startup.log";
std::string configPath = std::string(root)+"/fallout2.cfg";
bool validName(const std::string& n) {
    if (n.empty() || n.size() > 32 || n == "vanilla") return false;
    for (char c : n) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}
std::string path(const std::string& n) { return std::string(root) + "/mods/" + n; }
bool regular(const std::string& p) { struct stat s; return stat(p.c_str(), &s) == 0 && S_ISREG(s.st_mode); }
std::string profileConfig(const std::string& n) {
    const auto standard = path(n)+"/fallout2.cfg";
    return n == "nevada" && !regular(standard) ? path(n)+"/Nevada.cfg" : standard;
}
bool ready(const std::string& n) {
    if (n == "sonora") {
        return regular(path(n)+"/master.dat")
            && regular(path(n)+"/critter.dat")
            && regular(path(n)+"/fission.dat");
    }
    return validName(n) && (regular(path(n)+"/master.dat") || regular(std::string(root)+"/master.dat")) && (regular(path(n)+"/critter.dat") || regular(std::string(root)+"/critter.dat")) && regular(profileConfig(n));
}
std::vector<std::string> scan() {
    std::vector<std::string> v;
    std::string p = std::string(root)+"/mods";
    mkdir(p.c_str(), 0777);
    DIR* d = opendir(p.c_str());
    if (d) { while (auto* e = readdir(d)) { if (validName(e->d_name)) { struct stat st; if (stat(path(e->d_name).c_str(), &st)==0 && S_ISDIR(st.st_mode)) v.emplace_back(e->d_name); } } closedir(d); }
    std::sort(v.begin(), v.end());
    v.insert(v.begin(), "vanilla");
    return v;
}
bool save(const std::string& n) {
    std::string tmp = std::string(selection)+".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = fprintf(f, "%s\n", n.c_str()) > 0;
    if (fclose(f) != 0) ok = false;
    if (ok && rename(tmp.c_str(), selection) == 0) return true;
    // FAT implementations may reject replacement of an existing destination.
    std::string backup = std::string(selection)+".bak";
    if (ok && !regular(backup) && rename(selection, backup.c_str()) == 0) {
        if (rename(tmp.c_str(), selection) == 0) { remove(backup.c_str()); return true; }
        rename(backup.c_str(), selection);
    }
    remove(tmp.c_str());
    return false;
}
}
void ctrModProfileStartup() {
    active = "vanilla";
    if (regular(pending)) { remove(selection); remove("sdmc:/3ds/fallout2/mod-profile.txt.bak"); remove("sdmc:/3ds/fallout2/mod-profile.txt.tmp"); remove(pending); ctrModProfileStage("Recovered to vanilla after incomplete startup"); return; }
    FILE* f = fopen(selection, "rb");
    if (!f) return;
    char buf[64] = {};
    bool ok = fgets(buf, sizeof(buf), f) != nullptr;
    fclose(f);
    if (!ok) return;
    std::string n(buf); n.erase(n.find_last_not_of("\r\n") + 1);
    if (ready(n)) {
        FILE* marker = fopen(pending, "wb");
        if (!marker) return;
        bool written = fprintf(marker, "%s\n", n.c_str()) > 0;
        if (fclose(marker) != 0) written = false;
        if (!written || chdir(path(n).c_str()) != 0) { remove(pending); return; }
        active = n; configPath = profileConfig(n);
        ctrModProfileStage("Selected profile; starting engine");
    }
}
const char* ctrModProfileConfigPath() { return configPath.c_str(); }
bool ctrModProfileActive() { return active != "vanilla"; }
void ctrModProfileStage(const char* stage) {
    FILE* f = fopen(logPath, "a");
    if (f) { fprintf(f, "%s: %s\n", active.c_str(), stage); fclose(f); }
}
void ctrModProfileStartupComplete() {
    if (ctrModProfileActive()) { ctrModProfileStage("Main menu ready"); remove(pending); }
}
const char* ctrModProfileArchive(const char* name) {
    static std::string resolved;
    resolved = regular(path(active)+"/"+name) ? path(active)+"/"+name : std::string(root)+"/"+name;
    return resolved.c_str();
}
void ctrModProfileLoadArchives() {
    if (active == "sonora" || active == "ettu") {
        for (const char* file : {"fission.dat", "sfall.dat", "ce.dat"}) {
            if (regular(file)) {
                int rc = dbOpen(file, 0, nullptr, 1);
                ctrModProfileStage((std::string(rc == 0 ? "Opened " : "Failed to open ")+file).c_str());
            }
        }
    }
    if (!ctrModProfileActive()) return;
    if (active == "nevada") {
        for (const char* file : {"sfall.dat", "mods/sfall_us.dat", "mods/INVENTORYFILTER.DAT"}) {
            struct stat st;
            if (stat(file, &st) == 0 && (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode))) {
                int rc = dbOpen(file,0,nullptr,1);
                ctrModProfileStage((std::string(rc == 0 ? "Opened " : "Failed to open ")+file).c_str());
            }
        }
    }
    FILE* f=fopen("mods/mods_order.txt","rb");
    if (!f) { ctrModProfileStage("No mods_order.txt"); return; }
    char line[256]; unsigned count=0;
    while (fgets(line,sizeof(line),f) && count<64) {
        std::string n(line);
        auto first=n.find_first_not_of(" \t\r\n");
        if(first==std::string::npos) continue;
        n=n.substr(first,n.find_last_not_of(" \t\r\n")-first+1);
        if(n[0]=='#'||n[0]==';') continue;
        if(n.size()<5 || n.size()>120 || n.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- .")!=std::string::npos || n.find("..")!=std::string::npos) { ctrModProfileStage("Skipped unsupported archive order entry"); continue; }
        std::string ext=n.substr(n.size()-4); for(char& c:ext) if(c>='A'&&c<='Z') c+=32;
        if(ext!=".dat") { ctrModProfileStage("Skipped non-DAT archive order entry"); continue; }
        std::string file="mods/"+n;
        if (!regular(file)) ctrModProfileStage(("Archive listed but absent: "+file).c_str());
        else if (dbOpen(file.c_str(),0,nullptr,1)==0) { ++count; ctrModProfileStage(("Opened "+file).c_str()); }
        else ctrModProfileStage(("Archive exists but failed to open: "+file).c_str());
    }
    fclose(f);
}
int ctrModProfileMenu() {
    auto profiles = scan();
    size_t selected = 0;
    for (size_t i=0; i<profiles.size(); ++i) if (profiles[i]==active) selected=i;
    int oldFont = fontGetCurrent(), result = 0;
    auto oldMode=ctr_input.mode; ctr_input.mode=DISPLAY_MODE_ADAPT; setActiveRectMap(DISPLAY_MODS);
    colorPaletteLoad("color.pal"); paletteFadeTo(_cmap); mouseShowCursor();
    bool done=false, failed=false, restartRequired=false;
    ctr_input_set_menu_quit(true);
    u32 previous=ctr_input.frame.kHeld;
    while (!done) {
        int win=windowCreate(0,0,640,480,_colorTable[0],WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
        if (win<0) break;
        // Original save/load artwork, with opaque panels over the old labels.
        CacheEntry* artKey=nullptr; int w=0,h=0;
        auto* art=artLockFrameDataReturningSize(buildFid(OBJ_TYPE_INTERFACE,237,0,0,0),&artKey,&w,&h);
        if (art && w==640 && h==480) memcpy(windowGetBuffer(win),art,640*480);
        if (artKey) artUnlock(artKey);
        int green=_colorTable[21091], bright=_colorTable[32747], dark=_colorTable[0], metal=_colorTable[10570];
        auto panel=[&](int x,int y,int width,int height,int fill){ windowFill(win,x,y,width,height,metal); windowFill(win,x+2,y+2,width-4,height-4,fill); };
        auto label=[&](int x,int y,int width,const std::string& value,int color){ std::string text=value; while (!text.empty() && fontGetStringWidth(text.c_str())>width) text.pop_back(); windowDrawText(win,text.c_str(),width,x,y,color | 0x02000000); };
        fontSetCurrent(103);
        panel(128,8,384,224,dark);
        label(145,18,350,"MOD PROFILES",bright);
        label(145,55,350,"Active: "+active,green);
        const auto& n=profiles[selected];
        label(145,88,350,"Selected: "+n,bright);
        label(145,120,350,restartRequired?"Selection saved":n=="vanilla"?"Original game":ready(n)?"Ready to try":"Missing config or DAT files",green);
        label(145,152,350,restartRequired?"Press A or B to return":n=="vanilla"?"Original saves":"Separate profile saves",green);
        label(145,184,350,restartRequired?"Then EXIT and relaunch":failed?"Selection failed":"Activate, then relaunch",green);
        fontSetCurrent(101);
        panel(160,244,320,232,dark);
        size_t page=(selected/4)*4;
        bool buttonsOk=true;
        auto button=[&](int x,int y,int width,int height,int code,const std::string& text,bool highlight){
            panel(x,y,width,height,highlight?metal:dark);
            label(x+8,y+(height-fontGetLineHeight())/2,width-16,text,highlight?bright:green);
            if(buttonCreate(win,x,y,width,height,-1,-1,-1,code,nullptr,nullptr,nullptr,0)<0) buttonsOk=false;
        };
        for(size_t i=0;i<4 && page+i<profiles.size();++i) button(168,250+int(i)*40,304,36,2000+int(i),profiles[page+i]+(profiles[page+i]==active?"  [ACTIVE]":""),page+i==selected);
        button(168,414,64,24,2010,"PREV",false); button(238,414,64,24,2011,"NEXT",false);
        button(308,414,164,24,2014,"ACTIVATE",false); button(168,442,148,28,2012,"PREFS",false); button(324,442,148,28,2013,"BACK",false);
        windowRefresh(win);
        if(!buttonsOk){windowDestroy(win);break;}
        while(true){
            sharedFpsLimiter.mark(); int key=inputGetInput();
            u32 held=ctr_input.frame.kHeld, pressed=held & ~previous; previous=held;
            if (pressed & KEY_DUP) key=KEY_ARROW_UP; else if (pressed & KEY_DDOWN) key=KEY_ARROW_DOWN; else if (pressed & KEY_DLEFT) key=KEY_ARROW_LEFT; else if (pressed & KEY_DRIGHT) key=KEY_ARROW_RIGHT; else if (pressed & KEY_A) key=KEY_RETURN; else if (pressed & KEY_B) key=KEY_ESCAPE;
            if(_game_user_wants_to_quit==3){ctrModProfileStage("System requested exit from mod menu");result=2;done=true;break;}
            renderPresent(); sharedFpsLimiter.throttle();
            if(restartRequired) {
                if(key==KEY_RETURN || key==KEY_ESCAPE || key==2013 || key==2014) { done=true; break; }
                continue;
            }
            if(key==KEY_ESCAPE || key==2013){done=true;break;}
            if(key==2012){result=1;done=true;break;}
            if(key==KEY_ARROW_UP){if(selected>0)--selected;break;}
            if(key==KEY_ARROW_DOWN){if(selected+1<profiles.size())++selected;break;}
            if(key==2010 || key==KEY_ARROW_LEFT){selected=page>=4?page-4:0;break;}
            if(key==2011 || key==KEY_ARROW_RIGHT){if(page+4<profiles.size())selected=page+4;break;}
            if(key>=2000 && key<2004 && page+size_t(key-2000)<profiles.size()){selected=page+size_t(key-2000);failed=false;break;}
            if(key==2014 || key==KEY_RETURN){if(n==active){done=true;break;} ctrModProfileStage(("Activation requested: "+n).c_str()); if((n=="vanilla"||ready(n))&&save(n)){ctrModProfileStage(("Selection saved; return to main menu and EXIT: "+n).c_str());restartRequired=true;}else {failed=true;ctrModProfileStage("Selection save failed");}break;}
        }
        windowDestroy(win);
    }
    ctr_input_set_menu_quit(false);
    fontSetCurrent(oldFont); mouseHideCursor(); ctr_input.mode=oldMode; setActiveRectMap(DISPLAY_MAIN); return result;
}
}
#endif
