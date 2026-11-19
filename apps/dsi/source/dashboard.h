#ifndef BTS_DSI_DASHBOARD_H
#define BTS_DSI_DASHBOARD_H
#include "tuner.h"
#define BTS_IDLE_GAUGES 6
typedef struct {
    int field_index[BTS_IDLE_GAUGES];
    int count;
} BtIdleGauges;
/* Select INI-defined physical metrics, excluding every top-gauge identity. */
void bt_idle_select(const BtDefinition *d, const char *top_a, const char *top_b, BtIdleGauges *out);
/* Include only the actually visible metrics in the active ECU capture. */
int bt_idle_required_length(const BtDefinition *d,const BtIdleGauges *g,int minimum);
#endif
