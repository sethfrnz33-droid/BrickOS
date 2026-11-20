#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "protocol.h"
#include "vcp_tcp_gateway.h"
#include "freertos/semphr.h"

static uint8_t tx[2000],rx[2000];
static size_t tx_len,rx_len,read_at;
static int ready=1,claim_count,release_count,fail_crc;
static int64_t now_us=100000;
const char *esp_err_to_name(int e){(void)e;return "TEST_ERROR";}
int64_t esp_timer_get_time(void){return now_us+=1000;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void*)1;}
int xSemaphoreTake(SemaphoreHandle_t h,uint32_t timeout){(void)h;(void)timeout;return 1;}
int xSemaphoreGive(SemaphoreHandle_t h){(void)h;return 1;}
void vTaskDelay(uint32_t ticks){now_us+=(int64_t)ticks*1000;}
bool vcp_gateway_usb_ready(void){return ready!=0;}
esp_err_t brick_usb_transaction_begin(void){assert(ready);claim_count++;read_at=0;return ESP_OK;}
void brick_usb_transaction_end(void){release_count++;}
static void be32(uint8_t *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
esp_err_t brick_usb_transaction_write(const uint8_t *p,size_t n){
 assert(n<sizeof(tx));memcpy(tx,p,n);tx_len=n;
 const char response[]="rusEFI 2026.11.test";
 size_t count=strlen(response)+2;
 rx[0]=0;rx[1]=(uint8_t)count;rx[2]=0;memcpy(rx+3,response,sizeof(response));
 rx_len=2+count+4;
 be32(rx+2+count,ts_crc32(rx+2,count));
 if(fail_crc)rx[rx_len-1]^=1;
 return ESP_OK;
}
size_t brick_usb_transaction_read(uint8_t *p,size_t n,uint32_t timeout){
 (void)timeout;size_t remaining=rx_len-read_at;
 if (n > 3) n = 3;
 if (n > remaining) n = remaining;
 if(n){memcpy(p,rx+read_at,n);read_at+=n;}return n;
}
int main(void){
 assert(ts_init()==ESP_OK);
 ts_reply_t r={0};
 assert(ts_request('S',NULL,0,&r,4000)==ESP_OK);
 assert(tx_len==7 && tx[0]==0 && tx[1]==1 && tx[2]=='S');
 assert((uint32_t)((tx[3]<<24)|(tx[4]<<16)|(tx[5]<<8)|tx[6])==ts_crc32(tx+2,1));
 assert(r.status==0 && r.length==strlen("rusEFI 2026.11.test")+1);
 assert(!memcmp(r.bytes,"rusEFI 2026.11.test",r.length));
 fail_crc=1;assert(ts_request('S',NULL,0,&r,4000)==ESP_ERR_INVALID_CRC);
 assert(claim_count==2 && release_count==2);
 ready=0;assert(ts_request('S',NULL,0,&r,4000)==ESP_ERR_NOT_FOUND);
 assert(claim_count==2 && release_count==2);
 puts("PASS native USB client: CRC-framed S, fragmented USB, CRC rejection, USB disconnect");
 return 0;
}
