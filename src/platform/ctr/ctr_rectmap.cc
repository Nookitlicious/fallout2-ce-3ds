#include <iostream>
#include <stdlib.h>
#include <cmath>
#include <algorithm>

#include "ctr_rectmap.h"
#include "ctr_main_menu_layout.h"
#include "ctr_input.h"

//#include "plib/gnw/gnw.h"
//#include "game/loadsave.h"

namespace fallout {

#define MIN_SCALE_FACTOR 0.4167

#define MIN_SCALE_X 320
#define MIN_SCALE_Y 240

#define MAX_SCALE_X 640
#define MAX_SCALE_Y 480

#define SCALE_STEP 0.1

rectMap_t ***rectMaps = { NULL };
int *numRectsInMap = { NULL };

int indicatorSlotNum = 0;

int saveSlotCursor = 4;
float saveSlotOffset = 0.f;

static float scaleFactor = 1.f;

float offsetX_field = 160.f;
float offsetY_field = 40.f;

float offsetX_field_scaled_max;
float offsetY_field_scaled_max;

bool isAgeWindow = false;
bool isSexWindow = false;

int offsetY_char = 0;


void setSaveSlotOffset(int count)
{
    if ((!((saveSlotCursor-count) < 0 )) && (!((saveSlotCursor-count) > 4 )))
        return;

    if ((saveSlotCursor-count) < 0 ) {
        saveSlotCursor -= (saveSlotCursor-count);
    } else {
        saveSlotCursor-=1;
    }

    saveSlotOffset = 34.f * ((saveSlotCursor) - 4);

    if (saveSlotOffset < 0)
        saveSlotOffset = 0.f;
}

float getSaveSlotOffset()
{
    return saveSlotOffset;
}

void setIndicatorSlotNum(int count)
{
    indicatorSlotNum = count;
}

int getIndicatorSlotNum()
{
    return indicatorSlotNum;
}

void setActiveRectMap(rectMap_e rectmap)
{
    if (rectmap != DISPLAY_LAST) {
        ctr_rectMap.main = rectmap;
    }

    if (isGameplayRectMap(ctr_rectMap.main) && ctr_input.mode != DISPLAY_MODE_ADAPT)
        ctr_rectMap.active = DISPLAY_FULL;
    else
        ctr_rectMap.active = ctr_rectMap.main;

    if (ctr_rectMap.active == DISPLAY_GUI)
        ctr_input_center_mouse();
}

void restoreGameplayRectMap(rectMap_e previous)
{
    ctr_input.mode = DISPLAY_MODE_ADAPT;
    setActiveRectMap(isGameplayRectMap(previous) ? previous : DISPLAY_GUI);
}

void setPreviousRectMap(int index)
{
    if (index >= 0 && index <= 2) {
        ctr_rectMap.prev[index] = ctr_rectMap.main;
    }
}

rectMap_e getPreviousRectMap(int index)
{
    if (index >= 0 && index <= 2) {
        return ctr_rectMap.prev[index];
    }
//    GNWSystemError("getPreviousRectMap failed\n");
    return DISPLAY_FULL;
}

void setRectMapPos(rectMap_e rectmap, float x, float y, float w, float h, bool isTop)
{
    rectMaps[rectmap][0]->src_x = x;
    rectMaps[rectmap][0]->src_y = y;
    rectMaps[rectmap][0]->src_w = w;
    rectMaps[rectmap][0]->src_h = h;

    float maxDstWidth = isTop ? 400.f : 320.f;

    const float maxDstHeight = 240.f;

    float aspectRatio = w / h;

    float newWidth, newHeight;
    if (maxDstWidth / maxDstHeight > aspectRatio) {
        newHeight = maxDstHeight;
        newWidth = maxDstHeight * aspectRatio;
    } else {
        newWidth = maxDstWidth;
        newHeight = maxDstWidth / aspectRatio;
    }

    float newX = (maxDstWidth - newWidth) / 2.f;
    float newY = (maxDstHeight - newHeight) / 2.f;

    rectMaps[rectmap][0]->dst_x = newX;
    rectMaps[rectmap][0]->dst_y = newY;
    rectMaps[rectmap][0]->dst_w = newWidth;
    rectMaps[rectmap][0]->dst_h = newHeight;
}

int setRectMapScaled(bool scaled_rect_top, float rawScaleFactor)
{
    if (rawScaleFactor != -1.0) {
        float newScaleFactor = roundf(rawScaleFactor / SCALE_STEP) * SCALE_STEP;

        if (newScaleFactor < 0.0f) newScaleFactor = 0.0f;
        if (newScaleFactor > 1.0f) newScaleFactor = 1.0f;

        if (scaleFactor == newScaleFactor)
            return -1;

        scaleFactor = newScaleFactor;
    }

    float mappedScaleFactor = MIN_SCALE_FACTOR + (scaleFactor * (1.0f - MIN_SCALE_FACTOR));

    int scaled_x = roundf(MAX_SCALE_X - (MAX_SCALE_X - MIN_SCALE_X) * mappedScaleFactor);
    int scaled_y = roundf(MAX_SCALE_Y - (MAX_SCALE_Y - MIN_SCALE_Y) * mappedScaleFactor);

    if (scaled_rect_top)
        scaled_x += roundf(160 - (80 * mappedScaleFactor));

    rectMaps[DISPLAY_FIELD][0]->src_w = scaled_x;
    rectMaps[DISPLAY_FIELD][0]->src_h = scaled_y;

    offsetX_field_scaled_max = MAX_SCALE_X - scaled_x;
    offsetY_field_scaled_max = MAX_SCALE_Y - scaled_y;

    if (offsetY_field_scaled_max >= 100)
        offsetY_field_scaled_max -= 100;

    if (offsetY_field < 0)
        offsetY_field = 0;
    if (offsetY_field > offsetY_field_scaled_max)
        offsetY_field = offsetY_field_scaled_max;

    if (offsetX_field < 0)
        offsetX_field = 0;
    if (offsetX_field > offsetX_field_scaled_max)
        offsetX_field = offsetX_field_scaled_max;

    rectMaps[DISPLAY_FIELD][0]->src_x = offsetX_field;
    rectMaps[DISPLAY_FIELD][0]->src_y = offsetY_field;

    setRectMapPos(DISPLAY_FIELD,
            rectMaps[DISPLAY_FIELD][0]->src_x, rectMaps[DISPLAY_FIELD][0]->src_y,
            rectMaps[DISPLAY_FIELD][0]->src_w, rectMaps[DISPLAY_FIELD][0]->src_h,
            scaled_rect_top);

    return 0;
}

void addRectMap(rectMap_e rectmap, int map_index, float src_x, float src_y, float src_w, float src_h,
        float dst_x, float dst_y, float dst_w, float dst_h)
{
    if (rectMaps[rectmap] == NULL) {
        rectMaps[rectmap] = (rectMap_t **)malloc(sizeof(rectMap_t *));
    } else {
        rectMaps[rectmap] = (rectMap_t **)realloc(rectMaps[rectmap], 
                (numRectsInMap[rectmap] + 1) * sizeof(rectMap_t *));
    }

    rectMaps[rectmap][numRectsInMap[rectmap]] = (rectMap_t *)malloc(sizeof(rectMap_t));
    if (rectMaps[rectmap][numRectsInMap[rectmap]] == NULL) {
//        GNWSystemError("addRectMap failed\n");
        exit(EXIT_FAILURE);
    }

    rectMaps[rectmap][numRectsInMap[rectmap]]->src_x = src_x;
    rectMaps[rectmap][numRectsInMap[rectmap]]->src_y = src_y;
    rectMaps[rectmap][numRectsInMap[rectmap]]->src_w = src_w;
    rectMaps[rectmap][numRectsInMap[rectmap]]->src_h = src_h;

    rectMaps[rectmap][numRectsInMap[rectmap]]->dst_x = dst_x;
    rectMaps[rectmap][numRectsInMap[rectmap]]->dst_y = dst_y;
    rectMaps[rectmap][numRectsInMap[rectmap]]->dst_w = dst_w;
    rectMaps[rectmap][numRectsInMap[rectmap]]->dst_h = dst_h;

    numRectsInMap[rectmap]++;
}

void rectmap_init()
{
    if (rectMaps == NULL) {
        rectMaps = (rectMap_t ***)calloc(DISPLAY_LAST, sizeof(rectMap_t **));
        numRectsInMap = (int *)calloc(DISPLAY_LAST, sizeof(int));
        if (rectMaps == NULL || numRectsInMap == NULL) {
//            GNWSystemError("rectmap_init failed\n");
            exit(EXIT_FAILURE);
        }
    }
}

void ctr_rectmap_exit()
{
    if (rectMaps != NULL) {
        for (int i = 0; i < DISPLAY_LAST; ++i) {
            if (rectMaps[i] != NULL) {
                for (int j = 0; j < numRectsInMap[i]; ++j) {
                    free(rectMaps[i][j]);
                }
                free(rectMaps[i]);
            }
        }
        free(numRectsInMap);
        free(rectMaps);

        rectMaps = NULL;
        numRectsInMap = NULL;
    }
}

ScopedCharacterModalMap::ScopedCharacterModalMap(rectMap_e map, int x, int y, int w, int h, bool enabled)
    : previous_(ctr_rectMap.main), enabled_(enabled && w > 0 && h > 0)
{
    if (enabled_) {
        setRectMapPos(map, x, y, w, h, false);
        setActiveRectMap(map);
    }
}

ScopedCharacterModalMap::~ScopedCharacterModalMap()
{
    if (enabled_) setActiveRectMap(previous_);
}

void characterModalTouchToTexture(rectMap_e map, int x, int y, int* outX, int* outY)
{
    const auto* r = rectMaps[map][0];
    const float top = 240.f - r->dst_y - r->dst_h;
    // Letterbox taps land outside the centered modal, not on the last button.
    *outX = 0;
    *outY = 0;
    if (x >= r->dst_x && x < r->dst_x + r->dst_w && y >= top && y < top + r->dst_h) {
        *outX = static_cast<int>(r->src_x) + std::clamp(static_cast<int>((x-r->dst_x)*r->src_w/r->dst_w), 0, static_cast<int>(r->src_w)-1);
        *outY = static_cast<int>(r->src_y) + std::clamp(static_cast<int>((y-top)*r->src_h/r->dst_h), 0, static_cast<int>(r->src_h)-1);
    }
}

void scrollCharacterView(int delta)
{
    if (!isAgeWindow && !isSexWindow) offsetY_char = std::clamp(offsetY_char + delta, 0, 525);
}

static bool worldMapSwapped = false;

bool worldMapScreensSwapped() { return worldMapSwapped; }

void toggleWorldMapScreens() { worldMapSwapped = !worldMapSwapped; }

rectMap_t worldMapViewportRect()
{
    auto r = *rectMaps[DISPLAY_WORLDMAP_TOP][0];
    if (worldMapSwapped) r.dst_x -= 40.f;
    return r;
}

void setWorldMapOrigin(int x, int y)
{
    worldMapSwapped = false;
    auto* top = rectMaps[DISPLAY_WORLDMAP_TOP][0];
    top->src_x = x + 22;
    top->src_y = y + 21;
    const int panelY[] = {130, 0, 320};
    for (int i = 0; i < numRectsInMap[DISPLAY_WORLDMAP]; ++i) {
        auto* r = rectMaps[DISPLAY_WORLDMAP][i];
        r->src_x = x + 472;
        r->src_y = y + panelY[i];
    }
}

void worldMapTouchToTexture(int x, int y, int* outX, int* outY)
{
    const auto* viewport = rectMaps[DISPLAY_WORLDMAP_TOP][0];
    const int originX = static_cast<int>(viewport->src_x) - 22;
    const int originY = static_cast<int>(viewport->src_y) - 21;
    // Inert border, outside the map viewport and every control button.
    *outX = originX + 475;
    *outY = originY + 470;
    if (worldMapSwapped) {
        const auto r = worldMapViewportRect();
        const float top = 240 - r.dst_y - r.dst_h;
        if (x >= r.dst_x && x < r.dst_x + r.dst_w && y >= top && y < top + r.dst_h) {
            *outX = static_cast<int>(r.src_x + (x - r.dst_x) * r.src_w / r.dst_w);
            *outY = static_cast<int>(r.src_y + (y - top) * r.src_h / r.dst_h);
        }
        return;
    }
    for (int i = 0; i < numRectsInMap[DISPLAY_WORLDMAP]; ++i) {
        const auto* r = rectMaps[DISPLAY_WORLDMAP][i];
        const float top = 240 - r->dst_y - r->dst_h;
        if (x >= r->dst_x && x < r->dst_x + r->dst_w
            && y >= top && y < top + r->dst_h) {
            *outX = static_cast<int>(r->src_x + (x - r->dst_x) * r->src_w / r->dst_w);
            *outY = static_cast<int>(r->src_y + (y - top) * r->src_h / r->dst_h);
            const int localX = *outX - originX;
            const int localY = *outY - originY;
            // Snap the entire visible name/plate (including its border) to
            // the existing transparent name button. Empty rows stay inert.
            if (i == 0 && localX >= 501 && localY >= 135 && localY < 313) {
                const int row = (localY - 135) / 27;
                *outX = originX + 550;
                *outY = originY + std::min(145 + row * 27, 307);
            } else if (i == 0 && localX < 501 && localY >= 130 && localY < 167) {
                // Expand the two little scroll arrows without overlapping names.
                *outX = originX + 484;
                *outY = originY + (localY < 151 ? 141 : 156);
            } else if (i == 2 && localX >= 500 && localX < 625 && localY >= 420 && localY < 464) {
                *outX = originX + 550;
                *outY = originY + 442;
            }
            return;
        }
    }
}

void ctr_rectmap_init()
{
    rectmap_init();

    addRectMap(DISPLAY_FULL,              0,   0,   0, 640, 480,   0,   0, 320, 240);
    addRectMap(DISPLAY_FIELD,             0,   0,   0, 400, 240,   0,   0, 400, 240);
    addRectMap(DISPLAY_MOVIE,             0, 0,  80, 640, 320,  0,   20, 400, 200);
    addRectMap(DISPLAY_MOVIE_SUB,         0, 105, 401, 430,  80,   0,  60, 320, 120);
    // Original left-hand metal plates, including all of the EXIT row.
    addRectMap(DISPLAY_MAIN, 0, ctr_menu::BottomSrcX, ctr_menu::BottomSrcY,
        ctr_menu::BottomSrcW, ctr_menu::BottomSrcH, ctr_menu::BottomDstX,
        ctr_menu::BottomDstY, ctr_menu::BottomDstW, ctr_menu::BottomDstH);

    addRectMap(DISPLAY_GUI,               0,   5, 380, 200, 100,   0, 100, 200, 100); // monitor
    addRectMap(DISPLAY_GUI,               1, 520, 380, 140, 100, 200, 100, 140, 100); // skill pip
    addRectMap(DISPLAY_GUI,               2, 200, 380, 320, 100,   0,   0, 320, 100); // weapon

    addRectMap(DISPLAY_GUI_INDICATOR,     0,   0, 359, 127,  21,   0, 200, 127,  20); // stat indicator
    addRectMap(DISPLAY_GUI_INDICATOR,     1, 130, 359, 127,  21, 193, 200, 127,  20); // stat indicator
    addRectMap(DISPLAY_GUI_INDICATOR,     2, 260, 359, 127,  21,   0, 220, 127,  20); // stat indicator
    addRectMap(DISPLAY_GUI_INDICATOR,     3, 390, 359, 127,  21, 193, 220, 127,  20); // stat indicator

    addRectMap(DISPLAY_PAUSE,             0, 240,  72, 160, 215,  71,   0, 178, 240);

    addRectMap(DISPLAY_DIALOG_TOP,        0,  80,   0, 480, 290,   0,   0, 397, 240);
    addRectMap(DISPLAY_DIALOG_TOP,        1, 130, 225, 390,  65,  10,   0, 380,  60); // full reply window

    addRectMap(DISPLAY_DIALOG,            0, 120, 320, 407, 160,   0,  80, 320, 160); // full options window + margins
    addRectMap(DISPLAY_DIALOG,            1,   5, 440,  70,  37,  45,  10,  70,  37); // bottom review
    addRectMap(DISPLAY_DIALOG,            2, 575, 324,  50,  28, 135,  15,  50,  28); // bottom barter
    addRectMap(DISPLAY_DIALOG,            3, 575, 399,  50,  28, 210,  15,  50,  28); // bottom about
    addRectMap(DISPLAY_DIALOG,            4,   5, 417,  70,  18,  48,  52,  70,  18); // bottom review text
    addRectMap(DISPLAY_DIALOG,            5, 565, 305,  70,  18, 125,  52,  70,  18); // bottom barter text
    addRectMap(DISPLAY_DIALOG,            6, 565, 360,  70,  35, 200,  44,  70,  35); // bottom about text

    // Review buttons are independent of the normal dialogue options window.
    addRectMap(DISPLAY_DIALOG_REVIEW, 0, 475, 152, 35, 35, 32, 130, 100, 100);
    addRectMap(DISPLAY_DIALOG_REVIEW, 1, 475, 191, 35, 37, 188, 130, 100, 100);
    addRectMap(DISPLAY_DIALOG_REVIEW, 2, 499, 398, 82, 46, 78, 18, 164, 92);
    // Town entrance list is an engine window occupying the right-hand panel.
    addRectMap(DISPLAY_TOWN_ENTRANCES, 0, 472, 0, 168, 480, 0, 0, 320, 240);

    addRectMap(DISPLAY_SKILLDEX,          0, 465,  50, 165, 140,  10,  50, 150, 140);
    addRectMap(DISPLAY_SKILLDEX,          1, 465, 195, 165, 140, 160,  50, 150, 140);
    addRectMap(DISPLAY_SKILLDEX,          2, 497,  14, 90,   29, 105, 195, 110,  35); // skilldex
    addRectMap(DISPLAY_SKILLDEX,          3, 485, 339, 117,  24, 102,  10, 117,  24); // cancel

    addRectMap(DISPLAY_INVENTORY,         0,  95,  10, 470, 357,   0,   0, 320, 240);
    addRectMap(DISPLAY_INVENTORY_USE,     0,  80,   0, 290, 375,  52,   0, 215, 240); // single small inventory
    addRectMap(DISPLAY_INVENTORY_LOOT,    0,  95,  10, 505, 360,   0,   0, 320, 240);

    addRectMap(DISPLAY_INVENTORY_TRADE,   0,  88, 290, 460, 192,   0,  80, 320, 160); // during barter
    addRectMap(DISPLAY_INVENTORY_TRADE,   1,   0, 420,  80,  60,  80,   0,  80,  60); // offer
    addRectMap(DISPLAY_INVENTORY_TRADE,   2, 560, 420,  80,  60, 160,   0,  80,  60); // talk
    addRectMap(DISPLAY_INVENTORY_TRADE,   3,   0, 290,  80, 140,   0,   0,  60,  80); // during barter
    addRectMap(DISPLAY_INVENTORY_TRADE,   4, 560, 290,  80, 140, 260,   0,  60,  80); // during barter

    // Original Fallout 2 world-map viewport and right-side control panel.
    // Keep the map aspect ratio; dedicate the entire left touchscreen column
    // to the seven city tabs. Date/dial and Town/World share the right column.
    addRectMap(DISPLAY_WORLDMAP_TOP, 0, 22, 21, 450, 443, 78, 0, 244, 240);
    addRectMap(DISPLAY_WORLDMAP, 0, 472, 130, 168, 190, 0, 0, 184, 240);
    addRectMap(DISPLAY_WORLDMAP, 1, 472, 0, 168, 135, 188, 125, 132, 115);
    addRectMap(DISPLAY_WORLDMAP, 2, 472, 320, 168, 160, 188, 0, 132, 121);
    addRectMap(DISPLAY_PIPBOY,            0,   0, 330, 254, 150,   0,   0, 320, 190);
    addRectMap(DISPLAY_PIPBOY,            1,   0,   0, 254,  40,   0, 190, 320,  50);
    addRectMap(DISPLAY_AUTOMAP, 0, 60, 0, 519, 480, 30, 0, 260, 240);
    addRectMap(DISPLAY_SAVE_COMMENT, 0, 0, 0, 1, 1, 0, 0, 1, 1);

    addRectMap(DISPLAY_CHAR_SELECT,       0,   0,   0, 640, 480,   0,   0, 320, 240); // char selection at game start

    addRectMap(DISPLAY_CHAR_TOP,          0, 335, 257, 300, 190,   0,   0, 400, 240);

    addRectMap(DISPLAY_CHAR,              0,  10,  30, 320, 210,   0,   0, 320, 210); // left
    addRectMap(DISPLAY_CHAR,              1, 340,   0, 320, 240,   0,   0, 320, 240); // right
    addRectMap(DISPLAY_CHAR,              2,  10,   0, 320,  30,   0, 210, 320,  30); // name / age / sex
    addRectMap(DISPLAY_CHAR,              3, 336, 450, 300,  25,   0,   0, 320,  25); // options / done / cancel

    addRectMap(DISPLAY_CHAR_OPTIONS,     0, 0, 0, 640, 480, 0, 0, 320, 240);
    addRectMap(DISPLAY_CHAR_FILE,        0, 0, 0, 640, 480, 0, 0, 320, 240);

    addRectMap(DISPLAY_CHAR_EDIT_AGE,     0, 155,   0, 140,  70, 145, 170, 140,  70);
    addRectMap(DISPLAY_CHAR_EDIT_SEX,     0, 195,   0, 140,  70, 180, 170, 140,  70);

    addRectMap(DISPLAY_CHAR_PERK_TOP,     0, 304, 110, 290, 180,   0,   0, 400, 240);
    addRectMap(DISPLAY_CHAR_PERK,         0,  44, 105, 247, 199,   0,   0, 320, 240);

    addRectMap(DISPLAY_LOADSAVE_TOP,      0, 330,  30, 295, 190,  15,   0, 370, 238); // image
    addRectMap(DISPLAY_LOADSAVE_TOP,      1, 337, 226, 280,  95, 155,   0, 230,  78); // desc

    addRectMap(DISPLAY_LOADSAVE_SLOT,     0,  50,  80, 220, 170,  32,  32, 270, 175); // slots

    addRectMap(DISPLAY_LOADSAVE,          0,  35,  58,  10,  27,   3, 131,  25,  70); // arrows
    addRectMap(DISPLAY_LOADSAVE,          1, 380, 344,  97,  26,  60,   0,  97,  26); // done
    addRectMap(DISPLAY_LOADSAVE,          2, 485, 344,  97,  26, 180,   0,  97,  26); // cancel

    addRectMap(DISPLAY_LOADSAVE_BACK,     0,  20,  20, 290, 200,   0,  40, 320, 200); // frame top
    addRectMap(DISPLAY_LOADSAVE_BACK,     1,  20, 440, 290,  20,   0,   0, 320,  40); // frame bottom

    addRectMap(DISPLAY_MODS, 0, 160, 240, 320, 240, 0, 0, 320, 240);
    addRectMap(DISPLAY_OPTIONS,           0,   0,   0, 640, 480,   0,   0, 320, 240); // preferences TODO

    addRectMap(DISPLAY_ENDGAME,           0,   0,   0, 640, 480,   0,   0, 320, 240); // TODO
    addRectMap(DISPLAY_DYNAMIC,           0,   0,   0,   0,   0,   0,   0,   0,   0); // TODO
    addRectMap(DISPLAY_DEAD,              0,   0,   0, 640, 480,   0,   0, 320, 240); // TODO
}

} //namespace fallout
