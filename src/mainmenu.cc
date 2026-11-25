#include "mainmenu.h"

#include <ctype.h>
#include <initializer_list>

#include "art.h"
#include "color.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "palette.h"
#include "preferences.h"
#include "sfall_config.h"
#include "svga.h"
#include "text_font.h"
#include "version.h"
#include "window_manager.h"

#ifdef __3DS__
#include "memory.h"
#include "debug.h"
#include "settings.h"
#include "platform/ctr/ctr_main_menu_layout.h"
#include "platform/ctr/ctr_mod_profiles.h"
#include "platform/ctr/ctr_gfx.h"
#include "platform/ctr/ctr_original_menu.h"
#endif

namespace fallout {

#define MAIN_MENU_WINDOW_WIDTH 640
#define MAIN_MENU_WINDOW_HEIGHT 480

typedef enum MainMenuButton {
    MAIN_MENU_BUTTON_INTRO,
    MAIN_MENU_BUTTON_NEW_GAME,
    MAIN_MENU_BUTTON_LOAD_GAME,
    MAIN_MENU_BUTTON_OPTIONS,
    MAIN_MENU_BUTTON_CREDITS,
    MAIN_MENU_BUTTON_EXIT,
    MAIN_MENU_BUTTON_COUNT,
} MainMenuButton;

static int main_menu_fatal_error();
static void main_menu_play_sound(const char* fileName);

// 0x5194F0
static int gMainMenuWindow = -1;

// 0x5194F4
static unsigned char* gMainMenuWindowBuffer = nullptr;

// 0x519504
static bool _in_main_menu = false;

// 0x519508
static bool gMainMenuWindowInitialized = false;

// 0x51950C
static unsigned int gMainMenuScreensaverDelay = 120000;

// 0x519510
static const int gMainMenuButtonKeyBindings[MAIN_MENU_BUTTON_COUNT] = {
    KEY_LOWERCASE_I, // intro
    KEY_LOWERCASE_N, // new game
    KEY_LOWERCASE_L, // load game
    KEY_LOWERCASE_O, // options
    KEY_LOWERCASE_C, // credits
    KEY_LOWERCASE_E, // exit
};

// 0x519528
static const int _return_values[MAIN_MENU_BUTTON_COUNT] = {
    MAIN_MENU_INTRO,
    MAIN_MENU_NEW_GAME,
    MAIN_MENU_LOAD_GAME,
    MAIN_MENU_OPTIONS,
    MAIN_MENU_CREDITS,
    MAIN_MENU_EXIT,
};

// 0x614840
static int gMainMenuButtons[MAIN_MENU_BUTTON_COUNT];

// 0x614858
static bool gMainMenuWindowHidden;

static FrmImage _mainMenuBackgroundFrmImage;
static FrmImage _mainMenuButtonNormalFrmImage;
static FrmImage _mainMenuButtonPressedFrmImage;
#ifdef __3DS__
// GNW borrows these images until the menu window is destroyed. Allocate
// only while the menu exists, so gameplay has no extra buffer overhead.
static unsigned char* gCtrMenuRows = nullptr;
static_assert(MAIN_MENU_BUTTON_COUNT == ctr_menu::RowCount);
#endif

// 0x481650
int mainMenuWindowInit()
{
    int fid;
    MessageListItem msg;
    int len;

    if (gMainMenuWindowInitialized) {
        return 0;
    }

    colorPaletteLoad("color.pal");

    int mainMenuWindowX = (screenGetWidth() - MAIN_MENU_WINDOW_WIDTH) / 2;
    int mainMenuWindowY = (screenGetHeight() - MAIN_MENU_WINDOW_HEIGHT) / 2;
    gMainMenuWindow = windowCreate(mainMenuWindowX,
        mainMenuWindowY,
        MAIN_MENU_WINDOW_WIDTH,
        MAIN_MENU_WINDOW_HEIGHT,
        0,
        WINDOW_HIDDEN | WINDOW_MOVE_ON_TOP);
    if (gMainMenuWindow == -1) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    gMainMenuWindowBuffer = windowGetBuffer(gMainMenuWindow);

    // v10.5: The stock left-side metal panels are required for the 3DS
    // split layout. Bypass patch/localized art resolution for this asset only.
    bool originalBackground = false;
#ifdef __3DS__
    originalBackground = ctr_menu::loadOriginalBackground(settings.system.master_dat_path.c_str(),
        gMainMenuWindowBuffer, MAIN_MENU_WINDOW_WIDTH * MAIN_MENU_WINDOW_HEIGHT);
    debugPrint("\n3DS main menu: %s\n", originalBackground ? "MASTER.DAT original artwork" : "normal art fallback");
#endif
    if (!originalBackground) {
        // Preserve normal art loading if the original entry is unavailable.
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 140, 0, 0, 0);
        if (!_mainMenuBackgroundFrmImage.lock(backgroundFid)) {
            return main_menu_fatal_error();
        }
        if (_mainMenuBackgroundFrmImage.getWidth() != 640 || _mainMenuBackgroundFrmImage.getHeight() != 480) {
            _mainMenuBackgroundFrmImage.unlock();
            return main_menu_fatal_error();
        }
        blitBufferToBuffer(_mainMenuBackgroundFrmImage.getData(), 640, 480, 640, gMainMenuWindowBuffer, 640);
        _mainMenuBackgroundFrmImage.unlock();
    }

    int oldFont = fontGetCurrent();
    fontSetCurrent(100);

    // SFALL: Allow to change font color/flags of copyright/version text
    //        It's the last byte ('3C' by default) that picks the colour used. The first byte supplies additional flags for this option
    //        0x010000 - change the color for version string only
    //        0x020000 - underline text (only for the version string)
    //        0x040000 - monospace font (only for the version string)
    int fontSettings = _colorTable[21091], fontSettingsSFall = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_FONT_COLOR_KEY, &fontSettingsSFall);
    if (fontSettingsSFall && !(fontSettingsSFall & 0x010000))
        fontSettings = fontSettingsSFall & 0xFF;

    // SFALL: Allow to move copyright text
    int offsetX = 0, offsetY = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_X_KEY, &offsetX);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_Y_KEY, &offsetY);

    // Copyright.
    msg.num = 20;
    if (messageListGetItem(&gMiscMessageList, &msg)) {
        windowDrawText(gMainMenuWindow, msg.text, 0, offsetX + 15, offsetY + 460, fontSettings | 0x06000000);
    }

    // SFALL: Make sure font settings are applied when using 0x010000 flag
    if (fontSettingsSFall)
        fontSettings = fontSettingsSFall;

    // TODO: Allow to move version text
    // Version.
    char version[VERSION_MAX];
    versionGetVersion(version, sizeof(version));
    len = fontGetStringWidth(version);
    windowDrawText(gMainMenuWindow, version, 0, 615 - len, 460, fontSettings | 0x06000000);

    // menuup.frm
    fid = buildFid(OBJ_TYPE_INTERFACE, 299, 0, 0, 0);
    if (!_mainMenuButtonNormalFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    // menudown.frm
    fid = buildFid(OBJ_TYPE_INTERFACE, 300, 0, 0, 0);
    if (!_mainMenuButtonPressedFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = -1;
    }

#ifdef __3DS__
    // Fixed geometry matches the original plates and both screen mappings.
    // Old ddraw.ini offsets cannot move these rows off their artwork.
    using namespace ctr_menu;
    constexpr int rowBytes = RowWidth * RowHeight;
    gCtrMenuRows = static_cast<unsigned char*>(internal_malloc(RowCount * 2 * rowBytes));
    if (gCtrMenuRows == nullptr
        || _mainMenuButtonNormalFrmImage.getWidth() != LampSize
        || _mainMenuButtonNormalFrmImage.getHeight() != LampSize
        || _mainMenuButtonPressedFrmImage.getWidth() != LampSize
        || _mainMenuButtonPressedFrmImage.getHeight() != LampSize) {
        fontSetCurrent(oldFont);
        return main_menu_fatal_error();
    }

    fontSettings = _colorTable[21091];
    fontSettingsSFall = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_BIG_FONT_COLOR_KEY, &fontSettingsSFall);
    if (fontSettingsSFall)
        fontSettings = fontSettingsSFall & 0xFF;

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; ++index) {
        unsigned char* normal = gCtrMenuRows + index * 2 * rowBytes;
        unsigned char* pressed = normal + rowBytes;
        unsigned char* source = gMainMenuWindowBuffer + rowY(index) * MAIN_MENU_WINDOW_WIDTH + RowX;
        blitBufferToBuffer(source, RowWidth, RowHeight, MAIN_MENU_WINDOW_WIDTH, normal, RowWidth);
        msg.num = 9 + index;
        const char* label = index == MAIN_MENU_BUTTON_OPTIONS ? "OPTIONS / MODS" : (messageListGetItem(&gMiscMessageList, &msg) ? msg.text : nullptr);
        if (label != nullptr) {
            // Prefer original large lettering, with smaller interface fonts
            // as a fallback for long/localized labels.
            for (int font : {104, 103, 102, 101, 100, 0}) {
                fontSetCurrent(font);
                len = fontGetStringWidth(label);
                int h = fontGetLineHeight();
                if (h <= 0 || h > RowHeight - 2 || len <= 0 || len > LabelRight - LabelLeft)
                    continue;
                int x = LabelLeft + (LabelRight - LabelLeft - len) / 2;
                int y = (RowHeight - h) / 2;
                fontDrawText(normal + y * RowWidth + x, label, LabelRight - x, RowWidth, fontSettings);
                break;
            }
        }
        blitBufferToBuffer(normal, RowWidth, RowHeight, RowWidth, pressed, RowWidth);
        blitBufferToBufferTrans(_mainMenuButtonNormalFrmImage.getData(), LampSize, LampSize, LampSize,
            normal + LampY * RowWidth + LampX, RowWidth);
        blitBufferToBufferTrans(_mainMenuButtonPressedFrmImage.getData(), LampSize, LampSize, LampSize,
            pressed + LampY * RowWidth + LampX, RowWidth);

        // The whole metal plate (not just the tiny red lamp) is tappable.
        gMainMenuButtons[index] = buttonCreate(gMainMenuWindow,
            RowX, rowY(index), RowWidth, RowHeight, -1, -1, 1111,
            gMainMenuButtonKeyBindings[index], normal, pressed, nullptr, 0);
        if (gMainMenuButtons[index] == -1) {
            fontSetCurrent(oldFont);
            return main_menu_fatal_error();
        }
    }
#else
    // SFALL: Allow to move menu buttons
    offsetX = offsetY = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_X_KEY, &offsetX);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_Y_KEY, &offsetY);

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = buttonCreate(gMainMenuWindow,
            offsetX + 30,
            offsetY + 19 + index * 42 - index,
            26,
            26,
            -1,
            -1,
            1111,
            gMainMenuButtonKeyBindings[index],
            _mainMenuButtonNormalFrmImage.getData(),
            _mainMenuButtonPressedFrmImage.getData(),
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (gMainMenuButtons[index] == -1) {
            // NOTE: Uninline.
            return main_menu_fatal_error();
        }

        buttonSetMask(gMainMenuButtons[index], _mainMenuButtonNormalFrmImage.getData());
    }

    fontSetCurrent(104);

    // SFALL: Allow to change font color of buttons
    fontSettings = _colorTable[21091];
    fontSettingsSFall = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_BIG_FONT_COLOR_KEY, &fontSettingsSFall);
    if (fontSettingsSFall)
        fontSettings = fontSettingsSFall & 0xFF;

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        msg.num = 9 + index;
        if (messageListGetItem(&gMiscMessageList, &msg) && msg.text != nullptr) {
            len = fontGetStringWidth(msg.text);
            fontDrawText(gMainMenuWindowBuffer + offsetX + 640 * (offsetY + 42 * index - index + 20) + 126 - (len / 2), msg.text, 640 - (126 - (len / 2)) - 1, 640, fontSettings);
        }
    }

#endif

    fontSetCurrent(oldFont);

    gMainMenuWindowInitialized = true;
    gMainMenuWindowHidden = true;

    return 0;
}

// 0x481968
void mainMenuWindowFree()
{
    if (!gMainMenuWindowInitialized && gMainMenuWindow == -1) {
        return;
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        // The window owns its buttons and destroys them in windowDestroy.
        gMainMenuButtons[index] = -1;
    }

    if (gMainMenuWindow != -1) {
        windowDestroy(gMainMenuWindow);
        gMainMenuWindow = -1;
    }

#ifdef __3DS__
    if (gCtrMenuRows != nullptr) {
        internal_free(gCtrMenuRows);
        gCtrMenuRows = nullptr;
    }
#endif
    _mainMenuButtonPressedFrmImage.unlock();
    _mainMenuButtonNormalFrmImage.unlock();

    gMainMenuWindowBuffer = nullptr;
    gMainMenuWindowInitialized = false;
}

// 0x481A00
void mainMenuWindowHide(bool animate)
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    if (gMainMenuWindowHidden) {
        return;
    }

    soundContinueAll();

    if (animate) {
        paletteFadeTo(gPaletteBlack);
        soundContinueAll();
    }

    windowHide(gMainMenuWindow);

    gMainMenuWindowHidden = true;
}

// 0x481A48
void mainMenuWindowUnhide(bool animate)
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    if (!gMainMenuWindowHidden) {
        return;
    }

    windowShow(gMainMenuWindow);

    if (animate) {
        colorPaletteLoad("color.pal");
        paletteFadeTo(_cmap);
    }

#ifdef __3DS__
    if (ctrModProfileActive()) {
        ctrModProfileStage("Refreshing mod main menu and invalidating render cache");
        windowRefresh(gMainMenuWindow);
        ctr_gfx_invalidate();
        renderPresent();
    }
#endif
    gMainMenuWindowHidden = false;
}

// 0x481AA8
int _main_menu_is_enabled()
{
    return 1;
}

// 0x481AEC
int mainMenuWindowHandleEvents()
{
    _in_main_menu = true;

    bool oldCursorIsHidden = cursorIsHidden();
#ifdef __3DS__
    // Keep the initial cursor off the helmet. Touch input moves it normally.
    mouseHideCursor();
    _mouse_set_position(ctr_menu::RowX + ctr_menu::RowWidth / 2, ctr_menu::rowY(0) + ctr_menu::RowHeight / 2);
    if (!oldCursorIsHidden) {
        mouseShowCursor();
    }
#endif
    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    unsigned int tick = getTicks();

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
#ifdef __3DS__
        if (_game_user_wants_to_quit == 3) { rc = MAIN_MENU_EXIT; break; }
#endif

        for (int buttonIndex = 0; buttonIndex < MAIN_MENU_BUTTON_COUNT; buttonIndex++) {
            if (keyCode == gMainMenuButtonKeyBindings[buttonIndex] || keyCode == toupper(gMainMenuButtonKeyBindings[buttonIndex])) {
                // NOTE: Uninline.
                main_menu_play_sound("nmselec1");

                rc = _return_values[buttonIndex];

                if (buttonIndex == MAIN_MENU_BUTTON_CREDITS && (gPressedPhysicalKeys[SDL_SCANCODE_RSHIFT] != KEY_STATE_UP || gPressedPhysicalKeys[SDL_SCANCODE_LSHIFT] != KEY_STATE_UP)) {
                    rc = MAIN_MENU_QUOTES;
                }

                break;
            }
        }

        if (rc == -1) {
            if (keyCode == KEY_CTRL_R) {
                rc = MAIN_MENU_SELFRUN;
                continue;
            } else if (keyCode == KEY_PLUS || keyCode == KEY_EQUAL) {
                brightnessIncrease();
            } else if (keyCode == KEY_MINUS || keyCode == KEY_UNDERSCORE) {
                brightnessDecrease();
            } else if (keyCode == KEY_UPPERCASE_D || keyCode == KEY_LOWERCASE_D) {
                rc = MAIN_MENU_SCREENSAVER;
                continue;
            } else if (keyCode == 1111) {
                if (!(mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT)) {
                    // NOTE: Uninline.
                    main_menu_play_sound("nmselec0");
                }
                continue;
            }
        }

        if (keyCode == KEY_ESCAPE || _game_user_wants_to_quit == 3) {
            rc = MAIN_MENU_EXIT;

            // NOTE: Uninline.
            main_menu_play_sound("nmselec1");
            break;
        } else if (_game_user_wants_to_quit == 2) {
            _game_user_wants_to_quit = 0;
        } else {
            if (getTicksSince(tick) >= gMainMenuScreensaverDelay) {
                rc = MAIN_MENU_TIMEOUT;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (oldCursorIsHidden) {
        mouseHideCursor();
    }

    _in_main_menu = false;

    return rc;
}

// NOTE: Inlined.
//
// 0x481C88
static int main_menu_fatal_error()
{
    mainMenuWindowFree();

    return -1;
}

// NOTE: Inlined.
//
// 0x481C94
static void main_menu_play_sound(const char* fileName)
{
    soundPlayFile(fileName);
}

} // namespace fallout
