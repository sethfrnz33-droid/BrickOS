#ifndef BTS_DSI_SURFACE3D_H
#define BTS_DSI_SURFACE3D_H
#include <stdint.h>
#include <stdbool.h>
/* Actual height-field rasterization in software for Nintendo DS/DSi bitmap
 * screens; each Z cell is mapped to a 3D vertex, shaded and depth tested.
 * Axes remain the INI-provided numeric X/Y values, not guessed channels. */
typedef struct {
    const double *values, *xbins, *ybins;
    int width, height;
    bool has_x, has_y;
    double yaw, elevation;
    int selected_x,selected_y,trace_x,trace_y;
    bool trace_valid;
} BtSurface3D;
void bt_surface3d_draw(uint16_t *framebuffer,const BtSurface3D *surface);
#endif
