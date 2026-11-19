#ifndef TEST_DSWIFI_H
#define TEST_DSWIFI_H
#include <stdbool.h>
#include <netinet/in.h>
#define INIT_ONLY false
#define WFC_CONNECT true
#define ASSOCSTATUS_DISCONNECTED 0
#define ASSOCSTATUS_SEARCHING 1
#define ASSOCSTATUS_ASSOCIATING 2
#define ASSOCSTATUS_ACQUIRINGDHCP 3
#define ASSOCSTATUS_ASSOCIATED 4
static inline bool Wifi_InitDefault(bool b){(void)b;return true;}
static inline int Wifi_AssocStatus(void){return 4;}
static inline void Wifi_AutoConnect(void){}
static inline int Wifi_DisconnectAP(void){return 0;}
static inline struct in_addr Wifi_GetIPInfo(struct in_addr*g,struct in_addr*m,struct in_addr*d1,struct in_addr*d2){struct in_addr z={0};*g=*m=*d1=*d2=z;return z;}
#endif
