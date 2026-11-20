#include "seth_evidence.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static seth_sample_t sample(seth_signal_t role,float value,uint32_t when){
    seth_sample_t s={role,SETH_SOURCE_SYNTHETIC,when,value,true,true};
    return s;
}
static bool has(const seth_evidence_report_t *r,const char *code){
    for(unsigned i=0;i<r->count;i++)if(strcmp(r->findings[i].code,code)==0)return true;
    return false;
}
static void safe(const seth_evidence_report_t *r){
    assert(r->replay_only&&!r->may_adjust_ecu&&!r->physically_verified);
    assert(r->count<=SETH_EVIDENCE_MAX_FINDINGS);
    for(unsigned i=0;i<r->count;i++){
        assert(r->findings[i].explanation&&r->findings[i].missing);
        assert(!r->findings[i].diagnostic_cause_proven);
    }
}
static void test_empty_and_wrong_definition(void){
    seth_evidence_t e;seth_evidence_init(&e);
    seth_evidence_report_t r;
    seth_evidence_review(&e,0,false,&r);
    assert(has(&r,"NO_EVIDENCE"));safe(&r);
    seth_sample_t rpm=sample(SETH_SIGNAL_RPM,820.0f,20);
    assert(seth_evidence_add(&e,&rpm));
    seth_evidence_review(&e,10,false,&r);
    assert(has(&r,"DEFINITION_UNMATCHED"));safe(&r);
}
static void test_lambda_trims_and_readiness(void){
    seth_evidence_t e;seth_evidence_init(&e);
    seth_sample_t a=sample(SETH_SIGNAL_LAMBDA,1.00f,1000);
    seth_sample_t b=sample(SETH_SIGNAL_TARGET_LAMBDA,1.00f,1001);
    seth_sample_t c=sample(SETH_SIGNAL_WIDEBAND_READY,1.00f,1002);
    seth_sample_t d=sample(SETH_SIGNAL_STFT_DELTA,5.0f,1002);
    assert(seth_evidence_add(&e,&a));assert(seth_evidence_add(&e,&b));
    assert(seth_evidence_add(&e,&c));assert(seth_evidence_add(&e,&d));
    seth_evidence_report_t r;
    seth_evidence_review(&e,5,true,&r);
    assert(has(&r,"LAMBDA_CONTEXT_PRESENT"));
    assert(has(&r,"FEEDBACK_MAY_MASK_BASE"));safe(&r);
    seth_evidence_review(&e,0,true,&r);
    assert(has(&r,"LAMBDA_EVIDENCE_INCOMPLETE"));
    assert(!has(&r,"FEEDBACK_MAY_MASK_BASE"));
    c.value=0.0f;c.timestamp_ms=1005;
    assert(seth_evidence_add(&e,&c));
    seth_evidence_review(&e,10,true,&r);
    assert(has(&r,"LAMBDA_EVIDENCE_INCOMPLETE"));
    assert(!has(&r,"LAMBDA_CONTEXT_PRESENT"));
}
static void test_distinct_idle_sync_and_trigger_evidence(void){
    seth_evidence_t e;seth_evidence_init(&e);
    seth_sample_t a=sample(SETH_SIGNAL_RPM,780.0f,42);
    seth_sample_t b=sample(SETH_SIGNAL_IDLE_TARGET,850.0f,43);
    seth_sample_t c=sample(SETH_SIGNAL_SYNC_FLAG,0.0f,43);
    seth_sample_t d=sample(SETH_SIGNAL_TRIGGER_ERROR_COUNT,2.0f,41);
    seth_sample_t f=sample(SETH_SIGNAL_TRIGGER_ERROR_COUNT,3.0f,43);
    assert(seth_evidence_add(&e,&a));assert(seth_evidence_add(&e,&b));
    assert(seth_evidence_add(&e,&c));assert(seth_evidence_add(&e,&d));
    assert(seth_evidence_add(&e,&f));
    seth_evidence_report_t r;seth_evidence_review(&e,2,true,&r);
    assert(has(&r,"IDLE_TARGET_AND_ACTUAL_AVAILABLE"));
    assert(has(&r,"SYNC_REPORTED_ABSENT"));
    assert(has(&r,"TRIGGER_ERROR_COUNT_INCREASED"));
    safe(&r);
}
static void test_missing_and_overflow(void){
    seth_evidence_t e;seth_evidence_init(&e);
    seth_sample_t s=sample(SETH_SIGNAL_LAMBDA,0.95f,0);
    assert(seth_evidence_add(&e,&s));
    seth_evidence_report_t r;seth_evidence_review(&e,10,true,&r);
    assert(has(&r,"LAMBDA_EVIDENCE_INCOMPLETE"));
    s.valid=false;assert(seth_evidence_add(&e,&s));
    s=sample(SETH_SIGNAL_RPM,900.0f,0);
    s.role=SETH_SIGNAL_COUNT;assert(!seth_evidence_add(&e,&s));
    s.role=SETH_SIGNAL_RPM;s.value=NAN;assert(!seth_evidence_add(&e,&s));
    s.value=900.0f;
    for(unsigned i=0;i<SETH_EVIDENCE_MAX_SAMPLES+40;i++){
        s.timestamp_ms=i;
        assert(seth_evidence_add(&e,&s));
    }
    seth_evidence_review(&e,2,true,&r);
    assert(r.dropped>0);
    assert(has(&r,"CAPTURE_GAPS"));
    safe(&r);
}
int main(void){
    test_empty_and_wrong_definition();
    test_lambda_trims_and_readiness();
    test_distinct_idle_sync_and_trigger_evidence();
    test_missing_and_overflow();
    puts("Seth synthetic evidence reviews passed (no ECU/engine control)");
    return 0;
}
