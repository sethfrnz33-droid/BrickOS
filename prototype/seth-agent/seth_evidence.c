#include "seth_evidence.h"
#include <string.h>
#include <math.h>

void seth_evidence_init(seth_evidence_t *e){
    if(e)memset(e,0,sizeof(*e));
}
bool seth_evidence_add(seth_evidence_t *e,const seth_sample_t *s){
    if(!e||!s||s->role<SETH_SIGNAL_RPM||s->role>=SETH_SIGNAL_COUNT||
       s->source<SETH_SOURCE_SYNTHETIC||s->source>SETH_SOURCE_EXTERNAL_RECORD||
       !isfinite(s->value))return false;
    if(e->count==SETH_EVIDENCE_MAX_SAMPLES)e->discarded++;
    else e->count++;
    e->samples[e->next]=*s;
    e->next=(uint16_t)((e->next+1)%SETH_EVIDENCE_MAX_SAMPLES);
    e->received++;
    return true;
}
static void add(seth_evidence_report_t *r,const char *code,const char *explanation,const char *missing){
    if(r->count>=SETH_EVIDENCE_MAX_FINDINGS)return;
    seth_finding_t *f=&r->findings[r->count++];
    f->code=code;f->explanation=explanation;f->missing=missing;
    f->diagnostic_cause_proven=false;
}
static bool comparable(const seth_sample_t *a,const seth_sample_t *b,uint32_t max_skew){
    if(!a||!b||!a->valid||!b->valid||
       !a->role_semantics_verified||!b->role_semantics_verified)return false;
    uint32_t delta=a->timestamp_ms>b->timestamp_ms?
        a->timestamp_ms-b->timestamp_ms:b->timestamp_ms-a->timestamp_ms;
    return delta<=max_skew;
}
void seth_evidence_review(const seth_evidence_t *e,uint32_t tolerance,
                          bool definition_matched,seth_evidence_report_t *out){
    if(!out)return;
    memset(out,0,sizeof(*out));
    out->replay_only=true;
    out->may_adjust_ecu=false;out->physically_verified=false;
    if(!e||!e->count){add(out,"NO_EVIDENCE","No samples were supplied for review.","Recorded evidence");return;}
    out->samples=e->received;out->dropped=e->discarded;
    if(!definition_matched){
        add(out,"DEFINITION_UNMATCHED","Decoded values cannot be interpreted without a verified matching ECU definition.","Exact ECU/INI identity");
        return;
    }
    const seth_sample_t *last[SETH_SIGNAL_COUNT]={0};
    const seth_sample_t *older_trigger=NULL;
    for(size_t i=0;i<e->count;i++){
        const seth_sample_t *s=&e->samples[i];
        if(!s->valid||!s->role_semantics_verified)continue;
        if(!last[s->role]||s->timestamp_ms>=last[s->role]->timestamp_ms){
            if(s->role==SETH_SIGNAL_TRIGGER_ERROR_COUNT&&last[s->role])
                older_trigger=last[s->role];
            last[s->role]=s;
        }
    }
    if(e->discarded)
        add(out,"CAPTURE_GAPS","The fixed-size review buffer discarded older samples. Absence of earlier evidence cannot be interpreted as absence of a problem.","Original complete recorded event");
    const seth_sample_t *lambda=last[SETH_SIGNAL_LAMBDA],
                        *target=last[SETH_SIGNAL_TARGET_LAMBDA],
                        *ready=last[SETH_SIGNAL_WIDEBAND_READY],
                        *stft=last[SETH_SIGNAL_STFT_DELTA],
                        *ltft=last[SETH_SIGNAL_LTFT_DELTA];
    if(lambda||target){
        if(!lambda||!target||!ready||ready->value<0.5f||
           !comparable(lambda,ready,tolerance)||!comparable(lambda,target,tolerance))
            add(out,"LAMBDA_EVIDENCE_INCOMPLETE","Reported lambda, target or sensor readiness are missing, unaligned, or not ready. No mixture conclusion is justified.","Fresh synchronized measurement, active target and readiness");
        else{
            add(out,"LAMBDA_CONTEXT_PRESENT","Measured and requested lambda are available in comparable records; this does not establish base fuel-map accuracy or calibration safety.","Active corrections, sensor calibration, fuel model and independent validation");
            if((stft&&comparable(lambda,stft,tolerance)&&stft->value!=0.0f)||
               (ltft&&comparable(lambda,ltft,tolerance)&&ltft->value!=0.0f))
                add(out,"FEEDBACK_MAY_MASK_BASE","A supplied fuel-trim channel is non-neutral in this observation. An on-target lambda value alone cannot validate the base VE model.","Verified trim scaling, active correction flags and fuel reference");
        }
    }
    if(comparable(last[SETH_SIGNAL_RPM],last[SETH_SIGNAL_IDLE_TARGET],tolerance))
        add(out,"IDLE_TARGET_AND_ACTUAL_AVAILABLE","Reported engine speed and idle target were captured close enough to compare by the caller's timestamp criterion; no fault threshold has been assumed.","Active idle state and actuator/load context");
    const seth_sample_t *sync=last[SETH_SIGNAL_SYNC_FLAG];
    if(sync&&sync->value<0.5f)
        add(out,"SYNC_REPORTED_ABSENT","The supplied ECU synchronization flag was not asserted in this observation. This does not identify a physical component fault.","Operating state and supported trigger evidence");
    const seth_sample_t *trigger=last[SETH_SIGNAL_TRIGGER_ERROR_COUNT];
    if(trigger&&older_trigger&&trigger->value>older_trigger->value)
        add(out,"TRIGGER_ERROR_COUNT_INCREASED","A supplied error counter increased in this record. A counter alone cannot reproduce or diagnose individual trigger edges.","Qualified special trigger trace, source timestamps and context");
    if(!out->count)
        add(out,"NO_COMPARABLE_PATTERN","The available data did not establish a supported interpretation.","Verified simultaneous channels and state context");
}
