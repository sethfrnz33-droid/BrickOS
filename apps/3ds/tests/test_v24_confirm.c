#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include "test_confirm.h"
#define L 1u
#define R 2u
#define B 4u
#define TOUCH 8u
#define A 16u
int main(void){
 BrickTestConfirm t={0};
 brick_test_confirm_begin(&t,BRICK_TEST_TRIGGER);
 assert(t.kind==BRICK_TEST_TRIGGER&&!t.released);
 assert(brick_test_confirm_input(&t,0,L|R,L,R,B)==0); /* inherited held buttons rejected */
 assert(brick_test_confirm_input(&t,0,L,L,R,B)==0);
 assert(brick_test_confirm_input(&t,0,0,L,R,B)==0); /* must release both */
 assert(t.released);
 assert(brick_test_confirm_input(&t,TOUCH|A,TOUCH|A,L,R,B)==0);
 assert(brick_test_confirm_input(&t,L,L,L,R,B)==0); /* one shoulder alone */
 assert(brick_test_confirm_input(&t,R,L|R,L,R,B)==1); /* both together */
 assert(t.kind==BRICK_TEST_NONE);
 brick_test_confirm_begin(&t,BRICK_TEST_INJECTOR);
 assert(brick_test_confirm_input(&t,B,B|L|R,L,R,B)==-1); /* cancel wins */
 assert(t.kind==BRICK_TEST_NONE);
 brick_test_confirm_begin(&t,BRICK_TEST_IGNITION);
 assert(brick_test_confirm_input(&t,0,0,L,R,B)==0);
 assert(brick_test_confirm_input(&t,L|R,L|R,L,R,B)==1);
 assert(brick_test_confirm_input(&t,L|R,L|R,L,R,B)==0);
 puts("PASS safety-confirm input: release-gate, L+R, B cancel, no tap/A or held-through");
}
