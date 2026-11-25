// Minimal SDL declarations for host-only tests; never used by Makefile.ctr.
#pragma once
#include <stddef.h>
struct SDL_Window; struct SDL_Surface; struct SDL_Renderer; struct SDL_Texture; union SDL_Event;
#define SDL_NUM_SCANCODES 512
extern "C" {
int SDL_strcasecmp(const char*, const char*);
int SDL_strncasecmp(const char*, const char*, size_t);
char* SDL_strupr(char*);
char* SDL_strlwr(char*);
char* SDL_itoa(int, char*, int);
char* SDL_strdup(const char*);
}

#ifndef SDL_SCANCODE_LALT
#define SDL_SCANCODE_LALT 226
#define SDL_SCANCODE_RALT 230
#endif
