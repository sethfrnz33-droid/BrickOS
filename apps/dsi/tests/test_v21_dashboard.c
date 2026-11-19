#include "dashboard.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    BtDefinition d={0};
    BtField channels[12]={0};d.valid=true;d.channels=channels;d.channel_count=11;
    const char *names[]={"AFRValue","VBatt","RPMValue","RPMduplicated","MAPValue",
        "TPSValue","coolant","intake","ignitionAdvanceCyl1","oilPressure","stateCode"};
    for(int i=0;i<11;i++){
        snprintf(channels[i].name,sizeof(channels[i].name),"%s",names[i]);
        channels[i].kind=0;channels[i].type=BT_U16;channels[i].offset=i*2;
        channels[i].width=channels[i].height=1;channels[i].scaled=true;
    }
    channels[3].offset=channels[2].offset; /* same signal, different INI name */
    channels[9].offset=2100;
    BtIdleGauges g;
    bt_idle_select(&d,"AFRValue","VBatt",&g);
    assert(g.count==6);
    int expected[]={2,4,5,6,7,8};
    for(int i=0;i<6;i++)assert(g.field_index[i]==expected[i]);
    assert(bt_idle_required_length(&d,&g,256)==256);
    bt_idle_select(&d,"RPMduplicated","coolant",&g);
    for(int i=0;i<g.count;i++)assert(g.field_index[i]!=2&&g.field_index[i]!=3&&g.field_index[i]!=6);
    uint8_t packet[20+16]={ 'B','L','V','1',5,0,0,0,0,0,0,0,20,0,0,0 };
    BtLive l={0};bool changed=false;
    assert(bt_live_decode_http(&l,packet,sizeof(packet),&changed)&&changed);
    changed=true;
    assert(bt_live_decode_http(&l,packet,sizeof(packet),&changed)&&!changed);
    assert(l.valid&&l.sequence==5);
    packet[16]=0x32; /* same sequence with inconsistent data is invalid */
    assert(!bt_live_decode_http(&l,packet,sizeof(packet),&changed));
    packet[4]=6;
    assert(bt_live_decode_http(&l,packet,sizeof(packet),&changed)&&changed);
    packet[12]=255;
    assert(!bt_live_decode_http(&l,packet,sizeof(packet),&changed));
    puts("PASS: idle physical channel dedup, capture sizing, repeated live frames aren't transport faults");
    return 0;
}
