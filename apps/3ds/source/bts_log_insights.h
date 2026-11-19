#ifndef BTS_LOG_INSIGHTS_H
#define BTS_LOG_INSIGHTS_H
/* Read-only log statistics. No synthetic samples, no ECU commands. */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#define BTS_HIST_BINS 12
typedef struct {
 unsigned samples,valid,missing,gaps,max_gap_ms,hist[BTS_HIST_BINS];
 uint32_t duration_ms;
 double mean,minimum,maximum,p05,median,p95;
} BtsLogInsights;
static inline int bts_log_double_cmp(const void *a,const void *b){
 double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);
}
static inline double bts_log_percentile(const double *sorted,int n,double p){
 if(n<=0)return NAN;
 double pos=p*(double)(n-1);int lo=(int)pos,hi=lo+1;
 if(hi>=n)return sorted[n-1];return sorted[lo]+(sorted[hi]-sorted[lo])*(pos-lo);
}
static inline bool bts_log_inspect(const float *values,const uint32_t *times,
 const uint8_t *has,int count,BtsLogInsights *out,double *scratch){
 if(!values||!times||!has||!out||!scratch||count<1||count>8192)return false;
 *out=(BtsLogInsights){0};out->samples=(unsigned)count;
 uint32_t first=times[0],last=times[count-1];if(last<first)return false;
 out->duration_ms=last-first;
 uint64_t dt_total=0;int dt_count=0;
 for(int i=1;i<count;i++){
    if(times[i]<times[i-1])return false;
    uint32_t dt=times[i]-times[i-1];dt_total+=dt;dt_count++;
    if(dt>out->max_gap_ms)out->max_gap_ms=dt;
 }
 double typical_dt=dt_count?(double)dt_total/dt_count:0;
 /* Flag observed long sample gaps, not an invented count of missing packets. */
 for(int i=1;i<count;i++)if(typical_dt>0&&times[i]-times[i-1]>3*typical_dt)out->gaps++;
 double sum=0;
 for(int i=0;i<count;i++){
    if(!has[i]||!isfinite(values[i])){out->missing++;continue;}
    scratch[out->valid++]=values[i];sum+=values[i];
 }
 if(!out->valid){out->minimum=out->maximum=out->mean=NAN;
 out->p05=out->median=out->p95=NAN;return true;}
 qsort(scratch,out->valid,sizeof(double),bts_log_double_cmp);
 out->minimum=scratch[0];out->maximum=scratch[out->valid-1];out->mean=sum/out->valid;
 out->p05=bts_log_percentile(scratch,(int)out->valid,.05);
 out->median=bts_log_percentile(scratch,(int)out->valid,.50);
 out->p95=bts_log_percentile(scratch,(int)out->valid,.95);
 double span=out->maximum-out->minimum;
 for(unsigned i=0;i<out->valid;i++){
    int bin=span>1e-12?(int)((scratch[i]-out->minimum)/span*BTS_HIST_BINS):BTS_HIST_BINS/2;
    if(bin>=BTS_HIST_BINS)bin=BTS_HIST_BINS-1;if(bin<0)bin=0;
    out->hist[bin]++;
 }
 return true;
}
#endif
