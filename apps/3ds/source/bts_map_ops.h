#ifndef BTS_MAP_OPS_H
#define BTS_MAP_OPS_H
/* Pure, deterministic LOCAL map transforms. Never writes to an ECU.
 * Caller validates all outputs against the ECU's INI numeric encoding and
 * commits the whole region atomically, or not at all. */
#include <math.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {int x0,y0,x1,y1;} BtsMapRegion;
enum {BTS_MAP_ADD, BTS_MAP_PERCENT, BTS_MAP_INTERP_X, BTS_MAP_INTERP_Y,
      BTS_MAP_SMOOTH, BTS_MAP_BILINEAR};
static inline BtsMapRegion bts_map_region(int w,int h,int anchor,int cursor,bool selected){
 int ax=(selected?anchor:cursor)%w, ay=(selected?anchor:cursor)/w;
 int bx=cursor%w, by=cursor/w;
 BtsMapRegion r={ax<bx?ax:bx,ay<by?ay:by,ax>bx?ax:bx,ay>by?ay:by};
 if(r.x0<0)r.x0=0;if(r.y0<0)r.y0=0;
 if(r.x1>=w)r.x1=w-1;if(r.y1>=h)r.y1=h-1;
 return r;
}
static inline int bts_map_transform(const double *src,double *dst,int w,int h,
 BtsMapRegion r,int operation,double amount){
 if(!src||!dst||w<1||h<1||w*h>4096||r.x0<0||r.y0<0||r.x1>=w||r.y1>=h||
    r.x0>r.x1||r.y0>r.y1||!isfinite(amount))return -1;
 for(int i=0;i<w*h;i++)dst[i]=src[i];
 int modified=0;
 for(int y=r.y0;y<=r.y1;y++)for(int x=r.x0;x<=r.x1;x++){
    int index=y*w+x;double old=src[index],v=old;
    if(!isfinite(old))return -1;
    switch(operation){
      case BTS_MAP_ADD:v=old+amount;break;
      case BTS_MAP_PERCENT:v=old*(1.0+amount/100.0);break;
      case BTS_MAP_INTERP_X:
        if(x==r.x0||x==r.x1)continue;
        if(r.x1-r.x0<2)return -1;
        if(!isfinite(src[y*w+r.x0])||!isfinite(src[y*w+r.x1]))return -1;
        v=src[y*w+r.x0]+(src[y*w+r.x1]-src[y*w+r.x0])*(double)(x-r.x0)/(r.x1-r.x0);
        break;
      case BTS_MAP_INTERP_Y:
        if(y==r.y0||y==r.y1)continue;
        if(r.y1-r.y0<2)return -1;
        if(!isfinite(src[r.y0*w+x])||!isfinite(src[r.y1*w+x]))return -1;
        v=src[r.y0*w+x]+(src[r.y1*w+x]-src[r.y0*w+x])*(double)(y-r.y0)/(r.y1-r.y0);
        break;
      case BTS_MAP_BILINEAR:{
        if(r.x1-r.x0<1||r.y1-r.y0<1)return -1;
        double a=src[r.y0*w+r.x0],b=src[r.y0*w+r.x1];
        double c=src[r.y1*w+r.x0],d=src[r.y1*w+r.x1];
        if(!isfinite(a)||!isfinite(b)||!isfinite(c)||!isfinite(d))return -1;
        double tx=(double)(x-r.x0)/(r.x1-r.x0);
        double ty=(double)(y-r.y0)/(r.y1-r.y0);
        v=(a*(1.0-tx)+b*tx)*(1.0-ty)+(c*(1.0-tx)+d*tx)*ty;
        break;
      }
      case BTS_MAP_SMOOTH:{
        double sum=old;int n=1;
        if(x>0&&isfinite(src[index-1])){sum+=src[index-1];n++;}
        if(x<w-1&&isfinite(src[index+1])){sum+=src[index+1];n++;}
        if(y>0&&isfinite(src[index-w])){sum+=src[index-w];n++;}
        if(y<h-1&&isfinite(src[index+w])){sum+=src[index+w];n++;}
        v=sum/n;break;
      }
      default:return -1;
    }
    if(!isfinite(v))return -1;
    dst[index]=v;
    if(fabs(v-old)>1e-9)modified++;
 }
 return modified;
}
#endif
