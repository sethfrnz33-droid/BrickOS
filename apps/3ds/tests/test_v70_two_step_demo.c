/* Pure UI-state test: no network, ECU commands, or physical outputs. */
#include <assert.h>
#include "bts_demo_confirm.h"
#define KEY_ENABLE 1u
#define KEY_LEFT 2u
#define KEY_RIGHT 4u
#define KEY_CANCEL 8u

int main(void) {
    BtsDemoConfirm state={0};
    assert(bts_demo_input(&state,KEY_ENABLE,KEY_ENABLE,
        KEY_ENABLE,KEY_LEFT,KEY_RIGHT,KEY_CANCEL)==0);
    bts_demo_begin(&state);
    assert(state.phase==BTS_DEMO_ENABLE);
    /* L+R alone cannot skip the first explicit ENABLE. */
    assert(bts_demo_input(&state,KEY_LEFT|KEY_RIGHT,KEY_LEFT|KEY_RIGHT,
        KEY_ENABLE,KEY_LEFT,KEY_RIGHT,KEY_CANCEL)==0);
    assert(bts_demo_input(&state,KEY_ENABLE,KEY_ENABLE,
        KEY_ENABLE,KEY_LEFT,KEY_RIGHT,KEY_CANCEL)==2);
    assert(state.phase==BTS_DEMO_SHOULDERS);
    /* Shoulders must be observed released before both can complete. */
    assert(bts_demo_input(&state,KEY_LEFT|KEY_RIGHT,KEY_LEFT|KEY_RIGHT,
        KEY_ENABLE,KEY_LEFT,KEY_RIGHT,KEY_CANCEL)==0);
    assert(bts_demo_input(&state,0,0,
        KEY_ENABLE,KEY_LEFT,KEY_RIGHT,KEY_CANCEL)==0);
    assert(bts_demo_input(&state,KEY_LEFT,KEY_LEFT,
        KEY_ENABLE,KEY_LEFT,KEY_RIGHT,KEY_CANCEL)==0);
    assert(bts_demo_input(&state,KEY_RIGHT,KEY_LEFT|KEY_RIGHT,
        KEY_ENABLE,KEY_LEFT,KEY_RIGHT,KEY_CANCEL)==1);
    assert(state.phase==BTS_DEMO_IDLE);
    /* Cancel always clears the prompt regardless of stage. */
    bts_demo_begin(&state);
    assert(bts_demo_input(&state,KEY_CANCEL,KEY_CANCEL,
        KEY_ENABLE,KEY_LEFT,KEY_RIGHT,KEY_CANCEL)==-1);
    assert(state.phase==BTS_DEMO_IDLE);
    bts_demo_begin(&state);
    assert(bts_demo_input(&state,KEY_ENABLE,KEY_ENABLE,
        KEY_ENABLE,KEY_LEFT,KEY_RIGHT,KEY_CANCEL)==2);
    assert(bts_demo_input(&state,KEY_CANCEL,KEY_CANCEL,
        KEY_ENABLE,KEY_LEFT,KEY_RIGHT,KEY_CANCEL)==-1);
    assert(state.phase==BTS_DEMO_IDLE);
    return 0;
}
