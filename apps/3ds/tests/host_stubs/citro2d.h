#pragma once
#include <3ds.h>
typedef void* C2D_TextBuf;
typedef struct {int unused;} C2D_Text;
typedef struct {float width,height;} C2D_SubTex;
typedef struct {C2D_SubTex *subtex;} C2D_Image;
typedef void* C2D_SpriteSheet;
typedef struct {} C3D_RenderTarget;
#define C2D_DEFAULT_MAX_OBJECTS 4096
#define C3D_DEFAULT_CMDBUF_SIZE 1024
#define C3D_FRAME_SYNCDRAW 0
#define C2D_WithColor 1
static inline C3D_RenderTarget *C2D_CreateScreenTarget(int a,int b){(void)a;(void)b;return NULL;}
static inline C2D_TextBuf C2D_TextBufNew(int x){(void)x;return NULL;}
static inline C2D_SpriteSheet C2D_SpriteSheetLoadFromMem(const void*a,size_t n){(void)a;(void)n;return NULL;}
static inline C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet s,int i){(void)s;(void)i;return (C2D_Image){NULL};}
static inline u32 C2D_Color32(int r,int g,int b,int a){(void)r;(void)g;(void)b;(void)a;return 0;}
