#ifndef BTS_AUDIO_RULES_H
#define BTS_AUDIO_RULES_H
/* Pure transition rules; no ECU commands, no audio driver dependencies. */
#include <stdbool.h>
#include <stddef.h>
#include <ctype.h>

enum {
 BTS_SOUND_LAUNCH=0, BTS_SOUND_VVL, BTS_SOUND_GONG, BTS_SOUND_CONFIG_ERROR,
 BTS_SOUND_PRESSURE, BTS_SOUND_FIREWORK, BTS_SOUND_UNSAVED,
 BTS_SOUND_BURN_START, BTS_SOUND_BURN_OK, BTS_SOUND_CONNECTED,
 BTS_SOUND_COOLANT_HOT, BTS_SOUND_LAMBDA_DISPLAY, BTS_SOUND_UNDO,
 BTS_SOUND_CYLINDER_OVER6, BTS_SOUND_COUNT
};
static const char *const bts_sound_files[BTS_SOUND_COUNT]={
 "launch.wav", "vvl.wav", "four_cyl_honda.wav", "config_error.wav",
 "boost_nitrous.wav", "limiter_antilag.wav", "unsaved.wav",
 "burning.wav", "burn_verified.wav", "connected.wav", "coolant_hot.wav", "lambda_display.wav",
 "undo.wav", "more_than_six_cylinders.wav"
};
static bool bts_contains_ci(const char *haystack,const char *needle){
 if(!haystack||!needle||!*needle)return false;
 for(const unsigned char *h=(const unsigned char*)haystack;*h;h++){
  const unsigned char *a=h,*b=(const unsigned char*)needle;
  while(*a&&*b&&tolower(*a)==tolower(*b)){a++;b++;}
  if(!*b)return true;
 }
 return false;
}
static bool bts_is_cylinder_count_field(const char *field){
 return (bts_contains_ci(field,"cylinder")||bts_contains_ci(field,"cylinders")) &&
        (bts_contains_ci(field,"count")||bts_contains_ci(field,"number")||bts_contains_ci(field,"num"));
}
static int bts_sound_for_entry(const char *label,const char *target){
 if(bts_contains_ci(label,"lambda")||bts_contains_ci(target,"lambda"))
    return BTS_SOUND_LAMBDA_DISPLAY;
 if(bts_contains_ci(label,"cylinder")||bts_contains_ci(target,"cylinder")||
    bts_contains_ci(label,"cyl ")||bts_contains_ci(target,"cyl "))
    return BTS_SOUND_CYLINDER_OVER6;
 if(bts_contains_ci(label,"Launch Control")||bts_contains_ci(target,"launchControl"))return BTS_SOUND_LAUNCH;
 if(bts_contains_ci(label,"VVL Control")||bts_contains_ci(target,"vvl"))return BTS_SOUND_VVL;
 if(bts_contains_ci(label,"Nitrous")||bts_contains_ci(target,"nitrous"))return BTS_SOUND_PRESSURE;
 if(bts_contains_ci(label,"Rotational Idle")||bts_contains_ci(target,"rotationalIdle"))return BTS_SOUND_PRESSURE;
 if(bts_contains_ci(label,"Anti-Lag")||bts_contains_ci(label,"Antilag")||bts_contains_ci(target,"antilag"))return BTS_SOUND_FIREWORK;
 if(bts_contains_ci(label,"Limiter")||bts_contains_ci(target,"rpmLimiter"))return BTS_SOUND_FIREWORK;
 return -1;
}
static int bts_sound_for_choice(const char *field,const char *label,int value){
 if(bts_is_cylinder_count_field(field) && value==4)
     return BTS_SOUND_GONG;
 if(bts_contains_ci(field,"trigger")&&bts_contains_ci(label,"honda"))return BTS_SOUND_GONG;
 return -1;
}
/* Value changes are the events, not redraws. A high cylinder count plays
 * once when crossing from <=6 to >6, not again for 7->8 adjustments. */
static int bts_sound_for_change(const char *field,const char *label,double old_value,double new_value){
 if(old_value==new_value)return -1;
 /* 4-cylinder and Honda pattern retain their separate explicit cue. */
 int special=bts_sound_for_choice(field,label,(int)new_value);
 if(special>=0)return special;
 return bts_sound_for_entry(field,label);
}
/* An unavailable sample never arms a level-triggered cue. The caller supplies
   fresh, INI-decoded telemetry only; stale data must not operate alerts. */
typedef struct {bool high;unsigned long long last_ms;} BtsAudioLevel;
static bool bts_level_cross(BtsAudioLevel *st,double value,double limit,double clear_below){
 if(!st)return false;
 if(value<=clear_below){st->high=false;return false;}
 if(value>limit && !st->high){st->high=true;return true;}
 return false;
}
static bool bts_level_repeat(BtsAudioLevel *st,double value,double limit,
                              unsigned long long now,unsigned long long interval){
 if(!st)return false;
 if(value<=limit){st->high=false;st->last_ms=0;return false;}
 if(!st->high){st->high=true;st->last_ms=now;return true;}
 if(now-st->last_ms>=interval){st->last_ms=now;return true;}
 return false;
}
#endif
