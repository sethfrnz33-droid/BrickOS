#include "tuner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

int main(int argc,char**argv){
    assert(argc>=2);
    FILE*f=fopen(argv[1],"rb");assert(f);
    static BtDefinition d;
    unsigned char buf[1409];size_t n;
    bt_ini_reset(&d);
    while((n=fread(buf,1,sizeof buf,f)))bt_ini_feed(&d,buf,n);
    fclose(f);assert(bt_ini_finish(&d));
    int refs=0,unresolved=0,missing_ch=0,placed=0;
    for(int i=0;i<d.ref_count;i++){
        const BtDialogRef*r=&d.refs[i];
        if(r->placement[0])placed++;
        if(r->kind!=7)continue;
        refs++;
        const BtGauge*g=bt_gauge(&d,r->name);
        if(!g){unresolved++;fprintf(stderr,"missing gauge %s\n",r->name);continue;}
        if(!bt_field(&d,g->channel,true)){
            missing_ch++;
            char expr[256];
            if(bt_ini_rhs(argv[1],"OutputChannels",g->channel,expr,sizeof expr))
                fprintf(stderr,"computed %s: %s => %.100s\n",g->name,g->channel,expr);
            else fprintf(stderr,"unresolved channel %s => %s\n",g->name,g->channel);
        }
    }
    const BtGauge*ref=bt_gauge(&d,"pedalPositionGauge");
    assert(ref&&ref->channel[0]);
    /* readoutPanel now resolves gauge-template readouts as additional read-only
     * references: original 67 gauge directives remain, never removed. */
    fprintf(stderr,"v34 gauge refs=%d placements=%d unresolved=%d templates=%d\n",refs,placed,unresolved,d.gauge_count);
    assert(d.gauge_count>=380&&refs>=67&&unresolved==0&&placed>90);
    for(int i=0;i<d.ref_count;i++)if(!strcasecmp(d.refs[i].owner,"pedalSensor")&&d.refs[i].kind==1){
        if(!strcasecmp(d.refs[i].name,"pedalSensorLeft"))assert(!strcasecmp(d.refs[i].placement,"West"));
        if(!strcasecmp(d.refs[i].name,"pedalGauges"))assert(!strcasecmp(d.refs[i].placement,"East"));
    }
    printf("PASS v26 gauge references %d, templates %d, placements %d, unmapped %d, computed %d\n",refs,d.gauge_count,placed,unresolved,missing_ch);
    bt_ini_reset(&d);
    return 0;
}
