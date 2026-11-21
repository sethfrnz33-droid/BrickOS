/* Seth: ESP-portable deterministic guide *prototype*, never connected to ECU.
 * Source-specific observations will be added later behind a verified interface.
 */
#include "seth_agent.h"
#include "seth_coordinator.h"
#include <ctype.h>
#include <string.h>

static const char *const stage_titles[SETH_STAGE_COUNT] = {
    "ECU configuration", "Trigger and synchronization", "Base tables",
    "Physical reference checks", "Starting observations", "Idle",
    "Steady-state fueling", "Ignition analysis", "Instrumented validation",
    "Final review"
};
static bool valid_stage(unsigned n){return n<SETH_STAGE_COUNT;}
static bool eq(const char *a,const char *b){return a&&b&&strcmp(a,b)==0;}
static bool contains(const char *text,const char *needle){
    return text&&needle&&strstr(text,needle)!=NULL;
}
static void base_packet(const seth_agent_t *s,seth_packet_t *p){
    memset(p,0,sizeof(*p));
    p->speaker="Seth";
    p->read_only=true;
    p->tuning_verified=false;
    p->gdi_unsupported=s->build.injection==SETH_INJ_GDI||
                       s->build.injection==SETH_INJ_MIXED;
    p->stage=s->stage;
    p->session_revision=s->session_revision;
    p->study_target=s->study_target[0]?s->study_target:NULL;
    p->evidence="No verified live ECU evidence attached. Education and user-reported build facts only.";
}
static const seth_lesson_t *find_lesson(const char *id,size_t *index){
    if(!id)return NULL;
    for(size_t i=0;i<seth_lesson_count;i++)if(eq(seth_lessons[i].id,id)){
        if(index)*index=i;
        return &seth_lessons[i];
    }
    return NULL;
}
static const char *lesson_variant(const seth_agent_t *s,const seth_lesson_t *l){
    if(s->alternate_explanation)return l->alternate;
    switch(s->education.depth){
    case BRICK_EDU_QUICK:return l->quick;
    case BRICK_EDU_LESSON:return l->lesson;
    case BRICK_EDU_TECHNICAL:return l->technical;
    default:return l->guided;
    }
}
void seth_init(seth_agent_t *s){
    if(!s)return;
    memset(s,0,sizeof(*s));
    brick_edu_init(&s->education);
    s->screen=SETH_HOME;
    s->build.injection=SETH_INJ_UNKNOWN;
}
bool seth_preferences(seth_agent_t *s,brick_edu_depth_t d,brick_edu_interaction_t i){
    return s&&brick_edu_preferences(&s->education,d,i);
}
bool seth_open_tour(seth_agent_t *s){
    if(!s)return false;
    s->screen=SETH_TOUR;
    return true;
}
bool seth_select_stage(seth_agent_t *s,unsigned stage){
    if(!s||!valid_stage(stage))return false;
    s->stage=stage;s->screen=SETH_TOUR;
    return true;
}
bool seth_open_lesson(seth_agent_t *s,const char *id){
    size_t i=0;
    const seth_lesson_t *l=find_lesson(id,&i);
    if(!s||!l)return false;
    if(!brick_edu_open(&s->education,l->id,l->version))return false;
    s->lesson_index=i;
    for(unsigned n=0;n<SETH_STAGE_COUNT;n++)
        if(eq(l->chapter,seth_stage_ids[n])){s->stage=n;break;}
    s->screen=SETH_LESSON;s->alternate_explanation=false;
    return true;
}
bool seth_explain_alternate(seth_agent_t *s){
    if(!s||s->screen!=SETH_LESSON)return false;
    const char *id=seth_lessons[s->lesson_index].id;
    if(!brick_edu_confused(&s->education,id))return false;
    s->alternate_explanation=true;
    return true;
}
bool seth_start_check(seth_agent_t *s){
    if(!s||s->screen!=SETH_LESSON)return false;
    s->screen=SETH_CHECK;
    return true;
}
bool seth_answer_check(seth_agent_t *s,int selected){
    if(!s||s->screen!=SETH_CHECK||selected< -1||selected>=3)return false;
    const seth_lesson_t *l=&seth_lessons[s->lesson_index];
    const brick_edu_check_t result=selected<0?BRICK_EDU_CHECK_SKIPPED:
        (unsigned)selected==l->correct_index?BRICK_EDU_CHECK_CORRECT:BRICK_EDU_CHECK_INCORRECT;
    if(!brick_edu_check(&s->education,l->id,result))return false;
    s->alternate_explanation=result==BRICK_EDU_CHECK_INCORRECT;
    s->screen=SETH_LESSON;
    return true;
}
bool seth_report_understanding(seth_agent_t *s){
    return s&&s->screen==SETH_LESSON&&
        brick_edu_self_report_understood(&s->education,seth_lessons[s->lesson_index].id);
}
void seth_open_build(seth_agent_t *s){if(s)s->screen=SETH_BUILD;}
bool seth_build_answer(seth_agent_t *s,const char *key,const char *value){
    if(!s||!key||!value)return false;
    /* Strict enums; never infer regulation from pump type or physical validity. */
    if(eq(key,"injection")){
        if(eq(value,"pfi"))s->build.injection=SETH_INJ_PFI;
        else if(eq(value,"gdi"))s->build.injection=SETH_INJ_GDI;
        else if(eq(value,"mixed"))s->build.injection=SETH_INJ_MIXED;
        else if(eq(value,"unknown"))s->build.injection=SETH_INJ_UNKNOWN;
        else return false;
    }else if(eq(key,"pump")){
        if(eq(value,"relay"))s->build.pump=SETH_PUMP_RELAY;
        else if(eq(value,"pwm"))s->build.pump=SETH_PUMP_PWM;
        else if(eq(value,"external"))s->build.pump=SETH_PUMP_EXTERNAL;
        else if(eq(value,"unknown"))s->build.pump=SETH_PUMP_UNKNOWN;
        else return false;
    }else if(eq(key,"regulator")){
        if(eq(value,"mechanical"))s->build.regulator=SETH_REG_MECHANICAL;
        else if(eq(value,"electronic"))s->build.regulator=SETH_REG_ELECTRONIC;
        else if(eq(value,"hybrid"))s->build.regulator=SETH_REG_HYBRID;
        else if(eq(value,"unconfirmed"))s->build.regulator=SETH_REG_NOT_CONFIRMED;
        else if(eq(value,"unknown"))s->build.regulator=SETH_REG_UNKNOWN;
        else return false;
    }else if(eq(key,"pressure_sensor")){
        if(eq(value,"ecu"))s->build.pressure_sensor=SETH_SENSOR_ECU;
        else if(eq(value,"external"))s->build.pressure_sensor=SETH_SENSOR_EXTERNAL;
        else if(eq(value,"both"))s->build.pressure_sensor=SETH_SENSOR_BOTH;
        else if(eq(value,"absent"))s->build.pressure_sensor=SETH_SENSOR_ABSENT;
        else if(eq(value,"unknown"))s->build.pressure_sensor=SETH_SENSOR_UNKNOWN;
        else return false;
    }else return false;
    if(s->build.revision<UINT32_MAX)s->build.revision++;
    s->screen=SETH_BUILD;
    return true;
}
void seth_ask_diagnosis(seth_agent_t *s,const char *user_text){
    if(!s)return;
    s->diagnostic_case=SETH_CASE_UNKNOWN;
    size_t i=0;
    if(user_text){
        for(;i+1<sizeof(s->symptom)&&user_text[i];i++)
            s->symptom[i]=(char)tolower((unsigned char)user_text[i]);
    }
    s->symptom[i]=0;
    const char *t=s->symptom;
    if(contains(t,"won't start")||contains(t,"no start")||contains(t,"crank")||contains(t,"cough")||contains(t,"start then"))s->diagnostic_case=SETH_CASE_START;
    else if(contains(t,"idle")||contains(t,"hunt")||contains(t,"wander"))s->diagnostic_case=SETH_CASE_IDLE;
    else if(contains(t,"afr")||contains(t,"lambda")||contains(t,"lean")||contains(t,"rich")||contains(t,"trim"))s->diagnostic_case=SETH_CASE_FUEL;
    else if(contains(t,"sync")||contains(t,"trigger")||contains(t,"cam"))s->diagnostic_case=SETH_CASE_SYNC;
    else if(contains(t,"sensor")||contains(t,"temperature")||contains(t,"voltage"))s->diagnostic_case=SETH_CASE_SENSOR;
    else if(contains(t,"ignition")||contains(t,"spark")||contains(t,"knock"))s->diagnostic_case=SETH_CASE_IGNITION;
    s->screen=SETH_DIAGNOSIS;
}
void seth_open_report(seth_agent_t *s){if(s)s->screen=SETH_REPORT;}
static void render_build(const seth_agent_t *s,seth_packet_t *p){
    p->title="Build archive (user-reported)";
    p->state="hardware facts unverified";
    if(s->build.injection==SETH_INJ_UNKNOWN){
        p->text="First: is this port injection (PFI), direct injection (GDI), or a mixture? Not sure is fine.";
        p->question="Injection architecture?";
        p->choices[0]="PFI";p->choices[1]="GDI";p->choices[2]="Mixed";p->choices[3]="Not sure";
        p->choice_count=4;
    }else if(p->gdi_unsupported){
        p->text="GDI and mixed PFI/GDI calibration are outside BrickOS Guided Fuel Tuning.";
        p->detail="We can still explain general ECU concepts; no GDI fuel tuning or PFI substitution.";
        p->state="unsupported_gdi";
    }else if(s->build.pump==SETH_PUMP_UNKNOWN){
        p->text="How is the pump commanded? A relay, compatible PWM controller, or an independent controller?";
        p->question="Pump command method?";
        p->choices[0]="Relay";p->choices[1]="PWM";p->choices[2]="External controller";p->choices[3]="Not sure";
        p->choice_count=4;
    }else if(s->build.regulator==SETH_REG_UNKNOWN){
        p->text="Pump command does not tell me how rail pressure is regulated. Is a mechanical regulator, electronic regulation, or both present?";
        p->question="Pressure regulation?";
        p->choices[0]="Mechanical";p->choices[1]="Electronic";p->choices[2]="Both";p->choices[3]="Not sure";
        p->choice_count=4;
    }else if(s->build.pressure_sensor==SETH_SENSOR_UNKNOWN){
        p->text="Where is rail pressure measured, if anywhere? An ECU field alone cannot verify a physical sensor.";
        p->question="Pressure evidence source?";
        p->choices[0]="ECU sensor";p->choices[1]="External reading";p->choices[2]="Both";p->choices[3]="Unknown/absent";
        p->choice_count=4;
    }else{
        p->text="I have a basic outline of the reported fuel system. That is useful context, but it is not physical verification.";
        p->detail="Keep real calibration evidence and independent hardware checks separate.";
        p->state="build_outline_recorded";
    }
}
static void render_case(const seth_agent_t *s,seth_packet_t *p){
    p->title="Read-only investigation";
    p->state="needs_evidence";
    p->detail="Several explanations remain possible. This is not a confirmed fault or instruction to operate the engine.";
    p->next="A later recorder can attach timestamped, validated ECU observations. No sensor values have been received.";
    switch(s->diagnostic_case){
    case SETH_CASE_START:
        p->text="I understand a starting problem. Possible groups include synchronization, power, fuel delivery, ignition, or mechanical issues.";
        p->question="Which documented ECU event or diagnostic trace already exists?";break;
    case SETH_CASE_IDLE:
        p->text="Idle variation can involve feedback response, load changes, airflow, fueling, or ignition corrections.";
        p->question="Do existing logs show both target and reported idle RPM, with timing?";break;
    case SETH_CASE_FUEL:
        p->text="Target and measured lambda, sensor readiness, active correction modes and bank-specific trims must be considered together.";
        p->question="Are the reported target and measurement from a valid, comparable period?";break;
    case SETH_CASE_SYNC:
        p->text="Trigger error counters and synchronization status describe different things. Tooth-level traces require a dedicated supported recorder.";
        p->question="Is a specialized ECU trigger record available?";break;
    case SETH_CASE_SENSOR:
        p->text="A decoded value is not proof of a working physical sensor. Source freshness, units and independent checks matter.";
        p->question="Which named input and existing evidence are available?";break;
    case SETH_CASE_IGNITION:
        p->text="Base and corrected spark are different; lack of reported knock cannot establish optimal timing.";
        p->question="Which independent timing and instrumented evidence has been verified?";break;
    default:
        p->text="I don't have enough context to classify that yet. Choose starting, idle, fuel, trigger, sensors, or ignition.";
        p->question="Which topic matches the issue?";break;
    }
    if((s->diagnostic_case==SETH_CASE_FUEL||s->diagnostic_case==SETH_CASE_START) &&
        (s->build.injection==SETH_INJ_GDI||s->build.injection==SETH_INJ_MIXED))
        p->next="GDI/mixed injection-specific fuel analysis is not supported; only generic read-only explanation is available.";
}
static void render_lesson(const seth_agent_t *s,seth_packet_t *p,bool check){
    if(s->lesson_index>=seth_lesson_count){p->title="Unavailable lesson";return;}
    const seth_lesson_t *l=&seth_lessons[s->lesson_index];
    p->title=l->objective;
    p->state=check?"optional_education_check":"learning_only";
    p->detail="Understanding progress is independent of hardware verification and ECU calibration.";
    if(check){
        p->text="Optional knowledge check. This never changes an ECU setting.";
        p->question=l->check_question;
        for(unsigned i=0;i<3;i++)p->choices[i]=l->options[i];
        p->choices[3]="I don't know";
        p->choice_count=4;
        p->next="A quiz result records understanding of this example only.";
    }else{
        p->text=lesson_variant(s,l);
        p->next=s->study_target[0] ? "Continue the prerequisite path when ready; the original topic is remembered." :
            (s->alternate_explanation?"Try another explanation or return to the guide.":"Explain another way, ask a question, or start an optional understanding check.");
    }
}
void seth_render(const seth_agent_t *s,seth_packet_t *p){
    if(!s||!p)return;
    base_packet(s,p);
    switch(s->screen){
    case SETH_HOME:
        p->title="Welcome to Ask Seth";
        p->text="Hey, I'm Seth. Want to go through the full setup, get a clear explanation, or pick up a topic you're curious about?";
        p->detail="Pick a topic, ask for a simpler explanation, or follow the full course. This page won't change your ECU settings.";
        p->choices[0]="Full tour";p->choices[1]="Learn a concept";
        p->choices[2]="My engine build";p->choices[3]="Investigate a symptom";
        p->choice_count=4;p->state="ready";break;
    case SETH_TOUR:
        p->title=stage_titles[s->stage];
        p->text="This chapter separates what you can learn, what the ECU reports, and what still needs independent verification.";
        p->detail="Browse any chapter or open a matching concept. Completing a page never certifies the engine.";
        p->state="chapter_open";
        p->next="Select a chapter concept from the shared lesson catalogue.";
        {
            seth_task_plan_t plan;
            if(seth_plan_current(s,&plan)){
                p->task_id=plan.task_id;
                p->task_reason=plan.reason;
                p->task_lesson_id=plan.next_lesson_id;
                p->task_question_field=plan.question_field;
                if(plan.kind==SETH_TASK_UNSUPPORTED)
                    p->next="Guided GDI fuel tuning is not supported.";
                else if(plan.kind==SETH_TASK_BUILD_QUESTION)
                    p->next="Answer the missing engine-build question, or choose Not sure.";
                else if(plan.kind==SETH_TASK_LEARN)
                    p->next="Continue to the next relevant concept lesson.";
                else
                    p->next="Learning is separate from externally validated engine evidence.";
            }
        }
        break;
    case SETH_LESSON:render_lesson(s,p,false);break;
    case SETH_CHECK:render_lesson(s,p,true);break;
    case SETH_BUILD:render_build(s,p);break;
    case SETH_DIAGNOSIS:render_case(s,p);break;
    case SETH_REPORT:
        p->title="Project learning and evidence report";
        p->text="Lessons, user-reported hardware and ECU/vehicle verification are separate outcomes.";
        p->detail="This prototype has no live ECU measurements, no verified pressure/timing checks and no validated tune.";
        p->state="not_validated";
        p->next="Review missing engine facts and independent evidence before making tuning claims.";
        break;
    }
}
