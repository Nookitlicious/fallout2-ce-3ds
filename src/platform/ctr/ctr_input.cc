#include <string.h>
#include <stdio.h>
#include <algorithm>

#include "ctr_input.h"
#include "ctr_mod_profiles.h"
#include "game.h"
#include "ctr_gfx.h"
#include "ctr_rectmap.h"
#include "ctr_main_menu_layout.h"
#include "ctr_new_controls.h"
//#include "ctr_sys.h"

#include "platform_compat.h"
#include "sfall_config.h"

//#include "plib/gnw/gnw.h"
#include "svga.h"
#include "input.h"
#include "kb.h"
#include "character_editor.h"
#include "mouse.h"
#include "touch.h"
#include "debug.h"

//#include "gdialog.h"
#include "map.h"
#include "scripts.h"

namespace fallout {

#define FLAG_NONE 0x00
#define FLAG_SELECT_DDOWN 0x01
#define FLAG_SELECT_DUP 0x02
#define FLAG_SELECT_DLEFT 0x04
#define FLAG_SELECT_DRIGHT 0x08
#define FLAG_START_DDOWN 0x10
#define FLAG_START_DUP 0x20
#define FLAG_START_DLEFT 0x40
#define FLAG_START_DRIGHT 0x80

#define KEY_PRESS(x) ctr_input.frame.kHeld & x && (!(oldpad & x))
#define KEY_HELD(x) (ctr_input.frame.kHeld & x)
#define KEY_RELEASED(x) (!(ctr_input.frame.kHeld & x) && (oldpad & x))

#define MAX_OFFSET 240

#define ABOUT_INPUT_MAX_SIZE 128
#define MAX_DICT_WORDS 100

ctr_input_t ctr_input;

static u8 key_combi_flags = FLAG_NONE;
static u32 oldpad = 0;
static bool scaled_rect_top = false;
static int cursorDivisor = 10;
static float previousSliderState = -1.0f;
static float manualScaleFactor = 0.0f;
static bool newModel = false;
static int newShoulderZoom = 1;
static int newCStickPan = 1;

void ctr_input_get_new_controls(bool* model, bool* shoulder, bool* stick) {
    *model = newModel; *shoulder = newModel && newShoulderZoom != 0;
    *stick = newModel && newCStickPan != 0;
}
static int helpTouchX = -1, helpTouchY = -1;
void ctr_input_sync_help_buttons() {
    oldpad = ctr_input.frame.kHeld;
    helpTouchX = helpTouchY = -1;
}
bool ctr_input_take_help_touch(int* x, int* y) {
    if (helpTouchX < 0 || helpTouchY < 0) return false;
    *x = helpTouchX; *y = helpTouchY;
    helpTouchX = helpTouchY = -1;
    return true;
}

static bool adaptedGameplay()
{
    return isGameplayRectMap(ctr_rectMap.active);
}

static bool cStickPansHere()
{
    return ctr_new_controls::stickPans(newModel, newCStickPan != 0,
        adaptedGameplay() || ctr_rectMap.active == DISPLAY_WORLDMAP
        || (ctr_rectMap.active == DISPLAY_FULL && isGameplayRectMap(ctr_rectMap.main)));
}

int offsetX = 0;
int offsetY = 0;

static SwkbdDictWord words[MAX_DICT_WORDS];
static int num_words;
static int dictId = -1;


void ctr_input_center_mouse(void)
{
    mouseHideCursor();
    _mouse_set_position(offsetX_field + (rectMaps[DISPLAY_FIELD][0]->src_w / 2),
            offsetY_field + (rectMaps[DISPLAY_FIELD][0]->src_h / 2));
    mouseShowCursor();
}

void input_touch_to_texture(rectMap_e rectmap, int touch_x, int touch_y, int *originalX, int *originalY)
{
    for (int i = 0; i < numRectsInMap[rectmap]; ++i) {

    rectMap_t *dstRect = rectMaps[rectmap][i];

        if (touch_x >= dstRect->dst_x && touch_x < (dstRect->dst_x + dstRect->dst_w) &&
            touch_y >= (240 - (dstRect->dst_y + dstRect->dst_h)) && touch_y < (240 - dstRect->dst_y)) {

            rectMap_t *srcRect = rectMaps[rectmap][i];

            float x_scale = dstRect->dst_w / srcRect->src_w;
            float y_scale = dstRect->dst_h / srcRect->src_h;

            float adjusted_touch_x = touch_x - dstRect->dst_x;
            float adjusted_touch_y = touch_y - (240 - (dstRect->dst_y + dstRect->dst_h));

            *originalX = (adjusted_touch_x / x_scale) + srcRect->src_x;
            *originalY = (adjusted_touch_y / y_scale) + srcRect->src_y;
        }
    }
}

void ctr_input_get_touch(int *newX, int *newY)
{
    switch (ctr_rectMap.active)
    {
        case DISPLAY_WORLDMAP:
            worldMapTouchToTexture(ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            break;
        case DISPLAY_MAIN:
            ctr_menu::mapTouch(ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            break;
        case DISPLAY_FULL:
            *newX = (ctr_input.frame.touchX * screenGetWidth()) / 320;
            *newY = (ctr_input.frame.touchY * screenGetHeight()) / 240;
            break;
        case DISPLAY_CHAR_OPTIONS:
        case DISPLAY_CHAR_FILE:
            characterModalTouchToTexture(ctr_rectMap.active, ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            break;
        case DISPLAY_CHAR:
            input_touch_to_texture(DISPLAY_CHAR,
                    ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);

            if ((ctr_input.frame.touchY > 30) && (ctr_input.frame.touchY < 215)) {
                if (offsetY_char <= 240) {
                    *newY += offsetY_char;
                    *newX -= 330;
                } else if ((offsetY_char > 240) && (offsetY_char <= 480)) {
                    int visableTop = 480 - offsetY_char;

                    if (ctr_input.frame.touchY <= visableTop) {
                        *newY += offsetY_char;
                        *newX -= 330;
                    } else {
                        *newY += (-visableTop);
                        *newX -= 10;
                    }
                } else if (offsetY_char > 480) {
                    *newY += offsetY_char-480;
                    *newX -= 10;
                }
            }
            if (isAgeWindow)
                input_touch_to_texture(DISPLAY_CHAR_EDIT_AGE,
                        ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);

            if (isSexWindow)
                input_touch_to_texture(DISPLAY_CHAR_EDIT_SEX,
                        ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            break;
        case DISPLAY_DIALOG_REVIEW:
            // Inert corner outside all three review buttons.
            *newX = 0; *newY = 0;
            input_touch_to_texture(ctr_rectMap.active, ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            break;
        case DISPLAY_TOWN_ENTRANCES:
            *newX = 472 + std::clamp(ctr_input.frame.touchX, 0, 319) * 168 / 320;
            *newY = std::clamp(ctr_input.frame.touchY, 0, 239) * 2;
            break;
        case DISPLAY_PIPBOY:
        case DISPLAY_AUTOMAP:
        case DISPLAY_SAVE_COMMENT:
        case DISPLAY_DYNAMIC:
            // A tap in the letterbox must not reuse a previous button position.
            *newX = rectMaps[ctr_rectMap.active][0]->src_x > 0 ? 0 : screenGetWidth() - 1;
            *newY = rectMaps[ctr_rectMap.active][0]->src_y > 0 ? 0 : screenGetHeight() - 1;
            input_touch_to_texture(ctr_rectMap.active, ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            break;
        case DISPLAY_LOADSAVE:
            input_touch_to_texture(DISPLAY_LOADSAVE_SLOT,
                    ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            *newY += getSaveSlotOffset();
        default:
            input_touch_to_texture(ctr_rectMap.active,
                    ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            break;
    }
}

static bool menuQuitEnabled = false;
void ctr_input_set_menu_quit(bool enabled) { menuQuitEnabled = enabled; }

void ctr_input_process(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if ((ctr_input.input == INPUT_TOUCH) &&
                (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERUP || e.type == SDL_FINGERMOTION)) {
            float touchX = e.tfinger.x * 320;
            float touchY = e.tfinger.y * 240;

            offsetX = touchX * MAX_OFFSET / 320;
            offsetY = 240 - (touchY * MAX_OFFSET / 240);

            ctr_input.frame.touchX = touchX;
            ctr_input.frame.touchY = touchY;
        }

        switch (e.type) {
        case SDL_FINGERDOWN:
            if (ctr_rectMap.active == DISPLAY_CONTROLS_HELP) {
                helpTouchX = std::clamp(static_cast<int>(e.tfinger.x * 320), 0, 319);
                helpTouchY = std::clamp(static_cast<int>(e.tfinger.y * 240), 0, 239);
            }
            touch_handle_start(&(e.tfinger));
            break;
        case SDL_FINGERMOTION:
            touch_handle_move(&(e.tfinger));
            break;
        case SDL_FINGERUP:
            touch_handle_end(&(e.tfinger));
            break;

        case SDL_QUIT:
            if (menuQuitEnabled) {
                ctrModProfileStage("SDL quit received in menu; requesting orderly exit");
                _game_user_wants_to_quit = 3;
                return;
            }
            ctrModProfileStage("Alpha12: SDL quit outside menu; exit cleanup requested");
            exit(EXIT_SUCCESS);
            break;
        }
    }
    touch_process_gesture();

    ctr_input.frame.kHeld = hidKeysHeld();

    if ((ctr_rectMap.active == DISPLAY_FIELD) || (ctr_rectMap.active == DISPLAY_GUI)) {
        float scaleFactorToUse;
        float currentSliderState = osGet3DSliderState();
        int sliderChanged = (currentSliderState != previousSliderState);

        if (key_combi_flags & (FLAG_SELECT_DDOWN | FLAG_SELECT_DUP)) {
            scaleFactorToUse = manualScaleFactor;
        } else if (sliderChanged) {
            scaleFactorToUse = currentSliderState;
            previousSliderState = currentSliderState;
        } else if (scaled_rect_top == (ctr_rectMap.active == DISPLAY_GUI)) {
            return;
        } else {
            scaleFactorToUse = -1.0f;
        }
        scaled_rect_top = (ctr_rectMap.active == DISPLAY_GUI);

        if (setRectMapScaled(scaled_rect_top, scaleFactorToUse) == 0) {
            mouseHideCursor();
            _mouse_set_position(offsetX_field + (rectMaps[DISPLAY_FIELD][0]->src_w / 2),
                    offsetY_field + (rectMaps[DISPLAY_FIELD][0]->src_h / 2));
            mouseShowCursor();
        }
    }
}

void input_frame_touch(void)
{
    return;
}

void input_frame_axis(int *delta_x, int *delta_y, bool independentPan)
{
    int x, y;
    int moveX;
    int moveY;

    if ((ctr_rectMap.active == DISPLAY_FULL) || (ctr_rectMap.active == DISPLAY_FIELD) ||
            (ctr_rectMap.active == DISPLAY_GUI) || (ctr_rectMap.active == DISPLAY_WORLDMAP) ||
            (ctr_rectMap.active == DISPLAY_CHAR_SELECT) || (ctr_rectMap.active == DISPLAY_CHAR) ||
            (ctr_rectMap.active == DISPLAY_PIPBOY)) {

        circlePosition circle;
        hidCircleRead(&circle);

        circlePosition cstick;
        hidCstickRead(&cstick);

        // In map-panning mode the C-stick is consumed only by the pan handler.
        // All other screens retain the original cursor/character-scroll behavior.
        if (cStickPansHere()) {
            cstick.dx = 0;
            cstick.dy = 0;
        }
        // Moderate Circle Pad speed for accurate O3DS cursor targeting.
        moveX = (int)circle.dx / cursorDivisor + (int)cstick.dx / 15;
        moveY = -((int)circle.dy / cursorDivisor + (int)cstick.dy / 15);

        if (abs(moveX) < 2) {
            moveX = 0;
        }
        if (abs(moveY) < 2) {
            moveY = 0;
        }

        if ((ctr_rectMap.active == DISPLAY_FIELD) || (ctr_rectMap.active == DISPLAY_GUI)) {
            if ((moveX != 0) || (moveY != 0)) {
                mouseGetPosition(&x, &y);
                if (independentPan) {
                    // Apply raw Circle Pad movement once; the pan handler will
                    // keep the cursor in its current map/interface region.
                    *delta_x += moveX;
                    *delta_y += moveY;
                } else if (y > 380) {
                    ctr_input_center_mouse();
                } else if (y + moveY >= 380) {
                    mouseHideCursor();
                    _mouse_set_position(x, 379);
                    mouseShowCursor();
                } else {
                    *delta_x += moveX;
                    *delta_y += moveY;
                }
            }
        } else if (ctr_rectMap.active == DISPLAY_CHAR) {
            scrollCharacterView(moveY);
        } else {
            *delta_x += moveX;
            *delta_y += moveY;
        }
    }
}

static ctr_new_controls::Directions readPanDirections()
{
    ctr_new_controls::Directions stick = {false, false, false, false};
    if (cStickPansHere()) {
        circlePosition cstick;
        hidCstickRead(&cstick);
        stick = ctr_new_controls::panDirections(true,
            KEY_HELD(KEY_SELECT) || KEY_HELD(KEY_START), cstick.dx, cstick.dy);
    }
    return ctr_new_controls::combine({KEY_HELD(KEY_DUP) != 0,
        KEY_HELD(KEY_DDOWN) != 0, KEY_HELD(KEY_DLEFT) != 0, KEY_HELD(KEY_DRIGHT) != 0}, stick);
}

void input_frame_buttons(int *delta_x, int *delta_y, int *buttons,
    ctr_new_controls::Directions pan, bool independentPan)
{
#ifdef _DEBUG_OVERLAY
    if (KEY_HELD(KEY_START) && KEY_PRESS(KEY_DUP)) {
        ctr_gfx.overlayEnable = !ctr_gfx.overlayEnable;
        key_combi_flags |= FLAG_START_DUP;
    }

    if (KEY_HELD(KEY_START) && KEY_PRESS(KEY_DDOWN)) {
        ctr_gfx.overlayEnableExtra = !ctr_gfx.overlayEnableExtra;
        key_combi_flags |= FLAG_START_DDOWN;
    }

    if (KEY_HELD(KEY_START) && KEY_PRESS(KEY_DRIGHT)) {
        linearHeapAvailable = ctr_sys_check_linear_heap();
        heapAvailable = ctr_sys_check_heap();
        key_combi_flags |= FLAG_START_DRIGHT;
    }

    if (KEY_HELD(KEY_START) && KEY_PRESS(KEY_DLEFT)) {
        ctr_gfx.isWide = !ctr_gfx.isWide;
        gfxSetWide(ctr_gfx.isWide);
        ctr_gfx_reinit();
        key_combi_flags |= FLAG_SELECT_DLEFT;
    }
#endif
    const bool shoulderZoomHere = ctr_new_controls::shoulderZoom(newModel, newShoulderZoom != 0, adaptedGameplay());
    if (shoulderZoomHere && key_combi_flags == FLAG_NONE && !KEY_HELD(KEY_SELECT) && !KEY_HELD(KEY_START)) {
        const bool out = KEY_PRESS(KEY_ZL);
        const bool in = KEY_PRESS(KEY_ZR);
        if ((out || in) && !((KEY_HELD(KEY_ZL)) && (KEY_HELD(KEY_ZR)))) {
            manualScaleFactor = ctr_new_controls::zoomStep(getRectMapScaleFactor(), out, in);
            if (setRectMapScaled(ctr_rectMap.active == DISPLAY_GUI, manualScaleFactor) == 0) {
                ctr_input_center_mouse();
                ctr_gfx_invalidate();
            }
        }
    }
    if (isGameplayRectMap(ctr_rectMap.main) && KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_DDOWN)) {
        if (shoulderZoomHere) manualScaleFactor = getRectMapScaleFactor();
        manualScaleFactor -= 0.2f; // zoom out
        if (manualScaleFactor < 0.0f) {
            manualScaleFactor = 0.0f;
        }
        key_combi_flags |= FLAG_SELECT_DDOWN;
    }

    if (isGameplayRectMap(ctr_rectMap.main) && KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_DUP)) {
        if (shoulderZoomHere) manualScaleFactor = getRectMapScaleFactor();
        manualScaleFactor += 0.2f; // zoom in
        if (manualScaleFactor > 1.0f) {
            manualScaleFactor = 1.0f;
        }
        key_combi_flags |= FLAG_SELECT_DUP;
    }

    if (KEY_RELEASED(KEY_START)) {
        if (key_combi_flags) {
            key_combi_flags = FLAG_NONE;
        } else {
            if ((ctr_rectMap.active != DISPLAY_MOVIE) &&
                    (ctr_rectMap.active != DISPLAY_MAIN) &&
                    (ctr_rectMap.active != DISPLAY_FULL)) {
//                GNW_add_input_buffer(KEY_ESCAPE);
            }
        }
    }

    if (KEY_RELEASED(KEY_SELECT)) {
        if (key_combi_flags) {
            key_combi_flags = FLAG_NONE;
        } else {
            if (ctr_rectMap.active == DISPLAY_WORLDMAP) {
                toggleWorldMapScreens();
                ctr_gfx_invalidate();
            } else if (isGameplayRectMap(ctr_rectMap.main)) {
                ctr_input.mode = ((ctr_input.mode + 1) % DISPLAY_MODE_FULL_TOP);
                setActiveRectMap(DISPLAY_LAST);
            }
        }
    }

    if (key_combi_flags == FLAG_NONE) {
        const bool panUp = pan.up;
        const bool panDown = pan.down;
        const bool panLeft = pan.left;
        const bool panRight = pan.right;
        if (ctr_rectMap.active == DISPLAY_DIALOG_REVIEW) {
            if (KEY_PRESS(KEY_DUP)) enqueueInputEvent(KEY_ARROW_UP);
            if (KEY_PRESS(KEY_DDOWN)) enqueueInputEvent(KEY_ARROW_DOWN);
            if (KEY_PRESS(KEY_B)) enqueueInputEvent(KEY_ESCAPE);
            oldpad = ctr_input.frame.kHeld;
            return;
        }
        if (ctr_rectMap.active == DISPLAY_TOWN_ENTRANCES) {
            if (KEY_PRESS(KEY_DUP)) enqueueInputEvent(KEY_ARROW_UP);
            if (KEY_PRESS(KEY_DDOWN)) enqueueInputEvent(KEY_ARROW_DOWN);
            if (KEY_PRESS(KEY_A)) enqueueInputEvent(KEY_RETURN);
            if (KEY_PRESS(KEY_X)) enqueueInputEvent(kCtrWorldMapEnterOrToggleEntrances);
            if (KEY_PRESS(KEY_B)) enqueueInputEvent(kCtrWorldMapEnterOrToggleEntrances);
            if (KEY_PRESS(KEY_DLEFT)) enqueueInputEvent(2071);
            if (KEY_PRESS(KEY_DRIGHT)) enqueueInputEvent(2072);
            oldpad = ctr_input.frame.kHeld;
            return;
        }
        if (ctr_rectMap.active == DISPLAY_CHAR_OPTIONS || ctr_rectMap.active == DISPLAY_CHAR_FILE) {
            if (KEY_PRESS(KEY_B)) enqueueInputEvent(KEY_ESCAPE);
            if (KEY_PRESS(KEY_A)) enqueueInputEvent(ctr_rectMap.active == DISPLAY_CHAR_FILE ? 500 : KEY_RETURN);
            if (ctr_rectMap.active == DISPLAY_CHAR_FILE) {
                if (KEY_PRESS(KEY_DUP)) enqueueInputEvent(KEY_ARROW_UP);
                if (KEY_PRESS(KEY_DDOWN)) enqueueInputEvent(KEY_ARROW_DOWN);
            }
            oldpad = ctr_input.frame.kHeld;
            return;
        }
        if (ctr_rectMap.active == DISPLAY_CHAR) {
            if (KEY_HELD(KEY_DUP)) scrollCharacterView(-6);
            if (KEY_HELD(KEY_DDOWN)) scrollCharacterView(6);
            if (KEY_HELD(KEY_R)) *buttons |= MOUSE_STATE_LEFT_BUTTON_DOWN;
            if (KEY_PRESS(KEY_L)) *buttons |= MOUSE_STATE_RIGHT_BUTTON_DOWN;
            oldpad = ctr_input.frame.kHeld;
            return;
        }
        if (ctr_rectMap.active == DISPLAY_WORLDMAP) {
            // X enters the current city, or toggles entrances in the town-map loop.
            if (KEY_PRESS(KEY_X)) enqueueInputEvent(kCtrWorldMapEnterOrToggleEntrances);
            // Alpha 17: D-pad always pans the world map; city list remains touch/cursor controlled.
            // Circle Pad can reach both panels. Y is the Town/World switch.
            if (KEY_PRESS(KEY_Y)) enqueueInputEvent(KEY_UPPERCASE_T);
            static u32 previousDirection = 0;
            static unsigned int lastPan = 0;
            const u32 direction = (panUp ? static_cast<u32>(KEY_DUP) : 0u)
                | (panDown ? static_cast<u32>(KEY_DDOWN) : 0u)
                | (panLeft ? static_cast<u32>(KEY_DLEFT) : 0u)
                | (panRight ? static_cast<u32>(KEY_DRIGHT) : 0u);
            const unsigned int now = getTicks();
            if (direction && (direction != previousDirection || now - lastPan >= 100u)) {
                if (independentPan) {
                    // One event per repeat, including diagonals. No list-focus events.
                    enqueueInputEvent(kCtrWorldMapPanBase + ctr_new_controls::panMask(pan));
                } else {
                    if (direction & KEY_DUP) enqueueInputEvent(KEY_ARROW_UP);
                    if (direction & KEY_DDOWN) enqueueInputEvent(KEY_ARROW_DOWN);
                    if (direction & KEY_DLEFT) enqueueInputEvent(KEY_ARROW_LEFT);
                    if (direction & KEY_DRIGHT) enqueueInputEvent(KEY_ARROW_RIGHT);
                }
                lastPan = now;
            }
            previousDirection = direction;
            if (KEY_HELD(KEY_A) || KEY_HELD(KEY_R)) *buttons |= MOUSE_STATE_LEFT_BUTTON_DOWN;
            oldpad = ctr_input.frame.kHeld;
            return;
        }
        if (ctr_rectMap.active == DISPLAY_PIPBOY) {
            if (KEY_HELD(KEY_DUP)) *delta_y -= 4;
            if (KEY_HELD(KEY_DDOWN)) *delta_y += 4;
            if (KEY_HELD(KEY_DLEFT)) *delta_x -= 4;
            if (KEY_HELD(KEY_DRIGHT)) *delta_x += 4;
            if (KEY_HELD(KEY_A) || KEY_HELD(KEY_R)) *buttons |= MOUSE_STATE_LEFT_BUTTON_DOWN;
            oldpad = ctr_input.frame.kHeld;
            return;
        }
        if (KEY_PRESS(KEY_L)) {
            *buttons |= MOUSE_STATE_RIGHT_BUTTON_DOWN;
        }

        if ((ctr_rectMap.active == DISPLAY_FULL) ||
                (ctr_rectMap.active == DISPLAY_FIELD) || (ctr_rectMap.active == DISPLAY_GUI) ||
                (ctr_rectMap.active == DISPLAY_WORLDMAP) || (ctr_rectMap.active == DISPLAY_CHAR_SELECT) ||
                (ctr_rectMap.active == DISPLAY_CHAR)) {
            if (KEY_HELD(KEY_R)) {
                *buttons |= MOUSE_STATE_LEFT_BUTTON_DOWN;
            }
        }

        if ((KEY_PRESS(KEY_A)) || (!shoulderZoomHere && (KEY_PRESS(KEY_ZL)))) {
            if ((ctr_rectMap.active == DISPLAY_FIELD) || (ctr_rectMap.active == DISPLAY_GUI)) {
                if (ctr_rectMap.active == DISPLAY_FIELD)
                    setActiveRectMap(DISPLAY_GUI);
                else
                    setActiveRectMap(DISPLAY_FIELD);
            }
        }

        if (KEY_PRESS(KEY_Y)) {
            // ctr_input.mode = ((ctr_input.mode + 1) % DISPLAY_MODE_LAST);
        }

        if (KEY_PRESS(KEY_X)) {
            // currentInput = ((currentInput + 1) % (INPUT_LAST));
        }

        if (independentPan && cStickPansHere()) {
            const int dx = ctr_new_controls::axisDirection(panLeft, panRight);
            const int dy = ctr_new_controls::axisDirection(panUp, panDown);
            if (adaptedGameplay()) {
                auto* view = rectMaps[DISPLAY_FIELD][0];
                const auto px = ctr_new_controls::panAxis(static_cast<int>(view->src_x),
                    static_cast<int>(offsetX_field_scaled_max), dx);
                const auto py = ctr_new_controls::panAxis(static_cast<int>(view->src_y),
                    static_cast<int>(offsetY_field_scaled_max), dy);
                offsetX_field = view->src_x = px.offset;
                offsetY_field = view->src_y = py.offset;
                // One combined call: mapScroll throttles consecutive calls, so
                // a vertical call used to starve the horizontal one.
                if (px.scroll || py.scroll) mapScroll(px.scroll, py.scroll);
                int x, y;
                mouseGetPosition(&x, &y);
                if (ctr_rectMap.active == DISPLAY_GUI && y >= 380) {
                    // Panning moves only the map, never the interface cursor.
                    *delta_x = ctr_new_controls::cursorDelta(x, *delta_x, 0, 1, screenGetWidth()-2);
                    *delta_y = ctr_new_controls::cursorDelta(y, *delta_y, 0, 380, screenGetHeight()-1);
                } else {
                    // Keep the same position on the LCD when the viewport moves.
                    // Stay inside the map, away from the engine edge-scroll trigger.
                    const int left = std::max(1, px.offset);
                    const int right = std::min(screenGetWidth()-2, px.offset + static_cast<int>(view->src_w)-1);
                    const int top = std::max(1, py.offset);
                    const int bottom = std::min(379, std::min(screenGetHeight()-2, py.offset + static_cast<int>(view->src_h)-1));
                    *delta_x = ctr_new_controls::cursorDelta(x, *delta_x, px.cursorShift, left, right);
                    *delta_y = ctr_new_controls::cursorDelta(y, *delta_y, py.cursorShift, top, bottom);
                }
                if (px.cursorShift || py.cursorShift) ctr_gfx_invalidate();
            } else if (ctr_rectMap.active == DISPLAY_FULL && (dx || dy)) {
                mapScroll(dx, dy);
            }
            oldpad = ctr_input.frame.kHeld;
            return;
        }

        if ((ctr_rectMap.active == DISPLAY_FIELD) || (ctr_rectMap.active == DISPLAY_GUI)) {
            offsetX_field = rectMaps[DISPLAY_FIELD][0]->src_x;
            offsetY_field = rectMaps[DISPLAY_FIELD][0]->src_y;

            if (panUp) {
                if (offsetY_field > 0) {
                    int deltaY = (offsetY_field >= 20) ? 20 : offsetY_field;
                    offsetY_field -= deltaY;
                    if (ctr_rectMap.active != DISPLAY_GUI) {
                        *delta_y -= deltaY;
                    } else {
                        int x, y;
                        mouseGetPosition(&x, &y);
                        if (y < 380) {
                            *delta_y -= deltaY;
                        }
                    }
                } else {
                    mapScroll(0, -1);
                }
            }
            if (panDown) {
                if (offsetY_field < offsetY_field_scaled_max) {
                    int deltaY = (offsetY_field_scaled_max - offsetY_field >= 20) ?
                                 20 : offsetY_field_scaled_max - offsetY_field;
                    offsetY_field += deltaY;
                    if (ctr_rectMap.active != DISPLAY_GUI) {
                        *delta_y += deltaY;
                    } else {
                        int x, y;
                        mouseGetPosition(&x, &y);
                        if (y < 380) {
                            *delta_y += deltaY;
                        }
                    }
                } else {
                    mapScroll(0, 1);
                }
            }
            if (panLeft) {
                if (offsetX_field > 0) {
                    int deltaX = (offsetX_field >= 20) ? 20 : offsetX_field;
                    offsetX_field -= deltaX;
                    if (ctr_rectMap.active != DISPLAY_GUI) {
                        *delta_x -= deltaX;
                    } else {
                        int x, y;
                        mouseGetPosition(&x, &y);
                        if (y < 380) {
                            *delta_x -= deltaX;
                        }
                    }
                } else {
                    mapScroll(-1, 0);
                }
            }
            if (panRight) {
                if (offsetX_field < offsetX_field_scaled_max) {
                    int deltaX = (offsetX_field_scaled_max - offsetX_field >= 20) ?
                                 20 : offsetX_field_scaled_max - offsetX_field;
                    offsetX_field += deltaX;
                    if (ctr_rectMap.active != DISPLAY_GUI) {
                        *delta_x += deltaX;
                    } else {
                        int x, y;
                        mouseGetPosition(&x, &y);
                        if (y < 380) {
                            *delta_x += deltaX;
                        }
                    }
                } else {
                    mapScroll(1, 0);
                }
            }
            rectMaps[DISPLAY_FIELD][0]->src_x = offsetX_field;
            rectMaps[DISPLAY_FIELD][0]->src_y = offsetY_field;
        } else {
            if (panUp) {
                mapScroll(0, -1);
//                *buttons |= KEY_ARROW_UP;
            }
            if (panDown) {
                mapScroll(0, 1);
//                *buttons |= KEY_ARROW_DOWN;
            }
            if (panLeft) {
                mapScroll(-1, 0);
            }
            if (panRight) {
                mapScroll(1, 0);
            }
        }
    }
    oldpad = ctr_input.frame.kHeld;
}

int ctr_input_frame(void)
{
    if (ctr_rectMap.active == DISPLAY_CONTROLS_HELP) {
        oldpad = ctr_input.frame.kHeld;
        return 1;
    }
    if (ctr_rectMap.active == DISPLAY_PAUSE && KEY_PRESS(KEY_X)) {
        enqueueInputEvent(505);
    }
    int delta_x = 0;
    int delta_y = 0;
    int buttons = 0;

    input_frame_touch();
    const auto pan = readPanDirections();
    const bool independentPan = cStickPansHere() && ctr_new_controls::any(pan)
        && key_combi_flags == FLAG_NONE && !KEY_HELD(KEY_SELECT) && !KEY_HELD(KEY_START);
    input_frame_axis(&delta_x, &delta_y, independentPan);
    input_frame_buttons(&delta_x, &delta_y, &buttons, pan, independentPan);

    if (ctr_rectMap.active == DISPLAY_WORLDMAP && (delta_x || delta_y)) {
        // Let the Circle Pad reach both the map and its control panel, without
        // teleporting back to the map after a touch or D-pad selection.
        const auto* r = rectMaps[DISPLAY_WORLDMAP_TOP][0];
        const int originX = static_cast<int>(r->src_x) - 22;
        const int originY = static_cast<int>(r->src_y) - 21;
        int x, y; mouseGetPosition(&x, &y);
        const int nextX = std::clamp(x + delta_x, originX + 22, originX + 639);
        const bool onMap = nextX < originX + 472;
        const int nextY = std::clamp(y + delta_y, originY + (onMap ? 21 : 0), originY + (onMap ? 463 : 479));
        delta_x = nextX - x;
        delta_y = nextY - y;
    }
    if (ctr_rectMap.active == DISPLAY_PIPBOY && (delta_x || delta_y)) {
        int x, y; mouseGetPosition(&x, &y);
        const int left = (screenGetWidth()-640)/2 + 254;
        const int top = (screenGetHeight()-480)/2 + 46;
        if (x < left || x >= left+374 || y < top || y >= top+410) {
            mouseHideCursor(); _mouse_set_position(left+187, top+205); mouseShowCursor();
        }
        mouseGetPosition(&x, &y);
        if (x+delta_x < left) delta_x = left-x;
        if (x+delta_x >= left+374) delta_x = left+373-x;
        if (y+delta_y < top) delta_y = top-y;
        if (y+delta_y >= top+410) delta_y = top+409-y;
    }
    if (independentPan || (delta_x != 0) || (delta_y != 0) || (buttons != 0)) {
        // Consume hardware pan frames even if the cursor is stationary; don't
        // let SDL's relative-mouse fallback add another cursor movement.
        _mouse_simulate_input(delta_x, delta_y, buttons);
        return 1;
    }

    return 0;
}

bool ctr_input_key_pressed(void)
{
    bool pressed = (ctr_input.frame.kHeld && (!(oldpad)));
    oldpad = ctr_input.frame.kHeld;

    return pressed;
}
/*
void to_lowercase(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

void swkbd_about_lookup_word(void)
{
    Script* scr;

    int message_list_id;
    int message_id;
    char* str;

    num_words = 0;

    while (num_words < MAX_DICT_WORDS) {
        if (scr_ptr(dialog_target->sid, &scr) != -1) {
            message_list_id = scr->scr_script_idx + 1;
            for (message_id = 1000; message_id < 1100; message_id++) {
                str = scr_get_msg_str(message_list_id, message_id);

                if (str != NULL && compat_stricmp(str, "Error") != 0) {
                    to_lowercase(str);
#ifdef _DEBUG
                    debug_printf("about_lookup_word: %s\n", str);
#endif
                    swkbdSetDictWord(&words[num_words], str, str);
                    num_words++;
                }
            }
        }
        message_list_id = 1;
        for (message_id = 600 * map_data.field_34 + 1000; message_id < 600 * map_data.field_34 + 1100; message_id++) {
            str = scr_get_msg_str(message_list_id, message_id);

            if (str != NULL && compat_stricmp(str, "Error") != 0) {
                to_lowercase(str);
#ifdef _DEBUG
                debug_printf("about_lookup_word: %s\n", str);
#endif
                swkbdSetDictWord(&words[num_words], str, str);
                num_words++;
            }
        }
        break;
    }
}

void ctr_input_swkdb_init(void)
{
    if (dictId == dialog_target->sid)
        return;

    dictId = dialog_target->sid;
    swkbd_about_lookup_word();
}
*/
// Fallout's legacy text fields accept single-byte ASCII, not UTF-8 sequences.
struct TextInputRules {
    int maxLength;
    bool dosFilename;
};

static bool validKeyboardText(const TextInputRules& rules, const char* text, size_t length)
{
    if (length > static_cast<size_t>(rules.maxLength)) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        unsigned char ch = text[i];
        if (ch < ' ' || ch > 'z' || (rules.dosFilename && !_isdoschar(ch))) {
            return false;
        }
    }
    return true;
}

static SwkbdCallbackResult validateKeyboardText(void* user, const char** message, const char* text, size_t length)
{
    const TextInputRules& rules = *static_cast<const TextInputRules*>(user);
    if (!validKeyboardText(rules, text, length)) {
        *message = rules.dosFilename ? "Use a valid DOS filename (up to 8 characters)." : "Use English letters, numbers or supported punctuation.";
        return SWKBD_CALLBACK_CONTINUE;
    }
    return SWKBD_CALLBACK_OK;
}

bool ctr_input_edit_text(const char* hintText, char* text, int maxLength, bool allowEmpty, bool dosFilename)
{
    if (maxLength < 1 || maxLength > 255) {
        return false;
    }
    TextInputRules rules = { maxLength, dosFilename };
    char initial[256] = {};
    strncpy(initial, text, maxLength);
    // Allow editing a legacy name even if its previous encoding is unsupported.
    if (!validKeyboardText(rules, initial, strlen(initial))) {
        initial[0] = '\0';
    }
    char result[256 * 3 + 1] = {};
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, maxLength);
    swkbdSetValidation(&swkbd, allowEmpty ? SWKBD_ANYTHING : SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetInitialText(&swkbd, initial);
    swkbdSetHintText(&swkbd, hintText);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Done", true);
    swkbdSetFilterCallback(&swkbd, validateKeyboardText, &rules);

    renderPresent();
    SwkbdButton button = swkbdInputText(&swkbd, result, sizeof(result));
    // Synchronize physical button state after the modal applet exits.
    hidScanInput();
    ctr_input.frame.kHeld = hidKeysHeld();
    oldpad = ctr_input.frame.kHeld;
    SDL_FlushEvents(SDL_KEYDOWN, SDL_TEXTINPUT);
    // The applet owns the displays while open; force the next game frame.
    ctr_gfx_invalidate();
    size_t length = strlen(result);
    if (button != SWKBD_BUTTON_RIGHT || (!allowEmpty && length == 0)
        || !validKeyboardText(rules, result, length)) {
        return false;
    }
    memcpy(text, result, length + 1);
    return true;
}

void ctr_input_init(void)
{
    newModel = false;
    bool detectedNew = false;
    if (R_SUCCEEDED(APT_CheckNew3DS(&detectedNew))) newModel = detectedNew;
    newShoulderZoom = 1;
    newCStickPan = 1;
    if (newModel) {
        configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, "N3DSShoulderZoom", &newShoulderZoom);
        configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, "N3DSCStickPan", &newCStickPan);
    }
    cursorDivisor = 10;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, "O3DSCursorDivisor", &cursorDivisor);
    if (cursorDivisor < 4) cursorDivisor = 4;
    if (cursorDivisor > 20) cursorDivisor = 20;
    ctr_input.input = INPUT_TOUCH;
    previousSliderState = osGet3DSliderState();

    if (setRectMapScaled(true, 0) == 0) {
        mouseHideCursor();
        _mouse_set_position(offsetX_field + (rectMaps[DISPLAY_FIELD][0]->src_w / 2),
                offsetY_field + (rectMaps[DISPLAY_FIELD][0]->src_h / 2));
        mouseShowCursor();
    }
}

void ctr_input_exit(void)
{
    return;
}

} // namespace fallout
