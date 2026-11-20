/* BrickOS / Seth standalone helper prototype — NOT linked to firmware.
 * Pure C11, bounded memory. No networking, ECU bus, engine control or writes.
 */
#ifndef SETH_AGENT_H
#define SETH_AGENT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../esp_education_core.h"

#define SETH_STAGE_COUNT 10u
#define SETH_MAX_CHOICES 4u
#define SETH_MAX_SYMPTOM 160u

typedef struct {
    const char *id, *chapter, *objective;
    const char *quick, *guided, *lesson, *technical;
    const char *alternate, *check_question, *check_explanation;
    const char *options[3];
    unsigned correct_index;
    uint32_t version;
} seth_lesson_t;

/* A future build tool may put this generated const catalog in flash. */
extern const seth_lesson_t seth_lessons[];
extern const size_t seth_lesson_count;
extern const char *const seth_stage_ids[SETH_STAGE_COUNT];

typedef enum {SETH_HOME, SETH_TOUR, SETH_LESSON, SETH_CHECK, SETH_BUILD, SETH_DIAGNOSIS, SETH_REPORT} seth_screen_t;
typedef enum {SETH_INJ_UNKNOWN, SETH_INJ_PFI, SETH_INJ_GDI, SETH_INJ_MIXED} seth_injection_t;
typedef enum {SETH_PUMP_UNKNOWN, SETH_PUMP_RELAY, SETH_PUMP_PWM, SETH_PUMP_EXTERNAL} seth_pump_t;
typedef enum {SETH_REG_UNKNOWN, SETH_REG_MECHANICAL, SETH_REG_ELECTRONIC, SETH_REG_HYBRID, SETH_REG_NOT_CONFIRMED} seth_regulator_t;
typedef enum {SETH_SENSOR_UNKNOWN, SETH_SENSOR_ECU, SETH_SENSOR_EXTERNAL, SETH_SENSOR_BOTH, SETH_SENSOR_ABSENT} seth_pressure_sensor_t;
typedef enum {SETH_CASE_UNKNOWN, SETH_CASE_START, SETH_CASE_IDLE, SETH_CASE_FUEL, SETH_CASE_SYNC, SETH_CASE_SENSOR, SETH_CASE_IGNITION} seth_case_t;

typedef struct {
    seth_injection_t injection;
    seth_pump_t pump;
    seth_regulator_t regulator;
    seth_pressure_sensor_t pressure_sensor;
    uint32_t revision;
    /* All values are USER-REPORTED, never independently verified. */
} seth_build_t;

typedef struct {
    brick_edu_state_t education;
    seth_build_t build;
    seth_screen_t screen;
    unsigned stage;
    size_t lesson_index;
    bool alternate_explanation;
    seth_case_t diagnostic_case;
    char symptom[SETH_MAX_SYMPTOM];
    bool ecu_definition_matched; /* externally set; never set by a lesson */
    uint32_t session_revision;    /* local dialog actions only; not ECU authority */
} seth_agent_t;

typedef struct {
    const char *speaker, *title, *text, *detail, *question;
    const char *choices[SETH_MAX_CHOICES];
    unsigned choice_count;
    const char *state, *evidence, *next;
    unsigned stage;
    bool read_only, tuning_verified, gdi_unsupported;
} seth_packet_t;

/* All functions return state or human-readable packets. No ECU operations. */
void seth_init(seth_agent_t *s);
bool seth_preferences(seth_agent_t *s, brick_edu_depth_t depth, brick_edu_interaction_t style);
bool seth_open_tour(seth_agent_t *s);
bool seth_select_stage(seth_agent_t *s, unsigned stage);
bool seth_open_lesson(seth_agent_t *s, const char *concept_id);
bool seth_explain_alternate(seth_agent_t *s);
bool seth_start_check(seth_agent_t *s);
bool seth_answer_check(seth_agent_t *s, int selected_index); /* -1 skips */
bool seth_report_understanding(seth_agent_t *s);
void seth_open_build(seth_agent_t *s);
bool seth_build_answer(seth_agent_t *s, const char *field, const char *value);
void seth_ask_diagnosis(seth_agent_t *s, const char *user_text);
void seth_open_report(seth_agent_t *s);
void seth_render(const seth_agent_t *s, seth_packet_t *out);

#endif
