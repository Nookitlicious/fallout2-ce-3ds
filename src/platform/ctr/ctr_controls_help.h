#ifndef FALLOUT_CTR_CONTROLS_HELP_H
#define FALLOUT_CTR_CONTROLS_HELP_H

#include <algorithm>
#include <array>
#include <string>
#include <cstring>
#include "art.h"
#include "color.h"
#include "game.h"
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "obj_types.h"
#include "svga.h"
#include "skill.h"
#include "stat.h"
#include "perk.h"
#include "text_font.h"
#include "touch.h"
#include "window_manager.h"
#include "ctr_gfx.h"
#include "ctr_input.h"
#include "ctr_rectmap.h"

namespace fallout {
namespace ctr_help {
struct Page { const char* title; const char* button; std::array<const char*, 8> lines; };
inline constexpr Page pages[] = {
    {"GENERAL GAMEPLAY", "GENERAL", {"Circle Pad: Move cursor", "D-pad: Pan camera", "R: Interact / left-click", "L: Change cursor / right-click", "A: Switch screen focus", "SELECT: Change screen layout", "SELECT + Up/Down: Zoom", "3D slider: Zoom; Touch: UI"}},
    {"COMBAT & MOVEMENT", "COMBAT", {"L: Change cursor mode", "Point at a tile and press R.", "Movement uses AP in combat.", "Tap the weapon to attack.", "Select a target with R.", "Check AP before an action.", "Tap END TURN when finished.", "Equip weapons in inventory."}},
    {"WORLD MAP", "WORLD MAP", {"D-pad: Pan map", "Circle Pad: Move cursor", "A / R: Select destination", "X: Enter known location", "Y: Switch town / world map", "SELECT: Swap screens", "Entrances: D-pad select/page", "A confirms; B / X returns"}},
    {"INVENTORY", "INVENTORY", {"Touch INV to open inventory.", "Touch to select items.", "Drag items into equip slots.", "Check item weight / quantity.", "L: Change cursor mode", "Use equipped items in game.", "Check your carry weight.", "Tap DONE to close inventory."}},
    {"DIALOGUE", "DIALOGUE", {"Talk with interaction cursor.", "Touch a response to choose it.", "Read the NPC reply on top.", "Tap REVIEW for dialogue log.", "D-pad Up/Down: Scroll review", "B: Close dialogue review", "Tap BARTER when available.", "Use speech to avoid some fights."}},
    {"CHARACTER & PIP-BOY", "CHARACTER", {"Touch CHAR for character stats.", "D-pad Up/Down: Scroll", "R / L: Left / right-click", "A / B: Confirm / close dialogs", "Touch PIP to open Pip-Boy.", "Circle Pad / D-pad: Cursor", "A / R: Select Pip-Boy entry", "Touch the displayed buttons."}},
    {"BEGINNER TIPS", "BEGINNER TIPS", {"Talk to people for quests.", "Save often in multiple slots.", "L: Switch cursor modes", "Use the skill menu for actions.", "Pip-Boy shows quest progress.", "Keep enough AP to move.", "You can avoid some fights.", "Options > Controls: This guide"}},
    {"MODS & INSTALL", "MODS & INSTALL", {"Mods are downloaded separately.", "Use the GitHub install guide.", "Et Tu / Nevada / Sonora: games", "UPU: Fallout 2 enhancements", "Install each in its own profile.", "Select mod, then relaunch.", "Profiles keep separate saves.", "Some PC mods are incompatible."}},
    {"NEW 3DS CONTROLS", "NEW 3DS", {"", "", "", "", "A still switches focus.", "Circle Pad still moves cursor.", "D-pad overrides C-stick.", "Settings: ddraw.ini"}},
};
inline constexpr int kPageCount = sizeof(pages)/sizeof(pages[0]);
inline constexpr int kTopX = 120, kBottomX = 160, kBottomY = 240;
struct Hit { int x,y,w,h; };
inline constexpr Hit choiceRect(int index, int count) {
    if (index == count) return {count == 9 ? 164 : 88, 180, 144, 30};
    return {12 + (index % 2)*152, 36 + (index/2)*36, 144, 30};
}
inline int touchChoice(int x, int y, int count) {
    for (int i=0;i<=count;++i) {
        const auto r=choiceRect(i,count);
        if(x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h) return i;
    }
    return -1;
}
inline int cardId(int page) {
    // Same original skilldex artwork used by the character editor.
    switch(page) {
        case 0: return statGetFrmId(STAT_AGILITY);
        case 1: return skillGetFrmId(SKILL_SMALL_GUNS);
        case 2: return skillGetFrmId(SKILL_OUTDOORSMAN);
        case 3: return perkGetFrmId(PERK_PACK_RAT);
        case 4: return skillGetFrmId(SKILL_SPEECH);
        case 5: return statGetFrmId(STAT_STRENGTH);
        case 6: return statGetFrmId(STAT_INTELLIGENCE);
        case 7: return skillGetFrmId(SKILL_REPAIR);
        case 8: return skillGetFrmId(SKILL_SCIENCE);
        default: return -1;
    }
}
inline void label(int win, int x, int y, int width, int maxHeight, const char* value, int color, bool centered=false) {
    const int old = fontGetCurrent();
    for (int font : {101, 103, 100, 0}) {
        fontSetCurrent(font);
        if (fontGetLineHeight() > 0 && fontGetLineHeight() <= maxHeight && fontGetStringWidth(value) <= width) break;
    }
    if (fontGetLineHeight() > 0 && fontGetLineHeight() <= maxHeight && fontGetStringWidth(value) <= width) {
        if (centered) x += (width-fontGetStringWidth(value))/2;
        windowDrawText(win,value,fontGetStringWidth(value),x,y,color | 0x02000000);
    }
    fontSetCurrent(old);
}
// Map the genuine Pip-Boy screen and bezel together. Keep the outer
// edges intact and compress only the center; never tile the curved bezel.
inline int insetAxis(int pos,int dest,int src,int first,int last) {
    if(pos<first) return pos;
    if(pos>=dest-last) return src-(dest-pos);
    return first+(pos-first)*(src-first-last)/(dest-first-last);
}
inline bool validImage(const FrmImage& image) {
    return image.isLocked() && image.getWidth()>0 && image.getHeight()>0;
}
inline void pipScreen(int win,const FrmImage& pip,int x,int y,int w) {
    // Original pipboy.cc defines the CRT content at (254,46), 374x410.
    // Crop (240,20)..(640,480), preserving 14/12px horizontal and
    // 26/24px vertical borders. Content then occupies y=26..215.
    windowFill(win,x,y,w,240,_colorTable[10570]);
    if(!validImage(pip) || pip.getWidth()!=640 || pip.getHeight()!=480) {
        windowFill(win,x+14,y+26,w-26,190,_colorTable[0]);
        return;
    }
    auto* dst=windowGetBuffer(win); const auto* src=pip.getData();
    for(int j=0;j<240;++j) for(int i=0;i<w;++i)
        dst[(y+j)*640+x+i]=src[(20+insetAxis(j,240,460,26,24))*640+240+insetAxis(i,w,400,14,12)];
}
// A physical control panel, not a second CRT. The original Options
// casing is bare metal; retain its edges while resampling the interior.
inline void controlPanel(int win,const FrmImage& panel) {
    windowFill(win,kBottomX,kBottomY,320,240,_colorTable[10570]);
    if(!validImage(panel)) return;
    const int sw=panel.getWidth(),sh=panel.getHeight();
    const int cx=std::min(12,sw/3),cy=std::min(12,sh/3);
    auto* dst=windowGetBuffer(win); const auto* src=panel.getData();
    for(int j=0;j<240;++j) for(int i=0;i<320;++i) {
        const int sx=insetAxis(i,320,sw,cx,cx);
        const int sy=insetAxis(j,240,sh,cy,cy);
        dst[(kBottomY+j)*640+kBottomX+i]=src[sy*sw+sx];
    }
}
inline void buttonArt(int win,const FrmImage& image,int x,int y,int w,int h,bool selected) {
    if(!validImage(image)) {
        windowFill(win,x,y,w,h,_colorTable[10570]);
        windowFill(win,x+1,y+1,w-2,h-2,_colorTable[0]);
    } else {
        auto* dst=windowGetBuffer(win);
        // Exact original button pixels, resampled just to the final touch size.
        for(int j=0;j<h;++j) for(int i=0;i<w;++i) {
            const auto px=image.getData()[(j*image.getHeight()/h)*image.getWidth()+i*image.getWidth()/w];
            if(px!=0) dst[(y+j)*640+x+i]=px;
        }
    }
    if(selected) {
        const int green=_colorTable[992];
        windowFill(win,x,y,w,1,green); windowFill(win,x,y+h-1,w,1,green);
        windowFill(win,x,y,1,h,green); windowFill(win,x+w-1,y,1,h,green);
    }
}
inline bool drawCard(int win,int page,int x,int y,int w,int h) {
    FrmImage card;
    const int id=cardId(page);
    if(id<0 || !card.lock(buildFid(OBJ_TYPE_SKILLDEX,id,0,0,0)) || !validImage(card)) return false;
    const int sw=card.getWidth(),sh=card.getHeight();
    // Original card art is ink on pale paper. Recolor only its dark ink to
    // terminal green, retaining the original drawing and its aspect ratio.
    auto ink=[](unsigned char px){ const int rgb=Color2RGB(px); return std::max({(rgb>>10)&31,(rgb>>5)&31,rgb&31})<=9; };
    int l=sw,t=sh,r=-1,b=-1;
    for(int j=0;j<sh;++j) for(int i=0;i<sw;++i) if(ink(card.getData()[j*sw+i])) {l=std::min(l,i);r=std::max(r,i);t=std::min(t,j);b=std::max(b,j);}
    if(r<l || b<t) return false;
    const int cw=r-l+1,ch=b-t+1;
    const int dw=std::min(w,std::max(1,h*cw/ch)),dh=std::min(h,std::max(1,w*ch/cw));
    auto* dst=windowGetBuffer(win); const int green=_colorTable[992];
    for(int j=0;j<dh;++j) for(int i=0;i<dw;++i)
        if(ink(card.getData()[(t+j*ch/dh)*sw+l+i*cw/dw])) dst[(y+(h-dh)/2+j)*640+x+(w-dw)/2+i]=green;
    return true;
}
inline void draw(int win, const FrmImage& pip, const FrmImage& panel, const FrmImage& up, const FrmImage& down,
    const FrmImage& lampUp, const FrmImage& lampDown, int selected, int opened, int count, bool shoulder, bool stick) {
    const int black=_colorTable[0], green=_colorTable[992], bright=_colorTable[32747];
    windowFill(win,0,0,640,480,black);
    pipScreen(win,pip,kTopX,0,400);
    // Draw directly on the original CRT, with a safe inset for its curves.
    // No opaque rectangle may overlap the bezel.
    label(win,kTopX+30,31,340,15,"PIP-BOY 2000 / FIELD MANUAL",bright,true);
    windowFill(win,kTopX+30,48,340,1,green);
    label(win,kTopX+30,52,340,15,pages[opened].title,green,true);
    const bool hasCard=drawCard(win,opened,kTopX+279,79,89,112);
    for(int i=0;i<8;++i) {
        const char* line=pages[opened].lines[i];
        if(opened==kPageCount-1) {
            if(i==0) line=shoulder?"ZL / ZR: Zoom out / in":"ZL: Switch interface focus";
            if(i==1) line=shoulder?"Shoulder zoom: ENABLED":"Shoulder zoom: DISABLED";
            if(i==2) line=stick?"C-stick: Pan gameplay / map":"C-stick: Move cursor / scroll";
            if(i==3) line=stick?"C-stick panning: ENABLED":"C-stick panning: DISABLED";
            if(i==6 && !stick) line="SELECT + Up/Down: Zoom";
        }
        label(win,kTopX+30,70+i*17,hasCard?244:340,16,line,green);
    }
    windowFill(win,kTopX+30,207,340,1,green);
    // Physical buttons on metal casing, with the original Pip-Boy red
    // switches. No CRT artwork or green display on the touchscreen.
    controlPanel(win,panel);
    buttonArt(win,up,kBottomX+50,kBottomY+6,220,24,false);
    label(win,kBottomX+55,kBottomY+10,210,16,"FIELD MANUAL / CONTROLS",_colorTable[18979],true);
    for(int i=0;i<=count;++i) {
        const auto r=choiceRect(i,count); const bool sel=i==selected;
        buttonArt(win,sel && validImage(down)?down:up,kBottomX+r.x,kBottomY+r.y,r.w,r.h,false);
        const auto& lamp=sel && validImage(lampDown)?lampDown:lampUp;
        if(validImage(lamp)) {
            const int lw=std::min(18,lamp.getWidth());
            const int lh=std::max(1,std::min(18,lw*lamp.getHeight()/lamp.getWidth()));
            buttonArt(win,lamp,kBottomX+r.x+8,kBottomY+r.y+(r.h-lh)/2,lw,lh,false);
        }
        label(win,kBottomX+r.x+30,kBottomY+r.y+7,r.w-36,16,i==count?"BACK":pages[i].button,sel?green:_colorTable[18979],true);
    }
    label(win,kBottomX+12,kBottomY+219,296,12,"A: OPEN   B: BACK",bright,true);
    windowRefresh(win); ctr_gfx_invalidate();
}
} // namespace ctr_help

inline void ctrControlsHelpMenu() {
    using namespace ctr_help;
    if (screenGetWidth()!=640 || screenGetHeight()!=480) return;
    const auto previousLayout=ctr_rectMap.main;
    const auto previousMode=ctr_input.mode;
    const bool hidden=cursorIsHidden();
    int mx,my; mouseGetPosition(&mx,&my);
    const int win=windowCreate(0,0,640,480,_colorTable[0],WINDOW_MODAL|WINDOW_MOVE_ON_TOP);
    if(win<0) return;
    mouseHideCursor();
    ctr_input.mode=DISPLAY_MODE_ADAPT;
    setActiveRectMap(DISPLAY_CONTROLS_HELP);
    ctr_input_sync_help_buttons();
    FrmImage background,panel,up,down,lampUp,lampDown;
    background.lock(buildFid(OBJ_TYPE_INTERFACE,127,0,0,0));
    panel.lock(buildFid(OBJ_TYPE_INTERFACE,220,0,0,0));
    up.lock(buildFid(OBJ_TYPE_INTERFACE,221,0,0,0));
    down.lock(buildFid(OBJ_TYPE_INTERFACE,222,0,0,0));
    lampUp.lock(buildFid(OBJ_TYPE_INTERFACE,8,0,0,0));
    lampDown.lock(buildFid(OBJ_TYPE_INTERFACE,9,0,0,0));
    bool model, shoulder, stick;
    ctr_input_get_new_controls(&model,&shoulder,&stick);
    const int count=model?kPageCount:kPageCount-1;
    int selected=0,opened=0;
    bool done=false;
    u32 previous=ctr_input.frame.kHeld;
    draw(win,background,panel,up,down,lampUp,lampDown,selected,opened,count,shoulder,stick);
    while(!done && _game_user_wants_to_quit==0) {
        sharedFpsLimiter.mark();
        const int key=inputGetInput();
        const u32 held=ctr_input.frame.kHeld, pressed=held & ~previous;
        previous=held;
        const int oldSelected=selected,oldOpened=opened;
        if(pressed & KEY_DUP) selected=selected>=2?selected-2:selected;
        if(pressed & KEY_DDOWN) selected=std::min(count,selected+2);
        if(pressed & KEY_DLEFT) selected=std::max(0,selected-1);
        if(pressed & KEY_DRIGHT) selected=std::min(count,selected+1);
        if((pressed & KEY_A) || key==KEY_RETURN) { if(selected==count) done=true; else opened=selected; }
        if((pressed & KEY_B) || key==KEY_ESCAPE) done=true;
        int touchX, touchY;
        if(ctr_input_take_help_touch(&touchX,&touchY)) {
            const int choice=touchChoice(touchX,touchY,count);
            if(choice==count) done=true;
            else if(choice>=0) opened=selected=choice;
        }
        // This menu consumes its own touch; do not replay taps in the parent.
        Gesture gesture; while(touch_get_gesture(&gesture)) {}
        if(oldSelected!=selected || oldOpened!=opened) draw(win,background,panel,up,down,lampUp,lampDown,selected,opened,count,shoulder,stick);
        renderPresent(); sharedFpsLimiter.throttle();
    }
    // Keep the modal active until the closing tap/button is released.
    while(_game_user_wants_to_quit==0 && (ctr_input.frame.kHeld & (KEY_TOUCH|KEY_A|KEY_B|KEY_X))) {
        sharedFpsLimiter.mark(); inputGetInput();
        Gesture gesture; while(touch_get_gesture(&gesture)) {}
        renderPresent(); sharedFpsLimiter.throttle();
    }
    ctr_input_sync_help_buttons();
    background.unlock(); panel.unlock(); up.unlock(); down.unlock(); lampUp.unlock(); lampDown.unlock();
    windowDestroy(win);
    ctr_input.mode=previousMode;
    setActiveRectMap(previousLayout);
    _mouse_set_position(mx,my);
    if(!hidden) mouseShowCursor();
    ctr_gfx_invalidate();
}
} // namespace fallout
#endif
