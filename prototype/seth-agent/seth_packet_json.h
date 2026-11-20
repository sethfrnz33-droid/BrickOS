/* Bounded portable JSON renderer for Seth packets. No heap or I/O. */
#ifndef SETH_PACKET_JSON_H
#define SETH_PACKET_JSON_H
#include "seth_agent.h"
#include <stddef.h>
/* Return UTF-8 bytes written (not including NUL), or 0 on error/overflow. */
size_t seth_packet_json(const seth_packet_t *packet,char *dest,size_t capacity);
#endif
