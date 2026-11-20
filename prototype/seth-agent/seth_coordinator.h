/* Seth staged planner: source-independent educational task ownership.
 * No ECU IO, no calibration command primitives, no hardware authority.
 */
#ifndef SETH_COORDINATOR_H
#define SETH_COORDINATOR_H
#include "seth_agent.h"
typedef enum {
    SETH_TASK_LEARN,
    SETH_TASK_BUILD_QUESTION,
    SETH_TASK_REVIEW_EXISTING_EVIDENCE,
    SETH_TASK_UNSUPPORTED,
    SETH_TASK_EXTERNAL_REVIEW
} seth_task_kind_t;
typedef struct {
    seth_task_kind_t kind;
    unsigned stage;
    const char *task_id;
    const char *title;
    const char *reason;
    const char *next_lesson_id;
    const char *question_field;
    const char *evidence_needed;
    const char *external_check;
    bool can_open_lesson;
    bool verified;           /* always false in this prototype */
    bool may_control_ecu;    /* always false in this prototype */
} seth_task_plan_t;
typedef struct {
    unsigned seen_concepts;
    unsigned demonstrated_concepts;
    unsigned needs_help;
    unsigned total_authored_concepts;
    unsigned chapters_with_lessons;
    unsigned externally_verified_chapters; /* 0: this module has no external evidence */
} seth_course_summary_t;
bool seth_plan_stage(const seth_agent_t *agent, unsigned stage, seth_task_plan_t *out);
bool seth_plan_current(const seth_agent_t *agent, seth_task_plan_t *out);
bool seth_follow_plan(seth_agent_t *agent, const seth_task_plan_t *plan);
void seth_course_summary(const seth_agent_t *agent, seth_course_summary_t *out);
#endif
