#ifndef BTS_LOG_COMPARE_H
#define BTS_LOG_COMPARE_H
/* Portable, read-only comparison math. Never sends ECU commands. */
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <strings.h>
#include <string.h>

#define BTS_COMPARE_CHANNELS 9
#define BTS_COMPARE_GAP_MS 500U

typedef struct {
    unsigned paired, skipped, base_valid, reference_valid;
    double base_sum, reference_sum, delta_sum, abs_delta_sum;
    double minimum_delta, maximum_delta, maximum_abs_delta;
} BtsCompareStats;
static inline void bts_compare_stats_init(BtsCompareStats *s){
    if(s){memset(s,0,sizeof(*s));s->minimum_delta=INFINITY;s->maximum_delta=-INFINITY;}
}
static inline void bts_compare_stats_add(BtsCompareStats *s,bool valid_a,double a,bool valid_b,double b){
    if(!s)return;
    if(valid_a&&isfinite(a))s->base_valid++;
    if(valid_b&&isfinite(b))s->reference_valid++;
    if(!valid_a||!valid_b||!isfinite(a)||!isfinite(b)){s->skipped++;return;}
    double d=b-a;s->paired++;s->base_sum+=a;s->reference_sum+=b;s->delta_sum+=d;
    s->abs_delta_sum+=fabs(d);if(d<s->minimum_delta)s->minimum_delta=d;
    if(d>s->maximum_delta)s->maximum_delta=d;
    if(fabs(d)>s->maximum_abs_delta)s->maximum_abs_delta=fabs(d);
}
static inline int bts_compare_channel_index(const char *name,
                      const char others[BTS_COMPARE_CHANNELS][64]){
    if(!name||!*name)return -1;
    for(int i=0;i<BTS_COMPARE_CHANNELS;i++)if(others[i][0]&&!strcasecmp(name,others[i]))return i;
    return -1;
}
/* Relative timestamps are compared from each log's first sample, not absolute
 * recording time. Return a real nearby sample only; never interpolate gaps. */
static inline int bts_compare_nearest(const uint32_t *timestamps,int count,
                                      uint32_t target,uint32_t maximum_gap){
    if(!timestamps||count<=0)return -1;
    int lo=0,hi=count;
    while(lo<hi){int m=lo+(hi-lo)/2;if(timestamps[m]<target)lo=m+1;else hi=m;}
    int closest=-1;uint32_t distance=UINT32_MAX;
    for(int k=lo-1;k<=lo;k++){
        if(k<0||k>=count)continue;
        uint32_t t=timestamps[k],d=t>target?t-target:target-t;
        if(d<distance){distance=d;closest=k;}
    }
    return distance<=maximum_gap?closest:-1;
}
#endif
