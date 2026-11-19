#include "tuner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(void){
    const char *p="/tmp/bts-v24-iniparser.ini";
    FILE *f=fopen(p,"wb");assert(f);
    fprintf(f,"[TunerStudio]\nsignature=\"rusEFI.test.123\"\n");
    fprintf(f,"#define ALL_PINS = ");
    for(int i=0;i<200;i++){
        if(i)fprintf(f,",");
        fprintf(f,"\"%s%d\"",i==79?"INVALID":"PIN-",i);
    }
    fprintf(f,"\n#define MODE_CHOICES = $ALL_PINS\n");
    fprintf(f,"[Constants]\npage=1\npinMode=bits, U08, 0, [0:7], $MODE_CHOICES\n");
    fprintf(f,"[ConstantsExtensions]\nreadOnly = pinMode\nrequiresPowerCycle = pinMode\n");
    fprintf(f,"[SettingContextHelp]\npinMode=\"Use the selected ECU hardware pin and check the actual wiring before writing.\"\n");
    fprintf(f,"[TableEditor]\ntable=veMap,veMap,\"Fuel Table\"\nxBins=rpmBins,rpm,readOnly\nyBins=mapBins,MAP,readOnly\nzBins=veValues\n");
    fprintf(f,"[CurveEditor]\ncurve=warmCurve,\"Warmup Curve\"\nxBins=warmTemp,coolant,readOnly\nyBins=warmFuel,afr,readOnly\n");
    fprintf(f,"[UserDefined]\ndialog=diagnostics,\"Diagnostic Panel\"\nliveGraph=afrGraph,\"AFR Analysis\",South\ngraphLine=rpm\ngraphLine=afr\ngraphLine=coolant\n");
    fprintf(f,"[OutputChannels]\nrpm=scalar, U16, 0, \"rpm\", 1, 0\n");
    assert(fclose(f)==0);
    BtDefinition def={0};
    bt_ini_feed(&def,(const uint8_t*)"",0);
    FILE *source=fopen(p,"rb");assert(source);
    char buf[1024];size_t got;
    while((got=fread(buf,1,sizeof(buf),source))>0)bt_ini_feed(&def,(const uint8_t*)buf,got);
    fclose(source);
    assert(bt_ini_finish(&def));
    assert(def.map_count==2&&def.maps[0].x_readonly&&def.maps[0].y_readonly);
    assert(def.maps[1].curve&&def.maps[1].x_readonly&&def.maps[1].y_readonly);
    assert(def.graph_count==1&&def.graph_line_count==3);
    const BtField *pin=bt_field(&def,"pinMode",false);
    assert(pin&&pin->read_only&&pin->power_cycle);
    assert(!strcmp(def.graphs[0].name,"afrGraph"));
    assert(!strcmp(def.graph_lines[2].channel,"coolant"));
    assert(def.refs[def.ref_count-1].kind==6);
    bt_ini_reset(&def);
    char options[256][36];
    int n=bt_bit_choices(p,"pinMode",options,256);
    assert(n==200);
    assert(!strcmp(options[0],"PIN-0"));
    assert(!strcmp(options[199],"PIN-199"));
    assert(!options[79][0]);
    char help[256];
    assert(bt_ini_context_help(p,"pinMode",help,sizeof(help)));
    assert(strstr(help,"actual wiring"));
    assert(!bt_ini_context_help(p,"missing",help,sizeof(help)));
    assert(!help[0]);
    char small[64][36];
    assert(!bt_bit_choices(p,"pinMode",small,64)); /* don't pretend full list */
    remove(p);
    puts("PASS v2.4: 200 INI macro choices, nested #defines, reserved enum holes, full contextual help");
    return 0;
}
