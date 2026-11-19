#include <stdio.h>
#include <assert.h>
#include "../source/bts_log_quality.h"
int main(void){
 float x[]={10,10,NAN,11,12,12};
 uint32_t t[]={0,10,20,30,40,150};
 uint8_t v[]={1,1,0,1,1,1};
 BtsSignalQuality q;
 assert(bts_signal_quality(x,t,v,6,&q));
 assert(q.valid==5&&q.missing==1&&q.longest_constant_run==2);
 assert(q.long_gaps==1&&q.maximum_observed_gap_ms==110&&q.time_of_largest_gap_ms==150);
 uint32_t bad[]={0,10,8,30,40,150};
 assert(!bts_signal_quality(x,bad,v,6,&q));
 puts("PASS observed log quality and corruption checks, no sensor fault inference");
}
