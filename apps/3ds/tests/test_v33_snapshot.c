#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../source/bts_cal_snapshot.h"
int main(void){
 FILE *f=tmpfile();assert(f);
 uint8_t input[]={2,3,4,5,6,7},output[sizeof input]={0};
 assert(bts_snapshot_write(f,"rusefi.test.signature","veTable",1,1510,input,sizeof input));
 rewind(f);assert(bts_snapshot_read(f,"rusefi.test.signature","veTable",1,1510,output,sizeof output));
 assert(!memcmp(input,output,sizeof input));
 rewind(f);assert(!bts_snapshot_read(f,"other ECU","veTable",1,1510,output,sizeof output));
 rewind(f);assert(!bts_snapshot_read(f,"rusefi.test.signature","veTable",1,1511,output,sizeof output));
 fseek(f,-1,SEEK_END);fputc('X',f);fflush(f);rewind(f);
 assert(!bts_snapshot_read(f,"rusefi.test.signature","veTable",1,1510,output,sizeof output));
 fclose(f);puts("PASS exact signed calibration field snapshot, mismatch and tamper rejected");
}
