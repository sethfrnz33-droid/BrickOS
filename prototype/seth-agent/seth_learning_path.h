/* Prerequisite-aware teaching, preserving the originally requested topic.
 * No mandatory quiz/pass gate: introduced concepts may lead to later lessons.
 */
#ifndef SETH_LEARNING_PATH_H
#define SETH_LEARNING_PATH_H
#include "seth_agent.h"
bool seth_request_topic(seth_agent_t *s,const char *concept_id);
bool seth_continue_topic(seth_agent_t *s);
#endif
