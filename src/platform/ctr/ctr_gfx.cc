#include <malloc.h>
#include <stdio.h>
#include <initializer_list>
#include <string.h>

#include "ctr_render_rows.h"
#include "ctr_scene_gate.h"
#include "o3ds_v9_options.h"

#include "ctr_input.h"
#include "ctr_gfx.h"
#include "ctr_mod_profiles.h"
#include "ctr_rectmap.h"
#include "ctr_main_menu_layout.h"
//#include "ctr_sys.h"

#include "vshader_shbin.h"
#include "frame_tex.h"

//#include "plib/gnw/gnw.h"
//#include "plib/gnw/memory.h"

namespace fallout {

ctr_gfx_t ctr_gfx;
ctr_rectMap_t ctr_rectMap;

static DVLB_s *vshader_dvlb;
static shaderProgram_s program;
static int8_t uLoc_projection;

static C3D_Mtx topProjection;
static C3D_Mtx bottomProjection;

static C3D_RenderTarget *topRenderTarget;
static C3D_RenderTarget *bottomRenderTarget;

static uint8_t *renderTextureData;
static const uint16_t renderTextureWidth = 1024;
static const uint16_t renderTextureHeight = 512;
static const uint32_t renderTextureStride = renderTextureWidth * 3;
static const uint32_t renderTextureByteCount = renderTextureStride * renderTextureHeight;

static CtrRenderRows renderRows;
static bool forceFullUpload;
static bool profileEnabled;
static uint32_t profileStart;
static uint32_t profileFrames;
static uint32_t profileUploads;
static uint32_t profileDrawCalls;
static uint32_t lastSceneDraw;
static bool sceneValid;
static bool rendererInitialized;
static unsigned int submittedFrames;
static bool zoneLoading = false;
static bool zoneLoadingPresented = false;
static SceneSnapshot lastScene;
// Only gameplay layouts use this fast path. Other layouts, movies and
// debug overlays always retain the V8 draw behavior. Compare geometry
// exactly, including the cropped/zoomed field and screen swapping.
static bool captureScene(SceneSnapshot& scene)
{
    if (ctr_rectMap.active != DISPLAY_GUI && ctr_rectMap.active != DISPLAY_FIELD) return false;
    if (ctr_gfx.overlayEnable || ctr_gfx.overlayEnableExtra || !rectMaps || !numRectsInMap) return false;
    if (numRectsInMap[DISPLAY_FIELD] != 1 || numRectsInMap[DISPLAY_GUI] != 3 || numRectsInMap[DISPLAY_GUI_INDICATOR] != 4) return false;
    scene.display = ctr_rectMap.active; scene.mode = ctr_input.mode;
    scene.indicators = getIndicatorSlotNum(); scene.wide = ctr_gfx.isWide;
    int n = 0;
    for (int group : {DISPLAY_FIELD, DISPLAY_GUI, DISPLAY_GUI_INDICATOR}) {
        if (!rectMaps[group]) return false;
        for (int i = 0; i < numRectsInMap[group]; ++i) {
            if (!rectMaps[group][i]) return false;
            scene.rects[n++] = *rectMaps[group][i];
        }
    }
    return true;
}
static uint32_t profileRows;
static uint32_t profilePossibleRows;
static uint32_t profileConvertMs;
static uint32_t profileUploadMs;
static uint32_t profileDrawMs;

static C3D_Tex render_tex;
static C3D_Tex static_tex;
static int activeTexFilter;

static bool isN3DS;
static bool targetTop;

static C2D_TextBuf overlayTextBuf;

bool beginRender(bool vSync);
void drawTopRenderTarget(uint32_t clearColor);
void drawBottomRenderTarget(uint32_t clearColor);
void finishRender();

bool drawRects();
void drawFrameRect();

static bool loadTextureFromMem(C3D_Tex* tex, C3D_TexCube* cube, const void* data, size_t size)
{
	Tex3DS_Texture t3x = Tex3DS_TextureImport(data, size, tex, cube, true);
	if (!t3x)
		return false;

	Tex3DS_TextureFree(t3x);
	return true;
}

static bool loadTextureFromFile(C3D_Tex* tex, C3D_TexCube* cube, const char* filePath)
{
	FILE* file = fopen(filePath, "rb");
	if (!file)
		return false;

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);

	void* data = malloc(size);
	if (!data) {
		fclose(file);
		return false;
	}

	size_t bytesRead = fread(data, 1, size, file);
	fclose(file);

	if (bytesRead != size) {
		free(data);
		return false;
	}

	bool result = loadTextureFromMem(tex, cube, data, size);

	free(data);

	return result;
}

#ifdef _DEBUG_OVERLAY
void show_overlay(void)
{
    if (!ctr_gfx.overlayEnable && !ctr_gfx.overlayEnableExtra)
        return;

    C2D_Prepare();
    C2D_SceneTarget(bottomRenderTarget);
    C2D_TextBufClear(overlayTextBuf);
    C2D_Text dynamicText;

    char fpsBuffer[32];
    char buffer[512];

    if ((ctr_gfx.overlayEnable) && (ctr_gfx.overlayEnableExtra)) {
        snprintf(fpsBuffer, sizeof(buffer), "FPS: %5.2f", ctr_sys_get_fps());
        snprintf(buffer, sizeof(buffer),
                "%s  MEM: %3.2f MB / %3.2f MB\n\ngpu: %5.2f%%  cpu: %5.2f%%  cmdBuf:%5.2f%%\nisWide: %s\nCur mem: %6d blocks, %9u bytes\nMax mem: %6d blocks, %9u bytes\n\nLinear Heap at start: %9.2f MB\nHeap at start:           %9.2f MB\nLinear Heap current : %9.2f MB\nHeap current :           %9.2f MB",
                fpsBuffer,get_mem_allocated() / (1024.0f * 1024.0f),get_max_allocated() / (1024.0f * 1024.0f),
                C3D_GetDrawingTime()*6, C3D_GetProcessingTime()*6, C3D_GetCmdBufUsage()*100, ctr_gfx.isWide?"true":"false",
                get_num_blocks(),get_mem_allocated(),get_max_blocks(),get_max_allocated(),
                linearHeapAvailableAtStart / (1024.0f * 1024.0f),
                heapAvailableAtStart / (1024.0f * 1024.0f),
                linearHeapAvailable / (1024.0f * 1024.0f),
                heapAvailable / (1024.0f * 1024.0f)
        );
        C2D_DrawRectSolid(10, 10, 0.9, 300, 25, C2D_Color32f(0, 1, 0, 0.5));
        C2D_DrawRectSolid(10, 42, 0.9, 300, 65, C2D_Color32f(1, 0, 1, 0.5));
		C2D_DrawRectSolid(10,114, 0.9, 300, 75, C2D_Color32f(0, 0, 1, 0.5));
    } else if (ctr_gfx.overlayEnable) {
        snprintf(fpsBuffer, sizeof(buffer), "FPS: %5.2f", ctr_sys_get_fps());
        snprintf(buffer, sizeof(buffer),
                "%s  MEM: %3.2f MB / %3.2f MB",
                fpsBuffer,get_mem_allocated() / (1024.0f * 1024.0f),get_max_allocated() / (1024.0f * 1024.0f));
        C2D_DrawRectSolid(10, 10, 0.9, 300, 25, C2D_Color32f(0, 1, 0, 0.5));
    } else if (ctr_gfx.overlayEnableExtra) {
        snprintf(buffer, sizeof(buffer),
                "gpu: %5.2f%%  cpu: %5.2f%%  cmdBuf:%5.2f%%\nisWide: %s\nCur mem: %6d blocks, %9u bytes\nMax mem: %6d blocks, %9u bytes\n\nLinear Heap at start: %9.2f MB\nHeap at start:           %9.2f MB\nLinear Heap current : %9.2f MB\nHeap current :           %9.2f MB",
                C3D_GetDrawingTime()*6, C3D_GetProcessingTime()*6, C3D_GetCmdBufUsage()*100, ctr_gfx.isWide?"true":"false",
                get_num_blocks(),get_mem_allocated(),get_max_blocks(),get_max_allocated(),
                linearHeapAvailableAtStart / (1024.0f * 1024.0f),
                heapAvailableAtStart / (1024.0f * 1024.0f),
                linearHeapAvailable / (1024.0f * 1024.0f),
                heapAvailable / (1024.0f * 1024.0f)
        );
        C2D_DrawRectSolid(10, 10, 0.9, 300, 65, C2D_Color32f(1, 0, 1, 0.5));
        C2D_DrawRectSolid(10, 80, 0.9, 300, 75, C2D_Color32f(0, 0, 1, 0.5));
    }
    C2D_TextParse(&dynamicText, overlayTextBuf, buffer);
    C2D_TextOptimize(&dynamicText);
    C2D_DrawText(&dynamicText, C2D_WithColor, 20.0f, 15.0f, 1.0f, 0.5f, 0.5f, C2D_Color32f(1, 1, 1, 1));
    C2D_Flush();
}
#endif

inline void setTextureFilter(int linear)
{
    int newTexFilter;

    if (linear != -1)
        newTexFilter = linear;
    else
        newTexFilter = isN3DS ? GPU_LINEAR : GPU_NEAREST;

    if (newTexFilter != activeTexFilter) {
        C3D_TexSetFilter(&render_tex, GPU_LINEAR, (GPU_TEXTURE_FILTER_PARAM) newTexFilter);
        activeTexFilter = newTexFilter;
    }
}

inline void _drawRect(const float texW , const float texH, const float subTexX, const float subTexY, const float subTexW, const float subTexH,
        const float posX, const float posY, const float width, const float height)
{
    const float vertZ = 0.5f;
    const float vertW = 1.f;

    C3D_ImmDrawBegin(GPU_TRIANGLE_STRIP);
    {
        // Bottom left corner
        C3D_ImmSendAttrib(posX, posY, vertZ, vertW);                                   // v0 = position xyzw
        C3D_ImmSendAttrib(subTexX / texW, 1.f - (subTexY + subTexH) / texH, 0.f, 0.f); // v1 = texcoord uv

        // Bottom right corner
        C3D_ImmSendAttrib(width + posX, posY, vertZ, vertW);
        C3D_ImmSendAttrib((subTexX + subTexW) / texW, 1.f - (subTexY + subTexH) / texH, 0.f, 0.f);

        // Top left corner
        C3D_ImmSendAttrib(posX, height + posY, vertZ, vertW);
        C3D_ImmSendAttrib(subTexX / texW, 1.f - subTexY / texH, 0.f, 0.f);

        // Top right corner
        C3D_ImmSendAttrib(width + posX, height + posY, vertZ, vertW);
        C3D_ImmSendAttrib((subTexX + subTexW) / texW, 1.f - subTexY / texH, 0.f, 0.f);
    }
    C3D_ImmDrawEnd();
}

inline void drawRect_tex(const float subTexX, const float subTexY, const float subTexW, const float subTexH,
        const float posX, const float posY, const float width, const float height)
{
    const float texW = 512;
    const float texH = 512;

    _drawRect(texW, texH, subTexX, subTexY, subTexW, subTexH, posX, posY, width, height);
}

inline void drawRect(const float subTexX, const float subTexY, const float subTexW, const float subTexH,
        const float posX, const float posY, const float width, const float height)
{
    float newPosX = posX;
    float newWidth = width;

    const float texW = renderTextureWidth;
    const float texH = renderTextureHeight;

    if (targetTop && ctr_gfx.isWide) {
        newPosX *= 2.f;
        newWidth *= 2.f;
    }

    _drawRect(texW, texH, subTexX, subTexY, subTexW, subTexH, newPosX, posY, newWidth, height);
}

bool drawRects()
{
    if (!beginRender(false)) return false;
    drawTopRenderTarget(0x000000);

    switch (ctr_rectMap.active)
    {
        case DISPLAY_SPLASH:
            break;

        case DISPLAY_MAIN:
            // Same framebuffer, but never sample the interactive plates.
            drawRect(ctr_menu::TopSrcX, ctr_menu::TopSrcY,
                ctr_menu::TopSrcW, ctr_menu::TopSrcH, ctr_menu::TopDstX,
                ctr_menu::TopDstY, ctr_menu::TopDstW, ctr_menu::TopDstH);
            break;

        case DISPLAY_MOVIE:
            drawRect(rectMaps[ctr_rectMap.active][0]->src_x, rectMaps[ctr_rectMap.active][0]->src_y,
                    rectMaps[ctr_rectMap.active][0]->src_w, rectMaps[ctr_rectMap.active][0]->src_h,
                    rectMaps[ctr_rectMap.active][0]->dst_x, rectMaps[ctr_rectMap.active][0]->dst_y,
                    rectMaps[ctr_rectMap.active][0]->dst_w, rectMaps[ctr_rectMap.active][0]->dst_h
            );
            break;

        case DISPLAY_DIALOG:
        case DISPLAY_INVENTORY_TRADE:
            for (int i = 0; i < numRectsInMap[DISPLAY_DIALOG_TOP]; i++) {
                drawRect(rectMaps[DISPLAY_DIALOG_TOP][i]->src_x, rectMaps[DISPLAY_DIALOG_TOP][i]->src_y,
                        rectMaps[DISPLAY_DIALOG_TOP][i]->src_w, rectMaps[DISPLAY_DIALOG_TOP][i]->src_h,
                        rectMaps[DISPLAY_DIALOG_TOP][i]->dst_x, rectMaps[DISPLAY_DIALOG_TOP][i]->dst_y,
                        rectMaps[DISPLAY_DIALOG_TOP][i]->dst_w, rectMaps[DISPLAY_DIALOG_TOP][i]->dst_h
                );
            }
            break;

        case DISPLAY_SAVE_COMMENT:
            // Keep the description or progress message visible on top.
            { const auto* r = rectMaps[DISPLAY_SAVE_COMMENT][0];
              drawRect(r->src_x, r->src_y, r->src_w, r->src_h,
                  40 + r->dst_x, r->dst_y, r->dst_w, r->dst_h); }
            break;
        case DISPLAY_LOADSAVE:
            for (int i = 0; i < numRectsInMap[DISPLAY_LOADSAVE_TOP]; i++) {
                drawRect(rectMaps[DISPLAY_LOADSAVE_TOP][i]->src_x, rectMaps[DISPLAY_LOADSAVE_TOP][i]->src_y,
                        rectMaps[DISPLAY_LOADSAVE_TOP][i]->src_w, rectMaps[DISPLAY_LOADSAVE_TOP][i]->src_h,
                        rectMaps[DISPLAY_LOADSAVE_TOP][i]->dst_x, rectMaps[DISPLAY_LOADSAVE_TOP][i]->dst_y,
                        rectMaps[DISPLAY_LOADSAVE_TOP][i]->dst_w, rectMaps[DISPLAY_LOADSAVE_TOP][i]->dst_h
                );
            }
            break;

        case DISPLAY_FIELD:
            for (int i = 0; i < numRectsInMap[DISPLAY_GUI]; i++) {
                drawRect(rectMaps[DISPLAY_GUI][i]->src_x, rectMaps[DISPLAY_GUI][i]->src_y,
                        rectMaps[DISPLAY_GUI][i]->src_w, rectMaps[DISPLAY_GUI][i]->src_h,
                        40+rectMaps[DISPLAY_GUI][i]->dst_x, rectMaps[DISPLAY_GUI][i]->dst_y,
                        rectMaps[DISPLAY_GUI][i]->dst_w, rectMaps[DISPLAY_GUI][i]->dst_h
                );
            }
            for (int i = 0; i < getIndicatorSlotNum(); i++) {
                drawRect(rectMaps[DISPLAY_GUI_INDICATOR][i]->src_x, rectMaps[DISPLAY_GUI_INDICATOR][i]->src_y,
                        rectMaps[DISPLAY_GUI_INDICATOR][i]->src_w, rectMaps[DISPLAY_GUI_INDICATOR][i]->src_h,
                        40+rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_x, rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_y,
                        rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_w, rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_h
                );
            }
            break;

        case DISPLAY_GUI:
            drawRect(rectMaps[DISPLAY_FIELD][0]->src_x, rectMaps[DISPLAY_FIELD][0]->src_y,
                    rectMaps[DISPLAY_FIELD][0]->src_w, rectMaps[DISPLAY_FIELD][0]->src_h,
                    rectMaps[DISPLAY_FIELD][0]->dst_x, rectMaps[DISPLAY_FIELD][0]->dst_y,
                    rectMaps[DISPLAY_FIELD][0]->dst_w, rectMaps[DISPLAY_FIELD][0]->dst_h);
            break;

        case DISPLAY_CHAR_PERK:
            drawRect(rectMaps[DISPLAY_CHAR_PERK_TOP][0]->src_x, rectMaps[DISPLAY_CHAR_PERK_TOP][0]->src_y,
                    rectMaps[DISPLAY_CHAR_PERK_TOP][0]->src_w, rectMaps[DISPLAY_CHAR_PERK_TOP][0]->src_h,
                    rectMaps[DISPLAY_CHAR_PERK_TOP][0]->dst_x, rectMaps[DISPLAY_CHAR_PERK_TOP][0]->dst_y,
                    rectMaps[DISPLAY_CHAR_PERK_TOP][0]->dst_w, rectMaps[DISPLAY_CHAR_PERK_TOP][0]->dst_h
            );
            break;

        case DISPLAY_CHAR:
            for (int i = 0; i < numRectsInMap[DISPLAY_CHAR_TOP]; i++) {
                drawRect(rectMaps[DISPLAY_CHAR_TOP][i]->src_x, rectMaps[DISPLAY_CHAR_TOP][i]->src_y,
                        rectMaps[DISPLAY_CHAR_TOP][i]->src_w, rectMaps[DISPLAY_CHAR_TOP][i]->src_h,
                        rectMaps[DISPLAY_CHAR_TOP][i]->dst_x, rectMaps[DISPLAY_CHAR_TOP][i]->dst_y,
                        rectMaps[DISPLAY_CHAR_TOP][i]->dst_w, rectMaps[DISPLAY_CHAR_TOP][i]->dst_h
                );
            }
            break;

        case DISPLAY_PIPBOY: {
            const auto* r = rectMaps[DISPLAY_PIPBOY][0];
            // Content only; the original controls are on the touch screen.
            drawRect(r->src_x + 254, r->src_y - 330 + 46, 374, 410, 90.5f, 0, 219.f, 240);
            break;
        }
        case DISPLAY_AUTOMAP: {
            const auto* r = rectMaps[DISPLAY_AUTOMAP][0];
            drawRect(r->src_x, r->src_y, r->src_w, r->src_h, 70.25f, 0, 259.5f, 240);
            break;
        }
        case DISPLAY_MODS:
            drawRect(120, 0, 400, 240, 0, 0, 400, 240);
            break;
        case DISPLAY_WORLDMAP: {
            if (worldMapScreensSwapped()) {
                for (int i = 0; i < numRectsInMap[DISPLAY_WORLDMAP]; ++i) {
                    const auto* r = rectMaps[DISPLAY_WORLDMAP][i];
                    drawRect(r->src_x, r->src_y, r->src_w, r->src_h,
                        40.f + r->dst_x, r->dst_y, r->dst_w, r->dst_h);
                }
            } else {
                const auto r = worldMapViewportRect();
                drawRect(r.src_x, r.src_y, r.src_w, r.src_h,
                    r.dst_x, r.dst_y, r.dst_w, r.dst_h);
            }
            break;
        }
        case DISPLAY_OPTIONS:
        case DISPLAY_CHAR_SELECT:
        case DISPLAY_FULL:
            drawRect(offsetX, (240 - offsetY), 400, 240, 0,   0, 400, 240);
            break;

        default:
            drawRect(rectMaps[DISPLAY_FULL][0]->src_x, rectMaps[DISPLAY_FULL][0]->src_y,
                    rectMaps[DISPLAY_FULL][0]->src_w, rectMaps[DISPLAY_FULL][0]->src_h,
                    40+rectMaps[DISPLAY_FULL][0]->dst_x, rectMaps[DISPLAY_FULL][0]->dst_y,
                    rectMaps[DISPLAY_FULL][0]->dst_w, rectMaps[DISPLAY_FULL][0]->dst_h
            );
            break;
    }

    drawBottomRenderTarget(0x000000);

    switch (ctr_rectMap.active)
    {
		case DISPLAY_DIALOG:
        case DISPLAY_SKILLDEX:
        case DISPLAY_INVENTORY_TRADE:
            drawFrameRect();

        default:
            break;
    }

    switch (ctr_rectMap.active)
    {
        case DISPLAY_MOVIE:
            setTextureFilter(1);
            drawRect(rectMaps[DISPLAY_MOVIE_SUB][0]->src_x, rectMaps[DISPLAY_MOVIE_SUB][0]->src_y,
                    rectMaps[DISPLAY_MOVIE_SUB][0]->src_w, rectMaps[DISPLAY_MOVIE_SUB][0]->src_h,
                    rectMaps[DISPLAY_MOVIE_SUB][0]->dst_x, rectMaps[DISPLAY_MOVIE_SUB][0]->dst_y,
                    rectMaps[DISPLAY_MOVIE_SUB][0]->dst_w, rectMaps[DISPLAY_MOVIE_SUB][0]->dst_h);
            setTextureFilter(-1);
            break;

        case DISPLAY_SPLASH:
            setTextureFilter(1);
            drawRect(rectMaps[DISPLAY_FULL][0]->src_x, rectMaps[DISPLAY_FULL][0]->src_y,
                    rectMaps[DISPLAY_FULL][0]->src_w, rectMaps[DISPLAY_FULL][0]->src_h,
                    rectMaps[DISPLAY_FULL][0]->dst_x, rectMaps[DISPLAY_FULL][0]->dst_y,
                    rectMaps[DISPLAY_FULL][0]->dst_w, rectMaps[DISPLAY_FULL][0]->dst_h
            );
            setTextureFilter(-1);
            break;

        case DISPLAY_DIALOG:
            for (int i = 0; i < numRectsInMap[ctr_rectMap.active]; i++) {
                drawRect(rectMaps[ctr_rectMap.active][i]->src_x, rectMaps[ctr_rectMap.active][i]->src_y,
                        rectMaps[ctr_rectMap.active][i]->src_w, rectMaps[ctr_rectMap.active][i]->src_h,
                        rectMaps[ctr_rectMap.active][i]->dst_x, rectMaps[ctr_rectMap.active][i]->dst_y,
                        rectMaps[ctr_rectMap.active][i]->dst_w, rectMaps[ctr_rectMap.active][i]->dst_h
                );
            }
            break;

        case DISPLAY_LOADSAVE:

            for (int i = 0; i < numRectsInMap[DISPLAY_LOADSAVE_BACK]; i++) {
                drawRect(rectMaps[DISPLAY_LOADSAVE_BACK][i]->src_x, rectMaps[DISPLAY_LOADSAVE_BACK][i]->src_y,
                        rectMaps[DISPLAY_LOADSAVE_BACK][i]->src_w, rectMaps[DISPLAY_LOADSAVE_BACK][i]->src_h,
                        rectMaps[DISPLAY_LOADSAVE_BACK][i]->dst_x, rectMaps[DISPLAY_LOADSAVE_BACK][i]->dst_y,
                        rectMaps[DISPLAY_LOADSAVE_BACK][i]->dst_w, rectMaps[DISPLAY_LOADSAVE_BACK][i]->dst_h
                );
            }

            drawRect(rectMaps[DISPLAY_LOADSAVE_SLOT][0]->src_x,
			        rectMaps[DISPLAY_LOADSAVE_SLOT][0]->src_y + getSaveSlotOffset(),
                    rectMaps[DISPLAY_LOADSAVE_SLOT][0]->src_w, rectMaps[DISPLAY_LOADSAVE_SLOT][0]->src_h,
                    rectMaps[DISPLAY_LOADSAVE_SLOT][0]->dst_x, rectMaps[DISPLAY_LOADSAVE_SLOT][0]->dst_y,
                    rectMaps[DISPLAY_LOADSAVE_SLOT][0]->dst_w, rectMaps[DISPLAY_LOADSAVE_SLOT][0]->dst_h
            );
            for (int i = 0; i < numRectsInMap[ctr_rectMap.active]; i++) {
                drawRect(rectMaps[ctr_rectMap.active][i]->src_x, rectMaps[ctr_rectMap.active][i]->src_y,
                        rectMaps[ctr_rectMap.active][i]->src_w, rectMaps[ctr_rectMap.active][i]->src_h,
                        rectMaps[ctr_rectMap.active][i]->dst_x, rectMaps[ctr_rectMap.active][i]->dst_y,
                        rectMaps[ctr_rectMap.active][i]->dst_w, rectMaps[ctr_rectMap.active][i]->dst_h
                );
            }
            break;

        case DISPLAY_CHAR:

            if (offsetY_char <= 240) {
                drawRect(rectMaps[DISPLAY_CHAR][0]->src_x, rectMaps[DISPLAY_CHAR][0]->src_y+offsetY_char,
                        rectMaps[DISPLAY_CHAR][0]->src_w, rectMaps[DISPLAY_CHAR][0]->src_h,
                        rectMaps[DISPLAY_CHAR][0]->dst_x, rectMaps[DISPLAY_CHAR][0]->dst_y,
                        rectMaps[DISPLAY_CHAR][0]->dst_w, rectMaps[DISPLAY_CHAR][0]->dst_h
                );
            } else if ((offsetY_char > 240) && (offsetY_char <= 480)) {
                drawRect(rectMaps[DISPLAY_CHAR][0]->src_x, rectMaps[DISPLAY_CHAR][0]->src_y+240,
                        rectMaps[DISPLAY_CHAR][0]->src_w, rectMaps[DISPLAY_CHAR][0]->src_h,
                        rectMaps[DISPLAY_CHAR][0]->dst_x, rectMaps[DISPLAY_CHAR][0]->dst_y-(240-offsetY_char),
                        rectMaps[DISPLAY_CHAR][0]->dst_w, rectMaps[DISPLAY_CHAR][0]->dst_h
                );
                drawRect(rectMaps[DISPLAY_CHAR][1]->src_x, rectMaps[DISPLAY_CHAR][1]->src_y,
                        rectMaps[DISPLAY_CHAR][1]->src_w, rectMaps[DISPLAY_CHAR][1]->src_h,
                        rectMaps[DISPLAY_CHAR][1]->dst_x+10, rectMaps[DISPLAY_CHAR][0]->dst_y-(480-offsetY_char),
                        rectMaps[DISPLAY_CHAR][1]->dst_w, rectMaps[DISPLAY_CHAR][1]->dst_h
                );
            } else if (offsetY_char > 480) {
                drawRect(rectMaps[DISPLAY_CHAR][1]->src_x, rectMaps[DISPLAY_CHAR][1]->src_y+(offsetY_char-480),
                        rectMaps[DISPLAY_CHAR][1]->src_w, rectMaps[DISPLAY_CHAR][1]->src_h,
                        rectMaps[DISPLAY_CHAR][1]->dst_x+10, rectMaps[DISPLAY_CHAR][1]->dst_y,
                        rectMaps[DISPLAY_CHAR][1]->dst_w, rectMaps[DISPLAY_CHAR][1]->dst_h
                );
            }
            for (int i = 2; i < 4; i++) {
                drawRect(rectMaps[ctr_rectMap.active][i]->src_x, rectMaps[ctr_rectMap.active][i]->src_y,
                        rectMaps[ctr_rectMap.active][i]->src_w, rectMaps[ctr_rectMap.active][i]->src_h,
                        rectMaps[ctr_rectMap.active][i]->dst_x, rectMaps[ctr_rectMap.active][i]->dst_y,
                        rectMaps[ctr_rectMap.active][i]->dst_w, rectMaps[ctr_rectMap.active][i]->dst_h
                );

            }
            if (isAgeWindow){
                drawRect(rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->src_x, rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->src_y,
                        rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->src_w, rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->src_h,
                        rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->dst_x, rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->dst_y,
                        rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->dst_w, rectMaps[DISPLAY_CHAR_EDIT_AGE][0]->dst_h
                );
            }
            if (isSexWindow){
                drawRect(rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->src_x, rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->src_y,
                        rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->src_w, rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->src_h,
                        rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->dst_x, rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->dst_y,
                        rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->dst_w, rectMaps[DISPLAY_CHAR_EDIT_SEX][0]->dst_h
                );
            }
            break;

        case DISPLAY_GUI:
            for (int i = 0; i < getIndicatorSlotNum(); i++) {
                drawRect(rectMaps[DISPLAY_GUI_INDICATOR][i]->src_x, rectMaps[DISPLAY_GUI_INDICATOR][i]->src_y,
                        rectMaps[DISPLAY_GUI_INDICATOR][i]->src_w, rectMaps[DISPLAY_GUI_INDICATOR][i]->src_h,
                        rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_x, rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_y,
                        rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_w, rectMaps[DISPLAY_GUI_INDICATOR][i]->dst_h
                );
            }

        case DISPLAY_WORLDMAP:
            if (ctr_rectMap.active == DISPLAY_WORLDMAP && worldMapScreensSwapped()) {
                const auto r = worldMapViewportRect();
                drawRect(r.src_x, r.src_y, r.src_w, r.src_h,
                    r.dst_x, r.dst_y, r.dst_w, r.dst_h);
                break;
            }
            [[fallthrough]];
        case DISPLAY_FULL:
            if (ctr_rectMap.active == DISPLAY_FULL && ctr_input.mode == DISPLAY_MODE_FULL_TOP)
                break;

        default:
            for (int i = 0; i < numRectsInMap[ctr_rectMap.active]; i++) {
                drawRect(rectMaps[ctr_rectMap.active][i]->src_x, rectMaps[ctr_rectMap.active][i]->src_y,
                        rectMaps[ctr_rectMap.active][i]->src_w, rectMaps[ctr_rectMap.active][i]->src_h,
                        rectMaps[ctr_rectMap.active][i]->dst_x, rectMaps[ctr_rectMap.active][i]->dst_y,
                        rectMaps[ctr_rectMap.active][i]->dst_w, rectMaps[ctr_rectMap.active][i]->dst_h
                );
            }
            break;
    }

#ifdef _DEBUG_OVERLAY
    show_overlay();
#endif

    finishRender();
    return true;
}

void drawFrameRect()
{
    C3D_TexBind(0, &static_tex);
    drawRect_tex(0, 0, 320, 240, 0, 0, 320, 240);
    C3D_TexBind(0, &render_tex);
}

void ctr_gfx_invalidate()
{
    renderRows.invalidate();
    sceneValid = false;
}

void ctr_gfx_copy_rect(const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch,
    int width, int height, int destY)
{
    renderRows.copyRect(src, srcPitch, dst, dstPitch, width, height, destY, forceFullUpload);
}

// Native text is independent of the game palette, including world-map fade to black.
// No game windows, save state, or input mappings are changed during this display.
void ctr_gfx_set_zone_loading(bool enabled)
{
    if (zoneLoading == enabled) return;
    zoneLoading = enabled;
    zoneLoadingPresented = false;
    ctr_gfx_invalidate();
}

static void drawZoneLoading()
{
    if (zoneLoadingPresented || !rendererInitialized || !overlayTextBuf) return;
    if (!beginRender(true)) return;
    C2D_Prepare();
    C2D_TextBufClear(overlayTextBuf);
    C2D_Text heading, detail;
    C2D_TextParse(&heading, overlayTextBuf, "LOADING AREA...");
    C2D_TextOptimize(&heading);
    C2D_TextParse(&detail, overlayTextBuf, "Please wait...");
    C2D_TextOptimize(&detail);
    const auto green = C2D_Color32f(0.55f, 0.95f, 0.38f, 1.f);
    C3D_RenderTargetClear(topRenderTarget, C3D_CLEAR_ALL, 0, 0);
    C2D_SceneTarget(topRenderTarget);
    C2D_DrawText(&heading, C2D_WithColor | C2D_AlignCenter,
        ctr_gfx.isWide ? 400.f : 200.f, 100.f, 0.5f, 0.8f, 0.8f, green);
    C2D_Flush();
    C3D_RenderTargetClear(bottomRenderTarget, C3D_CLEAR_ALL, 0, 0);
    C2D_SceneTarget(bottomRenderTarget);
    C2D_DrawText(&detail, C2D_WithColor | C2D_AlignCenter,
        160.f, 110.f, 0.5f, 0.6f, 0.6f, green);
    C2D_Flush();
    finishRender();
    zoneLoadingPresented = true;
}

void ctr_gfx_draw(SDL_Surface* gSdlSurface)
{
    if (!rendererInitialized) return;
    if (zoneLoading) {
        drawZoneLoading();
        return;
    }
    if (gSdlSurface == nullptr || gSdlSurface->format == nullptr
        || gSdlSurface->format->palette == nullptr || gSdlSurface->pixels == nullptr
        || gSdlSurface->format->palette->colors == nullptr
        || gSdlSurface->format->palette->ncolors < 256
        || renderTextureData == nullptr || gSdlSurface->format->BytesPerPixel != 1
        || gSdlSurface->w <= 0 || gSdlSurface->h <= 0
        || gSdlSurface->w > renderTextureWidth || gSdlSurface->h > renderTextureHeight
        || gSdlSurface->pitch < gSdlSurface->w) {
        static bool reportedRejected = false;
        if (!reportedRejected) {
            reportedRejected = true;
            char message[128];
            snprintf(message, sizeof(message), "Renderer rejected surface: %dx%d pitch %d texture %dx%d", gSdlSurface ? gSdlSurface->w : 0, gSdlSurface ? gSdlSurface->h : 0, gSdlSurface ? gSdlSurface->pitch : 0, renderTextureWidth, renderTextureHeight);
            ctrModProfileStage(message);
        }
        return;
    }

    static bool reportedAccepted = false;
    if (!reportedAccepted) {
        reportedAccepted = true;
        char message[96];
        snprintf(message, sizeof(message), "Renderer accepted surface: %dx%d pitch %d", gSdlSurface->w, gSdlSurface->h, gSdlSurface->pitch);
        ctrModProfileStage(message);
    }
    uint32_t start = profileEnabled ? SDL_GetTicks() : 0;
    if (profileEnabled && profileFrames == 0) profileStart = start;
    int convertedRows = renderRows.update(
        static_cast<const uint8_t*>(gSdlSurface->pixels), gSdlSurface->pitch,
        gSdlSurface->w, gSdlSurface->h, gSdlSurface->format->palette->colors,
        renderTextureData, renderTextureStride, forceFullUpload, o3dsV9FastRender());
    uint32_t convertEnd = profileEnabled ? SDL_GetTicks() : 0;

    // Keep the existing synchronous GPU upload, byte order, and texture size.
    // The VRAM texture remains valid when only the screen layout changes.
    if (convertedRows != 0) {
        GSPGPU_FlushDataCache(renderTextureData, renderTextureByteCount);
        C3D_SyncDisplayTransfer((u32*)renderTextureData, GX_BUFFER_DIM(renderTextureWidth, renderTextureHeight),
            (u32*)render_tex.data, GX_BUFFER_DIM(renderTextureWidth, renderTextureHeight), TEXTURE_TRANSFER_FLAGS);
        GSPGPU_FlushDataCache(render_tex.data, renderTextureByteCount);
    }
    uint32_t uploadEnd = profileEnabled ? SDL_GetTicks() : 0;

    // Leave input, script and audio timing untouched. Only suppress exact
    // duplicate gameplay frames, and refresh at least every 50 ms as a
    // conservative heartbeat for display/resume behavior. Any pixel or layout
    // change is submitted immediately, so cursor movement is never throttled.
    SceneSnapshot scene{};
    bool trackScene = o3dsV9FastRender() && !forceFullUpload && captureScene(scene);
    uint32_t drawNow = SDL_GetTicks();
    bool redraw = shouldRedrawScene(trackScene, sceneValid, convertedRows,
        lastScene, scene, drawNow - lastSceneDraw);
    if (redraw) {
        C3D_BindProgram(&program);

        uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
        C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord

        Mtx_OrthoTilt(&topProjection, 0, ctr_gfx.isWide ? GSP_SCREEN_HEIGHT_TOP_2X : GSP_SCREEN_HEIGHT_TOP, 0, GSP_SCREEN_WIDTH, 0.1f, 1.f, true);
        Mtx_OrthoTilt(&bottomProjection, 0, GSP_SCREEN_HEIGHT_BOTTOM, 0, GSP_SCREEN_WIDTH, 0.1f, 1.f, true);

        C3D_TexBind(0, &render_tex);

        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

        if (!drawRects()) {
            sceneValid = false;
            return;
        }

        lastSceneDraw = drawNow;
        if (trackScene) lastScene = scene;
        sceneValid = trackScene;
    }

    if (profileEnabled) {
        uint32_t now = SDL_GetTicks();
        ++profileFrames;
        profileDrawCalls += redraw;
        profileUploads += convertedRows != 0;
        profileRows += convertedRows;
        profilePossibleRows += gSdlSurface->h;
        profileConvertMs += convertEnd - start;
        profileUploadMs += uploadEnd - convertEnd;
        profileDrawMs += now - uploadEnd;
        if (now - profileStart >= 10000) {
            FILE* file = fopen("sdmc:/3ds/fallout2/o3ds-render-profile.csv", "a");
            if (file) {
                fprintf(file, "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
                    (unsigned long)(now - profileStart), (unsigned long)profileFrames,
                    (unsigned long)profileUploads, (unsigned long)profileRows,
                    (unsigned long)profilePossibleRows, (unsigned long)profileConvertMs,
                    (unsigned long)profileUploadMs, (unsigned long)profileDrawMs, (unsigned long)profileDrawCalls);
                fclose(file);
            }
            profileStart = now;
            profileFrames = profileUploads = profileRows = profilePossibleRows = profileDrawCalls = 0;
            profileConvertMs = profileUploadMs = profileDrawMs = 0;
        }
    }
}

bool beginRender(bool vSync)
{
    const bool ok = C3D_FrameBegin(vSync ? C3D_FRAME_SYNCDRAW : 0);
    if (!ok) {
        static bool reportedFailure = false;
        if (!reportedFailure) {
            reportedFailure = true;
            ctrModProfileStage("GPU frame begin failed");
        }
    }
    return ok;
}

void drawTopRenderTarget(uint32_t clearColor)
{
    C3D_RenderTargetClear(topRenderTarget, C3D_CLEAR_ALL, clearColor, 0);
    C3D_FrameDrawOn(topRenderTarget);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &topProjection);
    targetTop = true;
}

void drawBottomRenderTarget(uint32_t clearColor)
{
    C3D_RenderTargetClear(bottomRenderTarget, C3D_CLEAR_ALL, clearColor, 0);
    C3D_FrameDrawOn(bottomRenderTarget);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &bottomProjection);
    targetTop = false;
}

void finishRender()
{
    C3D_FrameEnd(0);
    ++submittedFrames;
    if (submittedFrames == 1 || submittedFrames == 60) {
        char message[128];
        snprintf(message, sizeof(message), "GPU submitted frame %u; vblank top=%lu bottom=%lu; GPU right=%d",
            submittedFrames, (unsigned long)C3D_FrameCounter(0),
            (unsigned long)C3D_FrameCounter(1), (int)gspHasGpuRight());
        ctrModProfileStage(message);
    }
}

void initTransferTexture()
{
    renderTextureData = (uint8_t *)linearAlloc(renderTextureByteCount);

    if (!renderTextureData) {
        ctrModProfileStage("GPU init failed: linear texture buffer allocation");
        exit(EXIT_FAILURE);
    }
    memset(renderTextureData, 0, renderTextureByteCount);

    if (!C3D_TexInitVRAM(&render_tex, renderTextureWidth, renderTextureHeight, GPU_RGB8)) {
        ctrModProfileStage("GPU init failed: VRAM texture allocation");
        exit(EXIT_FAILURE);
    }

    setTextureFilter(-1);
}

void ctr_gfx_reinit()
{
    sceneValid = false;
    C3D_RenderTargetDelete(topRenderTarget);

    topRenderTarget = C3D_RenderTargetCreate(
            GSP_SCREEN_WIDTH, ctr_gfx.isWide ? GSP_SCREEN_HEIGHT_TOP_2X : GSP_SCREEN_HEIGHT_TOP, GPU_RB_RGB8, GPU_RB_DEPTH16);
    if (!topRenderTarget) {
        ctrModProfileStage("GPU init failed: top render target allocation");
        exit(EXIT_FAILURE);
    }
    C3D_RenderTargetSetOutput(topRenderTarget, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    C3D_BindProgram(&program);

    uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

    C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord

    Mtx_OrthoTilt(&topProjection, 0, ctr_gfx.isWide ? GSP_SCREEN_HEIGHT_TOP_2X : GSP_SCREEN_HEIGHT_TOP, 0, GSP_SCREEN_WIDTH, 0.1f, 1.f, true);
}

void ctr_gfx_init()
{
    sceneValid = false;
    submittedFrames = 0;
    ctrModProfileStage("GPU init: begin");

    u8 is2DS = 0;
    cfguInit();
    CFGU_GetModelNintendo2DS(&is2DS);
    APT_CheckNew3DS(&isN3DS);

    // SDL owns the graphics service. Do not restart its event thread.
    gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
    gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
    gfxSet3D(false);
    ctrModProfileStage("GPU init: retaining SDL graphics service");

if (is2DS == 1) {
    gfxSetWide(true);
    ctr_gfx.isWide = gfxIsWide();
} else
    ctr_gfx.isWide = false;

    char message[128];
    snprintf(message, sizeof(message), "GPU init: new=%d 2DS flag=%u wide=%d GPU right=%d",
        (int)isN3DS, (unsigned int)is2DS, (int)ctr_gfx.isWide, (int)gspHasGpuRight());
    ctrModProfileStage(message);

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        ctrModProfileStage("GPU init failed: C3D_Init");
        exit(EXIT_FAILURE);
    }

    topRenderTarget = C3D_RenderTargetCreate(
            GSP_SCREEN_WIDTH, ctr_gfx.isWide ? GSP_SCREEN_HEIGHT_TOP_2X : GSP_SCREEN_HEIGHT_TOP, GPU_RB_RGB8, GPU_RB_DEPTH16);
    if (!topRenderTarget) {
        ctrModProfileStage("GPU init failed: top render target allocation");
        exit(EXIT_FAILURE);
    }
    C3D_RenderTargetSetOutput(topRenderTarget, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    bottomRenderTarget = C3D_RenderTargetCreate(GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_BOTTOM, GPU_RB_RGB8, GPU_RB_DEPTH16);
    if (!bottomRenderTarget) {
        ctrModProfileStage("GPU init failed: bottom render target allocation");
        exit(EXIT_FAILURE);
    }
    C3D_RenderTargetSetOutput(bottomRenderTarget, GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    vshader_dvlb = DVLB_ParseFile((uint32_t *)vshader_shbin, vshader_shbin_size);
    shaderProgramInit(&program);
    shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
    C3D_BindProgram(&program);

    uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

    C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord

    Mtx_OrthoTilt(&topProjection, 0, ctr_gfx.isWide ? GSP_SCREEN_HEIGHT_TOP_2X : GSP_SCREEN_HEIGHT_TOP, 0, GSP_SCREEN_WIDTH, 0.1f, 1.f, true);
    Mtx_OrthoTilt(&bottomProjection, 0, GSP_SCREEN_HEIGHT_BOTTOM, 0, GSP_SCREEN_WIDTH, 0.1f, 1.f, true);

    initTransferTexture();
    ctr_gfx_invalidate();
    // Optional troubleshooting and profiling switches (checked once at startup).
    FILE* flag = fopen("sdmc:/3ds/fallout2/o3ds-render-compat.flag", "rb");
    forceFullUpload = flag != nullptr;
    if (flag) fclose(flag);
    flag = fopen("sdmc:/3ds/fallout2/o3ds-render-profile.flag", "rb");
    profileEnabled = flag != nullptr;
    if (flag) fclose(flag);
    profileFrames = profileUploads = profileRows = profilePossibleRows = profileDrawCalls = 0;
    profileConvertMs = profileUploadMs = profileDrawMs = 0;
    if (profileEnabled) {
        FILE* log = fopen("sdmc:/3ds/fallout2/o3ds-render-profile.csv", "w");
        if (log) {
            fputs("elapsed_ms,render_calls,uploads,converted_rows,possible_rows,convert_ms,upload_ms,draw_ms,draw_submissions\n", log);
            fclose(log);
        } else {
            profileEnabled = false;
        }
    }
    romfsInit();

	if (!loadTextureFromFile(&static_tex, NULL, "romfs:/gfx/frame_tex.t3x")) {
//		GNWSystemError("loadTextureFromFile failed\n");
		exit(EXIT_FAILURE);
	}
	C3D_TexSetFilter(&static_tex, GPU_LINEAR, GPU_NEAREST);

    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    overlayTextBuf = C2D_TextBufNew(4096);
    rendererInitialized = true;
    ctrModProfileStage("GPU init: complete");
}

void ctr_gfx_exit()
{
    if (!rendererInitialized) return;
    rendererInitialized = false;
    sceneValid = false;
    // FrameSync waits for both VBlank counters, which may stop when HOME
    // suspends the application. Deleting linked render targets drains the
    // Citro3D command queue without requiring a new VBlank.
    ctrModProfileStage("GPU cleanup: draining queue via render targets");
    C3D_RenderTargetDelete(topRenderTarget);
    topRenderTarget = nullptr;
    C3D_RenderTargetDelete(bottomRenderTarget);
    bottomRenderTarget = nullptr;
    ctrModProfileStage("GPU cleanup: releasing resources");
    shaderProgramFree(&program);
    DVLB_Free(vshader_dvlb);

    linearFree(renderTextureData);
    renderTextureData = nullptr;
    C3D_TexDelete(&render_tex);
    C3D_TexDelete(&static_tex);

    C2D_TextBufDelete(overlayTextBuf);
    C2D_Fini();

    ctrModProfileStage("GPU cleanup: C3D_Fini begins");
    C3D_Fini();
    romfsExit();
    cfguExit();
    ctrModProfileStage("GPU cleanup complete");
}

} // namespace fallout
