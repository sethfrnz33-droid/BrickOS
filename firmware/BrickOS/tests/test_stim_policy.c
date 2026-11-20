#include <assert.h>
#include <stdio.h>
#include "stim_policy.h"
int main(void) {
    assert(brick_stim_policy(false,true,0,true,0));
    assert(!brick_stim_policy(false,true,1,true,0));
    assert(!brick_stim_policy(false,true,900,true,0));
    assert(brick_stim_policy(true,true,900,true,0));
    assert(!brick_stim_policy(true,true,900,true,12.4));
    assert(!brick_stim_policy(false,true,0,true,12.4));
    assert(!brick_stim_policy(false,false,0,true,0));
    assert(!brick_stim_policy(false,true,0,false,0));
    assert(!brick_stim_policy(true,true,1000,true,1.0));
    assert(!brick_stim_policy(false,true,0,true,1.0));
    assert(!brick_stim_policy(true,true,0.0/0.0,true,0));
    puts("PASS: stim OFF + nonzero RPM rejects; stim ON + synthetic RPM allowed; battery gate enforced");
    return 0;
}
