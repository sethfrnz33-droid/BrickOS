#include "esp_education_core.h"
#include <string.h>
static bool valid_id(const char *id) {
    if(!id) return false;
    size_t n=0;
    for(;n<BRICK_EDU_ID_CAP;n++) {
        const char c=id[n];
        if(!c)return n>0;
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;
    }
    return false;
}
void brick_edu_init(brick_edu_state_t *s) {
    if(s){memset(s,0,sizeof(*s));s->schema_version=1u;s->depth=BRICK_EDU_GUIDED;s->interaction=BRICK_EDU_OPTIONAL_CHECKS;}
}
bool brick_edu_preferences(brick_edu_state_t *s,brick_edu_depth_t d,brick_edu_interaction_t x) {
    if(!s||d>BRICK_EDU_TECHNICAL||x>BRICK_EDU_COURSE_MODE||d<0||x<0)return false;
    s->depth=d;s->interaction=x;s->event_sequence++;return true;
}
brick_edu_concept_t *brick_edu_find(brick_edu_state_t *s,const char *id) {
    if(!s||!valid_id(id))return NULL;
    for(size_t i=0;i<s->concept_count;i++)if(strcmp(s->concepts[i].concept_id,id)==0)return &s->concepts[i];
    return NULL;
}
bool brick_edu_open(brick_edu_state_t *s,const char *id,uint32_t version) {
    if(!s||!valid_id(id)||!version)return false;
    brick_edu_concept_t *r=brick_edu_find(s,id);
    if(!r) {
        if(s->concept_count>=BRICK_EDU_MAX_CONCEPTS)return false;
        r=&s->concepts[s->concept_count++];
        memset(r,0,sizeof(*r));
        memcpy(r->concept_id,id,strlen(id)+1);
        r->content_version_hash=version;
    } else if(r->content_version_hash!=version) {
        r->content_version_hash=version;
        r->needs_revisit=true;
        if(r->understanding==BRICK_EDU_DEMONSTRATED)r->understanding=BRICK_EDU_INTRODUCED;
    }
    if(r->understanding==BRICK_EDU_UNSEEN)r->understanding=BRICK_EDU_INTRODUCED;
    r->lesson_seen=true;
    s->event_sequence++;
    return true;
}
bool brick_edu_confused(brick_edu_state_t *s,const char *id) {
    brick_edu_concept_t *r=brick_edu_find(s,id);
    if(!r)return false;
    r->understanding=BRICK_EDU_NEEDS_HELP;
    r->requested_help++;
    r->needs_revisit=true;
    s->event_sequence++;
    return true;
}
bool brick_edu_self_report_understood(brick_edu_state_t *s,const char *id) {
    brick_edu_concept_t *r=brick_edu_find(s,id);
    if(!r)return false;
    /* Self-reported understanding is never an independent concept check. */
    if(r->understanding==BRICK_EDU_UNSEEN)r->understanding=BRICK_EDU_INTRODUCED;
    s->event_sequence++;
    return true;
}
bool brick_edu_check(brick_edu_state_t *s,const char *id,brick_edu_check_t result) {
    brick_edu_concept_t *r=brick_edu_find(s,id);
    if(!r||result<BRICK_EDU_CHECK_CORRECT||result>BRICK_EDU_CHECK_SKIPPED)return false;
    if(r->check_attempts<UINT16_MAX)r->check_attempts++;
    if(result==BRICK_EDU_CHECK_CORRECT){r->understanding=BRICK_EDU_DEMONSTRATED;r->needs_revisit=false;}
    if(result==BRICK_EDU_CHECK_INCORRECT){r->understanding=BRICK_EDU_NEEDS_HELP;r->needs_revisit=true;}
    /* Skipping the question does not alter claim of understanding. */
    s->event_sequence++;
    return true;
}
bool brick_edu_revisit_due_to_content_change(brick_edu_state_t *s,const char *id,uint32_t v) {
    brick_edu_concept_t *r=brick_edu_find(s,id);
    if(!r||!v)return false;
    if(r->content_version_hash==v)return true;
    r->content_version_hash=v;
    r->needs_revisit=true;
    if(r->understanding==BRICK_EDU_DEMONSTRATED)r->understanding=BRICK_EDU_INTRODUCED;
    s->event_sequence++;
    return true;
}
