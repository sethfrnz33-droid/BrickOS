#pragma once
#include <stdint.h>
typedef void *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t h, uint32_t timeout);
int xSemaphoreGive(SemaphoreHandle_t h);
