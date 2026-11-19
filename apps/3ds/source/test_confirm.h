/* Brick Tuner Suite: explicit, input-only hazardous-test confirmation.
 * This module NEVER sends an ECU command and is NOT hardware isolation.
 */
#ifndef BRICK_TEST_CONFIRM_H
#define BRICK_TEST_CONFIRM_H
#include <stdbool.h>
#include <stdint.h>

enum { BRICK_TEST_NONE=0, BRICK_TEST_TRIGGER, BRICK_TEST_INJECTOR, BRICK_TEST_IGNITION };
typedef struct { unsigned kind; bool released; } BrickTestConfirm;
static inline void brick_test_confirm_begin(BrickTestConfirm *state, unsigned kind) {
    state->kind=kind; state->released=false;
}
/* result: -1 = user cancelled, 0 = still pending, 1 = two-trigger acknowledgement.
 * Keys must have BOTH been released while the dialog was open before either
 * may be used to acknowledge. A, touch and START are never accepted.
 */
static inline int brick_test_confirm_input(BrickTestConfirm *state,
                                            uint32_t down,uint32_t held,
                                            uint32_t left,uint32_t right,
                                            uint32_t cancel) {
    if(!state->kind)return 0;
    if(down&cancel){state->kind=BRICK_TEST_NONE;state->released=false;return -1;}
    const uint32_t both=left|right;
    if(!state->released){
        if((held&both)==0)state->released=true;
        return 0;
    }
    if((held&both)==both){state->kind=BRICK_TEST_NONE;state->released=false;return 1;}
    return 0;
}
#endif
