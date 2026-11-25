#include <cstring>
#include <cassert>
#include <cctype>
#include <cstdio>
using size_t = std::size_t;
enum SwkbdCallbackResult { SWKBD_CALLBACK_OK, SWKBD_CALLBACK_CONTINUE };
enum SwkbdButton { SWKBD_BUTTON_LEFT, SWKBD_BUTTON_RIGHT, SWKBD_BUTTON_NONE };
enum { SWKBD_TYPE_QWERTY, SWKBD_ANYTHING, SWKBD_NOTEMPTY_NOTBLANK, SDL_KEYDOWN, SDL_TEXTINPUT };
struct SwkbdState {};
using Callback = SwkbdCallbackResult (*)(void*, const char**, const char*, size_t);
static Callback cb; static void* cbdata; static const char* answer; static SwkbdButton pressed;
static int limit; static int validation; static bool leftSubmit, rightSubmit;
static const char* initialText;
static void swkbdInit(SwkbdState*,int,int,int len){limit=len;}
static void swkbdSetValidation(SwkbdState*,int v,int,int){validation=v;}
static void swkbdSetInitialText(SwkbdState*,const char* t){initialText=t;}
static void swkbdSetHintText(SwkbdState*,const char*){}
static void swkbdSetButton(SwkbdState*,SwkbdButton b,const char*,bool s){if(b==SWKBD_BUTTON_LEFT) leftSubmit=s; else rightSubmit=s;}
static void swkbdSetFilterCallback(SwkbdState*,Callback c,void* d){cb=c;cbdata=d;}
static SwkbdButton swkbdInputText(SwkbdState*,char* out,size_t n){assert(initialText);strncpy(out,answer,n-1);return pressed;}
static void renderPresent(){} static void ctr_gfx_invalidate(){} static void hidScanInput(){} static int hidKeysHeld(){return 0;}
static void SDL_FlushEvents(int,int){} static int oldpad;
static struct { struct {int kHeld;} frame; } ctr_input;
static bool _isdoschar(int c){return isalnum(c)||strchr("#@!$`'~^&()-_=[]{}",c)!=nullptr;}
