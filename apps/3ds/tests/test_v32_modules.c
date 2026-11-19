/* Pure C unit tests for v3.2 math; run on host without a 3DS runtime. */
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "../source/bts_map_ops.h"
#include "../source/bts_log_insights.h"
static int near(double a,double b){return fabs(a-b)<1e-5;}
int main(void){
 double src[16],dst[16];for(int i=0;i<16;i++)src[i]=i;
 BtsMapRegion r=bts_map_region(4,4,5,10,true);
 assert(r.x0==1&&r.y0==1&&r.x1==2&&r.y1==2);
 assert(bts_map_transform(src,dst,4,4,r,BTS_MAP_ADD,2)==4);
 assert(near(dst[5],7)&&near(dst[10],12)&&near(dst[0],0));
 assert(bts_map_transform(src,dst,4,4,r,BTS_MAP_PERCENT,5)==4);
 assert(near(dst[5],5.25)&&near(dst[6],6.3));
 r=(BtsMapRegion){0,1,3,1};
 assert(bts_map_transform(src,dst,4,4,r,BTS_MAP_INTERP_X,0)==0);
 assert(near(dst[5],5)&&near(dst[6],6));
 double uneven[16]={0};uneven[4]=10;uneven[7]=40;
 assert(bts_map_transform(uneven,dst,4,4,r,BTS_MAP_INTERP_X,0)==2);
 assert(near(dst[5],20)&&near(dst[6],30));
 r=(BtsMapRegion){1,0,1,3};uneven[1]=0;uneven[13]=90;
 assert(bts_map_transform(uneven,dst,4,4,r,BTS_MAP_INTERP_Y,0)==2);
 assert(near(dst[5],30)&&near(dst[9],60));
 r=(BtsMapRegion){1,1,1,1};
 assert(bts_map_transform(src,dst,4,4,r,BTS_MAP_SMOOTH,0)==0);
 assert(bts_map_transform(src,dst,4,4,(BtsMapRegion){-1,0,1,1},BTS_MAP_ADD,1)==-1);
 double invalid[16];memcpy(invalid,src,sizeof invalid);invalid[5]=NAN;
 assert(bts_map_transform(invalid,dst,4,4,r,BTS_MAP_ADD,1)==-1);
 /* Missing telemetry remains missing; histogram never invents samples. */
 float v[]={1,2,3,4,5,6,7,8,9,10};
 uint32_t t[]={0,100,200,300,400,500,600,700,800,3000};
 uint8_t has[]={1,1,1,0,1,1,1,1,1,1};
 BtsLogInsights stats;double scratch[10];
 assert(bts_log_inspect(v,t,has,10,&stats,scratch));
 assert(stats.samples==10&&stats.valid==9&&stats.missing==1&&stats.gaps==1);
 assert(stats.max_gap_ms==2200&&stats.duration_ms==3000);
 assert(near(stats.minimum,1)&&near(stats.maximum,10));
 assert(near(stats.median,6)&&stats.p05<stats.median&&stats.p95>stats.median);
 unsigned total=0;for(int i=0;i<BTS_HIST_BINS;i++)total+=stats.hist[i];assert(total==9);
 uint8_t none[10]={0};assert(bts_log_inspect(v,t,none,10,&stats,scratch));
 assert(stats.valid==0&&isnan(stats.median));
 puts("PASS v3.2: 16 map transform, bounds, missing-data and log-stat checks");return 0;
}
