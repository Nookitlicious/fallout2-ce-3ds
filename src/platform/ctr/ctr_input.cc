#include <string.h>
#include <stdio.h>

#include "ctr_input.h"
#include "ctr_mod_profiles.h"
#include "game.h"
#include "ctr_gfx.h"
#include "ctr_rectmap.h"
#include "ctr_main_menu_layout.h"
//#include "ctr_sys.h"

#include "platform_compat.h"
#include "sfall_config.h"

//#include "plib/gnw/gnw.h"
#include "svga.h"
#include "input.h"
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
        case DISPLAY_MAIN:
            ctr_menu::mapTouch(ctr_input.frame.touchX, ctr_input.frame.touchY, newX, newY);
            break;
        case DISPLAY_FULL:
            *newX = (ctr_input.frame.touchX * screenGetWidth()) / 320;
            *newY = (ctr_input.frame.touchY * screenGetHeight()) / 240;
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

void input_frame_axis(int *delta_x, int *delta_y)
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

        // Moderate Circle Pad speed for accurate O3DS cursor targeting; C-stick unchanged.
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
                if (y > 380) {
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
            if (!isAgeWindow && !isSexWindow) {
                offsetY_char += moveY;

                if (offsetY_char < 0)
                    offsetY_char=0;

                if (offsetY_char > 525)
                    offsetY_char=525;
            }
        } else {
            *delta_x += moveX;
            *delta_y += moveY;
        }
    }
}

void input_frame_buttons(int *delta_x, int *delta_y, int *buttons)
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
    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_DDOWN)) {
        manualScaleFactor -= 0.2f; // zoom out
        if (manualScaleFactor < 0.0f) {
            manualScaleFactor = 0.0f;
        }
        key_combi_flags |= FLAG_SELECT_DDOWN;
    }

    if (KEY_HELD(KEY_SELECT) && KEY_PRESS(KEY_DUP)) {
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
            if (isGameplayRectMap(ctr_rectMap.main)) {
                ctr_input.mode = ((ctr_input.mode + 1) % DISPLAY_MODE_FULL_TOP);
                setActiveRectMap(DISPLAY_LAST);
            }
        }
    }

    if (key_combi_flags == FLAG_NONE) {
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

        if ((KEY_PRESS(KEY_A)) || (KEY_PRESS(KEY_ZL))) {
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

        if ((ctr_rectMap.active == DISPLAY_FIELD) || (ctr_rectMap.active == DISPLAY_GUI)) {
            offsetX_field = rectMaps[DISPLAY_FIELD][0]->src_x;
            offsetY_field = rectMaps[DISPLAY_FIELD][0]->src_y;

            if (KEY_HELD(KEY_DUP)) {
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
            if (KEY_HELD(KEY_DDOWN)) {
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
            if (KEY_HELD(KEY_DLEFT)) {
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
            if (KEY_HELD(KEY_DRIGHT)) {
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
            if (KEY_HELD(KEY_DUP)) {
                mapScroll(0, -1);
//                *buttons |= KEY_ARROW_UP;
            }
            if (KEY_HELD(KEY_DDOWN)) {
                mapScroll(0, 1);
//                *buttons |= KEY_ARROW_DOWN;
            }
            if (KEY_HELD(KEY_DLEFT)) {
                mapScroll(-1, 0);
            }
            if (KEY_HELD(KEY_DRIGHT)) {
                mapScroll(1, 0);
            }
        }
    }
    oldpad = ctr_input.frame.kHeld;
}

int ctr_input_frame(void)
{
    int delta_x = 0;
    int delta_y = 0;
    int buttons = 0;

    input_frame_touch();
    input_frame_axis(&delta_x, &delta_y);
    input_frame_buttons(&delta_x, &delta_y, &buttons);

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
    if ((delta_x != 0) || (delta_y != 0) || (buttons != 0)) {
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
