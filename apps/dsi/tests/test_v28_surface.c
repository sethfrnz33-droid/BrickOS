#include "surface3d.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SW 256
#define SH 192
#define BG 0x8c63u
static uint16_t guarded[SW*SH+16];
static double cells[2048],xbins[64],ybins[64];
static unsigned changed_pixels(void){
    unsigned n=0;
    for(int i=0;i<SW*SH;i++)if(guarded[i+8]!=BG)n++;
    return n;
}
static uint32_t checksum(void){
    uint32_t h=2166136261u;
    for(int i=0;i<SW*SH;i++){h^=guarded[i+8];h*=16777619u;}
    return h;
}
static void reset_fb(void){
    for(size_t i=0;i<sizeof(guarded)/sizeof(guarded[0]);i++)guarded[i]=0x1357;
    for(int i=0;i<SW*SH;i++)guarded[i+8]=BG;
}
static void assert_guards(void){
    for(int i=0;i<8;i++){assert(guarded[i]==0x1357);assert(guarded[8+SW*SH+i]==0x1357);}
}
static void preview(const char *filename){
    FILE *f=fopen(filename,"wb");if(!f)return;
    fprintf(f,"P6\n%d %d\n255\n",SW,SH);
    for(int i=0;i<SW*SH;i++){
        uint16_t v=guarded[i+8];
        unsigned char rgb[3]={(unsigned char)((v&31)*255/31),
          (unsigned char)(((v>>5)&31)*255/31),(unsigned char)(((v>>10)&31)*255/31)};
        fwrite(rgb,1,3,f);
    }
    fclose(f);
}
int main(int argc,char **argv){
    for(int y=0;y<16;y++)for(int x=0;x<16;x++){
        double dx=x-7.5,dy=y-7.5;
        cells[y*16+x]=50+30*exp(-(dx*dx+dy*dy)/34.0)
                      +12*sin((double)x*.55)*cos((double)y*.38);
    }
    for(int i=0;i<16;i++){xbins[i]=600+i*i*15;ybins[i]=15+i*4;}
    BtSurface3D s={.values=cells,.xbins=xbins,.ybins=ybins,.width=16,.height=16,
      .has_x=true,.has_y=true,.yaw=.62,.elevation=.72,
      .selected_x=6,.selected_y=7,.trace_valid=true,.trace_x=12,.trace_y=10};
    reset_fb();bt_surface3d_draw(&guarded[8],&s);assert_guards();
    unsigned colored=changed_pixels();uint32_t a=checksum();
    assert(colored>2000&&colored<25000);
    if(argc>1)preview(argv[1]);
    s.yaw+=.9;
    reset_fb();bt_surface3d_draw(&guarded[8],&s);assert_guards();
    uint32_t b=checksum();assert(a!=b);
    s.elevation=.28;
    reset_fb();bt_surface3d_draw(&guarded[8],&s);assert_guards();
    assert(b!=checksum());
    /* Large INI tables decimate to 17x17; avoid reading beyond axis arrays. */
    for(int i=0;i<2048;i++)cells[i]=(i%64)*.6+(i/64)*.5;
    s.width=64;s.height=32;s.has_x=false;s.has_y=false;
    s.selected_x=63;s.selected_y=31;s.trace_x=0;s.trace_y=0;
    reset_fb();bt_surface3d_draw(&guarded[8],&s);assert_guards();
    assert(changed_pixels()>300);
    /* Intentionally invalid structures leave the existing display intact. */
    s.width=0;uint32_t before=checksum();bt_surface3d_draw(&guarded[8],&s);
    assert(checksum()==before);
    printf("PASS 3D surface: %u rendered pixels; distinct yaw/tilt; trace; color; large maps; framebuffer bounds\n",colored);
}
