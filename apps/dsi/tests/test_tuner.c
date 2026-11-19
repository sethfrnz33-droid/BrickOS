#include "tuner.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BtDefinition def;
int main(void){
    const char *ini="[TunerStudio]\n"
      "signature = \"test-ecu-v1\"\r\n"
      "[Constants]\r\n"
      "endianness = little\r\n"
      "pageIdentifier = \"\\x00\\x01\", \"\\x00\\x02\"\n"
      "page = 1\n"
      "veTable = array, U16, 32, [4x3], %, 0.1, 0\n"
      "rpmAxis = array, U16, 12, [4], RPM, 1, 0\n"
      "loadAxis = array, U08, 20, [3], kPa, 1, 0\n"
      "bitsSample = bits, U08, 2, [2:3], \"A\", \"B\"\n"
      "[OutputChannels]\n"
      "RPMValue = scalar, U16, 0, RPM, 1, 0\n"
      "TPSValue = scalar, U08, 4, %, 0.5, 0\n"
      "AFRValue = scalar, F32, 8, AFR, 1, 0\n"
      "BadValue = scalar, U16, 8, %, {expr}, 0\n"
      "[TableEditor]\n"
      "table = veTable, fuelTable, \"Fuel VE\"\n"
      "xBins = rpmAxis, RPMValue\n"
      "yBins = loadAxis, MAPValue\n"
      "zBins = veTable\n";
    bt_ini_reset(&def);
    for(size_t i=0;i<strlen(ini);i+=5){size_t n=strlen(ini)-i;if(n>5)n=5;bt_ini_feed(&def,(const uint8_t*)ini+i,n);}
    assert(bt_ini_finish(&def));
    assert(!strcmp(def.signature,"test-ecu-v1"));
    assert(def.page_count==2 && def.page_ids[0]==256 && def.page_ids[1]==512);
    assert(def.channel_count==4 && def.constant_count==4 && def.map_count==1);
    assert(!strcmp(def.maps[0].z,"veTable"));
    const BtField *f=bt_field(&def,"RPMValue",true);
    assert(f&&f->scaled&&f->type==BT_U16);
    uint8_t sample[16]={0xDC,0x05,0,0,200,0,0,0,0,0,0,0};
    double v=-999;assert(bt_field_value(&def,f,sample,16,0,&v)&&v==1500);
    f=bt_field(&def,"TPSValue",true);assert(bt_field_value(&def,f,sample,16,0,&v)&&v==100.0);
    f=bt_field(&def,"BadValue",true);assert(!bt_field_value(&def,f,sample,16,0,&v));
    uint8_t live_bytes[32]={ 'B','L','V','1',1,0,0,0,0,0,0,0,16,0,0,0 };
    memcpy(live_bytes+16,sample,16);
    BtLive live={0};assert(bt_live_decode(&live,live_bytes,sizeof(live_bytes)));
    assert(!bt_live_decode(&live,live_bytes,sizeof(live_bytes)));
    live_bytes[4]++;
    assert(bt_live_decode(&live,live_bytes,sizeof(live_bytes)));
    live_bytes[12]=255;assert(!bt_live_decode(&live,live_bytes,sizeof(live_bytes)));
    const char *names[]={"RPMValue","TPSValue","AFRValue"};
    assert(bt_required_live_length(&def,names,3)==256);
    /* Signature hashes beyond the old CSV 127-byte limit are preserved. */
    char long_signature[155];
    memset(long_signature,'A',140);
    memcpy(long_signature+140,".1293678056",12);
    char long_ini[450];
    snprintf(long_ini,sizeof(long_ini),
        "[TunerStudio]\nsignature = \"%s\"\n"
        "[Constants]\npage = 1\n"
        "limiter = scalar, U16, 0, RPM, 1, 0\n"
        "[OutputChannels]\nRPMValue = scalar, U16, 0, RPM, 1, 0\n",long_signature);
    bt_ini_reset(&def);
    bt_ini_feed(&def,(const uint8_t*)long_ini,strlen(long_ini));
    assert(bt_ini_finish(&def));
    assert(!strcmp(def.signature,long_signature));
    printf("PASS: streaming INI, full-length signatures, identifier byte order, tables, scaling, BLV1 frames and bounds\n");
    return 0;
}
