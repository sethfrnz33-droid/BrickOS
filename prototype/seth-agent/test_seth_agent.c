#include "seth_agent.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static seth_packet_t packet(const seth_agent_t *s){
    seth_packet_t p;
    seth_render(s,&p);
    assert(p.speaker && strcmp(p.speaker,"Seth")==0);
    assert(p.read_only);
    assert(!p.tuning_verified);
    assert(p.choice_count<=SETH_MAX_CHOICES);
    return p;
}
static void test_first_entry_and_course(void){
    seth_agent_t a;
    seth_init(&a);
    seth_packet_t p=packet(&a);
    assert(a.screen==SETH_HOME && p.choice_count==4);
    assert(seth_open_tour(&a));
    assert(seth_select_stage(&a,5));
    p=packet(&a);
    assert(strcmp(p.title,"Idle")==0);
    assert(!seth_select_stage(&a,99));
    assert(seth_open_lesson(&a,"idle_target"));
    p=packet(&a);
    assert(p.text && strlen(p.text)>25);
    assert(strcmp(p.state,"learning_only")==0);
    assert(a.education.concept_count==1);
    assert(seth_preferences(&a,BRICK_EDU_TECHNICAL,BRICK_EDU_OPTIONAL_CHECKS));
    p=packet(&a);
    assert(strstr(p.text,"target")!=NULL || strstr(p.text,"Target")!=NULL);
    assert(a.stage==5);
}
static void test_confusion_and_education_only(void){
    seth_agent_t a;seth_init(&a);
    assert(seth_open_lesson(&a,"fuel_trim_masking"));
    const char *first=packet(&a).text;
    assert(seth_explain_alternate(&a));
    seth_packet_t p=packet(&a);
    assert(strcmp(p.text,first)!=0);
    assert(a.education.concepts[0].understanding==BRICK_EDU_NEEDS_HELP);
    assert(seth_report_understanding(&a));
    assert(a.education.concepts[0].understanding!=BRICK_EDU_DEMONSTRATED);
    assert(seth_start_check(&a));
    p=packet(&a);
    assert(p.choice_count==4);
    const size_t idx=a.lesson_index;
    const unsigned correct=seth_lessons[idx].correct_index;
    assert(correct<3);
    assert(seth_answer_check(&a,(int)((correct+1)%3)));
    assert(a.education.concepts[0].understanding==BRICK_EDU_NEEDS_HELP);
    assert(seth_start_check(&a));
    assert(seth_answer_check(&a,(int)correct));
    assert(a.education.concepts[0].understanding==BRICK_EDU_DEMONSTRATED);
    assert(seth_start_check(&a));
    assert(seth_answer_check(&a,-1));
    assert(a.education.concepts[0].understanding==BRICK_EDU_DEMONSTRATED);
    assert(a.build.revision==0);
    assert(!a.ecu_definition_matched);
    assert(!seth_answer_check(&a,0));
}
static void test_build_architecture_without_assumptions(void){
    seth_agent_t a;seth_init(&a);seth_open_build(&a);
    seth_packet_t p=packet(&a);
    assert(strstr(p.text,"port injection")!=NULL);
    assert(seth_build_answer(&a,"injection","pfi"));
    assert(seth_build_answer(&a,"pump","pwm"));
    assert(a.build.regulator==SETH_REG_UNKNOWN);
    p=packet(&a);
    assert(strstr(p.text,"regulated")!=NULL);
    assert(!seth_build_answer(&a,"regulator","garbage"));
    assert(a.build.regulator==SETH_REG_UNKNOWN);
    assert(seth_build_answer(&a,"regulator","mechanical"));
    assert(a.build.pump==SETH_PUMP_PWM);
    assert(seth_build_answer(&a,"pressure_sensor","absent"));
    p=packet(&a);
    assert(strcmp(p.state,"build_outline_recorded")==0);
    assert(!p.tuning_verified);
    assert(a.build.revision==4);
    assert(seth_build_answer(&a,"injection","gdi"));
    p=packet(&a);
    assert(p.gdi_unsupported);
    assert(strcmp(p.state,"unsupported_gdi")==0);
    assert(!p.tuning_verified);
    assert(seth_build_answer(&a,"injection","mixed"));
    assert(packet(&a).gdi_unsupported);
}
static void test_diagnostic_is_a_hypothesis_without_sensor_input(void){
    seth_agent_t a;seth_init(&a);
    seth_ask_diagnosis(&a,"It coughs when it cranks and won't start");
    seth_packet_t p=packet(&a);
    assert(a.diagnostic_case==SETH_CASE_START);
    assert(strcmp(p.state,"needs_evidence")==0);
    assert(strstr(p.text,"Possible groups")!=NULL);
    assert(strstr(p.evidence,"No verified live")!=NULL);
    seth_ask_diagnosis(&a,"Idle wanders");
    assert(a.diagnostic_case==SETH_CASE_IDLE);
    seth_ask_diagnosis(&a,"The AFR seems wrong at cruise");
    assert(a.diagnostic_case==SETH_CASE_FUEL);
    assert(!packet(&a).tuning_verified);
    seth_ask_diagnosis(&a,"what's going on");
    assert(a.diagnostic_case==SETH_CASE_UNKNOWN);
    assert(strstr(packet(&a).text,"don't have enough context")!=NULL);
}
static void test_bounds(void){
    seth_agent_t a;seth_init(&a);
    assert(!seth_open_lesson(&a,"missing_concept"));
    assert(!seth_start_check(&a));
    assert(!seth_explain_alternate(&a));
    assert(!seth_build_answer(&a,"pressure_sensor","suspicious"));
    assert(!seth_preferences(&a,(brick_edu_depth_t)200,BRICK_EDU_RECOMMEND));
    char huge[500];memset(huge,'x',sizeof(huge));huge[sizeof(huge)-1]=0;
    seth_ask_diagnosis(&a,huge);
    assert(strlen(a.symptom)<sizeof(a.symptom));
    assert(sizeof(a)<12288);
    for(size_t n=0;n<seth_lesson_count;n++){
        assert(seth_open_lesson(&a,seth_lessons[n].id));
        seth_packet_t p=packet(&a);
        assert(p.text && p.text[0]);
        assert(p.title && p.title[0]);
    }
}
int main(void){
    assert(seth_lesson_count>=20 && seth_lesson_count<=48);
    test_first_entry_and_course();
    test_confusion_and_education_only();
    test_build_architecture_without_assumptions();
    test_diagnostic_is_a_hypothesis_without_sensor_input();
    test_bounds();
    puts("Seth standalone prototype: all tests passed (read-only)");
    return 0;
}
