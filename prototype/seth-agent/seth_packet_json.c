#include "seth_packet_json.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {char *p;size_t cap,len;bool ok;} builder_t;
static void append(builder_t *b,const char *s){
    if(!b->ok||!s){b->ok=false;return;}
    for(;*s;s++){
        if(b->len+1>=b->cap){b->ok=false;return;}
        b->p[b->len++]=*s;
    }
}
static void byte(builder_t *b,char c){
    if(!b->ok)return;
    if(b->len+1>=b->cap){b->ok=false;return;}
    b->p[b->len++]=c;
}
static void number(builder_t *b,unsigned n){
    char chars[11];unsigned count=0;
    do{chars[count++]=(char)('0'+(n%10));n/=10;}while(n&&count<sizeof(chars));
    while(count)byte(b,chars[--count]);
}
static void json_string(builder_t *b,const char *s){
    if(!s){append(b,"null");return;}
    static const char *const hex="0123456789abcdef";
    byte(b,'"');
    for(const unsigned char *a=(const unsigned char *)s;*a&&b->ok;a++){
        const unsigned c=*a;
        if(c=='"'||c=='\\'){byte(b,'\\');byte(b,(char)c);}
        else if(c=='\n')append(b,"\\n");
        else if(c=='\r')append(b,"\\r");
        else if(c=='\t')append(b,"\\t");
        else if(c<32u){
            append(b,"\\u00");byte(b,hex[c>>4]);byte(b,hex[c&15]);
        }else byte(b,(char)c);
    }
    byte(b,'"');
}
static void property(builder_t *b,const char *name,const char *value){
    json_string(b,name);byte(b,':');json_string(b,value);
}
size_t seth_packet_json(const seth_packet_t *p,char *out,size_t cap){
    if(!p||!out||cap<16||p->choice_count>SETH_MAX_CHOICES)return 0;
    builder_t b={out,cap,0,true};
    byte(&b,'{');
    property(&b,"schema","brickos.seth.packet.v0");byte(&b,',');
    property(&b,"speaker",p->speaker);byte(&b,',');
    property(&b,"title",p->title);byte(&b,',');
    property(&b,"text",p->text);byte(&b,',');
    property(&b,"detail",p->detail);byte(&b,',');
    property(&b,"question",p->question);byte(&b,',');
    property(&b,"state",p->state);byte(&b,',');
    property(&b,"evidence",p->evidence);byte(&b,',');
    property(&b,"next",p->next);byte(&b,',');
    property(&b,"study_target",p->study_target);byte(&b,',');
    property(&b,"task_id",p->task_id);byte(&b,',');
    property(&b,"task_reason",p->task_reason);byte(&b,',');
    property(&b,"task_lesson_id",p->task_lesson_id);byte(&b,',');
    property(&b,"task_question_field",p->task_question_field);byte(&b,',');
    append(&b,"\"choices\":[");
    for(unsigned i=0;i<p->choice_count;i++){
        if(i)byte(&b,',');
        json_string(&b,p->choices[i]);
    }
    append(&b,"],\"stage\":");number(&b,p->stage);
    append(&b,",\"revision\":");number(&b,p->session_revision);
    append(&b,",\"read_only\":");append(&b,p->read_only?"true":"false");
    append(&b,",\"tuning_verified\":");append(&b,p->tuning_verified?"true":"false");
    append(&b,",\"gdi_unsupported\":");append(&b,p->gdi_unsupported?"true":"false");
    byte(&b,'}');
    if(!b.ok){out[0]=0;return 0;}
    out[b.len]=0;
    return b.len;
}
