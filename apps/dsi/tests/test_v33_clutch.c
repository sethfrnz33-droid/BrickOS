#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tuner.h"
#include "clutch_lego_rules.h"
static void parse_file(BtDefinition *d,const char *path){
    FILE *f=fopen(path,"rb");assert(f);
    uint8_t b[4096];size_t n;
    bt_ini_reset(d);
    while((n=fread(b,1,sizeof b,f))>0)bt_ini_feed(d,b,n);
    fclose(f);
    assert(bt_ini_finish(d));
    assert(!d->truncated&&!d->out_of_memory);
}
static uint32_t seq=0;
static int next(BtClutchLego *a,int v){return bt_clutch_lego_step(a,true,v,++seq);}
static int transition(BtClutchLego *a,int new_state){
    assert(next(a,new_state)==-1);
    assert(bt_clutch_lego_step(a,true,new_state,seq)==-1); /* duplicate sample */
    return next(a,new_state);
}
int main(int argc,char **argv){
    assert(argc==2);
    BtDefinition d={0};parse_file(&d,argv[1]);
    const BtField *f=bt_field(&d,"clutchDownState",true);
    const BtField *up=bt_field(&d,"clutchUpState",true);
    assert(f&&up);
    assert(f->kind==2&&f->bit_lo==10&&f->bit_hi==10&&f->offset==1444);
    assert(f->scaled&&f->type==BT_U32);
    assert(up->type==BT_S08&&up->offset==1438);
    assert(f->offset+bt_type_size(f->type)<=BTS_LIVE_CAP);
    uint8_t live[BTS_LIVE_CAP]={0};double v=-1;
    assert(bt_field_value(&d,f,live,sizeof live,0,&v)&&v==0);
    live[f->offset+1]|=(uint8_t)(1u<<2);
    assert(bt_field_value(&d,f,live,sizeof live,0,&v)&&v==1);
    BtClutchLego alert={0};
    assert(next(&alert,0)==-1); /* first good state is baseline */
    for(int i=0;i<BTS_CLUTCH_LEGO_SOUNDS*2;i++){
        int cue=transition(&alert,(i%2)==0?1:0);
        assert(cue==i%BTS_CLUTCH_LEGO_SOUNDS);
    }
    /* One-sample glitch does not count as clutch change. */
    assert(next(&alert,1)==-1);
    assert(next(&alert,0)==-1);
    assert(alert.next_sound==0);
    /* A stale link resets baseline, not the sound rotation. */
    assert(bt_clutch_lego_step(&alert,false,0,++seq)==-1);
    assert(next(&alert,1)==-1);
    assert(transition(&alert,0)==0);
    bt_ini_reset(&d);
    puts("PASS v33: real clutch bits, press/release edge debounce, sequence dedupe, 7-sound rotation, reconnect baseline");
    return 0;
}
