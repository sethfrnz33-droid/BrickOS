/* Portable, bounded and checksummed Seth session checkpoint.
 * Pure byte encoder/decoder. It does NOT access flash, SD, filesystem or ECU.
 */
#ifndef SETH_CHECKPOINT_H
#define SETH_CHECKPOINT_H
#include "seth_agent.h"
#include <stddef.h>
#include <stdint.h>
#define SETH_CHECKPOINT_MAX_BYTES 8192u
/* Write versioned little-endian record to user-owned buffer; 0 on failure. */
size_t seth_checkpoint_encode(const seth_agent_t *s,uint8_t *out,size_t cap);
/* Decode atomically using a separate, caller-owned scratch seth_agent_t.
 * A failed restore leaves *destination untouched. Distinct nonoverlapping
 * destination, scratch and input buffers are required.
 * Neither live ECU identity nor any authentication/control authority survives.
 */
bool seth_checkpoint_restore(seth_agent_t *destination,seth_agent_t *scratch,
                             const uint8_t *bytes,size_t len);
#endif
