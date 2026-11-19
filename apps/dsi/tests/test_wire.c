#include "wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static BtDefinition d;
int main(void){
  char json[4096];bool online=false;
  assert(bt_get_status(json,sizeof(json)));
  assert(bt_json_bool(json,"ecu_session_connected",&online)&&online);
  char sig[80];assert(bt_json_string(json,"ecu_signature",sig,sizeof(sig)));
  assert(!strcmp(sig,"test-ecu-v1"));
  assert(bt_get_ini(&d));assert(d.valid&&d.map_count==1);
  BtLive live={0};assert(bt_get_live(&live,256));
  double v=0;assert(bt_field_value(&d,bt_field(&d,"RPMValue",true),live.bytes,live.length,0,&v)&&v==1500);
  const BtField*f=bt_field(&d,"veTable",false);assert(f);
  double cells[12];assert(bt_read_field(&d,f,cells,12));
  assert(cells[0]==10.0&&cells[11]==21.0);
  assert(!bt_read_field(&d,f,cells,11));
  puts("PASS: HTTP Content-Length, chunked INI, binary BLV1 and R read via JSON");
  return 0;
}
