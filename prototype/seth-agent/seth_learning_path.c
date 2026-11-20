#include "seth_learning_path.h"
#include <string.h>

static const seth_lesson_t *lesson(const char *id){
    if(!id)return NULL;
    for(size_t i=0;i<seth_lesson_count;i++)
        if(strcmp(id,seth_lessons[i].id)==0)return &seth_lessons[i];
    return NULL;
}
static bool seen(const seth_agent_t *s,const char *id){
    for(size_t i=0;i<s->education.concept_count;i++)
        if(strcmp(s->education.concepts[i].concept_id,id)==0)
            return s->education.concepts[i].lesson_seen;
    return false;
}
static const char *first_unseen(const seth_agent_t *s,const seth_lesson_t *l,unsigned depth){
    if(!l||depth>=BRICK_EDU_MAX_CONCEPTS)return NULL;
    for(unsigned i=0;i<l->prerequisite_count;i++){
        const char *dep=l->prerequisites[i];
        const seth_lesson_t *p=lesson(dep);
        if(!p)return NULL; /* catalog corruption must not create a false path */
        if(!seen(s,dep)){
            const char *earlier=first_unseen(s,p,depth+1);
            return earlier?earlier:dep;
        }
    }
    return l->id;
}
bool seth_request_topic(seth_agent_t *s,const char *id){
    if(!s||!lesson(id))return false;
    const size_t len=strlen(id);
    if(len>=sizeof(s->study_target))return false;
    char saved[BRICK_EDU_ID_CAP];memcpy(saved,s->study_target,sizeof(saved));
    memcpy(s->study_target,id,len+1);
    if(seth_continue_topic(s))return true;
    memcpy(s->study_target,saved,sizeof(saved));
    return false;
}
bool seth_continue_topic(seth_agent_t *s){
    if(!s||!s->study_target[0])return false;
    const seth_lesson_t *goal=lesson(s->study_target);
    if(!goal)return false;
    if(s->screen==SETH_LESSON&&s->lesson_index<seth_lesson_count&&
        strcmp(seth_lessons[s->lesson_index].id,goal->id)==0){
        s->study_target[0]=0;
        s->screen=SETH_TOUR; /* finishing the lesson is not engine verification */
        return true;
    }
    const char *next=first_unseen(s,goal,0);
    if(!next)return false;
    return seth_open_lesson(s,next);
}
