#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../source/gauge_hold.h"
int main(void){
    float last=0;
    bool valid=false;
    gauge_hold_accept(&last,&valid,1625.4,true);
    assert(valid && fabsf(last-1625.4f)<0.02f);
    gauge_hold_accept(&last,&valid,0.0,false); /* one missing packet */
    assert(valid && fabsf(last-1625.4f)<0.02f);
    gauge_hold_accept(&last,&valid,NAN,true); /* corrupt/nonfinite data */
    assert(valid && fabsf(last-1625.4f)<0.02f);
    gauge_hold_accept(&last,&valid,1e300,true); /* float overflow */
    assert(valid && fabsf(last-1625.4f)<0.02f);
    assert(!gauge_hold_is_stale(1200,1000,900));
    assert(gauge_hold_is_stale(2000,1000,900));
    /* A 10-minute outage holds the value, with a conspicuous STALE label. */
    assert(gauge_hold_is_stale(601000,1000,900));
    assert(valid && fabsf(last-1625.4f)<0.02f);
    gauge_hold_accept(&last,&valid,850.0,true); /* fresh data resumes */
    assert(valid && fabsf(last-850.0f)<0.02f);
    gauge_hold_reset(&valid); /* new INI or channel selection */
    assert(!valid);
    gauge_hold_accept(&last,&valid,10.0,true);
    assert(valid && fabsf(last-10.0f)<0.02f);
    puts("PASS: cache survives dropouts, nonfinite fields, long outages; resets on reassignment; recovers");
}
