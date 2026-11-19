#ifndef BTS_CARD_MOTION_H
#define BTS_CARD_MOTION_H
/* Physics for floating gauge cards. Sensor inputs are calibrated/normalized
 * outside this module; this pure step can be regression tested on a host PC.
 * Positions are bounded in native 3DS pixels, not layout percentages. */
#include <math.h>
#include <stdbool.h>
#define BTS_CARD_COUNT 9

typedef struct { float x,y,vx,vy; } BTSCardBody;
typedef struct {
    BTSCardBody card[BTS_CARD_COUNT];
    float gravity_x,gravity_y,spin_x,spin_y,phase;
    bool sensor_active;
} BTSCardWorld;

static inline float bts_card_limit(float a,float low,float high){
    return a<low?low:a>high?high:a;
}
static inline void bts_card_motion_step(BTSCardWorld *w,float ax,float ay,
                                        float gyro_x,float gyro_y,float dt,bool valid){
    if(!w)return;
    dt=bts_card_limit(dt,0.001f,.055f);
    if(!valid){ax=ay=gyro_x=gyro_y=0;}
    ax=bts_card_limit(ax,-1,1);
    ay=bts_card_limit(ay,-1,1);
    gyro_x=bts_card_limit(gyro_x,-1,1);
    gyro_y=bts_card_limit(gyro_y,-1,1);
    const float gmix=bts_card_limit(5.4f*dt,0,1);
    const float rmix=bts_card_limit(8.0f*dt,0,1);
    w->gravity_x+=(ax-w->gravity_x)*gmix;
    w->gravity_y+=(ay-w->gravity_y)*gmix;
    w->spin_x+=(gyro_x-w->spin_x)*rmix;
    w->spin_y+=(gyro_y-w->spin_y)*rmix;
    w->phase+=dt;
    if(w->phase>5000.f)w->phase-=5000.f;
    w->sensor_active=valid;
    for(int i=0;i<BTS_CARD_COUNT;i++){
        BTSCardBody *b=&w->card[i];
        float mass=0.78f+0.045f*(float)(i%5);
        float slowx=.38f*sinf(w->phase*.74f+i*1.33f);
        float slowy=.44f*cosf(w->phase*.61f+i*1.09f);
        float target_x=bts_card_limit(w->gravity_x*2.35f*mass+slowx,-3.3f,3.3f);
        float target_y=bts_card_limit(w->gravity_y*1.65f*mass+slowy,-2.6f,2.6f);
        /* The gyro contributes velocity and rebound rather than becoming a
         * raw displacement; the accelerometer supplies the gravity target. */
        b->vx+=(19.0f*(target_x-b->x)+4.5f*w->spin_y)*dt;
        b->vy+=(19.0f*(target_y-b->y)-3.5f*w->spin_x)*dt;
        float drag=expf(-6.7f*dt);
        b->vx*=drag;b->vy*=drag;
        b->x=bts_card_limit(b->x+b->vx*dt,-3.8f,3.8f);
        b->y=bts_card_limit(b->y+b->vy*dt,-3.1f,3.1f);
    }
}
#endif
