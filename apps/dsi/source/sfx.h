#ifndef BTS_DSI_SFX_H
#define BTS_DSI_SFX_H
#include <stdbool.h>
/* Cue IDs agree with bts_audio_rules.h from the 3DS edition. */
bool bts_sfx_play(int cue,bool sd_ready);
bool bts_sfx_files_ready(bool sd_ready);
const char *bts_sfx_status(bool sd_ready);
void bts_sfx_shutdown(void);
#endif
