#include "tuner.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) ++s;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = 0;
    return s;
}
static void cp(char *dst,size_t cap,const char *src) {
    if (!cap) return;
    size_t n = src ? strlen(src) : 0;
    if (n >= cap) n = cap-1;
    if (n) memcpy(dst,src,n);
    dst[n]=0;
}
static int csv(const char *s,char a[][128],int max) {
    int n=0, depth=0, quote=0, at=0;
    if (!s||max<1) return 0;
    memset(a,0,(size_t)max*128);
    for (;;) {
        int c=(unsigned char)*s;
        if (c=='"') quote=!quote;
        if (!quote && c=='[') ++depth;
        if (!quote && c==']' && depth>0) --depth;
        if (!c || (c==','&&!quote&&!depth)) {
            if (n>=max) break;
            char *v=trim(a[n]);
            if (v!=a[n]) memmove(a[n],v,strlen(v)+1);
            size_t len=strlen(a[n]);
            if (len>1&&a[n][0]=='"'&&a[n][len-1]=='"') {
                memmove(a[n],a[n]+1,len-2);a[n][len-2]=0;
            }
            ++n;at=0;
            if (!c||n==max) break;
        } else if (n<max && at<127) a[n][at++]=(char)c;
        ++s;
    }
    return n;
}
static bool num(const char*s,double *v) {
    if (!s||!*s) return false;
    char *end=NULL;
    double x=strtod(s,&end);
    while (end&&isspace((unsigned char)*end))++end;
    if(end==s||!end||*end||!isfinite(x))return false;
    *v=x;return true;
}
static int positive_int(const char *s,int otherwise) {
    double x;return num(s,&x)&&x>=0&&x<=65535&&floor(x)==x?(int)x:otherwise;
}
int bt_type_size(BtType t) {
    if(t==BT_U08||t==BT_S08)return 1;
    if(t==BT_U16||t==BT_S16)return 2;
    if(t==BT_U32||t==BT_S32||t==BT_F32)return 4;
    if(t==BT_F64)return 8;
    return 0;
}
static BtType dtype(const char *s) {
    static const char *names[]={"U08","S08","U16","S16","U32","S32","F32","F64"};
    for(int i=0;i<8;++i)if(!strcasecmp(s,names[i]))return (BtType)i;
    return BT_UNKNOWN;
}
/* DSi DS-mode memory is scarce: allocate metadata only for entries actually in the INI.
   The maximum capacities remain unchanged, unlike the oversized static v0.6 BSS. */
static bool grow(BtDefinition *d,void **ptr,int *capacity,int needed,size_t item_size,int hard_max,int step){
    if(needed<=*capacity)return true;
    int next=*capacity;
    while(next<needed){
        if(next>hard_max-step)next=hard_max;
        else next+=step;
        if(next<needed && next==hard_max){d->truncated=true;return false;}
    }
    void *p=realloc(*ptr,(size_t)next*item_size);
    if(!p){d->out_of_memory=true;d->truncated=true;return false;}
    *ptr=p;*capacity=next;return true;
}
static void parse_field(BtDefinition*d,const char *key,const char *value,bool runtime) {
    BtField *f;
    if(runtime){
        if(d->channel_count>=BTS_MAX_CHANNELS||!grow(d,(void**)&d->channels,&d->channel_capacity,d->channel_count+1,sizeof(BtField),BTS_MAX_CHANNELS,32)){d->truncated=true;return;}
        f=&d->channels[d->channel_count];
    }else{
        if(d->constant_count>=BTS_MAX_CONSTANTS||!grow(d,(void**)&d->constants,&d->constant_capacity,d->constant_count+1,sizeof(BtField),BTS_MAX_CONSTANTS,32)){d->truncated=true;return;}
        f=&d->constants[d->constant_count];
    }
    char cols[11][128];int n=csv(value,cols,11);
    if(n<3)return;
    int kind=!strcasecmp(cols[0],"scalar")?0:!strcasecmp(cols[0],"array")?1:!strcasecmp(cols[0],"bits")?2:-1;
    BtType type=dtype(cols[1]);int off=positive_int(cols[2],-1);
    if(kind<0||type==BT_UNKNOWN||off<0)return;
    memset(f,0,sizeof(*f));cp(f->name,sizeof(f->name),key);
    f->kind=kind;f->type=type;f->offset=off;f->page=runtime?0:d->current_page;
    f->width=f->height=1;
    int p=3;
    if(kind==1){int w=0,h=1;if(sscanf(cols[3],"[%dx%d]",&w,&h)!=2&&sscanf(cols[3],"[%d]",&w)!=1)w=0;
        if(w<1||h<1||w*h>2048)return;
        f->width=w;f->height=h;p=4;
    }else if(kind==2){p=4;int lo=-1,hi=-1;
        if(sscanf(cols[3],"[%d:%d]",&lo,&hi)!=2){if(sscanf(cols[3],"[%d]",&lo)==1)hi=lo;}
        f->bit_lo=lo;f->bit_hi=hi;f->scaled=lo>=0&&hi>=lo&&hi<bt_type_size(type)*8&&hi-lo<32;
        f->scale=1.0;
    }
    if(kind!=2){
        if(p<n)cp(f->unit,sizeof(f->unit),cols[p]);
        f->scaled=p+2<n&&num(cols[p+1],&f->scale)&&num(cols[p+2],&f->translate)&&f->scale!=0.0;
        if(p+4<n&&num(cols[p+3],&f->low)&&num(cols[p+4],&f->high)&&f->low<=f->high)f->limits_known=true;
    }
    if(runtime)++d->channel_count;else ++d->constant_count;
}
/* DS ARM9 stack is tiny (DTCM). A 64 x 128-byte CSV array used to be
 * allocated inside each of these functions and could overflow the stack
 * when the INI reached a pageIdentifier/pageSize/pageChunkWrite line.
 * The parser is single-threaded, so one shared static buffer is safe. */
static char page_csv_scratch[64][128];
static void page_ids(BtDefinition*d,const char*value) {
    char (*a)[128]=page_csv_scratch;int n=csv(value,a,64);
    for(int i=0;i<n;i++){
        unsigned lo=0,hi=0;
        char *x=strstr(a[i],"\\x");
        d->page_ids[i]=x&&sscanf(x,"\\x%2x\\x%2x",&lo,&hi)==2?(int)(lo|(hi<<8)):positive_int(a[i],-1);
    }
    d->page_count=n;
}
static void page_cap(const char*value,bool*dest,const char*required){
    char (*a)[128]=page_csv_scratch;int n=csv(value,a,64);
    for(int i=0;i<64;i++)dest[i]=i<n&&!strcasecmp(a[i],required);
}
static void page_sizes(BtDefinition*d,const char*value){
    char (*a)[128]=page_csv_scratch;int n=csv(value,a,64);
    for(int i=0;i<64;i++)d->page_sizes[i]=i<n?positive_int(a[i],-1):-1;
}
static void add_menu(BtDefinition *d,const char *label,const char *target,int group){
    if(!label||!*label)return;
    if(d->menu_count>=BTS_MAX_MENUS||!grow(d,(void**)&d->menus,&d->menu_capacity,d->menu_count+1,sizeof(BtMenu),BTS_MAX_MENUS,32)){d->truncated=true;return;}
    while(*label=='&')label++;
    if(!*label)return;
    BtMenu*m=&d->menus[d->menu_count++];memset(m,0,sizeof(*m));
    cp(m->label,sizeof(m->label),label);cp(m->target,sizeof(m->target),target);
    m->group=group;
}
static void add_ref(BtDefinition*d,const char*owner,const char*name,const char*label,int kind){
    if(!owner||!*owner||!name||!*name)return;
    if(d->ref_count>=BTS_MAX_REFS||!grow(d,(void**)&d->refs,&d->ref_capacity,d->ref_count+1,sizeof(BtDialogRef),BTS_MAX_REFS,64)){d->truncated=true;return;}
    BtDialogRef*r=&d->refs[d->ref_count++];memset(r,0,sizeof(*r));
    cp(r->owner,sizeof(r->owner),owner);cp(r->name,sizeof(r->name),name);
    cp(r->label,sizeof(r->label),label);r->kind=kind;
}
static void line_parse(BtDefinition *d,char *line) {
    char *p=trim(line);
    if(!*p||*p==';'||*p=='#')return;
    if(*p=='['){char *e=strchr(p,']');if(e){*e=0;cp(d->section,sizeof(d->section),p+1);d->last_map=-1;
        if(!strcasecmp(d->section,"Constants"))d->had_constants=true;
        if(!strcasecmp(d->section,"OutputChannels"))d->had_outputs=true;
        d->in_main_menu=false;
    }return;}
    char *eq=strchr(p,'=');if(!eq)return;
    *eq=0;char *key=trim(p),*val=trim(eq+1);
    if(!strcasecmp(d->section,"TunerStudio")&&!strcasecmp(key,"signature")){
        /* The CSV scratch columns are only 127 bytes; an ECU can use a
         * longer, descriptive signature with its hash at the very end. */
        const char *begin=val,*end=val+strlen(val);
        if(*begin=='"'||*begin=='\''){
            char quote=*begin++;
            const char *closing=strrchr(begin,quote);
            if(closing)end=closing;
        }
        size_t length=(size_t)(end-begin);
        if(length>=sizeof(d->signature)){
            d->truncated=true;d->had_signature=false;
            return;
        }
        memcpy(d->signature,begin,length);d->signature[length]=0;
        d->had_signature=length>0;return;
    }
    if(!strcasecmp(d->section,"UserDefined")){
        char a[5][128];int n=csv(val,a,5);
        if(!strcasecmp(key,"dialog")){cp(d->current_dialog,sizeof(d->current_dialog),n?a[0]:"");}
        else if(!strcasecmp(key,"field")&&n>=2&&*a[1]&&*a[1]!='{'&&!strchr(a[1],' '))
            add_ref(d,d->current_dialog,a[1],a[0],0);
        else if(!strcasecmp(key,"panel")&&n>0)
            add_ref(d,d->current_dialog,a[0],a[0],1);
        return;
    }
    if(!strcasecmp(d->section,"Menu")){
        char a[6][128];int n=csv(val,a,6);
        if(!strcasecmp(key,"menuDialog"))d->in_main_menu=n>0&&!strncasecmp(a[0],"main",4);
        else if(d->in_main_menu&&!strcasecmp(key,"menu")&&n>0)add_menu(d,a[0],"",0);
        else if(d->in_main_menu&&!strcasecmp(key,"groupMenu")&&n>0)add_menu(d,a[0],"",1);
        else if(d->in_main_menu&&(!strcasecmp(key,"subMenu")||!strcasecmp(key,"groupChildMenu"))&&n>1&&strncasecmp(a[0],"std_separator",13))
            add_menu(d,a[1],a[0],!strcasecmp(key,"subMenu")?3:2);
        return;
    }
    if(!strcasecmp(d->section,"Constants")){
        if(!strcasecmp(key,"page")){d->current_page=positive_int(val,1);return;}
        if(!strcasecmp(key,"pageIdentifier")){page_ids(d,val);return;}
        if(!strcasecmp(key,"pageSize")){page_sizes(d,val);return;}
        if(!strcasecmp(key,"pageChunkWrite")){page_cap(val,d->page_writeable,"C%2i%2o%2c%v");return;}
        if(!strcasecmp(key,"burnCommand")){page_cap(val,d->page_burnable,"B%2i");return;}
        if(!strcasecmp(key,"endianness")){d->big_endian=!strncasecmp(val,"big",3);return;}
        parse_field(d,key,val,false);return;
    }
    if(!strcasecmp(d->section,"OutputChannels")){parse_field(d,key,val,true);return;}
    bool table=!strcasecmp(d->section,"TableEditor"),curve=!strcasecmp(d->section,"CurveEditor");
    if(!table&&!curve)return;
    if((table&&!strcasecmp(key,"table"))||(curve&&!strcasecmp(key,"curve"))){
        d->last_map=-1;
        if(d->map_count>=BTS_MAX_MAPS||!grow(d,(void**)&d->maps,&d->map_capacity,d->map_count+1,sizeof(BtMap),BTS_MAX_MAPS,16)){d->truncated=true;return;}
        char a[5][128];int n=csv(val,a,5);if(n<1||!*a[0])return;
        BtMap*m=&d->maps[d->map_count];memset(m,0,sizeof(*m));
        cp(m->name,sizeof(m->name),a[0]);
        if(table&&n>1)cp(m->alt,sizeof(m->alt),a[1]);
        cp(m->label,sizeof(m->label),n>2&&table?a[2]:(n>1?a[1]:a[0]));
        m->curve=curve;d->last_map=d->map_count++;
    }else if(d->last_map>=0){
        BtMap*m=&d->maps[d->last_map];char a[4][128];csv(val,a,4);
        if(!strcasecmp(key,"xBins")){
            cp(m->x,sizeof(m->x),a[0]);
            cp(m->xchan,sizeof(m->xchan),a[1]);
        }
        if(!strcasecmp(key,"yBins")){
            cp(curve?m->z:m->y,curve?sizeof(m->z):sizeof(m->y),a[0]);
            cp(m->ychan,sizeof(m->ychan),a[1]);
        }
        if(!strcasecmp(key,"zBins"))cp(m->z,sizeof(m->z),a[0]);
    }
}
void bt_ini_reset(BtDefinition*d) {
    free(d->channels);free(d->constants);free(d->maps);free(d->menus);free(d->refs);
    memset(d,0,sizeof(*d));d->current_page=1;d->last_map=-1;
    for(int i=0;i<64;i++){d->page_ids[i]=-1;d->page_sizes[i]=-1;}
}
void bt_ini_feed(BtDefinition*d,const uint8_t*b,size_t n) {
    if(d->truncated)return; /* Reject rather than repeatedly reallocating after OOM. */
    for(size_t i=0;i<n;i++){
        if(d->truncated)return;
        uint8_t c=b[i];
        if(c=='\n'||c=='\r'){
            if(!d->line_overflow&&d->line_len){d->line[d->line_len]=0;line_parse(d,d->line);}
            d->line_len=0;d->line_overflow=false;
        }else if(d->line_len<(int)sizeof(d->line)-1&&!d->line_overflow)d->line[d->line_len++]=(char)c;
        else d->line_overflow=true;
    }
}
bool bt_ini_finish(BtDefinition*d) {
    if(!d->line_overflow&&d->line_len){d->line[d->line_len]=0;line_parse(d,d->line);}
    d->line_len=0;
    d->valid=!d->truncated&&d->had_constants&&d->had_outputs&&d->had_signature&&d->channel_count>0;
    return d->valid;
}
const BtField*bt_field(const BtDefinition*d,const char*name,bool runtime){
    const BtField*list=runtime?d->channels:d->constants;
    int count=runtime?d->channel_count:d->constant_count;
    for(int i=0;i<count;i++)if(!strcasecmp(list[i].name,name))return &list[i];
    return NULL;
}
bool bt_field_value(const BtDefinition *d,const BtField*f,const uint8_t*b,int n,int index,double*out) {
    if(!f||!f->scaled||!out||index<0||index>=f->width*f->height)return false;
    int size=bt_type_size(f->type),off=f->offset+size*index;
    if(size<1||off<0||off>n-size)return false;
    uint64_t u=0;
    if(d->big_endian)for(int i=0;i<size;i++)u=(u<<8)|b[off+i];
    else for(int i=size-1;i>=0;i--)u=(u<<8)|b[off+i];
    double raw=0;
    switch(f->type){
        case BT_U08:case BT_U16:case BT_U32:raw=(double)u;break;
        case BT_S08:raw=(int8_t)u;break;
        case BT_S16:raw=(int16_t)u;break;
        case BT_S32:raw=(int32_t)u;break;
        case BT_F32:{uint32_t b32=(uint32_t)u;float f32;memcpy(&f32,&b32,4);raw=f32;break;}
        case BT_F64:{double f64;memcpy(&f64,&u,8);raw=f64;break;}
        default:return false;
    }
    if(f->kind==2){uint64_t mask=(1ULL<<(f->bit_hi-f->bit_lo+1))-1ULL;
        raw=(double)(((uint64_t)u>>f->bit_lo)&mask);
    }
    double v=raw*f->scale+f->translate;
    if(!isfinite(v))return false;
    *out=v;return true;
}
bool bt_live_decode(BtLive *l,const uint8_t*b,size_t n) {
    if(!b||n<20||memcmp(b,"BLV1",4))return false;
    int payload=b[12]|(b[13]<<8);
    if(payload<4||payload>BTS_LIVE_CAP||n!=(size_t)payload+16)return false;
    uint32_t seq=(uint32_t)b[4]|((uint32_t)b[5]<<8)|((uint32_t)b[6]<<16)|((uint32_t)b[7]<<24);
    if(l->valid&&seq==l->sequence)return false;
    memcpy(l->bytes,b+16,(size_t)payload);l->length=payload;l->sequence=seq;l->valid=true;
    return true;
}
bool bt_live_decode_http(BtLive *l, const uint8_t *b, size_t n, bool *changed) {
    if(changed)*changed=false;
    if(!l||!b||n<20||memcmp(b,"BLV1",4))return false;
    int payload=b[12]|(b[13]<<8);
    if(payload<4||payload>BTS_LIVE_CAP||n!=(size_t)payload+16)return false;
    uint32_t seq=(uint32_t)b[4]|((uint32_t)b[5]<<8)|
                 ((uint32_t)b[6]<<16)|((uint32_t)b[7]<<24);
    if(l->valid && seq==l->sequence) {
        /* A repeated, byte-identical sample occurs when HTTP is faster than
         * the ECU collector. Keep the socket healthy; do not count it as Hz. */
        return l->length==payload && !memcmp(l->bytes,b+16,(size_t)payload);
    }
    if(!bt_live_decode(l,b,n))return false;
    if(changed)*changed=true;
    return true;
}
int bt_required_live_length(const BtDefinition*d,const char*const *names,int count) {
    int max=64;
    for(int i=0;i<count;i++){
        const BtField*f=bt_field(d,names[i],true);
        if(f&&f->scaled&&bt_type_size(f->type)>0){int end=f->offset+bt_type_size(f->type);
            if(end>max&&end<=BTS_LIVE_CAP)max=end;
        }
    }
    if(max<256)max=256;
    return max;
}

/* Local cell encoding only. Never mutate the ECU or silently clamp an invalid value. */
bool bt_encode_value(const BtDefinition*d,const BtField*f,uint8_t*b,int len,int idx,double value){
    if(!d||!f||!b||!f->scaled||idx<0||idx>=f->width*f->height||
       !isfinite(value)||!isfinite(f->scale)||f->scale==0)return false;
    if(f->limits_known&&(value<f->low-1e-8||value>f->high+1e-8))return false;
    int size=bt_type_size(f->type),pos=f->offset+size*idx;
    if(size<1||pos<0||pos>len-size)return false;
    double raw=(value-f->translate)/f->scale;
    if(!isfinite(raw))return false;
    /* TunerStudio 'bits' fields are selectable INI enum values, not
     * non-editable scalars. Change ONLY the declared bits; preserve the rest
     * of their backing byte/word from the ECU baseline. This is still a
     * staged local edit and uses exactly the same protected commit path. */
    if(f->kind==2){
        int width=f->bit_hi-f->bit_lo+1;
        if(width<1||width>32||f->bit_lo<0||f->bit_hi>=size*8 ||
           raw!=floor(raw)||raw<0||raw>=(double)(1ULL<<width))return false;
        uint64_t previous=0;
        for(int j=0;j<size;j++){
            int shift=d->big_endian?(size-1-j)*8:j*8;
            previous|=(uint64_t)b[pos+j]<<shift;
        }
        const uint64_t mask=((1ULL<<width)-1ULL)<<f->bit_lo;
        uint64_t edited=(previous&~mask)|(((uint64_t)raw<<f->bit_lo)&mask);
        for(int j=0;j<size;j++){
            int shift=d->big_endian?(size-1-j)*8:j*8;
            b[pos+j]=(uint8_t)(edited>>shift);
        }
        return true;
    }
    uint64_t n=0;
    if(f->type==BT_F32){if(fabs(raw)>3.4028234663852886e38)return false;
        float v=(float)raw;if(!isfinite(v))return false;uint32_t bits;memcpy(&bits,&v,4);n=bits;
    }else if(f->type==BT_F64){memcpy(&n,&raw,8);
    }else{
        double mn=0,mx=0;
        switch(f->type){
            case BT_U08:mx=255;break;case BT_S08:mn=-128;mx=127;break;
            case BT_U16:mx=65535;break;case BT_S16:mn=-32768;mx=32767;break;
            case BT_U32:mx=4294967295.0;break;case BT_S32:mn=-2147483648.0;mx=2147483647.0;break;
            default:return false;
        }
        double rounded=round(raw);
        if(rounded<mn||rounded>mx)return false;
        n=(uint64_t)(int64_t)rounded;
    }
    for(int j=0;j<size;j++){
        int shift=d->big_endian?(size-1-j)*8:j*8;
        b[pos+j]=(uint8_t)(n>>shift);
    }
    return true;
}

/* SD-backed enum captions from exactly the INI whose fields were parsed.
 * Supports direct lists and TunerStudio #define macro references; never
 * substitutes inferred choices for missing or unsupported definitions.
 * Temporary buffers are static to stay off the tiny ARM9 stack. */
static char choice_line[8192],choice_spec[4096];
static bool find_ini_choices(FILE *f,const char *field){
    bool in_constants=false;
    while(fgets(choice_line,sizeof(choice_line),f)){
        char *p=trim(choice_line);
        if(!*p||*p==';'||*p=='#')continue;
        if(*p=='['){in_constants=!strncasecmp(p,"[Constants]",11);continue;}
        if(!in_constants)continue;
        char *eq=strchr(p,'=');if(!eq)continue;
        *eq=0;
        if(strcasecmp(trim(p),field))continue;
        char *value=trim(eq+1);
        if(strncasecmp(value,"bits",4))break;
        int comma=0,quote=0,brackets=0;
        for(char *q=value;*q;q++){
            if(*q=='"')quote=!quote;
            if(!quote){
                if(*q=='[')brackets++;
                if(*q==']'&&brackets)brackets--;
                if(*q==','&&!brackets&&++comma==4){
                    cp(choice_spec,sizeof(choice_spec),trim(q+1));return true;
                }
            }
        }
        break;
    }
    return false;
}
static bool resolve_ini_macro(FILE *f,const char *name){
    rewind(f);
    while(fgets(choice_line,sizeof(choice_line),f)){
        char *p=trim(choice_line);
        if(strncmp(p,"#define ",8))continue;
        p=trim(p+8);
        char *eq=strchr(p,'=');if(!eq)continue;
        *eq=0;
        if(strcasecmp(trim(p),name))continue;
        cp(choice_spec,sizeof(choice_spec),trim(eq+1));return true;
    }
    return false;
}
int bt_bit_choices(const char *ini_path,const char *field_name,char labels[][36],int cap){
    if(!ini_path||!field_name||!labels||cap<1)return 0;
    FILE *f=fopen(ini_path,"rb");if(!f)return 0;
    int count=0;
    memset(labels,0,(size_t)cap*36);
    if(!find_ini_choices(f,field_name)){fclose(f);return 0;}
    char *input=trim(choice_spec);
    if(*input=='$'){
        char name[64]={0};int n=0;input++;
        while((isalnum((unsigned char)*input)||*input=='_')&&n<63)name[n++]=*input++;
        if(!n||!resolve_ini_macro(f,name)){fclose(f);return 0;}
        input=trim(choice_spec);
    }
    /* Some INIs wrap choices in braces. The braces are syntax, not names. */
    size_t len=strlen(input);
    if(len>1&&input[0]=='{'&&input[len-1]=='}'){
        input[len-1]=0;input=trim(input+1);
    }
    char (*a)[128]=page_csv_scratch;
    int n=csv(input,a,64),index=0;
    for(int i=0;i<n;i++){
        char *label=trim(a[i]);
        char *eq=strchr(label,'=');
        if(eq){
            *eq=0;char *end=NULL;
            long requested=strtol(trim(label),&end,10);
            if(end!=label&&!*trim(end)&&requested>=0&&requested<=65535)index=(int)requested;
            label=trim(eq+1);
        }
        if(index>=cap)break;
        if(*label&&strncasecmp(label,"INVALID",7)&&strcasecmp(label,"reserved"))
            cp(labels[index],36,label);
        if(index+1>count)count=index+1;
        index++;
    }
    fclose(f);return count;
}
