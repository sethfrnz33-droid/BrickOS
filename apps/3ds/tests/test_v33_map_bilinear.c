#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "../source/bts_map_ops.h"
int main(void){
 double src[25]={0},dst[25];
 src[0]=10;src[4]=30;src[20]=20;src[24]=40;
 BtsMapRegion r={0,0,4,4};
 assert(bts_map_transform(src,dst,5,5,r,BTS_MAP_BILINEAR,0)>0);
 assert(fabs(dst[2+2*5]-25)<1e-9);
 assert(fabs(dst[2]-20)<1e-9);
 assert(fabs(dst[0]-10)<1e-9&&fabs(dst[24]-40)<1e-9);
 assert(bts_map_transform(src,dst,5,5,(BtsMapRegion){2,2,2,4},BTS_MAP_BILINEAR,0)==-1);
 src[24]=NAN;
 assert(bts_map_transform(src,dst,5,5,r,BTS_MAP_BILINEAR,0)==-1);
 puts("PASS bilinear map interpolation: correct corners and edge interpolation, rejects invalid input");
}
