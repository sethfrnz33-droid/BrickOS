#ifndef BRICK_GAUGE_HOLD_H
#define BRICK_GAUGE_HOLD_H
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <float.h>
/* Preserve the last decoded physical ECU value when packets are unavailable.
 * A missing or nonfinite field NEVER replaces a known-good reading with 0. */
static inline void gauge_hold_accept(float *cached, bool *present, double sample, bool decoded){
    if(!cached || !present || !decoded || !isfinite(sample) || sample>FLT_MAX || sample<-FLT_MAX)return;
    *cached=(float)sample;
    *present=true;
}
static inline void gauge_hold_reset(bool *present){
    if(present)*present=false;
}
static inline bool gauge_hold_is_stale(uint64_t now,uint64_t received,uint64_t limit){
    return received!=0 && now>received && now-received>limit;
}
#endif
