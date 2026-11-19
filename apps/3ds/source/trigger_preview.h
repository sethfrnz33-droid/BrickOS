#ifndef DREAM_TRIGGER_PREVIEW_H
#define DREAM_TRIGGER_PREVIEW_H

/* Deterministic, non-actuating display model. These calculations never send
 * commands to an ECU, create voltages, or change ECU calibration.
 * Angle is measured in crankshaft degrees across the 720-degree 4-stroke cycle.
 */
#include <stdbool.h>
#include <math.h>

typedef struct {
    int teeth;
    int missing;
    const char *name;
} DreamTriggerWheel;

static const DreamTriggerWheel dream_wheels[] = {
    {36, 1, "36-1"},
    {60, 2, "60-2"},
    {24, 1, "24-1"},
    {12, 1, "12-1"},
    {24, 0, "24-0"},
};
#define DREAM_WHEEL_COUNT ((int)(sizeof(dream_wheels)/sizeof(dream_wheels[0])))

static inline double dream_wrap_angle(double angle) {
    if (!isfinite(angle)) return 0.0;
    angle=fmod(angle,720.0);
    return angle<0.0?angle+720.0:angle;
}

/* Display is high during first 42% of an existing tooth; missing teeth are
 * omitted at the end of each crank rotation. No electrical output. */
static inline bool dream_crank_high(double angle, const DreamTriggerWheel *wheel) {
    if (!wheel || wheel->teeth<2 || wheel->missing<0 || wheel->missing>=wheel->teeth)
        return false;
    double rotation=fmod(dream_wrap_angle(angle),360.0);
    double teeth_angle=rotation*(double)wheel->teeth/360.0;
    int tooth=(int)floor(teeth_angle);
    return tooth<(wheel->teeth-wheel->missing) &&
           (teeth_angle-(double)tooth)<0.42;
}

/* Single reference cam pulse per complete four-stroke cycle, illustrative only. */
static inline bool dream_cam_high(double angle) {
    return dream_wrap_angle(angle)<25.0;
}

static inline double dream_angle_at_time(double elapsed_ms, int rpm) {
    if (rpm<=0 || !isfinite(elapsed_ms)) return 0.0;
    return dream_wrap_angle(elapsed_ms*(double)rpm*360.0/60000.0);
}

static inline int dream_clamp_rpm(int rpm) {
    if (rpm<100) return 100;
    if (rpm>8000) return 8000;
    return rpm;
}

#endif
