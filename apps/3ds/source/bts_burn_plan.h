#ifndef BTS_BURN_PLAN_H
#define BTS_BURN_PLAN_H
#include <stdint.h>
/* Produces bounded contiguous spans including at most max_gap bytes of
 * unchanged baseline between actual changes. The caller must first verify
 * ALL baseline bytes against ECU memory, including included gap bytes.
 * Returns the next scan position, or total if nothing remains. */
static inline int bts_next_burn_span(const uint8_t *baseline,
    const uint8_t *edited,int total,int scan,int max_write,int max_gap,
    int *start,int *length){
 if(start)*start=0;
 if(length)*length=0;
 if(!baseline||!edited||!start||!length||total<0||scan<0||
    scan>total||max_write<1||max_gap<0)return total;
 while(scan<total&&baseline[scan]==edited[scan])scan++;
 if(scan>=total)return total;
 int last_change=scan;
 for(int idx=scan+1;idx<total&&idx-scan<max_write;idx++){
    if(baseline[idx]!=edited[idx])last_change=idx;
    else if(idx-last_change>max_gap)break;
 }
 *start=scan;
 *length=last_change-scan+1;
 return last_change+1;
}
#endif
