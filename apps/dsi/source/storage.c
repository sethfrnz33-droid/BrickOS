#include "storage.h"
#include "signature.h"
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#if defined(BTS_NATIVE_NDS)
#include <fat.h>
#endif

static char folder[92]="", cached[128]="", pending[128]="", backup[128]="";
static char message[80]="SD NOT INITIALIZED";
static bool ready=false;
static FILE *writer=NULL;
static size_t written=0;
/* Larger buffered FAT writes avoid a disk transaction for every TCP packet. */
static char write_buffer[4096];
static void note(const char *desc){snprintf(message,sizeof(message),"%s",desc);}
static bool make_folder(const char *prefix){
    if(mkdir(prefix,0777)!=0 && errno!=EEXIST)return false;
    char test[140];snprintf(test,sizeof(test),"%s/.bts-test",prefix);
    FILE *f=fopen(test,"wb");if(!f)return false;
    bool ok=fputc('B',f)!=EOF;
    if(fclose(f)!=0)ok=false;
    if(!ok){remove(test);return false;}
    remove(test);
    snprintf(folder,sizeof(folder),"%s",prefix);
    snprintf(cached,sizeof(cached),"%s/ecu.ini",folder);
    snprintf(pending,sizeof(pending),"%s/ecu.tmp",folder);
    snprintf(backup,sizeof(backup),"%s/ecu.bak",folder);
    return true;
}
/* A failed libfat init alone does not identify the cause. In particular,
 * DS-mode homebrew needs a loader-provided DLDI driver to reach the DSi SD.
 * Probe both named volumes without writing before attempting our own folder.
 * Never retry fatInitDefault within a process: libdvm/libfat can cache failure.
 */
#if defined(BTS_NATIVE_NDS)
static bool root_accessible(const char *path){
    DIR *dir=opendir(path);
    if(!dir)return false;
    closedir(dir);
    return true;
}
#endif
bool bt_sd_init(void){
    if(ready)return true;
#if defined(BTS_NATIVE_NDS)
    bool initialized=fatInitDefault();
    bool sd_seen=root_accessible("sd:/");
    bool fat_seen=root_accessible("fat:/");
    if(!initialized && !sd_seen && !fat_seen){
        note("NO SD DEVICE / TRY DSI MODE");return false;
    }
    /* Do not accidentally create files on a different default device. */
    if(!((sd_seen && make_folder("sd:/BrickECU")) ||
         (fat_seen && make_folder("fat:/BrickECU")))){
        note((sd_seen || fat_seen)?"SD FOUND / WRITE FAILED":"NO SD VOLUME / CHECK LOADER");
        return false;
    }
#else
    #ifndef BTS_HOST_CACHE_DIR
    #define BTS_HOST_CACHE_DIR "/tmp/BrickECU-DSi-SD-Test"
    #endif
    if(!make_folder(BTS_HOST_CACHE_DIR)){note("HOST TEST CACHE FAILED");return false;}
#endif
    ready=true;note("SD READY / BRICKECU");return true;
}
bool bt_sd_ready(void){return ready;}
const char *bt_sd_asset_root(void){return ready?folder:"";}
int bt_sd_layout_load(void){
    if(!ready)return 2;
    char path[144];snprintf(path,sizeof(path),"%s/dsi-layout.cfg",folder);
    FILE *file=fopen(path,"rb");if(!file)return 2;
    int v=2;bool okay=fscanf(file,"BTS_LAYOUT_V1 %d",&v)==1;
    fclose(file);
    return okay&&v>=2&&v<=4?v:2;
}
bool bt_sd_layout_save(int layout){
    if(!ready||layout<2||layout>4)return false;
    char path[144];snprintf(path,sizeof(path),"%s/dsi-layout.cfg",folder);
    FILE *file=fopen(path,"wb");if(!file)return false;
    bool ok=fprintf(file,"BTS_LAYOUT_V1 %d\n",layout)>0;
    if(fclose(file)!=0)ok=false;
    return ok;
}

/* Stable INI target identifiers are never pointer indexes. Do not reuse a
 * favorite list for a different ECU signature or after a malformed read. */
static bool fav_text_ok(const char *s,size_t cap){
    size_t n=strnlen(s,cap);if(!n||n==cap)return false;
    for(size_t i=0;i<n;i++)if((unsigned char)s[i]<32)return false;
    return true;
}
int bt_sd_favorites_load(const char *signature,BtFavorite *items,int capacity){
    if(!ready||!signature||!items||capacity<1||capacity>BTS_MAX_FAVORITES)return 0;
    char path[155],saved[160],header[28];
    snprintf(path,sizeof(path),"%s/dsi-favorites.cfg",folder);
    FILE *f=fopen(path,"rb");if(!f)return 0;
    int n=0,count=0;
    bool ok=fgets(header,sizeof(header),f)!=NULL && !strcmp(header,"BTS_FAVORITES_V1\n") &&
       fgets(saved,sizeof(saved),f)!=NULL;
    if(ok){size_t len=strlen(saved);if(len&&saved[len-1]=='\n')saved[len-1]=0;
        ok=!strcmp(saved,signature);}
    if(ok){ok=fscanf(f,"%d\n",&count)==1&&count>=0&&count<=capacity;}
    while(ok&&n<count){
        BtFavorite item={{0},{0}};
        ok=fread(&item,1,sizeof(item),f)==sizeof(item) &&
           fav_text_ok(item.target,sizeof(item.target)) &&
           fav_text_ok(item.label,sizeof(item.label));
        if(ok)items[n++]=item;
    }
    if(fclose(f)!=0)ok=false;
    return ok?n:0;
}
bool bt_sd_favorites_save(const char *signature,const BtFavorite *items,int count){
    if(!ready||!signature||!fav_text_ok(signature,160)||count<0||count>BTS_MAX_FAVORITES||
       (count&&!items))return false;
    char path[155],tmp[155];
    snprintf(path,sizeof(path),"%s/dsi-favorites.cfg",folder);
    snprintf(tmp,sizeof(tmp),"%s/dsi-favorites.tmp",folder);
    FILE *f=fopen(tmp,"wb");if(!f)return false;
    bool ok=fprintf(f,"BTS_FAVORITES_V1\n%s\n%d\n",signature,count)>0;
    for(int i=0;i<count&&ok;i++){
        ok=fav_text_ok(items[i].target,sizeof(items[i].target))&&
          fav_text_ok(items[i].label,sizeof(items[i].label))&&
          fwrite(&items[i],1,sizeof(items[i]),f)==sizeof(items[i]);
    }
    if(fflush(f)!=0)ok=false;
    if(fclose(f)!=0)ok=false;
    if(!ok){remove(tmp);return false;}
    remove(path);if(rename(tmp,path)!=0){remove(tmp);return false;}
    return true;
}

const char *bt_sd_message(void){return message;}
size_t bt_sd_written(void){return written;}
const char *bt_sd_cache_path(void){return ready?cached:"";}
void bt_sd_abort(void){if(writer){fclose(writer);writer=NULL;}if(ready)remove(pending);}
bool bt_sd_begin(void){
    if(!ready){note("SD NOT AVAILABLE");return false;}
    bt_sd_abort();
    written=0;
    writer=fopen(pending,"wb");
    if(!writer){note("SD INI OPEN FAILED");return false;}
    /* This buffer remains alive until fclose(), including all retries. */
    setvbuf(writer,write_buffer,_IOFBF,sizeof(write_buffer));
    note("DOWNLOADING ECU INI TO SD");return true;
}
bool bt_sd_append(const void *data,size_t count){
    if(!writer){note("SD FILE NOT OPEN");return false;}
    if(count && fwrite(data,1,count,writer)!=count){note("SD WRITE FAILED / CHECK SPACE");return false;}
    written+=count;
    return true;
}
bool bt_sd_end(void){
    if(!writer){note("SD FILE NOT OPEN");return false;}
    int rc=fflush(writer);
    if(fclose(writer)!=0)rc=-1;
    writer=NULL;
    if(rc){note("SD FLUSH FAILED");return false;}
    note("INI SAVED / VALIDATING SD COPY");return true;
}
FILE *bt_sd_open_pending(void){return ready?fopen(pending,"rb"):NULL;}
FILE *bt_sd_open_cached(void){return ready?fopen(cached,"rb"):NULL;}
bool bt_sd_commit(void){
    if(!ready||writer){note("SD FILE NOT COMPLETE");return false;}
    /* Preserve the previous good INI until the replacement is verified.
       FAT may reject rename over an existing destination. */
    FILE *old=fopen(cached,"rb");bool exists=old!=NULL;
    if(old)fclose(old);
    if(exists){
        remove(backup);
        if(rename(cached,backup)!=0){note("SD CACHE BACKUP FAILED");return false;}
    }
    if(rename(pending,cached)!=0){
        if(exists)rename(backup,cached);
        note("SD CACHE REPLACE FAILED");return false;
    }
    if(exists)remove(backup);
    note("ECU INI PARSED / CACHED ON SD");return true;
}

/* Only after a complete INI has been parsed, record what the ECU actually
 * reported so a real hash difference can be diagnosed without guesswork.
 * Never mark an INI valid or authorize writes based on this report. */
bool bt_sd_signature_report(const char *ecu, const char *ini, int http_code){
    if(!ready)return false;
    char path[150],eh[11],ih[11];
    snprintf(path,sizeof(path),"%s/signature-debug.txt",folder);
    FILE *f=fopen(path,"wb");if(!f)return false;
    bool e=bt_signature_hash_copy(ecu,eh),i=bt_signature_hash_copy(ini,ih);
    fprintf(f,"Brick Tuner Suite DSi signature check\n");
    fprintf(f,"ESP32 HTTP response: %d (%s)\n",http_code,http_code==200?"OK":"not OK");
    fprintf(f,"ECU status signature: %s\n",ecu?ecu:"<missing>");
    fprintf(f,"INI definition signature: %s\n",ini?ini:"<missing>");
    fprintf(f,"ECU layout hash: %s\n",e?eh:"<not detected>");
    fprintf(f,"INI layout hash: %s\n",i?ih:"<not detected>");
    fprintf(f,"Layout text match (diagnostic only): %s\n",bt_signature_matches(ecu,ini)?"YES":"NO");
    fprintf(f,"Note: v62 accepts complete parsed INIs independent of signature text.\n");
    fprintf(f,"Note: ECU writes still require an independent identity check.\n");
    return fclose(f)==0;
}

/* Keep the fully downloaded but mismatched source separately for inspection.
 * The unverified file is never used for live decoding or ECU writes. */
bool bt_sd_preserve_unverified(void){
    if(!ready||writer)return false;
    char path[150];
    snprintf(path,sizeof(path),"%s/ecu-unverified.ini",folder);
    remove(path);
    if(rename(pending,path)!=0)return false;
    return true;
}

/* Diagnostic file on the same mounted volume as ecu.ini. No additional mount
 * attempts and no source INI changes. Written only once a failed request ends. */
bool bt_sd_http_report(const char *path,const char *status_line,int code,const char *body){
    if(!ready)return false;
    char name[152];snprintf(name,sizeof(name),"%s/http-debug.txt",folder);
    FILE *f=fopen(name,"wb");if(!f)return false;
    fprintf(f,"Brick Tuner Suite DSi HTTP diagnostic\n");
    fprintf(f,"Request: GET %s\n",path?path:"");
    fprintf(f,"Response: %s\n",status_line?status_line:"");
    fprintf(f,"Status number: %d\n",code);
    fprintf(f,"Response body (first 95 characters): %s\n",body?body:"");
    return fclose(f)==0;
}

/* A parsed INI snapshot avoids re-tokenizing the full definition on each
 * startup. Serialize ONLY pointer-free arrays and a versioned header, never
 * the BtDefinition pointers. Verify CRCs of the full raw INI and metadata
 * before publishing anything. A corrupt/stale snapshot falls back to parsing
 * the valid raw file; it never enables ECU writes by itself. */
typedef struct {
    char magic[8];
    uint32_t format, raw_size, raw_crc, payload_crc;
    uint32_t field_size, map_size, menu_size, ref_size, graph_size, graph_line_size, gauge_size;
    uint32_t channels, constants, maps, menus, refs, pages, graphs, graph_lines, gauges;
    uint32_t big_endian;
    int32_t page_ids[64], page_sizes[64];
    uint8_t page_writeable[64], page_burnable[64];
    char signature[160];
} BtsSnapshotHeader;
static uint32_t bts_hash(uint32_t v,const void *data,size_t len){
    const uint8_t *p=(const uint8_t *)data;
    for(size_t i=0;i<len;i++){v^=p[i];v*=16777619u;}
    return v;
}
static bool bts_raw_identity(uint32_t *size_out,uint32_t *crc_out){
    if(!ready)return false;
    FILE *f=fopen(cached,"rb");if(!f)return false;
    uint8_t buffer[2048];uint64_t count=0;uint32_t crc=2166136261u;
    size_t n;while((n=fread(buffer,1,sizeof(buffer),f))>0){
        count+=n;if(count>2800000){fclose(f);return false;}
        crc=bts_hash(crc,buffer,n);
    }
    bool ok=!ferror(f);
    if(fclose(f)!=0)ok=false;
    if(ok){*size_out=(uint32_t)count;*crc_out=crc;}
    return ok;
}
static bool snap_rw(FILE *f,void *ptr,size_t count,size_t sz,uint32_t *hash,bool writing){
    if(!count)return true;
    if(!ptr||sz==0||count>SIZE_MAX/sz)return false;
    size_t n=count*sz;
    if(writing){
        if(fwrite(ptr,1,n,f)!=n)return false;
    }else if(fread(ptr,1,n,f)!=n)return false;
    *hash=bts_hash(*hash,ptr,n);
    return true;
}
static bool snapshot_arrays(FILE*f,BtDefinition*d,uint32_t *hash,bool write){
    return snap_rw(f,d->channels,(size_t)d->channel_count,sizeof(BtField),hash,write) &&
      snap_rw(f,d->constants,(size_t)d->constant_count,sizeof(BtField),hash,write) &&
      snap_rw(f,d->maps,(size_t)d->map_count,sizeof(BtMap),hash,write) &&
      snap_rw(f,d->menus,(size_t)d->menu_count,sizeof(BtMenu),hash,write) &&
      snap_rw(f,d->refs,(size_t)d->ref_count,sizeof(BtDialogRef),hash,write) &&
      snap_rw(f,d->graphs,(size_t)d->graph_count,sizeof(BtIniGraph),hash,write) &&
      snap_rw(f,d->graph_lines,(size_t)d->graph_line_count,sizeof(BtGraphLine),hash,write) &&
      snap_rw(f,d->gauges,(size_t)d->gauge_count,sizeof(BtGauge),hash,write);
}
bool bt_sd_snapshot_save(const BtDefinition*d){
    if(!ready||!d||!d->valid)return false;
    BtsSnapshotHeader head={0};
    memcpy(head.magic,"BTSMD34",8);head.format=5;
    if(!bts_raw_identity(&head.raw_size,&head.raw_crc))return false;
    head.field_size=sizeof(BtField);head.map_size=sizeof(BtMap);
    head.menu_size=sizeof(BtMenu);head.ref_size=sizeof(BtDialogRef);
    head.graph_size=sizeof(BtIniGraph);head.graph_line_size=sizeof(BtGraphLine);
    head.gauge_size=sizeof(BtGauge);
    head.channels=(uint32_t)d->channel_count;head.constants=(uint32_t)d->constant_count;
    head.maps=(uint32_t)d->map_count;head.menus=(uint32_t)d->menu_count;
    head.refs=(uint32_t)d->ref_count;head.pages=(uint32_t)d->page_count;
    head.graphs=(uint32_t)d->graph_count;head.graph_lines=(uint32_t)d->graph_line_count;
    head.gauges=(uint32_t)d->gauge_count;
    head.big_endian=(uint32_t)d->big_endian;
    memcpy(head.page_ids,d->page_ids,sizeof(head.page_ids));
    memcpy(head.page_sizes,d->page_sizes,sizeof(head.page_sizes));
    for(int i=0;i<64;i++){head.page_writeable[i]=d->page_writeable[i]?1:0;
        head.page_burnable[i]=d->page_burnable[i]?1:0;}
    snprintf(head.signature,sizeof(head.signature),"%s",d->signature);
    char path[155],tmp[155];
    snprintf(path,sizeof(path),"%s/ecu.meta",folder);
    snprintf(tmp,sizeof(tmp),"%s/ecu.meta.tmp",folder);
    FILE *f=fopen(tmp,"wb");if(!f)return false;
    bool ok=fwrite(&head,1,sizeof(head),f)==sizeof(head);
    uint32_t hash=2166136261u;
    if(ok)ok=snapshot_arrays(f,(BtDefinition*)d,&hash,true);
    head.payload_crc=hash;
    if(ok)ok=fseek(f,0,SEEK_SET)==0&&fwrite(&head,1,sizeof(head),f)==sizeof(head);
    if(fflush(f)!=0)ok=false;
    if(fclose(f)!=0)ok=false;
    if(!ok){remove(tmp);return false;}
    /* Snapshot is disposable. A failed rename just means raw INI fallback. */
    remove(path);
    if(rename(tmp,path)!=0){remove(tmp);return false;}
    return true;
}
bool bt_sd_snapshot_load(BtDefinition*d){
    if(!ready||!d)return false;
    uint32_t raw_size=0,raw_crc=0;
    if(!bts_raw_identity(&raw_size,&raw_crc))return false;
    char path[155];snprintf(path,sizeof(path),"%s/ecu.meta",folder);
    FILE *f=fopen(path,"rb");if(!f)return false;
    BtsSnapshotHeader h={0};
    bool ok=fread(&h,1,sizeof(h),f)==sizeof(h);
    if(ok)ok=memcmp(h.magic,"BTSMD34",8)==0&&h.format==5&&
        h.raw_size==raw_size&&h.raw_crc==raw_crc&&
        h.field_size==sizeof(BtField)&&h.map_size==sizeof(BtMap)&&
        h.menu_size==sizeof(BtMenu)&&h.ref_size==sizeof(BtDialogRef)&&
        h.graph_size==sizeof(BtIniGraph)&&h.graph_line_size==sizeof(BtGraphLine)&&
        h.gauge_size==sizeof(BtGauge)&&
        h.channels<=BTS_MAX_CHANNELS&&h.constants<=BTS_MAX_CONSTANTS&&
        h.maps<=BTS_MAX_MAPS&&h.menus<=BTS_MAX_MENUS&&
        h.refs<=BTS_MAX_REFS&&h.graphs<=BTS_MAX_INI_GRAPHS&&
        h.graph_lines<=BTS_MAX_GRAPH_LINES&&h.gauges<=BTS_MAX_GAUGES&&
        h.pages<=64&&h.signature[159]==0&&
        h.signature[0]!=0;
    static BtDefinition t; /* Never put the 8KB line buffer on the ARM9 stack. */
    bt_ini_reset(&t);
    if(ok){
        t.channel_count=(int)h.channels;t.constant_count=(int)h.constants;
        t.map_count=(int)h.maps;t.menu_count=(int)h.menus;t.ref_count=(int)h.refs;
        t.graph_count=(int)h.graphs;t.graph_line_count=(int)h.graph_lines;
        t.gauge_count=(int)h.gauges;
        t.channel_capacity=t.channel_count;t.constant_capacity=t.constant_count;
        t.map_capacity=t.map_count;t.menu_capacity=t.menu_count;t.ref_capacity=t.ref_count;
        t.graph_capacity=t.graph_count;t.graph_line_capacity=t.graph_line_count;
        t.gauge_capacity=t.gauge_count;
        if(t.channel_count)t.channels=malloc((size_t)t.channel_count*sizeof(BtField));
        if(t.constant_count)t.constants=malloc((size_t)t.constant_count*sizeof(BtField));
        if(t.map_count)t.maps=malloc((size_t)t.map_count*sizeof(BtMap));
        if(t.menu_count)t.menus=malloc((size_t)t.menu_count*sizeof(BtMenu));
        if(t.ref_count)t.refs=malloc((size_t)t.ref_count*sizeof(BtDialogRef));
        if(t.graph_count)t.graphs=malloc((size_t)t.graph_count*sizeof(BtIniGraph));
        if(t.graph_line_count)t.graph_lines=malloc((size_t)t.graph_line_count*sizeof(BtGraphLine));
        if(t.gauge_count)t.gauges=malloc((size_t)t.gauge_count*sizeof(BtGauge));
        ok=(!t.channel_count||t.channels)&&(!t.constant_count||t.constants)&&
            (!t.map_count||t.maps)&&(!t.menu_count||t.menus)&&(!t.ref_count||t.refs)&&
            (!t.graph_count||t.graphs)&&(!t.graph_line_count||t.graph_lines)&&
            (!t.gauge_count||t.gauges);
        uint32_t hash=2166136261u;
        if(ok)ok=snapshot_arrays(f,&t,&hash,false)&&hash==h.payload_crc;
        if(ok){
            /* Reject trailing garbage. */
            ok=fgetc(f)==EOF&&!ferror(f);
        }
    }
    if(fclose(f)!=0)ok=false;
    if(ok){
        memcpy(t.page_ids,h.page_ids,sizeof(t.page_ids));
        memcpy(t.page_sizes,h.page_sizes,sizeof(t.page_sizes));
        for(int i=0;i<64;i++){t.page_writeable[i]=h.page_writeable[i]!=0;
            t.page_burnable[i]=h.page_burnable[i]!=0;}
        t.page_count=(int)h.pages;t.current_page=1;t.last_map=-1;t.last_graph=-1;
        t.big_endian=h.big_endian!=0;
        snprintf(t.signature,sizeof(t.signature),"%s",h.signature);
        t.had_constants=true;t.had_outputs=true;t.had_signature=true;
        t.valid=true;
        bt_ini_reset(d);*d=t;
        memset(&t,0,sizeof(t)); /* Ownership transferred to d. */
        note("SD PARSED INI CACHE READY");
        return true;
    }
    bt_ini_reset(&t);
    return false;
}

/* Deliberately separate from ecu.meta: different ECUs can share stored
 * preferences only when the desired runtime channel actually exists. */
static bool gauge_name_valid(const char *s){
    size_t n=strlen(s);
    if(n<1||n>=64)return false;
    for(size_t i=0;i<n;i++){
        char c=s[i];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
             (c>='0'&&c<='9')||c=='_'))return false;
    }
    return true;
}
bool bt_sd_gauges_load(char slots[2][64]){
    if(!ready||!slots)return false;
    char path[160];snprintf(path,sizeof(path),"%s/dsi-gauges.cfg",folder);
    FILE *f=fopen(path,"rb");if(!f)return false;
    char magic[16]={0},one[64]={0},two[64]={0};
    bool ok=fgets(magic,sizeof(magic),f)&&!strcmp(magic,"BTS-GAUGES-1\n")&&
        fgets(one,sizeof(one),f)&&fgets(two,sizeof(two),f);
    fclose(f);
    if(!ok)return false;
    one[strcspn(one,"\r\n")]=0;two[strcspn(two,"\r\n")]=0;
    if(!gauge_name_valid(one)||!gauge_name_valid(two))return false;
    memcpy(slots[0],one,strlen(one)+1);
    memcpy(slots[1],two,strlen(two)+1);
    return true;
}
bool bt_sd_gauges_save(const char slots[2][64]){
    if(!ready||!slots||!gauge_name_valid(slots[0])||!gauge_name_valid(slots[1]))return false;
    char path[160],tmp[160];
    snprintf(path,sizeof(path),"%s/dsi-gauges.cfg",folder);
    snprintf(tmp,sizeof(tmp),"%s/dsi-gauges.tmp",folder);
    FILE *f=fopen(tmp,"wb");if(!f)return false;
    bool ok=fprintf(f,"BTS-GAUGES-1\n%s\n%s\n",slots[0],slots[1])>0;
    if(fflush(f)!=0)ok=false;
    if(fclose(f)!=0)ok=false;
    if(!ok){remove(tmp);return false;}
    remove(path);
    if(rename(tmp,path)!=0){remove(tmp);return false;}
    return true;
}
