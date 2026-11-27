#ifndef FALLOUT_CTR_METAL_MENU_H
#define FALLOUT_CTR_METAL_MENU_H

#include <algorithm>
#include <array>
#include <cstring>
#include <list>
#include <string>
#include <vector>
#include "art.h"
#include "color.h"
#include "draw.h"
#include "game_sound.h"
#include "obj_types.h"
#include "settings.h"
#include "text_font.h"
#include "window_manager.h"
#include "ctr_main_menu_layout.h"
#include "ctr_original_menu.h"

namespace fallout {
namespace ctr_menu {
// Keep button pixels alive until their owning engine window is destroyed.
// Draw in final-screen proportions before resampling to the engine viewport.
class MetalMenu {
    std::array<unsigned char, RowWidth * RowHeight> plate{};
    FrmImage lampUp, lampDown;
    bool havePlate = false;
    struct Images { std::vector<unsigned char> up, down; };
    std::list<Images> rows;

    static void scale(const unsigned char* src, int sw, int sh, int sp,
        unsigned char* dst, int dw, int dh, int dp, bool transparent = false) {
        for (int y = 0; y < dh; ++y) for (int x = 0; x < dw; ++x) {
            unsigned char pixel = src[(y * sh / dh) * sp + x * sw / dw];
            if (!transparent || pixel != 0) dst[y * dp + x] = pixel;
        }
    }

    void render(std::vector<unsigned char>& pixels, int w, int h,
        const char* label, bool selected, bool pressed, bool lamp)
    {
        pixels.resize(w * h);
        if (havePlate) {
            // Preserve metal end caps instead of widening the lamp recess.
            const int leftCap = std::min(44, w / 3), rightCap = std::min(8, w / 6);
            scale(plate.data(), 44, RowHeight, RowWidth, pixels.data(), leftCap, h, w);
            scale(plate.data() + 44, RowWidth - 52, RowHeight, RowWidth,
                pixels.data() + leftCap, w - leftCap - rightCap, h, w);
            scale(plate.data() + RowWidth - 8, 8, RowHeight, RowWidth,
                pixels.data() + w - rightCap, rightCap, h, w);
        } else {
            // Graceful fallback for total conversions with different artwork.
            std::fill(pixels.begin(), pixels.end(), _colorTable[10570]);
            bufferFill(pixels.data() + w + 1, w - 2, h - 2, w, _colorTable[0]);
        }
        int left = 5, right = w - 5;
        FrmImage& image = pressed || selected ? lampDown : lampUp;
        if (lamp && image.isLocked() && image.getWidth() > 0 && image.getHeight() > 0) {
            const int size = std::min({h - 6, 26, w / 4});
            if (size > 0) {
                const int cap = std::min(44, w / 3);
                const int lampX = std::clamp((LampX + LampSize / 2) * cap / 44 - size / 2, 0, cap - size);
                scale(image.getData(), image.getWidth(), image.getHeight(), image.getWidth(),
                    pixels.data() + ((h - size) / 2) * w + lampX, size, size, w, true);
                left = cap + 2;
            }
        }
        const int oldFont = fontGetCurrent();
        std::string text(label);
        // Smallest font is also used for unusually long translated names.
        for (int font : {104, 103, 102, 101, 100, 0}) {
            fontSetCurrent(font);
            if (fontGetLineHeight() > 0 && fontGetLineHeight() <= h - 4
                && fontGetStringWidth(text.c_str()) <= right - left) break;
        }
        while (!text.empty() && fontGetStringWidth(text.c_str()) > right - left) text.pop_back();
        const int th = fontGetLineHeight(), tw = fontGetStringWidth(text.c_str());
        if (th > 0 && th <= h - 4 && tw > 0 && tw <= right - left) {
            const int x = left + (right - left - tw) / 2;
            const int y = (h - th) / 2;
            fontDrawText(pixels.data() + y * w + x, text.c_str(), right - x, w,
                _colorTable[0]);
            // A light foreground and dark shadow remain readable on metal.
            if (x > 0 && y > 0) fontDrawText(pixels.data() + (y - 1) * w + x - 1,
                text.c_str(), right - x, w, _colorTable[selected || pressed ? 32747 : 21091]);
        }
        fontSetCurrent(oldFont);
        if (selected || pressed) bufferDrawRect(pixels.data(), w, 0, 0, w - 1, h - 1, _colorTable[32747]);
    }

    void make(std::vector<unsigned char>& out, int w, int h, int displayW, int displayH,
        const char* label, bool selected, bool pressed, bool lamp)
    {
        std::vector<unsigned char> display;
        render(display, displayW, displayH, label, selected, pressed, lamp);
        out.resize(w * h);
        scale(display.data(), displayW, displayH, displayW, out.data(), w, h, w);
    }
public:
    MetalMenu() {
        // The original game's main-menu plate is already used by the split main menu.
        std::vector<unsigned char> bg(640 * 480);
        havePlate = loadOriginalBackground(settings.system.master_dat_path.c_str(), bg.data(), bg.size());
        if (!havePlate) havePlate = loadOriginalBackground("sdmc:/3ds/fallout2/master.dat", bg.data(), bg.size());
        if (havePlate) {
            blitBufferToBuffer(bg.data() + rowY(0) * 640 + RowX, RowWidth, RowHeight, 640, plate.data(), RowWidth);
        } else {
            FrmImage fallback;
            if (fallback.lock(buildFid(OBJ_TYPE_INTERFACE, 140, 0, 0, 0))
                && fallback.getWidth() == 640 && fallback.getHeight() == 480) {
                blitBufferToBuffer(fallback.getData() + rowY(0) * 640 + RowX, RowWidth, RowHeight, 640, plate.data(), RowWidth);
                havePlate = true;
            }
        }
        lampUp.lock(buildFid(OBJ_TYPE_INTERFACE, 299, 0, 0, 0));
        lampDown.lock(buildFid(OBJ_TYPE_INTERFACE, 300, 0, 0, 0));
    }
    void clear() { rows.clear(); } // only after windowDestroy
    void heading(int win, int x, int y, int w, int h, int dw, int dh, const char* text) {
        std::vector<unsigned char> pixels;
        make(pixels, w, h, dw, dh, text, false, false, false);
        const int pitch = windowGetWidth(win);
        blitBufferToBuffer(pixels.data(), w, h, w, windowGetBuffer(win) + y * pitch + x, pitch);
    }
    int button(int win, int x, int y, int w, int h, int dw, int dh, int code, const char* text, bool selected = false, bool lamp = true) {
        rows.emplace_back();
        Images& images = rows.back();
        make(images.up, w, h, dw, dh, text, selected, false, lamp);
        make(images.down, w, h, dw, dh, text, selected, true, lamp);
        const int id = buttonCreate(win, x, y, w, h, -1, -1, -1, code,
            images.up.data(), images.down.data(), nullptr, 0);
        if (id != -1) buttonSetCallbacks(id, _gsound_red_butt_press, _gsound_red_butt_release);
        return id;
    }
};
}
}
#endif
