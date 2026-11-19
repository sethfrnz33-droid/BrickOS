#include "tuner.h"
#include "wire.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static BtDefinition d;
static int set_ini(void){
 const char*ini="[TunerStudio]\n"
    "signature = \"test-ecu-v1\"\n"
    "[Constants]\npageIdentifier = \"\\x00\\x01\"\npageSize = 256\n"
    "pageChunkWrite = C%2i%2o%2c%v\nburnCommand = B%2i\n"
    "page = 1\nve = array, U16, 32, [2], %, 0.1, 0, 0, 200\n"
    "[OutputChannels]\nRPMValue = scalar, U16, 0, RPM, 1, 0\n"
    "[Menu]\nmenuDialog = mainMenu\nmenu = \"Fuel\"\nsubMenu = veTable, \"Fuel VE\"\n"
    "[TableEditor]\ntable = veTable, alternate, \"Fuel VE\"\n"
    "xBins = rpmAxis, RPMValue\nyBins = mapAxis, MAPValue\nzBins = ve\n";
 bt_ini_reset(&d);bt_ini_feed(&d,(const uint8_t*)ini,strlen(ini));
 assert(bt_ini_finish(&d));
 assert(d.page_sizes[0]==256&&d.page_writeable[0]&&d.page_burnable[0]);
 assert(d.menu_count==2&&d.map_count==1&&!strcmp(d.maps[0].alt,"alternate"));
 return 1;
}
int main(int argc,char**argv){
 assert(set_ini());
 const BtField*f=bt_field(&d,"ve",false);assert(f&&f->limits_known);
 uint8_t baseline[4]={100,0,200,0},new_data[4];
 memcpy(new_data,baseline,4);
 BtField rel=*f;rel.offset=0;
 assert(bt_encode_value(&d,&rel,new_data,4,0,11.0));
 assert(new_data[0]==110&&new_data[1]==0);
 assert(!bt_encode_value(&d,&rel,new_data,4,0,300.0));
 char msg[128];
 if(argc>1&&!strcmp(argv[1],"mismatch")){
     strcpy(d.signature,"different-layout.1293678057");
 }
 int ok=bt_commit_field(&d,f,baseline,new_data,4,msg,sizeof(msg));
 if(argc>1&&!strcmp(argv[1],"success")){assert(ok);assert(!strcmp(msg,"BURN VERIFIED / READBACK MATCHED"));}
 else if(argc>1){assert(!ok);puts(msg);}
 else assert(0);
 return 0;
}
