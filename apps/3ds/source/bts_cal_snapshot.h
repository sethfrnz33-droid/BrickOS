#ifndef BTS_CAL_SNAPSHOT_H
#define BTS_CAL_SNAPSHOT_H
/* Portable binary-format field snapshots, strictly read-only on load.
 * This format stores a single INI calibration field, not a complete ECU tune.
 * It does not touch firmware, trigger outputs, or ECU flash. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define BTS_SNAPSHOT_MAX 4096
#define BTS_SNAPSHOT_HEADER 252U
static inline uint32_t bts_snapshot_crc32(const uint8_t *src,size_t len){
    uint32_t crc=~(uint32_t)0;
    for(size_t i=0;i<len;i++){
        crc^=src[i];
        for(int j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u&-(crc&1u));
    }
    return ~crc;
}
static inline void bts_snapshot_le32(uint8_t *out,uint32_t v){
    for(int i=0;i<4;i++)out[i]=(uint8_t)(v>>(8*i));
}
static inline uint32_t bts_snapshot_get32(const uint8_t *in){
    return (uint32_t)in[0]|((uint32_t)in[1]<<8)|((uint32_t)in[2]<<16)|((uint32_t)in[3]<<24);
}
/* bytes: magic[12], exact signature[160], INI field[64], page/offset/len/crc[16]. */
static inline bool bts_snapshot_write(FILE *fp,const char *signature,const char *field,
                   uint32_t page,uint32_t offset,const uint8_t *data,uint32_t length){
    if(!fp||!signature||!*signature||!field||!*field||!data||length<1||
       length>BTS_SNAPSHOT_MAX||strlen(signature)>=160||strlen(field)>=64||page<1||page>64)return false;
    uint8_t head[BTS_SNAPSHOT_HEADER]={0};
    memcpy(head,"BTS-CAL-V1",10);
    memcpy(head+12,signature,strlen(signature));
    memcpy(head+172,field,strlen(field));
    bts_snapshot_le32(head+236,page);
    bts_snapshot_le32(head+240,offset);
    bts_snapshot_le32(head+244,length);
    bts_snapshot_le32(head+248,bts_snapshot_crc32(data,length));
    return fwrite(head,1,sizeof(head),fp)==sizeof(head)&&fwrite(data,1,length,fp)==length;
}
static inline bool bts_snapshot_read(FILE *fp,const char *signature,const char *field,
                   uint32_t page,uint32_t offset,uint8_t *out,uint32_t length){
    if(!fp||!signature||!*signature||!field||!*field||!out||length<1||
       length>BTS_SNAPSHOT_MAX||strlen(signature)>=160||strlen(field)>=64)return false;
    uint8_t head[BTS_SNAPSHOT_HEADER];
    if(fread(head,1,sizeof(head),fp)!=sizeof(head))return false;
    if(memcmp(head,"BTS-CAL-V1",10)||head[10]||head[11])return false;
    if(!memchr(head+12,0,160)||!memchr(head+172,0,64))return false;
    if(strcmp((const char*)head+12,signature)||strcmp((const char*)head+172,field))return false;
    if(bts_snapshot_get32(head+236)!=page||bts_snapshot_get32(head+240)!=offset||
       bts_snapshot_get32(head+244)!=length)return false;
    if(fread(out,1,length,fp)!=length||fgetc(fp)!=EOF)return false;
    return bts_snapshot_get32(head+248)==bts_snapshot_crc32(out,length);
}
#endif
