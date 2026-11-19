#ifndef BTS_LOG_QUALITY_H
#define BTS_LOG_QUALITY_H
/* Recorded-data availability report: it NEVER diagnoses an ECU hardware fault.
 * Constant sensor values may be normal, so no automatic failure inference. */
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

typedef struct {
    unsigned total,valid,missing,repeated_timestamps,long_gaps,longest_constant_run;
    uint32_t maximum_observed_gap_ms,time_of_largest_gap_ms;
    double first_valid,last_valid;
} BtsSignalQuality;
static inline bool bts_signal_quality(const float *values,const uint32_t *times,
     const uint8_t *has,int n,BtsSignalQuality *out){
    if(!values||!times||!has||!out||n<1||n>8192)return false;
    *out=(BtsSignalQuality){0};out->total=(unsigned)n;
    uint64_t dt_sum=0;
    for(int i=1;i<n;i++){
       if(times[i]<times[i-1])return false;
       uint32_t delta=times[i]-times[i-1];dt_sum+=delta;
       if(!delta)out->repeated_timestamps++;
       if(delta>out->maximum_observed_gap_ms){
           out->maximum_observed_gap_ms=delta;out->time_of_largest_gap_ms=times[i];
       }
    }
    double mean_dt=n>1?(double)dt_sum/(n-1):0;
    for(int i=1;i<n;i++)if(mean_dt>0&&times[i]-times[i-1]>3*mean_dt)out->long_gaps++;
    unsigned run=0;float previous=0;bool previous_ok=false;
    for(int i=0;i<n;i++){
       bool ok=has[i]&&isfinite(values[i]);
       if(!ok){out->missing++;run=0;previous_ok=false;continue;}
       if(!out->valid)out->first_valid=values[i];
       out->last_valid=values[i];out->valid++;
       if(previous_ok&&values[i]==previous)run++;else run=1;
       if(run>out->longest_constant_run)out->longest_constant_run=run;
       previous=values[i];previous_ok=true;
    }
    return true;
}
#endif
