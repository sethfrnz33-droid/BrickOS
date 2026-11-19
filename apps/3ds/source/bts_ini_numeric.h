#ifndef BTS_INI_NUMERIC_H
#define BTS_INI_NUMERIC_H
/* Fail-closed subset of TunerStudio numeric expressions, not a TS interpreter. */
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
enum {BTS_INI_METRIC,BTS_INI_LAMBDA,BTS_INI_FUEL_ALGO,BTS_INI_VE_OVERRIDE,
BTS_INI_IGN_OVERRIDE,BTS_INI_AFR_OVERRIDE,BTS_INI_FLAG_COUNT};
typedef struct {double values[BTS_INI_FLAG_COUNT];bool known[BTS_INI_FLAG_COUNT];} BtsIniFlags;
static const char *const bts_ini_flag_names[BTS_INI_FLAG_COUNT]={
"useMetricOnInterface","useLambdaOnInterface","fuelAlgorithm",
"veOverrideMode","ignOverrideMode","afrOverrideMode"};
typedef struct {const char *cursor;int depth;} BtsIniMath;
static void bts_ini_skip(BtsIniMath *m){while(isspace((unsigned char)*m->cursor))m->cursor++;}
static bool bts_ini_sum(BtsIniMath *m,double *out);
static bool bts_ini_atom(BtsIniMath *m,double *out){
 bts_ini_skip(m);if(m->depth>12)return false;
 if(*m->cursor=='+'||*m->cursor=='-'){
  char op=*m->cursor++;double value=0;
  if(!bts_ini_atom(m,&value))return false;
  *out=op=='-'?-value:value;return isfinite(*out);
 }
 if(*m->cursor=='('){
  m->cursor++;m->depth++;
  if(!bts_ini_sum(m,out))return false;
  bts_ini_skip(m);if(*m->cursor!=')')return false;
  m->cursor++;m->depth--;return isfinite(*out);
 }
 const char *before=m->cursor;char *after=NULL;double value=strtod(before,&after);
 if(after==before||!isfinite(value))return false;
 m->cursor=after;*out=value;return true;
}
static bool bts_ini_product(BtsIniMath *m,double *out){
 if(!bts_ini_atom(m,out))return false;
 for(;;){
  bts_ini_skip(m);char op=*m->cursor;
  if(op!='*'&&op!='/')return true;
  m->cursor++;double other=0;
  if(!bts_ini_atom(m,&other)||(op=='/'&&other==0))return false;
  *out=op=='*'?*out*other:*out/other;
  if(!isfinite(*out))return false;
 }
}
static bool bts_ini_sum(BtsIniMath *m,double *out){
 if(!bts_ini_product(m,out))return false;
 for(;;){
  bts_ini_skip(m);char op=*m->cursor;
  if(op!='+'&&op!='-')return true;
  m->cursor++;double other=0;
  if(!bts_ini_product(m,&other))return false;
  *out=op=='+'?*out+other:*out-other;
  if(!isfinite(*out))return false;
 }
}
static bool bts_ini_math(const char *s,double *out){
 if(!s||!out)return false;
 BtsIniMath m={s,0};
 if(!bts_ini_sum(&m,out))return false;
 bts_ini_skip(&m);return !*m.cursor&&isfinite(*out);
}
static bool bts_ini_predicate(const char *src,const BtsIniFlags *flags,bool *out){
 if(!src||!flags||!out)return false;
 char condition[180];size_t n=0;
 for(const char *p=src;*p;p++){
  if(isspace((unsigned char)*p))continue;
  if(n+1>=sizeof(condition))return false;
  condition[n++]=*p;
 }
 condition[n]=0;
 if(!strcmp(condition,"useMetricOnInterface")){
  if(!flags->known[BTS_INI_METRIC])return false;
  *out=flags->values[BTS_INI_METRIC]!=0;return true;
 }
 if(!strcmp(condition,"useLambdaOnInterface")){
  if(!flags->known[BTS_INI_LAMBDA])return false;
  *out=flags->values[BTS_INI_LAMBDA]!=0;return true;
 }
 const int ids[3]={BTS_INI_VE_OVERRIDE,BTS_INI_IGN_OVERRIDE,BTS_INI_AFR_OVERRIDE};
 const char *const names[3]={"veOverrideMode","ignOverrideMode","afrOverrideMode"};
 for(int i=0;i<3;i++){
  char test[180];
  snprintf(test,sizeof(test),"%s==1||(%s==0&&fuelAlgorithm==0)",names[i],names[i]);
  if(!strcmp(condition,test)){
   if(!flags->known[ids[i]]||!flags->known[BTS_INI_FUEL_ALGO])return false;
   double mode=flags->values[ids[i]],fuel=flags->values[BTS_INI_FUEL_ALGO];
   *out=mode==1||(mode==0&&fuel==0);return true;
  }
  snprintf(test,sizeof(test),"!(%s==1||(%s==0&&fuelAlgorithm==0))||useMetricOnInterface",names[i],names[i]);
  if(!strcmp(condition,test)){
   if(!flags->known[ids[i]]||!flags->known[BTS_INI_FUEL_ALGO]||!flags->known[BTS_INI_METRIC])return false;
   double mode=flags->values[ids[i]],fuel=flags->values[BTS_INI_FUEL_ALGO];
   *out=!(mode==1||(mode==0&&fuel==0))||flags->values[BTS_INI_METRIC]!=0;return true;
  }
 }
 return false;
}
static bool bts_ini_numeric_impl(const char *src,const BtsIniFlags *flags,double *out,int calls){
 if(!src||!out||calls>8)return false;
 char value[200];size_t len=strlen(src);
 if(len>=sizeof(value))return false;
 memcpy(value,src,len+1);
 char *start=value;while(isspace((unsigned char)*start))start++;
 char *end=start+strlen(start);
 while(end>start&&isspace((unsigned char)end[-1]))*--end=0;
 if(*start=='{'&&end>start+1&&end[-1]=='}'){start++;*--end=0;}
 while(*start=='('&&end>start+1&&end[-1]==')'){
  int nest=0;bool whole=true;
  for(char *p=start;p<end;p++){
   if(*p=='(')nest++;
   else if(*p==')'){
    if(--nest<0){whole=false;break;}
    if(nest==0&&p!=end-1){whole=false;break;}
   }
  }
  if(!whole||nest!=0)break;
  start++;*--end=0;
 }
 char *question=NULL,*colon=NULL;int nest=0;
 for(char *p=start;*p;p++){
  if(*p=='(')nest++;
  else if(*p==')'){if(--nest<0)return false;}
  else if(nest==0&&*p=='?'){
   if(question)return false;question=p;
  }else if(nest==0&&*p==':'){
   if(colon)return false;colon=p;
  }
 }
 if(nest!=0)return false;
 if(!question&&!colon)return bts_ini_math(start,out);
 if(!question||!colon||colon<question)return false;
 *question=0;*colon=0;bool yes=false;
 if(!bts_ini_predicate(start,flags,&yes))return false;
 return bts_ini_numeric_impl(yes?question+1:colon+1,flags,out,calls+1);
}
static bool bts_ini_numeric(const char *s,const BtsIniFlags *flags,double *out){
 return bts_ini_numeric_impl(s,flags,out,0);
}
static bool bts_ini_unit(const char *src,const BtsIniFlags *flags,char *out,size_t cap){
 if(!src||!flags||!out||cap<8||!flags->known[BTS_INI_METRIC])return false;
 bool metric=flags->values[BTS_INI_METRIC]!=0;
 if(strstr(src,"bitStringValue(unitsLabels, useMetricOnInterface)"))
  snprintf(out,cap,"%s",metric?"C":"F");
 else if(strstr(src,"bitStringValue(pressureUnitsLabels, useMetricOnInterface)"))
  snprintf(out,cap,"%s",metric?"kPa":"psi");
 else if(strstr(src,"bitStringValue(velocityUnitsLabels, useMetricOnInterface)"))
  snprintf(out,cap,"%s",metric?"km/h":"mph");
 else if(strstr(src,"useLambdaOnInterface ? \"lambda\" : \"AFR (gasoline scale)\"")){
  if(!flags->known[BTS_INI_LAMBDA])return false;
  snprintf(out,cap,"%s",flags->values[BTS_INI_LAMBDA]!=0?"lambda":"AFR");
 }else return false;
 return true;
}
#endif
