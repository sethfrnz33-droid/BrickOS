#include "seth_checkpoint.h"
#include <string.h>

static const uint8_t MAGIC[8]={'B','R','I','C','K','S','0','1'};
#define HEADER 16u
typedef struct {uint8_t *buffer;size_t len,pos;bool valid;} writer_t;
typedef struct {const uint8_t *buffer;size_t len,pos;bool valid;} reader_t;
static void w8(writer_t *w,uint8_t v){
    if(!w->valid||w->pos>=w->len){w->valid=false;return;}
    w->buffer[w->pos++]=v;
}
static void w16(writer_t *w,uint16_t v){w8(w,(uint8_t)v);w8(w,(uint8_t)(v>>8));}
static void w32(writer_t *w,uint32_t v){w16(w,(uint16_t)v);w16(w,(uint16_t)(v>>16));}
static uint8_t r8(reader_t *r){
    if(!r->valid||r->pos>=r->len){r->valid=false;return 0;}
    return r->buffer[r->pos++];
}
static uint16_t r16(reader_t *r){
    uint16_t low=r8(r),high=r8(r);return (uint16_t)(low|(high<<8));
}
static uint32_t r32(reader_t *r){
    uint32_t low=r16(r),high=r16(r);return low|(high<<16);
}
static uint32_t crc32_data(const uint8_t *p,size_t n){
    uint32_t crc=0xffffffffu;
    for(size_t i=0;i<n;i++){
        crc^=p[i];
        for(unsigned bit=0;bit<8;bit++)
            crc=(crc>>1)^((crc&1u)?0xedb88320u:0u);
    }
    return ~crc;
}
static bool valid_id(const char *s,size_t n){
    if(n==0||n>=BRICK_EDU_ID_CAP)return false;
    for(size_t i=0;i<n;i++){
        unsigned char c=(unsigned char)s[i];
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;
    }
    return true;
}
static size_t length_bounded(const char *s,size_t max){
    if(!s)return max;
    for(size_t n=0;n<max;n++)if(s[n]==0)return n;
    return max;
}
static void wtext(writer_t *w,const char *str,size_t max){
    size_t n=length_bounded(str,max);
    if(n>=max||n>255u){w->valid=false;return;}
    w8(w,(uint8_t)n);
    for(size_t i=0;i<n;i++)w8(w,(uint8_t)str[i]);
}
static bool rtext(reader_t *r,char *out,size_t max){
    size_t n=r8(r);
    if(!r->valid||n>=max){r->valid=false;return false;}
    for(size_t i=0;i<n;i++)out[i]=(char)r8(r);
    if(!r->valid)return false;
    out[n]=0;
    return true;
}
static const char *active_lesson_id(const seth_agent_t *s){
    if(s->lesson_index>=seth_lesson_count)return NULL;
    return seth_lessons[s->lesson_index].id;
}
size_t seth_checkpoint_encode(const seth_agent_t *s,uint8_t *buffer,size_t cap){
    if(!s||!buffer||cap<HEADER+24u||cap>SETH_CHECKPOINT_MAX_BYTES||
       s->education.concept_count>BRICK_EDU_MAX_CONCEPTS||
       s->screen>SETH_REPORT||s->stage>=SETH_STAGE_COUNT||
       s->education.depth>BRICK_EDU_TECHNICAL||
       s->education.interaction>BRICK_EDU_COURSE_MODE||
       s->build.injection>SETH_INJ_MIXED||s->build.pump>SETH_PUMP_EXTERNAL||
       s->build.regulator>SETH_REG_NOT_CONFIRMED||
       s->build.pressure_sensor>SETH_SENSOR_ABSENT)return 0;
    memset(buffer,0,HEADER);
    writer_t w={buffer,cap,HEADER,true};
    w8(&w,(uint8_t)s->screen);w8(&w,(uint8_t)s->stage);
    w8(&w,(uint8_t)s->education.depth);w8(&w,(uint8_t)s->education.interaction);
    w8(&w,s->alternate_explanation?1u:0u);w8(&w,(uint8_t)s->diagnostic_case);
    w8(&w,(uint8_t)s->build.injection);w8(&w,(uint8_t)s->build.pump);
    w8(&w,(uint8_t)s->build.regulator);w8(&w,(uint8_t)s->build.pressure_sensor);
    w32(&w,s->build.revision);
    w32(&w,s->education.event_sequence);
    /* Never serialize ecu_definition_matched. A fresh ECU handshake is required. */
    const char *lesson=active_lesson_id(s);
    wtext(&w,lesson?lesson:"",BRICK_EDU_ID_CAP);
    w8(&w,(uint8_t)s->education.concept_count);
    for(size_t i=0;i<s->education.concept_count;i++){
        const brick_edu_concept_t *c=&s->education.concepts[i];
        size_t n=length_bounded(c->concept_id,BRICK_EDU_ID_CAP);
        if(!valid_id(c->concept_id,n)||!c->content_version_hash||
           c->understanding>BRICK_EDU_DEMONSTRATED){w.valid=false;break;}
        wtext(&w,c->concept_id,BRICK_EDU_ID_CAP);
        w32(&w,c->content_version_hash);
        w16(&w,c->check_attempts);
        w8(&w,c->understanding);w8(&w,c->requested_help);
        w8(&w,c->needs_revisit?1u:0u);
        w8(&w,c->lesson_seen?1u:0u);
    }
    size_t symptom_len=length_bounded(s->symptom,SETH_MAX_SYMPTOM);
    if(symptom_len>=SETH_MAX_SYMPTOM)w.valid=false;
    w16(&w,(uint16_t)symptom_len);
    for(size_t i=0;i<symptom_len;i++)w8(&w,(uint8_t)s->symptom[i]);
    if(!w.valid||w.pos-HEADER>UINT32_MAX){buffer[0]=0;return 0;}
    memcpy(buffer,MAGIC,sizeof(MAGIC));
    uint32_t payload=(uint32_t)(w.pos-HEADER);
    buffer[8]=(uint8_t)payload;buffer[9]=(uint8_t)(payload>>8);
    buffer[10]=(uint8_t)(payload>>16);buffer[11]=(uint8_t)(payload>>24);
    uint32_t crc=crc32_data(buffer+HEADER,payload);
    buffer[12]=(uint8_t)crc;buffer[13]=(uint8_t)(crc>>8);
    buffer[14]=(uint8_t)(crc>>16);buffer[15]=(uint8_t)(crc>>24);
    return w.pos;
}
bool seth_checkpoint_restore(seth_agent_t *dst,seth_agent_t *scratch,
                             const uint8_t *blob,size_t len){
    if(!dst||!scratch||!blob||dst==scratch||len<HEADER+24u||
       len>SETH_CHECKPOINT_MAX_BYTES||memcmp(blob,MAGIC,sizeof(MAGIC))!=0)return false;
    reader_t header={blob,len,8,true};
    uint32_t payload=r32(&header),crc=r32(&header);
    if(!header.valid||payload!=len-HEADER||
       crc32_data(blob+HEADER,payload)!=crc)return false;
    seth_init(scratch);
    reader_t r={blob,len,HEADER,true};
    uint8_t screen=r8(&r),stage=r8(&r),depth=r8(&r),interaction=r8(&r);
    uint8_t alt=r8(&r),diagnostic=r8(&r),injection=r8(&r),pump=r8(&r);
    uint8_t regulator=r8(&r),sensor=r8(&r);
    uint32_t revision=r32(&r),sequence=r32(&r);
    char lesson[BRICK_EDU_ID_CAP];
    if(!rtext(&r,lesson,sizeof(lesson)))return false;
    if(!r.valid||screen>SETH_REPORT||stage>=SETH_STAGE_COUNT||
       depth>BRICK_EDU_TECHNICAL||interaction>BRICK_EDU_COURSE_MODE||
       alt>1||diagnostic>SETH_CASE_IGNITION||injection>SETH_INJ_MIXED||
       pump>SETH_PUMP_EXTERNAL||regulator>SETH_REG_NOT_CONFIRMED||
       sensor>SETH_SENSOR_ABSENT)return false;
    scratch->screen=(seth_screen_t)screen;scratch->stage=stage;
    scratch->education.depth=(brick_edu_depth_t)depth;
    scratch->education.interaction=(brick_edu_interaction_t)interaction;
    scratch->alternate_explanation=alt!=0;
    scratch->diagnostic_case=(seth_case_t)diagnostic;
    scratch->build.injection=(seth_injection_t)injection;
    scratch->build.pump=(seth_pump_t)pump;
    scratch->build.regulator=(seth_regulator_t)regulator;
    scratch->build.pressure_sensor=(seth_pressure_sensor_t)sensor;
    scratch->build.revision=revision;scratch->education.event_sequence=sequence;
    uint8_t count=r8(&r);
    if(!r.valid||count>BRICK_EDU_MAX_CONCEPTS)return false;
    for(size_t i=0;i<count;i++){
        brick_edu_concept_t *c=&scratch->education.concepts[i];
        if(!rtext(&r,c->concept_id,sizeof(c->concept_id)))return false;
        size_t n=strlen(c->concept_id);
        if(!valid_id(c->concept_id,n))return false;
        for(size_t j=0;j<i;j++)if(strcmp(scratch->education.concepts[j].concept_id,c->concept_id)==0)return false;
        c->content_version_hash=r32(&r);
        c->check_attempts=r16(&r);
        c->understanding=r8(&r);
        c->requested_help=r8(&r);
        uint8_t revisit=r8(&r),seen=r8(&r);
        if(!r.valid||!c->content_version_hash||c->understanding>BRICK_EDU_DEMONSTRATED||
           revisit>1||seen>1)return false;
        c->needs_revisit=revisit!=0;c->lesson_seen=seen!=0;
    }
    scratch->education.concept_count=count;
    uint16_t symptom_len=r16(&r);
    if(!r.valid||symptom_len>=SETH_MAX_SYMPTOM)return false;
    for(size_t i=0;i<symptom_len;i++){
        scratch->symptom[i]=(char)r8(&r);
        if(scratch->symptom[i]==0)return false;
    }
    scratch->symptom[symptom_len]=0;
    if(!r.valid||r.pos!=len)return false;
    bool found=false;
    for(size_t i=0;i<seth_lesson_count;i++){
        if(strcmp(seth_lessons[i].id,lesson)==0){
            scratch->lesson_index=i;found=true;break;
        }
    }
    if((screen==SETH_LESSON||screen==SETH_CHECK)&&!found){
        scratch->screen=SETH_HOME;scratch->alternate_explanation=false;
    }
    scratch->ecu_definition_matched=false;
    *dst=*scratch; /* Atomic assignment only after full integrity validation. */
    return true;
}
