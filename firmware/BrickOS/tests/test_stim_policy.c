#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "stim_policy.h"

int main(void) {
    assert(brick_stim_isolated_policy(false,true,0,true,12.4,true));
    assert(brick_stim_isolated_policy(false,true,0,true,1.0,true));
    assert(brick_stim_isolated_policy(false,true,0,true,0.0,true));
    assert(!brick_stim_isolated_policy(false,true,0,true,12.4,false));
    assert(!brick_stim_isolated_policy(false,false,0,true,12.4,true));
    assert(!brick_stim_isolated_policy(false,true,0,false,12.4,true));
    assert(!brick_stim_isolated_policy(false,true,1000,true,12.4,true));
    assert(brick_stim_isolated_policy(true,true,1000,true,12.4,true));
    assert(!brick_stim_isolated_policy(true,true,NAN,true,12.4,true));
    assert(!brick_stim_isolated_policy(false,true,0,true,NAN,true));
    assert(!brick_stim_isolated_policy(false,true,0,true,40.0,true));
    puts("PASS: active GPIO8 isolated policy, fresh readings and stopped RPM");
    return 0;
}
