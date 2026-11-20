#pragma once
#include <stdbool.h>
#include <math.h>
/* Bench-only internal-stimulation gate. False for missing, stale or invalid
 * measurements; once this session has acknowledged Start, synthetic RPM is
 * permitted but the missing-battery-voltage condition remains mandatory. */
static inline bool brick_stim_policy(bool active_by_this_bridge,
                                     bool fresh_rpm, double rpm,
                                     bool fresh_battery, double vbat) {
    return fresh_rpm && fresh_battery && isfinite(rpm) && isfinite(vbat) &&
           rpm >= -0.1 && rpm <= 35000.0 &&
           vbat >= -0.1 && vbat < 1.0 &&
           (active_by_this_bridge || fabs(rpm) <= 0.1);
}


/*
 * In the harness-isolated bench profile, the ECU can remain powered to
 * communicate via USB while its external harness supply is hard-isolated.
 * Caller MUST verify a stable GPIO8 HIGH at every preflight/start, with a
 * continuously monitored GPIO8 watchdog and Stop on exit/unsafe transition.
 */
static inline bool brick_stim_isolated_policy(bool active_by_this_bridge,
                                              bool fresh_rpm,double rpm,
                                              bool fresh_battery,double vbat,
                                              bool gpio8_high_verified) {
    return gpio8_high_verified && fresh_rpm && fresh_battery &&
           isfinite(rpm) && isfinite(vbat) &&
           rpm>=-0.1 && rpm<=35000.0 &&
           vbat>=-0.1 && vbat<=36.0 &&
           (active_by_this_bridge || fabs(rpm)<=0.1);
}
