#pragma once
#include <3ds.h>
static inline Thread threadCreate(void (*f)(void*),void*a,size_t n,int priority,int core,bool detached){(void)f;(void)a;(void)n;(void)priority;(void)core;(void)detached;return NULL;}
