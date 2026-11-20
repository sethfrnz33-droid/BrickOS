/* Extended standalone tests: stage negotiation, UI revision and checkpoint.
 * These tests intentionally cannot access an ECU or hardware.
 */
#include "seth_coordinator.h"
#include "seth_checkpoint.h"
#include "seth_actions.h"
#include "seth_packet_json.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void test_chapter_planning(void){
    seth_agent_t s;seth_init(&s);
    seth_task_plan_t p;
    assert(seth_plan_stage(&s,0,&p));
    assert(p.kind==SETH_TASK_LEARN);
    assert(strcmp(p.next_lesson_id,"ecu_vs_engine")==0);
    assert(!p.verified&&!p.may_control_ecu);
    assert(seth_follow_plan(&s,&p));
    assert(s.screen==SETH_LESSON);
    assert(seth_plan_stage(&s,0,&p));
    assert(p.kind==SETH_TASK_LEARN);
    assert(strcmp(p.next_lesson_id,"ini_definition")==0);
    assert(seth_plan_stage(&s,6,&p));
    assert(p.kind==SETH_TASK_BUILD_QUESTION);
    assert(strcmp(p.question_field,"injection")==0);
    assert(!seth_plan_stage(&s,10,&p));
    seth_course_summary_t summary;
    seth_course_summary(&s,&summary);
    assert(summary.seen_concepts==1);
    assert(summary.externally_verified_chapters==0);
    assert(summary.total_authored_concepts==seth_lesson_count);
    assert(summary.chapters_with_lessons==10);
}

static void test_regulator_and_gdi_branches(void){
    seth_agent_t s;seth_init(&s);
    seth_task_plan_t p;
    assert(seth_build_answer(&s,"injection","pfi"));
    assert(seth_plan_stage(&s,6,&p));
    assert(strcmp(p.question_field,"pump")==0);
    assert(seth_build_answer(&s,"pump","pwm"));
    assert(s.build.regulator==SETH_REG_UNKNOWN);
    assert(seth_plan_stage(&s,6,&p));
    assert(p.kind==SETH_TASK_BUILD_QUESTION);
    assert(strcmp(p.question_field,"regulator")==0);
    assert(seth_build_answer(&s,"regulator","mechanical"));
    assert(seth_plan_stage(&s,6,&p));
    assert(strcmp(p.question_field,"pressure_sensor")==0);
    assert(seth_build_answer(&s,"pressure_sensor","absent"));
    assert(seth_plan_stage(&s,6,&p));
    assert(p.kind==SETH_TASK_LEARN);
    assert(!p.may_control_ecu);
    assert(s.build.pump==SETH_PUMP_PWM);
    assert(s.build.regulator==SETH_REG_MECHANICAL);
    assert(seth_build_answer(&s,"injection","gdi"));
    assert(seth_plan_stage(&s,6,&p));
    assert(p.kind==SETH_TASK_UNSUPPORTED);
    assert(strcmp(p.task_id,"unsupported_gdi")==0);
    assert(!seth_follow_plan(&s,&p));
    assert(seth_build_answer(&s,"injection","mixed"));
    assert(seth_plan_stage(&s,6,&p)&&p.kind==SETH_TASK_UNSUPPORTED);
    assert(seth_plan_stage(&s,0,&p));
    assert(p.kind==SETH_TASK_LEARN); /* GDI scope does not block general theory. */
}

static void test_action_revision_and_scope(void){
    seth_agent_t s;seth_init(&s);
    assert(!seth_dispatch(&s,1,"full_tour",""));
    assert(s.screen==SETH_HOME && s.session_revision==0);
    assert(seth_dispatch(&s,0,"full_tour",""));
    assert(s.session_revision==1);
    assert(!seth_dispatch(&s,0,"build",""));
    assert(!seth_dispatch(&s,1,"ts_write","evil"));
    assert(!seth_dispatch(&s,1,"start_engine",""));
    assert(!seth_dispatch(&s,1,"set_fact","pump=pwm"));
    assert(seth_dispatch(&s,1,"stage","6"));
    assert(seth_dispatch(&s,2,"continue",""));
    assert(s.screen==SETH_BUILD);
    assert(seth_dispatch(&s,3,"set_fact","injection=pfi"));
    assert(seth_dispatch(&s,4,"set_fact","pump=pwm"));
    assert(s.build.regulator==SETH_REG_UNKNOWN);
    assert(!seth_dispatch(&s,5,"set_fact","regulator=fake"));
    assert(s.session_revision==5);
    assert(seth_dispatch(&s,5,"set_fact","regulator=mechanical"));
    assert(seth_dispatch(&s,6,"set_fact","pressure_sensor=unknown"));
    assert(seth_dispatch(&s,7,"depth","technical"));
    assert(s.education.depth==BRICK_EDU_TECHNICAL);
    assert(seth_dispatch(&s,8,"interaction","course"));
    assert(s.education.interaction==BRICK_EDU_COURSE_MODE);
    assert(!seth_dispatch(&s,9,"burn_flash",""));
    assert(seth_dispatch(&s,9,"report",""));
    assert(s.screen==SETH_REPORT);
    seth_packet_t p;seth_render(&s,&p);
    assert(!p.tuning_verified&&p.read_only);
}

static void test_checkpoint_roundtrip_and_tamper(void){
    seth_agent_t original,restored,scratch;
    seth_init(&original);
    assert(seth_dispatch(&original,0,"lesson","lambda_vs_target"));
    assert(seth_dispatch(&original,1,"explain_alternate",""));
    assert(seth_dispatch(&original,2,"check_start",""));
    assert(seth_dispatch(&original,3,"check_answer","skip"));
    assert(seth_dispatch(&original,4,"build",""));
    assert(seth_dispatch(&original,5,"set_fact","injection=pfi"));
    assert(seth_dispatch(&original,6,"set_fact","pump=pwm"));
    assert(seth_dispatch(&original,7,"set_fact","regulator=mechanical"));
    assert(seth_dispatch(&original,8,"set_fact","pressure_sensor=ecu"));
    original.ecu_definition_matched=true; /* must never be restored */
    uint8_t bytes[SETH_CHECKPOINT_MAX_BYTES];
    size_t n=seth_checkpoint_encode(&original,bytes,sizeof(bytes));
    assert(n>80 && n<sizeof(bytes));
    seth_init(&restored);
    assert(seth_checkpoint_restore(&restored,&scratch,bytes,n));
    assert(restored.session_revision==original.session_revision);
    assert(restored.build.revision==original.build.revision);
    assert(restored.build.regulator==SETH_REG_MECHANICAL);
    assert(restored.build.pump==SETH_PUMP_PWM);
    assert(restored.education.concept_count==1);
    assert(strcmp(restored.education.concepts[0].concept_id,"lambda_vs_target")==0);
    assert(restored.education.concepts[0].understanding==BRICK_EDU_NEEDS_HELP);
    assert(!restored.ecu_definition_matched);
    assert(!seth_dispatch(&restored,2,"report","")); /* stale action */
    assert(seth_dispatch(&restored,restored.session_revision,"report",""));
    uint32_t rev=restored.session_revision;
    bytes[n-1]^=1;
    assert(!seth_checkpoint_restore(&restored,&scratch,bytes,n));
    assert(restored.session_revision==rev);
    bytes[n-1]^=1;
    assert(!seth_checkpoint_restore(&restored,&scratch,bytes,n-1));
    assert(!seth_checkpoint_restore(&restored,&restored,bytes,n));
    assert(restored.session_revision==rev);
}

static void test_capacity_and_repeated_use(void){
    seth_agent_t s,temp;seth_init(&s);
    uint8_t blob[SETH_CHECKPOINT_MAX_BYTES];
    for(unsigned n=0;n<1200;n++){
        assert(seth_dispatch(&s,s.session_revision,"full_tour",""));
        assert(seth_dispatch(&s,s.session_revision,"stage","6"));
        assert(seth_dispatch(&s,s.session_revision,"home",""));
        size_t len=seth_checkpoint_encode(&s,blob,sizeof(blob));
        assert(len>30&&len<sizeof(blob));
        assert(seth_checkpoint_restore(&s,&temp,blob,len));
        assert(!s.ecu_definition_matched);
    }
    assert(!seth_dispatch(&s,s.session_revision,"set_fact","injection=gdi"));
    char oversized[200];memset(oversized,'x',sizeof(oversized));oversized[199]=0;
    assert(!seth_dispatch(&s,s.session_revision,"diagnose",oversized));
}

int main(void){
    test_chapter_planning();
    test_regulator_and_gdi_branches();
    test_action_revision_and_scope();
    test_checkpoint_roundtrip_and_tamper();
    test_capacity_and_repeated_use();
    puts("Seth coordinator, action authority separation, and checkpoint tests passed");
    return 0;
}
