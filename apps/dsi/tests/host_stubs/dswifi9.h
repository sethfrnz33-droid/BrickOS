#ifndef HOST_DSWIFI9_H
#define HOST_DSWIFI9_H
#include <stdbool.h>
#include <netinet/in.h>
#define ASSOCSTATUS_DISCONNECTED 0
#define ASSOCSTATUS_SEARCHING 1
#define ASSOCSTATUS_ASSOCIATING 2
#define ASSOCSTATUS_ACQUIRINGDHCP 3
#define ASSOCSTATUS_ASSOCIATED 4
#define INIT_ONLY false
static inline bool Wifi_InitDefault(bool x){(void)x;return true;}
static inline void Wifi_AutoConnect(void){}
static inline void Wifi_DisconnectAP(void){}
static inline int Wifi_AssocStatus(void){return ASSOCSTATUS_DISCONNECTED;}
static inline struct in_addr Wifi_GetIPInfo(struct in_addr *a,struct in_addr *b,struct in_addr*c,struct in_addr*d){
    struct in_addr z={0};*a=*b=*c=*d=z;return z;
}
#endif
