#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "../source/trigger_preview.h"

int main(void) {
    assert(DREAM_WHEEL_COUNT == 5);
    for (int w=0;w<DREAM_WHEEL_COUNT;w++) {
        const DreamTriggerWheel *wheel=&dream_wheels[w];
        assert(dream_crank_high(0.0,wheel));
        assert(dream_crank_high(360.0,wheel));
        assert(!dream_crank_high(360.0-wheel->missing*360.0/wheel->teeth+0.001,wheel)
               || wheel->missing==0); // first missing interval starts here
        assert(dream_crank_high(0.0,wheel)==dream_crank_high(720.0,wheel));
        assert(dream_crank_high(75.1,wheel)==dream_crank_high(435.1,wheel));
    }
    assert(dream_cam_high(0.0));
    assert(dream_cam_high(720.0));
    assert(!dream_cam_high(26.0));
    assert(!dream_cam_high(359.0));
    assert(!dream_cam_high(719.9));
    assert(fabs(dream_angle_at_time(1000.0,60)-360.0)<.001);
    assert(fabs(dream_angle_at_time(1000.0,120)-0.0)<.001);
    assert(dream_angle_at_time(1000.0,0)==0.0);
    assert(dream_clamp_rpm(-1)==100);
    assert(dream_clamp_rpm(9999)==8000);
    printf("PASS: offline trigger model timing, gaps, cam, RPM limits\n");
    return 0;
}
