#ifndef BTS_DSI_CLUTCH_LEGO_RULES_H
#define BTS_DSI_CLUTCH_LEGO_RULES_H
#include <stdbool.h>
#include <stdint.h>
/* Pure, testable state-edge detector. One cue per confirmed transition,
 * including both press and release. Never play for the first sample or after
 * a gap in live ECU data. A repeated sequence number is not a new sample. */
#define BTS_CLUTCH_LEGO_SOUNDS 7
typedef struct {
    bool known, have_sequence;
    int stable, pending, pending_count;
    uint32_t last_sequence;
    unsigned next_sound;
} BtClutchLego;
static inline int bt_clutch_lego_step(BtClutchLego *s, bool valid, int signal,
                               uint32_t sequence){
    if(!s)return -1;
    if(!valid || (signal!=0 && signal!=1)){
        s->known=false;s->have_sequence=false;
        s->pending=-1;s->pending_count=0;
        return -1;
    }
    if(s->have_sequence && sequence==s->last_sequence)return -1;
    s->have_sequence=true;s->last_sequence=sequence;
    if(!s->known){
        s->known=true;s->stable=signal;
        s->pending=-1;s->pending_count=0;
        return -1;
    }
    if(signal==s->stable){s->pending=-1;s->pending_count=0;return -1;}
    if(s->pending!=signal){s->pending=signal;s->pending_count=1;return -1;}
    if(++s->pending_count<2)return -1;
    s->stable=signal;s->pending=-1;s->pending_count=0;
    unsigned result=s->next_sound;
    s->next_sound=(s->next_sound+1)%BTS_CLUTCH_LEGO_SOUNDS;
    return (int)result;
}
#endif
