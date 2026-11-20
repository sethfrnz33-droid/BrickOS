#include "seth_actions.h"
#include "seth_coordinator.h"
#include "seth_learning_path.h"
#include <string.h>
#include <stdint.h>
static bool same(const char *a,const char *b){return a&&b&&strcmp(a,b)==0;}
static bool small(const char *s,size_t max){
    if(!s)return false;
    for(size_t i=0;i<=max;i++)if(s[i]==0)return i<=max;
    return false;
}
static bool choose_depth(seth_agent_t *a,const char *value){
    if(same(value,"quick"))return seth_preferences(a,BRICK_EDU_QUICK,a->education.interaction);
    if(same(value,"guided"))return seth_preferences(a,BRICK_EDU_GUIDED,a->education.interaction);
    if(same(value,"lesson"))return seth_preferences(a,BRICK_EDU_LESSON,a->education.interaction);
    if(same(value,"technical"))return seth_preferences(a,BRICK_EDU_TECHNICAL,a->education.interaction);
    return false;
}
static bool choose_interaction(seth_agent_t *a,const char *value){
    if(same(value,"recommend"))return seth_preferences(a,a->education.depth,BRICK_EDU_RECOMMEND);
    if(same(value,"optional_checks"))return seth_preferences(a,a->education.depth,BRICK_EDU_OPTIONAL_CHECKS);
    if(same(value,"teach_as_we_go"))return seth_preferences(a,a->education.depth,BRICK_EDU_TEACH_AS_YOU_GO);
    if(same(value,"course"))return seth_preferences(a,a->education.depth,BRICK_EDU_COURSE_MODE);
    return false;
}
static bool parse_fact(seth_agent_t *a,const char *value){
    if(a->screen!=SETH_BUILD||!small(value,90))return false;
    const char *separator=strchr(value,'=');
    if(!separator||separator==value||!separator[1])return false;
    size_t klen=(size_t)(separator-value),vlen=strlen(separator+1);
    if(klen>=32||vlen>=48)return false;
    char key[32],val[48];
    memcpy(key,value,klen);key[klen]=0;
    memcpy(val,separator+1,vlen+1);
    return seth_build_answer(a,key,val);
}
bool seth_dispatch(seth_agent_t *s,uint32_t revision,
                   const char *action,const char *value){
    if(!s||revision!=s->session_revision||revision==UINT32_MAX||
       !small(action,35))return false;
    bool ok=false;
    if(same(action,"home")){s->screen=SETH_HOME;s->study_target[0]=0;ok=true;}
    else if(same(action,"full_tour")){s->study_target[0]=0;ok=seth_open_tour(s);}
    else if(same(action,"stage")){
        if(small(value,1)&&value[0]>='0'&&value[0]<='9'&&value[1]==0)
            ok=seth_select_stage(s,(unsigned)(value[0]-'0'));
    }
    else if(same(action,"continue")){
        seth_task_plan_t plan;
        if(seth_plan_current(s,&plan))ok=seth_follow_plan(s,&plan);
    }
    else if(same(action,"lesson")){
        if(small(value,BRICK_EDU_ID_CAP-1)){
            ok=seth_open_lesson(s,value);
            if(ok)s->study_target[0]=0;
        }
    }
    else if(same(action,"study_topic")){
        if(small(value,BRICK_EDU_ID_CAP-1))ok=seth_request_topic(s,value);
    }
    else if(same(action,"continue_topic"))ok=seth_continue_topic(s);
    else if(same(action,"depth")){
        if(small(value,24))ok=choose_depth(s,value);
    }
    else if(same(action,"interaction")){
        if(small(value,24))ok=choose_interaction(s,value);
    }
    else if(same(action,"explain_alternate"))ok=seth_explain_alternate(s);
    else if(same(action,"check_start"))ok=seth_start_check(s);
    else if(same(action,"check_answer")){
        if(same(value,"skip"))ok=seth_answer_check(s,-1);
        else if(small(value,1)&&value[0]>='0'&&value[0]<='2'&&value[1]==0)
            ok=seth_answer_check(s,value[0]-'0');
    }
    else if(same(action,"i_understand"))ok=seth_report_understanding(s);
    else if(same(action,"build")){seth_open_build(s);ok=true;}
    else if(same(action,"set_fact"))ok=parse_fact(s,value);
    else if(same(action,"diagnose")&&small(value,SETH_MAX_SYMPTOM-1)){
        seth_ask_diagnosis(s,value);ok=true;
    }
    else if(same(action,"report")){seth_open_report(s);ok=true;}
    if(ok)s->session_revision++;
    return ok;
}
