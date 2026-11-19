#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int32_t s32; typedef int32_t Result; typedef int Handle;
typedef int LightLock; typedef void* Thread;
typedef struct { int px,py; } touchPosition;
typedef struct { short dx,dy; } circlePosition;
#define R_SUCCEEDED(x) ((x)>=0)
#define R_FAILED(x) ((x)<0)
#define RESET_ONESHOT 1
#define GFX_TOP 0
#define GFX_BOTTOM 1
#define GFX_LEFT 0
#define KEY_A (1<<0)
#define KEY_B (1<<1)
#define KEY_X (1<<2)
#define KEY_Y (1<<3)
#define KEY_L (1<<4)
#define KEY_R (1<<5)
#define KEY_START (1<<6)
#define KEY_SELECT (1<<7)
#define KEY_DUP (1<<8)
#define KEY_DDOWN (1<<9)
#define KEY_DLEFT (1<<10)
#define KEY_DRIGHT (1<<11)
#define KEY_CPAD_UP (1<<12)
#define KEY_CPAD_DOWN (1<<13)
#define KEY_CPAD_LEFT (1<<14)
#define KEY_CPAD_RIGHT (1<<15)
#define KEY_TOUCH (1<<16)
/* New 3DS C-Stick HID bits (host syntax-only shim). */
#define KEY_CSTICK_RIGHT (1U<<24)
#define KEY_CSTICK_LEFT (1U<<25)
#define KEY_CSTICK_UP (1U<<26)
#define KEY_CSTICK_DOWN (1U<<27)
typedef int16_t s16;
typedef struct ndspWaveBuf {s16 *data_pcm16; u32 nsamples; bool looping; u8 status; struct ndspWaveBuf *next;} ndspWaveBuf;
#define NDSP_WBUF_FREE 0
#define NDSP_WBUF_QUEUED 1
#define NDSP_WBUF_PLAYING 2
#define NDSP_WBUF_DONE 3
#define NDSP_OUTPUT_STEREO 1
#define NDSP_FORMAT_MONO_PCM16 1
#define NDSP_INTERP_LINEAR 1
typedef struct {int opaque;} SwkbdState;
typedef int SwkbdButton;
#define SWKBD_TYPE_NORMAL 0
#define SWKBD_BUTTON_RIGHT 1

#ifndef GFX_RIGHT
#define GFX_RIGHT 1
#endif

/* Real libctru motion APIs, stubbed for source-level host verification. */
typedef struct {s16 x,y,z;} accelVector;
typedef struct {s16 x,z,y;} angularRate;
static inline Result HIDUSER_EnableAccelerometer(void){return 0;}
static inline Result HIDUSER_EnableGyroscope(void){return 0;}
static inline Result HIDUSER_DisableAccelerometer(void){return 0;}
static inline Result HIDUSER_DisableGyroscope(void){return 0;}
static inline Result HIDUSER_GetGyroscopeRawToDpsCoefficient(float *f){if(f)*f=.07f;return 0;}
static inline void hidAccelRead(accelVector *a){if(a){a->x=0;a->y=0;a->z=512;}}
static inline void hidGyroRead(angularRate *g){if(g){g->x=g->y=g->z=0;}}
