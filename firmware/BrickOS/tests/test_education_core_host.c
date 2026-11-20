/* Host-side tests for the ESP-portable, read-only education state. */
#include "../../../prototype/esp_education_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
  brick_edu_state_t s;
  brick_edu_init(&s);
  assert(s.schema_version==1u);
  assert(s.concept_count==0u);
  assert(s.depth==BRICK_EDU_GUIDED);
  assert(brick_edu_open(&s,"lambda_vs_target",100u));
  brick_edu_concept_t *e=brick_edu_find(&s,"lambda_vs_target");
  assert(e && e->lesson_seen && e->understanding==BRICK_EDU_INTRODUCED);
  assert(brick_edu_preferences(&s,BRICK_EDU_QUICK,BRICK_EDU_COURSE_MODE));
  assert(s.depth==BRICK_EDU_QUICK && s.interaction==BRICK_EDU_COURSE_MODE);
  assert(!brick_edu_preferences(&s,(brick_edu_depth_t)88,BRICK_EDU_COURSE_MODE));
  assert(brick_edu_confused(&s,"lambda_vs_target"));
  assert(e->understanding==BRICK_EDU_NEEDS_HELP && e->needs_revisit);
  assert(brick_edu_self_report_understood(&s,"lambda_vs_target"));
  assert(e->understanding==BRICK_EDU_NEEDS_HELP); /* self-report != verified skill */
  assert(brick_edu_check(&s,"lambda_vs_target",BRICK_EDU_CHECK_SKIPPED));
  assert(e->understanding==BRICK_EDU_NEEDS_HELP);
  assert(brick_edu_check(&s,"lambda_vs_target",BRICK_EDU_CHECK_CORRECT));
  assert(e->understanding==BRICK_EDU_DEMONSTRATED && !e->needs_revisit);
  assert(brick_edu_revisit_due_to_content_change(&s,"lambda_vs_target",101u));
  assert(e->understanding==BRICK_EDU_INTRODUCED && e->needs_revisit);
  assert(brick_edu_check(&s,"lambda_vs_target",BRICK_EDU_CHECK_INCORRECT));
  assert(e->understanding==BRICK_EDU_NEEDS_HELP);
  assert(!brick_edu_open(&s,"bad id",1u));
  assert(!brick_edu_open(&s,"",1u));
  assert(!brick_edu_open(&s,"unknown",0u));
  for(unsigned i=0;i<BRICK_EDU_MAX_CONCEPTS-1;i++) {
    char id[32];snprintf(id,sizeof(id),"test_%u",i);
    assert(brick_edu_open(&s,id,1u));
  }
  assert(s.concept_count==BRICK_EDU_MAX_CONCEPTS);
  assert(!brick_edu_open(&s,"overflow",1u));
  assert(brick_edu_find(&s,"overflow")==NULL);
  assert(sizeof(s)<8192u);
  puts("BrickOS education core: all bounded, read-only tests passed");
  return 0;
}
