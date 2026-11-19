#include "tuner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
int main(void){
    BtDefinition d={0};bt_ini_reset(&d);
    const char ini[]="[TunerStudio]\nsignature=\"board.1234567890\"\n"
        "[Constants]\npage = 1\nmode = bits, U16, 2, [3:5], \"OFF\", \"LOW\", \"MEDIUM\", \"HIGH\"\n"
        "setpoint = scalar, U16, 10, \"kPa\", 1, 0, 0, 500\n"
        "[OutputChannels]\nrpm = scalar, U16, 2, \"rpm\", 1, 0\n";
    bt_ini_feed(&d,(const uint8_t*)ini,strlen(ini));assert(bt_ini_finish(&d));
    const BtField *f=bt_field(&d,"mode",false);assert(f&&f->kind==2&&f->scaled);
    uint8_t b[16]={0}; b[2]=0xAF;b[3]=0x55;
    double actual=0;assert(bt_field_value(&d,f,b,sizeof(b),0,&actual));assert(actual==5);
    assert(bt_encode_value(&d,f,b,sizeof(b),0,2));
    assert(b[2]==0x97 && b[3]==0x55); // clear bits 3..5, set 2, preserve other bits
    assert(bt_field_value(&d,f,b,sizeof(b),0,&actual) && actual==2);
    assert(!bt_encode_value(&d,f,b,sizeof(b),0,8));
    assert(!bt_encode_value(&d,f,b,sizeof(b),0,1.3));
    assert(b[2]==0x97);
    char labels[32][36]={{0}};
    const char *path="/tmp/bts-v22-bits.ini";
    FILE *out=fopen(path,"wb");assert(out);
    assert(fwrite(ini,1,strlen(ini),out)==strlen(ini));assert(fclose(out)==0);
    int options=bt_bit_choices(path,"mode",labels,32);
    assert(options==4);assert(!strcmp(labels[0],"OFF"));assert(!strcmp(labels[3],"HIGH"));
    remove(path);
    const char macro_ini[]="[TunerStudio]\nsignature=\"abc.1234567\"\n"
      "#define MY_CHOICES = { \"OFF\", \"INVALID\", \"ON\" }\n"
      "[Constants]\nmode = bits, U16, 2, [3:5], $MY_CHOICES\n"
      "[OutputChannels]\nrpm = scalar, U16, 2, \"rpm\", 1, 0\n";
    out=fopen(path,"wb");assert(out);
    assert(fwrite(macro_ini,1,strlen(macro_ini),out)==strlen(macro_ini));assert(fclose(out)==0);
    options=bt_bit_choices(path,"mode",labels,32);
    assert(options==3);assert(!strcmp(labels[0],"OFF"));assert(!labels[1][0]);
    assert(!strcmp(labels[2],"ON"));
    remove(path);
    d.big_endian=true;
    uint8_t be[4]={0,0,0x55,0xAF};
    assert(bt_field_value(&d,f,be,sizeof(be),0,&actual)&&actual==5);
    assert(bt_encode_value(&d,f,be,sizeof(be),0,2));
    assert(be[2]==0x55&&be[3]==0x97);
    bt_ini_reset(&d);
    puts("PASS v2.2: INI bit enums, unchanged neighbor bits, range validation, SD choices, big endian");
    return 0;
}
