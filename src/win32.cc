#include "win32.h"

#include <stdlib.h>

#include <SDL.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "main.h"
#include "svga.h"
#include "window_manager.h"

#if __APPLE__ && TARGET_OS_IOS
#include "platform/ios/paths.h"
#endif

#ifdef __3DS__
#include "3ds.h"
#include "audio_engine.h"
#include "platform/ctr/ctr_mod_profiles.h"
#include "platform/ctr/ctr_gfx.h"
#include "platform/ctr/ctr_rectmap.h"

u32 __ctru_heap_size = 0;
u32 __ctru_linear_heap_size = 32 * 1024 * 1024;
u32 __stacksize__ = 64 * 1024;

#endif

namespace fallout {

// 0x51E444
bool gProgramIsActive = false;

#ifdef __3DS__
// Also used by atexit when the engine exits without returning to main.
static void ctrShutdownRuntime()
{
    static bool shutdownStarted = false;
    if (shutdownStarted) {
        return;
    }
    shutdownStarted = true;

    ctrModProfileStage("Alpha12: runtime cleanup begins");
    audioEngineExit();
    ctrModProfileStage("Alpha12: renderer cleanup begins");
    ctr_gfx_exit();
    ctr_rectmap_exit();
    directDrawFree();
    ctrModProfileStage("SDL shutdown begins");
    SDL_Quit();
    ctrModProfileStage("SDL shutdown complete; runtime shutdown follows");
}
#endif

#ifdef _WIN32
HANDLE GNW95_mutex = nullptr;
#endif

int main(int argc, char* argv[])
{
    int rc;

#if _WIN32
    GNW95_mutex = CreateMutexA(0, TRUE, "GNW95MUTEX");
    if (GetLastError() != ERROR_SUCCESS) {
        return 0;
    }
#endif

#if __APPLE__ && TARGET_OS_IOS
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(iOSGetDocumentsPath());
#endif

#if __APPLE__ && TARGET_OS_OSX
    char* basePath = SDL_GetBasePath();
    chdir(basePath);
    SDL_free(basePath);
#endif

#if __ANDROID__
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    chdir(SDL_AndroidGetExternalStoragePath());
#endif

#ifdef __3DS__
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

    osSetSpeedupEnable(true);
chdir("sdmc:/3ds/fallout2/");
    ctrModProfileStartup();
    // Register before engine handlers, so those handlers run first on exit.
    if (atexit(ctrShutdownRuntime) != 0) {
        ctrModProfileStage("Alpha12: failed to register runtime cleanup");
        return EXIT_FAILURE;
    }
    ctrModProfileStage("Alpha12: runtime cleanup registered");

// Prevent crashes when launched without arguments.
if (argc <= 0 || argv == nullptr || argv[0] == nullptr) {
    static char executableName[] = "fallout2";
    static char* fallbackArgv[] = { executableName, nullptr };
    argc = 1;
    argv = fallbackArgv;
}
#else

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        return EXIT_FAILURE;
    }

    atexit(SDL_Quit);

    SDL_ShowCursor(SDL_DISABLE);
#endif
    gProgramIsActive = true;
    rc = falloutMain(argc, argv);
#ifdef __3DS__
    ctrModProfileStage("falloutMain returned; explicit runtime shutdown begins");
    ctrShutdownRuntime();
#endif

#if _WIN32
    CloseHandle(GNW95_mutex);
#endif

    return rc;
}

} // namespace fallout

int main(int argc, char* argv[])
{
    return fallout::main(argc, argv);
}
