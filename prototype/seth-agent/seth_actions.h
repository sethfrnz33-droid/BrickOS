/* Standalone UI action router. Allowed actions never imply ECU authority. */
#ifndef SETH_ACTIONS_H
#define SETH_ACTIONS_H
#include "seth_agent.h"
#include <stdint.h>
/* Only accepted actions advance session_revision.
 * Old UI actions must not silently apply to a newer Seth dialogue.
 * This is a synchronization safeguard, NOT network authentication.
 */
bool seth_dispatch(seth_agent_t *agent,uint32_t expected_revision,
                   const char *action,const char *value);
#endif
