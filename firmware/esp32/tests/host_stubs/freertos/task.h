#pragma once
#include "FreeRTOS.h"
typedef int BaseType_t;
#define pdPASS 1
int xTaskCreatePinnedToCore(void (*fn)(void*), const char *name, unsigned stack, void *arg, unsigned priority, void *handle, int core);
void vTaskDelay(uint32_t ticks);
uint32_t uxTaskGetStackHighWaterMark(void *task);
