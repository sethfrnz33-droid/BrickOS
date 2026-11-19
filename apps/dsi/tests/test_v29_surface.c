/* Inspect the private 3D projection to guarantee uniform visual cell spacing
 * despite irregular ECU X/Y bins. Numeric bins remain unchanged. */
#include "../source/surface3d.c"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static double cells[16],x[4]={500,750,800,6500},y[4]={20,21,89,90};
int main(void){
    BtSurface3D s={0};s.values=cells;s.xbins=x;s.ybins=y;
    s.width=4;s.height=4;s.has_x=true;s.has_y=true;
    s.yaw=0;s.elevation=.7;
    for(int i=0;i<16;i++)cells[i]=i/4;
    min_z=0;max_z=5;
    s_yaw=sin(s.yaw);c_yaw=cos(s.yaw);
    s_elev=sin(s.elevation);c_elev=cos(s.elevation);
    axis_x_min=500;axis_x_max=6500;
    axis_y_min=20;axis_y_max=90;
    use_axis_x=use_axis_y=true;
    Vertex v[4];for(int i=0;i<4;i++)v[i]=project(&s,i,0);
    int d0=v[1].x-v[0].x,d1=v[2].x-v[1].x,d2=v[3].x-v[2].x;
    assert(abs(d0-d1)<=2&&abs(d1-d2)<=2);
    assert(x[3]==6500&&y[2]==89); /* source ECU axis values preserved */
    printf("PASS v29 surface: uniform cell widths %d / %d / %d with nonlinear axes\n",d0,d1,d2);
}
