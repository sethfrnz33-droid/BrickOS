#include "dashboard.h"
#include <string.h>
#include <strings.h>
#include <ctype.h>

static bool same_channel(const BtField *a,const BtField *b) {
    if(!a||!b)return false;
    return !strcasecmp(a->name,b->name) ||
        (a->offset==b->offset && a->type==b->type &&
         a->kind==b->kind && a->bit_lo==b->bit_lo && a->bit_hi==b->bit_hi);
}
static bool scalar_channel(const BtField *f) {
    if(!f||!f->scaled||f->kind!=0||f->width!=1||f->height!=1||f->offset<0)return false;
    int n=bt_type_size(f->type);
    return n>0 && f->offset+n<=BTS_LIVE_CAP;
}
static void append(const BtDefinition *d, const BtField *a, const BtField *b,
                   BtIdleGauges *out,int index) {
    if(!d||index<0||index>=d->channel_count||out->count>=BTS_IDLE_GAUGES)return;
    const BtField *f=&d->channels[index];
    if(!scalar_channel(f)||same_channel(f,a)||same_channel(f,b))return;
    for(int i=0;i<out->count;i++)
        if(same_channel(f,&d->channels[out->field_index[i]]))return;
    out->field_index[out->count++]=index;
}
static bool contains(const char *s,const char *fragment) {
    if(!s||!fragment)return false;
    for(;*s;s++) {
        const char *p=s,*q=fragment;
        while(*q&&*p&&tolower((unsigned char)*p)==tolower((unsigned char)*q)){p++;q++;}
        if(!*q)return true;
    }
    return false;
}
void bt_idle_select(const BtDefinition *d, const char *top_a, const char *top_b,
                    BtIdleGauges *out) {
    if(!out)return;
    memset(out,0,sizeof(*out));
    if(!d||!d->valid)return;
    const BtField *a=bt_field(d,top_a,true),*b=bt_field(d,top_b,true);
    /* Same recognizable defaults used by the 3DS idle gauge panel. */
    static const char *preferred[]={
        "RPMValue","MAPValue","TPSValue","coolant","intake",
        "ignitionAdvanceCyl1","oilPressure","fuelPressure","VBatt","AFRValue"
    };
    for(unsigned k=0;k<sizeof(preferred)/sizeof(preferred[0]);k++) {
        for(int i=0;i<d->channel_count;i++)if(!strcasecmp(d->channels[i].name,preferred[k])){
            append(d,a,b,out,i);break;
        }
    }
    static const char *hints[]={"pressure","temp","advance","voltage", "throttle",
        "lambda","boost","duty","battery","fuel","rpm","afr","clt","tps"};
    for(unsigned k=0;k<sizeof(hints)/sizeof(hints[0])&&out->count<BTS_IDLE_GAUGES;k++)
        for(int i=0;i<d->channel_count&&out->count<BTS_IDLE_GAUGES;i++)
            if(contains(d->channels[i].name,hints[k]))append(d,a,b,out,i);
}
int bt_idle_required_length(const BtDefinition *d,const BtIdleGauges *g,int minimum){
    if(!d||!g)return minimum;
    for(int i=0;i<g->count;i++) {
        int idx=g->field_index[i];
        if(idx<0||idx>=d->channel_count)continue;
        const BtField *f=&d->channels[idx];
        int end=f->offset+bt_type_size(f->type);
        if(scalar_channel(f)&&end>minimum)minimum=end;
    }
    return minimum;
}
