/* Brick Tuner Suite: LOCAL, NON-ACTUATING output-test UI demonstration.
 * It never transmits a controller command or authorizes physical hardware.
 * A chooses the displayed demo command; then both shoulders must be released
 * and held together to finish the UI demonstration. B cancels either step.
 */
#ifndef BTS_DEMO_CONFIRM_H
#define BTS_DEMO_CONFIRM_H

#include <stdbool.h>
#include <stdint.h>

enum {
    BTS_DEMO_IDLE=0,
    BTS_DEMO_ENABLE=1,
    BTS_DEMO_SHOULDERS=2
};

typedef struct {
    uint8_t phase;
    bool shoulders_released;
} BtsDemoConfirm;

static inline void bts_demo_begin(BtsDemoConfirm *state) {
    if (!state) return;
    state->phase=BTS_DEMO_ENABLE;
    state->shoulders_released=false;
}

static inline void bts_demo_reset(BtsDemoConfirm *state) {
    if (!state) return;
    state->phase=BTS_DEMO_IDLE;
    state->shoulders_released=false;
}

/* Return 2 after explicit ENABLE, 1 after completed two-shoulder demo,
 * -1 for cancel, or 0 for no state transition. No hardware side effects.
 * 'down' means newly pressed buttons; 'held' means currently held buttons.
 */
static inline int bts_demo_input(BtsDemoConfirm *state,
                                 uint32_t down,uint32_t held,
                                 uint32_t enable,uint32_t left,
                                 uint32_t right,uint32_t cancel) {
    if (!state || state->phase==BTS_DEMO_IDLE) return 0;
    if (down & cancel) {bts_demo_reset(state);return -1;}
    if (state->phase==BTS_DEMO_ENABLE) {
        if (down & enable) {
            state->phase=BTS_DEMO_SHOULDERS;
            state->shoulders_released=false;
            return 2;
        }
        return 0;
    }
    if (state->phase==BTS_DEMO_SHOULDERS) {
        const uint32_t both=left|right;
        if (!state->shoulders_released) {
            if (!(held & both)) state->shoulders_released=true;
            return 0;
        }
        if ((held & both)==both) {
            bts_demo_reset(state);
            return 1;
        }
    }
    return 0;
}
#endif
