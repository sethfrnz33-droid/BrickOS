#include "tuner.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
static bool values(const char *name,void*ctx,double *value){
    (void)ctx;
    if(!strcmp(name,"useMetricOnInterface")){*value=1;return true;}
    if(!strcmp(name,"fuelAlgorithm")){*value=0;return true;}
    if(!strcmp(name,"veOverrideMode")){*value=0;return true;}
    if(!strcmp(name,"ignOverrideMode")){*value=0;return true;}
    if(!strcmp(name,"useLambdaOnInterface")){*value=1;return true;}
    return false;
}
static void eval(const char*s,double expected){double value=0;assert(bt_numeric_eval(s,values,NULL,&value));assert(fabs(value-expected)<1e-6);}
int main(int argc,char**argv){
    eval("{useMetricOnInterface ? 1 : 1.8}",1);
    eval("{!(veOverrideMode == 1 || (veOverrideMode == 0 && fuelAlgorithm == 0)) || useMetricOnInterface ? 1 : 0.145038}",1);
    eval("{ useLambdaOnInterface ? 1/147 : 1/10 }",1.0/147.0);
    double v;assert(!bt_numeric_eval("{UNKNOWN_FIELD ? 1 : 0}",values,NULL,&v));
    assert(!bt_numeric_eval("{1/0}",values,NULL,&v));
    assert(!bt_numeric_eval("{useMetricOnInterface ? 1",values,NULL,&v));
    if(argc<2){puts("PASS v25: strict INI expression parser; supply full Proteus INI path for coverage");return 0;}
    const char *path=argv[1];FILE*f=fopen(path,"rb");assert(f);
    BtDefinition d={0};bt_ini_reset(&d);char b[777];size_t n;
    while((n=fread(b,1,sizeof(b),f))>0)bt_ini_feed(&d,(const uint8_t*)b,n);
    fclose(f);assert(bt_ini_finish(&d));assert(!d.truncated&&!d.out_of_memory);
    int multi=0,no_z=0,no_x=0,unknown_x=0,unknown_y=0,help_topics=0;
    for(int i=0;i<d.map_count;i++){
        const BtMap*m=&d.maps[i];
        if(m->series_index)multi++;
        if(m->help_topic[0])help_topics++;
        if(!bt_field(&d,m->z,false))no_z++;
        if(!m->x[0])no_x++;
        if(m->x[0]&&!bt_field(&d,m->x,false))unknown_x++;
        if(m->y[0]&&!bt_field(&d,m->y,false))unknown_y++;
    }
    assert(d.map_count>=200&&d.menu_count>300&&d.channel_count>1000&&multi>0);
    for(int i=0;i<d.ref_count;i++)assert(strcmp(d.refs[i].name,"@dialogHelp")&&strcmp(d.refs[i].name,"@help"));
    BtIniFormat fmt={0};assert(bt_ini_field_format(path,"ignitionLoadBins",&fmt));
    assert(strstr(fmt.scale,"ignOverrideMode"));
    assert(bt_numeric_eval(fmt.scale,values,NULL,&v));assert(fabs(v-1)<1e-8);
    double axis[2048]={0};int axis_count=0;
    assert(bt_ini_virtual_array(path,"gearCountArray",axis,2048,&axis_count));
    assert(axis_count==10&&axis[0]==-1&&axis[9]==8);
    assert(bt_ini_virtual_array(path,"solenoidCountArray",axis,2048,&axis_count));
    assert(axis_count==6&&axis[0]==1&&axis[5]==6);
    assert(bt_ini_virtual_array(path,"rangeInputArray",axis,2048,&axis_count));
    assert(axis_count==6&&axis[0]==0&&axis[5]==5);
    assert(bt_ini_virtual_array(path,"wueAfrTargetOffset",axis,2048,&axis_count));
    assert(axis_count==16&&fabs(axis[0]+1.5)<1e-6&&axis[15]==0);
    assert(!bt_ini_virtual_array(path,"wueAnalRecommend",axis,2048,&axis_count));
    char text[900];assert(bt_ini_context_help(path,"fuelAlgorithm",text,sizeof(text))&&strlen(text)>25);
    printf("PASS v25 full INI: %d map/curve series (%d additional), %d channels, %d menu nodes, %d refs, %d context help topics\n",d.map_count,multi,d.channel_count,d.menu_count,d.ref_count,help_topics);
    printf("Maps referencing undefined entries: z=%d, x=%d, y=%d; maps missing x=%d\n",no_z,unknown_x,unknown_y,no_x);
    bt_ini_reset(&d);return 0;
}
