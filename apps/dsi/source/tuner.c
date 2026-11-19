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
        if (!quote && (c=='['||c=='{'||c=='(')) ++depth;
        if (!quote && (c==']'||c=='}'||c==')') && depth>0) --depth;
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
static void add_menu(BtDefinition *d,const char *label,const char *target,int group,const char *condition){
    if(!label||!*label)return;
    if(d->menu_count>=BTS_MAX_MENUS||!grow(d,(void**)&d->menus,&d->menu_capacity,d->menu_count+1,sizeof(BtMenu),BTS_MAX_MENUS,32)){d->truncated=true;return;}
    while(*label=='&')label++;
    if(!*label)return;
    BtMenu*m=&d->menus[d->menu_count++];memset(m,0,sizeof(*m));
    cp(m->label,sizeof(m->label),label);cp(m->target,sizeof(m->target),target);
    cp(m->condition,sizeof(m->condition),condition);m->group=group;
}
static void add_ref(BtDefinition*d,const char*owner,const char*name,const char*label,int kind,const char*details,const char*condition){
    if(!owner||!*owner||!name||!*name)return;
    if(d->ref_count>=BTS_MAX_REFS||!grow(d,(void**)&d->refs,&d->ref_capacity,d->ref_count+1,sizeof(BtDialogRef),BTS_MAX_REFS,32)){d->truncated=true;return;}
    BtDialogRef*r=&d->refs[d->ref_count++];memset(r,0,sizeof(*r));
    cp(r->owner,sizeof(r->owner),owner);cp(r->name,sizeof(r->name),name);
    cp(r->label,sizeof(r->label),label);cp(r->detail,sizeof(r->detail),details);
    cp(r->condition,sizeof(r->condition),condition);r->kind=kind;
}
static bool placement(const char *s){
    return s&&(!strcasecmp(s,"West")||!strcasecmp(s,"East")||
        !strcasecmp(s,"North")||!strcasecmp(s,"South")||
        !strcasecmp(s,"Center"));
}
static void set_placement(BtDefinition *d,const char *where){
    if(d->ref_count>0&&placement(where))
        cp(d->refs[d->ref_count-1].placement,sizeof(d->refs[0].placement),where);
}
static bool ini_expression(const char *s){return s&&(*s=='{'||*s=='$');}
static void line_parse(BtDefinition *d,char *line) {
    char *p=trim(line);
    if(!*p||*p==';'||*p=='#')return;
    if(*p=='['){char *e=strchr(p,']');if(e){*e=0;cp(d->section,sizeof(d->section),p+1);d->last_map=-1;d->last_graph=-1;
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
    if(!strcasecmp(d->section,"GaugeConfigurations")){
        if(!strcasecmp(key,"gaugeCategory"))return;
        char cols[12][128];int n=csv(val,cols,12);
        if(n<3||!cols[0][0])return;
        if(d->gauge_count>=BTS_MAX_GAUGES||
           !grow(d,(void**)&d->gauges,&d->gauge_capacity,d->gauge_count+1,
                 sizeof(BtGauge),BTS_MAX_GAUGES,24)){
            d->truncated=true;return;
        }
        BtGauge*g=&d->gauges[d->gauge_count++];memset(g,0,sizeof(*g));
        cp(g->name,sizeof(g->name),key);
        cp(g->channel,sizeof(g->channel),cols[0]);
        cp(g->label,sizeof(g->label),cols[1]);
        cp(g->unit,sizeof(g->unit),cols[2]);
        if(n>=5&&num(cols[3],&g->min_value)&&num(cols[4],&g->max_value)&&
           g->max_value>g->min_value)g->range_known=true;
        return;
    }
    if(!strcasecmp(d->section,"UserDefined")||!strcasecmp(d->section,"UiDialogs")){
        char a[8][128];int n=csv(val,a,8);
        if(!strcasecmp(key,"dialog")){
            cp(d->current_dialog,sizeof(d->current_dialog),n?a[0]:"");
            d->last_graph=-1;
            /* A dialog title is metadata, not a selectable setting. */
        }else if(!strcasecmp(key,"liveGraph") && n>0 && *d->current_dialog && *a[0]){
            d->last_graph=-1;
            if(d->graph_count>=BTS_MAX_INI_GRAPHS ||
               !grow(d,(void**)&d->graphs,&d->graph_capacity,d->graph_count+1,sizeof(BtIniGraph),BTS_MAX_INI_GRAPHS,16)){
                d->truncated=true;return;
            }
            BtIniGraph *g=&d->graphs[d->graph_count];memset(g,0,sizeof(*g));
            cp(g->owner,sizeof(g->owner),d->current_dialog);
            cp(g->name,sizeof(g->name),a[0]);
            cp(g->title,sizeof(g->title),n>1?a[1]:"Live Graph");
            g->first_line=d->graph_line_count;
            d->last_graph=d->graph_count++;
            add_ref(d,d->current_dialog,a[0],g->title,6,"","");
        }else if(!strcasecmp(key,"graphLine")&&n>0&&*a[0]&&d->last_graph>=0){
            if(d->graph_line_count>=BTS_MAX_GRAPH_LINES ||
               !grow(d,(void**)&d->graph_lines,&d->graph_line_capacity,d->graph_line_count+1,sizeof(BtGraphLine),BTS_MAX_GRAPH_LINES,32)){
                d->truncated=true;return;
            }
            BtGraphLine *line=&d->graph_lines[d->graph_line_count++];memset(line,0,sizeof(*line));
            cp(line->channel,sizeof(line->channel),a[0]);line->graph=d->last_graph;
            d->graphs[d->last_graph].line_count++;
        }else if(!strcasecmp(key,"field")&&n>0){
            if(n>=2&&*a[1]&&!ini_expression(a[1])&&!strchr(a[1],' '))
                add_ref(d,d->current_dialog,a[1],a[0],0,n>4?a[4]:"",n>2?a[2]:"");
            else if(*a[0])add_ref(d,d->current_dialog,"@text",a[0],5,"",n>2?a[2]:"");
        }else if(!strcasecmp(key,"displayOnlyField")&&n>=2){
            add_ref(d,d->current_dialog,a[1],a[0],3,n>4?a[4]:"",n>2?a[2]:"");
        }else if(!strcasecmp(key,"panel")&&n>0){
            /* Keep INI border placement for grouping across DSi screens;
             * it is NOT a human-facing control or a conditional expression. */
            const char *condition=n>2?a[2]:(n>1&&!placement(a[1])?a[1]:"");
            add_ref(d,d->current_dialog,a[0],a[0],1,"",condition);
            set_placement(d,n>1?a[1]:"");
        }else if(!strcasecmp(key,"commandButton")&&n>=2){
            add_ref(d,d->current_dialog,a[1],a[0],2,n>3?a[3]:"",n>2?a[2]:"");
        }else if(!strcasecmp(key,"gauge")&&n>=1){
            /* Gauge names point to [GaugeConfigurations] templates, not
             * [OutputChannels] directly. Never display the word 'Gauge'
             * as a fake editable setting in the list. */
            add_ref(d,d->current_dialog,a[0],a[0],7,"",
                    n>2?a[2]:(n>1&&!placement(a[1])?a[1]:""));
            set_placement(d,n>1?a[1]:"");
        }else if((!strcasecmp(key,"runtimeValue")||!strcasecmp(key,"readout"))&&n>=1){
            const char *channel=n>1&&*a[1]&&!placement(a[1])?a[1]:a[0];
            add_ref(d,d->current_dialog,channel,a[0],4,"",n>2?a[2]:"");
            set_placement(d,n>1?a[1]:"");
        }else if(!strcasecmp(key,"topicHelp")||!strcasecmp(key,"help")||!strcasecmp(key,"helpText")){
            /* Help blocks describe the editor/dialog; never make them fields. */
        }
        return;
    }
    if(!strcasecmp(d->section,"Menu")){
        char a[7][128];int n=csv(val,a,7);
        if(!strcasecmp(key,"menuDialog"))d->in_main_menu=n>0&&!strncasecmp(a[0],"main",4);
        else if(d->in_main_menu&&!strcasecmp(key,"menu")&&n>0)add_menu(d,a[0],"",0,n>1?a[1]:"");
        else if(d->in_main_menu&&!strcasecmp(key,"groupMenu")&&n>0)add_menu(d,a[0],"",1,n>1?a[1]:"");
        else if(d->in_main_menu&&(!strcasecmp(key,"subMenu")||!strcasecmp(key,"groupChildMenu"))&&n>1&&strncasecmp(a[0],"std_separator",13))
            add_menu(d,a[1],a[0],!strcasecmp(key,"subMenu")?3:2,n>2?a[2]:"");
        return;
    }
    if(!strcasecmp(d->section,"ConstantsExtensions")){
        if(!strcasecmp(key,"readOnly")||!strcasecmp(key,"requiresPowerCycle")){
            char names[12][128];int count=csv(val,names,12);
            for(int j=0;j<count;j++)for(int i=0;i<d->constant_count;i++){
                if(!strcasecmp(names[j],d->constants[i].name)){
                    if(!strcasecmp(key,"readOnly"))d->constants[i].read_only=true;
                    else d->constants[i].power_cycle=true;
                }
            }
        }
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
        int original=d->last_map;
        char a[4][128];csv(val,a,4);
        /* A CurveEditor may contain multiple yBins series.  TunerStudio
         * displays them all; preserve each curve rather than discarding all
         * but the final series.  Each gets a reachable editor through NEXT. */
        if(curve&&!strcasecmp(key,"yBins")&&d->maps[original].z[0]){
            if(d->map_count>=BTS_MAX_MAPS ||
               !grow(d,(void**)&d->maps,&d->map_capacity,d->map_count+1,
                     sizeof(BtMap),BTS_MAX_MAPS,16)){d->truncated=true;return;}
            BtMap *clone=&d->maps[d->map_count];
            *clone=d->maps[original];
            clone->series_index=d->maps[original].series_index+1;
            clone->z[0]=0;
            d->last_map=d->map_count++;
        }
        BtMap*m=&d->maps[d->last_map];
        if(!strcasecmp(key,"xBins")){
            cp(m->x,sizeof(m->x),a[0]);
            cp(m->xchan,sizeof(m->xchan),a[1]);
            m->x_readonly=!strcasecmp(a[2],"readOnly");
        }
        if(!strcasecmp(key,"yBins")){
            cp(curve?m->z:m->y,curve?sizeof(m->z):sizeof(m->y),a[0]);
            cp(m->ychan,sizeof(m->ychan),a[1]);
            m->y_readonly=!strcasecmp(a[2],"readOnly");
        }
        if(!strcasecmp(key,"zBins"))cp(m->z,sizeof(m->z),a[0]);
        if(!strcasecmp(key,"topicHelp"))cp(m->help_topic,sizeof(m->help_topic),a[0]);
        if(!strcasecmp(key,curve?"columnLabel":"xyLabels")){
            cp(m->x_label,sizeof(m->x_label),a[0]);
            cp(m->y_label,sizeof(m->y_label),a[1]);
        }
        if(!strcasecmp(key,"lineLabel")&&curve&&*a[0])
            cp(m->y_label,sizeof(m->y_label),a[0]);
    }
}
void bt_ini_reset(BtDefinition*d) {
    free(d->channels);free(d->constants);free(d->maps);free(d->menus);free(d->refs);free(d->graphs);free(d->graph_lines);free(d->gauges);
    memset(d,0,sizeof(*d));d->current_page=1;d->last_map=-1;d->last_graph=-1;
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
/* Use actual map and menu captions for subpanels where the ECU INI
 * supplies them.  A placement directive must never appear as a list entry.
 * Fall back to a readable identifier, not a made-up setting or caption. */
static void panel_display_label(char *out,size_t size,const char *name){
    size_t n=0;
    for(size_t i=0;name[i]&&n+1<size;i++){
        unsigned char c=(unsigned char)name[i];
        unsigned char prev=i?(unsigned char)name[i-1]:0;
        unsigned char next=(unsigned char)name[i+1];
        if(c=='_'||c=='-'){if(n&&out[n-1]!=' '&&n+1<size)out[n++]=' ';continue;}
        if(i&&n&&out[n-1]!=' '&&
           ((isupper(c)&&(islower(prev)||isdigit(prev)||
             (isupper(prev)&&islower(next))))||
            (isdigit(c)&&isalpha(prev))||
            (isalpha(c)&&isdigit(prev))))out[n++]=' ';
        if(n+1<size){out[n]=(char)(n==0?toupper(c):c);n++;}
    }
    out[n]=0;
}
static void finalize_panel_labels(BtDefinition*d){
    for(int i=0;i<d->ref_count;i++){
        BtDialogRef *r=&d->refs[i];if(r->kind!=1)continue;
        const char *caption=NULL;
        for(int k=0;k<d->map_count;k++)if(!strcasecmp(d->maps[k].name,r->name)){
            caption=d->maps[k].label;break;
        }
        if(!caption)for(int k=0;k<d->menu_count;k++)
            if(d->menus[k].target[0]&&!strcasecmp(d->menus[k].target,r->name)){
                caption=d->menus[k].label;break;
            }
        if(caption&&*caption)cp(r->label,sizeof(r->label),caption);
        else panel_display_label(r->label,sizeof(r->label),r->name);
    }
}
bool bt_ini_finish(BtDefinition*d) {
    if(!d->line_overflow&&d->line_len){d->line[d->line_len]=0;line_parse(d,d->line);}
    d->line_len=0;
    d->valid=!d->truncated&&d->had_constants&&d->had_outputs&&d->had_signature&&d->channel_count>0;
    if(d->valid)finalize_panel_labels(d);
    return d->valid;
}
const BtGauge*bt_gauge(const BtDefinition*d,const char *name){
    if(!d||!name)return NULL;
    for(int i=0;i<d->gauge_count;i++)
        if(!strcasecmp(d->gauges[i].name,name))return &d->gauges[i];
    return NULL;
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

/* Load the exact caption list from the cached generated ECU INI.  The old
 * parser stopped at 32 choices and split at 64 CSV columns, silently dropping
 * GPIO and sensor enumerations with hundreds of choices.  Keep the identifier
 * numbering, including INVALID/reserved gaps, so the wire value is unchanged.
 * File buffers are static to fit the small ARM9 stack. */
static char choice_line[16384], choice_spec[16384];
static bool find_ini_choices(FILE *f,const char *field){
    bool in_constants=false;
    rewind(f);
    while(fgets(choice_line,sizeof(choice_line),f)){
        char *p=trim(choice_line);
        if(!*p||*p==';'||*p=='#')continue;
        if(*p=='['){in_constants=!strncasecmp(p,"[Constants]",11);continue;}
        if(!in_constants)continue;
        char *eq=strchr(p,'=');if(!eq)continue;
        *eq=0;if(strcasecmp(trim(p),field))continue;
        char *value=trim(eq+1);
        if(strncasecmp(value,"bits",4))break;
        int comma=0,quote=0,brackets=0;
        for(char *q=value;*q;q++){
            if(*q=='"')quote=!quote;
            if(!quote){
                if(*q=='[')brackets++;
                if(*q==']'&&brackets)brackets--;
                if(*q==','&&!brackets&&++comma==4){
                    if(strlen(trim(q+1))>=sizeof(choice_spec))return false;
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
        if(strncmp(p,"#define",7)||!isspace((unsigned char)p[7]))continue;
        p=trim(p+7);
        char *end=p;while(isalnum((unsigned char)*end)||*end=='_')end++;
        if(end==p||strncasecmp(p,name,(size_t)(end-p))||(size_t)(end-p)!=strlen(name))continue;
        p=trim(end);if(*p=='=')p=trim(p+1);
        if(!*p||strlen(p)>=sizeof(choice_spec))return false;
        cp(choice_spec,sizeof(choice_spec),p);return true;
    }
    return false;
}
/* Extract one CSV token without allocating a huge 256x128 matrix.  Explicit
 * index prefixes such as 42 = "Pin X" are respected. */
static const char *next_choice(const char *p,char out[128]){
    bool quote=false;int depth=0;size_t n=0;
    while(*p&&isspace((unsigned char)*p))p++;
    while(*p){
        char c=*p;
        if(c=='"')quote=!quote;
        if(!quote){if(c=='{')depth++;if(c=='}'&&depth)depth--;
            if(c==','&&depth==0){p++;break;}
            if(c==';'&&depth==0)break;
        }
        if(n<127)out[n++]=c;
        p++;
    }
    out[n]=0;
    char *v=trim(out);if(v!=out)memmove(out,v,strlen(v)+1);
    size_t len=strlen(out);
    if(len>1&&out[0]=='"'&&out[len-1]=='"'){
        memmove(out,out+1,len-2);out[len-2]=0;
    }
    return p;
}
int bt_bit_choices(const char *ini_path,const char *field_name,char labels[][36],int cap){
    if(!ini_path||!field_name||!labels||cap<1)return 0;
    FILE *f=fopen(ini_path,"rb");if(!f)return 0;
    memset(labels,0,(size_t)cap*36);
    if(!find_ini_choices(f,field_name)){fclose(f);return 0;}
    /* Permit nested indirection, but never make up a missing definition. */
    for(int depth=0;depth<8;depth++){
        char *input=trim(choice_spec);
        if(*input=='{'&&strchr(input,'}')){
            size_t len=strlen(input);
            if(len>1&&input[len-1]=='}'){
                input[len-1]=0;memmove(choice_spec,input+1,strlen(input+1)+1);
                input=trim(choice_spec);
            }
        }
        if(*input!='$')break;
        char name[96]={0};int n=0;input++;
        if(*input=='{')input++;
        while((isalnum((unsigned char)*input)||*input=='_')&&n<95)name[n++]=*input++;
        if(n==0||!resolve_ini_macro(f,name)){fclose(f);return 0;}
        if(depth==7){fclose(f);return 0;}
    }
    char *input=trim(choice_spec);
    int count=0,index=0;
    while(*input){
        char token[128];const char *next=next_choice(input,token);
        if(next==input)break;
        input=(char*)next;
        char *label=trim(token);
        if(!*label)continue;
        char *eq=strchr(label,'=');
        if(eq){
            *eq=0;char *end=NULL;
            long requested=strtol(trim(label),&end,0);
            if(end!=label&&!*trim(end)&&requested>=0&&requested<cap)index=(int)requested;
            else {fclose(f);return 0;}
            label=trim(eq+1);
        }
        size_t len=strlen(label);
        if(len>1&&label[0]=='"'&&label[len-1]=='"'){
            label[len-1]=0;label++;
        }
        if(index>=cap){fclose(f);return 0;} /* No silently truncated enums. */
        if(*label&&strncasecmp(label,"INVALID",7)&&strcasecmp(label,"reserved"))
            cp(labels[index],36,label);
        if(index+1>count)count=index+1;
        index++;
    }
    fclose(f);return count;
}

/* [SettingContextHelp] contains almost 2,000 individual entries in the
 * Proteus INI. Scanning 900KB from SD for EACH highlighted setting made the
 * DSi feel stuck inside nested folders. Keep only key hashes and SD offsets
 * (about 16KB on ARM9), then seek directly to the actual INI description. */
#define BTS_HELP_INDEX_CAP 2500
typedef struct { uint32_t key_hash; uint32_t file_offset; } BtHelpOffset;
static BtHelpOffset help_offsets[BTS_HELP_INDEX_CAP];
static int help_offset_count=-1;
static char help_index_path[256];
static uint32_t help_hash(const char *text){
    uint32_t hash=2166136261u;
    for(const unsigned char *p=(const unsigned char *)text;*p;p++){
        hash^=(unsigned char)tolower(*p);hash*=16777619u;
    }
    return hash;
}
static int compare_help(const void *aa,const void *bb){
    const BtHelpOffset *a=aa,*b=bb;
    if(a->key_hash<b->key_hash)return -1;
    if(a->key_hash>b->key_hash)return 1;
    return a->file_offset<b->file_offset?-1:a->file_offset>b->file_offset;
}
void bt_ini_help_index_reset(void){
    help_offset_count=-1;help_index_path[0]=0;
}
static bool help_index_open(const char *path){
    if(help_offset_count>=0 && !strcmp(help_index_path,path))return true;
    bt_ini_help_index_reset();
    FILE *f=fopen(path,"rb");if(!f)return false;
    bool in_section=false;
    int count=0;
    for(;;){
        long offset=ftell(f);
        if(offset<0 || (unsigned long)offset>UINT32_MAX)break;
        if(!fgets(choice_line,sizeof(choice_line),f))break;
        char *p=trim(choice_line);
        if(*p=='['){
            if(in_section)break;
            in_section=!strncasecmp(p,"[SettingContextHelp]",20);
            continue;
        }
        if(!in_section || !*p || *p==';' || *p=='#')continue;
        char *eq=strchr(p,'=');if(!eq)continue;
        *eq=0;
        if(count>=BTS_HELP_INDEX_CAP){fclose(f);return false;}
        help_offsets[count].key_hash=help_hash(trim(p));
        help_offsets[count].file_offset=(uint32_t)offset;
        count++;
    }
    fclose(f);
    qsort(help_offsets,(size_t)count,sizeof(help_offsets[0]),compare_help);
    cp(help_index_path,sizeof(help_index_path),path);
    help_offset_count=count;
    return true;
}
/* Read INI text exactly; the hash index is only for locating data, never an
 * approximation of the key or a synthetic tooltip. */
bool bt_ini_context_help(const char *ini_path,const char *target,char *out,size_t cap){
    if(!out||cap<2)return false;
    out[0]=0;
    if(!ini_path||!target||!*target||!help_index_open(ini_path))return false;
    uint32_t wanted=help_hash(target);
    int lo=0,hi=help_offset_count;
    while(lo<hi){int mid=lo+(hi-lo)/2;
        if(help_offsets[mid].key_hash<wanted)lo=mid+1;
        else hi=mid;
    }
    if(lo>=help_offset_count||help_offsets[lo].key_hash!=wanted)return false;
    FILE *f=fopen(ini_path,"rb");if(!f)return false;
    bool found=false;
    for(int i=lo;i<help_offset_count && help_offsets[i].key_hash==wanted;i++){
        if(fseek(f,(long)help_offsets[i].file_offset,SEEK_SET)!=0||
           !fgets(choice_line,sizeof(choice_line),f))continue;
        char *p=trim(choice_line),*eq=strchr(p,'=');if(!eq)continue;
        *eq=0;if(strcasecmp(trim(p),target))continue;
        char *v=trim(eq+1);size_t n=strlen(v);
        if(n>1&&v[0]=='"'){
            char *end=strrchr(v+1,'"');if(end){*end=0;v++;}
        }else{
            char *comment=strchr(v,';');if(comment)*comment=0;
            v=trim(v);
        }
        cp(out,cap,v);found=out[0]!=0;break;
    }
    fclose(f);return found;
}

/* Load the exact definition without reinterpreting expressions or CSV commas.
 * This runs only at editor opening (not per frame). */
bool bt_ini_rhs(const char *path,const char *section,const char *name,char *out,size_t cap){
    if(!out||cap<2)return false;
    out[0]=0;
    if(!path||!section||!name||!*name)return false;
    FILE*f=fopen(path,"rb");if(!f)return false;
    bool matching=false,found=false;
    while(fgets(choice_line,sizeof(choice_line),f)){
        char *p=trim(choice_line);
        if(*p=='['){char *close=strchr(p,']');
            if(close){*close=0;matching=!strcasecmp(p+1,section);}
            continue;
        }
        if(!matching||!*p||*p==';'||*p=='#')continue;
        char *eq=strchr(p,'=');if(!eq)continue;
        *eq=0;if(strcasecmp(trim(p),name))continue;
        p=trim(eq+1);
        if(strlen(p)>=cap)break;
        cp(out,cap,p);found=*out!=0;break;
    }
    fclose(f);return found;
}
/* TunerStudio also defines UI-only arrays without offsets. The Proteus INI
 * supplies x-axis defaults for gear, solenoid and range values in its
 * [ConstantsExtensions] defaultValue entries. They are not ECU addressable.
 * Read their *declared* values as read-only axes; do not invent a wire offset. */
bool bt_ini_virtual_array(const char *path,const char *name,double *values,int capacity,int *count){
    if(count)*count=0;
    if(!path||!name||!*name||!values||capacity<1||!count)return false;
    FILE*f=fopen(path,"rb");if(!f)return false;
    int section=0,declared=0,expected=0,n=0;
    bool defaults=false;
    while(fgets(choice_line,sizeof(choice_line),f)){
        char *p=trim(choice_line);
        if(*p=='['){
            section=(!strncasecmp(p,"[Constants]",11)||!strncasecmp(p,"[PcVariables]",13))?1:
                !strncasecmp(p,"[ConstantsExtensions]",21)?2:0;
            continue;
        }
        if(!section||!*p||*p==';'||*p=='#')continue;
        char *eq=strchr(p,'=');if(!eq)continue;
        *eq=0;char *key=trim(p),*value=trim(eq+1);
        if(section==1 && !strcasecmp(key,name)){
            char a[6][128];int fields=csv(value,a,6);
            if(fields>=3&&!strcasecmp(a[0],"array")&&a[2][0]=='['){
                int w=0,h=1;
                if(sscanf(a[2],"[ %d x %d",&w,&h)<1)w=0;
                if(w>=1&&h>=1&&w*h<=capacity){
                    declared=1;expected=w*h;
                }
            }
        }else if(section==2 && !strcasecmp(key,"defaultValue")){
            char *sep=strchr(value,',');if(!sep)continue;
            *sep=0;if(strcasecmp(trim(value),name))continue;
            const char *q=sep+1;
            while(*q){
                while(isspace((unsigned char)*q))q++;
                if(!*q||*q==';')break;
                if(n>=capacity){defaults=false;break;}
                char *end=NULL;
                double v=strtod(q,&end);
                if(end==q||!isfinite(v)){defaults=false;break;}
                values[n++]=v;q=end;
                if(*q&&!isspace((unsigned char)*q)&&*q!=';'){defaults=false;break;}
            }
            defaults=n>0;
        }
    }
    fclose(f);
    if(!declared||!defaults||n<expected)return false;
    *count=expected;return true;
}

bool bt_ini_field_format(const char *path,const char *name,BtIniFormat *format){
    if(!format)return false;
    memset(format,0,sizeof(*format));
    /* ARM9 parsing is single-threaded. Large temporary INI CSV buffers
     * belong in static working memory, not the DSi's tiny C stack. */
    static char buffer[1100];
    static char a[11][128];
    if(!bt_ini_rhs(path,"Constants",name,buffer,sizeof(buffer)))return false;
    int n=csv(buffer,a,11);
    if(n<8)return false;
    int idx=!strcasecmp(a[0],"array")?4:!strcasecmp(a[0],"scalar")?3:-1;
    if(idx<0||n<=idx+4)return false;
    cp(format->unit,sizeof(format->unit),a[idx]);
    cp(format->scale,sizeof(format->scale),a[idx+1]);
    cp(format->translate,sizeof(format->translate),a[idx+2]);
    cp(format->low,sizeof(format->low),a[idx+3]);
    cp(format->high,sizeof(format->high),a[idx+4]);
    return true;
}
/* Strict, bounded evaluator for arithmetic, comparisons, boolean operators,
 * ternaries, and existing ECU variable names in generated rusEFI INIs.
 * Missing operands, unsupported functions, division by zero and garbage FAIL;
 * no silent default values or arbitrary code evaluation. */
typedef struct {const char *p;BtVarLookup lookup;void*ctx;int depth;bool ok;} BtExpr;
static void expr_space(BtExpr*e){while(isspace((unsigned char)*e->p))e->p++;}
static double expr_ternary(BtExpr*e);
static double expr_atom(BtExpr*e){
    expr_space(e);
    if(++e->depth>48){e->ok=false;return 0;}
    double result=0;
    if(*e->p=='('){e->p++;result=expr_ternary(e);expr_space(e);
        if(*e->p!=')')e->ok=false;else e->p++;
    }else if(*e->p=='!'||*e->p=='+'||*e->p=='-'){
        char op=*e->p++;result=expr_atom(e);
        if(op=='!')result=!(result!=0);else if(op=='-')result=-result;
    }else if(isalpha((unsigned char)*e->p)||*e->p=='_'){
        char name[65];size_t n=0;
        while(isalnum((unsigned char)*e->p)||*e->p=='_'){
            if(n+1>=sizeof(name)){e->ok=false;break;}
            name[n++]=*e->p++;
        }
        name[n]=0;
        if(!n||!e->lookup||!e->lookup(name,e->ctx,&result))e->ok=false;
    }else{
        char *end=NULL;result=strtod(e->p,&end);
        if(end==e->p)e->ok=false;
        else e->p=end;
    }
    e->depth--;return result;
}
static double expr_mul(BtExpr*e){double x=expr_atom(e);
    for(;;){expr_space(e);char op=*e->p;
        if(op!='*'&&op!='/'&&op!='%')break;
        e->p++;
        double y=expr_atom(e);
        if(op=='*')x*=y;else if(op=='/'){if(y==0)e->ok=false;else x/=y;}
        else{if(y==0)e->ok=false;else x=fmod(x,y);}
    }return x;
}
static double expr_add(BtExpr*e){double x=expr_mul(e);
    for(;;){expr_space(e);char op=*e->p;
        if(op!='+'&&op!='-')break;
        e->p++;double y=expr_mul(e);
        x=op=='+'?x+y:x-y;
    }return x;
}
static double expr_compare(BtExpr*e){double x=expr_add(e);
    for(;;){expr_space(e);const char*p=e->p;int n=0;enum{EQ,NE,LE,GE,LT,GT}o=EQ;
        if(!strncmp(p,"==",2)){n=2;o=EQ;}
        else if(!strncmp(p,"!=",2)){n=2;o=NE;}
        else if(!strncmp(p,"<=",2)){n=2;o=LE;}
        else if(!strncmp(p,">=",2)){n=2;o=GE;}
        else if(*p=='<'){n=1;o=LT;}
        else if(*p=='>'){n=1;o=GT;}
        if(!n)break;
        e->p+=n;double y=expr_add(e);
        switch(o){case EQ:x=x==y;break;case NE:x=x!=y;break;case LE:x=x<=y;break;
                  case GE:x=x>=y;break;case LT:x=x<y;break;case GT:x=x>y;break;}
    }return x;
}
static double expr_and(BtExpr*e){double x=expr_compare(e);
    for(;;){expr_space(e);if(strncmp(e->p,"&&",2))break;
        e->p+=2;double y=expr_compare(e);x=(x!=0&&y!=0);
    }return x;
}
static double expr_or(BtExpr*e){double x=expr_and(e);
    for(;;){expr_space(e);if(strncmp(e->p,"||",2))break;
        e->p+=2;double y=expr_and(e);x=(x!=0||y!=0);
    }return x;
}
static double expr_ternary(BtExpr*e){double x=expr_or(e);
    expr_space(e);
    if(*e->p=='?'){
        e->p++;double yes=expr_ternary(e);expr_space(e);
        if(*e->p!=':'){e->ok=false;return 0;}
        e->p++;double no=expr_ternary(e);x=x!=0?yes:no;
    }return x;
}
bool bt_numeric_eval(const char *source,BtVarLookup var,void*ctx,double*out){
    if(!source||!out)return false;
    BtExpr e={source,var,ctx,0,true};expr_space(&e);
    bool braces=*e.p=='{';if(braces)e.p++;
    double x=expr_ternary(&e);expr_space(&e);
    if(braces){if(*e.p!='}')e.ok=false;else {e.p++;expr_space(&e);}}
    if(*e.p||!isfinite(x)||!e.ok)return false;
    *out=x;return true;
}

/* Render ECU-provided multiline help topics rather than treating `help =`
 * section headers as fake fields.  All topic text comes from the INI. */
bool bt_ini_topic_help(const char *ini_path,const char *topic,char *out,size_t cap){
    if(!out||cap<2)return false;
    out[0]=0;
    if(!ini_path||!topic||!*topic)return false;
    FILE *f=fopen(ini_path,"rb");if(!f)return false;
    bool section=false,found=false;size_t used=0;
    while(fgets(choice_line,sizeof(choice_line),f)){
        char *p=trim(choice_line);
        if(*p=='['){section=!strncasecmp(p,"[UserDefined]",13)||
            !strncasecmp(p,"[UiDialogs]",11);if(found)break;continue;}
        if(!section||!*p||*p==';'||*p=='#')continue;
        char *eq=strchr(p,'=');if(!eq)continue;
        *eq=0;char *key=trim(p),*val=trim(eq+1);
        if(!strcasecmp(key,"help")){
            if(found)break;
            char a[3][128];int n=csv(val,a,3);
            if(n>0&&!strcasecmp(a[0],topic))found=true;
            continue;
        }
        if(!found||strcasecmp(key,"text"))continue;
        size_t n=strlen(val);
        if(n>1&&val[0]=='"'&&val[n-1]=='"'){val[n-1]=0;val++;}
        for(const char *q=val;*q&&used+1<cap;){
            if(*q=='<'){const char *e=strchr(q,'>');if(e){q=e+1;continue;}}
            if(*q=='&'){
                if(!strncmp(q,"&amp;",5)){out[used++]='&';q+=5;continue;}
                if(!strncmp(q,"&lt;",4)){out[used++]='<';q+=4;continue;}
                if(!strncmp(q,"&gt;",4)){out[used++]='>';q+=4;continue;}
            }
            if(*q=='\\'&&q[1]=='n'){out[used++]=' ';q+=2;continue;}
            out[used++]=*q++;
        }
        if(used+2<cap)out[used++]=' ';
        out[used]=0;
    }
    fclose(f);
    return found&&used>0;
}
