#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "../source/bts_log_compare.h"
int main(void){
    const char names[9][64]={"AFR","RPM","MAP","TPS","COOLANT","IAT","VBatt","IGN","Oil"};
    assert(bts_compare_channel_index("rpm",names)==1);
    assert(bts_compare_channel_index("MAF",names)==-1);
    uint32_t t[]={100,200,350,900};
    assert(bts_compare_nearest(t,4,300,60)==2);
    assert(bts_compare_nearest(t,4,700,90)==-1);
    assert(bts_compare_nearest(t,4,1000,100)==3);
    BtsCompareStats s;bts_compare_stats_init(&s);
    bts_compare_stats_add(&s,1,10,1,12);
    bts_compare_stats_add(&s,1,10,0,0);
    bts_compare_stats_add(&s,1,14,1,13);
    bts_compare_stats_add(&s,1,NAN,1,2);
    assert(s.paired==2&&s.skipped==2&&s.base_valid==3&&s.reference_valid==3);
    assert(fabs(s.delta_sum-1)<1e-8&&fabs(s.abs_delta_sum-3)<1e-8);
    assert(s.minimum_delta==-1&&s.maximum_delta==2);
    puts("PASS log channel mapping, timestamp matching, missing-data exclusion, and true delta statistics");
}
