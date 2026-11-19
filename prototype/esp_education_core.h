/* BrickOS pure C education state prototype.
 * Designed to be portable to ESP-IDF; NOT linked to runtime firmware.
 * No ECU I/O, no calibration writes, no heap allocation, no user identity.
 */
#ifndef BRICK_EDUCATION_CORE_H
#define BRICK_EDUCATION_CORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BRICK_EDU_MAX_CONCEPTS 48u
#define BRICK_EDU_ID_CAP 48u
typedef enum { BRICK_EDU_QUICK, BRICK_EDU_GUIDED, BRICK_EDU_LESSON, BRICK_EDU_TECHNICAL } brick_edu_depth_t;
typedef enum { BRICK_EDU_RECOMMEND, BRICK_EDU_OPTIONAL_CHECKS, BRICK_EDU_TEACH_AS_YOU_GO, BRICK_EDU_COURSE_MODE } brick_edu_interaction_t;
typedef enum { BRICK_EDU_UNSEEN, BRICK_EDU_INTRODUCED, BRICK_EDU_NEEDS_HELP, BRICK_EDU_DEMONSTRATED } brick_edu_understanding_t;
typedef enum { BRICK_EDU_CHECK_CORRECT, BRICK_EDU_CHECK_INCORRECT, BRICK_EDU_CHECK_SKIPPED } brick_edu_check_t;
typedef struct {
    char concept_id[BRICK_EDU_ID_CAP];
    uint32_t content_version_hash;
    uint16_t check_attempts;
    uint8_t understanding;
    uint8_t requested_help;
    bool needs_revisit;
    bool lesson_seen;
} brick_edu_concept_t;
typedef struct {
    uint32_t schema_version;
    brick_edu_depth_t depth;
    brick_edu_interaction_t interaction;
    uint16_t concept_count;
    uint32_t event_sequence;
    brick_edu_concept_t concepts[BRICK_EDU_MAX_CONCEPTS];
} brick_edu_state_t;
void brick_edu_init(brick_edu_state_t *state);
bool brick_edu_preferences(brick_edu_state_t *state, brick_edu_depth_t depth,
                           brick_edu_interaction_t interaction);
brick_edu_concept_t *brick_edu_find(brick_edu_state_t *state, const char *concept_id);
bool brick_edu_open(brick_edu_state_t *state,const char *concept_id,uint32_t content_hash);
bool brick_edu_confused(brick_edu_state_t *state,const char *concept_id);
bool brick_edu_self_report_understood(brick_edu_state_t *state,const char *concept_id);
bool brick_edu_check(brick_edu_state_t *state,const char *concept_id,brick_edu_check_t result);
bool brick_edu_revisit_due_to_content_change(brick_edu_state_t *state,
                                              const char *concept_id,uint32_t new_hash);
/* Deliberately NO engine-readiness, tuning-authority, or ECU command functions. */
#endif
