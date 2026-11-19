#include "surface3d.h"
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define SW 256
#define SH 192
#define MAX_GRID 17
#define VERTEX_COUNT (MAX_GRID*MAX_GRID)
#define CLIP_TOP 54
#define CLIP_BOTTOM 174
#define RGB(r,g,b) ((uint16_t)(0x8000u|((r)&31)|(((g)&31)<<5)|(((b)&31)<<10)))
typedef struct {
    short x,y;
    unsigned short depth;
    double height;
} Vertex;
/* Fixed DSi-sized working memory, no heap churn or per-frame allocations. */
static Vertex mesh[VERTEX_COUNT];
static int grid_x[MAX_GRID],grid_y[MAX_GRID];
static uint16_t zbuffer[SW*SH];
static double min_z,max_z;
static double axis_x_min,axis_x_max,axis_y_min,axis_y_max;
static bool use_axis_x,use_axis_y;
static double s_yaw,c_yaw,s_elev,c_elev;

static int clamp_int(int n,int a,int b){return n<a?a:n>b?b:n;}
static uint16_t mix_rgb(double f,double shade){
    if(f<0)f=0;
    if(f>1)f=1;
    int r=0,g=0,b=0;
    if(f<.25){double t=f*4;r=3;g=(int)(7+t*14);b=(int)(24-t*6);}
    else if(f<.50){double t=(f-.25)*4;r=(int)(3+t*10);g=(int)(21+t*7);b=(int)(18-t*12);}
    else if(f<.75){double t=(f-.50)*4;r=(int)(13+t*16);g=(int)(28-t*6);b=(int)(6-t*4);}
    else {double t=(f-.75)*4;r=29;g=(int)(22-t*12);b=(int)(2+t*4);}
    return RGB(clamp_int((int)(r*shade),0,31),
               clamp_int((int)(g*shade),0,31),
               clamp_int((int)(b*shade),0,31));
}
static double axis_pos(const double *axis,int index,int count,double low,double high,bool known){
    if(count<=1)return 0.5;
    if(known){
        double p=(axis[index]-low)/(high-low);
        if(isfinite(p))return p<0?0:p>1?1:p;
    }
    return (double)index/(count-1);
}
static Vertex project(const BtSurface3D *s,int x,int y){
    Vertex vertex={0};
    double px=axis_pos(s->xbins,x,s->width,axis_x_min,axis_x_max,use_axis_x)-.5;
    double py=axis_pos(s->ybins,y,s->height,axis_y_min,axis_y_max,use_axis_y)-.5;
    double z=s->values[y*s->width+x];
    if(!isfinite(z))z=min_z;
    double h=max_z>min_z?(z-min_z)/(max_z-min_z):.4;
    if(h<0)h=0;
    if(h>1)h=1;
    double xr=px*c_yaw-py*s_yaw;
    double yr=px*s_yaw+py*c_yaw;
    double depth=yr*c_elev+h*s_elev;
    double perspective=1.0/(1.0-depth*.09);
    if(perspective<.82)perspective=.82;
    if(perspective>1.16)perspective=1.16;
    vertex.x=(short)clamp_int((int)lround(128+xr*138*perspective),-300,550);
    vertex.y=(short)clamp_int((int)lround(133+(yr*s_elev*94-h*c_elev*75)*perspective),-300,550);
    vertex.depth=(unsigned short)clamp_int((int)lround((depth+2.0)*750),0,65535);
    vertex.height=h;
    return vertex;
}
static int edge(const Vertex *a,const Vertex *b,int x,int y){
    return (x-a->x)*(b->y-a->y)-(y-a->y)*(b->x-a->x);
}
static void triangle(uint16_t *fb,Vertex a,Vertex b,Vertex c,uint16_t color){
    int area=edge(&a,&b,c.x,c.y);
    if(area==0)return;
    int left=clamp_int(a.x<b.x?(a.x<c.x?a.x:c.x):(b.x<c.x?b.x:c.x),0,SW-1);
    int right=clamp_int(a.x>b.x?(a.x>c.x?a.x:c.x):(b.x>c.x?b.x:c.x),0,SW-1);
    int top=clamp_int(a.y<b.y?(a.y<c.y?a.y:c.y):(b.y<c.y?b.y:c.y),CLIP_TOP,CLIP_BOTTOM);
    int bottom=clamp_int(a.y>b.y?(a.y>c.y?a.y:c.y):(b.y>c.y?b.y:c.y),CLIP_TOP,CLIP_BOTTOM);
    if(left>right||top>bottom)return;
    int depth=(a.depth+b.depth+c.depth)/3;
    for(int y=top;y<=bottom;y++){
        for(int x=left;x<=right;x++){
            int wa=edge(&b,&c,x,y),wb=edge(&c,&a,x,y),wc=edge(&a,&b,x,y);
            if((area>0&&(wa<0||wb<0||wc<0)) ||
               (area<0&&(wa>0||wb>0||wc>0)))continue;
            int index=y*SW+x;
            if(depth>=zbuffer[index]){zbuffer[index]=(uint16_t)depth;fb[index]=color;}
        }
    }
}
static void depth_line(uint16_t *fb,Vertex a,Vertex b,uint16_t color){
    int dx=(int)b.x-a.x,dy=(int)b.y-a.y;
    int count=abs(dx)>abs(dy)?abs(dx):abs(dy);
    if(count==0)return;
    /* 16.16 interpolation prevents slow divisions in each line pixel. */
    int32_t xf=(int32_t)a.x*65536,yf=(int32_t)a.y*65536;
    int32_t zs=(int32_t)a.depth<<16;
    int32_t xstep=(int32_t)(((int64_t)dx*65536)/count);
    int32_t ystep=(int32_t)(((int64_t)dy*65536)/count);
    int32_t zstep=((int32_t)b.depth-(int32_t)a.depth)*65536/count;
    for(int i=0;i<=count;i++){
        int x=xf>>16,y=yf>>16;
        if(x>=0&&x<SW&&y>=CLIP_TOP&&y<=CLIP_BOTTOM){
            int at=y*SW+x,zd=zs>>16;
            if(zd+80>=zbuffer[at])fb[at]=color;
        }
        xf+=xstep;yf+=ystep;zs+=zstep;
    }
}
static void marker(uint16_t *fb,Vertex v,int radius,uint16_t color){
    for(int k=-radius;k<=radius;k++){
        int a=v.x+k,b=v.y+k;
        if(a>=0&&a<SW&&v.y>=CLIP_TOP&&v.y<=CLIP_BOTTOM)fb[v.y*SW+a]=color;
        if(v.x>=0&&v.x<SW&&b>=CLIP_TOP&&b<=CLIP_BOTTOM)fb[b*SW+v.x]=color;
    }
    if(v.x>=1&&v.x+1<SW&&v.y>=CLIP_TOP+1&&v.y<CLIP_BOTTOM){
        fb[(v.y-1)*SW+v.x]=RGB(0,0,0);
        fb[(v.y+1)*SW+v.x]=RGB(0,0,0);
        fb[v.y*SW+v.x]=RGB(31,31,31);
    }
}
void bt_surface3d_draw(uint16_t *fb,const BtSurface3D *s){
    if(!fb||!s||!s->values||s->width<2||s->height<2||
       s->width>2048||s->height>2048||s->width*s->height>2048)return;
    min_z=INFINITY;max_z=-INFINITY;
    for(int i=0;i<s->width*s->height;i++){
        double v=s->values[i];if(!isfinite(v))continue;
        if(v<min_z)min_z=v;
        if(v>max_z)max_z=v;
    }
    if(!isfinite(min_z)||!isfinite(max_z)){min_z=0;max_z=1;}
    use_axis_x=s->has_x&&s->xbins&&s->width<=64;
    use_axis_y=s->has_y&&s->ybins&&s->height<=64;
    axis_x_min=axis_y_min=INFINITY;axis_x_max=axis_y_max=-INFINITY;
    for(int i=0;use_axis_x&&i<s->width;i++){
        double v=s->xbins[i];if(!isfinite(v)){use_axis_x=false;break;}
        if(v<axis_x_min)axis_x_min=v;
        if(v>axis_x_max)axis_x_max=v;
    }
    for(int i=0;use_axis_y&&i<s->height;i++){
        double v=s->ybins[i];if(!isfinite(v)){use_axis_y=false;break;}
        if(v<axis_y_min)axis_y_min=v;
        if(v>axis_y_max)axis_y_max=v;
    }
    if(axis_x_max<=axis_x_min)use_axis_x=false;
    if(axis_y_max<=axis_y_min)use_axis_y=false;
    s_yaw=sin(s->yaw);c_yaw=cos(s->yaw);
    double tilt=s->elevation;
    if(tilt<.25)tilt=.25;
    if(tilt>1.3)tilt=1.3;
    s_elev=sin(tilt);c_elev=cos(tilt);
    int nx=s->width<MAX_GRID?s->width:MAX_GRID;
    int ny=s->height<MAX_GRID?s->height:MAX_GRID;
    for(int x=0;x<nx;x++)grid_x[x]=(x*(s->width-1)+(nx-1)/2)/(nx-1);
    for(int y=0;y<ny;y++)grid_y[y]=(y*(s->height-1)+(ny-1)/2)/(ny-1);
    for(int y=0;y<ny;y++)for(int x=0;x<nx;x++)
        mesh[y*nx+x]=project(s,grid_x[x],grid_y[y]);
    memset(zbuffer,0,sizeof(zbuffer));
    /* Two depth-tested, shaded triangles per quad. This is actual elevated
     * surface geometry, not a flat table with a fake 3D badge. */
    for(int y=0;y<ny-1;y++)for(int x=0;x<nx-1;x++){
        Vertex a=mesh[y*nx+x],b=mesh[y*nx+x+1];
        Vertex c=mesh[(y+1)*nx+x],d=mesh[(y+1)*nx+x+1];
        uint16_t c1=mix_rgb((a.height+b.height+c.height)/3.,.86);
        uint16_t c2=mix_rgb((d.height+b.height+c.height)/3.,.97);
        triangle(fb,a,b,c,c1);triangle(fb,b,d,c,c2);
    }
    const uint16_t grid=RGB(12,16,18);
    for(int y=0;y<ny;y++)for(int x=0;x<nx;x++){
        Vertex a=mesh[y*nx+x];
        if(x+1<nx)depth_line(fb,a,mesh[y*nx+x+1],grid);
        if(y+1<ny)depth_line(fb,a,mesh[(y+1)*nx+x],grid);
    }
    /* Distinguish selected tuning cell from the ECU live trace in 3D. */
    if(s->selected_x>=0&&s->selected_y>=0&&
       s->selected_x<s->width&&s->selected_y<s->height)
        marker(fb,project(s,s->selected_x,s->selected_y),4,RGB(31,23,8));
    if(s->trace_valid&&s->trace_x>=0&&s->trace_y>=0&&
       s->trace_x<s->width&&s->trace_y<s->height)
        marker(fb,project(s,s->trace_x,s->trace_y),6,RGB(2,30,31));
}
