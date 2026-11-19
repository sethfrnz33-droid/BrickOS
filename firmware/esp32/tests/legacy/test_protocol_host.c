// Runs BrickOS's actual production protocol.c against a simulated TWAI ECU.
// This is an ISO-TP/CRC regression test, NOT an on-hardware acceptance test.
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/twai.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "protocol.h"
static int64_t now_us;
int64_t esp_timer_get_time(void){return now_us;}
const char *esp_err_to_name(int e){return e==ESP_OK?"OK":"ERROR";}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void *)1;}
int xSemaphoreTake(SemaphoreHandle_t h,uint32_t t){(void)h;(void)t;return 1;}
int xSemaphoreGive(SemaphoreHandle_t h){(void)h;return 1;}
static twai_message_t response_fifo[128]; static int response_head, response_tail;
QueueHandle_t xQueueCreate(unsigned depth, unsigned size){assert(depth>=128);assert(size==sizeof(twai_message_t));return (void*)2;}
int xQueueSend(QueueHandle_t q,const void *v,uint32_t timeout){(void)q;(void)timeout;if(response_tail>=128)return 0;response_fifo[response_tail++]=*(const twai_message_t*)v;return 1;}
int xQueueReceive(QueueHandle_t q,void *v,uint32_t timeout){(void)q;if(response_head<response_tail){*(twai_message_t*)v=response_fifo[response_head++];return 1;}response_head=response_tail=0;now_us+=(int64_t)timeout*1000;return 0;}
int xQueueReset(QueueHandle_t q){(void)q;response_head=response_tail=0;return 1;}
static twai_message_t fifo[128];static int head,tail;
static uint8_t pending[128];static size_t pending_len,pending_pos;
static uint32_t ecu_request_id=0x100,ecu_response_id=0x102;
static int send_count,flow_controls,install_count;
static void enq(twai_message_t m){assert(tail<128);fifo[tail++]=m;}
static twai_message_t pack(uint8_t *data,size_t n){
    twai_message_t m={.identifier=ecu_response_id,.extd=0,.data_length_code=8};
    memcpy(m.data,data,n);return m;
}
static void queue_response(const uint8_t *body,size_t len){
    uint8_t stream[128];
    size_t blen=len+1;
    stream[0]=(uint8_t)(blen>>8);stream[1]=(uint8_t)blen;stream[2]=0;
    memcpy(stream+3,body,len);
    uint32_t crc=ts_crc32(stream+2,blen);
    stream[2+blen]=(uint8_t)(crc>>24);stream[3+blen]=(uint8_t)(crc>>16);
    stream[4+blen]=(uint8_t)(crc>>8);stream[5+blen]=(uint8_t)crc;
    pending_len=blen+6;pending_pos=0;
    memcpy(pending,stream,pending_len);
    if(pending_len<=7){
        uint8_t first[8]={0};first[0]=(uint8_t)pending_len;
        memcpy(first+1,pending,pending_len);enq(pack(first,8));
        pending_pos=pending_len;
    }else{
        uint8_t first[8]={0};first[0]=0x10|(pending_len>>8);first[1]=pending_len;
        memcpy(first+2,pending,6);enq(pack(first,8));pending_pos=6;
    }
}
esp_err_t twai_driver_install(const twai_general_config_t *g,const twai_timing_config_t *t,const twai_filter_config_t *f){
    assert(g->tx_io==43 && g->rx_io==44);assert(t->rate==500000);
    ++install_count;
    assert(f->acceptance_mask==0xffffffffU);
    assert(g->alerts_enabled!=0);
    uint8_t announcement[8]={0x02,0x01,0,0,0,0x01,0,0};
    twai_message_t m={.identifier=0x770017,.extd=1,.data_length_code=8};
    memcpy(m.data,announcement,8);if(install_count==1)enq(m);
    return ESP_OK;
}
esp_err_t twai_read_alerts(uint32_t *alerts,uint32_t ticks){(void)ticks;*alerts=0;return ESP_OK;}
esp_err_t twai_start(void){return ESP_OK;}
esp_err_t twai_stop(void){return ESP_OK;}
esp_err_t twai_driver_uninstall(void){return ESP_OK;}
esp_err_t twai_transmit(const twai_message_t *m,uint32_t ticks){
    (void)ticks;assert(m->identifier==ecu_request_id);send_count++;
    uint8_t type=m->data[0]>>4;
    if(type==0){
        size_t l=m->data[0]&15;
        if(l==1 && m->data[1]=='Q'){
            uint8_t response[8]={7,'r','u','s','E','F','I',0};
            enq(pack(response,8));
        }else{
            assert(l>=7); // framed request
            // Busy rusEFI / wideband bus: 48 unrelated broadcasts must not
            // crowd the dedicated ISO-TP response queue.
            for (int i=0; i<48; ++i) {
                twai_message_t broadcast={.identifier=0x200u+(unsigned)(i%4),.data_length_code=8};
                enq(broadcast);
            }
            uint8_t response[7]={'r','u','s','E','F','I',0};
            queue_response(response,7);
        }
    }else if(type==3){
        flow_controls++;
        uint8_t seq=1;
        while(pending_pos<pending_len){
            uint8_t frame[8]={0};frame[0]=0x20 | (seq++ & 0x0f);
            size_t take=pending_len-pending_pos;if(take>7)take=7;
            memcpy(frame+1,pending+pending_pos,take);
            pending_pos+=take;enq(pack(frame,8));
        }
    }
    return ESP_OK;
}
esp_err_t twai_receive(twai_message_t *m,uint32_t ticks){
    if(head<tail){*m=fifo[head++];return ESP_OK;}
    head=tail=0;now_us+=ticks*1000;return ESP_ERR_TIMEOUT;
}
esp_err_t twai_clear_receive_queue(void){head=tail=0;return ESP_OK;}
esp_err_t twai_get_status_info(twai_status_info_t *info){*info=(twai_status_info_t){.state=TWAI_STATE_RUNNING};return ESP_OK;}
esp_err_t twai_initiate_recovery(void){return ESP_OK;}
int main(void){
    assert(ts_crc32((const uint8_t *)"123456789",9)==0xcbf43926u);
    assert(ts_init()==ESP_OK);
    assert(ts_can_request_id()==0x100 && ts_can_response_id()==0x102);
    assert(install_count==1);
    assert(!ts_can_ids_discovered());
    ts_can_monitor_tick();
    assert(ts_can_ids_discovered());
    assert(ts_can_frames_rx()>=1);
    assert(strcmp(ts_can_state(),"RUNNING")==0);
    ts_reply_t reply={0};
    assert(ts_request('S',NULL,0,&reply,5000)==ESP_OK);
    assert(reply.status==0 && reply.length==7);
    assert(memcmp(reply.bytes,"rusEFI",6)==0);
    assert(send_count>1 && flow_controls==1);
    assert(ts_can_frames_rx()>1 && ts_can_frames_tx()>1);
    uint8_t input[1]={'Q'},output[128]={0};size_t n=0;
    assert(ts_raw_exchange(input,1,output,sizeof(output),&n,5000)==ESP_OK);
    assert(n==7 && memcmp(output,"rusEFI",6)==0);
    puts("PASS: passive CAN sniff + late announce, TWAI GPIO43/44, ISO-TP multi-frame + FC, CRC, S query, raw CAN counters, TCP Q");
    return 0;
}
