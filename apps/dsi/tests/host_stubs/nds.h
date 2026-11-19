#ifndef HOST_NDS_H
#define HOST_NDS_H
#include <stdint.h>
#define RGB15(r,g,b) ((r)|((g)<<5)|((b)<<10))
#define BIT(x) (1u<<(x))
#define MODE_5_2D 1
#define VRAM_A_MAIN_BG 0
#define VRAM_C_SUB_BG 0
#define BgType_Bmp16 2
#define BgSize_B16_256x256 3
#define KEY_TOUCH BIT(0)
#define KEY_LEFT BIT(1)
#define KEY_RIGHT BIT(2)
#define KEY_UP BIT(3)
#define KEY_DOWN BIT(4)
#define KEY_X BIT(5)
#define KEY_Y BIT(6)
#define KEY_L BIT(7)
#define KEY_R BIT(8)
#define KEY_B BIT(9)
#define KEY_A BIT(10)
#define KEY_SELECT BIT(11)
#define KEY_START BIT(12)
typedef struct {int px,py;} touchPosition;
static inline void videoSetMode(int a){(void)a;}
static inline void videoSetModeSub(int a){(void)a;}
static inline void vramSetBankA(int a){(void)a;}
static inline void vramSetBankC(int a){(void)a;}
static inline int bgInit(int a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}
static inline int bgInitSub(int a,int b,int c,int d,int e){return bgInit(a,b,c,d,e);}
static inline void*bgGetGfxPtr(int a){static uint16_t b[256*256];(void)a;return b;}
static inline void swiWaitForVBlank(void){}
static inline void scanKeys(void){}
static inline uint32_t keysDown(void){return 0;}
static inline uint32_t keysHeld(void){return 0;}
static inline void touchRead(touchPosition*p){(void)p;}
static inline int pmMainLoop(void){return 0;}
#endif
