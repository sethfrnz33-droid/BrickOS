#ifndef TEST_NDS_H
#define TEST_NDS_H
#include <stdint.h>
#define RGB15(r,g,b) (((r)&31)|(((g)&31)<<5)|(((b)&31)<<10))
#define BIT(n) (1u<<(n))
typedef uint32_t u32;
typedef struct {int px,py;} touchPosition;
#define MODE_5_2D 1
#define VRAM_A_MAIN_BG 1
#define VRAM_C_SUB_BG 1
#define BgType_Bmp16 1
#define BgSize_B16_256x256 1
#define KEY_TOUCH 1
#define KEY_SELECT 2
#define KEY_START 4
#define KEY_L 8
#define KEY_R 16
#define KEY_A 32
#define KEY_B 64
#define KEY_X 128
#define KEY_Y 256
#define KEY_UP 512
#define KEY_DOWN 1024
#define KEY_LEFT 2048
#define KEY_RIGHT 4096
static inline void videoSetMode(int x){(void)x;}
static inline void videoSetModeSub(int x){(void)x;}
static inline void vramSetBankA(int x){(void)x;}
static inline void vramSetBankC(int x){(void)x;}
static inline int bgInit(int a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;return 1;}
static inline int bgInitSub(int a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;return 2;}
static inline void *bgGetGfxPtr(int x){static unsigned short mem[256*256];(void)x;return mem;}
static inline int pmMainLoop(void){return 0;}
static inline int isDSiMode(void){return 1;}
static inline void swiWaitForVBlank(void){}
static inline void scanKeys(void){}
static inline unsigned keysDown(void){return 0;}
static inline unsigned keysHeld(void){return 0;}
static inline void touchRead(touchPosition *t){t->px=0;t->py=0;}
#define SoundFormat_8Bit 1
static inline void soundEnable(void){}
static inline void DC_FlushAll(void){}
static inline void soundKill(int soundId){(void)soundId;}
static inline int soundPlaySample(const void *data,int format,uint32_t size,unsigned short hz,unsigned char vol,unsigned char pan,int loop,unsigned short point){
    (void)data;(void)format;(void)size;(void)hz;(void)vol;(void)pan;(void)loop;(void)point;return 0;
}
#endif
