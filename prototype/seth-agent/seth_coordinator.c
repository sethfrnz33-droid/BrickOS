#include "seth_coordinator.h"
#include <string.h>
static const char *const titles[SETH_STAGE_COUNT] = {
    "ECU configuration and installed hardware",
    "Synchronization and trigger interpretation",
    "Active baseline tables and injector model",
    "Independent timing and fuel-pressure references",
    "Observed starting states and event history",
    "Idle and warmup feedback",
    "Steady fuel evidence and closed-loop trims",
    "Reported ignition and independent torque evidence",
    "Qualified instrumented validation",
    "Final evidence and education audit"
};
static const char *const external[SETH_STAGE_COUNT] = {
    "Actual installed wiring, power and compatible outputs require appropriate independent verification.",
    "Actual physical trigger and timing reference need appropriate independent verification.",
    "Injector data, fuel supply assumptions and the installed hardware are not verified by an INI.",
    "Physical ignition reference and fuel-pressure measurement need independent evidence.",
    "Starting and physical system commissioning require qualified human supervision; Seth does not start an engine.",
    "Actual actuator operation, external loads and mechanical condition remain unverified.",
    "Lambda accuracy, injector/pressure model, operating regions and external validation are still unknown.",
    "No reported knock does not prove MBT or a physically correct spark reference.",
    "Independent professional instrumentation and comparable data are required for meaningful conclusions.",
    "A completed lesson is not a physically validated vehicle or a safety certification."
};
static void base(const seth_agent_t *s,unsigned stage,seth_task_plan_t *p){
    memset(p,0,sizeof(*p));
    p->kind=SETH_TASK_EXTERNAL_REVIEW;
    p->stage=stage;p->title=titles[stage];p->task_id="unverified_external_review";
    p->reason="No physically validated evidence was provided to this offline prototype.";
    p->evidence_needed="A verified ECU definition, properly attributed measurements and independent applicable evidence.";
    p->external_check=external[stage];
    p->verified=false;p->may_control_ecu=false;
    (void)s;
}
static bool fuel_stage(unsigned stage){return stage==6;}
static const char *next_build_field(const seth_agent_t *s){
    if(s->build.injection==SETH_INJ_UNKNOWN)return "injection";
    if(s->build.injection!=SETH_INJ_PFI)return NULL;
    if(s->build.pump==SETH_PUMP_UNKNOWN)return "pump";
    if(s->build.regulator==SETH_REG_UNKNOWN)return "regulator";
    if(s->build.pressure_sensor==SETH_SENSOR_UNKNOWN)return "pressure_sensor";
    return NULL;
}
bool seth_plan_stage(const seth_agent_t *s,unsigned stage,seth_task_plan_t *p){
    if(!s||!p||stage>=SETH_STAGE_COUNT)return false;
    base(s,stage,p);
    if(fuel_stage(stage)){
        if(s->build.injection==SETH_INJ_GDI||s->build.injection==SETH_INJ_MIXED){
            p->kind=SETH_TASK_UNSUPPORTED;p->task_id="unsupported_gdi";
            p->reason="BrickOS Guided Fuel Tuning does not support direct or mixed injection.";
            p->evidence_needed="No PFI fallback; this limitation does not prevent unrelated general lessons.";
            return true;
        }
        const char *missing=next_build_field(s);
        if(missing){
            p->kind=SETH_TASK_BUILD_QUESTION;
            p->task_id="build_fuel_system";
            p->reason="Seth needs a reported physical fuel-system description before choosing relevant fuel lessons. Answers are not physical verification.";
            p->question_field=missing;
            return true;
        }
    }
    const char *chapter=seth_stage_ids[stage];
    const seth_lesson_t *first=NULL,*revisit=NULL;
    for(size_t i=0;i<seth_lesson_count;i++){
        const seth_lesson_t *l=&seth_lessons[i];
        if(strcmp(l->chapter,chapter)!=0)continue;
        if(!first)first=l;
        brick_edu_concept_t *found=brick_edu_find((brick_edu_state_t *)&s->education,l->id);
        if(!found){
            p->kind=SETH_TASK_LEARN;p->task_id="teach_concept";
            p->reason="This applicable educational concept has not been introduced yet.";
            p->next_lesson_id=l->id;p->can_open_lesson=true;return true;
        }
        if(found->needs_revisit&&!revisit)revisit=l;
    }
    if(revisit){
        p->kind=SETH_TASK_LEARN;p->task_id="revisit_concept";
        p->reason="The learner requested another explanation or a previous understanding check needs review.";
        p->next_lesson_id=revisit->id;p->can_open_lesson=true;return true;
    }
    if(first){
        p->kind=SETH_TASK_REVIEW_EXISTING_EVIDENCE;p->task_id="await_existing_evidence";
        p->reason="This chapter's authored seed concepts were introduced. That does not establish the actual engine configuration, tune or safety.";
        return true;
    }
    p->task_id="lesson_not_authored";
    p->reason="This chapter does not yet have authored teaching content. Do not fabricate a lesson.";
    return true;
}
bool seth_plan_current(const seth_agent_t *s,seth_task_plan_t *p){
    return s&&seth_plan_stage(s,s->stage,p);
}
bool seth_follow_plan(seth_agent_t *s,const seth_task_plan_t *p){
    if(!s||!p||p->stage>=SETH_STAGE_COUNT)return false;
    /* Recompute instead of accepting client-supplied task IDs/fields. */
    seth_task_plan_t latest;
    if(!seth_plan_stage(s,p->stage,&latest) || latest.kind!=p->kind ||
       strcmp(latest.task_id,p->task_id)!=0)return false;
    if(latest.kind==SETH_TASK_LEARN&&latest.next_lesson_id)
        return seth_open_lesson(s,latest.next_lesson_id);
    if(latest.kind==SETH_TASK_BUILD_QUESTION){seth_open_build(s);return true;}
    return false; /* External check/unsupported cannot be auto-completed. */
}
void seth_course_summary(const seth_agent_t *s,seth_course_summary_t *out){
    if(!out)return;
    memset(out,0,sizeof(*out));
    if(!s)return;
    out->total_authored_concepts=(unsigned)seth_lesson_count;
    for(unsigned stage=0;stage<SETH_STAGE_COUNT;stage++){
        for(size_t i=0;i<seth_lesson_count;i++)
            if(strcmp(seth_lessons[i].chapter,seth_stage_ids[stage])==0){
                out->chapters_with_lessons++;
                break;
            }
    }
    for(size_t i=0;i<s->education.concept_count;i++){
        const brick_edu_concept_t *x=&s->education.concepts[i];
        if(x->lesson_seen)out->seen_concepts++;
        if(x->understanding==BRICK_EDU_DEMONSTRATED)out->demonstrated_concepts++;
        if(x->understanding==BRICK_EDU_NEEDS_HELP||x->needs_revisit)out->needs_help++;
    }
    /* Intentionally no externally verified status from a concept/quiz. */
}
