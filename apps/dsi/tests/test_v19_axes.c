#include "tuner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    BtDefinition def={0};bt_ini_reset(&def);
    const char *ini="[TableEditor]\n"
        "table = veTable, veTblMap, VE Table\n"
        "xBins = rpmBins, RPM, readOnly\n"
        "yBins = fuelBins, load, readOnly\n"
        "zBins = veTable\n";
    bt_ini_feed(&def,(const uint8_t*)ini,strlen(ini));
    assert(def.map_count==1);
    assert(!strcmp(def.maps[0].x,"rpmBins"));
    assert(!strcmp(def.maps[0].xchan,"RPM"));
    assert(!strcmp(def.maps[0].y,"fuelBins"));
    assert(!strcmp(def.maps[0].ychan,"load"));
    bt_ini_reset(&def);
    puts("PASS: INI table axis names and live channel bindings parsed separately");
}
