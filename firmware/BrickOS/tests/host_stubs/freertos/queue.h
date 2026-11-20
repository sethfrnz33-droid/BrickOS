#pragma once
#include <stddef.h>
#include <stdint.h>
#define pdPASS 1
#define pdTRUE 1
typedef void* QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned int,unsigned int);
int xQueueSend(QueueHandle_t,const void*,unsigned int);
int xQueueReceive(QueueHandle_t,void*,unsigned int);
void vQueueDelete(QueueHandle_t);
