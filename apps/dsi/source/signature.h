#ifndef BTS_DSI_SIGNATURE_H
#define BTS_DSI_SIGNATURE_H
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

/* rusEFI's configuration hash is the final decimal run.  The label, branch,
 * date, board name and INI filename prefix are not layout identifiers.
 * Accept surrounding INI quotes/whitespace and optional .ini suffixes, but
 * require an actual 6..10 digit layout hash; do not match truncated digits.
 */
static inline bool bt_signature_hash_copy(const char *signature, char out[11]){
    if(out)out[0]=0;
    if(!signature||!out)return false;
    const char *end=signature+strlen(signature);
    while(end>signature && (isspace((unsigned char)end[-1])||end[-1]=='"'||end[-1]=='\''))--end;
    if(end-signature>=4 && end[-4]=='.' &&
       (end[-3]=='i'||end[-3]=='I') && (end[-2]=='n'||end[-2]=='N') &&
       (end[-1]=='i'||end[-1]=='I')){
        end-=4;
        while(end>signature && isspace((unsigned char)end[-1]))--end;
    }
    const char *start=end;
    while(start>signature && start[-1]>='0' && start[-1]<='9')--start;
    size_t n=(size_t)(end-start);
    if(n<6 || n>10)return false;
    memcpy(out,start,n);out[n]=0;
    return true;
}
static inline bool bt_signature_matches(const char *ecu,const char *ini){
    if(!ecu||!ini||!*ecu||!*ini)return false;
    char e[11],i[11];
    bool eh=bt_signature_hash_copy(ecu,e),ih=bt_signature_hash_copy(ini,i);
    if(eh&&ih)return strcmp(e,i)==0;
    /* Legacy non-hash signatures: exact identity only, never prefix matching. */
    return !eh&&!ih&&!strcmp(ecu,ini);
}
#endif
