#pragma once
#include <3ds.h>
Result ptmuInit(void);
void ptmuExit(void);
Result PTMU_GetBatteryLevel(u8 *out);
Result PTMU_GetBatteryChargeState(u8 *out);
