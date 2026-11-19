#include "tuner.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void parse_ini(BtDefinition *d,const char *file){
    FILE *f=fopen(file,"rb");assert(f);
    bt_ini_reset(d);
    unsigned char buffer[1007];size_t n;
    while((n=fread(buffer,1,sizeof(buffer),f))>0)bt_ini_feed(d,buffer,n);
    fclose(f);assert(bt_ini_finish(d));
    assert(!d->truncated&&!d->out_of_memory);
}
int main(int argc,char **argv){
    assert(argc>=2);
    static BtDefinition d;
    parse_ini(&d,argv[1]);
    int position_panels=0,named_panels=0,retained_layout_wrappers=0;
    for(int i=0;i<d.ref_count;i++){
        const BtDialogRef*r=&d.refs[i];
        if(r->kind!=1)continue;
        assert(strcasecmp(r->label,"West") && strcasecmp(r->label,"East") &&
               strcasecmp(r->label,"Center") && strcasecmp(r->label,"North") &&
               strcasecmp(r->label,"South"));
        assert(r->label[0]);
        named_panels++;
        if(!strcasecmp(r->owner,"injectionSettings")&&
           (!strcasecmp(r->name,"injectionSettingsWest")||
            !strcasecmp(r->name,"injectionSettingsEast"))){
            retained_layout_wrappers++;
            assert(strcasecmp(r->label,"West")&&strcasecmp(r->label,"East"));
        }
        if(!strcasecmp(r->name,"ignAdder1Table")){
            assert(strcasecmp(r->label,"Center"));position_panels++;
        }
    }
    assert(named_panels>80&&retained_layout_wrappers==2&&position_panels>0);
    static char labels[256][36];
    int n=bt_bit_choices(argv[1],"mainRelayPin",labels,256);
    assert(n>70);
    int hidden=0,available=0,gapped=0;
    for(int i=0;i<n;i++){
        if(labels[i][0])available++;
        else {hidden++;if(i>1&&labels[i-1][0])gapped++;}
    }
    assert(hidden>5&&available>10&&gapped>0);
    assert(!strcmp(labels[10],"Highside 2"));
    assert(!strcmp(labels[22],"Lowside 8"));
    assert(labels[21][0]==0);
    printf("PASS v26 parser: %d nested panels without layout captions; %d enum choices (%d reserved slots hidden in picker); %d layout wrappers available for flattening\n",
           named_panels,available,hidden,retained_layout_wrappers);
    bt_ini_reset(&d);return 0;
}
