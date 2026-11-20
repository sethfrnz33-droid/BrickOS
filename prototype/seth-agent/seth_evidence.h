/* Offline, bounded evidence reviewer. No sensor/USB/CAN access or tune edits.
 * All analysis describes supplied samples, not physical truth or safety.
 */
#ifndef SETH_EVIDENCE_H
#define SETH_EVIDENCE_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define SETH_EVIDENCE_MAX_SAMPLES 128u
#define SETH_EVIDENCE_MAX_FINDINGS 8u
typedef enum {
    SETH_SIGNAL_RPM, SETH_SIGNAL_IDLE_TARGET,
    SETH_SIGNAL_LAMBDA, SETH_SIGNAL_TARGET_LAMBDA,
    SETH_SIGNAL_WIDEBAND_READY, SETH_SIGNAL_STFT_DELTA,
    SETH_SIGNAL_LTFT_DELTA, SETH_SIGNAL_SYNC_FLAG,
    SETH_SIGNAL_TRIGGER_ERROR_COUNT, SETH_SIGNAL_FUEL_PUMP_COMMAND,
    SETH_SIGNAL_LOW_FUEL_PRESSURE,
    SETH_SIGNAL_COUNT
} seth_signal_t;
typedef enum {SETH_SOURCE_SYNTHETIC, SETH_SOURCE_USER_REPORTED,
              SETH_SOURCE_DECODED_INI, SETH_SOURCE_EXTERNAL_RECORD} seth_source_t;
typedef struct {
    seth_signal_t role;
    seth_source_t source;
    uint32_t timestamp_ms;
    float value;
    bool valid;
    bool role_semantics_verified; /* caller attestation, not hardware proof */
} seth_sample_t;
typedef struct {
    seth_sample_t samples[SETH_EVIDENCE_MAX_SAMPLES];
    uint16_t count, next;
    uint32_t received, discarded;
} seth_evidence_t;
typedef struct {
    const char *code, *explanation, *missing;
    bool diagnostic_cause_proven; /* always false in offline prototype */
} seth_finding_t;
typedef struct {
    uint8_t count;
    seth_finding_t findings[SETH_EVIDENCE_MAX_FINDINGS];
    uint32_t samples, dropped;
    bool replay_only, may_adjust_ecu, physically_verified;
} seth_evidence_report_t;
void seth_evidence_init(seth_evidence_t *e);
bool seth_evidence_add(seth_evidence_t *e,const seth_sample_t *sample);
void seth_evidence_review(const seth_evidence_t *e,uint32_t max_cross_channel_skew_ms,
                          bool definition_matched,seth_evidence_report_t *out);
#endif
