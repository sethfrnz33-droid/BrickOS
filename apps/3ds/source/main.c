/* Brick Tuner Suite v4.1: contrast-scaled 3D surface, readable compact UI, 3D HOME banner.
 * Bench-only writes: direct zero-RPM checks, verified ESP32-side backups,
 * signature-bound temporary authorization, verified writes, burn and disarm.
 */
#include <3ds.h>
#include <citro2d.h>
#include "test_confirm.h"
#include "bts_demo_confirm.h"
#include "bts_ini_numeric.h"
#include <3ds/services/soc.h>
#include <3ds/services/ac.h>
#include <3ds/services/ptmu.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>
#include <malloc.h>
#include <sys/stat.h>
#include <dirent.h>
#include <3ds/thread.h>
#include <3ds/synchronization.h>
#include "trigger_preview.h"
#include "gauge_hold.h"
#include "bts_map_ops.h"
#include "bts_burn_plan.h"
#include "bts_log_insights.h"
#include "bts_log_compare.h"
#include "bts_log_quality.h"
#include "bts_cal_snapshot.h"
#include "bts_audio_rules.h"
#include "bts_card_motion.h"

/* Bounded, NUL-terminated copy for INI metadata, log labels and status text.
 * Unlike an unrestricted %s snprintf, the explicit byte limit is understood
 * by the compiler; overlong display strings remain safely truncated. */
static void copy_text(char *dest, size_t cap, const char *src){
 if(!dest || !cap)return;
 if(!src){dest[0]=0;return;}
 size_t n=0;
 while(n+1<cap && src[n])n++;
 memcpy(dest,src,n);
 dest[n]=0;
}

#define INI_CACHE "sdmc:/BrickECU/ecu.ini"
#define INI_TEMP  "sdmc:/BrickECU/ecu.tmp"
#define FAVORITES_CFG "sdmc:/BrickECU/favorites.txt"
#define FAVORITES_CAP 96
#define HOST_IP "192.168.4.1"
#define HOST_PORT 80
#define SOCKET_MEM (1024*1024)
#define RESPONSE_CAP (1600*1024)
#define MENU_CAP 1800
#define FIELD_CAP 2700
#define CHAN_CAP 1500
#define TABLE_CAP 350
#define REF_CAP 6000
#define RES_CAP 500
#define EDIT_CAP 4096
#define LIVE_CAP 3072

/* Field indices are looked up by INI name; never infer ECU offsets. */
typedef struct {char label[68],target[64];int group;} MenuItem;
typedef struct {char name[60],unit[26];int page,offset,width,height,kind,type,bytes,bitlo,bithi;double scale,translate,low,high;bool validScale,limitsKnown,powerCycle,readOnly;int dyn_axis_kind;const char *choice_spec,*source_spec;} Field;
typedef struct {char name[64];const char *value;} MacroDef;
typedef struct {int value;char label[56];} Choice;
typedef struct {char id[64],alt[64],label[76],x[60],y[60],z[60],xchan[60],ychan[60];bool curve,xro,yro;} TableDef;
enum {BTS_REGION_INHERIT=0,BTS_REGION_NORTH=1,BTS_REGION_WEST=2,
      BTS_REGION_CENTER=3,BTS_REGION_EAST=4,BTS_REGION_SOUTH=5};
typedef struct {char owner[64],ref[64],label[60];int panel;unsigned char region;} DialogRef;
typedef struct {char label[72],command[64],info[128],group[68];int field;int type;unsigned char region;} Resolved;

enum {T_U08,T_S08,T_U16,T_S16,T_U32,T_S32,T_F32,T_F64,T_BAD};
enum {K_SCALAR,K_ARRAY,K_BITS};
enum {PAGE_CATS,PAGE_MENU,PAGE_DETAIL,PAGE_FIELD,PAGE_DIAG,PAGE_MAPS,PAGE_GROUP,PAGE_CHOICES,PAGE_DISPLAY,PAGE_GAUGE_PICK,PAGE_GRAPH,PAGE_FAVORITES,PAGE_SEARCH,PAGE_RECORDER,PAGE_VIEWS,PAGE_KNOCK,PAGE_DASH,PAGE_LOG_BROWSER,PAGE_REPLAY,PAGE_CHANNELS,PAGE_REPLAY_STATS,PAGE_INI_GRAPH,PAGE_TRIGGER_SIM,PAGE_FAULT,PAGE_TRACE_MAPS,PAGE_INSIGHTS,PAGE_COMPARE_PICK,PAGE_COMPARE,PAGE_QUALITY,PAGE_HUB,PAGE_TUNE_HUB,PAGE_LOGS_HUB,PAGE_ANALYZE_HUB,PAGE_SETTINGS_HUB,PAGE_TUNE_TREE,PAGE_TUNE_SECTION,PAGE_SOUND_TEST};
#define INI_GRAPH_CAP 192
#define INI_GRAPH_LINES 6
typedef struct {char owner[64],id[64],title[64],channels[INI_GRAPH_LINES][64];int count;} IniGraphDef;
static IniGraphDef ini_graphs[INI_GRAPH_CAP];
static int ini_graph_count=0,ini_graph_selected=-1,ini_graph_return=PAGE_DETAIL,ini_graph_return_selected=0;
static float ini_graph_values[INI_GRAPH_LINES][240];
static bool ini_graph_ok[INI_GRAPH_LINES][240];
typedef struct {char name[64],channel[64],label[64],minimum[96],maximum[96];} GaugeDef;
static GaugeDef gauges[512];static int gauge_def_count=0;
static MenuItem menus[MENU_CAP];static int menu_count=0;
/* Optional TunerStudio [SettingContextHelp] descriptions from the ECU's own
 * INI. Store by exact field/reference key; do not invent ECU documentation. */
#define INI_HELP_CAP 1024
typedef struct {char key[64];char description[320];} IniSettingHelp;
static IniSettingHelp ini_setting_help[INI_HELP_CAP];
static int ini_setting_help_count=0;
typedef struct {char target[64],label[68];} Favorite;
static Favorite favorites[FAVORITES_CAP];static int favorite_count=0;
#define SEARCH_CAP (MENU_CAP+TABLE_CAP)
typedef struct {int type,index;} SearchHit;
static SearchHit search_hits[SEARCH_CAP];static int search_count=0;
static char search_query[48]="";static bool search_origin=false,favorites_origin=false;
static Field cal[FIELD_CAP], channels[CHAN_CAP];static int cal_count=0,chan_count=0;
static TableDef tables[TABLE_CAP];static int table_count=0;
static DialogRef refs[REF_CAP];static int ref_count=0;
static Resolved results[RES_CAP];static int result_count=0;
/* Use actual [UserDefined] dialog captions, not ambiguous field labels, to
 * reconstruct TunerStudio's panel grouping on the dual screens. */
#define DIALOG_CAPTION_CAP 768
typedef struct {char id[64],title[68];} DialogCaption;
/* TunerStudio indicatorPanel/readoutPanel are not ordinary dialogs. */
#define BTS_INDICATOR_CAP 256
typedef struct {char channel[64],off[72],on[72];} BtsIniIndicator;
static BtsIniIndicator ini_indicators[BTS_INDICATOR_CAP];
static int ini_indicator_count=0;
static int bts_ini_region(const char *name){
 if(!name||!*name)return BTS_REGION_INHERIT;
 if(!strcasecmp(name,"North"))return BTS_REGION_NORTH;
 if(!strcasecmp(name,"West"))return BTS_REGION_WEST;
 if(!strcasecmp(name,"Center"))return BTS_REGION_CENTER;
 if(!strcasecmp(name,"East"))return BTS_REGION_EAST;
 if(!strcasecmp(name,"South"))return BTS_REGION_SOUTH;
 return BTS_REGION_INHERIT;
}
static DialogCaption dialog_captions[DIALOG_CAPTION_CAP];
static int dialog_caption_count=0;
static const char *dialog_caption(const char *id){
 for(int i=0;i<dialog_caption_count;i++)
   if(!strcasecmp(dialog_captions[i].id,id))return dialog_captions[i].title;
 return NULL;
}
static int page_ids[64], page_sizes[64],page_ids_count=0,live_length=256,base_live_length=256;
static bool page_burnable[64]={false},page_writeable[64]={false};
static char ini_signature[160]="",ecu_signature[160]="",menu_info[80]="No INI loaded";
static bool menu_loaded=false,ini_big_endian=false,ecu_session=false;
/* We retain the parsed INI text so option and #define declarations keep their
 * original exact numeric meanings; never substitute generic pin selections. */
static char *ini_source=NULL;
static MacroDef macro_defs[160];static int macro_count=0;
#define CYCLE_CAP 128
static char power_cycle_names[CYCLE_CAP][64];static int power_cycle_count=0;
#define INI_READONLY_CAP 192
static char ini_readonly_names[INI_READONLY_CAP][64];
static int ini_readonly_count=0;
static Choice choices[512];static int choice_count=0;
static int group_item=-1,group_parent_selected=0;
/* One continuous whole-map editor: no split overview/zoom interaction. */
static bool map_text_dirty=true;
static C2D_TextBuf map_textbuf;
static C2D_Text map_texts[2048];
static uint8_t map_chars[2048];
static int map_text_count=0;
/* Non-actuating waveform illustration. Keep preview state isolated from ECU.
 * No simulator command and no calibration change is sent to the firmware. */
static bool stim_preview=false;
static u32 stim_preview_presses=0;
static int stim_rpm=800,stim_wheel=0;
static u64 stim_anchor_time=0;
static double stim_anchor_angle=0.0;
static int stim_return_page=PAGE_DETAIL;
static int current_table=-1,axis_field[2]={-1,-1},axis_len[2]={0,0},axis_tries[2]={0,0};
static bool axis_ready[2]={false,false};
static uint8_t axis_data[2][512],axis_original[2][512];
static bool axis_readonly[2]={false,false};
/* 0 = Z map, 1 = X header, 2 = Y header; axes share edit controls. */
static int map_focus=0,axis_cursor[2]={0,0};
/* A genuine three-dimensional height-field built from actual INI Z cells.
 * Circle pad rotates only the view: it can NEVER modify calibrations. */
static bool map_3d_view=false;
static float map_3d_yaw=-0.64f,map_3d_pitch=0.70f;
/* Tilt full range: +90 deg is straight down, -90 deg is underside. */
#define MAP3D_PITCH_MAX 1.57079632679f
#define MAP3D_PITCH_MIN (-1.57079632679f)
static u64 tune_last_activity=0;
static bool tune_screen_asleep=false;

static C2D_Text axis_texts[2][256];
static uint8_t axis_chars[2][256];
static int axis_text_count[2]={0,0};
static int ui_page=PAGE_HUB,category=0,selected=0,scroll_y=0,detail_sel=0;
static int tune_bucket=0,tune_section=0,menu_return_page=PAGE_CATS,maps_return_page=PAGE_TUNE_HUB;
/* True two-screen TunerStudio flow: top category -> bottom preview/focus -> top editor. */
static bool tune_lower_focus=false,tune_from_tree=false;
static int tune_lower_selected=0,tune_lower_scroll=0,tune_open_group=-1;
static volatile bool power_cycle_popup=false; /* persistent until A acknowledges; never guessed from link */
/* Per-screen navigation bookmarks: returning from an editor/graph opens at the
 * exact selected entry and scroll position, not row zero. */
#define PAGE_COUNT (PAGE_TUNE_SECTION + 1)
static struct { int selected,scroll; bool valid; } nav_bookmark[PAGE_COUNT];
static void nav_capture(void){
 if(ui_page>=0&&ui_page<PAGE_COUNT){
   nav_bookmark[ui_page].selected=selected;
   nav_bookmark[ui_page].scroll=scroll_y;
   nav_bookmark[ui_page].valid=true;
 }
}
static void nav_restore(void){
 if(ui_page>=0&&ui_page<PAGE_COUNT&&nav_bookmark[ui_page].valid){
   selected=nav_bookmark[ui_page].selected;
   scroll_y=nav_bookmark[ui_page].scroll;
 }
}
static int views_return_page=PAGE_CATS,knock_bank=0;
/* Explicit workspace hierarchy. Children remember where the user opened them. */
static int log_browser_parent=PAGE_LOGS_HUB;
static int favorites_parent=PAGE_TUNE_HUB;
static int search_parent=PAGE_TUNE_HUB;
static int display_parent=PAGE_SETTINGS_HUB;
static int analysis_return_page=PAGE_REPLAY;
static int quality_return_page=PAGE_INSIGHTS;
static int compare_return_page=PAGE_REPLAY;
static int trace_return_page=PAGE_REPLAY;
static int graph_return_page=PAGE_LOGS_HUB;
static int channels_return_page=PAGE_SETTINGS_HUB;
static int knock_return_page=PAGE_SETTINGS_HUB;
static int cats_return_page=PAGE_SETTINGS_HUB;
static int stats_return_page=PAGE_REPLAY;
static bool sfx_enabled=true,sfx_ready=false;
/* Menu beeps are separate from the requested event clips. Older saved
   sfx=0 must never silently disable ECU events. */
static bool event_audio_enabled=true;
static bool bts_ndsp_active=false;
static Result bts_ndsp_result=0;
static int sound_test_last=-1;
static bool sound_test_queued=false;
static bool workspace_page(int page){
 return page==PAGE_HUB||page==PAGE_TUNE_HUB||page==PAGE_LOGS_HUB||
        page==PAGE_ANALYZE_HUB||page==PAGE_SETTINGS_HUB;
}
static int workspace_count(int page){
 switch(page){
 case PAGE_HUB:return 4;
 case PAGE_TUNE_HUB:return 4;
 case PAGE_LOGS_HUB:return 3;
 case PAGE_ANALYZE_HUB:return 5;
 case PAGE_SETTINGS_HUB:return 4;
 default:return 0;
 }
}
static const char* workspace_title(int page){
 switch(page){
 case PAGE_HUB:return "BRICK TUNER SUITE";
 case PAGE_TUNE_HUB:return "TUNE / EDIT & BURN";
 case PAGE_LOGS_HUB:return "LOGS / RECORD & REPLAY";
 case PAGE_ANALYZE_HUB:return "ANALYZE / RECORDED DATA";
 case PAGE_SETTINGS_HUB:return "SETTINGS & DIAGNOSTICS";
 default:return "";
 }
}
static const char* workspace_action(int page,int index){
 static const char* root[]={"Tune","Analyze","Logging","DS Settings"};
 static const char* tune[]={"TunerStudio Categories","Open ECU Tree","Browse ECU Tree","Return To ECU Tree"};
 static const char* logs[]={"Record Live Log","Browse Saved Logs","Resume Loaded Replay"};
 static const char* analysis[]={"Analyze Loaded Log","Compare Two Logs","Trace Log to Map","Check Log Quality","Log Statistics"};
 static const char* settings[]={"Display & Gauges","Wi-Fi & ECU Connection","Menu Sound On / Off","Soundboard / Event Sounds"};
 if(index<0||index>=workspace_count(page))return "";
 if(page==PAGE_SETTINGS_HUB && index==2)return sfx_enabled?"Menu Sound: ON":"Menu Sound: OFF";
 switch(page){
 case PAGE_HUB:return root[index];case PAGE_TUNE_HUB:return tune[index];
 case PAGE_LOGS_HUB:return logs[index];case PAGE_ANALYZE_HUB:return analysis[index];
 case PAGE_SETTINGS_HUB:return settings[index];default:return "";
 }
}


#define SFX_LENGTH 3600
static s16 *sfx_samples=NULL;
static ndspWaveBuf sfx_buffers[3];
static u64 sfx_last_at=0;
#include "bts_event_audio.h"
static BtsAudioLevel bts_boost_level={0},bts_coolant_level={0};
static u64 bts_context_last_at[BTS_SOUND_COUNT]={0};
static bool bts_context_play(int cue){
 if(cue<0||cue>=BTS_SOUND_COUNT)return false;
 u64 now=osGetTime();
 /* Human actions only; prevent repeated popup/navigation/step sounds. */
 if(bts_context_last_at[cue] && now-bts_context_last_at[cue]<2500ULL)return false;
 bool queued=bts_audio_play(cue,event_audio_enabled);
 if(queued)bts_context_last_at[cue]=now?now:1;
 return queued;
}
static void bts_context_named(const char *label,const char *target){
 bts_context_play(bts_sound_for_entry(label,target));
}

/* Burn audio starts when the user confirms Burn, loops for all preflight,
 * write, flash and readback phases, and stops on every task outcome. */
/* Recorded knock values, never generated or interpolated to look live. */
#define KNOCK_HISTORY 180
static float knock_history[KNOCK_HISTORY];
static u32 knock_history_ms[KNOCK_HISTORY];
static bool knock_history_valid[KNOCK_HISTORY];
static int knock_head=0,knock_count=0;

static bool detail_from_maps=false;
static int field_return_page=PAGE_MENU;
static int open_field=-1,cell=0,grid_first_col=0,grid_first_row=0;
static char open_title[80]="",ui_note[160]="Automatic INI loading";
static uint8_t editor[EDIT_CAP],original[EDIT_CAP];static int editor_length=0;
/* In-RAM field edit undo/redo. Snapshots never write to ECU. */
#define EDIT_UNDO_CAP 16
typedef struct {uint8_t z[EDIT_CAP],axis[2][512];int zlen,alen[2];} EditSnapshot;
static EditSnapshot edit_undo[EDIT_UNDO_CAP];
static int edit_undo_count=0,edit_undo_at=-1;
static EditSnapshot edit_undo_before;
/* Map Tools operate ONLY on the local editable copy. No ECU communication. */
static bool map_tools_open=false,map_anchor_valid=false,map_diff_mode=false;
static int map_anchor=0,map_tool_cursor=0,map_tool_scroll=0;
#define MAP_TOOL_COUNT 18
static const char *map_tool_names[MAP_TOOL_COUNT]={
 "Mark corner / select region","Clear range selection","Increase by step",
 "Decrease by step","Increase 5 percent","Decrease 5 percent",
 "Interpolate horizontally","Interpolate vertically","Smooth 4 neighbors",
 "Bilinear fill (4 corners)","Restore original cells","Copy selected range","Paste at selected cell",
 "Show / hide differences","Undo local edit","Redo local edit",
 "Save field snapshot to SD","Compare latest field snapshot"
};
static double map_values_before[EDIT_CAP],map_values_after[EDIT_CAP];
static double map_clipboard[EDIT_CAP];static int map_clip_w=0,map_clip_h=0;
/* Read-only local comparison against an exact ECU-signature/INI-field snapshot.
 * The historical snapshot never becomes an ECU write command. */
static uint8_t snapshot_reference[EDIT_CAP];
static bool snapshot_valid=false,snapshot_diff_mode=false;
static int snapshot_ref_number=0;


static bool editor_loaded=false,staged=false,confirm_write=false,sd_backup=false,confirm_discard=false;
#define TUNE_STEP_COUNT 6
static int step_index=3;static const double steps[]={0.001,0.01,0.1,1.0,10.0,100.0};
static char net_status[160]="Connecting to saved Wi-Fi...";
static char wifi_ssid[33]="(none)";
static bool ac_ok=false,wifi_ok=false,wifi_busy=false,wifi_candidate=false,esp_status_valid=false;
/* PTMU supplies an ordinal 0..5 battery level, NOT a percentage. */
static bool ptmu_ok=false,battery_known=false,battery_charging=false;
static u8 battery_level=0;
static u64 battery_checked_at=0;
static void update_system_battery(u64 now){
 if(!ptmu_ok || (battery_checked_at&&now-battery_checked_at<5000))return;
 battery_checked_at=now;
 u8 level=0,charging=0;
 if(R_SUCCEEDED(PTMU_GetBatteryLevel(&level))&&level<=5){
    battery_level=level;battery_known=true;
 }else battery_known=false;
 battery_charging=R_SUCCEEDED(PTMU_GetBatteryChargeState(&charging))&&charging!=0;
}
static int wifi_slot=-1,wifi_last_error=0;
static u32 wifi_status=0,wifi_type=0,esp_wifi_clients=0,esp_disconnect_reason=0;
static u32 ac_last_error=0,ac_last_detail_error=0;
static u64 wifi_last_attempt=0;
static bool net_ok=false,live_valid=false,write_compiled=false,write_armed=false;
static bool stim_stop_supported=false, stim_start_supported=false;
static char stim_server_state[48]="UNKNOWN";
static char stim_token[12]="";
static char stim_failure[154]="";
enum {STIM_NONE,STIM_PREPARE,STIM_START,STIM_STOP};
static int stim_pending=STIM_NONE;
static bool stim_on_by_bridge=false;
static double stim_last_rpm=0,stim_last_vbat=0;
/* Overlay may acknowledge hazard; no actuator-start command is wired. */
static BrickTestConfirm hazardous_test={0};
static bool esp_backup_ready=false,esp_write_access_ready=false;
static char esp_firmware_build[32]="unknown";
static u32 live_seq=0;static int last_http=0,bytes_rx=0,live_bytes=0;
static uint8_t live_data[LIVE_CAP];static u64 live_received_at=0;
static float displayed[9];static bool has_value[9];
static const char *metric_labels[]={"RPM","MAP","AFR","TPS","COOLANT","INTAKE","BATTERY","IGNITION","OIL PRESS"};
static const char *metric_fields[]={"RPMValue","MAPValue","AFRValue","TPSValue","coolant","intake","VBatt","ignitionAdvanceCyl1","oilPressure"};
static u32 *soc_mem=NULL;static char *response=NULL;
static C2D_TextBuf textbuf;static C3D_RenderTarget *top,*bottom,*top_right;
/* Keep both upper-screen render targets valid for 3DS hardware.
 * Tuning interface is two-dimensional and does not need fake 3D geometry. */
static int stereo_eye=0;static float stereo_strength=0.0f;
/* Native accelerometer = gravity; native gyroscope = angular impulse. One
 * physical spring per gauge, stepped once per frame, never once per eye. */
static BTSCardWorld bts_card_world={0};
static bool bts_top_panel_3d=false;
static bool bts_accel_ok=false,bts_gyro_ok=false;
static float bts_gyro_dps_per_raw=0.07f;
static u64 bts_card_last_at=0;

static bool connected_sfx_played=false;
/* Icon atlas remains packaged for older utility screens; navigation uses vectors. */
extern const uint8_t dreamfont_start[],dreamfont_end[];
static C2D_SpriteSheet dreamfont_sheet=NULL;
extern const uint8_t ts_icons_start[],ts_icons_end[];
static C2D_SpriteSheet ts_icon_sheet=NULL;
#include "bts_mascot.h"
static u64 pad_next_at=0;
#define LIVE_STALE_AFTER_MS 900ULL
#define GAUGE_SLOTS 9
#define GRAPH_SAMPLES 240
#define DISPLAY_CFG "sdmc:/BrickECU/dreamtuner.cfg"
static char gauge_names[GAUGE_SLOTS][64];
static int gauge_fields[GAUGE_SLOTS],gauge_style=0,gauge_pick_slot=0,display_return_page=PAGE_CATS;
static int gauge_decimals[GAUGE_SLOTS]={-1,-1,-1,-1,-1,-1,-1,-1,-1}; /* -1 AUTO, otherwise 0..3 */
static int selectable_channels[CHAN_CAP],selectable_count=0;
static bool gauge_prefs_loaded=false,view_has[GAUGE_SLOTS];
/* Historical per-channel values from VALID incoming ECU samples only. */
typedef struct {float minimum,maximum;double mean;uint32_t count;} GaugeCapture;
static GaugeCapture gauge_capture[GAUGE_SLOTS];
static int gauge_stat_view=0; /* 0 live, 1 maximum, 2 minimum, 3 mean */
static void gauge_stats_reset(int i){
 if(i<0||i>=GAUGE_SLOTS)return;
 gauge_capture[i]=(GaugeCapture){0};
}
static void gauge_stats_accept(int i,float v){
 if(i<0||i>=GAUGE_SLOTS||!isfinite(v))return;
 GaugeCapture *s=&gauge_capture[i];
 if(s->count==0)s->minimum=s->maximum=v;
 if(v<s->minimum)s->minimum=v;
 if(v>s->maximum)s->maximum=v;
 if(s->count<UINT32_MAX){s->count++;s->mean+=(v-s->mean)/(double)s->count;}
}

static float view_values[GAUGE_SLOTS];
static bool needle_ready[GAUGE_SLOTS];
static float graph_values[GAUGE_SLOTS][GRAPH_SAMPLES];
static bool graph_ok[GAUGE_SLOTS][GRAPH_SAMPLES];
static u32 graph_ms[GRAPH_SAMPLES];
static int graph_head=0,graph_count=0,graph_seconds=10,graph_bank=0;
static u32 graph_last_sample_ms=0; /* 90 ms per chart point preserves full 20-second window */
static u64 rate_start_ms=0;
static u32 rate_count=0;
static float actual_hz=0;
static const char *gauge_styles[]={"FOUR FOCUS", "THREE LARGE", "PRIMARY + FOUR", "SIX CHANNELS", "NINE CHANNELS", "ONE LARGE", "TWO LARGE"};

static u32 bg,panel,panel2,white,secondary,green,blue,orange,red,outline;
static u32 soil,leaf,petal,berry,cream,dew,bright,muted_leaf;
/* Compact RAM logger: no SD write or file I/O on the telemetry hot path. */
#define LOG_MAX_SAMPLES 8192
#define LOG_FIELDS 9
typedef struct {u32 t_ms;float values[LOG_FIELDS];u16 valid;} LogSample;
static LogSample log_buffer[LOG_MAX_SAMPLES];
/* Replay has separate RAM storage: opening logs cannot overwrite a recording. */
static LogSample replay_buffer[LOG_MAX_SAMPLES];
static int replay_count=0;
static int log_count=0;static bool log_recording=false;
static u64 log_begin_ms=0;static char log_export_path[80]="Not yet exported";
static char log_channel_names[LOG_FIELDS][64];
static char record_ini_signature[160]="",record_ecu_signature[160]="";
static char record_bridge_build[32]="";
static bool record_session_connected=false;
/* On-device log browser + playback: read-only; no ECU write path. */
#define LOG_FILE_CAP 96
static char log_filenames[LOG_FILE_CAP][48];
static int log_file_count=0,log_browser_selected=0,log_browser_scroll=0;
static bool log_delete_pending=false;
static char log_delete_name[48]="";
static char replay_filename[48]="",replay_info[95]="No log selected";
static int replay_position=0;static u32 replay_time_ms=0;
static u64 replay_wall_ms=0;static bool replay_playing=false;
static int replay_speed=1,replay_stat_cursor=0;
/* Compare two REAL SD CSVs, aligning each recording from its own first sample.
 * Stored separately from the active replay. Never touches live ECU or writes. */
static LogSample *compare_buffer=NULL;
static int compare_count=0,compare_channel=0,compare_window=0;
static char compare_filename[48]="",compare_names[LOG_FIELDS][64];
static int compare_map[LOG_FIELDS];
static BtsCompareStats compare_stats[LOG_FIELDS];
static int compare_pick_selected=0;
static int compare_ecu_identity=0; /* 0 missing, 1 equal, 2 conflicting */

/* Standalone read-only analysis mode: no auto-tuning or ECU writes. */
static int insight_channel=0,insight_window=0,insight_view=0;
static bool insight_dirty=true,insight_valid=false;
static BtsLogInsights insight_stats;
static float insight_flat_values[LOG_MAX_SAMPLES];
static uint32_t insight_flat_times[LOG_MAX_SAMPLES];
static uint8_t insight_flat_valid[LOG_MAX_SAMPLES];
static double insight_scratch[LOG_MAX_SAMPLES];
static int insight_source_start=0,insight_source_count=0;
/* Offline recorded sample quality; not an ECU fault diagnostic. */
static BtsSignalQuality log_quality[LOG_FIELDS];
static int quality_channel=0,quality_valid_channels=0;

/* Bookmarks are explicit log positions, persisted in a harmless .marks sidecar. */
#define LOG_MARK_CAP 64
static u32 replay_marks[LOG_MARK_CAP];
static int replay_mark_count=0;
/* Trace Mode maps only fields whose X/Y source names EXACTLY match recorded
 * channel identifiers; never invent or guess RPM/load calibration mappings. */
static int trace_maps[TABLE_CAP],trace_map_count=0;
static int trace_log_x=-1,trace_log_y=-1;
static bool replay_trace_active=false,replay_trace_centered=false;
static u32 replay_trace_at=0;
/* Incremental map-usage heatmap built only from valid recorded samples.
 * 128 log samples per frame caps work on the original 3DS CPU. */
static u16 trace_coverage[EDIT_CAP];
static int trace_coverage_at=0,trace_valid_samples=0,trace_coverage_peak=0;
static bool trace_show_coverage=false,trace_coverage_done=false;


typedef struct {double min,max,mean;u32 count;} ReplayStat;
static ReplayStat replay_stats[LOG_FIELDS];
static int channel_cursor=0,channel_scroll=0;
static bool live_stale=false;
static int tracex=-1,tracey=-1;
static double trace_raw_x=NAN,trace_raw_y=NAN;
static float trace_interp_x=0,trace_interp_y=0;
static bool trace_found=false;
/* The UI never waits for HTTP; worker holds the shared HTTP response until
 * the main thread consumes its result. LightLock protects task handoff. */
enum {TASK_IDLE,TASK_STATUS,TASK_INI,TASK_LIVE,TASK_CAL,TASK_PANEL_PREVIEW,TASK_AXIS_X,TASK_AXIS_Y,TASK_WRITE,TASK_RPM_LIMIT,TASK_INI_FLAGS,
      TASK_STIM_PREPARE,TASK_STIM_START,TASK_STIM_STOP,TASK_ERROR,TASK_COMMAND_INSPECT,TASK_TEST_ABORT,TASK_SENSOR_CAPTURE};
static LightLock task_lock;
static volatile int task_state=0; /* 0 free, 1 pending, 2 busy, 3 completed */
static int task_type=TASK_IDLE,task_result=-1,task_page=0,task_offset=0,task_len=0,task_field=-1;
static uint8_t task_calibration[EDIT_CAP];
/* Up to three genuine ECU map/curve payloads, read-only and independent
 * of staged tuning edits. Never invent cells before a successful USB read. */
#define BTS_PANEL_PREVIEW_COUNT 3
typedef struct {
 int field,length,attempts;
 bool ready;
 uint8_t bytes[EDIT_CAP];
} BtsPanelPreview;
static BtsPanelPreview panel_previews[BTS_PANEL_PREVIEW_COUNT];
static void bts_panel_preview_init(void){
 for(int j=0;j<BTS_PANEL_PREVIEW_COUNT;j++){
    panel_previews[j].field=-1;
    panel_previews[j].length=0;
    panel_previews[j].attempts=0;
    panel_previews[j].ready=false;
 }
 for(int i=0;i<result_count;i++){
    if(results[i].type!=1&&results[i].type!=5)continue;
    int field=results[i].field;
    if(field<0||field>=cal_count)continue;
    const Field *f=&cal[field];
    if(f->bytes<1||f->bytes>EDIT_CAP||f->kind!=K_ARRAY)continue;
    bool exists=false;
    for(int j=0;j<BTS_PANEL_PREVIEW_COUNT;j++)
        if(panel_previews[j].field==field)exists=true;
    if(exists)continue;
    for(int j=0;j<BTS_PANEL_PREVIEW_COUNT;j++)
        if(panel_previews[j].field<0){
            panel_previews[j].field=field;
            panel_previews[j].length=f->bytes;
            break;
        }
 }
}
/* Metadata inspection only; no actuator commands or payload bytes. */
static bool command_inspect_pending=false,command_inspect_defined=false,command_inspect_visible=false;
static bool test_abort_pending=false;
static char test_abort_error[160]="";
static bool sensor_capture_pending=false,sensor_capture_inflight=false;
static bool sensor_capture_confirmation=false;
static char sensor_capture_command[64]="",sensor_capture_error[160]="";
static char command_inspect_name[64]="";
/* Multi-step, offline-safe UI demo for TunerStudio command buttons.
 * The demo never sends an ECU output command. */
static BtsDemoConfirm demo_confirm={0};
static char demo_command[64]="",demo_label[72]="";
static u64 demo_started_ms=0;
static unsigned demo_completed_count=0;
static volatile bool worker_stop=false,worker_reconnect=false;
static Thread worker=NULL;
static bool ini_refresh=false,status_requested=true,cal_requested=false;
static u64 ini_retry_at=0,live_last_enqueue=0,status_last_enqueue=0;
/* Faults originate from rusEFI retrieveConfigError = "e". No network/INI
 * failure may clear an ECU fault; only two successful empty replies do. */
#define BRICK_FAULT_MAX 384
#define BRICK_FAULT_LINK_CAP 6
typedef struct {int field,menu;char label[64];} BrickFaultLink;
static BrickFaultLink fault_links[BRICK_FAULT_LINK_CAP];
static int fault_link_count=0,fault_return_page=PAGE_CATS,fault_clear_count=0;
static char fault_candidate[BRICK_FAULT_MAX]="";
static int fault_candidate_count=0;
static char fault_message[BRICK_FAULT_MAX]="",fault_task_message[BRICK_FAULT_MAX]="";
static char fault_ecu_signature[160]="";
static bool fault_active=false,fault_poll_requested=true;
/* Reconnection requires fresh ECU confirmation: no cached error is presumed
 * resolved and no failed query may clear a genuine ECU error. */
static bool fault_recheck_after_reconnect=false,fault_wifi_seen=false;
/* A wireless interruption or an ECU-session disconnect invalidates the
 * previous session's fault once a NEW connected status is confirmed. */
static bool fault_session_was_lost=false;
static u64 fault_last_enqueue=0,fault_last_success=0;
static void fault_rebuild_links(void);

static int pending_cal_field=-1;
static bool ini_cached_on_sd=false,work_suspended=false;
/* Resolve TunerStudio conversion expressions from actual ECU flags, not
 * guessed defaults. The worker reads flags; only UI thread changes fields. */
static BtsIniFlags ini_flags={0},task_ini_flags={0};
static bool ini_flags_requested=false;
static u64 ini_flags_retry_at=0;
static char ini_flags_signature[160]="";
/* A write is placed ahead of telemetry polling. The queue only runs following
 * deliberate START -> A confirmation and remains subject to bench interlocks. */
static bool write_queued=false,write_inflight=false,write_verified=false;
static bool rpm_limit_requested=false,rpm_limit_known=false;
static u64 rpm_limit_retry_at=0;
static double rpm_limit_value=0;
static u32 successful_live=0,live_errors=0;static int live_http_code=0;


/* Small annotations in the original UI were below readable 3DS scale.
 * Set a minimum font size and clip a label with an ellipsis rather than
 * scaling it back to unreadable pixels. This is a visual-only change. */
static float bts_text_screen_width=400.f;
#define BTS_MIN_TEXT_SCALE .43f
static void text(float x,float y,float sz,u32 c,const char *s){
 if(!s||!(*s))return;
 if(sz<BTS_MIN_TEXT_SCALE)sz=BTS_MIN_TEXT_SCALE;
 C2D_Text t;C2D_TextParse(&t,textbuf,s);C2D_TextOptimize(&t);
 float limit=(bts_text_screen_width>0.f?bts_text_screen_width:400.f)-x-3.f;
 if(limit<3.f)return;
 float tw=0,th=0;
 C2D_TextGetDimensions(&t,sz,sz,&tw,&th);
 if(tw>limit){
    /* Trim whole UTF-8 characters; never split a multibyte INI label. */
    char clipped[256];size_t len=strlen(s);
    if(len>sizeof(clipped)-4)len=sizeof(clipped)-4;
    size_t estimated=(size_t)((float)len*limit/tw);
    if(estimated<len)len=estimated;
    while(len>0&&(((unsigned char)s[len]&0xc0)==0x80))len--;
    bool fits=false;
    while(len>0){
       memcpy(clipped,s,len);
       memcpy(clipped+len,"...",4);
       C2D_TextParse(&t,textbuf,clipped);C2D_TextOptimize(&t);
       C2D_TextGetDimensions(&t,sz,sz,&tw,&th);
       if(tw<=limit){fits=true;break;}
       len--;
       while(len>0&&(((unsigned char)s[len]&0xc0)==0x80))len--;
    }
    if(!fits)return;
 }
 C2D_DrawText(&t,C2D_WithColor,x,y,0,sz,sz,c);
}
static void rect(float x,float y,float w,float h,u32 c){C2D_DrawRectSolid(x,y,0,w,h,c);}
static void bts_rounded_solid(float x,float y,float w,float h,float radius,
                              u32 face,u32 bezel,float depth,float skew);
/* Fast GPU-friendly rounded rectangles: 3 horizontal bands, clipped corners.
 * All four corner radii are represented at the native 320px/400px scale. */
static void round_panel(float x,float y,float w,float h,float radius,u32 c){
 /* Universal actual rounded prism on the upper screen.  Small decorative
  * strokes and the lower touchscreen remain flat and precise to click. */
 if(bts_top_panel_3d && w>=85.f && h>=22.f && c!=outline){
    u32 translucent=(c&0x00FFFFFFu)|((u32)170<<24);
    bts_rounded_solid(x,y,w,h,radius,translucent,
      C2D_Color32(56,75,89,172),2.8f,bts_card_world.gravity_x*.8f);
    return;
 }
 if(w<2*radius||h<2*radius){rect(x,y,w,h,c);return;}
 rect(x+radius,y,w-2*radius,h,c);
 rect(x,y+radius,w,h-2*radius,c);
 rect(x+radius*.46f,y+radius*.27f,w-radius*.92f,radius*.73f,c);
 rect(x+radius*.46f,y+h-radius,w-radius*.92f,radius*.73f,c);
 rect(x+radius*.76f,y,w-radius*1.52f,radius*.27f,c);
 rect(x+radius*.76f,y+h-radius*.27f,w-radius*1.52f,radius*.27f,c);
}

/* Dark perspective grid: background rays converge at a distant horizon.
 * Background parallax is deliberately behind the floating gauge cards.
 * Sparse lines keep the original 3DS fill-rate and text legibility intact. */
/* On the 3DS, right-eye imagery LEFT of left-eye imagery is perceived in
 * front of the glass; the reverse appears behind it. Keep depth roles
 * explicit so menus and instrumentation do not accidentally invert. */
static float bts_stereo_shift(float pixels){
 return (stereo_eye?1.f:-1.f)*stereo_strength*pixels;
}
static float bts_front(float pixels){return -bts_stereo_shift(pixels);}
static float bts_rear(float pixels){return bts_stereo_shift(pixels);}

/* Universal rounded-rect solid made of triangles, not a stretched texture.
 * Every corner uses the exact radius supplied in 3DS pixels, regardless of
 * the card width/height (nine-slice geometry). A front face, curved bezel,
 * and a rear body provide visible thickness under the stereo slider. */
#define BTS_PRISM_VERTS 20
static void bts_rounded_outline(float x,float y,float w,float h,float r,
                                float px[BTS_PRISM_VERTS],float py[BTS_PRISM_VERTS]){
 float centers[4][2]={{x+w-r,y+r},{x+w-r,y+h-r},{x+r,y+h-r},{x+r,y+r}};
 for(int c=0;c<4;c++)for(int j=0;j<5;j++){
   float a=-1.57079632679f+(float)c*1.57079632679f+(float)j*.39269908170f;
   int i=c*5+j;
   px[i]=centers[c][0]+r*cosf(a);
   py[i]=centers[c][1]+r*sinf(a);
 }
}
static void bts_rounded_solid(float x,float y,float w,float h,float radius,
                              u32 face,u32 bezel,float depth,float skew){
 if(w<radius*2.f+3.f||h<radius*2.f+3.f)return;
 float fx[BTS_PRISM_VERTS],fy[BTS_PRISM_VERTS];
 float bx[BTS_PRISM_VERTS],by[BTS_PRISM_VERTS];
 float r=radius;
 bts_rounded_outline(x,y,w,h,r,fx,fy);
 float wallx=depth*.62f+bts_rear(.45f),wally=depth*.83f;
 for(int i=0;i<BTS_PRISM_VERTS;i++){
    float xlocal=(fx[i]-(x+w*.5f))/w;
    float ylocal=(fy[i]-(y+h*.5f))/h;
    /* Tiny physical tilt from the handheld's gravity vector, not a 2D skin. */
    fx[i]+=ylocal*skew*.50f;
    fy[i]+=xlocal*skew*.26f;
    bx[i]=fx[i]+wallx;by[i]=fy[i]+wally;
 }
 /* Small grounded shadow and dark back plane give the object volume. */
 u32 shade=C2D_Color32(0,0,0,96);
 for(int i=1;i+1<BTS_PRISM_VERTS;i++)
    C2D_DrawTriangle(bx[0]+1,by[0]+2,shade,bx[i]+1,by[i]+2,shade,bx[i+1]+1,by[i+1]+2,shade,0);
 for(int i=0;i<BTS_PRISM_VERTS;i++){
    int j=(i+1)%BTS_PRISM_VERTS;
    u32 side=(i<10)?C2D_Color32(47,66,80,166):C2D_Color32(19,35,47,188);
    C2D_DrawTriangle(fx[i],fy[i],side,bx[i],by[i],side,bx[j],by[j],side,0);
    C2D_DrawTriangle(fx[i],fy[i],side,bx[j],by[j],side,fx[j],fy[j],side,0);
 }
 /* A slightly lighter, genuine curved front perimeter; no stretched corners. */
 float ix[BTS_PRISM_VERTS],iy[BTS_PRISM_VERTS];
 float rim=1.55f;
 bts_rounded_outline(x+rim,y+rim,w-rim*2,h-rim*2,fmaxf(2.f,r-rim),ix,iy);
 for(int i=0;i<BTS_PRISM_VERTS;i++){
    int j=(i+1)%BTS_PRISM_VERTS;
    C2D_DrawTriangle(fx[i],fy[i],bezel,ix[i],iy[i],bezel,ix[j],iy[j],bezel,0);
    C2D_DrawTriangle(fx[i],fy[i],bezel,ix[j],iy[j],bezel,fx[j],fy[j],bezel,0);
 }
 float cx=x+w*.5f,cy=y+h*.5f;
 for(int i=0;i<BTS_PRISM_VERTS;i++){
    int j=(i+1)%BTS_PRISM_VERTS;
    C2D_DrawTriangle(cx,cy,face,ix[i],iy[i],face,ix[j],iy[j],face,0);
 }
}

static void bts_card_motion_update(u64 now){
 float dt=bts_card_last_at?(float)(now-bts_card_last_at)*.001f:1.f/30.f;
 bts_card_last_at=now;
 float ax=0,ay=0,gx=0,gy=0;
 if(bts_accel_ok){
    accelVector a;hidAccelRead(&a);
    /* Unit-free tilt ratios: clip invalid/implausible HID values. */
    float length=sqrtf((float)a.x*a.x+(float)a.y*a.y+(float)a.z*a.z);
    if(length>8.f&&length<15000.f){ax=a.x/length;ay=a.y/length;}
 }
 if(bts_gyro_ok){
    angularRate r;hidGyroRead(&r);
    gx=bts_card_limit((float)r.x*bts_gyro_dps_per_raw/130.f,-1.f,1.f);
    gy=bts_card_limit((float)r.y*bts_gyro_dps_per_raw/130.f,-1.f,1.f);
 }
 bts_card_motion_step(&bts_card_world,ax,ay,gx,gy,dt,bts_accel_ok||bts_gyro_ok);
}

static void draw_depth_grid(float w,float h){
 rect(0,0,w,h,bg);
 float horizon=h*.27f;
 float parallax=w>350.f?bts_rear(1.4f):0.f;
 for(int i=-7;i<=7;i++){
    float hx=w*.5f+(float)i*w*.036f+parallax;
    float bottomx=w*.5f+(float)i*w*.095f+parallax;
    C2D_DrawLine(hx,horizon,C2D_Color32(38,53,65,26),
                 bottomx,h,C2D_Color32(48,62,74,52),.66f,0);
 }
 for(int i=1;i<=8;i++){
    float t=(float)i/8.f;
    float y=horizon+(h-horizon)*t*t;
    u32 ink=C2D_Color32(45,60,71,(int)(18.f+37.f*t));
    C2D_DrawLine(0,y,ink,w,y,ink,.65f,0);
 }
}
/* Recolored rounded typographic glyph sprites. No font file is distributed.
 * Glyphs are generated from the freely licensed Gillius ADF face. */
static const char dream_glyphs[]="0123456789.-+:%/ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const unsigned char dream_advances[]={26,14,23,24,24,23,25,21,25,25,11,17,26,11,36,15,28,26,27,30,24,21,30,29,11,11,27,22,33,28,31,24,31,25,24,26,28,28,39,26,25,27};
static int dream_index(char c){
 if(c>='a'&&c<='z')c=(char)toupper((unsigned char)c);
 const char *v=strchr(dream_glyphs,c);
 return v?(int)(v-dream_glyphs):-1;
}
static float dream_advance(const char *s,float scale){
 float adv=0;
 while(*s){int i=dream_index(*s++);adv+=(i<0?20.0f:(float)dream_advances[i]+2.0f)*scale;}
 return adv;
}
static void dream_text(float x,float y,const char *s,float scale,float maxw){
 if(!dreamfont_sheet || !s){return;}
 float size=dream_advance(s,scale);
 if(maxw>0&&size>maxw){scale*=maxw/size;}
 while(*s){int i=dream_index(*s++);
   if(i<0){x+=20*scale;continue;}
   if((size_t)i<C2D_SpriteSheetCount(dreamfont_sheet)){
      C2D_Image glyph=C2D_SpriteSheetGetImage(dreamfont_sheet,i);
      /* Glyph artwork has its own 56px canvas; preserve the digit baseline.
       * The '.' ink is already lower in its sprite: do not lift it. */
      C2D_DrawImageAt(glyph,x,y,0,NULL,scale,scale);
   }
   x+=((float)dream_advances[i]+2)*scale;
 }
}
static bool in_map_view(void){
 return ui_page==PAGE_FIELD && open_field>=0 && open_field<cal_count &&
        cal[open_field].width*cal[open_field].height>1;
}
static void note(const char*s){snprintf(ui_note,sizeof(ui_note),"%s",s);}
static char *trim(char*s){while(isspace((unsigned char)*s))s++;char *e=s+strlen(s);while(e>s&&isspace((unsigned char)e[-1]))--e;*e=0;return s;}
static void clean(char*s){char *p=trim(s);size_t n=strlen(p);if(n>=2&&p[0]=='"'&&p[n-1]=='"'){p[n-1]=0;p++;}if(p!=s)memmove(s,p,strlen(p)+1);}
/* Split top-level CSV, preserving quoted strings and braces/parentheses. */
static int csv(const char*s,char out[][128],int limit){int n=0,depth=0;bool quote=false;size_t at=0;memset(out,0,(size_t)limit*128);if(!s||!limit)return 0;
 for(;*s&&*s!='\n'&&*s!='\r';s++){char ch=*s;if(ch=='"')quote=!quote;if(!quote){if(ch=='{'||ch=='['||ch=='(')depth++;if(ch=='}'||ch==']'||ch==')')depth--;if(ch==';'&&depth<=0)break;if(ch==','&&depth<=0){clean(out[n]);if(++n>=limit)return n;at=0;continue;}}
 if(at<127)out[n][at++]=ch;}clean(out[n]);return n+1;}
/* A TunerStudio help entry is a quoted string and may contain simple HTML
 * linebreaks/entities. Render it as plain text on the 3DS, never as markup. */
static void help_store(const char *key,const char *value){
 if(!key||!*key||!value||!*value)return;
 char raw[512];copy_text(raw,sizeof(raw),value);clean(raw);
 char desc[320];size_t at=0;
 for(const char *p=raw;*p&&at+1<sizeof(desc);){
    if(*p=='<'){
       const char *end=strchr(p,'>');
       if(end && end-p<48){if(at&&desc[at-1]!=' ')desc[at++]=' ';p=end+1;continue;}
    }
    if(*p=='&'){
       struct {const char *entity,*replacement;} const entities[]={
          {"&amp;","&"},{"&lt;","<"},{"&gt;",">"},{"&quot;","\""},{"&nbsp;"," "}};
       bool decoded=false;
       for(unsigned i=0;i<sizeof(entities)/sizeof(entities[0]);i++){
          size_t len=strlen(entities[i].entity);
          if(!strncmp(p,entities[i].entity,len)){
             const char *r=entities[i].replacement;
             while(*r&&at+1<sizeof(desc))desc[at++]=*r++;
             p+=len;decoded=true;break;
          }
       }
       if(decoded)continue;
    }
    if((*p=='\\'&&p[1]=='n')||(*p=='\\'&&p[1]=='r')){
       if(at&&desc[at-1]!=' ')desc[at++]=' ';
       p+=2;continue;
    }
    unsigned char ch=(unsigned char)*p++;
    if(ch<32||isspace(ch))ch=' ';
    if(ch==' '&&(!at||desc[at-1]==' '))continue;
    desc[at++]=(char)ch;
 }
 while(at&&desc[at-1]==' ')at--;
 desc[at]=0;
 if(!*desc)return;
 for(int i=0;i<ini_setting_help_count;i++)if(!strcasecmp(ini_setting_help[i].key,key)){
    copy_text(ini_setting_help[i].description,sizeof(ini_setting_help[i].description),desc);
    return;
 }
 if(ini_setting_help_count>=INI_HELP_CAP)return;
 IniSettingHelp *h=&ini_setting_help[ini_setting_help_count++];
 copy_text(h->key,sizeof(h->key),key);
 copy_text(h->description,sizeof(h->description),desc);
}
static const char *help_find(const char *key){
 if(!key||!*key)return NULL;
 for(int i=0;i<ini_setting_help_count;i++)if(!strcasecmp(key,ini_setting_help[i].key))
    return ini_setting_help[i].description;
 return NULL;
}
static bool number(const char*s,double*v){if(!s||!*s)return false;char*end;double n=strtod(s,&end);while(isspace((unsigned char)*end))end++;if(end==s||*end||!isfinite(n))return false;*v=n;return true;}
static int int_number(const char*s,int def){double v;if(!number(s,&v)||v<0||v>65535||floor(v)!=v)return def;return (int)v;}
static int dtype(const char*s){static const char*n[]={"U08","S08","U16","S16","U32","S32","F32","F64"};for(int i=0;i<8;i++)if(!strcasecmp(s,n[i]))return i;return T_BAD;}
static int dsize(int t){return t<=T_S08?1:t<=T_S16?2:t<=T_F32?4:t==T_F64?8:0;}
static Field *lookup(char *name,bool runtime){Field *p=runtime?channels:cal;int count=runtime?chan_count:cal_count;for(int i=0;i<count;i++)if(!strcasecmp(p[i].name,name))return &p[i];return NULL;}
static int field_index(const char*name){for(int i=0;i<cal_count;i++)if(!strcasecmp(cal[i].name,name))return i;return -1;}
static int parse_field(const char *name,const char *value,int page,bool runtime){char a[12][128];int n=csv(value,a,12);if(n<3)return 0;int kind=!strcasecmp(a[0],"scalar")?K_SCALAR:!strcasecmp(a[0],"array")?K_ARRAY:!strcasecmp(a[0],"bits")?K_BITS:-1;
 int typ=dtype(a[1]),off=int_number(a[2],-1);if(kind<0||typ==T_BAD||off<0)return 0;
 Field*f=NULL;if(runtime){if(chan_count>=CHAN_CAP)return 0;f=&channels[chan_count++];}else{if(cal_count>=FIELD_CAP)return 0;f=&cal[cal_count++];}
 memset(f,0,sizeof(*f));snprintf(f->name,sizeof(f->name),"%s",name);f->page=page;f->kind=kind;f->type=typ;f->offset=off;f->source_spec=value;f->width=1;f->height=1;int pos=3;
 if(kind==K_ARRAY){int w=0,h=1;if(sscanf(a[3],"[%dx%d]",&w,&h)!=2&&sscanf(a[3],"[%d]",&w)!=1)w=0;if(w<1||h<1||w*h>2048){f->bytes=0;return 1;}f->width=w;f->height=h;pos=4;}
 else if(kind==K_BITS)pos=4;
 f->bytes=f->width*f->height*dsize(typ);if(kind==K_BITS){int lo=-1,hi=-1;if(sscanf(a[3],"[%d:%d]",&lo,&hi)!=2){if(sscanf(a[3],"[%d]",&lo)==1)hi=lo;}f->bitlo=lo;f->bithi=hi;
 f->validScale=lo>=0&&hi>=lo&&hi<dsize(typ)*8&&hi-lo<32;
 f->scale=1;
 /* The fourth comma starts the optional list or $define reference. */
 const char *q=value;int commas=0;bool in_quotes=false;
 while(*q){if(*q=='"')in_quotes=!in_quotes;
   if(!in_quotes&&*q==','&&++commas==4){f->choice_spec=q+1;break;}
   q++;}
 return 1;}
 if(pos<n)snprintf(f->unit,sizeof(f->unit),"%s",a[pos]);double scale=0,trans=0;f->validScale=pos+2<n&&number(a[pos+1],&scale)&&number(a[pos+2],&trans)&&scale!=0;
 f->scale=scale;f->translate=trans;
 /* These scale expressions are conditional on *actual* ECU constants. Never
  * substitute a fixed kPa/PSI scale without reading those configuration bits. */
 if(!f->validScale && pos+1<n && strstr(a[pos+1],"useMetricOnInterface")
     && strstr(a[pos+1],"0.145038")){
   if(strstr(a[pos+1],"veOverrideMode"))f->dyn_axis_kind=1;
   else if(strstr(a[pos+1],"ignOverrideMode"))f->dyn_axis_kind=2;
   else if(strstr(a[pos+1],"afrOverrideMode"))f->dyn_axis_kind=3;
 }
 double lo=0,hi=0;
 if(pos+4<n&&number(a[pos+3],&lo)&&number(a[pos+4],&hi)&&lo<=hi){
   f->low=lo;f->high=hi;f->limitsKnown=true;
 }else if(pos+4<n){
   /* Conditional bounds must resolve before a calibration can be edited. */
   f->validScale=false;
 }
 if(f->unit[0]=='{')f->unit[0]=0;
 return 1;}
/* Evaluate the ECU's own numeric conversion, translation AND display
 * bounds together. Unknown modes and unknown limits stay unavailable. */
static bool ini_resolve_numeric_field(Field *f){
 if(!f||f->kind==K_BITS||!f->source_spec)return false;
 char parts[12][128];int n=csv(f->source_spec,parts,12);
 int pos=f->kind==K_ARRAY?4:3;
 if(n<pos+3)return false;
 double scale=0,translate=0,low=0,high=0;
 if(!bts_ini_numeric(parts[pos+1],&ini_flags,&scale) ||
    !bts_ini_numeric(parts[pos+2],&ini_flags,&translate) ||
    !isfinite(scale)||!isfinite(translate)||scale==0)return false;
 bool limits=false;
 if(n>pos+4){
    if(!bts_ini_numeric(parts[pos+3],&ini_flags,&low)||
       !bts_ini_numeric(parts[pos+4],&ini_flags,&high)||low>high)
       return false;
    limits=true;
 }
 f->scale=scale;f->translate=translate;
 f->limitsKnown=limits;
 if(limits){f->low=low;f->high=high;}
 if(parts[pos][0]=='{'){
    char unit[26];if(bts_ini_unit(parts[pos],&ini_flags,unit,sizeof(unit)))
       copy_text(f->unit,sizeof(f->unit),unit);
    else f->unit[0]=0;
 }
 f->validScale=true;return true;
}
static void ini_resolve_all_numeric_fields(void){
 for(int i=0;i<cal_count;i++)ini_resolve_numeric_field(&cal[i]);
 for(int i=0;i<chan_count;i++)ini_resolve_numeric_field(&channels[i]);
}
static void ini_drop_dynamic_conversions(void){
 memset(&ini_flags,0,sizeof(ini_flags));
 ini_flags_requested=false;ini_flags_retry_at=0;ini_flags_signature[0]=0;
 for(int pass=0;pass<2;pass++){
    Field *fields=pass?channels:cal;int count=pass?chan_count:cal_count;
    for(int i=0;i<count;i++){
       Field *f=&fields[i];if(f->kind==K_BITS||!f->source_spec)continue;
       char parts[12][128];int n=csv(f->source_spec,parts,12);
       int pos=f->kind==K_ARRAY?4:3;
       if(n>pos+2 && (strchr(parts[pos+1],'?')||strchr(parts[pos+2],'?') ||
            (n>pos+4&&(strchr(parts[pos+3],'?')||strchr(parts[pos+4],'?'))))){
           f->validScale=false;f->limitsKnown=false;
       }
    }
 }
 ini_resolve_all_numeric_fields();
}
static void parse_page_ids(const char*value){char a[64][128];int n=csv(value,a,64);page_ids_count=n;
 for(int i=0;i<n;i++){unsigned hi=0,lo=0;char *p=strstr(a[i],"\\x");
 if(p&&sscanf(p,"\\x%2x\\x%2x",&lo,&hi)==2)page_ids[i]=(int)(lo|(hi<<8));
 else page_ids[i]=int_number(a[i],-1);
 }}
/* Honor the installed INI's actual per-page flash and write commands. */
static void parse_page_capability(const char *value,bool *out,const char *expected){
 char a[64][128];int n=csv(value,a,64);
 for(int i=0;i<64;i++)out[i]=i<n&&!strcasecmp(a[i],expected);
}
static void parse_int_list(const char*value,int*dest,int max,int*count){char a[64][128];int n=csv(value,a,64);*count=n<max?n:max;for(int i=0;i<*count;i++)dest[i]=int_number(a[i],-1);}
static void add_menu(const char*label,const char*target,int group){
 while(*label=='&')label++;
 if(menu_count>=MENU_CAP||!*label)return;MenuItem*m=&menus[menu_count++];snprintf(m->label,sizeof(m->label),"%s",label);snprintf(m->target,sizeof(m->target),"%s",target);m->group=group;}
static void add_ref(const char*owner,const char*ref,const char*label,int panelref){
 if(ref_count>=REF_CAP||!*owner||!*ref)return;
 DialogRef*d=&refs[ref_count++];memset(d,0,sizeof(*d));
 copy_text(d->owner,sizeof(d->owner),owner);
 copy_text(d->ref,sizeof(d->ref),ref);
 copy_text(d->label,sizeof(d->label),label);
 d->panel=panelref;
}
/* TunerStudio's #define macros supply board-specific labels for pin and ADC
 * enums. Keep their original indices, including INVALID entries. */
static const char *macro_value(const char*name){
 for(int i=0;i<macro_count;i++)if(!strcasecmp(macro_defs[i].name,name))return macro_defs[i].value;
 return NULL;
}
static void parse_macro(char *line){
 if(strncmp(line,"#define ",8)||macro_count>=160)return;
 char *name=trim(line+8),*eq=strchr(name,'=');
 if(!eq)return;
 *eq=0;name=trim(name);
 if(!*name||strlen(name)>=sizeof(macro_defs[0].name))return;
 snprintf(macro_defs[macro_count].name,sizeof(macro_defs[0].name),"%s",name);
 macro_defs[macro_count++].value=trim(eq+1);
}
static const char *choice_for_value(int value){
 for(int i=0;i<choice_count;i++)if(choices[i].value==value)return choices[i].label;
 return NULL;
}
static void parse_choices(const Field *f){
 choice_count=0;
 if(!f||f->kind!=K_BITS||!f->choice_spec)return;
 const char *src=f->choice_spec;
 while(isspace((unsigned char)*src))src++;
 if(*src=='$'){
  char name[64];int n=0;src++;
  while((isalnum((unsigned char)*src)||*src=='_')&&n<63)name[n++]=*src++;
  name[n]=0;
  src=macro_value(name);
  if(!src)return;
 }
 int index=0,items=0;
 while(*src&&items<4096&&choice_count<512){
  char token[128];int len=0;bool quoted=false;
  while(*src&&(*src!=','||quoted)&&*src!='\n'&&*src!='\r'){
    if(*src=='"')quoted=!quoted;
    if(len<127)token[len++]=*src;
    src++;
  }
  token[len]=0;
  if(*src==',')src++;
  char *t=trim(token),*label=t;
  char *eq=strchr(t,'=');
  int value=index;
  if(eq){
    *eq=0;
    char *end=NULL;long parsed=strtol(trim(t),&end,10);
    if(end!=t&&*trim(end)==0&&parsed>=0&&parsed<=65535)value=(int)parsed;
    label=trim(eq+1);
  }
  clean(label);
  if(*label&&strncasecmp(label,"INVALID",7)&&strcasecmp(label,"reserved")){
    Choice*c=&choices[choice_count++];c->value=value;
    snprintf(c->label,sizeof(c->label),"%s",label);
  }
  index=value+1;items++;
 }
}
/* Parses only real INI declarations, including all named pages/menus/tables/dialogs. */
static int parse_ini_all(const char*body,int length){char*buf=(char*)malloc((size_t)length+1);if(!buf){note("Not enough memory to parse INI");return 0;}memcpy(buf,body,(size_t)length);buf[length]=0;
 if(ini_source)free(ini_source);
 if(compare_buffer)free(compare_buffer);
 ini_source=buf;
 macro_count=power_cycle_count=ini_readonly_count=0;rpm_limit_requested=rpm_limit_known=false;rpm_limit_retry_at=0;
 menu_count=cal_count=chan_count=table_count=ref_count=gauge_def_count=ini_graph_count=0;page_ids_count=0;ini_setting_help_count=0;dialog_caption_count=0;ini_indicator_count=0;
 memset(&ini_flags,0,sizeof(ini_flags));ini_flags_requested=false;ini_flags_retry_at=0;ini_flags_signature[0]=0;
 memset(nav_bookmark,0,sizeof(nav_bookmark));memset(page_ids,-1,sizeof(page_ids));memset(page_sizes,-1,sizeof(page_sizes));memset(page_burnable,0,sizeof(page_burnable));memset(page_writeable,0,sizeof(page_writeable));ini_signature[0]=0;
 char*save=NULL;char*row=strtok_r(buf,"\r\n",&save);char section[32]="";int currentpage=1,lasttable=-1,lastcurve=-1;char dialog[64]="";int currentgraph=-1;bool onmain=false;
 while(row){char*p=trim(row);if(!strncmp(p,"#define ",8))parse_macro(p);
 if(*p=='['){char*e=strchr(p,']');if(e){*e=0;snprintf(section,sizeof(section),"%s",p+1);}onmain=false;lasttable=lastcurve=-1;dialog[0]=0;currentgraph=-1;}
 else if(*p&&*p!=';'&&*p!='#'){char*eq=strchr(p,'=');if(eq){*eq=0;char*key=trim(p),*val=trim(eq+1);char a[6][128];int n=csv(val,a,6);
 if(!strcasecmp(section,"SettingContextHelp"))help_store(key,val);
 if(!strcasecmp(section,"TunerStudio")&&!strcasecmp(key,"signature"))snprintf(ini_signature,sizeof(ini_signature),"%s",a[0]);
 if(!strcasecmp(section,"ConstantsExtensions")&&!strcasecmp(key,"requiresPowerCycle")&&n>0&&power_cycle_count<CYCLE_CAP){
   copy_text(power_cycle_names[power_cycle_count++],sizeof(power_cycle_names[0]),a[0]);
 }
 if(!strcasecmp(section,"ConstantsExtensions")&&!strcasecmp(key,"readOnly")&&n>0&&ini_readonly_count<INI_READONLY_CAP){
   copy_text(ini_readonly_names[ini_readonly_count++],sizeof(ini_readonly_names[0]),a[0]);
 }
 if(!strcasecmp(section,"Constants")){if(!strcasecmp(key,"page"))currentpage=int_number(a[0],1);else if(!strcasecmp(key,"pageIdentifier"))parse_page_ids(val);else if(!strcasecmp(key,"pageSize")){int dummy;parse_int_list(val,page_sizes,64,&dummy);}else if(!strcasecmp(key,"burnCommand"))parse_page_capability(val,page_burnable,"B%2i");else if(!strcasecmp(key,"pageChunkWrite"))parse_page_capability(val,page_writeable,"C%2i%2o%2c%v");else if(!strcasecmp(key,"endianness"))ini_big_endian=strncasecmp(val,"big",3)==0;else parse_field(key,val,currentpage,false);}
 if(!strcasecmp(section,"OutputChannels"))parse_field(key,val,0,true);
 if(!strcasecmp(section,"GaugeConfigurations") && gauge_def_count<512 && n>=5){
   char gc[11][128];int gn=csv(val,gc,11);
   if(gn>=5){GaugeDef*g=&gauges[gauge_def_count++];memset(g,0,sizeof(*g));
     snprintf(g->name,sizeof(g->name),"%s",key);
     copy_text(g->channel,sizeof(g->channel),gc[0]);
     copy_text(g->label,sizeof(g->label),gc[1]);
     copy_text(g->minimum,sizeof(g->minimum),gc[3]);
     copy_text(g->maximum,sizeof(g->maximum),gc[4]);
   }
 }
 if(!strcasecmp(section,"Menu")){
 if(!strcasecmp(key,"menuDialog"))onmain=!strncasecmp(a[0],"main",4);
 else if(onmain&&!strcasecmp(key,"menu")){add_menu(a[0],"",0);}
 else if(onmain&&!strcasecmp(key,"groupMenu"))add_menu(a[0],"",1);
 else if(onmain&&(!strcasecmp(key,"subMenu")||!strcasecmp(key,"groupChildMenu"))&&n>1&&strncasecmp(a[0],"std_separator",13))add_menu(a[1],a[0],!strcasecmp(key,"subMenu")?3:2);
 }
 if(!strcasecmp(section,"TableEditor")){
 if(!strcasecmp(key,"table")&&table_count<TABLE_CAP){lasttable=table_count++;TableDef*t=&tables[lasttable];memset(t,0,sizeof(*t));copy_text(t->id,sizeof(t->id),a[0]);if(n>1)copy_text(t->alt,sizeof(t->alt),a[1]);if(n>2)copy_text(t->label,sizeof(t->label),a[2]);}
 else if(lasttable>=0){TableDef*t=&tables[lasttable];if(!strcasecmp(key,"xBins")){copy_text(t->x,sizeof(t->x),a[0]);if(n>1)copy_text(t->xchan,sizeof(t->xchan),a[1]);if(n>2)t->xro=!strcasecmp(a[2],"readOnly");}
 if(!strcasecmp(key,"yBins")){copy_text(t->y,sizeof(t->y),a[0]);if(n>1)copy_text(t->ychan,sizeof(t->ychan),a[1]);if(n>2)t->yro=!strcasecmp(a[2],"readOnly");}if(!strcasecmp(key,"zBins"))copy_text(t->z,sizeof(t->z),a[0]);}
 }
 if(!strcasecmp(section,"CurveEditor")){
    if(!strcasecmp(key,"curve")&&table_count<TABLE_CAP){
       lastcurve=table_count++;
       TableDef *t=&tables[lastcurve];memset(t,0,sizeof(*t));
       t->curve=true;
       copy_text(t->id,sizeof(t->id),a[0]);
       if(n>1)copy_text(t->label,sizeof(t->label),a[1]);
    }else if(lastcurve>=0){
       TableDef *t=&tables[lastcurve];
       if(!strcasecmp(key,"xBins")){copy_text(t->x,sizeof(t->x),a[0]);if(n>1)copy_text(t->xchan,sizeof(t->xchan),a[1]);if(n>2)t->xro=!strcasecmp(a[2],"readOnly");}
       if(!strcasecmp(key,"yBins")){copy_text(t->z,sizeof(t->z),a[0]);if(n>1)copy_text(t->ychan,sizeof(t->ychan),a[1]);if(n>2)t->yro=!strcasecmp(a[2],"readOnly");}
    }
 }
 if(!strcasecmp(section,"UserDefined")){
 if(!strcasecmp(key,"dialog")){
      copy_text(dialog,sizeof(dialog),a[0]);currentgraph=-1;
      if(n>1&&*a[1]&&dialog_caption_count<DIALOG_CAPTION_CAP){
         DialogCaption *c=&dialog_captions[dialog_caption_count++];
         copy_text(c->id,sizeof(c->id),dialog);
         copy_text(c->title,sizeof(c->title),a[1]);
      }
  }
 else if(!strcasecmp(key,"liveGraph") && *dialog && n>=1 && ini_graph_count<INI_GRAPH_CAP){
   currentgraph=ini_graph_count++;
   IniGraphDef *g=&ini_graphs[currentgraph];memset(g,0,sizeof(*g));
   snprintf(g->owner,sizeof(g->owner),"%s",dialog);
   copy_text(g->id,sizeof(g->id),a[0]);
   copy_text(g->title,sizeof(g->title),n>1?a[1]:"Live Graph");
   add_ref(dialog,a[0],n>1?a[1]:"Live Graph",3);
 }
 else if(!strcasecmp(key,"graphLine") && currentgraph>=0 && n>=1 && *a[0]){
   IniGraphDef *g=&ini_graphs[currentgraph];
   if(g->count<INI_GRAPH_LINES)copy_text(g->channels[g->count++],sizeof(g->channels[0]),a[0]);
 }
 else if(!strcasecmp(key,"indicatorPanel")&&n>=1){
     copy_text(dialog,sizeof(dialog),a[0]);currentgraph=-1;
  }
  else if(!strcasecmp(key,"readoutPanel")&&n>=1){
     copy_text(dialog,sizeof(dialog),a[0]);currentgraph=-1;
  }
  else if(!strcasecmp(key,"indicator")&&n>=3&&ini_indicator_count<BTS_INDICATOR_CAP){
     int idx=ini_indicator_count++;
     BtsIniIndicator *ind=&ini_indicators[idx];memset(ind,0,sizeof(*ind));
     copy_text(ind->off,sizeof(ind->off),a[1]);
     copy_text(ind->on,sizeof(ind->on),a[2]);
     char channel[128];copy_text(channel,sizeof(channel),a[0]);
     char *p=trim(channel);
     if(*p=='{'){p++;char *end=strrchr(p,'}');if(end)*end=0;p=trim(p);}
     /* Only bare ECU channel identifiers resolve to a live indicator;
      * arbitrary expression text is preserved, never guessed as a value. */
     copy_text(ind->channel,sizeof(ind->channel),p);
     char refid[40];snprintf(refid,sizeof(refid),"btsIndicator%d",idx);
     add_ref(dialog,refid,*ind->on?ind->on:refid,6);
  }
  else if(!strcasecmp(key,"readout")&&n>=1&&*a[0])
      add_ref(dialog,a[0],a[0],5);
  else if(!strcasecmp(key,"panel")&&n){
     int previous=ref_count;
     add_ref(dialog,a[0],a[0],1);
     if(ref_count>previous && n>=2)
        refs[ref_count-1].region=(unsigned char)bts_ini_region(a[1]);
  }
 else if(!strcasecmp(key,"field")&&n>=2&&*a[1]&&*a[1]!='{'&&strchr(a[1],' ')==NULL)add_ref(dialog,a[1],a[0],0);
 else if(!strcasecmp(key,"field")&&n==1&&*a[0])add_ref(dialog,a[0],a[0],4);
 else if(!strcasecmp(key,"gauge")&&*dialog&&n>=1&&*a[0]){
      int previous=ref_count;add_ref(dialog,a[0],a[0],5);
      if(ref_count>previous && n>=2)
          refs[ref_count-1].region=(unsigned char)bts_ini_region(a[1]);
  }
 else if(!strcasecmp(key,"commandButton")&&n>=2&&*a[1])
      add_ref(dialog,a[1],a[0],2);
 }
 }}row=strtok_r(NULL,"\r\n",&save);}
 for(int i=0;i<power_cycle_count;i++){
   Field *f=lookup(power_cycle_names[i],false);
   if(f)f->powerCycle=true;
 }
 for(int i=0;i<ini_readonly_count;i++){
   Field *f=lookup(ini_readonly_names[i],false);
   if(f)f->readOnly=true;
 }
 ini_resolve_all_numeric_fields();
 stim_preview=false;
 menu_loaded=menu_count>0;snprintf(menu_info,sizeof(menu_info),"%d menu  %d maps  %d fields",menu_count,table_count,cal_count);
 if(!menu_loaded){note("INI has no recognized menus");return 0;}note("ECU definition loaded - select a menu");return menu_count;}
/* Save a verified, complete INI once. No INI download is needed next launch.
 * Only accepted if the definition exposes both actual constants and channels. */
static void clear_live(void);
static bool ini_looks_valid(const char *data,int len){
    return data && len>5000 && len<RESPONSE_CAP-128 &&
           strstr(data,"[Constants]") && strstr(data,"[OutputChannels]") &&
           strstr(data,"[TunerStudio]");
}
static bool write_cached_ini(const char *data,int len){
    if(!ini_looks_valid(data,len))return false;
    mkdir("sdmc:/BrickECU",0777);
    FILE *f=fopen(INI_TEMP,"wb");if(!f)return false;
    bool ok=fwrite(data,1,(size_t)len,f)==(size_t)len;
    if(fclose(f)!=0)ok=false;
    if(!ok){remove(INI_TEMP);return false;}
    if(rename(INI_TEMP,INI_CACHE)==0)return true;
    /* FAT may refuse replacement-by-rename. Preserve the old definition
     * until the new file is definitely installed. */
    const char *backup="sdmc:/BrickECU/ecu.bak";
    remove(backup);
    if(rename(INI_CACHE,backup)!=0){remove(INI_TEMP);return false;}
    if(rename(INI_TEMP,INI_CACHE)!=0){
        rename(backup,INI_CACHE);
        remove(INI_TEMP);
        return false;
    }
    remove(backup);
    return true;
}
/* Optional 3DS sound, entirely local: no ECU command/output is associated
 * with any effect. Missing DSP firmware simply leaves the UI silent. */
static void sfx_init(void){
 bts_ndsp_result=ndspInit();
 if(R_FAILED(bts_ndsp_result))return;
 bts_ndsp_active=true;
 ndspSetOutputMode(NDSP_OUTPUT_STEREO);
 /* Load the eleven event clips first: failure to allocate optional    menu beeps must not take away the actual requested audio. */
 bts_audio_init(true);
 sfx_samples=(s16*)linearAlloc(3*SFX_LENGTH*sizeof(s16));
 if(!sfx_samples)return;
 /* Original Memphis-inspired UI percussion, not copyrighted samples.
  * A brighter attack + bass tail is audible on actual 3DS speakers. */
 const float f[3]={660.0f,940.0f,330.0f};
 for(int t=0;t<3;t++)for(int i=0;i<SFX_LENGTH;i++){
    float sec=(float)i/22050.0f;
    float u=(float)i/(float)SFX_LENGTH;
    float attack=fminf(1.0f,sec*700.0f);
    float decay=expf(-u*(t==1?6.0f:8.0f));
    float pitch=f[t]*(1.0f+(.30f-0.24f*u)*(1.0f-u));
    float tri=(2.0f/3.14159265f)*asinf(sinf(6.2831853f*pitch*sec));
    float bass=sinf(6.2831853f*(f[t]*0.50f)*sec);
    float edge=sinf(6.2831853f*(f[t]*2.9f)*sec);
    float value=(.68f*tri+.24f*bass+.08f*edge)*attack*decay;
    if(value>1.0f)value=1.0f;if(value< -1.0f)value=-1.0f;
    sfx_samples[t*SFX_LENGTH+i]=(s16)(23000.0f*value);
 }
 DSP_FlushDataCache(sfx_samples,3*SFX_LENGTH*sizeof(s16));
 ndspSetOutputMode(NDSP_OUTPUT_STEREO);
 ndspChnSetRate(0,22050.0f);
 ndspChnSetFormat(0,NDSP_FORMAT_MONO_PCM16);
 ndspChnSetInterp(0,NDSP_INTERP_LINEAR);
 float mix[12]={0};mix[0]=.93f;mix[1]=.93f;ndspChnSetMix(0,mix);
 memset(sfx_buffers,0,sizeof(sfx_buffers));
 for(int i=0;i<3;i++){
    sfx_buffers[i].data_pcm16=sfx_samples+i*SFX_LENGTH;
    sfx_buffers[i].nsamples=SFX_LENGTH;
 }
 sfx_ready=true;
}
static void sfx_play(int type){
 if(!sfx_ready||!sfx_enabled||type<0||type>2||bts_audio_burn_loop)return;
 /* Conditional clips take priority on the working CH0 mixer. A menu beep
  * cannot erase a clip just queued by the settings or ECU event handler. */
 if(bts_audio_active)return; /* never cut off a queued event */
 u64 now=osGetTime();if(now-sfx_last_at<42)return;
 sfx_last_at=now;
 ndspChnWaveBufClear(0);
 ndspChnWaveBufAdd(0,&sfx_buffers[type]);
}
static void sfx_close(void){
 if(!bts_ndsp_active)return;
 bts_audio_close();
 ndspChnWaveBufClear(0);
 ndspExit();
 if(sfx_samples)linearFree(sfx_samples);
 sfx_samples=NULL;sfx_ready=false;bts_ndsp_active=false;
}
static bool runtime_value(const char *name,double *value);
static void capture_knock_sample(void){
 double v=0;bool ok=runtime_value("m_knockLevel",&v);
 int at=knock_head;knock_history[at]=(float)v;
 knock_history_valid[at]=ok;knock_history_ms[at]=(u32)osGetTime();
 knock_head=(knock_head+1)%KNOCK_HISTORY;
 if(knock_count<KNOCK_HISTORY)knock_count++;
}

static void load_gauge_preferences(void){
 if(gauge_prefs_loaded)return;
 gauge_prefs_loaded=true;
 for(int i=0;i<GAUGE_SLOTS;i++)snprintf(gauge_names[i],sizeof(gauge_names[i]),"%s",metric_fields[i]);
 FILE *fp=fopen(DISPLAY_CFG,"rb");
 if(!fp)return;
 char line[160];
 while(fgets(line,sizeof(line),fp)){
  int v=0,i=0;char name[64];
  if(sscanf(line,"style=%d",&v)==1){if(v>=0&&v<7)gauge_style=v;continue;}
  if(sscanf(line,"sfx=%d",&v)==1){sfx_enabled=v!=0;continue;}
  if(sscanf(line,"event_audio=%d",&v)==1){event_audio_enabled=v!=0;continue;}
  if(sscanf(line,"precision%d=%d",&i,&v)==2){
    if(i>=0&&i<GAUGE_SLOTS&&v>=-1&&v<=3)gauge_decimals[i]=v;
    continue;
  }
  if(sscanf(line,"slot%d=%63[^\r\n]",&i,name)==2&&i>=0&&i<GAUGE_SLOTS){
     /* An INI name cannot contain this syntax; invalid names stay unavailable. */
     bool valid=true;for(char*q=name;*q;q++)if(!isalnum((unsigned char)*q)&&*q!='_')valid=false;
     if(valid)snprintf(gauge_names[i],sizeof(gauge_names[i]),"%s",name);
  }
 }
 fclose(fp);
}
static void save_gauge_preferences(void){
 mkdir("sdmc:/BrickECU",0777);
 FILE *fp=fopen(DISPLAY_CFG,"wb");if(!fp)return;
 fprintf(fp,"style=%d\n",gauge_style);fprintf(fp,"sfx=%d\n",sfx_enabled?1:0);
 fprintf(fp,"event_audio=%d\n",event_audio_enabled?1:0);
 for(int i=0;i<GAUGE_SLOTS;i++){
   fprintf(fp,"slot%d=%s\n",i,gauge_names[i]);
   fprintf(fp,"precision%d=%d\n",i,gauge_decimals[i]);
 }
 fclose(fp);
}
static void resolve_gauge_fields(void){
 selectable_count=0;
 for(int i=0;i<chan_count;i++){
   if(channels[i].kind==K_SCALAR&&channels[i].validScale&&channels[i].offset>=0&&
      channels[i].offset+dsize(channels[i].type)<=LIVE_CAP && selectable_count<CHAN_CAP)
       selectable_channels[selectable_count++]=i;
 }
 for(int i=0;i<GAUGE_SLOTS;i++){
   gauge_fields[i]=-1;
   Field*f=lookup(gauge_names[i],true);
   if(f&&f->kind==K_SCALAR&&f->validScale&&f->offset+dsize(f->type)<=LIVE_CAP)
        gauge_fields[i]=(int)(f-channels);
 }
}
static void expand_live_for_gauges(void){
 for(int i=0;i<GAUGE_SLOTS;i++){
  int idx=gauge_fields[i];if(idx<0)continue;
  Field *f=&channels[idx];int end=f->offset+dsize(f->type);
  if(end<=LIVE_CAP&&end>live_length)live_length=end;
 }
}
static void configure_channels(void){
 load_gauge_preferences();resolve_gauge_fields();
    live_length=256;
    for(int i=0;i<9;i++){
        Field *f=lookup((char*)metric_fields[i],true);
        if(f&&f->offset+dsize(f->type)>live_length)
            live_length=f->offset+dsize(f->type);
    }
    expand_live_for_gauges();
    if(live_length>LIVE_CAP)live_length=LIVE_CAP;
    base_live_length=live_length;
    clear_live();
}
/* Request only the extra runtime bytes required by the open map's live
 * X/Y cursor. BrickOS supports up to 3072; never invent missing channels. */
static void extend_live_for_table(const TableDef *t){
    live_length=base_live_length;
    if(!t)return;
    const char *names[2]={t->xchan,t->ychan};
    for(int i=0;i<2;i++){
       if(!*names[i])continue;
       Field *f=lookup((char*)names[i],true);
       if(f&&f->offset>=0){
          int end=f->offset+dsize(f->type);
          if(end>live_length&&end<=LIVE_CAP)live_length=end;
       }
    }
}

/* Knock payload expansion happens ONLY while the Knock Monitor is open.
 * Enlarging every normal live HTTP request would hurt old-3DS sample rates. */
static void extend_live_for_knock(void){
 live_length=base_live_length;
 const char *names[]={"m_knockLevel","m_knockRetard","m_knockCount",
   "m_knockCyl1","m_knockCyl2","m_knockCyl3","m_knockCyl4","m_knockCyl5","m_knockCyl6",
   "m_knockCyl7","m_knockCyl8","m_knockCyl9","m_knockCyl10","m_knockCyl11","m_knockCyl12"};
 for(size_t k=0;k<sizeof(names)/sizeof(names[0]);k++){
    Field *f=lookup((char*)names[k],true);
    if(f&&f->validScale&&f->kind==K_SCALAR&&f->offset>=0){
       int end=f->offset+dsize(f->type);
       if(end>live_length&&end<=LIVE_CAP)live_length=end;
    }
 }
}

/* Fetch only known read-only trigger channels from the INI, while this
 * screen is open. All values are taken from ECU samples, not the model. */
static void extend_live_for_trigger(void){
 live_length=base_live_length;
 const char *names[]={"RPMValue","totalTriggerErrorCounter","trgtriggerSyncGapRatio"};
 for(size_t i=0;i<sizeof(names)/sizeof(names[0]);i++){
  Field *f=lookup((char*)names[i],true);
  if(f&&f->validScale&&f->kind==K_SCALAR&&f->offset>=0){
   int end=f->offset+dsize(f->type);
   if(end<=LIVE_CAP&&end>live_length)live_length=end;
  }
 }
}

static bool load_ini_cache(void){
    FILE *f=fopen(INI_CACHE,"rb");if(!f)return false;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return false;}
    long n=ftell(f);
    if(n<=0||n>=RESPONSE_CAP-128||fseek(f,0,SEEK_SET)!=0){fclose(f);return false;}
    char *buffer=malloc((size_t)n+1);
    if(!buffer){fclose(f);return false;}
    bool ok=fread(buffer,1,(size_t)n,f)==(size_t)n;
    fclose(f);buffer[n]=0;
    if(ok && ini_looks_valid(buffer,(int)n))ok=parse_ini_all(buffer,(int)n)>0;
    else ok=false;
    free(buffer);
    if(ok){configure_channels();ini_cached_on_sd=true;
        note("INI loaded from SD cache - Y refreshes");}
    return ok;
}
static int readable(int fd, int writing, int milliseconds){
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd,&set);
    struct timeval timeout = { .tv_sec=milliseconds/1000,
                               .tv_usec=(milliseconds%1000)*1000 };
    return select(fd+1,writing?NULL:&set,writing?&set:NULL,NULL,&timeout);
}

/* The libctru 3DS SOC implementation does not reliably clear SO_ERROR after
 * an asynchronous connect; see devkitPro/libctru issue #412. After select,
 * check completion using another connect() and accept EISCONN (RetroArch's
 * established 3DS workaround). A socket is never reused after a hard error.
 */
static int connect_socket(void){
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0)return -1;
    struct sockaddr_in dest;
    memset(&dest,0,sizeof(dest));
    dest.sin_family=AF_INET;
    dest.sin_port=htons(HOST_PORT);
    inet_pton(AF_INET,HOST_IP,&dest.sin_addr);
    int r=connect(fd,(struct sockaddr*)&dest,sizeof(dest));
    if(r==0)return fd;
    int e=errno;
    if(e==EISCONN)return fd;
    if(e==EINPROGRESS || e==EALREADY || e==EWOULDBLOCK || e==EAGAIN){
        r=readable(fd,1,5000);
        if(r>0){
            r=connect(fd,(struct sockaddr*)&dest,sizeof(dest));
            if(r==0 || (r<0 && errno==EISCONN))return fd;
            e=errno;
        }else e=(r==0)?ETIMEDOUT:errno;
    }
    close(fd);
    errno=e;
    return -1;
}
/* AC status 3 is explicitly documented as connected; 2 is not documented
 * as fully connected and can occur on local-only APs. Do not rule out
 * local TCP just because AC reports 2. A real HTTP response is the proof.
 * Signal bars are not proof of a valid AP association.
 */
static void update_wifi_state(void){
    if(!ac_ok)return;
    u32 st=0,typ=0;
    Result rs=ACU_GetStatus(&st);
    if(R_SUCCEEDED(rs))wifi_status=st;
    Result rt=ACU_GetWifiStatus(&typ);
    if(R_SUCCEEDED(rt))wifi_type=typ;
    else wifi_type=0;
    char ssid[33]={0};
    Result rn=ACU_GetSSID(ssid);
    ssid[32]=0;
    if(R_SUCCEEDED(rn) && ssid[0]){
        snprintf(wifi_ssid,sizeof(wifi_ssid),"%s",ssid);
    }else{
        snprintf(wifi_ssid,sizeof(wifi_ssid),"(unavailable)");
    }
    ACU_GetLastErrorCode(&ac_last_error);
    ACU_GetLastDetailErrorCode(&ac_last_detail_error);
    wifi_candidate=(wifi_status==2 || wifi_status==3);
    wifi_ok=(wifi_status==3 && strcmp(wifi_ssid,"brick")==0);
}

/* Hold the AC service open for the app's entire lifetime so it does not
 * release our connection to a local-only Wi-Fi AP after a failed Internet
 * connectivity check. ACU_ConnectAsync completes when the saved network
 * connection attempt finishes; no Internet connection is required.
 *
 * slot=-1: any saved connection (normal automatic first attempt)
 * slot=0,1,2: force the 3DS System Settings Wi-Fi slot 1,2,3
 */
static void connect_wifi(int slot){
    if(!ac_ok || wifi_busy)return;
    wifi_busy=true;
    wifi_last_attempt=osGetTime();
    wifi_slot=slot;
    wifi_last_error=0;
    snprintf(net_status,sizeof(net_status),"Wi-Fi: request %s...",slot<0?"saved AP":slot==0?"slot 1":slot==1?"slot 2":"slot 3");
    acuConfig cfg;
    memset(&cfg,0,sizeof(cfg));
    Result r=ACU_CreateDefaultConfig(&cfg);
    if(R_SUCCEEDED(r))r=ACU_SetNetworkArea(&cfg,2);
    if(R_SUCCEEDED(r))r=ACU_SetAllowApType(&cfg,slot<0?(u8)AC_AP_TYPE_ALL:
                                            (u8)(AC_AP_TYPE_SLOT1<<slot));
    if(R_SUCCEEDED(r))r=ACU_SetRequestEulaVersion(&cfg);
    Handle event=0;
    if(R_SUCCEEDED(r))r=svcCreateEvent(&event,RESET_ONESHOT);
    if(R_SUCCEEDED(r))r=ACU_ConnectAsync(&cfg,event);
    if(R_SUCCEEDED(r))r=svcWaitSynchronization(event,12000000000LL);
    if(event)svcCloseHandle(event);
    if(R_FAILED(r)){
        wifi_last_error=(int)r;
        snprintf(net_status,sizeof(net_status),"AC connect: 0x%08lX",(unsigned long)r);
    }
    update_wifi_state();
    if(R_SUCCEEDED(r)){
        if(wifi_ok){
            snprintf(net_status,sizeof(net_status),"AC3 brick - local HTTP test next");
        }else if(wifi_status==2){
            snprintf(net_status,sizeof(net_status),"AC2: local-only possible; testing HTTP");
        }else{
            snprintf(net_status,sizeof(net_status),"AC%lu SSID:%s - X retry / L,R slot",
                     (unsigned long)wifi_status,wifi_ssid);
        }
    }
    wifi_busy=false;
}

/* Stop reading once the complete HTTP entity has arrived.  The previous
 * implementation waited for TCP close on every request (up to 3 seconds),
 * even if the entire live packet had already been received. */
static bool http_entity_complete(const char *buffer,int size){
 if(size<=0)return false;
 const char *head=NULL;
 for(int i=0;i+3<size&&i<4096;i++){
   if(buffer[i]=='\r'&&buffer[i+1]=='\n'&&buffer[i+2]=='\r'&&buffer[i+3]=='\n'){
      head=buffer+i+4;break;
   }
 }
 if(!head)return false;
 int header_len=(int)(head-buffer),body_bytes=size-header_len;
 long content_len=-1;bool chunked=false;
 const char *line=buffer,*h_end=head-4;
 while(line<h_end){
   const char *e=line;while(e<h_end&&!(e[0]=='\r'&&e+1<h_end&&e[1]=='\n'))e++;
   if(e==line)break;
   if(e-line>15 && !strncasecmp(line,"Content-Length:",15)){
     content_len=strtol(line+15,NULL,10);
   }
   if(e-line>18 && !strncasecmp(line,"Transfer-Encoding:",18)){
     for(const char *t=line+18;t+7<=e;t++)if(!strncasecmp(t,"chunked",7))chunked=true;
   }
   line=e+2;
 }
 if(chunked){
   /* ESP-IDF terminates chunked responses with a zero-size final chunk. */
   const char *end=buffer+size;
   if(body_bytes>=5&&memcmp(end-5,"0\r\n\r\n",5)==0)return true;
   if(body_bytes>=7&&memcmp(end-7,"\r\n0\r\n\r\n",7)==0)return true;
   return false;
 }
 return content_len>=0&&content_len<RESPONSE_CAP&&body_bytes>=content_len;
}
static int get_request(const char *path){
    if(!net_ok || !response){
        snprintf(net_status,sizeof(net_status),"Socket service unavailable");
        return -1;
    }
    /* The background worker already polls AC status every 1.3 seconds.
     * Do not query connection services on every telemetry HTTP request.
     * v0.4 returned here for AC2 without opening any socket. AC2 may
     * represent a network not cleared for Internet, but it can still be
     * useful for local traffic. Let the TCP/HTTP result decide instead.
     * If AC reports truly disconnected, do not attempt a socket.
     */
    if(!wifi_candidate){
        snprintf(net_status,sizeof(net_status),"No Wi-Fi link (AC%lu) - X reconnect",
                 (unsigned long)wifi_status);
        return -1;
    }
    int fd=connect_socket();
    if(fd<0){
        snprintf(net_status,sizeof(net_status),"TCP %s:%d errno %d (AC%lu)",
                 HOST_IP,HOST_PORT,errno,(unsigned long)wifi_status);
        return -1;
    }
    char request[256];
    int count=snprintf(request,sizeof(request),
      "GET %s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nConnection: close\r\n\r\n",
      path,HOST_IP);
    int sent=0;
    while(sent<count){
        if(readable(fd,1,3000)<=0){
            snprintf(net_status,sizeof(net_status),"HTTP send timeout");
            close(fd);
            return -1;
        }
        int n=send(fd,request+sent,count-sent,0);
        if(n<0 && (errno==EWOULDBLOCK || errno==EAGAIN))continue;
        if(n<=0){
            snprintf(net_status,sizeof(net_status),"HTTP send errno %d",errno);
            close(fd);
            return -1;
        }
        sent+=n;
    }
    int used=0;
    bool truncated=false;
    while(used<RESPONSE_CAP-1){
        int sr=readable(fd,0,3000);
        if(sr<=0)break;
        int n=recv(fd,response+used,RESPONSE_CAP-1-used,0);
        if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK))continue;
        if(n<=0)break;
        used+=n;
        response[used]=0;
        if(http_entity_complete(response,used))break;
    }
    if(used>=RESPONSE_CAP-1)truncated=true;
    close(fd);
    response[used]=0;
    bytes_rx=used;
    last_http=0;
    if(!used){
        snprintf(net_status,sizeof(net_status),"TCP open, HTTP response missing");
        return -1;
    }
    sscanf(response,"HTTP/%*s %d",&last_http);
    snprintf(net_status,sizeof(net_status),"GET %s: HTTP %d, %d bytes%s",
             path,last_http,used,truncated?" TRUNCATED":"");
    return used;
}
static char *http_body(void){
    char *p=strstr(response,"\r\n\r\n");
    return p?p+4:NULL;
}
/* ESP-IDF serves /api/ini/file as chunked HTTP/1.1. Remove HTTP chunk
 * framing in-place before passing the bytes to the real INI parser.
 */
static int decode_http_body(void){
    char *body=http_body();
    if(!body)return -1;
    char *end=response+bytes_rx;
    char *headers_end=body-4;
    bool chunked=false;
    for(char *lineptr=response;lineptr<headers_end;){
        char *next=strstr(lineptr,"\r\n");
        if(!next || next>headers_end)break;
        if(!strncasecmp(lineptr,"Transfer-Encoding:",18)){
            const char *value=lineptr+18;
            while(value<next && isspace((unsigned char)*value))value++;
            if(next-value>=7 && !strncasecmp(value,"chunked",7))chunked=true;
        }
        lineptr=next+2;
    }
    if(!chunked)return (int)(end-body);
    char *src=body,*dst=body;
    while(src<end){
        char *nl=memchr(src,'\n',(size_t)(end-src));
        if(!nl || nl-src>24)return -1;
        char length_buf[25];
        int len=(int)(nl-src);
        memcpy(length_buf,src,len);
        length_buf[len]=0;
        char *stop=NULL;
        unsigned long size=strtoul(length_buf,&stop,16);
        if(stop==length_buf || size>(unsigned long)(end-(nl+1)))return -1;
        src=nl+1;
        if(size==0){
            *dst=0;
            return (int)(dst-body);
        }
        memmove(dst,src,size);
        dst+=size;
        src+=size;
        if(src+1<end && src[0]=='\r' && src[1]=='\n')src+=2;
        else if(src<end && *src=='\n')src++;
        else return -1;
    }
    return -1;
}
/* POST one JSON operation via the BrickOS HTTP API. Network requests are
 * serialized by the worker; write/burn commands are NEVER retried on uncertain
 * outcomes. Full-page ESP32 backups need a longer first-response deadline. */
static bool json_number(const char *body,const char *name,double *out);
static bool json_bool(const char *body,const char *name,bool *out);
static bool json_string(const char *body,const char *name,char *out,int cap);
static int post_api(const char *path,const char *payload,int first_response_ms){
 if(!net_ok||!wifi_candidate||!response||!path||!payload)return -1;
 int fd=connect_socket();
 if(fd<0){snprintf(net_status,sizeof(net_status),"HTTP POST connect errno %d",errno);return -1;}
 const int blen=(int)strlen(payload);
 char head[320];
 int hlen=snprintf(head,sizeof(head),
      "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\n"
      "Content-Length: %d\r\nConnection: close\r\n\r\n",path,HOST_IP,blen);
 if(hlen<=0||hlen>=(int)sizeof(head)){close(fd);return -1;}
 const char *parts[]={head,payload};int lens[]={hlen,blen};
 for(int i=0;i<2;i++)for(int at=0;at<lens[i];){
   if(readable(fd,1,4000)<=0){close(fd);note("ESP32 HTTP send timeout");return -1;}
   int n=send(fd,parts[i]+at,lens[i]-at,0);
   if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))continue;
   if(n<=0){close(fd);note("ESP32 HTTP send failed");return -1;}at+=n;
 }
 int used=0;bool complete=false;
 while(used<RESPONSE_CAP-1){
   int r=readable(fd,0,used?4500:first_response_ms);
   if(r<=0)break;
   int n=recv(fd,response+used,RESPONSE_CAP-1-used,0);
   if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))continue;
   if(n<=0)break;
   used+=n;response[used]=0;
   if(http_entity_complete(response,used)){complete=true;break;}
 }
 close(fd);response[used]=0;bytes_rx=used;last_http=0;
 if(!complete){note(used?"ESP32 HTTP response incomplete":"ESP32 HTTP reply timeout");return -1;}
 sscanf(response,"HTTP/%*s %d",&last_http);
 char *body=http_body();
 if(last_http!=200){
   char err[110];
   if(body&&json_string(body,"error",err,sizeof(err)))snprintf(ui_note,sizeof(ui_note),"ESP32: %.105s",err);
   else snprintf(ui_note,sizeof(ui_note),"ESP32 HTTP %d at %s",last_http,path);
   return -1;
 }
 if(!body||!strstr(body,"\"ok\":true")){
   note("ESP32 did not confirm API request");return -1;
 }
 return used;
}
/* Encode the exact ECU signature in a JSON string, never assume its charset. */
static bool escape_json_string(const char *src,char *dst,size_t capacity){
 size_t at=0;
 for(const unsigned char *p=(const unsigned char*)src;*p;p++){
    unsigned char c=*p;
    if(c<32||c>=127)return false;
    if(c=='"'||c=='\\'){
      if(at+2>=capacity)return false;
      dst[at++]='\\';dst[at++]=(char)c;
    }else{
      if(at+1>=capacity)return false;
      dst[at++]=(char)c;
    }
 }
 if(at>=capacity)return false;
 dst[at]=0;return true;
}
/* C/B requests require the current ECU signature and an armed session.
 * ECU reply status must be 0 (or 4, accepted for rusEFI burn). */
static int post_ts(const char *command,const char *args){
 char payload[1400],sig[320];bool write=!strcmp(command,"C")||!strcmp(command,"B");
 if(write&&(!*ecu_signature||!escape_json_string(ecu_signature,sig,sizeof(sig)))){
    note("Cannot encode connected ECU signature");return -1;
 }
 int size=write?
    snprintf(payload,sizeof(payload),"{\"cmd\":\"%s\",\"args\":\"%s\",\"timeout_ms\":9000,\"signature\":\"%s\"}",command,args,sig):
    snprintf(payload,sizeof(payload),"{\"cmd\":\"%s\",\"args\":\"%s\",\"timeout_ms\":6000}",command,args);
 if(size<0||size>=(int)sizeof(payload)){note("ECU command too long");return -1;}
 int n=post_api("/api/ts",payload,12500);
 if(n<0)return -1;
 char *body=http_body();double status=-1;
 if(!body||!json_number(body,"status",&status)||
    (status!=0&&!(write&&command[0]=='B'&&status==4))){
     note("ECU rejected the TunerStudio command");return -1;
 }
 return n;
}
static bool json_number(const char*body,const char*name,double*out){char needle[80];snprintf(needle,sizeof(needle),"\"%s\"",name);const char*p=strstr(body,needle);if(!p)return false;p+=strlen(needle);while(*p&&*p!=':'&&*p!='\n')p++;if(*p!=':')return false;p++;while(isspace((unsigned char)*p))p++;char*end=NULL;double d=strtod(p,&end);if(p==end||!isfinite(d))return false;*out=d;return true;}
static bool json_bool(const char*body,const char*name,bool*out){char needle[80];snprintf(needle,sizeof(needle),"\"%s\"",name);const char*p=strstr(body,needle);if(!p)return false;p=strchr(p,':');if(!p)return false;p++;while(isspace((unsigned char)*p))p++;if(!strncmp(p,"true",4)){*out=true;return true;}if(!strncmp(p,"false",5)){*out=false;return true;}return false;}
static bool json_string(const char*body,const char*name,char*out,int cap){char needle[80];snprintf(needle,sizeof(needle),"\"%s\"",name);const char*p=strstr(body,needle);if(!p)return false;p=strchr(p,':');if(!p)return false;p++;while(isspace((unsigned char)*p))p++;if(*p++!='"')return false;int at=0;while(*p&&*p!='"'&&at<cap-1){if(*p=='\\'&&p[1])p++;out[at++]=*p++;}out[at]=0;return true;}
static void decode_status(void){if(last_http!=200)return;char*b=http_body();if(!b)return;
 const bool was_connected=ecu_session;
 char old_signature[160];copy_text(old_signature,sizeof(old_signature),ecu_signature);
 bool reported_session=false;
 /* A partial/malformed status must not be treated as a successful reconnect. */
 if(!json_bool(b,"ecu_session_connected",&reported_session))return;
 double v;if(json_number(b,"wifi_connected_clients",&v))esp_wifi_clients=(u32)v;if(json_number(b,"wifi_last_disconnect_reason",&v))esp_disconnect_reason=(u32)v;
 ecu_session=reported_session;json_bool(b,"write_compiled",&write_compiled);json_bool(b,"write_armed",&write_armed);
 json_bool(b,"write_access_ready",&esp_write_access_ready);
 json_bool(b,"backup_storage_ready",&esp_backup_ready);
 json_bool(b,"stim_remote_stop_supported",&stim_stop_supported);
 json_bool(b,"stim_remote_start_supported",&stim_start_supported);
 json_string(b,"stim_running_state",stim_server_state,sizeof(stim_server_state));
 stim_on_by_bridge=!strcmp(stim_server_state,"START_ACKNOWLEDGED");
 json_string(b,"brick_os_build",esp_firmware_build,sizeof(esp_firmware_build));
 json_string(b,"ecu_signature",ecu_signature,sizeof(ecu_signature));esp_status_valid=true;
 if(!ecu_session || !was_connected || strcmp(old_signature,ecu_signature)){
    if(ini_flags_requested||ini_flags_signature[0])ini_drop_dynamic_conversions();
 }
 if(!ecu_session && was_connected){
    fault_session_was_lost=true;
    fault_poll_requested=true;
    fault_recheck_after_reconnect=true;
 }
 if(ecu_session && (!was_connected || fault_session_was_lost)){
    /* A new verified ECU session must not inherit the old session's alert.
     * Immediately query retrieveConfigError again; a genuine current error
     * will reappear only after the normal two matching valid responses. */
    fault_active=false;fault_message[0]=0;fault_link_count=0;
    fault_candidate[0]=0;fault_candidate_count=0;fault_clear_count=0;
    fault_last_success=0;fault_session_was_lost=false;
    fault_recheck_after_reconnect=true;fault_poll_requested=true;
    fault_last_enqueue=0;
    if(ui_page==PAGE_FAULT){ui_page=fault_return_page;selected=scroll_y=0;}
    if(ui_page==PAGE_FIELD&&field_return_page==PAGE_FAULT)
       field_return_page=fault_return_page;
    note("New ECU session: previous fault reset; checking current errors");
 }
 /* A different controller is not the controller that raised the old fault. */
 if(ecu_signature[0]&&fault_ecu_signature[0]&&strcmp(fault_ecu_signature,ecu_signature)){
    fault_active=false;fault_message[0]=0;fault_link_count=0;fault_clear_count=0;
    fault_candidate[0]=0;fault_candidate_count=0;
    snprintf(fault_ecu_signature,sizeof(fault_ecu_signature),"%s",ecu_signature);
    fault_poll_requested=true;
 }
}
static int get_hex_reply(uint8_t*dst,int max){char*b=http_body();if(!b||last_http!=200)return -1;char*p=strstr(b,"\"hex\"");if(!p)return -1;p=strchr(p,':');if(!p)return -1;p++;while(isspace((unsigned char)*p))p++;if(*p++!='"')return -1;int n=0;while(isxdigit((unsigned char)p[0])&&isxdigit((unsigned char)p[1])&&n<max){char x[3]={p[0],p[1],0};dst[n++]=(uint8_t)strtoul(x,NULL,16);p+=2;}if(*p!='"')return -1;return n;}
static void tohex(const uint8_t*in,int n,char*out){static const char h[]="0123456789abcdef";for(int i=0;i<n;i++){out[2*i]=h[in[i]>>4];out[2*i+1]=h[in[i]&15];}out[n*2]=0;}
static int read_bytes(int page,int offset,uint8_t*dest,int length){if(page<1||page>64||length<1||offset<0||offset+length>65536)return -1;
 int id=page<=page_ids_count?page_ids[page-1]:(page-1)<<8;if(id<0||id>65535){note("INI page identifier invalid");return -1;}
 for(int at=0;at<length;){int n=length-at;if(n>512)n=512;uint8_t args[6]={(uint8_t)id,(uint8_t)(id>>8),(uint8_t)(offset+at),(uint8_t)((offset+at)>>8),(uint8_t)n,(uint8_t)(n>>8)};char hex[13];tohex(args,6,hex);
 if(post_ts("R",hex)<0)return -1;int got=get_hex_reply(dest+at,n);if(got!=n){note("ECU returned unexpected calibration length");return -1;}at+=n;}return length;}
static double get_number(const uint8_t*,int,int,int);
/* Read only named INI scalar/bit fields; failed reads never produce a guessed scale. */
static bool read_cal_constant(const char *name,double *value){
 Field *f=lookup((char*)name,false);
 if(!f||f->bytes<1||f->bytes>8||f->width*f->height!=1)return false;
 uint8_t data[8];if(read_bytes(f->page,f->offset,data,f->bytes)!=f->bytes)return false;
 double raw=get_number(data,f->bytes,0,f->type);
 if(!isfinite(raw))return false;
 if(f->kind==K_BITS){
   uint64_t mask=(1ULL<<(f->bithi-f->bitlo+1))-1ULL;
   *value=(double)((((uint64_t)(int64_t)raw)>>f->bitlo)&mask);
 }else if(f->validScale)*value=raw*f->scale+f->translate;
 else return false;
 return true;
}
/* Configurable, real, binary BLV1 samples, decoded using [OutputChannels]. */
/* Reset only for a new ECU definition, never merely for a lost Wi-Fi packet.
 * Stale readings remain visibly cached and must not enter new log samples. */
static void clear_live(void){
 live_valid=false;live_stale=false;trace_found=false;live_received_at=0;live_bytes=0;live_seq=0;
 for(int i=0;i<9;i++)has_value[i]=false;
 for(int i=0;i<GAUGE_SLOTS;i++){
    gauge_hold_reset(&view_has[i]);needle_ready[i]=false;gauge_stats_reset(i);
 }
}
static double get_number(const uint8_t*buf,int len,int offset,int type){int sz=dsize(type);if(offset<0||sz<=0||offset+sz>len)return NAN;uint64_t u=0;
 if(!ini_big_endian){for(int i=sz-1;i>=0;i--)u=(u<<8)|buf[offset+i];}else{for(int i=0;i<sz;i++)u=(u<<8)|buf[offset+i];}
 if(type==T_U08||type==T_U16||type==T_U32)return (double)u;
 if(type==T_S08)return (int8_t)u;if(type==T_S16)return (int16_t)u;if(type==T_S32)return (int32_t)u;
 if(type==T_F32){uint32_t bits=(uint32_t)u;float f;memcpy(&f,&bits,4);return f;}if(type==T_F64){double d;memcpy(&d,&u,8);return d;}return NAN;}
static double field_value(const uint8_t*bytes,int length,const Field*f,int index){if(!f||!f->validScale||index<0||index>=f->width*f->height)return NAN;
 double raw=get_number(bytes,length,index*dsize(f->type),f->type);if(!isfinite(raw))return NAN;
 if(f->kind==K_BITS){uint64_t mask=(1ULL<<(f->bithi-f->bitlo+1))-1;return (double)(((uint64_t)(int64_t)raw>>f->bitlo)&mask);}
 return raw*f->scale+f->translate;}
static bool set_number(uint8_t*bytes,int length,const Field*f,int index,double value){if(!f||f->readOnly||!f->validScale||!isfinite(value)||index<0||index>=f->width*f->height)return false;
 int sz=dsize(f->type),off=index*sz;
 if(off+sz>length)return false;
 if(f->kind!=K_BITS&&f->limitsKnown&&(value<f->low-1e-9||value>f->high+1e-9))return false;
 double raw=(value-f->translate)/f->scale;
 if(f->kind==K_BITS){uint64_t mask=(1ULL<<(f->bithi-f->bitlo+1))-1;if(floor(value)!=value||value<0||value>(double)mask)return false;
 double old=get_number(bytes,length,off,f->type);if(!isfinite(old))return false;
 uint64_t b=(uint64_t)(int64_t)old,shiftmask=mask<<f->bitlo;
 b=(b&~shiftmask)|((((uint64_t)value)&mask)<<f->bitlo);
 for(int i=0;i<sz;i++){int j=ini_big_endian?(sz-i-1):i;bytes[off+j]=(uint8_t)(b>>(i*8));}return true;}
 if(!isfinite(raw))return false;
 if(f->type<T_F32){double low=0,high=0;switch(f->type){case T_U08:low=0;high=255;break;case T_S08:low=-128;high=127;break;case T_U16:low=0;high=65535;break;case T_S16:low=-32768;high=32767;break;case T_U32:low=0;high=4294967295.0;break;case T_S32:low=-2147483648.0;high=2147483647.0;break;default:return false;}raw=round(raw);if(raw<low||raw>high)return false;}
 uint64_t bits=0;if(f->type==T_F32){float v=(float)raw;uint32_t b;memcpy(&b,&v,4);bits=b;}else if(f->type==T_F64){memcpy(&bits,&raw,8);}else bits=(uint64_t)(int64_t)raw;
 for(int i=0;i<sz;i++){int j=ini_big_endian?(sz-i-1):i;bytes[off+j]=(uint8_t)(bits>>(i*8));}return true;}
static void start_logging(void){
 log_count=0;log_begin_ms=osGetTime();log_recording=true;
 record_session_connected=ecu_session;
 snprintf(record_ecu_signature,sizeof(record_ecu_signature),"%.159s",ecu_session?ecu_signature:"NOT VERIFIED AT START");
 snprintf(record_ini_signature,sizeof(record_ini_signature),"%.159s",menu_loaded?ini_signature:"NO INI LOADED AT START");
 snprintf(record_bridge_build,sizeof(record_bridge_build),"%.31s",esp_firmware_build);
 for(int i=0;i<LOG_FIELDS;i++)copy_text(log_channel_names[i],sizeof(log_channel_names[i]),gauge_names[i]);
 note("Recording real telemetry in RAM");
}
static void log_sample(void){
 if(!log_recording||log_count>=LOG_MAX_SAMPLES)return;
 LogSample *r=&log_buffer[log_count++];
 u64 now=osGetTime();r->t_ms=(u32)(now-log_begin_ms);r->valid=0;
 for(int i=0;i<LOG_FIELDS;i++){
   r->values[i]=view_has[i]?view_values[i]:0.0f;
   if(view_has[i])r->valid|=(u16)(1u<<i);
 }
 if(log_count>=LOG_MAX_SAMPLES){log_recording=false;
   note("RAM log full: stop and export to SD");}
}
static bool export_csv_log(void){
 if(log_recording){note("Stop recording before exporting");return false;}
 if(!log_count){note("No telemetry samples recorded");return false;}
 mkdir("sdmc:/BrickECU",0777);mkdir("sdmc:/BrickECU/logs",0777);
 char name[96];FILE*fp=NULL;
 for(int n=1;n<=9999;n++){
   snprintf(name,sizeof(name),"sdmc:/BrickECU/logs/bricktuner-%04d.csv",n);
   FILE*existing=fopen(name,"rb");if(existing){fclose(existing);continue;}
   fp=fopen(name,"wb");break;
 }
 if(!fp){note("Cannot create log file on SD");return false;}
 setvbuf(fp,NULL,_IOFBF,16384);
 fprintf(fp,"elapsed_ms");
 for(int i=0;i<LOG_FIELDS;i++)fprintf(fp,",%s",log_channel_names[i]);
 fputc('\n',fp);
 for(int j=0;j<log_count;j++){
   LogSample *r=&log_buffer[j];fprintf(fp,"%lu",(unsigned long)r->t_ms);
   for(int i=0;i<LOG_FIELDS;i++){
     fputc(',',fp);if(r->valid&(1u<<i))fprintf(fp,"%.4f",r->values[i]);
   }
   fputc('\n',fp);
 }
 bool ok=ferror(fp)==0;if(fclose(fp)!=0)ok=false;
 if(!ok){remove(name);note("SD export incomplete: file removed");return false;}
 copy_text(log_export_path,sizeof(log_export_path),name);
 /* Separate metadata sidecar, preserving CSV import compatibility. A failed
  * sidecar is reported, never disguised as a complete session record. */
 char meta[140];int mn=snprintf(meta,sizeof(meta),"%s.session.txt",name);
 bool metadata_ok=false;
 if(mn>0&&(size_t)mn<sizeof(meta)){
    FILE *mf=fopen(meta,"wb");
    if(mf){
      fprintf(mf,"Brick Tuner Suite v3.4 session metadata\n");
      fprintf(mf,"ECU connected at start: %s\n",record_session_connected?"YES":"NO");
      fprintf(mf,"ECU signature at start: %.159s\n",record_ecu_signature);
      fprintf(mf,"INI signature at start: %.159s\n",record_ini_signature);
      fprintf(mf,"ESP32 build at start: %.31s\n",record_bridge_build);
      fprintf(mf,"Recorded samples: %d\n",log_count);
      fprintf(mf,"Sample duration ms: %lu\n",(unsigned long)log_buffer[log_count-1].t_ms);
      fprintf(mf,"Channel names: ");
      for(int k=0;k<LOG_FIELDS;k++)fprintf(mf,"%s%s",k?", ":"",log_channel_names[k]);
      fputc('\n',mf);
      metadata_ok=!ferror(mf);if(fclose(mf)!=0)metadata_ok=false;
      if(!metadata_ok)remove(meta);
    }
 }
 note(metadata_ok?"CSV and session metadata saved to SD":"CSV saved; session metadata unavailable");return true;
}
/* Only list files generated by this application's CSV exporter. Names contain
 * digits only: imported filenames never become arbitrary SD paths. */
static bool log_basename_valid(const char *name){
 if(!name||strlen(name)!=19||
    (strncmp(name,"dreamtuner-",11)&&strncmp(name,"bricktuner-",11))||
    strcmp(name+15,".csv"))return false;
 for(int i=11;i<15;i++)if(name[i]<'0'||name[i]>'9')return false;
 return true;
}
/* Never accept a caller-specified filesystem path. The sidecar is derived
 * exclusively from a validated, app-generated CSV basename. */
static bool log_marks_path(char *out,size_t cap){
 if(!log_basename_valid(replay_filename)||cap<96)return false;
 int n=snprintf(out,cap,"sdmc:/BrickECU/logs/%s.marks",replay_filename);
 return n>0&&(size_t)n<cap;
}
static void replay_marks_load(void){
 replay_mark_count=0;
 char path[128];if(!log_marks_path(path,sizeof(path))||!replay_count)return;
 FILE*fp=fopen(path,"rb");if(!fp)return;
 char line[80];
 if(!fgets(line,sizeof(line),fp)||strncmp(line,"BTS-MARKS-1",11)){fclose(fp);return;}
 while(replay_mark_count<LOG_MARK_CAP&&fgets(line,sizeof(line),fp)){
   char*end=NULL;unsigned long n=strtoul(line,&end,10);
   if(end==line||(*end!='\n'&&*end!='\r'&&*end!=0)||n>UINT32_MAX)continue;
   u32 t=(u32)n;
   if(t<replay_buffer[0].t_ms||t>replay_buffer[replay_count-1].t_ms)continue;
   int k=0;while(k<replay_mark_count&&replay_marks[k]<t)k++;
   if(k<replay_mark_count&&replay_marks[k]==t)continue;
   memmove(replay_marks+k+1,replay_marks+k,(size_t)(replay_mark_count-k)*sizeof(u32));
   replay_marks[k]=t;replay_mark_count++;
 }
 fclose(fp);
}
static bool replay_marks_save(void){
 char path[128];if(!log_marks_path(path,sizeof(path)))return false;
 char tmp[140];int n=snprintf(tmp,sizeof(tmp),"%s.tmp",path);
 if(n<=0||(size_t)n>=sizeof(tmp))return false;
 FILE*fp=fopen(tmp,"wb");if(!fp)return false;
 fputs("BTS-MARKS-1\n",fp);
 for(int i=0;i<replay_mark_count;i++)fprintf(fp,"%lu\n",(unsigned long)replay_marks[i]);
 bool ok=!ferror(fp);if(fclose(fp)!=0)ok=false;
 if(!ok){remove(tmp);return false;}
 /* On devkitPro FAT this replaces an existing sidecar; preserve old sidecar
  * if rename fails rather than falsely report success. */
 if(rename(tmp,path)!=0){
    /* Some FAT implementations refuse an overwrite. Try a direct write. */
    FILE*existing=fopen(path,"wb");if(!existing){remove(tmp);return false;}
    fputs("BTS-MARKS-1\n",existing);
    for(int i=0;i<replay_mark_count;i++)fprintf(existing,"%lu\n",(unsigned long)replay_marks[i]);
    ok=!ferror(existing);if(fclose(existing)!=0)ok=false;
    remove(tmp);return ok;
 }
 return true;
}
static void replay_mark_toggle(void){
 if(!replay_count)return;
 u32 t=replay_time_ms;int at=0;
 while(at<replay_mark_count&&replay_marks[at]<t)at++;
 int near=-1;
 if(at<replay_mark_count&&replay_marks[at]-t<=250U)near=at;
 if(at>0&&t-replay_marks[at-1]<=250U)near=at-1;
 if(near>=0){
   memmove(replay_marks+near,replay_marks+near+1,(size_t)(replay_mark_count-near-1)*sizeof(u32));
   replay_mark_count--;
   note(replay_marks_save()?"Log bookmark removed and saved":"Log bookmark removed; SD save failed");
 }else{
   if(replay_mark_count>=LOG_MARK_CAP){note("64 log bookmarks maximum");return;}
   memmove(replay_marks+at+1,replay_marks+at,(size_t)(replay_mark_count-at)*sizeof(u32));
   replay_marks[at]=t;replay_mark_count++;
   note(replay_marks_save()?"Log bookmark saved to SD":"Log bookmark added; SD save failed");
 }
}
static void replay_seek(u32 target);
static void insights_refresh(void){
 insight_dirty=false;insight_valid=false;
 if(replay_count<1||insight_channel<0||insight_channel>=LOG_FIELDS)return;
 u32 window=insight_window==1?10000U:insight_window==2?30000U:insight_window==3?60000U:0U;
 u32 end=replay_time_ms;
 u32 begin=window&&end>window?end-window:0;
 insight_source_start=0;
 if(window){while(insight_source_start<replay_count && replay_buffer[insight_source_start].t_ms<begin)insight_source_start++;}
 insight_source_count=0;
 for(int i=insight_source_start;i<replay_count;i++){
    LogSample *r=&replay_buffer[i];if(window&&r->t_ms>end)break;
    int dst=insight_source_count++;
    insight_flat_values[dst]=r->values[insight_channel];
    insight_flat_times[dst]=r->t_ms;
    insight_flat_valid[dst]=!!(r->valid&(1u<<insight_channel));
 }
 if(insight_source_count)
   insight_valid=bts_log_inspect(insight_flat_values,insight_flat_times,insight_flat_valid,
            insight_source_count,&insight_stats,insight_scratch);
}
static void quality_open(void){
 if(replay_count<1){note("Load a saved CSV log before reviewing data quality");return;}
 quality_valid_channels=0;
 for(int ch=0;ch<LOG_FIELDS;ch++){
    for(int i=0;i<replay_count;i++){
       insight_flat_values[i]=replay_buffer[i].values[ch];
       insight_flat_valid[i]=!!(replay_buffer[i].valid&(1u<<ch));
       insight_flat_times[i]=replay_buffer[i].t_ms;
    }
    if(bts_signal_quality(insight_flat_values,insight_flat_times,insight_flat_valid,
          replay_count,&log_quality[ch]) && log_quality[ch].valid)quality_valid_channels++;
 }
 quality_channel=0;ui_page=PAGE_QUALITY;replay_playing=false;
 note("Quality report: recorded observations only, not ECU fault codes");
}
static void quality_jump_gap(void){
 const BtsSignalQuality *q=&log_quality[quality_channel];
 if(!q->maximum_observed_gap_ms){note("No observed time gap in this log");return;}
 replay_seek(q->time_of_largest_gap_ms);
 ui_page=PAGE_REPLAY;replay_playing=false;
 note("Cursor moved to end of largest observed logging gap");
}
static void quality_export(void){
 if(!replay_count||!log_basename_valid(replay_filename))return;
 char path[106];FILE*fp=NULL;
 for(int n=1;n<=999;n++){
    snprintf(path,sizeof(path),"sdmc:/BrickECU/logs/quality-%03d.txt",n);
    FILE*old=fopen(path,"rb");if(old){fclose(old);continue;}
    fp=fopen(path,"wb");break;
 }
 if(!fp){note("Cannot create quality report on SD");return;}
 fprintf(fp,"Brick Tuner Suite v3.4 / Saved log availability audit\n");
 fprintf(fp,"Source log: %s\nRecords: %d\n",replay_filename,replay_count);
 fputs("Missing values and long intervals are observations, not verified ECU faults.\n",fp);
 for(int ch=0;ch<LOG_FIELDS;ch++){
    const BtsSignalQuality*q=&log_quality[ch];
    fprintf(fp,"%s: %u valid, %u missing, max observed gap %u ms, %u long gaps, longest unchanged reading run %u\n",
      log_channel_names[ch],q->valid,q->missing,q->maximum_observed_gap_ms,
      q->long_gaps,q->longest_constant_run);
 }
 bool ok=!ferror(fp);if(fclose(fp)!=0)ok=false;
 if(!ok){remove(path);note("Quality report incomplete, removed");return;}
 note("Saved recorded-data quality report to SD /BrickECU/logs");
}
static void insights_open(void){
 if(replay_count<1){note("Open a recorded log before analysis");return;}
 replay_playing=false;insight_channel=0;insight_window=0;insight_view=0;
 insight_dirty=true;ui_page=PAGE_INSIGHTS;
 note("Read-only log analysis. L/R channel; X plot; Y time window");
}
/* Jump only to events that exist in the saved log, never guessed telemetry. */
static void insights_jump_event(bool gap){
 if(insight_dirty)insights_refresh();
 if(!insight_valid||replay_count<2){note("No analyzable saved samples");return;}
 int found=-1;
 if(gap){
    uint32_t max_gap=0;
    for(int i=1;i<replay_count;i++){
       uint32_t delta=replay_buffer[i].t_ms-replay_buffer[i-1].t_ms;
       if(delta>max_gap){max_gap=delta;found=i;}
    }
    if(max_gap==0){note("No nonzero time gap recorded");return;}
 }else{
    if(insight_stats.valid<3||insight_stats.maximum-insight_stats.minimum<1e-9){
       note("Insufficient variation for recorded extreme search");return;
    }
    for(int pass=0;pass<2&&found<0;pass++){
       int from=pass==0?replay_position+1:0;
       int to=pass==0?replay_count:replay_position+1;
       for(int i=from;i<to;i++){
          LogSample *r=&replay_buffer[i];
          if(!(r->valid&(1u<<insight_channel)))continue;
          double v=r->values[insight_channel];
          if(isfinite(v)&&(v<insight_stats.p05||v>insight_stats.p95)){
             found=i;break;
          }
       }
    }
    if(found<0){note("No recorded samples beyond the selected percentiles");return;}
 }
 replay_seek(replay_buffer[found].t_ms);
 insight_dirty=true;ui_page=PAGE_REPLAY;replay_playing=false;
 note(gap?"Cursor moved to largest recorded time gap":"Cursor moved to an actual percentile extreme");
}
static void insights_export(void){
 if(insight_dirty)insights_refresh();
 if(!insight_valid||!log_basename_valid(replay_filename)){
    note("No valid recorded data to export");return;
 }
 char path[150];FILE *fp=NULL;
 for(int i=1;i<=999;i++){
   snprintf(path,sizeof(path),"sdmc:/BrickECU/logs/%s-analysis-%03d.txt",replay_filename,i);
   FILE *existing=fopen(path,"rb");if(existing){fclose(existing);continue;}
   fp=fopen(path,"wb");break;
 }
 if(!fp){note("Unable to create unique analysis report on SD");return;}
 const BtsLogInsights *v=&insight_stats;
 fprintf(fp,"Brick Tuner Suite v3.4 / Read-only log analysis\n");
 fprintf(fp,"Source CSV: %s\nRecorded channel: %s\n",replay_filename,log_channel_names[insight_channel]);
 fprintf(fp,"Time scope: %s\n",insight_window==0?"Complete log":insight_window==1?"Last 10s to cursor":insight_window==2?"Last 30s to cursor":"Last 60s to cursor");
 fprintf(fp,"Samples in window: %u\nValid sensor samples: %u\nUnavailable samples: %u\n",v->samples,v->valid,v->missing);
 fprintf(fp,"Duration ms: %u\nLong sample gaps detected: %u\nMaximum observed gap ms: %u\n",v->duration_ms,v->gaps,v->max_gap_ms);
 if(v->valid)fprintf(fp,"Min: %.6f\nP05: %.6f\nMean: %.6f\nMedian: %.6f\nP95: %.6f\nMax: %.6f\n",v->minimum,v->p05,v->mean,v->median,v->p95,v->maximum);
 else fputs("No valid sensor samples. No numeric statistics calculated.\n",fp);
 fputs("Historical data only. This report does not modify ECU calibration.\n",fp);
 bool ok=!ferror(fp);if(fclose(fp)!=0)ok=false;
 if(!ok){remove(path);note("Analysis report incomplete and removed");return;}
 note("Analysis report exported to SD /BrickECU/logs");
}

static void replay_seek(u32 target);
static void replay_mark_jump(int direction){
 if(!replay_mark_count){note("No log bookmarks. SELECT adds one");return;}
 if(direction>0){
   for(int i=0;i<replay_mark_count;i++)if(replay_marks[i]>replay_time_ms+10){replay_seek(replay_marks[i]);return;}
   replay_seek(replay_marks[0]);
 }else{
   for(int i=replay_mark_count-1;i>=0;i--)if(replay_marks[i]+10<replay_time_ms){replay_seek(replay_marks[i]);return;}
   replay_seek(replay_marks[replay_mark_count-1]);
 }
}
static void refresh_log_files(void){
 log_file_count=0;
 DIR *dir=opendir("sdmc:/BrickECU/logs");if(!dir){note("No saved logs on SD");return;}
 struct dirent *e;
 while((e=readdir(dir))!=NULL){
   if(!log_basename_valid(e->d_name))continue;
   if(log_file_count>=LOG_FILE_CAP)continue;
   int pos=log_file_count++;
   while(pos>0&&strcmp(log_filenames[pos-1],e->d_name)>0){
      memcpy(log_filenames[pos],log_filenames[pos-1],sizeof(log_filenames[pos]));pos--;
   }
   copy_text(log_filenames[pos],sizeof(log_filenames[pos]),e->d_name);
 }
 closedir(dir);
 if(log_browser_selected>=log_file_count)log_browser_selected=log_file_count?log_file_count-1:0;
 log_browser_scroll=0;
}
/* Delete only a known, selected app-generated CSV. No arbitrary paths;
 * sidecars are removed after the CSV, never instead of the CSV. */
static void log_delete_request(int index){
 if(log_recording){note("Stop recording before deleting saved logs");return;}
 if(index<0||index>=log_file_count||!log_basename_valid(log_filenames[index]))return;
 copy_text(log_delete_name,sizeof(log_delete_name),log_filenames[index]);
 log_delete_pending=true;
 note("Delete selected CSV? A CONFIRM / B CANCEL");
}
static void log_delete_confirm(void){
 if(!log_delete_pending)return;
 log_delete_pending=false;
 if(log_recording||!log_basename_valid(log_delete_name)){
    note("Log deletion blocked");return;
 }
 char path[128];
 int n=snprintf(path,sizeof(path),"sdmc:/BrickECU/logs/%s",log_delete_name);
 if(n<1||(size_t)n>=sizeof(path)||remove(path)!=0){
    note("Log deletion failed; file remains on SD");return;
 }
 char sidecar[160];
 n=snprintf(sidecar,sizeof(sidecar),"%s.marks",path);
 if(n>0&&(size_t)n<sizeof(sidecar))remove(sidecar);
 n=snprintf(sidecar,sizeof(sidecar),"%s.session.txt",path);
 if(n>0&&(size_t)n<sizeof(sidecar))remove(sidecar);
 /* Never keep deleted replay/compare data visible as though still saved. */
 if(!strcmp(replay_filename,log_delete_name)){
    replay_filename[0]=0;replay_count=0;replay_playing=false;
    replay_mark_count=0;replay_trace_active=false;
 }
 if(!strcmp(compare_filename,log_delete_name)){
    compare_filename[0]=0;free(compare_buffer);compare_buffer=NULL;compare_count=0;
 }
 refresh_log_files();
 selected=log_browser_selected;
 if(selected>=log_file_count)selected=log_file_count?log_file_count-1:0;
 bts_audio_play(BTS_SOUND_UNDO,event_audio_enabled);
 note("Log deleted from SD; KidPix played");
}
static bool log_parse_header_named(char *line,char names[LOG_FIELDS][64]){
 char *p=line;
 if(strncmp(p,"elapsed_ms,",11))return false;
 p+=11;
 for(int i=0;i<LOG_FIELDS;i++){
   char *end=(i==LOG_FIELDS-1)?strpbrk(p,"\r\n"):strchr(p,',');
   if(!end||end==p||end-p>=64)return false;
   size_t n=(size_t)(end-p);memcpy(names[i],p,n);names[i][n]=0;
   for(int k=0;k<(int)n;k++){
      char c=names[i][k];
      if(!isalnum((unsigned char)c)&&c!='_'&&c!='-'&&c!=' '&&c!='.')return false;
   }
   p=end+1;
 }
 return true;
}
static bool log_parse_header(char *line){return log_parse_header_named(line,log_channel_names);}
/* Strictly import elapsed_ms and nine optional numeric values, bounded to RAM.
 * Never populate gaps with fabricated sensor data. */
static bool log_parse_record(char *line,LogSample *out){
 memset(out,0,sizeof(*out));
 char *p=line,*end=NULL;
 unsigned long t=strtoul(p,&end,10); if(end==p||*end!=','||t>UINT32_MAX)return false;
 out->t_ms=(u32)t;p=end+1;
 for(int i=0;i<LOG_FIELDS;i++){
   if(*p!=','&&*p!='\n'&&*p!='\r'&&*p){
     float v=strtof(p,&end);
     if(end==p||!isfinite(v)||(*end!=','&&*end!='\r'&&*end!='\n'&&*end!=0))return false;
     out->values[i]=v;out->valid|=(u16)(1u<<i);p=end;
   }
   if(i<LOG_FIELDS-1){if(*p!=',')return false;p++;}
   else if(*p!='\r'&&*p!='\n'&&*p!=0)return false;
 }
 return true;
}
static void calculate_replay_statistics(void){
 for(int ch=0;ch<LOG_FIELDS;ch++){
    replay_stats[ch].count=0;replay_stats[ch].min=INFINITY;
    replay_stats[ch].max=-INFINITY;replay_stats[ch].mean=0;
 }
 for(int row=0;row<replay_count;row++){
    const LogSample *r=&replay_buffer[row];
    for(int ch=0;ch<LOG_FIELDS;ch++){
       if(!(r->valid&(1u<<ch)))continue;
       ReplayStat *st=&replay_stats[ch];double v=r->values[ch];
       if(v<st->min)st->min=v;
       if(v>st->max)st->max=v;
       st->count++;st->mean+=(v-st->mean)/(double)st->count;
    }
 }
}
static bool open_saved_log(int index){
 if(log_recording){note("Stop recording before opening a log");return false;}
 if(index<0||index>=log_file_count||!log_basename_valid(log_filenames[index]))return false;
 char path[105];snprintf(path,sizeof(path),"sdmc:/BrickECU/logs/%s",log_filenames[index]);
 FILE *fp=fopen(path,"rb");if(!fp){note("Could not open the selected log");return false;}
 char line[1024];
 if(!fgets(line,sizeof(line),fp)||!strchr(line,'\n')||!log_parse_header(line)){
    fclose(fp);note("Unsupported CSV header");return false;
 }
 int count=0;u32 previous=0;bool ok=true;
 while(count<LOG_MAX_SAMPLES&&fgets(line,sizeof(line),fp)){
    if(!strchr(line,'\n')&&!feof(fp)){ok=false;break;}
    LogSample row;
    if(!log_parse_record(line,&row)|| (count&&row.t_ms<previous)){ok=false;break;}
    replay_buffer[count++]=row;previous=row.t_ms;
 }
 if(ferror(fp))ok=false;
 if(count==LOG_MAX_SAMPLES&&fgets(line,sizeof(line),fp)){
    /* RAM-limited player: clearly distinguish truncated logs. */
    snprintf(replay_info,sizeof(replay_info),"First %d samples only",LOG_MAX_SAMPLES);
 }else snprintf(replay_info,sizeof(replay_info),"Read-only log replay");
 fclose(fp);
 if(!ok||!count){replay_count=0;note("Invalid or empty CSV log");return false;}
 free(compare_buffer);compare_buffer=NULL;compare_count=0;compare_filename[0]=0;
 replay_count=count;calculate_replay_statistics();replay_position=0;replay_time_ms=replay_buffer[0].t_ms;
 replay_wall_ms=osGetTime();replay_speed=1;replay_playing=false;
 snprintf(replay_filename,sizeof(replay_filename),"%s",log_filenames[index]);
 replay_marks_load();replay_trace_active=false;insight_dirty=true;
 ui_page=PAGE_REPLAY;note("Log loaded; A Play/Pause; D-pad scrub");return true;
}
/* Nearest recorded reference sample by relative time, with a 500ms maximum
 * mismatch. Missing / unordered sample data is never invented or interpolated. */
static int compare_nearest(u32 relative_ms){
 if(!compare_buffer||compare_count<1)return -1;
 u64 target=(u64)compare_buffer[0].t_ms+(u64)relative_ms;
 if(target>UINT32_MAX)return -1;
 int lo=0,hi=compare_count;
 while(lo<hi){int mid=lo+(hi-lo)/2;
    if((u64)compare_buffer[mid].t_ms<target)lo=mid+1;else hi=mid;
 }
 int best=-1;u32 distance=UINT32_MAX;
 for(int k=lo-1;k<=lo;k++){
    if(k<0||k>=compare_count)continue;
    u32 t=compare_buffer[k].t_ms;
    u32 gap=(u64)t>target?(u32)((u64)t-target):(u32)(target-t);
    if(gap<distance){distance=gap;best=k;}
 }
 return distance<=BTS_COMPARE_GAP_MS?best:-1;
}
static void compare_recompute(void){
 for(int ch=0;ch<LOG_FIELDS;ch++)bts_compare_stats_init(&compare_stats[ch]);
 if(!compare_buffer||!replay_count)return;
 for(int i=0;i<replay_count;i++){
    const LogSample *a=&replay_buffer[i];
    int near=compare_nearest(a->t_ms-replay_buffer[0].t_ms);
    const LogSample *b=near>=0?&compare_buffer[near]:NULL;
    for(int ch=0;ch<LOG_FIELDS;ch++){
       int ref=compare_map[ch];
       bool av=(a->valid&(1u<<ch))!=0;
       bool bv=b&&ref>=0&&(b->valid&(1u<<ref));
       bts_compare_stats_add(&compare_stats[ch],av,a->values[ch],bv,
            bv?b->values[ref]:0.0);
    }
 }
}
static void compare_open_picker(void){
 if(replay_count<1||!log_basename_valid(replay_filename)){
    note("Load a saved log before comparing sessions");return;
 }
 refresh_log_files();
 if(log_file_count<2){note("Export at least two logs before comparing");return;}
 replay_playing=false;
 ui_page=PAGE_COMPARE_PICK;selected=scroll_y=0;
 for(int i=0;i<log_file_count;i++)if(strcmp(log_filenames[i],replay_filename)){
    selected=i;break;
 }
 compare_pick_selected=selected;
 note("Select a second log. Same channel names required, no ECU writes.");
}
static bool compare_read_ecu_id(const char *basename,char *out,size_t cap){
 if(!out||cap<2||!log_basename_valid(basename))return false;
 out[0]=0;char path[128];
 int n=snprintf(path,sizeof(path),"sdmc:/BrickECU/logs/%s.session.txt",basename);
 if(n<1||(size_t)n>=sizeof(path))return false;
 FILE *fp=fopen(path,"rb");if(!fp)return false;
 char line[280];const char *prefix="ECU signature at start: ";size_t pre=strlen(prefix);
 bool ok=false;
 while(fgets(line,sizeof(line),fp)){
    if(strncmp(line,prefix,pre))continue;
    char *start=line+pre;size_t sz=strcspn(start,"\r\n");start[sz]=0;
    if(sz>=cap||sz<2||!strcmp(start,"NOT VERIFIED AT START"))break;
    memcpy(out,start,sz+1);ok=true;break;
 }
 fclose(fp);return ok;
}
static bool compare_open_log(int index){
 if(index<0||index>=log_file_count||!log_basename_valid(log_filenames[index]))return false;
 if(!strcmp(replay_filename,log_filenames[index])){
    note("Select a different log for comparison");return false;
 }
 char path[110];snprintf(path,sizeof(path),"sdmc:/BrickECU/logs/%s",log_filenames[index]);
 FILE *fp=fopen(path,"rb");if(!fp){note("Comparison log could not be opened");return false;}
 LogSample *candidate=(LogSample*)malloc(sizeof(LogSample)*LOG_MAX_SAMPLES);
 if(!candidate){fclose(fp);note("Not enough RAM for a second log");return false;}
 char names[LOG_FIELDS][64]={{0}};
 char line[1024];
 bool ok=fgets(line,sizeof(line),fp)&&strchr(line,'\n')&&log_parse_header_named(line,names);
 int count=0;u32 last=0;
 while(ok&&fgets(line,sizeof(line),fp)){
    if(count>=LOG_MAX_SAMPLES||(!strchr(line,'\n')&&!feof(fp))){ok=false;break;}
    LogSample row;
    if(!log_parse_record(line,&row)||(count&&row.t_ms<last)){ok=false;break;}
    candidate[count++]=row;last=row.t_ms;
 }
 if(ferror(fp)||!count)ok=false;
 fclose(fp);
 if(!ok){free(candidate);note("Invalid or oversized reference log; original replay preserved");return false;}
 int matched=0;
 for(int ch=0;ch<LOG_FIELDS;ch++){
    int id=bts_compare_channel_index(log_channel_names[ch],names);
    if(id>=0)matched++;
 }
 if(!matched){free(candidate);note("No matching recorded channel names between logs");return false;}
 free(compare_buffer);compare_buffer=candidate;compare_count=count;
 memcpy(compare_names,names,sizeof(compare_names));
 for(int ch=0;ch<LOG_FIELDS;ch++)compare_map[ch]=bts_compare_channel_index(log_channel_names[ch],names);
 snprintf(compare_filename,sizeof(compare_filename),"%s",log_filenames[index]);
 compare_channel=0;while(compare_channel<LOG_FIELDS&&compare_map[compare_channel]<0)compare_channel++;
 if(compare_channel==LOG_FIELDS)compare_channel=0;
 compare_window=0;
 char a_id[160],b_id[160];
 compare_ecu_identity=compare_read_ecu_id(replay_filename,a_id,sizeof(a_id))&&
   compare_read_ecu_id(compare_filename,b_id,sizeof(b_id))?
   (strcmp(a_id,b_id)==0?1:2):0;
 compare_recompute();ui_page=PAGE_COMPARE;
 note("Two recorded sessions loaded. L/R channels, X time window, START report");
 return true;
}
static void compare_export(void){
 if(!compare_buffer||compare_count<1||!log_basename_valid(replay_filename)||
    !log_basename_valid(compare_filename)){note("Both saved logs required");return;}
 char path[105];FILE *fp=NULL;
 for(int n=1;n<=999;n++){
    snprintf(path,sizeof(path),"sdmc:/BrickECU/logs/compare-%03d.txt",n);
    FILE *old=fopen(path,"rb");if(old){fclose(old);continue;}
    fp=fopen(path,"wb");break;
 }
 if(!fp){note("Could not create unique comparison report on SD");return;}
 fprintf(fp,"Brick Tuner Suite v3.4 / Offline log A-B comparison\n");
 fprintf(fp,"BASE: %s (%d records)\nREFERENCE: %s (%d records)\n",replay_filename,replay_count,compare_filename,compare_count);
 fputs("Timestamps are relative to each log's first sample. Only nearest real observations within 500 ms are paired.\n",fp);
 fprintf(fp,"ECU signature match: %s\n",compare_ecu_identity==1?"YES":compare_ecu_identity==2?"NO - DIFFERENT":"UNKNOWN");
 for(int ch=0;ch<LOG_FIELDS;ch++){
    BtsCompareStats *st=&compare_stats[ch];
    fprintf(fp,"\nCHANNEL %s: ",log_channel_names[ch]);
    if(compare_map[ch]<0){fputs("NOT RECORDED IN REFERENCE\n",fp);continue;}
    fprintf(fp,"%u paired, %u unpaired\n",st->paired,st->skipped);
    if(st->paired)fprintf(fp,"Base mean %.5f; Reference mean %.5f; Delta mean %.5f; Mean absolute delta %.5f; Largest absolute delta %.5f\n",
      st->base_sum/st->paired,st->reference_sum/st->paired,st->delta_sum/st->paired,
      st->abs_delta_sum/st->paired,st->maximum_abs_delta);
 }
 fputs("Informational report only; no ECU calibration was modified.\n",fp);
 bool ok=!ferror(fp);if(fclose(fp)!=0)ok=false;
 if(!ok){remove(path);note("Comparison report write failed; incomplete file removed");return;}
 note("A/B comparison report exported to SD /BrickECU/logs");
}
static void replay_seek(u32 target){
 if(replay_count<=0)return;
 if(target<replay_buffer[0].t_ms)target=replay_buffer[0].t_ms;
 if(target>replay_buffer[replay_count-1].t_ms)target=replay_buffer[replay_count-1].t_ms;
 replay_time_ms=target;
 int lo=0,hi=replay_count-1;
 while(lo<hi){int m=lo+(hi-lo+1)/2;if(replay_buffer[m].t_ms<=target)lo=m;else hi=m-1;}
 replay_position=lo;replay_wall_ms=osGetTime();
}
static void replay_update(u64 now){
 if(ui_page!=PAGE_REPLAY||!replay_playing||replay_count<1)return;
 u64 delta=(now>=replay_wall_ms)?now-replay_wall_ms:0;
 if(delta>1000)delta=1000; /* don't make an unexpected huge jump after sleep */
 u64 next=(u64)replay_time_ms+delta*(u64)replay_speed;
 u32 end=replay_buffer[replay_count-1].t_ms;
 if(next>=end){replay_seek(end);replay_playing=false;return;}
 replay_seek((u32)next);
}
/* Only offer tables whose documented INI axes can be recovered from the
 * nine data channels in this particular saved log. 2D tables require X and Y. */
static int trace_log_channel(const char *name){
 if(!name||!*name)return -1;
 for(int i=0;i<LOG_FIELDS;i++)if(!strcasecmp(name,log_channel_names[i]))return i;
 return -1;
}
static int trace_table_channels(int table,int *outx,int *outy){
 if(table<0||table>=table_count)return 0;
 TableDef*t=&tables[table];int f=field_index(t->z);
 if(f<0||cal[f].width<2||cal[f].height<1)return 0;
 int x=trace_log_channel(t->xchan),y=cal[f].height>1?trace_log_channel(t->ychan):-1;
 if(x<0||(cal[f].height>1&&y<0))return 0;
 if(outx)*outx=x;
 if(outy)*outy=y;
 return 1;
}
static void replay_open_trace(void){
 if(!replay_count||!menu_loaded){note("Log and matching ECU INI are required for map tracing");return;}
 trace_map_count=0;
 for(int t=0;t<table_count&&trace_map_count<TABLE_CAP;t++){
   if(trace_table_channels(t,NULL,NULL))trace_maps[trace_map_count++]=t;
 }
 if(!trace_map_count){
   note("No matching map axes. Record the INI's X/Y channels in gauge slots");return;
 }
 replay_playing=false;ui_page=PAGE_TRACE_MAPS;selected=scroll_y=0;
 note("Select a map: its cursor will follow this recorded log sample");
}
static void open_cal(int id,const char*label);
static void replay_open_selected_map(int choice){
 if(choice<0||choice>=trace_map_count||!ecu_session){
    note("Map tracing needs a connected ECU to read calibration values");return;}
 int idx=trace_maps[choice],x=-1,y=-1;
 if(!trace_table_channels(idx,&x,&y)){note("Map axis channels no longer match this log");return;}
 TableDef*t=&tables[idx];int z=field_index(t->z);
 if(z<0){note("INI map field unavailable");return;}
 trace_log_x=x;trace_log_y=y;
 replay_trace_at=replay_time_ms;replay_trace_active=true;replay_trace_centered=false;
 trace_show_coverage=false;trace_coverage_at=0;trace_valid_samples=trace_coverage_peak=0;
 trace_coverage_done=false;memset(trace_coverage,0,sizeof(trace_coverage));
 field_return_page=PAGE_TRACE_MAPS;
 open_cal(z,*t->label?t->label:t->id);
 note("Trace Mode: log-derived cursor, real ECU calibration; B returns");
}
static void channel_live_extend(void){
 if(channel_cursor>=0&&channel_cursor<selectable_count){
    Field *f=&channels[selectable_channels[channel_cursor]];
    int need=f->offset+dsize(f->type);
    if(need>live_length&&need<=LIVE_CAP)live_length=need;
 }
}
static void extend_live_for_ini_graph(void){
 live_length=base_live_length;
 if(ini_graph_selected<0||ini_graph_selected>=ini_graph_count)return;
 IniGraphDef*g=&ini_graphs[ini_graph_selected];
 for(int i=0;i<g->count;i++){
    Field*f=lookup(g->channels[i],true);
    if(!f||!f->validScale||f->kind!=K_SCALAR)continue;
    int end=f->offset+dsize(f->type);
    if(end>live_length&&end<=LIVE_CAP)live_length=end;
 }
}
static void open_ini_graph(int id,int returnpage,int returnselected){
 if(id<0||id>=ini_graph_count)return;
 if(ui_page!=PAGE_INI_GRAPH)nav_capture();
 ini_graph_selected=id;ini_graph_return=returnpage;ini_graph_return_selected=returnselected;
 ui_page=PAGE_INI_GRAPH;selected=scroll_y=0;
 memset(ini_graph_ok,0,sizeof(ini_graph_ok));
 extend_live_for_ini_graph();
 note("INI live graph: actual ECU channels only");
}
static void cycle_ini_graph(int direction){
 if(ini_graph_selected<0||ini_graph_selected>=ini_graph_count)return;
 const char *owner=ini_graphs[ini_graph_selected].owner;
 int next=ini_graph_selected;
 for(int i=0;i<ini_graph_count;i++){
    next=(next+direction+ini_graph_count)%ini_graph_count;
    if(!strcasecmp(owner,ini_graphs[next].owner)){open_ini_graph(next,ini_graph_return,ini_graph_return_selected);return;}
 }
}
static bool channel_live_value(int idx,double *out){
 if(!out||idx<0||idx>=chan_count||!live_valid)return false;
 Field *f=&channels[idx];
 if(!f->validScale||f->kind!=K_SCALAR||f->offset+dsize(f->type)>live_bytes)return false;
 double raw=get_number(live_data,live_bytes,f->offset,f->type);
 if(!isfinite(raw))return false;
 *out=raw*f->scale+f->translate;
 return isfinite(*out);
}
static void append_gauge_history(void){
 u32 now=(u32)osGetTime();
 /* Report the real network sample rate, not the downsampled chart rate. */
 if(!rate_start_ms)rate_start_ms=now;
 rate_count++;
 if((u32)(now-(u32)rate_start_ms)>=2000){
    actual_hz=(float)(rate_count*1000.0/(now-(u32)rate_start_ms));
    rate_start_ms=now;rate_count=0;
 }
 /* 240 points at 100 Hz covered only 2.4s, leaving 10s/20s graph
  * windows misleadingly empty. Keep ~22s at a fixed 90ms cadence. */
 if(graph_count>0&&(u32)(now-graph_last_sample_ms)<90U)return;
 graph_last_sample_ms=now;
 int at=graph_head;
 graph_ms[at]=now;
 for(int i=0;i<GAUGE_SLOTS;i++){
    graph_values[i][at]=view_values[i];graph_ok[i][at]=view_has[i];
 }
 for(int i=0;i<INI_GRAPH_LINES;i++){
    ini_graph_ok[i][at]=false;
    if(ui_page!=PAGE_INI_GRAPH || ini_graph_selected<0 || ini_graph_selected>=ini_graph_count)continue;
    IniGraphDef *g=&ini_graphs[ini_graph_selected];if(i>=g->count)continue;
    Field*f=lookup(g->channels[i],true);
    if(!f||!f->validScale||f->kind!=K_SCALAR||f->offset+dsize(f->type)>live_bytes)continue;
    double raw=get_number(live_data,live_bytes,f->offset,f->type),v=raw*f->scale+f->translate;
    if(!isfinite(v))continue;
    ini_graph_values[i][at]=(float)v;ini_graph_ok[i][at]=true;
 }
 graph_head=(graph_head+1)%GRAPH_SAMPLES;
 if(graph_count<GRAPH_SAMPLES)graph_count++;
}
/* Per-slot last-good values survive incomplete/short live responses.
 * Explicit channel reassignment and new INI definitions invalidate their cache. */
static void update_view_values(void){
 for(int i=0;i<GAUGE_SLOTS;i++){
    int idx=gauge_fields[i];if(idx<0||idx>=chan_count)continue;
    Field *f=&channels[idx];if(!f->validScale||f->offset<0||f->offset+dsize(f->type)>live_bytes)continue;
    double raw=get_number(live_data,live_bytes,f->offset,f->type);
    double v=raw*f->scale+f->translate;
    if(!isfinite(v))continue;
    gauge_hold_accept(&view_values[i],&view_has[i],v,true);
    gauge_stats_accept(i,(float)v);
 }
}
static void decode_live(void){
 if(last_http!=200){live_errors++;return;}
 int len=decode_http_body();char*b=http_body();if(!b||len<20||memcmp(b,"BLV1",4))return;uint8_t*p=(uint8_t*)b;
 int sample=p[12]|(p[13]<<8);if(sample<4||sample>LIVE_CAP||len!=sample+16)return;
 u32 seq=(u32)p[4]|((u32)p[5]<<8)|((u32)p[6]<<16)|((u32)p[7]<<24);
 if(live_valid&&seq==live_seq){
     /* An old sequence isn't a new sample; don't flicker real readings. */
     return;
 }
 live_seq=seq;memcpy(live_data,p+16,sample);live_bytes=sample;live_valid=true;live_stale=false;live_received_at=osGetTime();successful_live++;
 /* Keep each metric's last valid reading if this fresh packet is shorter or
  * contains a nonfinite channel; never convert missing bytes into zero. */
 for(int i=0;i<9;i++){Field*f=lookup((char*)metric_fields[i],true);if(!f||!f->validScale||f->offset<0||f->offset+dsize(f->type)>live_bytes)continue;
 double raw=get_number(live_data,live_bytes,f->offset,f->type);if(!isfinite(raw))continue;
 double value=raw*f->scale+f->translate;if(!isfinite(value))continue;
 gauge_hold_accept(&displayed[i],&has_value[i],value,true);}
 update_view_values();append_gauge_history();capture_knock_sample();log_sample();
}
static void decode_ini(void){
 if(last_http!=200){note("ESP has no cached ECU INI");ini_retry_at=osGetTime()+10000;return;}
 int size=decode_http_body();char *b=http_body();
 if(!b||!ini_looks_valid(b,size)){note("INI download incomplete - not cached");ini_retry_at=osGetTime()+10000;return;}
 if(parse_ini_all(b,size)<=0){note("INI parser rejected definition");ini_retry_at=osGetTime()+10000;return;}
 configure_channels();
 fault_rebuild_links();
 if(write_cached_ini(b,size)){ini_cached_on_sd=true;note("INI cached on SD - no repeat download");}
 else note("INI loaded (SD cache write failed)");
 ini_refresh=false; if(ui_page==PAGE_CATS && !menu_loaded)ui_page=PAGE_HUB; category=0;
}
static int cat_start(int wanted){int n=0;for(int i=0;i<menu_count;i++)if(menus[i].group==0){if(n==wanted)return i;n++;}return -1;}
static int cat_total(void){int n=0;for(int i=0;i<menu_count;i++)if(menus[i].group==0)n++;return n;}
static int cat_end(int start){int i=start+1;while(i<menu_count&&menus[i].group!=0)i++;return i;}
/* Navigation inspired by TunerStudio's functional menu groups. The *actual*
 * sections and controls come only from the firmware-provided INI, with one
 * catch-all bucket so an unfamiliar ECU page can NEVER disappear. */
#define TUNE_BUCKET_COUNT 11
/* Screenshot-matched TunerStudio/rusEFI primary navigation plus one
 * intentionally visible fallback for unknown firmware-defined menus. */
static const char *tune_bucket_names[TUNE_BUCKET_COUNT]={
 "Setup", "Fuel", "Ignition", "Cranking", "Idle", "Advanced",
 "Sensors", "CAN-bus", "Controller", "View", "Other ECU Pages"
};
static bool tune_match(const char *s,const char *needle){
 if(!s||!needle)return false;
 for(;*s;s++){const char *a=s,*b=needle;
   while(*a&&*b&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){a++;b++;}
   if(!*b)return true;
 }
 return false;
}
static int tune_bucket_of(const char *name){
 if(!name)return 10;
 /* Most rusEFI definition files already carry these exact top-level tabs.
  * Preserve their meaning instead of guessing from the first nested item. */
 char clean[80];size_t j=0;
 for(const char *p=name;*p&&j<sizeof(clean)-1;p++)
    if(*p!='&')clean[j++]=*p;
 clean[j]=0;
 if(!strcasecmp(clean,"Setup")||!strcasecmp(clean,"Basic")||!strcasecmp(clean,"Engine"))return 0;
 if(!strcasecmp(clean,"Fuel"))return 1;
 if(!strcasecmp(clean,"Ignition"))return 2;
 if(!strcasecmp(clean,"Cranking"))return 3;
 if(!strcasecmp(clean,"Idle"))return 4;
 if(!strcasecmp(clean,"Advanced"))return 5;
 if(!strcasecmp(clean,"Sensors"))return 6;
 if(!strcasecmp(clean,"CAN-bus")||!strcasecmp(clean,"CAN bus"))return 7;
 if(!strcasecmp(clean,"Controller"))return 8;
 if(!strcasecmp(clean,"View"))return 9;
 /* Fall back to heuristics only for different firmware-defined headings. */
 if(tune_match(clean,"crank")||tune_match(clean,"prime")||tune_match(clean,"startup"))return 3;
 if(tune_match(clean,"idle"))return 4;
 if(tune_match(clean,"fuel")||tune_match(clean,"inject")||tune_match(clean,"afr")||tune_match(clean,"lambda")||tune_match(clean,"ve table"))return 1;
 if(tune_match(clean,"ignition")||tune_match(clean,"spark")||tune_match(clean,"dwell")||tune_match(clean,"timing"))return 2;
 if(tune_match(clean,"sensor")||tune_match(clean,"analog")||tune_match(clean,"temperature"))return 6;
 if(tune_match(clean,"can ")||tune_match(clean,"can bus")||tune_match(clean,"network"))return 7;
 if(tune_match(clean,"diagnos")||tune_match(clean,"console")||tune_match(clean,"test")||tune_match(clean,"sd card"))return 8;
 if(tune_match(clean,"gauge")||tune_match(clean,"view")||tune_match(clean,"monitor")||tune_match(clean,"map")||tune_match(clean,"table")||tune_match(clean,"curve"))return 9;
 if(tune_match(clean,"boost")||tune_match(clean,"vvt")||tune_match(clean,"turbo")||tune_match(clean,"output")||tune_match(clean,"fan")||tune_match(clean,"advanced")||tune_match(clean,"control"))return 5;
 if(tune_match(clean,"engine")||tune_match(clean,"trigger")||tune_match(clean,"basic")||tune_match(clean,"setup")||tune_match(clean,"load"))return 0;
 return 10;
}
static int tune_section_count(int bucket){
 int n=0;
 for(int i=0;i<cat_total();i++){int start=cat_start(i);
    if(start>=0&&tune_bucket_of(menus[start].label)==bucket)n++;
 }
 return n;
}
static int tune_section_at(int bucket,int index){

 for(int i=0;i<cat_total();i++){int start=cat_start(i);
    if(start>=0&&tune_bucket_of(menus[start].label)==bucket&&index--==0)return i;
 }
 return -1;
}

/* A selected top-level TunerStudio tab previews its REAL INI menu items on
 * the bottom screen. Group children appear only when entering that folder.
 * Maps exist here ONLY if firmware placed them in the menu (no invented map hub).
 */
static int tune_preview_index(int bucket,int group,int nth){
 if(nth<0)return -1;
 if(group>=0){
   if(group>=menu_count||menus[group].group!=1)return -1;
   for(int i=group+1;i<menu_count&&menus[i].group==2;i++)
      if(nth--==0)return i;
   return -1;
 }
 int active_bucket=-1;
 for(int i=0;i<menu_count;i++){
   if(menus[i].group==0){active_bucket=tune_bucket_of(menus[i].label);continue;}
   if(active_bucket!=bucket || menus[i].group==2)continue;
   if(nth--==0)return i;
 }
 return -1;
}
static int tune_preview_total(int bucket,int group){
 int n=0;
 if(group>=0){
   if(group>=menu_count||menus[group].group!=1)return 0;
   for(int i=group+1;i<menu_count&&menus[i].group==2;i++)n++;
 }else{
   int active_bucket=-1;
   for(int i=0;i<menu_count;i++){
     if(menus[i].group==0){active_bucket=tune_bucket_of(menus[i].label);continue;}
     if(active_bucket==bucket && menus[i].group!=2)n++;
   }
 }
 return n;
}
static int tune_visible_buckets(void){
 /* Keep the ten screenshot tabs, and show the real fallback only when needed. */
 return tune_preview_total(10,-1)>0?11:10;
}
static void tune_reset_preview(void){
 tune_lower_focus=false;tune_lower_selected=0;tune_lower_scroll=0;
 tune_open_group=-1;tune_from_tree=false;
}
/* rusEFI uses groupMenu/groupChildMenu to organize nested settings. */
static int count_category_items(void){
 int begin=cat_start(category);if(begin<0)return 0;int total=0;
 for(int i=begin+1;i<cat_end(begin);i++)if(menus[i].group!=2)total++;
 return total;
}
/* Preserve the TunerStudio INI declaration order exactly; the old two-pass
 * folder-first sorter made Fuel/Ignition/Cranking differ from the PC menus. */
static int menu_index(int n){
 int begin=cat_start(category);if(begin<0||n<0)return -1;
 for(int i=begin+1;i<cat_end(begin);i++){
    if(menus[i].group==2)continue;
    if(n--==0)return i;
 }
 return -1;
}
static int group_child_index(int n){
 if(group_item<0||group_item>=menu_count)return -1;
 for(int i=group_item+1;i<menu_count&&menus[i].group==2;i++)if(n--==0)return i;
 return -1;
}
static int group_child_count(void){
 if(group_item<0||group_item>=menu_count)return 0;
 int total=0;
 for(int i=group_item+1;i<menu_count&&menus[i].group==2;i++)total++;
 return total;
}
static void save_favorites(void){
  mkdir("sdmc:/BrickECU",0777);
  FILE *fp=fopen(FAVORITES_CFG,"wb");
  if(!fp){note("SD card unavailable: favourites not saved");return;}
  for(int i=0;i<favorite_count;i++){
    /* Never use delimiter bytes from arbitrary INI labels as file controls. */
    if(strchr(favorites[i].target,'|')||strchr(favorites[i].label,'|'))continue;
    fprintf(fp,"%s|%s\n",favorites[i].target,favorites[i].label);
  }
  fclose(fp);
}
static void load_favorites(void){
  FILE *fp=fopen(FAVORITES_CFG,"rb");if(!fp)return;
  char buf[220];favorite_count=0;
  while(favorite_count<FAVORITES_CAP&&fgets(buf,sizeof(buf),fp)){
    char *sep=strchr(buf,'|');if(!sep)continue;*sep++=0;
    char *end=strpbrk(sep,"\r\n");if(end)*end=0;
    if(!*buf||!*sep||strlen(buf)>=sizeof(favorites[0].target)||strlen(sep)>=sizeof(favorites[0].label))continue;
    Favorite *f=&favorites[favorite_count++];
    snprintf(f->target,sizeof(f->target),"%s",buf);snprintf(f->label,sizeof(f->label),"%s",sep);
  }
  fclose(fp);
}
static bool favorite_has(const char *target){
 if(!target||!*target)return false;
 for(int i=0;i<favorite_count;i++)if(!strcasecmp(favorites[i].target,target))return true;
 return false;
}
static void favorite_toggle(const char *target,const char *label){
 if(!target||!*target||!label||!*label){note("This item cannot be bookmarked");return;}
 for(int i=0;i<favorite_count;i++)if(!strcasecmp(favorites[i].target,target)){
   for(int j=i+1;j<favorite_count;j++)favorites[j-1]=favorites[j];
   favorite_count--;save_favorites();note("Removed from Quick Access");return;
 }
 if(favorite_count>=FAVORITES_CAP){note("Quick Access is full");return;}
 if(strchr(target,'|')||strchr(label,'|')){note("Bookmark contains unsupported delimiter");return;}
 Favorite *f=&favorites[favorite_count++];
 snprintf(f->target,sizeof(f->target),"%s",target);snprintf(f->label,sizeof(f->label),"%s",label);
 save_favorites();note("Saved to Quick Access");
}
static void toggle_selected_favorite(void){
 const char *target=NULL,*label=NULL;
 if(ui_page==PAGE_DETAIL&&selected>=0&&selected<result_count){
    const Resolved *r=&results[selected];
    if((r->type==0||r->type==1||r->type==5)&&r->field>=0&&r->field<cal_count)
       favorite_toggle(cal[r->field].name,r->label);
    else note("Only calibration settings can be starred");
    return;
 }
 if(ui_page==PAGE_MENU){int k=menu_index(selected);if(k>=0&&menus[k].group!=1){target=menus[k].target;label=menus[k].label;}}
 else if(ui_page==PAGE_GROUP){int k=group_child_index(selected);if(k>=0){target=menus[k].target;label=menus[k].label;}}
 else if(ui_page==PAGE_MAPS&&selected>=0&&selected<table_count){target=tables[selected].id;label=*tables[selected].label?tables[selected].label:tables[selected].id;}
 else if(ui_page==PAGE_FAVORITES&&selected>=0&&selected<favorite_count){
  target=favorites[selected].target;label=favorites[selected].label;
  /* Removal mutates the array, so copy the selected record first. */
  char t[64],l[68];snprintf(t,sizeof(t),"%s",target);snprintf(l,sizeof(l),"%s",label);
  favorite_toggle(t,l);if(selected>=favorite_count&&selected>0)selected--;return;
 }
 favorite_toggle(target,label);
}

/* INI navigation search: only existing menu targets and real table definitions. */
static bool text_matches(const char *haystack,const char *needle){
 if(!needle||!*needle)return true;
 for(const char *p=haystack;p&&*p;p++){
   const char *h=p,*n=needle;
   while(*h&&*n&&tolower((unsigned char)*h)==tolower((unsigned char)*n)){h++;n++;}
   if(!*n)return true;
 }
 return false;
}
static void rebuild_search(void){
 search_count=0;
 if(!*search_query)return;
 for(int i=0;i<menu_count&&search_count<SEARCH_CAP;i++){
  if(menus[i].group==0||menus[i].group==1||!menus[i].target[0])continue;
  if(text_matches(menus[i].label,search_query)||text_matches(menus[i].target,search_query))
   search_hits[search_count++]=(SearchHit){0,i};
 }
 for(int i=0;i<table_count&&search_count<SEARCH_CAP;i++){
  const TableDef *t=&tables[i];
  if(text_matches(t->label,search_query)||text_matches(t->id,search_query)||text_matches(t->z,search_query))
   search_hits[search_count++]=(SearchHit){1,i};
 }
}
/* The stock 3DS software keyboard replaces endless menu scrolling with
 * an on-device search; it neither reads nor changes ECU calibration. */
static void search_prompt(void){
 nav_capture();
 SwkbdState keyboard;
 char buffer[sizeof(search_query)];
 snprintf(buffer,sizeof(buffer),"%s",search_query);
 swkbdInit(&keyboard,SWKBD_TYPE_NORMAL,2,42);
 swkbdSetHintText(&keyboard,"Find a map or ECU setting");
 SwkbdButton result=swkbdInputText(&keyboard,buffer,sizeof(buffer));
 if(result!=SWKBD_BUTTON_RIGHT)return;
 snprintf(search_query,sizeof(search_query),"%s",buffer);
 rebuild_search();ui_page=PAGE_SEARCH;selected=scroll_y=0;search_origin=false;
 note(search_count?"Select a real ECU item":"No INI matches; press Y to search again");
}
static bool has_field_result(int id){for(int i=0;i<result_count;i++)if(results[i].field==id)return true;return false;}
static void add_result(const char*label,int fid,int typ){if(fid<0||fid>=cal_count||result_count>=RES_CAP||has_field_result(fid))return;Resolved*r=&results[result_count++];memset(r,0,sizeof(*r));snprintf(r->label,sizeof(r->label),"%s",(label&&*label)?label:cal[fid].name);r->field=fid;r->type=typ;}
/* Read-only text and gauge controls from the ECU's actual INI. */
static void add_info_result(const char *raw){
 if(!raw||!*raw||result_count>=RES_CAP)return;
 while(*raw=='#'||*raw=='!')raw++;
 while(isspace((unsigned char)*raw))raw++;
 if(!*raw)return;
 Resolved *r=&results[result_count++];memset(r,0,sizeof(*r));
 copy_text(r->label,sizeof(r->label),raw);
 copy_text(r->info,sizeof(r->info),raw);
 r->field=-1;r->type=6;
}
static void add_ini_gauge_result(const char *name){
 if(!name||!*name||result_count>=RES_CAP)return;
 Resolved *r=&results[result_count];memset(r,0,sizeof(*r));
 for(int i=0;i<gauge_def_count;i++)if(!strcasecmp(name,gauges[i].name)){
   const GaugeDef *g=&gauges[i];
   copy_text(r->label,sizeof(r->label),*g->label?g->label:g->name);
   copy_text(r->command,sizeof(r->command),g->channel);
   Field *ch=lookup((char*)g->channel,true);
   r->field=ch?(int)(ch-channels):-1;
   r->type=7;result_count++;return;
 }
 copy_text(r->label,sizeof(r->label),name);
 r->field=-1;r->type=7;result_count++;
}
static void add_ini_indicator_result(const char *ref){
 if(!ref||result_count>=RES_CAP)return;
 int idx=-1;char extra='x';
 if(sscanf(ref,"btsIndicator%d%c",&idx,&extra)!=1||
    idx<0||idx>=ini_indicator_count)return;
 const BtsIniIndicator *ind=&ini_indicators[idx];
 Resolved *r=&results[result_count++];memset(r,0,sizeof(*r));
 copy_text(r->label,sizeof(r->label),*ind->on?ind->on:ind->off);
 r->field=idx;r->type=8; /* Live read-only indicator, never a command. */
}
static void add_action_result(const char*label,const char*command){
 if(result_count>=RES_CAP)return;
 Resolved*r=&results[result_count++];
 memset(r,0,sizeof(*r));
 snprintf(r->label,sizeof(r->label),"%s",label);
 snprintf(r->command,sizeof(r->command),"%s",command);
 r->field=-1;r->type=3; /* Per-command dispatch gates apply in enter_selected(). */
}
static void warn_before_hazardous_test(unsigned kind){
 brick_test_confirm_begin(&hazardous_test,kind);
 /* If shoulder buttons are already released, next L+R can confirm. */
 hazardous_test.released=(hidKeysHeld()&(KEY_L|KEY_R))==0;
 note("Hazardous-test warning: L+R acknowledge, B cancel");
}
/* "Abort test" is a safety stop, unlike test-start command buttons.
 * It transmits a fixed named cancel operation through the Bridge. */
static void request_output_test_abort(void){
 if(test_abort_pending){note("ECU test abort already queued");return;}
 if(!net_ok||!ecu_session||!ecu_signature[0]||!wifi_candidate){
    note("ECU not connected; cannot confirm test abort");return;
 }
 if(demo_confirm.phase)bts_demo_reset(&demo_confirm);
 test_abort_pending=true;
 note("Sending real ECU test abort; awaiting acknowledgement...");
}
/* Only mechanical throttle and pedal sensor grabs; ETB auto-calibration,
 * injector, ignition, and physical I/O bench tests use no execution path here. */
static bool is_sensor_capture_command(const char *name){
 if(!name)return false;
 return !strcmp(name,"cmd_calibrate_tps_closed")||
        !strcmp(name,"cmd_calibrate_tps_open")||
        !strcmp(name,"cmd_calibrate_pedal_up")||
        !strcmp(name,"cmd_calibrate_pedal_down");
}
static void begin_output_demo(const Resolved *r){
 if(!r||r->type!=3)return;
 if(demo_confirm.phase||hazardous_test.kind){
    note("Finish or cancel the current confirmation first");return;
 }
 if(is_sensor_capture_command(r->command)){
    if(!net_ok||!ecu_session||!ecu_signature[0]||!menu_loaded||
       strcmp(ini_signature,ecu_signature)){
       note("Connect ECU with matching INI before capturing sensor values");
       return;
    }
    if(sensor_capture_pending||sensor_capture_inflight){
       note("Previous sensor capture still in progress");return;
    }
    sensor_capture_confirmation=true;
 }else sensor_capture_confirmation=false;
 copy_text(demo_command,sizeof(demo_command),r->command);
 copy_text(demo_label,sizeof(demo_label),r->label);
 bts_demo_begin(&demo_confirm);
 demo_started_ms=osGetTime();
 note(sensor_capture_confirmation?
      "SENSOR CAPTURE: Press A, release L+R, then hold both to confirm":
      "DEMO: Press A to enable, then release and hold L+R; B cancels");
}
static bool is_stim_action(const Resolved *r){
 return r&&r->type==3&&
  (!strcmp(r->command,"cmd_enable_self_stim")||
   !strcmp(r->command,"cmd_disable_self_stim"));
}
static void open_offline_trigger(bool running){
 stim_return_page=ui_page;
 stim_preview=running;
 stim_anchor_time=osGetTime();stim_anchor_angle=0;
 extend_live_for_trigger();
 stim_preview_presses++;
 ui_page=PAGE_TRIGGER_SIM;selected=scroll_y=0;
 note("Offline wheel viewer: ECU pins and settings unchanged");
}
static void preview_stim_action(const Resolved *r){
 if(!is_stim_action(r)){note("Action unsupported by Brick Tuner Suite");return;}
 /* BrickOS independently matches the live ECU signature to its own INI
  * channel profile. Do not block its preflight merely because the 3DS has
  * not downloaded its UI menu yet: that was a redundant, misleading gate. */
 if(!net_ok||!ecu_session||!ecu_signature[0]){
   note("No live ECU USB session; trigger Start not requested");return;
 }
 if(stim_pending!=STIM_NONE || hazardous_test.kind){note("Finish current test request first");return;}
 if(!strcmp(r->command,"cmd_enable_self_stim")){
   if(!stim_start_supported){note("BrickOS v4.7 needed for real ECU Start");return;}
   stim_pending=STIM_PREPARE;
   note("Checking ECU RPM, voltage and bench authorization...");
 }else if(stim_stop_supported){
   stim_pending=STIM_STOP;
   note("Sending real ECU internal trigger Stop...");
 }else note("BrickOS trigger Stop endpoint unavailable");
}
/* BrickOS validates the exact ECU/INI signature itself, but the DS may
 * already have the matching INI on its SD card while the ESP32 cache is
 * absent. Upload that local copy automatically ONCE if BrickOS explicitly
 * reports a missing matching channel profile. This transmits INI text only,
 * never a trigger command, and a subsequent independent Bridge preflight is
 * still required to authorize any ECU action. No manual INI transfer step. */
static bool stim_send_all(int fd,const char *src,size_t len){
 size_t at=0;
 while(at<len){
   if(readable(fd,1,5000)<=0)return false;
   int n=send(fd,src+at,len-at,0);
   if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))continue;
   if(n<=0)return false;
   at+=(size_t)n;
 }
 return true;
}
static bool stim_sync_local_ini(void){
 if(!menu_loaded||!ecu_signature[0]||!ini_signature[0]||strcmp(ecu_signature,ini_signature)){
   note("ESP32 needs the matching INI; DS has no matching local copy");return false;
 }
 FILE *file=fopen(INI_CACHE,"rb");
 if(!file){note("ESP32 needs INI; DS SD cache missing");return false;}
 if(fseek(file,0,SEEK_END)!=0){fclose(file);return false;}
 long len=ftell(file);
 if(len<5000||len>1200000||fseek(file,0,SEEK_SET)!=0){fclose(file);note("DS cached INI size is invalid");return false;}
 int fd=connect_socket();
 if(fd<0){fclose(file);note("Unable to connect for automatic INI transfer");return false;}
 char head[250];
 int hn=snprintf(head,sizeof(head),
    "POST /api/ini/upload HTTP/1.1\r\nHost: %s\r\n"
    "Content-Type: text/plain\r\nContent-Length: %ld\r\n"
    "Connection: close\r\n\r\n",HOST_IP,len);
 bool ok=hn>0&&hn<(int)sizeof(head)&&stim_send_all(fd,head,(size_t)hn);
 char chunk[2048];long left=len;
 while(ok&&left>0){
   size_t wanted=(size_t)(left>(long)sizeof(chunk)?(long)sizeof(chunk):left);
   size_t got=fread(chunk,1,wanted,file);
   if(got!=wanted||!stim_send_all(fd,chunk,got)){ok=false;break;}
   left-=(long)got;
 }
 fclose(file);
 if(!ok){close(fd);note("Automatic INI transfer interrupted");return false;}
 int used=0;bool complete=false;
 while(used<RESPONSE_CAP-1){
   int wait=readable(fd,0,used?5000:30000);
   if(wait<=0)break;
   int n=recv(fd,response+used,RESPONSE_CAP-1-used,0);
   if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK))continue;
   if(n<=0)break;
   used+=n;response[used]=0;
   if(http_entity_complete(response,used)){complete=true;break;}
 }
 close(fd);response[used]=0;bytes_rx=used;last_http=0;
 if(complete)sscanf(response,"HTTP/%*s %d",&last_http);
 if(!complete||last_http!=200||!http_body()||!strstr(http_body(),"\"ok\":true")){
   note("ESP32 rejected INI sync; check Bridge storage/USB session");return false;
 }
 note("Matching DS INI copied to ESP32; repeating ECU preflight");
 return true;
}
/* Only the network worker sends ECU commands. L+R requests a single-use
 * start token, never a raw TS X14 command from the UI thread. */
static int stim_http_action(int type){
 char sig[320],payload[700];
 if(!ecu_signature[0]||!escape_json_string(ecu_signature,sig,sizeof(sig)))return -1;
 if(type==TASK_STIM_PREPARE){
   snprintf(payload,sizeof(payload),"{\"signature\":\"%s\"}",sig);
   if(post_api("/api/stim/preflight",payload,14000)<0){
     /* A missing server INI is NOT a reason for a new manual setup screen:
      * first try the verified copy already on the 3DS SD card. */
     bool profile_missing=
        strstr(ui_note,"No matching INI or built-in firmware channel profile")!=NULL ||
        strstr(ui_note,"Preflight blocked: require verified INI")!=NULL;
     if(!profile_missing||!stim_sync_local_ini()||
        post_api("/api/stim/preflight",payload,14000)<0)return -1;
   }
   char *body=http_body();
   if(!body || !json_string(body,"token",stim_token,sizeof(stim_token)) || !stim_token[0])return -1;
   json_number(body,"rpm",&stim_last_rpm);
   json_number(body,"vbat",&stim_last_vbat);
   return 1;
 }
 if(type==TASK_STIM_START){
   if(!stim_token[0])return -1;
   snprintf(payload,sizeof(payload),"{\"signature\":\"%s\",\"token\":\"%s\",\"confirm\":\"BENCH_NO_12V_LR_CONFIRMED\"}",sig,stim_token);
   stim_token[0]=0; /* One attempt, no blind retry of a possibly accepted command. */
   if(post_api("/api/stim/start",payload,14000)<0)return -1;
 }else if(type==TASK_STIM_STOP){
   if(post_api("/api/stim/stop","{}",14000)<0)return -1;
 }else return -1;
 bool acknowledged=false;
 return json_bool(http_body(),"command_acknowledged",&acknowledged)&&acknowledged?1:-1;
}
static double stim_phase(u64 now){
 return dream_wrap_angle(stim_anchor_angle+
      (stim_preview?dream_angle_at_time((double)(now-stim_anchor_time),stim_rpm):0.0));
}
static void stim_reanchor(void){
 u64 now=osGetTime();stim_anchor_angle=stim_phase(now);stim_anchor_time=now;
}
/* Never invent settings or offsets. Every shortcut must resolve to a real
 * writable/readable Field parsed from the currently loaded matching INI.
 * Firmware reports ONE priority fault string, not a structured error list.
 * We can offer TWO+ choices only when its text identifies those features. */
static bool fault_contains(const char *text,const char *needle){
 if(!text||!needle||!*needle)return false;
 size_t n=strlen(needle);
 for(const char *p=text;*p;p++)if(!strncasecmp(p,needle,n))return true;
 return false;
}
static void fault_add_field(const char *id,const char *label){
 if(!id||!menu_loaded||strcmp(ecu_signature,ini_signature))return;
 int idx=field_index(id);
 if(idx<0||idx>=cal_count)return;
 for(int i=0;i<fault_link_count;i++)if(fault_links[i].field==idx)return;
 if(fault_link_count>=BRICK_FAULT_LINK_CAP)return;
 BrickFaultLink *l=&fault_links[fault_link_count++];
 l->field=idx;l->menu=-1;
 snprintf(l->label,sizeof(l->label),"%s",label&&*label?label:id);
}
/* A fallback link must point to a real ECU INI menu target. If an ECU only
 * reports a human-readable error, this allows direct navigation to the
 * relevant TunerStudio dialog instead of opening an empty error screen. */
static void fault_add_menu(int idx){
 if(idx<0||idx>=menu_count||!menus[idx].target[0]||menus[idx].group==0)return;
 if(!menu_loaded||strcmp(ecu_signature,ini_signature))return;
 for(int i=0;i<fault_link_count;i++)if(fault_links[i].menu==idx)return;
 if(fault_link_count>=BRICK_FAULT_LINK_CAP)return;
 BrickFaultLink *l=&fault_links[fault_link_count++];
 l->field=-1;l->menu=idx;
 copy_text(l->label,sizeof(l->label),menus[idx].label);
}
static void fault_add_menu_hint(const char *hint){
 if(!hint||!*hint)return;
 for(int i=0;i<menu_count;i++){
    if(menus[i].group==0||!menus[i].target[0])continue;
    if(fault_contains(menus[i].label,hint)){fault_add_menu(i);return;}
 }
}
static void fault_rebuild_links(void){
 fault_link_count=0;
 if(!fault_active||!menu_loaded||strcmp(ecu_signature,ini_signature))return;
 /* Exact INI identifiers in the ECU message take precedence. */
 for(int i=0;i<cal_count&&fault_link_count<BRICK_FAULT_LINK_CAP;i++){
   const char *id=cal[i].name;
   if(strlen(id)>=6 && fault_contains(fault_message,id))fault_add_field(id,id);
 }
 static const struct {const char *hint,*id,*label;} rules[]={
  {"primary trigger","triggerInputPins1","Trigger input 1"},
  {"trigger input","triggerInputPins1","Trigger input 1"},
  {"crank input","triggerInputPins1","Crank input"},
  {"crank sensor","triggerInputPins1","Crank input"},
  {"secondary trigger","triggerInputPins2","Trigger input 2"},
  {"cam sensor","camInputs1","Cam input"},
  {"cam input","camInputs1","Cam input"},
  {"flex fuel","flexSensorPin","Flex fuel input"},
  {"flex sensor","flexSensorPin","Flex fuel input"},
  {"flex input","flexSensorPin","Flex fuel input"},
  {"vehicle speed","vehicleSpeedSensorInputPin","Vehicle speed input"},
  {"vss input","vehicleSpeedSensorInputPin","Vehicle speed input"},
  {"map sensor","map.sensor.hwChannel","MAP input"},
  {"boost control","boostControlPin","Boost control output"},
  {"trigger emulator","triggerSimulatorPins1","Trigger simulator output"}
 };
 for(size_t i=0;i<sizeof(rules)/sizeof(rules[0]);i++)
   if(fault_contains(fault_message,rules[i].hint))fault_add_field(rules[i].id,rules[i].label);
 if(fault_link_count)return; /* exact field ID links take precedence */
 /* Full words from the actual INI menus, not invented calibration fields. */
 for(int i=0;i<menu_count&&fault_link_count<BRICK_FAULT_LINK_CAP;i++){
    const char *label=menus[i].label;
    if(menus[i].group==0||!menus[i].target[0]||strlen(label)<6)continue;
    if(fault_contains(fault_message,label))fault_add_menu(i);
 }
 if(fault_link_count)return;
 static const struct {const char *hint,*menu;} menu_rules[]={
   {"trigger","Trigger"},{"crank","Cranking"},
   {"ignition","Ignition"},{"spark","Ignition"},
   {"injector","Injector"},{"fuel pump","Fuel Pump"},
   {"cooling fan","Cooling Fan"},{"fan output","Cooling Fan"},
   {"map sensor","MAP sensor"},{"throttle","Throttle"},
   {"boost","Boost"},{"can bus","CAN Bus"},
   {"vvt","VVT"},{"vvl","VVL"},
   {"idle","Idle"},{"analog input","Analog input"},
   {"output pin","Outputs"},{"pin conflict","Outputs"}
 };
 for(size_t i=0;i<sizeof(menu_rules)/sizeof(menu_rules[0]);i++){
    if(fault_contains(fault_message,menu_rules[i].hint)){
       fault_add_menu_hint(menu_rules[i].menu);
       if(fault_link_count)break;
    }
 }
}
/* Call only after successful CRC/HTTP response: zero length is confirmed
 * clear; a dropped packet is NEVER interpreted as a resolved problem. */
/* The ECU's e response must be a genuine text message. Never promote raw
 * binary/protocol garbage (for example ?1|$) into a configuration fault. */
static bool fault_reports_clear(const char *message);
static bool fault_payload_valid(const char *p){
 if(!p)return false;
 const char *original=p;
 int letters=0,word=0,len=0;
 for(;*p;p++){
    unsigned char c=(unsigned char)*p;
    if((c<32&&c!='\n'&&c!='\r'&&c!='\t')||c>126)return false;
    if(isalpha(c)){word++;if(word>=3)letters=1;}
    else word=0;
    if(++len>=BRICK_FAULT_MAX-1)return false;
 }
 return !len||letters||fault_reports_clear(original);
}
static bool fault_reports_clear(const char *message){
 if(!message)return true;
 while(isspace((unsigned char)*message))message++;
 if(!*message)return true;
 char clean[BRICK_FAULT_MAX];size_t n=strlen(message);
 while(n>0&&isspace((unsigned char)message[n-1]))n--;
 if(n>=sizeof(clean))n=sizeof(clean)-1;
 memcpy(clean,message,n);clean[n]=0;
 message=clean;
 static const char *const clear_messages[]={"0","OK","NO ERROR","NO ERRORS",
    "NONE","NO CONFIG ERROR","NO CONFIG ERRORS","NO CONFIGURATION ERROR",
    "NO CONFIGURATION ERRORS","CONFIGURATION OK","NULL"};
 for(size_t i=0;i<sizeof(clear_messages)/sizeof(clear_messages[0]);i++){
    if(!strcasecmp(message,clear_messages[i]))return true;
 }
 /* Some rusEFI builds add a trailing period to their clean responses. */
 while(n>0 && (clean[n-1]=='.'||clean[n-1]=='!'||clean[n-1]==':'))clean[--n]=0;
 for(size_t i=0;i<sizeof(clear_messages)/sizeof(clear_messages[0]);i++){
    if(!strcasecmp(clean,clear_messages[i]))return true;
 }
 return false;
}
static void fault_accept_message(const char *message){
 if(!fault_payload_valid(message))return; /* corrupt packets cannot create faults */
 if(fault_reports_clear(message)){
   fault_candidate[0]=0;fault_candidate_count=0;
   if(fault_active && (fault_recheck_after_reconnect || ++fault_clear_count>=2)){
     fault_active=false;fault_message[0]=0;fault_link_count=0;fault_clear_count=0;
     note("ECU rechecked: configuration error cleared");
     if(ui_page==PAGE_FAULT){ui_page=fault_return_page;selected=scroll_y=0;nav_restore();}
     if(ui_page==PAGE_FIELD&&field_return_page==PAGE_FAULT)
        field_return_page=fault_return_page;
   }
   fault_recheck_after_reconnect=false;
   return;
 }
 fault_clear_count=0;
 /* Two successive IDENTICAL valid ECU responses are required. One isolated
  * corrupt/stale payload never interrupts tuning with a false alarm. */
 if(strcmp(message,fault_candidate)){
    copy_text(fault_candidate,sizeof(fault_candidate),message);
    fault_candidate_count=1;
    return;
 }
 if(fault_candidate_count<3)fault_candidate_count++;
 if(fault_candidate_count<2)return;
 fault_recheck_after_reconnect=false;
 if(!fault_active||strcmp(message,fault_message)){
   copy_text(fault_message,sizeof(fault_message),message);
   fault_active=true;fault_rebuild_links();
   bts_mascot_say();
   bts_audio_play(BTS_SOUND_CONFIG_ERROR,event_audio_enabled);
   note("Confirmed ECU ERROR: SELECT+Y for related settings");
 }
}
static bool changed(void);
static void open_cal(int id,const char *label);
static void fault_open_page(void){
 if(!fault_active||!fault_payload_valid(fault_message)){note("No confirmed ECU fault");return;}
 if(ui_page==PAGE_FAULT)return;
 if(ui_page==PAGE_FIELD&&changed()){
   note("Finish or discard current edits before opening ECU fault");return;
 }
 nav_capture();fault_return_page=ui_page;ui_page=PAGE_FAULT;
 bts_mascot_say();
 selected=scroll_y=0;
 fault_rebuild_links();
 /* One exact field has an unambiguous repair destination: open it now. */
 if(fault_link_count==1&&fault_links[0].field>=0){
    field_return_page=PAGE_FAULT;
    open_cal(fault_links[0].field,fault_links[0].label);
    note("Opened configuration field named by ECU error");
 }
}
static int find_table(const char*id){for(int i=0;i<table_count;i++)if(!strcasecmp(tables[i].id,id)||!strcasecmp(tables[i].alt,id))return i;return -1;}
static void resolve_item(const char*id,int depth){if(depth>8||!id||!*id||result_count>=RES_CAP)return;
 int first=result_count;
 int tid=find_table(id);if(tid>=0){TableDef*t=&tables[tid];int z=field_index(t->z),x=field_index(t->x),y=field_index(t->y);
 /* TunerStudio's table id is its 2D entry and its alternate id is the
  * 3D entry. Both reference the SAME INI Z field and ECU bytes. */
 bool is_3d=!t->curve&&*t->alt&&!strcasecmp(id,t->alt)&&strcasecmp(t->id,t->alt);
 if(z>=0)add_result(t->label,z,is_3d?5:1);if(x>=0)add_result("X axis",x,0);if(y>=0)add_result("Y axis",y,0);return;}
 int f=field_index(id);if(f>=0){add_result(NULL,f,0);return;}
 for(int i=0;i<ref_count;i++){if(strcasecmp(refs[i].owner,id))continue;
 int before=result_count;
 if(refs[i].panel==1)resolve_item(refs[i].ref,depth+1);
 else if(refs[i].panel==2)add_action_result(refs[i].label,refs[i].ref);
 else if(refs[i].panel==4)add_info_result(refs[i].label);
 else if(refs[i].panel==5)add_ini_gauge_result(refs[i].ref);
 else if(refs[i].panel==6)add_ini_indicator_result(refs[i].ref);
 else if(refs[i].panel==3){
   for(int g=0;g<ini_graph_count;g++)if(!strcasecmp(refs[i].ref,ini_graphs[g].id)){      if(result_count<RES_CAP){
         Resolved *r=&results[result_count++];memset(r,0,sizeof(*r));r->field=g;r->type=4;
         char pretty[58];snprintf(pretty,sizeof(pretty),"%s",ini_graphs[g].count?ini_graphs[g].channels[0]:"Live Data");
         snprintf(r->label,sizeof(r->label),"Graph %d: %.43s",g+1,pretty);
      }break;
   }
 }
 else {f=field_index(refs[i].ref);if(f>=0)add_result(refs[i].label,f,0);}
 /* The explicit INI placement is inherited by the entire child panel,
  * but never overrides a deeper explicit North/West/East/etc position. */
 if(refs[i].region){
    for(int j=before;j<result_count;j++)
       if(!results[j].region)results[j].region=refs[i].region;
 }
}
 /* The deepest named panel owns its controls. Unnamed layout wrappers
  * (e.g. outputsDialogLeft/Right) leave child names intact. */
 const char *caption=dialog_caption(id);
 if((!caption||!*caption)&&depth==0)caption=open_title;
 if(caption&&*caption){
   for(int i=first;i<result_count;i++)if(!results[i].group[0])
       copy_text(results[i].group,sizeof(results[i].group),caption);
 }
}
/* Contiguous group boundaries preserve the exact order of the ECU INI. */
static const char *config_group_label(int idx){
 if(idx<0||idx>=result_count)return open_title;
 return results[idx].group[0]?results[idx].group:open_title;
}
static int config_group_count(void){
 int n=result_count?1:0;
 for(int i=1;i<result_count;i++)
    if(strcasecmp(config_group_label(i),config_group_label(i-1)))n++;
 return n;
}
static int config_group_start(int group){
 if(group<0)return -1;
 if(!result_count)return -1;
 if(group==0)return 0;
 int n=0;
 for(int i=1;i<result_count;i++){
    if(strcasecmp(config_group_label(i),config_group_label(i-1))){
       if(++n==group)return i;
    }
 }
 return -1;
}
static int config_group_end(int start){
 if(start<0||start>=result_count)return 0;
 int i=start+1;
 while(i<result_count&&!strcasecmp(config_group_label(i),config_group_label(start)))i++;
 return i;
}
static int config_group_of(int index){
 if(index<0||index>=result_count)return 0;
 int n=0;
 for(int i=1;i<=index;i++)
    if(strcasecmp(config_group_label(i),config_group_label(i-1)))n++;
 return n;
}
static void config_group_shift(int direction){
 int count=config_group_count();if(count<2)return;
 int current=config_group_of(selected);
 int next=(current+direction+count)%count;
 int first=config_group_start(next);
 if(first>=0){selected=first;scroll_y=0;}
}
static void open_cal(int id,const char*label);
static void open_ini_graph(int id,int returnpage,int returnselected);
static void open_target(const char*target,const char*label){result_count=detail_sel=0;snprintf(open_title,sizeof(open_title),"%s",label);
 resolve_item(target,0);bts_panel_preview_init();ui_page=PAGE_DETAIL;selected=scroll_y=0;
 if(!result_count){note("INI view has no supported fields or graphs");return;}
 bts_context_named(label,target);
 if(result_count==1 && results[0].type==4){
    int ret=favorites_origin?PAGE_FAVORITES:(search_origin?PAGE_SEARCH:(group_item>=0?PAGE_GROUP:PAGE_MENU));
    open_ini_graph(results[0].field,ret,0);return;
 }
 /* A table-only dialog goes straight to the usable coloured map. */
 if(results[0].type==1||results[0].type==5){
   bool from_3d_ini=results[0].type==5;
   field_return_page=favorites_origin?PAGE_FAVORITES:(search_origin?PAGE_SEARCH:(detail_from_maps?PAGE_MAPS:(group_item>=0?PAGE_GROUP:PAGE_MENU)));
   open_cal(results[0].field,results[0].label);
   map_3d_view=from_3d_ini;
   return;
 }
 note("Choose an INI-labeled ECU setting");}
static void edit_history_capture(EditSnapshot *s){
 memset(s,0,sizeof(*s));
 s->zlen=editor_length;
 if(editor_length>0&&editor_length<=EDIT_CAP)memcpy(s->z,editor,(size_t)editor_length);
 for(int a=0;a<2;a++){
    s->alen[a]=axis_ready[a]?axis_len[a]:0;
    if(s->alen[a]>0&&s->alen[a]<=512)memcpy(s->axis[a],axis_data[a],(size_t)s->alen[a]);
 }
}
static void edit_history_changed(void){
 if(edit_undo_at<0){
   edit_undo[0]=edit_undo_before;edit_undo_at=0;edit_undo_count=1;
 }
 /* Erase redo branch after a new edit. */
 edit_undo_count=edit_undo_at+1;
 if(edit_undo_count==EDIT_UNDO_CAP){
   memmove(edit_undo,edit_undo+1,(EDIT_UNDO_CAP-1)*sizeof(EditSnapshot));
   edit_undo_count--;edit_undo_at--;
 }
 edit_history_capture(&edit_undo[edit_undo_count++]);
 edit_undo_at=edit_undo_count-1;
}
static void edit_history_move(int direction){
 if(!editor_loaded||!staged){note("Enable local EDIT mode before undo/redo");return;}
 int next=edit_undo_at+direction;
 if(next<0||next>=edit_undo_count){note(direction<0?"No earlier local edit":"No later local edit");return;}
 EditSnapshot *s=&edit_undo[next];
 if(s->zlen!=editor_length){note("Undo history no longer matches this ECU field");return;}
 for(int a=0;a<2;a++)if(s->alen[a]!=(axis_ready[a]?axis_len[a]:0)){
    note("Axis data changed since edit; undo blocked");return;
 }
 memcpy(editor,s->z,(size_t)editor_length);
 for(int a=0;a<2;a++)if(s->alen[a]>0)memcpy(axis_data[a],s->axis[a],(size_t)s->alen[a]);
 edit_undo_at=next;map_text_dirty=true;confirm_write=false;
 /* All Undo routes (touch, keyboard and map tools) share this point.
  * Play the KidPix cue only after an undo actually succeeds. */
 if(direction<0){
   bool queued=bts_audio_play(BTS_SOUND_UNDO,event_audio_enabled);
   note(queued?"Undo applied; KidPix sound queued":"Undo applied; sound unavailable (see Soundboard)");
 }else note("Local edit redone; ECU unchanged");
}
static void open_cal(int id,const char*label){
 map_3d_view=false;
 if(id<0||id>=cal_count)return;
 Field *f=&cal[id];
 /* Direct editor entry and INI field actions can carry the actual match. */
 int context=bts_sound_for_entry(label,f->name);
 bts_context_play(context);
 open_field=id;editor_length=0;editor_loaded=staged=confirm_write=sd_backup=false;
 step_index=3;confirm_discard=false;edit_undo_count=0;edit_undo_at=-1;cell=grid_first_col=grid_first_row=0;map_focus=0;axis_cursor[0]=axis_cursor[1]=0;ui_page=PAGE_FIELD;
 map_text_dirty=true;map_tools_open=false;map_anchor_valid=false;map_diff_mode=false;
 snapshot_valid=snapshot_diff_mode=false;snapshot_ref_number=0;
 current_table=-1;
 for(int i=0;i<table_count;i++)if(!strcasecmp(tables[i].z,f->name)){current_table=i;break;}
 for(int i=0;i<2;i++){
    axis_field[i]=-1;axis_len[i]=axis_tries[i]=0;axis_ready[i]=axis_readonly[i]=false;
 }
 if(current_table>=0){
    TableDef *t=&tables[current_table];
    extend_live_for_table(t);
    axis_field[0]=field_index(t->x);axis_field[1]=field_index(t->y);axis_readonly[0]=t->xro;axis_readonly[1]=t->yro;
    for(int i=0;i<2;i++)if(axis_field[i]>=0){
       Field *a=&cal[axis_field[i]];
       if(a->bytes<=0||a->bytes>512)axis_field[i]=-1;
    }
 }
 snprintf(open_title,sizeof(open_title),"%s",(label&&*label)?label:f->name);
 parse_choices(f);
 if(f->bytes<=0||f->bytes>EDIT_CAP){note("Field too large / unsupported");return;}
 pending_cal_field=id;cal_requested=true;
 note("Reading actual ECU calibration...");
}
static bool changed(void){
 if(editor_loaded&&memcmp(original,editor,(size_t)editor_length)!=0)return true;
 for(int a=0;a<2;a++)if(axis_ready[a]&&axis_len[a]>0&&
      memcmp(axis_original[a],axis_data[a],(size_t)axis_len[a])!=0)return true;
 return false;
}
static bool focus_axis_ready(int a){
 if(a<0||a>1||axis_field[a]<0||!axis_ready[a]||axis_readonly[a])return false;
 const Field *f=&cal[axis_field[a]];
 return !f->readOnly && f->validScale && f->kind==K_ARRAY && axis_len[a]>=f->bytes;
}
static int focused_index(void){return map_focus==0?cell:axis_cursor[map_focus-1];}
static bool active_focus_data(Field **out_field,uint8_t **out_data,int *out_len,int *out_index){
 if(map_focus==0){if(!editor_loaded||open_field<0)return false;
   *out_field=&cal[open_field];*out_data=editor;*out_len=editor_length;*out_index=cell;return true;}
 int a=map_focus-1;if(!focus_axis_ready(a))return false;
 *out_field=&cal[axis_field[a]];*out_data=axis_data[a];*out_len=axis_len[a];*out_index=axis_cursor[a];return true;
}
/* TunerStudio orientation: X low -> high left to right; Y low -> high bottom to top.
 * The ECU's actual array indexes are NEVER reordered when reading/writing.
 * Only viewport and keypad orientation change. */
static double axis_value(int axis,int index);
static bool axis_reverse(int axis,int size){
 if(size<2)return false;
 double first=axis_value(axis,0),last=axis_value(axis,size-1);
 if(isfinite(first)&&isfinite(last)&&fabs(last-first)>1e-9)
    return axis==0 ? first>last : first<last;
 return axis==1; /* Typical INI axes are stored low -> high. */
}
static int to_view(int axis,int raw,int count){return axis_reverse(axis,count)?count-1-raw:raw;}
static int to_raw(int axis,int view,int count){return axis_reverse(axis,count)?count-1-view:view;}
static float to_view_pos(int axis,float raw,int count){return axis_reverse(axis,count)?count-1-raw:raw;}
static void move_cell(int dx,int dy){
 if(open_field<0)return;
 Field*f=&cal[open_field];int w=f->width,h=f->height;
 if(w<1||h<1)return;
 /* A curve is a 1-D sequence even when its INI backing array is Nx1 or 1xN.
  * D-pad left/right always select the adjacent point. Up selects the editable
  * X breakpoint (if available); down selects the Y output value. */
 if(current_table>=0&&tables[current_table].curve){
    int count=w*h;
    if(dx){cell+=dx;if(cell<0)cell=0;if(cell>=count)cell=count-1;}
    axis_cursor[0]=cell;
    if(dy<0){if(focus_axis_ready(0))map_focus=1;
        else note("INI X axis not writable / awaiting ECU data");}
    if(dy>0)map_focus=0;
    return;
 }
 int rawx=cell%w,rawy=cell/w;
 int vx=to_view(0,rawx,w),vy=to_view(1,rawy,h);
 bool grid=in_map_view()&&current_table>=0&&!tables[current_table].curve;
 if(grid){
  if(map_focus==0){
   /* X-axis headers are along the BOTTOM. Y-axis labels are at the LEFT. */
   if(dy>0&&vy==h-1&&axis_field[0]>=0){
       map_focus=1;axis_cursor[0]=rawx;return;
   }
   if(dx<0&&vx==0&&axis_field[1]>=0){
       map_focus=2;axis_cursor[1]=rawy;return;
   }
  }else if(map_focus==1){
   int ax=to_view(0,axis_cursor[0],w);
   ax+=dx;if(ax<0)ax=0;if(ax>=w)ax=w-1;
   axis_cursor[0]=to_raw(0,ax,w);
   if(dy<0){
      map_focus=0;
      cell=to_raw(1,h-1,h)*w+axis_cursor[0];
   }
   return;
  }else if(map_focus==2){
   int ay=to_view(1,axis_cursor[1],h);
   ay+=dy;if(ay<0)ay=0;if(ay>=h)ay=h-1;
   axis_cursor[1]=to_raw(1,ay,h);
   if(dx>0){
      map_focus=0;
      cell=axis_cursor[1]*w+to_raw(0,0,w);
   }
   return;
  }
 }
 vx+=dx;vy+=dy;
 if(vx<0)vx=0;if(vx>=w)vx=w-1;
 if(vy<0)vy=0;if(vy>=h)vy=h-1;
 cell=to_raw(1,vy,h)*w+to_raw(0,vx,w);
 /* The 10x10 viewport smoothly follows the visible cursor when drawn. */
}
static void adjust_cell(double dir){
 if(open_field<0||!editor_loaded)return;
 if(!staged){note("Press EDIT or SELECT to stage changes");return;}
 Field *f=NULL;uint8_t *buf=NULL;int len=0,index=0;
 if(!active_focus_data(&f,&buf,&len,&index)){
   note(map_focus?"INI axis unavailable or readOnly":"ECU data not loaded");return;}
 if(f->kind==K_BITS&&choice_count>0){note("Use A for the INI option list");return;}
 if(f->readOnly){note("INI marks this field read-only");return;}
 if(!f->validScale){note("Unsupported numeric encoding; edit blocked");return;}
 double val=field_value(buf,len,f,index);
 if(!isfinite(val)){note("Invalid ECU numeric value");return;}
 edit_history_capture(&edit_undo_before);
 int sz=dsize(f->type),off=index*sz;
 uint8_t before[8];if(sz<1||sz>8||off<0||off+sz>len)return;
 memcpy(before,buf+off,(size_t)sz);
 if(!set_number(buf,len,f,index,val+(f->kind==K_BITS?dir:steps[step_index]*dir))){
    note("Value exceeds INI limits or numeric encoding");return;}
 if(!memcmp(before,buf+off,(size_t)sz)){
    note("Step smaller than ECU value resolution; use L/R");return;
 }
 edit_history_changed();
 /* Fine-tuning a cell is not reopening the setting. Keep value-change sounds
  * on explicit choice/direct-entry actions only. */
 confirm_write=false;map_text_dirty=true;
 note("Value staged locally; ECU unchanged");
}

/* Direct numeric entry: editing is local only, independent of physical buttons.
 * The underlying INI scale/range/representation remains authoritative. */
static void numeric_edit_prompt(void){
 if(!editor_loaded||!staged){note("SELECT first to stage local calibration edits");return;}
 Field *f=NULL;uint8_t *buf=NULL;int len=0,index=0;
 if(!active_focus_data(&f,&buf,&len,&index)||f->kind==K_BITS||f->readOnly){
    note("Select a writable numeric cell / use A options for enums");return;
 }
 double old=field_value(buf,len,f,index);
 if(!isfinite(old)){note("Cannot edit: invalid ECU value");return;}
 char entry[40];snprintf(entry,sizeof(entry),"%.6f",old);
 SwkbdState kb;swkbdInit(&kb,SWKBD_TYPE_NORMAL,2,24);
 swkbdSetHintText(&kb,"Exact decimal value (INI units)");
 swkbdSetInitialText(&kb,entry);
 if(swkbdInputText(&kb,entry,sizeof(entry))!=SWKBD_BUTTON_RIGHT)return;
 char *end=NULL;double value=strtod(entry,&end);
 while(end&&isspace((unsigned char)*end))end++;
 if(end==entry||!end||*end||!isfinite(value)){note("Invalid number; enter digits with optional decimal");return;}
 edit_history_capture(&edit_undo_before);
 uint8_t previous[8];int size=dsize(f->type),off=index*size;
 if(size<1||size>8||off+size>len){note("Invalid INI value address");return;}
 memcpy(previous,buf+off,(size_t)size);
 if(!set_number(buf,len,f,index,value)){
    note("Value outside INI limits or numeric encoding");return;
 }
 if(memcmp(previous,buf+off,(size_t)size)==0){
    note("No stored change: use a larger step for this field");return;
 }
 if(index==0 && (int)old!=(int)value){
    int cue=bts_sound_for_change(f->name,"",old,value);
    if(cue<0)cue=bts_sound_for_entry(f->name,f->name);
    bts_context_play(cue);
 }
 edit_history_changed();confirm_write=false;map_text_dirty=true;
 note("Exact numeric value staged locally; START reviews burn");
}
/* Region edit tools: stage atomically, retain one undo frame per operation.
 * A failed numeric encoding never leaves part of the map altered. */
static BtsMapRegion map_selected_region(void){
 Field *f=&cal[open_field];
 return bts_map_region(f->width,f->height,map_anchor,cell,map_anchor_valid);
}
static bool snapshot_eligible(void){
 return editor_loaded&&open_field>=0&&open_field<cal_count&&editor_length>0&&
      editor_length<=EDIT_CAP&&ecu_session&&ecu_signature[0]&&menu_loaded&&
      cal[open_field].page>=1&&cal[open_field].page<=64;
}
static void snapshot_save_current(void){
 if(!snapshot_eligible()){
    note("Snapshot requires confirmed ECU identity and a loaded calibration field");return;
 }
 mkdir("sdmc:/BrickECU",0777);mkdir("sdmc:/BrickECU/snapshots",0777);
 char path[105];FILE *fp=NULL;
 for(int n=1;n<=9999;n++){
    snprintf(path,sizeof(path),"sdmc:/BrickECU/snapshots/field-%04d.bts",n);
    FILE *existing=fopen(path,"rb");if(existing){fclose(existing);continue;}
    fp=fopen(path,"wb");break;
 }
 if(!fp){note("Cannot create local SD calibration snapshot");return;}
 const Field *f=&cal[open_field];
 bool ok=bts_snapshot_write(fp,ecu_signature,f->name,f->page,f->offset,editor,editor_length);
 if(fclose(fp)!=0)ok=false;
 if(!ok){remove(path);note("SD calibration snapshot incomplete; removed");return;}
 note(changed()?"SD snapshot saved: LOCAL EDITS included; NOT burned":"SD snapshot saved: current loaded field values");
}
static void snapshot_compare_latest(void){
 if(!snapshot_eligible()){
    note("Compare requires confirmed ECU identity and loaded field");return;
 }
 DIR *d=opendir("sdmc:/BrickECU/snapshots");
 if(!d){note("No local field snapshots found");return;}
 const Field *f=&cal[open_field];
 static uint8_t candidate[EDIT_CAP];
 int latest=0,scanned=0;
 struct dirent *e;
 while((e=readdir(d))!=NULL){
    if(++scanned>2048)break; /* don't block the 3DS scanning an arbitrary SD directory */
    const char *name=e->d_name;
    if(strlen(name)!=14||strncmp(name,"field-",6)||strcmp(name+10,".bts"))continue;
    int version=0;bool digits=true;
    for(int k=6;k<10;k++){if(name[k]<'0'||name[k]>'9'){digits=false;break;}
       version=version*10+(name[k]-'0');}
    if(!digits||version<=latest)continue;
    char path[105];snprintf(path,sizeof(path),"sdmc:/BrickECU/snapshots/%s",name);
    FILE *fp=fopen(path,"rb");if(!fp)continue;
    bool ok=bts_snapshot_read(fp,ecu_signature,f->name,f->page,f->offset,candidate,editor_length);
    fclose(fp);
    if(ok){memcpy(snapshot_reference,candidate,(size_t)editor_length);latest=version;}
 }
 closedir(d);
 if(!latest){note("No compatible verified field snapshot on SD");return;}
 snapshot_valid=snapshot_diff_mode=true;map_diff_mode=false;snapshot_ref_number=latest;
 map_text_dirty=true;
 note("Comparing local field against last matching SD snapshot; ECU unchanged");
}
static void map_tools_apply(void){
 if(open_field<0||!editor_loaded){note("Load the real ECU calibration first");return;}
 Field *f=&cal[open_field];int w=f->width,h=f->height,n=w*h;
 if(n<2||n>EDIT_CAP||f->kind==K_BITS||!f->validScale){note("This INI field does not support map tools");return;}
 BtsMapRegion r=map_selected_region();
 int action=map_tool_cursor;
 if(action==0){map_anchor=cell;map_anchor_valid=true;note("Corner marked. B to map, move cursor, reopen Tools");return;}
 if(action==1){map_anchor_valid=false;note("Range cleared; active region is one cell");return;}
 if(action==13){snapshot_diff_mode=false;map_diff_mode=!map_diff_mode;note(map_diff_mode?"Differences from ECU baseline highlighted":"Normal value heatmap restored");return;}
 if(action==14||action==15){edit_history_move(action==14?-1:1);return;}
 if(action==16){snapshot_save_current();return;}
 if(action==17){snapshot_compare_latest();return;}
 if(action==11){
    map_clip_w=r.x1-r.x0+1;map_clip_h=r.y1-r.y0+1;
    for(int y=0;y<map_clip_h;y++)for(int x=0;x<map_clip_w;x++){
       double v=field_value(editor,editor_length,f,(r.y0+y)*w+r.x0+x);
       if(!isfinite(v)){map_clip_w=map_clip_h=0;note("Selection contains invalid values");return;}
       map_clipboard[y*map_clip_w+x]=v;
    }
    char msg[90];snprintf(msg,sizeof(msg),"Copied %dx%d cells locally",map_clip_w,map_clip_h);note(msg);return;
 }
 if(action==12 && (map_clip_w<1||map_clip_h<1||cell%w+map_clip_w>w||cell/w+map_clip_h>h)){
    note("Clipboard absent or destination exceeds map bounds");return;
 }
 if(f->readOnly){note("INI marks this map read-only");return;}
 if(!staged){note("SELECT enables local EDIT for modifying map values");return;}
 for(int i=0;i<n;i++){
    map_values_before[i]=field_value(editor,editor_length,f,i);
    if(!isfinite(map_values_before[i])){note("Invalid map values; operation cancelled");return;}
    map_values_after[i]=map_values_before[i];
 }
 int modified=0;
 if(action>=2&&action<=9){
    int op=BTS_MAP_ADD;double amount=steps[step_index];
    if(action==3)amount=-amount;
    else if(action==4||action==5){op=BTS_MAP_PERCENT;amount=action==4?5:-5;}
    else if(action==6)op=BTS_MAP_INTERP_X;
    else if(action==7)op=BTS_MAP_INTERP_Y;
    else if(action==8)op=BTS_MAP_SMOOTH;
    else if(action==9)op=BTS_MAP_BILINEAR;
    modified=bts_map_transform(map_values_before,map_values_after,w,h,r,op,amount);
    if(modified<0){note("Selection too small or values invalid for operation");return;}
 }else if(action==10){
    for(int y=r.y0;y<=r.y1;y++)for(int x=r.x0;x<=r.x1;x++){
       int i=y*w+x;map_values_after[i]=field_value(original,editor_length,f,i);
       if(!isfinite(map_values_after[i])){note("Original baseline invalid");return;}
    }
 }else if(action==12){
    int px=cell%w,py=cell/w;
    for(int y=0;y<map_clip_h;y++)for(int x=0;x<map_clip_w;x++)
       map_values_after[(py+y)*w+px+x]=map_clipboard[y*map_clip_w+x];
 }else return;
 static uint8_t candidate[EDIT_CAP];
 memcpy(candidate,editor,(size_t)editor_length);
 for(int i=0;i<n;i++)if(fabs(map_values_after[i]-map_values_before[i])>1e-9){
    if(!set_number(candidate,editor_length,f,i,map_values_after[i])){
       note("INI limit/encoding rejected a cell; entire edit cancelled");return;
    }
 }
 if(!memcmp(candidate,editor,(size_t)editor_length)){note("No values changed");return;}
 edit_history_capture(&edit_undo_before);
 memcpy(editor,candidate,(size_t)editor_length);
 edit_history_changed();map_text_dirty=true;confirm_write=false;
 char msg[96];snprintf(msg,sizeof(msg),"Region tool applied locally (%dx%d); ECU unchanged",r.x1-r.x0+1,r.y1-r.y0+1);
 note(msg);
}
/* Verified full ECU backup on ESP32 storage, using the same endpoint as
 * the working phone tuner. No duplicate 3DS SD read or extra download dialog. */
static bool save_backup(int page){
 if(page<1||page>64){note("Invalid ECU page for backup");return false;}
 int size=page_sizes[page-1];
 int id=page<=page_ids_count?page_ids[page-1]:-1;
 if(size<=0||size>65536||id<0||id>65535){
    note("INI backup page definition invalid");return false;
 }
 if(!esp_backup_ready){note("ESP32 backup storage unavailable; no writes sent");return false;}
 char sig[320],body[550];
 if(!escape_json_string(ecu_signature,sig,sizeof(sig))){
    note("Invalid ECU signature for backup");return false;
 }
 int n=snprintf(body,sizeof(body),
     "{\"page\":%d,\"page_id\":%d,\"size\":%d,\"signature\":\"%s\"}",
     page,id,size,sig);
 if(n<=0||n>=(int)sizeof(body)){note("Backup request too long");return false;}
 char message[96];snprintf(message,sizeof(message),"Saving verified ECU page %d to ESP32...",page);note(message);
 if(post_api("/api/backup",body,130000)<0)return false;
 double actual_page=-1,actual_size=-1,crc=-1;char stored[40]={0};
 char *reply=http_body();
 if(!reply||!json_number(reply,"page",&actual_page)||
    !json_number(reply,"length",&actual_size)||
    !json_number(reply,"crc32",&crc)||
    !json_string(reply,"stored",stored,sizeof(stored))||
    actual_page!=page||actual_size!=size||crc<0||crc>4294967295.0||
    strcmp(stored,"ESP32_STORAGE")!=0){
    note("Incomplete ESP32 backup receipt; no writes sent");return false;
 }
 snprintf(message,sizeof(message),"ECU page %d backed up and CRC-verified",page);note(message);
 return true;
}
/* Fresh, independent output-channel RPM check immediately before writes.
 * A dashboard's last telemetry sample is never used as a safety interlock. */
static bool stopped_rpm(void){
 Field *f=lookup("RPMValue",true);
 if(!f||f->kind!=K_SCALAR||!f->validScale||f->offset<0){
    note("Cannot locate ECU RPM output channel; no write");return false;
 }
 int len=dsize(f->type);
 if(len<=0||len>8||f->offset+len>65536){note("ECU RPM encoding invalid; no write");return false;}
 uint8_t req[4]={(uint8_t)f->offset,(uint8_t)(f->offset>>8),(uint8_t)len,(uint8_t)(len>>8)};
 char hex[9];tohex(req,4,hex);
 if(post_ts("O",hex)<0){note("Fresh RPM query failed; no write");return false;}
 uint8_t raw[8];int got=get_hex_reply(raw,len);
 if(got!=len){note("ECU RPM query returned wrong length");return false;}
 double rpm=get_number(raw,len,0,f->type)*f->scale+f->translate;
 if(!isfinite(rpm)||fabs(rpm)>0.1){note("Engine must be stopped (ECU RPM not zero)");return false;}
 return true;
}
static bool arm_ecu(void){
 char sig[320],body[500];
 if(!escape_json_string(ecu_signature,sig,sizeof(sig)))return false;
 int n=snprintf(body,sizeof(body),
    "{\"confirm\":\"BENCH_ONLY_ENGINE_STOPPED_BACKUP_SAVED\",\"signature\":\"%s\"}",sig);
 if(n<0||n>=(int)sizeof(body))return false;
 note("Authorizing verified bench write...");
 if(post_api("/api/arm",body,12500)<0)return false;
 bool ok=false;char *reply=http_body();
 return reply&&json_bool(reply,"ok",&ok)&&ok;
}
static void disarm_ecu(void){(void)post_api("/api/disarm","{}",12500);}
static bool ts_write_block(int page,int off,const uint8_t*data,int n){if(page<1||page>64||n<1||n>128||off<0||off+n>65536)return false;
 int id=page<=page_ids_count?page_ids[page-1]:(page-1)<<8;if(id<0||id>65535)return false;uint8_t args[134];args[0]=(uint8_t)id;args[1]=id>>8;args[2]=(uint8_t)off;args[3]=off>>8;args[4]=(uint8_t)n;args[5]=n>>8;memcpy(args+6,data,n);char hex[269];tohex(args,n+6,hex);
 if(post_ts("C",hex)<0)return false;uint8_t verify[128];if(read_bytes(page,off,verify,n)!=n)return false;return !memcmp(verify,data,(size_t)n);}
static bool burn_page(int pg){int id=pg<=page_ids_count?page_ids[pg-1]:(pg-1)<<8;if(id<0||id>65535)return false;uint8_t args[2]={(uint8_t)id,(uint8_t)(id>>8)};char hex[5];tohex(args,2,hex);return post_ts("B",hex)>=0;}
/* Bench-only transaction: confirm signature + stopped RPM, verify a full
 * backup and each original value, then arm and send signed write/burn commands.
 * Never retry C/B automatically after an uncertain transport result. */
static bool task_idle(void);
typedef struct {Field *field;uint8_t *modified,*baseline;int length;} PendingField;
/* Executed exclusively by the network worker; gauges cannot contend. */
static void perform_queued_write(void){
 write_verified=false;
 u64 burn_started_at=osGetTime();
 if(!net_ok||!wifi_candidate){note("Wi-Fi disconnected; staged changes retained");return;}
 if(open_field<0||!editor_loaded||!staged||!changed()){
    note("No staged change to apply");return;
 }
 if(get_request("/api/status")<=0||last_http!=200){
    note("Fresh ESP32 status unavailable; no write");return;
 }
 decode_status();
 if(!write_compiled||!esp_write_access_ready){
    note("Installed ESP32 firmware has writes disabled");return;
 }
 if(!ecu_session||!*ecu_signature||strcmp(ecu_signature,ini_signature)){
    note("ECU session/INI signature mismatch; no write");return;
 }
 if(!esp_backup_ready){
    note("ESP32 backup storage unavailable; no write");return;
 }
 if(!stopped_rpm())return;
 PendingField pending[3];int pc=0;
 Field *zf=&cal[open_field];
 if(memcmp(original,editor,(size_t)editor_length))
    pending[pc++]=(PendingField){zf,editor,original,editor_length};
 for(int a=0;a<2;a++){
   if(!axis_ready[a]||axis_field[a]<0||axis_len[a]<=0)continue;
   if(memcmp(axis_original[a],axis_data[a],(size_t)axis_len[a])){
      if(!focus_axis_ready(a)){
        note("Axis unsupported or read-only; no write");return;
      }
      pending[pc++]=(PendingField){&cal[axis_field[a]],axis_data[a],axis_original[a],axis_len[a]};
   }
 }
 if(pc==0){note("No modified ECU fields");return;}
 bool backed[65]={false},burned[65]={false},armed=false;
 int chunks=0;
 /* Phase 1: require verified backups and compare-before-write, for ALL pages. */
 for(int i=0;i<pc;i++){
   PendingField *p=&pending[i];int pg=p->field->page;
   if(pg<1||pg>64||p->length<=0||p->field->offset<0||
      p->field->offset+p->length>page_sizes[pg-1]){
      note("INI page/field range invalid; no write");return;
   }
   if(!page_writeable[pg-1]||!page_burnable[pg-1]){
      note("INI page has no persistent burn command; no write");return;
   }
   if(!backed[pg]){
     if(!save_backup(pg))return;
     backed[pg]=true;
   }
   uint8_t *check=(uint8_t*)malloc((size_t)p->length);
   if(!check){note("Insufficient verify memory; no write");return;}
   bool same=read_bytes(pg,p->field->offset,check,p->length)==p->length&&
             !memcmp(check,p->baseline,(size_t)p->length);
   free(check);
   if(!same){note("ECU field changed; reload before writing");return;}
 }
 u64 preflight_done_at=osGetTime();
 /* Phase 2: independent RPM check, then short-lived server authorization. */
 if(!stopped_rpm())return;
 if(!arm_ecu()){
   char error[128];copy_text(error,sizeof(error),ui_note);
   disarm_ecu();note(error);return;
 }
 armed=true;
 /* No early returns below: disarm, including failed partial writes. */
 bool ok=true;
 for(int i=0;i<pc&&ok;i++){
   PendingField *p=&pending[i];int at=0;
   if(!stopped_rpm()){ok=false;break;}
   while(at<p->length){
      int start=0,len=0;
      at=bts_next_burn_span(p->baseline,p->modified,p->length,at,128,12,&start,&len);
      if(len==0)break;
      /* Gaps we include were already compared against an independent ECU
       * read in Phase 1. Do not retry an uncertain command result. */
      note("Writing and verifying ECU calibration...");
      if(!ts_write_block(p->field->page,p->field->offset+start,p->modified+start,len)){
         note("Write outcome uncertain; inspect ECU before retry");ok=false;break;
      }
      chunks++;
   }
   if(ok)burned[p->field->page]=true;
 }
 u64 writes_done_at=osGetTime();
 if(ok){
   bool has_flash=false;for(int page=1;page<=64;page++)if(burned[page])has_flash=true;
   /* Burn-start sound has been looping since the confirmed Burn press. */
   for(int page=1;page<=64;page++)if(burned[page]){
     note("Burning ECU flash; waiting for acknowledgement...");
     if(!burn_page(page)){
        note("Burn outcome uncertain; read ECU before retry");ok=false;break;
     }
   }
 }
 u64 flash_done_at=osGetTime();
 if(ok){
   for(int i=0;i<pc;i++){
     PendingField *p=&pending[i];
     uint8_t *verify=(uint8_t*)malloc((size_t)p->length);
     bool verified=verify&&read_bytes(p->field->page,p->field->offset,verify,p->length)==p->length
         &&!memcmp(verify,p->modified,(size_t)p->length);
     free(verify);
     if(!verified){note("Post-burn readback mismatch; inspect ECU");ok=false;break;}
   }
 }
 if(armed){disarm_ecu();armed=false;}
 if(!ok)return;
 bool power_cycle_needed=false;
 for(int i=0;i<pc;i++){
   if(pending[i].field->powerCycle)power_cycle_needed=true;
   memcpy(pending[i].baseline,pending[i].modified,(size_t)pending[i].length);
 }
 staged=false;edit_undo_count=0;edit_undo_at=-1;
 map_text_dirty=true;write_verified=true;
 rpm_limit_requested=false;rpm_limit_known=false;rpm_limit_retry_at=0;
 char msg[128];
 if(power_cycle_needed){
   power_cycle_popup=true;
   snprintf(msg,sizeof(msg),"Burn verified; ECU power cycle required");
 }
 else snprintf(msg,sizeof(msg),"Burn verified %.1fs (%d chunks, prep %.1fs, flash %.1fs)",
       (double)(osGetTime()-burn_started_at)/1000.,chunks,
       (double)(preflight_done_at-burn_started_at)/1000.,
       (double)(flash_done_at-writes_done_at)/1000.);
 note(msg);
}
static void request_burn(void){
 confirm_write=false;
 if(write_queued||write_inflight){note("Write already queued; no duplicate commands");return;}
 if(open_field<0||!editor_loaded||!staged||!changed()){
    note("No changed ECU data to write");return;
 }
 if(cal[open_field].readOnly){note("ECU INI marks this calibration read-only");return;}
 /* Queued writes take the next worker slot; never fail due to a live poll. */
 write_queued=true;
 bts_audio_start_burn_loop(event_audio_enabled);
 note("Burn queued; verifying ECU safety and backups");
}
static void back(void){
 if(map_tools_open){map_tools_open=false;note("Returned to map editor");return;}
 int old_page=ui_page;if(confirm_write){confirm_write=false;note("Write confirmation cancelled");return;}
 if(ui_page==PAGE_CHOICES){ui_page=PAGE_FIELD;selected=scroll_y=0;}
 else if(ui_page==PAGE_FAULT){ui_page=fault_return_page;selected=scroll_y=0;}
 else if(ui_page==PAGE_FAVORITES){ui_page=favorites_parent;selected=scroll_y=0;favorites_origin=false;}
 else if(ui_page==PAGE_SEARCH){ui_page=search_parent;selected=scroll_y=0;search_origin=false;}
 else if(ui_page==PAGE_FIELD){
     if(changed()&&!confirm_discard){
        confirm_discard=true;
        note("UNBURNED changes: B discards / A keeps editing");return;
     }
     confirm_discard=false;
     if(changed()){bts_audio_play(BTS_SOUND_UNSAVED,event_audio_enabled);
         note("Unburned local changes discarded; ECU unchanged");}
     staged=false;
     if(replay_trace_active)replay_trace_active=false;
     live_length=base_live_length;
     ui_page=field_return_page;
     selected=field_return_page==PAGE_DETAIL?detail_sel:(field_return_page==PAGE_TUNE_TREE?tune_bucket:0);
     scroll_y=0;}
 else if(ui_page==PAGE_DETAIL){
    if(tune_from_tree){ui_page=PAGE_TUNE_TREE;selected=tune_bucket;scroll_y=0;tune_from_tree=false;}
    else {ui_page=favorites_origin?PAGE_FAVORITES:(search_origin?PAGE_SEARCH:(detail_from_maps?PAGE_MAPS:(group_item>=0?PAGE_GROUP:PAGE_MENU)));selected=scroll_y=0;}
 }
 else if(ui_page==PAGE_GROUP){ui_page=PAGE_MENU;selected=group_parent_selected;scroll_y=0;group_item=-1;}
 else if(ui_page==PAGE_MENU){ui_page=menu_return_page;
   selected=ui_page==PAGE_TUNE_SECTION?tune_section:(ui_page==PAGE_TUNE_TREE?tune_bucket:0);
   scroll_y=0;group_item=-1;}
 else if(ui_page==PAGE_TUNE_SECTION){ui_page=PAGE_TUNE_TREE;selected=tune_bucket;scroll_y=0;}
 else if(ui_page==PAGE_TUNE_TREE){
   tune_from_tree=false;
   if(tune_lower_focus){
      if(tune_open_group>=0){tune_open_group=-1;tune_lower_selected=tune_lower_scroll=0;}
      else tune_lower_focus=false;
   }else{ui_page=PAGE_HUB;selected=0;scroll_y=0;tune_reset_preview();}
 }
 else if(ui_page==PAGE_COMPARE){ui_page=compare_return_page;replay_playing=false;}
 else if(ui_page==PAGE_COMPARE_PICK){ui_page=compare_return_page;replay_playing=false;}
 else if(ui_page==PAGE_QUALITY){ui_page=quality_return_page;replay_playing=false;}
 else if(ui_page==PAGE_INSIGHTS){ui_page=analysis_return_page;replay_playing=false;}
 else if(ui_page==PAGE_REPLAY_STATS){ui_page=stats_return_page;replay_playing=false;}
 else if(ui_page==PAGE_REPLAY){replay_playing=false;replay_trace_active=false;ui_page=PAGE_LOG_BROWSER;selected=log_browser_selected;scroll_y=0;}
 else if(ui_page==PAGE_TRACE_MAPS){ui_page=trace_return_page;replay_trace_active=false;selected=scroll_y=0;}
 else if(ui_page==PAGE_LOG_BROWSER){log_delete_pending=false;ui_page=log_browser_parent;selected=scroll_y=0;}
 else if(ui_page==PAGE_CHANNELS){live_length=base_live_length;ui_page=channels_return_page;selected=scroll_y=0;}
 else if(ui_page==PAGE_INI_GRAPH){
    live_length=base_live_length;ui_page=ini_graph_return;
    selected=ini_graph_return_selected;scroll_y=selected>5?selected-5:0;
 }
 else if(ui_page==PAGE_KNOCK||ui_page==PAGE_DASH||ui_page==PAGE_GRAPH){
    if(ui_page==PAGE_KNOCK)live_length=base_live_length;
    ui_page=ui_page==PAGE_DASH?PAGE_HUB:ui_page==PAGE_KNOCK?knock_return_page:graph_return_page;selected=scroll_y=0;
 }
 else if(ui_page==PAGE_TRIGGER_SIM){stim_preview=false;live_length=base_live_length;ui_page=stim_return_page;
    selected=scroll_y=0;note("Offline trigger viewer closed; ECU unchanged");}
 else if(ui_page==PAGE_VIEWS){ui_page=PAGE_HUB;selected=scroll_y=0;}
 else if(ui_page==PAGE_GAUGE_PICK){ui_page=PAGE_DISPLAY;selected=scroll_y=0;}
 else if(ui_page==PAGE_DISPLAY){ui_page=display_parent;selected=scroll_y=0;}
 else if(ui_page==PAGE_DIAG){ui_page=PAGE_SETTINGS_HUB;selected=scroll_y=0;}
 else if(ui_page==PAGE_SOUND_TEST){ui_page=PAGE_SETTINGS_HUB;selected=scroll_y=0;}
 else if(ui_page==PAGE_RECORDER){ui_page=PAGE_LOGS_HUB;selected=scroll_y=0;}
 else if(ui_page==PAGE_MAPS){ui_page=maps_return_page;selected=scroll_y=0;}
 else if(ui_page==PAGE_CATS){ui_page=cats_return_page;}
 else if(workspace_page(ui_page)){ui_page=PAGE_HUB;selected=scroll_y=0;}
 if(ui_page!=old_page)nav_restore();
}
/* Every workspace entry points to an existing, real tool. No placeholders. */
static void workspace_enter(int page,int choice){
 int prev=ui_page;
 nav_capture();
 selected=scroll_y=0;
 if(page==PAGE_HUB){
    switch(choice){case 0:ui_page=PAGE_TUNE_TREE;tune_reset_preview();break;
    case 1:ui_page=PAGE_ANALYZE_HUB;break;
    case 2:ui_page=PAGE_LOGS_HUB;break;
    case 3:ui_page=PAGE_SETTINGS_HUB;break;}
 }else if(page==PAGE_TUNE_HUB){
    /* Legacy hub may still exist in old SD state, but it has no separate
     * maps/favorites/search route: all calibration starts in the ECU tree. */
    (void)choice;ui_page=PAGE_TUNE_TREE;tune_reset_preview();
 }else if(page==PAGE_LOGS_HUB){
    switch(choice){case 0:ui_page=PAGE_RECORDER;break;
    case 1:log_browser_parent=page;refresh_log_files();ui_page=PAGE_LOG_BROWSER;selected=log_browser_selected;break;
    case 2:if(replay_count>0){ui_page=PAGE_REPLAY;replay_playing=false;}else {log_browser_parent=page;refresh_log_files();ui_page=PAGE_LOG_BROWSER;}break;
    /* Live graphs are under INI View, not duplicate Logging. */
    /* Live channel inspector stays under View. */}
 }else if(page==PAGE_ANALYZE_HUB){
    if(replay_count<=0){
       log_browser_parent=page;refresh_log_files();ui_page=PAGE_LOG_BROWSER;
       if(replay_count<=0)note("Choose a recorded log, then return to Analyze");
    }else switch(choice){
       case 0:analysis_return_page=page;insights_open();break;
       case 1:compare_return_page=page;compare_open_picker();break;
       case 2:trace_return_page=page;replay_open_trace();break;
       case 3:quality_return_page=page;quality_open();break;
       case 4:stats_return_page=page;ui_page=PAGE_REPLAY_STATS;replay_playing=false;break;
    }
 }else if(page==PAGE_SETTINGS_HUB){
    switch(choice){
       case 0:display_parent=page;ui_page=PAGE_DISPLAY;break;
       case 1:ui_page=PAGE_DIAG;break;
       case 2:sfx_enabled=!sfx_enabled;save_gauge_preferences();
              note(sfx_enabled?"Menu beeps enabled":"Menu beeps disabled (event sounds unaffected)");return;
       case 3:ui_page=PAGE_SOUND_TEST;sound_test_last=-1;sound_test_queued=false;return;
    }
 }
 if(ui_page==prev)note("Feature currently unavailable in this session");
}
static void tune_enter_preview(void){
 if(!menu_loaded){note("No ECU INI loaded; waiting for SD cache or ESP bridge");return;}
 if(!tune_lower_focus){
    if(tune_preview_total(tune_bucket,tune_open_group)<=0){note("No INI menu entries in this tab");return;}
    tune_lower_focus=true;tune_lower_selected=tune_lower_scroll=0;
    return;
 }
 int at=tune_preview_index(tune_bucket,tune_open_group,tune_lower_selected);
 if(at<0 || at>=menu_count)return;
 MenuItem *m=&menus[at];
 if(m->group==1){tune_open_group=at;tune_lower_selected=tune_lower_scroll=0;
    bts_context_named(m->label,m->target);
    note("Open TunerStudio subfolder; B returns to its parent");return;}
 if(!*m->target){note("INI entry has no associated ECU dialog");return;}
 nav_capture();
 tune_from_tree=true;group_item=-1;detail_from_maps=false;
 search_origin=favorites_origin=false;
 open_target(m->target,m->label);
 /* Direct-to-map and single-graph transitions return to the lower INI menu,
  * not to an unrelated old PAGE_MENU or the all-maps browser. */
 if(ui_page==PAGE_FIELD)field_return_page=PAGE_TUNE_TREE;
 if(ui_page==PAGE_INI_GRAPH)ini_graph_return=PAGE_TUNE_TREE;
 if(ui_page==PAGE_DETAIL && result_count==1 && results[0].type!=3 && results[0].type!=4){
    int id=results[0].field;
    if(id>=0&&id<cal_count){
      bool from_3d_ini=results[0].type==5;
      field_return_page=PAGE_TUNE_TREE;
      open_cal(id,results[0].label);
      map_3d_view=from_3d_ini;
    }
 }
}
static void enter_selected(void){
 if(workspace_page(ui_page)){workspace_enter(ui_page,selected);return;}

 nav_capture();
 if(ui_page==PAGE_TUNE_TREE){tune_enter_preview();return;}
 if(ui_page==PAGE_TUNE_SECTION){
   int c=tune_section_at(tune_bucket,selected);
   if(c>=0){tune_section=selected;category=c;menu_return_page=PAGE_TUNE_SECTION;
      ui_page=PAGE_MENU;selected=scroll_y=0;}
   return;
 }
 if(ui_page==PAGE_FAULT){
   if(selected>=0&&selected<fault_link_count){
      if(!fault_active){note("ECU fault already resolved");return;}
      int id=fault_links[selected].field;
      if(id>=0&&id<cal_count){
        field_return_page=PAGE_FAULT;
        open_cal(id,fault_links[selected].label);
        note("Opened ECU-reported configuration field");return;
      }
      int menu=fault_links[selected].menu;
      if(menu>=0&&menu<menu_count&&menus[menu].target[0]){
         group_item=-1;detail_from_maps=false;search_origin=favorites_origin=false;
         open_target(menus[menu].target,menus[menu].label);
         if(ui_page==PAGE_FIELD)field_return_page=PAGE_FAULT;
         note("Opened related ECU INI setting from error report");return;
      }
   }
   if(selected==fault_link_count){
       /* Root fallback is always usable, even for an unfamiliar ECU error. */
       ui_page=PAGE_TUNE_TREE;tune_reset_preview();
       tune_bucket=0;selected=tune_bucket;scroll_y=0;
       if(fault_contains(fault_message,"fuel")||fault_contains(fault_message,"inject"))tune_bucket=1;
       else if(fault_contains(fault_message,"ignition")||fault_contains(fault_message,"spark"))tune_bucket=2;
       else if(fault_contains(fault_message,"crank"))tune_bucket=3;
       else if(fault_contains(fault_message,"idle"))tune_bucket=4;
       else if(fault_contains(fault_message,"boost")||fault_contains(fault_message,"vvt"))tune_bucket=5;
       else if(fault_contains(fault_message,"sensor")||fault_contains(fault_message,"analog"))tune_bucket=6;
       else if(fault_contains(fault_message,"can bus"))tune_bucket=7;
       selected=tune_bucket;
       note("Showing related ECU category; no exact setting identified");return;
   }
   fault_poll_requested=true;note("Refreshing current ECU fault...");return;
 }
 if(ui_page==PAGE_VIEWS){
   int which=selected;selected=scroll_y=0;
   if(which==0)ui_page=PAGE_DASH;
   else if(which==1)ui_page=PAGE_GRAPH;
   else if(which==2){ui_page=PAGE_KNOCK;extend_live_for_knock();}
   else if(which==3)ui_page=PAGE_RECORDER;
   else if(which==4){refresh_log_files();ui_page=PAGE_LOG_BROWSER;selected=log_browser_selected;}
   else if(which==5){ui_page=PAGE_CHANNELS;channel_cursor=selected=scroll_y=0;channel_live_extend();}
   else if(which==6){ui_page=PAGE_DISPLAY;display_return_page=PAGE_VIEWS;}
   else if(which==7){ui_page=PAGE_TUNE_TREE;tune_reset_preview();}
   else if(which==8)search_prompt();
   else if(which==9)ui_page=PAGE_DIAG;
   else if(which==10){sfx_enabled=!sfx_enabled;save_gauge_preferences();ui_page=PAGE_VIEWS;selected=10;sfx_play(1);note(sfx_enabled?"Menu sound ON":"Menu sound OFF");}
   else if(which==11)open_offline_trigger(false);
   return;
 }
 else if(ui_page==PAGE_LOG_BROWSER){log_browser_selected=selected;open_saved_log(selected);return;}
 else if(ui_page==PAGE_COMPARE_PICK){compare_pick_selected=selected;compare_open_log(selected);return;}
 else if(ui_page==PAGE_TRACE_MAPS){replay_open_selected_map(selected);return;}
 if(ui_page==PAGE_CHANNELS){channel_cursor=selected;channel_live_extend();note("INI channel selected; live reading on top screen");return;}
 if(ui_page==PAGE_DISPLAY){
  if(selected==0){gauge_style=(gauge_style+1)%7;save_gauge_preferences();}
  else if(selected>=1&&selected<=9){gauge_pick_slot=selected-1;ui_page=PAGE_GAUGE_PICK;selected=scroll_y=0;}

  return;
 }
 if(ui_page==PAGE_GAUGE_PICK){
  if(selected>=0&&selected<selectable_count){
    int idx=selectable_channels[selected];snprintf(gauge_names[gauge_pick_slot],sizeof(gauge_names[gauge_pick_slot]),"%s",channels[idx].name);
    gauge_fields[gauge_pick_slot]=idx;
    /* The previous channel's cached number must not appear for this channel. */
    gauge_hold_reset(&view_has[gauge_pick_slot]);needle_ready[gauge_pick_slot]=false;
    gauge_stats_reset(gauge_pick_slot);
    live_length=base_live_length;expand_live_for_gauges();base_live_length=live_length;
    if(current_table>=0&&current_table<table_count)extend_live_for_table(&tables[current_table]);
    save_gauge_preferences();ui_page=PAGE_DISPLAY;selected=gauge_pick_slot+1;scroll_y=selected>5?selected-5:0;
    note("Gauge channel saved to SD");
  }
  return;
 }
 if(ui_page==PAGE_CATS){if(selected==cat_total()+2){views_return_page=PAGE_CATS;ui_page=PAGE_VIEWS;selected=scroll_y=0;}else if(selected==cat_total()+1){search_parent=PAGE_CATS;search_prompt();}
    else if(selected==cat_total()){favorites_parent=PAGE_CATS;ui_page=PAGE_FAVORITES;selected=scroll_y=0;}
    else {category=selected;group_item=-1;search_origin=favorites_origin=false;menu_return_page=PAGE_CATS;ui_page=PAGE_MENU;selected=scroll_y=0;}}
 else if(ui_page==PAGE_MENU){
    int k=menu_index(selected);
    if(k>=0&&k<menu_count){
       if(menus[k].group==1){group_item=k;group_parent_selected=selected;ui_page=PAGE_GROUP;selected=scroll_y=0;
         bts_context_named(menus[k].label,menus[k].target);}
       else if(menus[k].target[0]){group_item=-1;detail_from_maps=false;open_target(menus[k].target,menus[k].label);}
    }
 }
 else if(ui_page==PAGE_GROUP){
    int k=group_child_index(selected);
    if(k>=0&&k<menu_count&&menus[k].target[0]){detail_from_maps=false;open_target(menus[k].target,menus[k].label);}
 }
 else if(ui_page==PAGE_DETAIL){if(selected>=0&&selected<result_count){
     if(results[selected].type==3){
       /* Only supported, guarded commands can be dispatched. The bridge's
        * /api/ts endpoint rejects all Z physical output test commands. */
       if(!strcmp(results[selected].command,"cmd_test_cancel"))
           request_output_test_abort();
       else if(is_stim_action(&results[selected]))
           preview_stim_action(&results[selected]);
       else if(is_sensor_capture_command(results[selected].command))
           begin_output_demo(&results[selected]);
       else note("Physical output test unavailable in BrickOS");
     }
     else if(results[selected].type==4){open_ini_graph(results[selected].field,PAGE_DETAIL,selected);}
      else if(results[selected].type==6){note("INI description: read only");}
      else if(results[selected].type==7){note("INI gauge: read only");}
      else if(results[selected].type==8){note("ECU status indicator: read only");}
     else {detail_sel=selected;field_return_page=PAGE_DETAIL;
       bool from_3d_ini=results[selected].type==5;
       open_cal(results[selected].field,results[selected].label);
       map_3d_view=from_3d_ini;
     }
 }}
 else if(ui_page==PAGE_SEARCH){if(selected>=0&&selected<search_count){
   SearchHit hit=search_hits[selected];search_origin=true;favorites_origin=false;detail_from_maps=false;group_item=-1;
   if(hit.type==0){MenuItem *m=&menus[hit.index];open_target(m->target,m->label);}
   else {TableDef *t=&tables[hit.index];int z=field_index(t->z);if(z>=0){
       field_return_page=PAGE_SEARCH;open_cal(z,*t->label?t->label:t->id);
     }else open_target(t->id,*t->label?t->label:t->id);
   }
 }}
 else if(ui_page==PAGE_FAVORITES){if(selected>=0&&selected<favorite_count){
    detail_from_maps=false;group_item=-1;favorites_origin=true;search_origin=false;
    open_target(favorites[selected].target,favorites[selected].label);
 }}
 else if(ui_page==PAGE_MAPS){if(selected>=0&&selected<table_count){
   TableDef*t=&tables[selected];search_origin=favorites_origin=false;detail_from_maps=true;group_item=-1;
   int z=field_index(t->z);if(z>=0){field_return_page=PAGE_MAPS;
     open_cal(z,*t->label?t->label:t->id);}
   else open_target(t->id,*t->label?t->label:t->id);
 }}
 else if(ui_page==PAGE_CHOICES){
    if(open_field>=0&&editor_loaded&&selected>=0&&selected<choice_count){
       Field *f=&cal[open_field];
       if(f->readOnly){note("INI option is read-only");return;}
       edit_history_capture(&edit_undo_before);
       double old_choice=field_value(editor,editor_length,f,0);
       int chosen=choices[selected].value;
       int cue=isfinite(old_choice)?bts_sound_for_change(f->name,choices[selected].label,old_choice,(double)chosen):-1;
       if(set_number(editor,editor_length,f,0,(double)chosen)){
         if(isfinite(old_choice)&&(int)old_choice!=chosen){
            if(cue<0)cue=bts_sound_for_entry(f->name,f->name);
            bts_context_play(cue);
         }
         edit_history_changed();staged=true;confirm_write=false;ui_page=PAGE_FIELD;selected=scroll_y=0;
         note("Option staged locally - ECU unchanged");
       }else note("Unsupported INI choice encoding");
    }
 }
 else if(ui_page==PAGE_FIELD&&confirm_write)request_burn();
}
static int active_count(void){if(workspace_page(ui_page))return workspace_count(ui_page);if(ui_page==PAGE_TUNE_TREE)return tune_visible_buckets();
 if(ui_page==PAGE_TUNE_SECTION)return tune_section_count(tune_bucket);
 if(ui_page==PAGE_FAULT)return fault_link_count+2;if(ui_page==PAGE_TRACE_MAPS)return trace_map_count;if(ui_page==PAGE_COMPARE_PICK)return log_file_count;if(ui_page==PAGE_VIEWS)return 12;if(ui_page==PAGE_LOG_BROWSER)return log_file_count;if(ui_page==PAGE_CHANNELS)return selectable_count;if(!menu_loaded)return 0;if(ui_page==PAGE_CATS)return cat_total()+3;if(ui_page==PAGE_MENU)return count_category_items();
 if(ui_page==PAGE_GROUP)return group_child_count();if(ui_page==PAGE_CHOICES)return choice_count;
 if(ui_page==PAGE_DETAIL)return result_count;if(ui_page==PAGE_FAVORITES)return favorite_count;if(ui_page==PAGE_SEARCH)return search_count;if(ui_page==PAGE_MAPS)return table_count;if(ui_page==PAGE_DISPLAY)return 10;if(ui_page==PAGE_GAUGE_PICK)return selectable_count;return 0;}
static void pretty_label(char *out,size_t cap,const char *src){
 if(!out||cap==0)return;snprintf(out,cap,"%s",src?src:"");
 bool word=true;
 for(char *p=out;*p;p++){
  if(*p=='_'||*p=='-'){*p=' ';word=true;continue;}
  if(isspace((unsigned char)*p)||*p=='/'||*p=='('){word=true;continue;}
  if(word&&islower((unsigned char)*p))*p=(char)toupper((unsigned char)*p);
  word=false;
 }
}
static const char *gauge_slot_label(int i){
 if(i<0||i>=GAUGE_SLOTS)return "--";
 int idx=gauge_fields[i];return idx>=0&&idx<chan_count?channels[idx].name:gauge_names[i];
}
static const char *gauge_slot_unit(int i){
 if(i<0||i>=GAUGE_SLOTS)return "";
 int idx=gauge_fields[i];return idx>=0&&idx<chan_count?channels[idx].unit:"";
}
static void value_label(int i,char *out,size_t cap){
 if(!view_has[i])snprintf(out,cap,"--");
 else if(gauge_decimals[i]>=0&&gauge_decimals[i]<=3)
     snprintf(out,cap,"%.*f",gauge_decimals[i],view_values[i]);
 else if(fabsf(view_values[i])>=1000)snprintf(out,cap,"%.0f",view_values[i]);
 else if(fabsf(view_values[i])>=100)snprintf(out,cap,"%.1f",view_values[i]);
 else snprintf(out,cap,"%.2f",view_values[i]);
}
static void live_graph_region(int slot,float x,float y,float w,float h){
 rect(x,y,w,h,panel);
 char name[64],val[40];pretty_label(name,sizeof(name),gauge_slot_label(slot));value_label(slot,val,sizeof(val));
 if(strlen(name)>26)name[26]=0;
 text(x+5,y+3,.37f,cream,name);text(x+w-83,y+3,.41f,bright,val);
 float mn=INFINITY,mx=-INFINITY;
 u32 now=(u32)osGetTime();
 for(int n=0;n<graph_count;n++){
  int at=(graph_head-1-n+GRAPH_SAMPLES)%GRAPH_SAMPLES;
  if((u32)(now-graph_ms[at])>(u32)graph_seconds*1000U)break;
  if(graph_ok[slot][at]){float v=graph_values[slot][at];if(v<mn)mn=v;if(v>mx)mx=v;}
 }
 float plotx=x+7,ploty=y+23,plotw=w-14,ploth=h-28;
 rect(plotx,ploty+ploth*.5f,plotw,1,outline);
 if(!isfinite(mn)||!isfinite(mx)){text(x+8,y+30,.36f,orange,"Awaiting ECU samples");return;}
 if(mx-mn<.001f){mx+=.5f;mn-=.5f;}
 bool prev=false;float px=0,py=0;
 for(int n=graph_count-1;n>=0;n--){
  int at=(graph_head-1-n+GRAPH_SAMPLES)%GRAPH_SAMPLES;
  if((u32)(now-graph_ms[at])>(u32)graph_seconds*1000U){prev=false;continue;}
  if(!graph_ok[slot][at]){prev=false;continue;}
  float ax=plotx+plotw*(1.0f-(float)(now-graph_ms[at])/(graph_seconds*1000.0f));
  float ay=ploty+ploth*(1.0f-(graph_values[slot][at]-mn)/(mx-mn));
  if(prev)C2D_DrawLine(px,py,slot%3==0?leaf:slot%3==1?dew:petal,ax,ay,slot%3==0?leaf:slot%3==1?dew:petal,1.6f,0);
  px=ax;py=ay;prev=true;
 }
}
static void shorten(char*dst,int cap,const char*src,int max);
static const char* home_tile_name(int i);
/* Clean full-screen live digital instrument panel. No ornamental meshes can
 * cover the readings: on all non-graph/map pages the top always shows gauges. */
static void home_digit(int slot,float x,float y,float w,float h,bool focused){
 (void)focused; /* Gauges display measurements; RPM is not a selected menu. */
 BTSCardBody *body=&bts_card_world.card[slot%BTS_CARD_COUNT];
 x+=body->x;y+=body->y;
 float skew=(bts_card_world.gravity_x*.9f+bts_card_world.spin_y*.35f);
 /* The actual card is a rounded extrusion; normal digital text is drawn on
  * its near face, sharing the same eye disparity so there is no ghosting. */
 x+=bts_front(2.7f);
 bts_rounded_solid(x,y,w,h,7.6f,
       C2D_Color32(14,24,32,168),C2D_Color32(56,82,96,187),4.2f,skew);
 char label[78],name[78],val[42];
 pretty_label(label,sizeof(label),gauge_slot_label(slot));
 shorten(name,sizeof(name),label,(w>=180?23:14));
 value_label(slot,val,sizeof(val));
 if(ui_page==PAGE_DASH&&gauge_stat_view!=0){
    GaugeCapture *cap=&gauge_capture[slot];
    if(cap->count){
       double data=gauge_stat_view==1?cap->maximum:gauge_stat_view==2?cap->minimum:cap->mean;
       snprintf(val,sizeof(val),fabs(data)>=1000.0?"%.0f":"%.2f",data);
    }else snprintf(val,sizeof(val),"--");
 }
 float label_scale=h>=75?.49f:.43f;
 float value_scale=h>=100?1.26f:h>=75?1.09f:h>=52?.90f:.65f;
 /* Center text on its 3D face. Never stretch lettering with the card body. */
 C2D_Text t;float tw=0,th=0;
 C2D_TextParse(&t,textbuf,name);C2D_TextOptimize(&t);
 C2D_TextGetDimensions(&t,label_scale,label_scale,&tw,&th);
 text(x+fmaxf(8,(w-tw)*.5f),y+4,label_scale,secondary,name);
 C2D_TextParse(&t,textbuf,val);C2D_TextOptimize(&t);
 C2D_TextGetDimensions(&t,value_scale,value_scale,&tw,&th);
 text(x+fmaxf(8,(w-tw)*.5f),y+(h>=75?29:h>=52?22:16),value_scale,white,val);
 const char *unit=gauge_slot_unit(slot);
 if(*unit&&h>=52){char short_unit[20];snprintf(short_unit,sizeof(short_unit),"%.10s",unit);
   text(x+w-55,y+h-20,.43f,secondary,short_unit);}
 if(view_has[slot])rect(x+w-8,y+8,3,8,live_stale?orange:green);
}
static void home_status_rail(void){
 float x=5+bts_front(4.f),w=86;
 bts_rounded_solid(x,8,w,222,7.f,C2D_Color32(12,20,29,170),C2D_Color32(43,64,78,186),4.5f,0.f);
 text(x+8,13,.40f,bright,"CONNECTION");
 rect(x+8,34,w-16,1,outline);
 text(x+9,40,.37f,secondary,"WI-FI");
 text(x+9,58,.46f,wifi_candidate?green:orange,wifi_candidate?"LINK UP":"OFFLINE");
 text(x+9,91,.36f,secondary,"DS BATTERY");
 if(battery_known){
   for(int i=0;i<5;i++)rect(x+9+i*12,110,9,10,i<battery_level?green:outline);
   text(x+9,126,.36f,battery_level<=1?orange:secondary,battery_charging?"CHARGING":"LEVEL / 5");
 }else text(x+9,112,.40f,secondary,"UNAVAILABLE");
 text(x+9,159,.37f,secondary,"LIVE RX");
 char rate[28];
 if(ecu_session&&live_valid)snprintf(rate,sizeof(rate),"%.1f Hz",actual_hz);
 else snprintf(rate,sizeof(rate),"-- Hz");
 text(x+9,177,.55f,ecu_session&&!live_stale?green:orange,rate);
 text(x+9,211,.31f,live_stale?orange:secondary,ecu_session?(live_stale?"RX HELD":"ECU READY"):"NO ECU");
}
static void shorten(char*dst,int cap,const char*src,int max);
static void draw_scalar_top(void){
 draw_depth_grid(400,240);
 char title[100];shorten(title,sizeof(title),open_title,39);
 text(11,6,.57f,white,title);
 if(open_field<0||open_field>=cal_count)return; const Field*f=&cal[open_field];
 text(11,28,.40f,secondary,f->kind==K_BITS?"ECU OPTION / INI CALIBRATION":"ECU SCALAR / INI CALIBRATION");
 round_panel(10,48,380,100,6,panel);
 text(20,57,.36f,secondary,staged?"STAGED LOCAL VALUE":"VERIFIED ECU READ");
 if(!editor_loaded){
   text(20,96,.80f,orange,"Reading from ECU...");
 }else if(!f->validScale){
   text(20,96,.58f,orange,"Unsupported INI scaling");
 }else{
   double v=field_value(editor,editor_length,f,0);
   char value[85];
   if(!isfinite(v))snprintf(value,sizeof(value),"Unavailable");
   else if(f->kind==K_BITS){
      const char*choice=choice_for_value((int)v);
      if(choice)shorten(value,sizeof(value),choice,26);
      else snprintf(value,sizeof(value),"Option %.0f",v);
   }else snprintf(value,sizeof(value),"%.4f  %.14s",v,f->unit);
   text(20,94,strlen(value)>18?.61f:.87f,white,value);
 }
 if(editor_loaded&&f->validScale){
   double before=field_value(original,editor_length,f,0);
   char line[110];
   if(isfinite(before))snprintf(line,sizeof(line),"ORIGINAL ECU READING: %.4f",before);
   else snprintf(line,sizeof(line),"ORIGINAL ECU READING: unavailable");
   text(17,163,.41f,secondary,line);
   if(f->limitsKnown){
     snprintf(line,sizeof(line),"INI LIMITS: %.4f to %.4f  %s",f->low,f->high,f->unit);
     text(17,182,.40f,cream,line);
   }else text(17,182,.40f,secondary,"INI does not specify numeric limits");
 }
 if(f->readOnly)text(17,205,.42f,orange,"INI READ ONLY - VIEW WITHOUT EDITING");
 else if(f->powerCycle)text(17,205,.42f,orange,"POWER CYCLE REQUIRED AFTER BURN");
 else text(17,205,.38f,secondary,"CHANGES ARE LOCAL UNTIL VERIFIED BURN");
 text(17,224,.32f,bright,"BOTTOM SCREEN: CHANGE VALUE / REVIEW BURN");
}
static void home_dashboard(void){
 draw_depth_grid(400,240);
 /* Home never inherits user preferences that hide instruments. 2x3 = six
  * useful real channels, always the same positions when launching. */
 if(ui_page==PAGE_HUB){
    home_status_rail();
    for(int i=0;i<6;i++){
       float x=98+(i%2)*149,y=8+(i/2)*74;
       home_digit(i,x,y,144,66,false);
    }
    return;
 }
 if(gauge_style==0){
   for(int i=0;i<4;i++)home_digit(i,8+(i%2)*194,9+(i/2)*106,188,99,false);
 }else if(gauge_style==1){
   for(int i=0;i<3;i++)home_digit(i,8+i*130,11,124,201,false);
 }else if(gauge_style==2){
   home_digit(0,8,10,384,93,false);
   for(int i=1;i<=4;i++)home_digit(i,8+((i-1)%2)*194,129+((i-1)/2)*42,188,37,false);
 }else if(gauge_style==3){
   for(int i=0;i<6;i++)home_digit(i,8+(i%3)*130,10+(i/3)*106,124,98,false);
 }else if(gauge_style==4){
   for(int i=0;i<9;i++)home_digit(i,8+(i%3)*130,8+(i/3)*69,124,68,false);
 }else if(gauge_style==5){
   home_digit(0,8,12,384,200,false);
 }else{
   home_digit(0,8,12,188,200,false);
   home_digit(1,204,12,188,200,false);
 }
 rect(0,217,400,23,soil);rect(0,217,400,1,outline);
 text(11,220,.38f,secondary,gauge_styles[gauge_style]);
 if(ui_page==PAGE_DASH){
    const char *mode[]={"LIVE","MAX","MIN","AVG"};
    text(202,220,.37f,gauge_stat_view?orange:green,mode[gauge_stat_view]);
 }
 text(271,220,.35f,bright,"L/R VIEW");
}

/* Upper screen is the tuning tree. No duplicate menus on the lower screen.
 * The lower touchscreen previews the highlighted item's meaning and actions. */
static bool tune_context_page(void){
 return ui_page==PAGE_TUNE_TREE||ui_page==PAGE_TUNE_SECTION||
        ui_page==PAGE_MENU||ui_page==PAGE_GROUP||ui_page==PAGE_DETAIL||ui_page==PAGE_MAPS;
}
static int tune_menu_index_for(int cat,int n){
 int start=cat_start(cat);if(start<0||n<0)return -1;
 int end=cat_end(start);
 for(int pass=0;pass<2;pass++)for(int i=start+1;i<end;i++){
   if(menus[i].group==2)continue;
   if((menus[i].group==1)!=(pass==0))continue;
   if(n--==0)return i;
 }
 return -1;
}
static int tune_menu_count_for(int cat){
 int start=cat_start(cat);if(start<0)return 0;
 int count=0;
 for(int i=start+1;i<cat_end(start);i++)if(menus[i].group!=2)count++;
 return count;
}
static int tune_root_preview_category(void){
 if(ui_page!=PAGE_TUNE_TREE || tune_section_count(selected)!=1)return -1;
 int cat=tune_section_at(selected,0);
 return cat>=0?cat:-1;
}
static int tune_top_count(void){
 if(ui_page==PAGE_TUNE_TREE){
    int c=tune_root_preview_category();
    return c>=0?tune_menu_count_for(c):tune_section_count(selected);
 }
 if(ui_page==PAGE_TUNE_SECTION)return tune_section_count(tune_bucket);
 if(ui_page==PAGE_MENU)return count_category_items();
 if(ui_page==PAGE_GROUP)return group_child_count();
 if(ui_page==PAGE_DETAIL)return result_count;
 if(ui_page==PAGE_MAPS)return table_count;
 return 0;
}
static const char *tune_top_name(int i){
 if(i<0)return "";
 if(ui_page==PAGE_TUNE_TREE||ui_page==PAGE_TUNE_SECTION){
    int c=tune_root_preview_category();
    if(c>=0){int j=tune_menu_index_for(c,i);return j>=0?menus[j].label:"";}
    int b=ui_page==PAGE_TUNE_TREE?selected:tune_bucket;
    int cat=tune_section_at(b,i),j=cat>=0?cat_start(cat):-1;
    if(cat==-2)return "All ECU Maps";
    return j>=0?menus[j].label:"";
 }
 if(ui_page==PAGE_MENU){int j=menu_index(i);return j>=0?menus[j].label:"";}
 if(ui_page==PAGE_GROUP){int j=group_child_index(i);return j>=0?menus[j].label:"";}
 if(ui_page==PAGE_DETAIL)return i<result_count?results[i].label:"";
 if(ui_page==PAGE_MAPS)return i<table_count?(*tables[i].label?tables[i].label:tables[i].id):"";
 return "";
}
static void draw_ts_icon(int index,float x,float y,float scale){
 if(ts_icon_sheet && index>=0 && (size_t)index<C2D_SpriteSheetCount(ts_icon_sheet)){
    C2D_Image im=C2D_SpriteSheetGetImage(ts_icon_sheet,index);
    C2D_DrawImageAt(im,x,y,0,NULL,scale,scale);
 }else{round_panel(x+4,y+4,23,23,4,panel2);text(x+11,y+7,.52f,cream,"+");}
}
/* Center labels and live values within the actual 63 px gauge panel.
 * No inner accent strip: it previously crossed the leftmost value digits. */
static void bts_tune_gauge_center_text(float x,float y,float width,
                                       float scale,u32 color,const char *content){
 C2D_Text measured;
 C2D_TextParse(&measured,textbuf,content);C2D_TextOptimize(&measured);
 float tw=0.f,th=0.f;
 if(scale<BTS_MIN_TEXT_SCALE)scale=BTS_MIN_TEXT_SCALE;
 C2D_TextGetDimensions(&measured,scale,scale,&tw,&th);
 /* Do not let a long live reading spill outside its gauge. */
 if(tw>width-6.f && tw>0.f)scale*= (width-6.f)/tw;
 if(scale<BTS_MIN_TEXT_SCALE)scale=BTS_MIN_TEXT_SCALE;
 C2D_TextGetDimensions(&measured,scale,scale,&tw,&th);
 /* Saturated readings should never draw over their neighboring gauges. */
 if(tw>width-4.f){text(x+width*.5f-9.f,y,.43f,orange,"OVR");return;}
 C2D_DrawText(&measured,C2D_WithColor,x+(width-tw)*.5f,y,0,scale,scale,color);
}
static void draw_tune_gauge_rail(void){
 const int channels[]={0,4,2};
 const char *labels[]={"RPM","CLT","AFR"};
 for(int i=0;i<3;i++){
    const float x=330+bts_rear(2.3f),y=13.0f+i*74.0f;
    round_panel(x+2,y+5,63,65,5,C2D_Color32(0,0,0,210));
    round_panel(x-1,y-1,65,67,5,outline);
    round_panel(x,y,63,65,4,panel);
    bts_tune_gauge_center_text(x,y+5,63,.46f,secondary,labels[i]);
    char value[32]="--";int m=channels[i];
    if(has_value[m]&&isfinite(displayed[m]))
      snprintf(value,sizeof(value),i==0?"%.0f":i==1?"%.1f":"%.2f",displayed[m]);
    bts_tune_gauge_center_text(x,y+29,63,i==0?.67f:.62f,
         has_value[m]?(live_stale?orange:white):secondary,value);
 }
}
static void draw_tune_categories_top(void){
 draw_depth_grid(400,240);
 int count=tune_visible_buckets();
 for(int i=0;i<count;i++){
    int col=i%2,row=i/2;
    bool active=tune_bucket==i;
    float lift=active?stereo_strength*3.f:0.f;
    float x=7+col*159+(active?bts_front(3.4f):0.f);
    float y=8+row*34-lift;
    if(active)round_panel(x+2,y+5,150,30,5,C2D_Color32(0,0,0,215));
    round_panel(x,y,150,30,5,active?muted_leaf:panel);
    if(active){
       rect(x,y+4,3,22,green);
       rect(x+6,y,138,1,bright);
    }
    if(i<10)draw_ts_icon(i,x+5,y-1,.70f);
    else draw_ts_icon(12,x+6,y-1,.70f);
    char show[34];shorten(show,sizeof(show),tune_bucket_names[i],17);
    text(x+36,y+5,.53f,active?white:cream,show);
 }
 draw_tune_gauge_rail();
 text(10,229,.36f,tune_lower_focus?green:bright,
      tune_lower_focus?"A: OPEN OPTION     B: CATEGORY":"A: SETTINGS     B: HOME");
}

/* Two-screen configuration editor: named INI subpanels on top, values and
 * visually distinct action buttons on the touchscreen below. */
/* TunerStudio's explicit North, West, Center, East and South placements are
 * real layout instructions, not extra folder names. Each screen stays a
 * usable part of the same INI dialog. */
static int bts_effective_region(int index){
 if(index<0||index>=result_count)return BTS_REGION_CENTER;
 return results[index].region?results[index].region:BTS_REGION_CENTER;
}
static bool bts_spatial_layout(void){
 for(int i=0;i<result_count;i++)if(results[i].region)return true;
 return false;
}
static const char *bts_region_label(int region){
 switch(region){
 case BTS_REGION_NORTH:return "NORTH";
 case BTS_REGION_WEST:return "WEST";
 case BTS_REGION_EAST:return "EAST";
 case BTS_REGION_SOUTH:return "SOUTH";
 default:return "CENTER";
 }
}
static int bts_first_in_region(int region){
 for(int i=0;i<result_count;i++)
    if(bts_effective_region(i)==region)return i;
 return -1;
}
static int bts_next_region_field(int direction){
 if(selected<0||selected>=result_count)return 0;
 int old=bts_effective_region(selected);
 for(int step=1;step<=5;step++){
    int region=(old-1+5+direction*step+25)%5+1;
    int first=bts_first_in_region(region);
    if(first>=0)return first;
 }
 return selected;
}
static bool bts_indicator_state(const Resolved *r,bool *on){
 if(!r||r->type!=8||r->field<0||r->field>=ini_indicator_count||!on)return false;
 const char *key=ini_indicators[r->field].channel;
 if(!*key)return false;
 /* A bare output channel is evaluated from real live ECU bytes. */
 for(const char *p=key;*p;p++)
     if(!isalnum((unsigned char)*p)&&*p!='_')return false;
 Field *ch=lookup((char*)key,true);
 if(!ch)return false;
 double value=0;
 if(!channel_live_value((int)(ch-channels),&value))return false;
 *on=value>.5;
 return true;
}
static bool bts_result_curve(const Resolved *r){
 if(!r||r->field<0||r->field>=cal_count)return false;
 const char *field=cal[r->field].name;
 for(int i=0;i<table_count;i++)if(tables[i].curve&&
      !strcasecmp(tables[i].z,field))return true;
 return false;
}
/* Draw actual calibration values returned by the USB ECU. Unread fields are
 * always marked unavailable; a synthetic graph must never look like data. */
static void draw_ini_spatial_plot(const Resolved *r,
                                  float x,float y,float w,float h){
 if(!r||r->field<0||r->field>=cal_count||w<16||h<18)return;
 const Field *f=&cal[r->field];
 const BtsPanelPreview *preview=NULL;
 for(int k=0;k<BTS_PANEL_PREVIEW_COUNT;k++){
    if(panel_previews[k].field==r->field){preview=&panel_previews[k];break;}
 }
 if(!preview||!preview->ready||preview->length!=f->bytes||
    !f->validScale||f->kind!=K_ARRAY){
     text(x+7,y+7,.35f,secondary,"READING ACTUAL ECU DATA");
     return;
 }
 int nx=f->width,ny=f->height,cells=nx*ny;
 if(nx<1||ny<1||cells>4096)return;
 double minimum=INFINITY,maximum=-INFINITY;
 for(int i=0;i<cells;i++){
    double value=field_value(preview->bytes,preview->length,f,i);
    if(!isfinite(value))continue;
    if(value<minimum)minimum=value;
    if(value>maximum)maximum=value;
 }
 if(!isfinite(minimum)||!isfinite(maximum)){
     text(x+7,y+7,.37f,orange,"ECU VALUES UNAVAILABLE");
     return;
 }
 float plot_x=x+7,plot_y=y+6,plot_w=w-14,plot_h=h-17;
 bool curve=bts_result_curve(r);
 if(curve){
    if(cells==1){rect(plot_x,plot_y+plot_h*.5f,3,3,green);}
    else {
       double delta=maximum-minimum;
       for(int px=0;px<(int)plot_w;px+=2){
          double where=px*(cells-1)/(double)plot_w;
          int left=(int)where;
          if(left>=cells)left=cells-1;
          int right=left+1<cells?left+1:left;
          double a=field_value(preview->bytes,preview->length,f,left);
          double b=field_value(preview->bytes,preview->length,f,right);
          if(!isfinite(a)||!isfinite(b))continue;
          double sample=a+(b-a)*(where-left);
          float yy=(float)(delta>1e-9?(sample-minimum)/delta:0.5);
          rect(plot_x+px,plot_y+plot_h-3-yy*(plot_h-4),2.5f,2.5f,green);
       }
    }
 }else {
    float cell_w=plot_w/nx,cell_h=plot_h/ny;
    double delta=maximum-minimum;
    for(int row=0;row<ny;row++)for(int col=0;col<nx;col++){
       int idx=(ny-1-row)*nx+col;
       double value=field_value(preview->bytes,preview->length,f,idx);
       if(!isfinite(value))continue;
       double fraction=delta>1e-9?(value-minimum)/delta:.5;
       if(fraction<0)fraction=0;
       if(fraction>1)fraction=1;
       u32 color=C2D_Color32((u8)(34+183*fraction),
                            (u8)(103+100*(1-fabs(2*fraction-1))),
                            (u8)(159-111*fraction),255);
       rect(plot_x+col*cell_w,plot_y+row*cell_h,
            fmaxf(.6f,cell_w-.6f),fmaxf(.6f,cell_h-.6f),color);
    }
 }
 char range[72];snprintf(range,sizeof(range),"%.1f to %.1f %s",
                         minimum,maximum,f->unit);
 text(plot_x,plot_y+plot_h+1,.30f,secondary,range);
}
static void draw_ini_spatial_zone(int region,float x,float y,float width,float height){
 int first=bts_first_in_region(region);if(first<0)return;
 bool active=bts_effective_region(selected)==region;
 round_panel(x,y,width,height,5,active?panel2:panel);
 if(active)rect(x,y+7,3,height-14,green);
 char name[92];shorten(name,sizeof(name),config_group_label(first),width>175?24:15);
 text(x+9,y+4,.46f,active?white:cream,name);
 text(x+width-42,y+5,.30f,secondary,bts_region_label(region));
 rect(x+8,y+25,width-16,1,outline);
 int indicator=-1,gauge=-1,visual=-1,fields=0;
 for(int i=0;i<result_count;i++)if(bts_effective_region(i)==region){
    if(indicator<0&&results[i].type==8)indicator=i;
    if(gauge<0&&results[i].type==7)gauge=i;
    if(visual<0&&(results[i].type==1||results[i].type==5||
                    results[i].type==4))visual=i;
    if(results[i].type==0||results[i].type==3)fields++;
 }
 float cursor=y+31;
 if(indicator>=0&&cursor+22<=y+height){
    Resolved *r=&results[indicator];BtsIniIndicator *ind=&ini_indicators[r->field];
    bool on=false,valid=bts_indicator_state(r,&on);
    const char *label=valid?(on?ind->on:ind->off):"STATUS: NO LIVE DATA";
    char shortened[80];shorten(shortened,sizeof(shortened),label,width>175?25:14);
    round_panel(x+7,cursor,width-14,22,4,panel2);
    rect(x+11,cursor+6,7,9,valid?(on?green:secondary):orange);
    text(x+23,cursor+4,.38f,valid?white:secondary,shortened);
    cursor+=26;
 }
 if(gauge>=0&&cursor+24<=y+height){
    Resolved *r=&results[gauge];double live=0;char value[32]="--";
    bool ok=channel_live_value(r->field,&live);
    if(ok)snprintf(value,sizeof(value),fabs(live)>=1000?"%.0f":"%.2f",live);
    char label[76];shorten(label,sizeof(label),r->label,width>175?20:12);
    text(x+9,cursor,.35f,secondary,label);
    text(x+width-67,cursor,.49f,ok?bright:orange,value);
    cursor+=23;
 }
 if(visual>=0&&cursor+35<y+height){
    Resolved *r=&results[visual];
    bool curve=bts_result_curve(r),graph=r->type==4;
    char title[85];shorten(title,sizeof(title),r->label,width>175?26:17);
    text(x+8,cursor,.39f,white,title);
    cursor+=20;
    float bottom=y+height-8;
    round_panel(x+7,cursor,width-14,bottom-cursor,4,panel2);
    if(graph){
       text(x+14,cursor+6,.39f,bright,"LIVE GRAPH");
       text(x+14,cursor+27,.35f,secondary,"A OPENS CHANNEL GRAPH");
    }else{
       /* The plot is the INI's own map or curve, not a generic placeholder. */
       draw_ini_spatial_plot(r,x+9,cursor+2,width-18,bottom-cursor-2);
    }
 }else if(fields){
    int shown=0;
    for(int i=0;i<result_count&&shown<3;i++){
       const Resolved *r=&results[i];
       if(bts_effective_region(i)!=region||(r->type!=0&&r->type!=3))continue;
       char item[70];shorten(item,sizeof(item),r->label,width>175?26:16);
       text(x+10,cursor+shown*19,.38f,i==selected?bright:secondary,item);
       shown++;
    }
 }
}
static void draw_ini_spatial_top(void){
 draw_depth_grid(400,240);
 char title[82];shorten(title,sizeof(title),open_title,39);
 text(8,5,.62f,white,title);
 text(302,8,.33f,bright,"INI LAYOUT");
 bool north=bts_first_in_region(BTS_REGION_NORTH)>=0;
 bool south=bts_first_in_region(BTS_REGION_SOUTH)>=0;
 float cy=north?79:31,cb=south?193:230;
 if(north)draw_ini_spatial_zone(BTS_REGION_NORTH,7,30,386,46);
 if(south)draw_ini_spatial_zone(BTS_REGION_SOUTH,7,195,386,36);
 int column_regions[]={BTS_REGION_WEST,BTS_REGION_CENTER,BTS_REGION_EAST};
 int n=0;for(int i=0;i<3;i++)if(bts_first_in_region(column_regions[i])>=0)n++;
 if(n){
   float gap=6,width=(386-(n-1)*gap)/(float)n;
   int ordinal=0;
   for(int i=0;i<3;i++){
      if(bts_first_in_region(column_regions[i])<0)continue;
      draw_ini_spatial_zone(column_regions[i],7+ordinal*(width+gap),
                           cy,width,cb-cy-3);
      ordinal++;
   }
 }
 text(8,233,.32f,secondary,"TOP: ORIGINAL INI POSITIONS / BOTTOM: INTERACTIVE CONTROLS");
}
static void draw_tune_dialog_top(void){
 if(bts_spatial_layout()){draw_ini_spatial_top();return;}
 draw_depth_grid(400,240);
 char label[82];shorten(label,sizeof(label),open_title,43);
 text(12,7,.56f,white,label);
 int group=config_group_of(selected),total=config_group_count();
 text(12,29,.35f,secondary,"ECU PANELS  /  L-R SWITCH SECTION");
 round_panel(7,48,153,179,5,panel);
 int first=group>6?group-6:0;
 for(int r=0;r<7&&first+r<total;r++){
    int i=first+r,start=config_group_start(i);
    if(start<0)break;
    float y=54+r*23;
    bool active=i==group;
    if(active){round_panel(11,y,145,21,3,muted_leaf);rect(12,y+3,3,15,green);}
    char name[76];shorten(name,sizeof(name),config_group_label(start),19);
    text(20,y+3,.43f,active?white:secondary,name);
 }
 round_panel(167,48,226,179,5,panel2);
 int at=config_group_start(group),until=config_group_end(at);
 if(at>=0){
    char title[90];shorten(title,sizeof(title),config_group_label(at),28);
    text(178,58,.55f,white,title);
    char amount[40];snprintf(amount,sizeof(amount),"%d controls  /  section %d of %d",until-at,group+1,total);
    text(178,83,.33f,secondary,amount);
 }
 if(selected>=0&&selected<result_count){
    const Resolved *r=&results[selected];
    rect(177,106,204,1,outline);
    char name[90];shorten(name,sizeof(name),r->label,31);
    text(179,114,.55f,white,name);
    if(r->type==3){
       round_panel(177,144,204,39,5,panel);
       text(190,149,.46f,orange,"TUNERSTUDIO COMMAND");
       text(179,168,.36f,secondary,
          !strcmp(r->command,"cmd_test_cancel")?
           "A SEND REAL ECU TEST ABORT":
          is_sensor_capture_command(r->command)?
           "A REAL SENSOR CAPTURE / L+R CONFIRM":
          is_stim_action(r)?"Uses guarded trigger action":
           "Physical output command not available");
    }else if(r->type==6){
       text(178,144,.44f,secondary,"INI INFORMATION / READ ONLY");
    }else if(r->type==7){
       double value=0.0;char shown[45]="Awaiting data";
       if(channel_live_value(r->field,&value))snprintf(shown,sizeof(shown),"%.2f",value);
       text(178,144,.50f,bright,shown);
       text(178,168,.33f,secondary,"LIVE ECU CHANNEL");
    }else if(r->type==4){
       text(178,144,.44f,bright,"A  OPEN LIVE GRAPH");
    }else if(r->field>=0&&r->field<cal_count){
       const Field *f=&cal[r->field];
       char detail[88];shorten(detail,sizeof(detail),f->name,31);
       text(179,144,.37f,secondary,detail);
       text(179,165,.41f,bright,f->kind==K_BITS?"A  CHOOSE VALUE":f->kind==K_ARRAY?"A  OPEN TABLE / CURVE":"A  EDIT VALUE");
    }
    text(178,201,.33f,secondary,"Settings use real ECU INI definitions");
 }
}
static void draw_tune_context_top(void){
 if(ui_page==PAGE_TUNE_TREE){draw_tune_categories_top();return;}
 if(ui_page==PAGE_DETAIL){draw_tune_dialog_top();return;}

 draw_depth_grid(400,240);
 /* Left: all ten TS tabs exactly as in the supplied screenshots. 11th
  * fallback collects unfamiliar firmware categories, never drops data. */
 round_panel(6,6,113,223,5,panel);
 int b=ui_page==PAGE_TUNE_TREE?selected:(ui_page==PAGE_MAPS?9:tune_bucket);
 if(ui_page!=PAGE_TUNE_TREE&&ui_page!=PAGE_TUNE_SECTION&&ui_page!=PAGE_MAPS &&
    menu_return_page==PAGE_CATS){
    int start=cat_start(category);
    if(start>=0)b=tune_bucket_of(menus[start].label);
 }
 if(b<0||b>=TUNE_BUCKET_COUNT)b=0;
 for(int i=0;i<TUNE_BUCKET_COUNT;i++){
   float y=12+i*19;
   if(i==b){round_panel(10,y-1,104,16,3,muted_leaf);rect(10,y,3,13,green);}
   char label[30];shorten(label,sizeof(label),tune_bucket_names[i],17);
   text(16,y+1,.43f,i==b?white:secondary,label);
 }
 /* Right: the actual INI menu children, never fabricated settings. */
 round_panel(124,6,271,223,5,panel);
 const char *heading=tune_bucket_names[b];
 if(ui_page==PAGE_MENU){int st=cat_start(category);if(st>=0)heading=menus[st].label;}
 else if(ui_page==PAGE_GROUP){if(group_item>=0)heading=menus[group_item].label;}
 else if(ui_page==PAGE_DETAIL)heading=open_title;
 else if(ui_page==PAGE_MAPS)heading="ECU Maps & Curves";
 char compact[77];shorten(compact,sizeof(compact),heading,29);
 text(132,12,.54f,white,compact);
 rect(132,34,253,1,outline);
 int total=tune_top_count();
 /* At the category root the selected category owns the left-side cursor.
  * When inside a folder, selected highlights the actual right-side item. */
 int focus=ui_page==PAGE_TUNE_TREE?-1:selected;
 if(total<=0){
    text(132,102,.42f,secondary,"No ECU INI items here");
    text(132,122,.35f,secondary,"Try another category");
 }else{
    /* Larger legible INI submenu labels; eight 22px rows remain visible. */
    const int rows=8;
    int first=focus>=rows?focus-rows+1:0;
    for(int r=0;r<rows&&first+r<total;r++){
       int i=first+r;
       float y=44+r*22;
       bool active=i==focus;
       float cardx=129+(active?bts_front(3.4f):0.f);
       float labelpop=cardx-129;
       if(active)round_panel(cardx,y-1,260,20,3,muted_leaf);
       const char*name=tune_top_name(i);
       char shortname[70];shorten(shortname,sizeof(shortname),name,27);
       text(137+labelpop,y+1,.54f,active?white:cream,shortname);
       if(active)text(374+labelpop,y+1,.44f,green,">");
    }
    if(total>rows){
      char page[30];
      if(focus<0)snprintf(page,sizeof(page),"+%d more",total-rows);
      else snprintf(page,sizeof(page),"%d/%d",focus+1,total);
      text(337,211,.32f,secondary,page);
    }
 }
 text(127,231,.29f,secondary,ui_page==PAGE_TUNE_TREE?
     "DPAD CHOOSE TAB  A OPEN": "DPAD MOVE  A OPEN  B BACK");
}

static void draw_live_graph_top(void){
 draw_depth_grid(400,240);
 for(int i=0;i<3;i++)live_graph_region(i+graph_bank*3,4,2+i*78,392,75);
}
/* Real INI-defined graph lines: each trace has independent scaling, so
 * simultaneously graphed channels with different units stay readable. */
static void draw_ini_graph_top(void){
 draw_depth_grid(400,240);
 if(ini_graph_selected<0||ini_graph_selected>=ini_graph_count)return;
 IniGraphDef*g=&ini_graphs[ini_graph_selected];
 rect(0,0,400,24,soil);
 char title[72];snprintf(title,sizeof(title),"%.29s",g->owner);
 text(7,2,.51f,cream,title);
 text(274,4,.31f,live_stale?orange:bright,live_stale?"Last Sample":"ECU Live");
 int lines=g->count<INI_GRAPH_LINES?g->count:INI_GRAPH_LINES;
 if(lines<1){text(15,107,.54f,orange,"No INI graph channels");return;}
 int cols=lines>=3?2:1,rows=(lines+cols-1)/cols;
 float w=cols==1?390.0f:193.0f;
 float h=(207.0f-3.0f*(rows-1))/(float)rows;
 u32 now=(u32)osGetTime();
 for(int slot=0;slot<lines;slot++){
   float x=5+(slot%cols)*(w+4),y=27+(slot/cols)*(h+3);
   rect(x,y,w,h,panel);
   char label[66];snprintf(label,sizeof(label),"%.*s",cols==2?15:34,g->channels[slot]);
   text(x+5,y+2,cols==2?.32f:.39f,cream,label);
   double mn=INFINITY,mx=-INFINITY,latest=NAN;
   for(int n=0;n<graph_count;n++){
     int at=(graph_head-1-n+GRAPH_SAMPLES)%GRAPH_SAMPLES;
     if((u32)(now-graph_ms[at])>(u32)(graph_seconds*1000))break;
     if(ini_graph_ok[slot][at]){
       double val=ini_graph_values[slot][at];
       if(val<mn)mn=val;if(val>mx)mx=val;
       if(!isfinite(latest))latest=val;
     }
   }
   if(!isfinite(latest)){
      Field *f=lookup(g->channels[slot],true);
      const char *reason=(!f||f->kind!=K_SCALAR||!f->validScale)?"INI channel unsupported":"Awaiting ECU data";
      text(x+8,y+27,.35f,orange,reason);continue;
   }
   char value[30];snprintf(value,sizeof(value),"%.2f",latest);
   text(x+w-65,y+2,.36f,bright,value);
   if(mx-mn<1e-5){mx+=1;mn-=1;}
   float px=x+5,py=y+21,pw=w-10,ph=h-26;
   rect(px,py+ph*.5f,pw,1,outline);
   bool prev=false;float lastx=0,lasty=0;
   for(int n=graph_count-1;n>=0;n--){
      int at=(graph_head-1-n+GRAPH_SAMPLES)%GRAPH_SAMPLES;
      if((u32)(now-graph_ms[at])>(u32)(graph_seconds*1000)){prev=false;continue;}
      if(!ini_graph_ok[slot][at]){prev=false;continue;}
      float gx=px+pw*(1.0f-(float)(now-graph_ms[at])/(graph_seconds*1000.0f));
      float gy=py+ph*(1.0f-(ini_graph_values[slot][at]-mn)/(mx-mn));
      if(prev)C2D_DrawLine(lastx,lasty,slot==0?leaf:slot==1?dew:slot==2?petal:bright,
                           gx,gy,slot==0?leaf:slot==1?dew:slot==2?petal:bright,2.0f,0);
      lastx=gx;lasty=gy;prev=true;
   }
 }
}
static void map_touch_button(float x,float y,float w,const char*label,bool active);
static void draw_note(const char*hint);
static void shorten(char*dst,int cap,const char*src,int max);
static void pretty_label(char*out,size_t cap,const char*src);
static bool runtime_value(const char*name,double*value);
static void draw_ini_graph_deck(void){
 if(ini_graph_selected<0||ini_graph_selected>=ini_graph_count)return;
 IniGraphDef *g=&ini_graphs[ini_graph_selected];
 text(9,27,.50f,cream,"Live INI Graphs");
 char summary[84];snprintf(summary,sizeof(summary),"%.30s - %d lines",g->id,g->count);
 text(9,45,.33f,secondary,summary);
 for(int i=0;i<g->count && i<INI_GRAPH_LINES;i++){
    float y=61+i*19;rect(7,y,306,18,panel);
    char label[70],shortlabel[45],value[32]="--";
    pretty_label(label,sizeof(label),g->channels[i]);
    shorten(shortlabel,sizeof(shortlabel),label,24);
    double sample=0;
    if(runtime_value(g->channels[i],&sample))snprintf(value,sizeof(value),"%.2f",sample);
    text(12,y+1,.36f,cream,shortlabel);
    text(262,y+1,.38f,bright,value);
 }
 map_touch_button(6,185,151,"Previous Graph",false);
 map_touch_button(163,185,151,"Next Graph",false);
 draw_note("L/R NEXT GRAPH  X TIME WINDOW  B BACK");
}
static void draw_topbar(void){
 rect(0,0,320,28,soil);rect(0,27,320,1,outline);
 round_panel(6,5,18,18,5,blue);text(11,6,.41f,white,"B");
 const char *where="HOME";
 if(workspace_page(ui_page))where=workspace_title(ui_page);
 else if(ui_page==PAGE_TUNE_TREE)where="TUNE / INI MENU";
 else if(ui_page==PAGE_TUNE_SECTION)where="TUNE / SECTIONS";
 else if(ui_page==PAGE_MENU)where="ECU SETTINGS";
 else if(ui_page==PAGE_GROUP)where="SETTINGS FOLDER";
 else if(ui_page==PAGE_MAPS)where="CALIBRATION MAPS";
 else if(ui_page==PAGE_FAVORITES)where="QUICK ACCESS";
 else if(ui_page==PAGE_SEARCH)where="SEARCH";
 else if(ui_page==PAGE_DETAIL)where="PARAMETERS";
 else if(ui_page==PAGE_CHOICES)where="OPTIONS";
 else if(ui_page==PAGE_FIELD)where="CALIBRATION EDITOR";
 else if(ui_page==PAGE_DIAG)where="CONNECTION";
 else if(ui_page==PAGE_FAULT)where="ECU FAULT";
 else if(ui_page==PAGE_DISPLAY)where="GAUGE SETTINGS";
 else if(ui_page==PAGE_VIEWS)where="INSTRUMENTS";
 else if(ui_page==PAGE_DASH)where="DIGITAL DASH";
 else if(ui_page==PAGE_GRAPH||ui_page==PAGE_INI_GRAPH)where="LIVE GRAPHS";
 else if(ui_page==PAGE_CHANNELS)where="ECU CHANNELS";
 else if(ui_page==PAGE_RECORDER||ui_page==PAGE_LOG_BROWSER)where="DATA LOGGER";
 else if(ui_page==PAGE_SOUND_TEST)where="SOUNDBOARD";
 else if(ui_page==PAGE_TRIGGER_SIM)where="TRIGGER DIAGNOSTICS";
 else if(ui_page==PAGE_KNOCK)where="KNOCK MONITOR";
 else if(ui_page==PAGE_INSIGHTS)where="LOG ANALYSIS";
 else if(ui_page==PAGE_QUALITY)where="DATA QUALITY";
 else if(ui_page==PAGE_COMPARE||ui_page==PAGE_COMPARE_PICK)where="COMPARE LOGS";
 text(32,4,.52f,white,where);
 /* Persistent, truthful state for operations that must not be overlooked. */
 if(log_recording)text(251,7,.35f,orange,"REC");
 else if(write_queued||write_inflight)text(245,7,.34f,orange,"BURN");
 else if(ui_page==PAGE_FIELD&&changed())text(244,7,.35f,bright,"EDIT");
 else if(ecu_session&&live_stale)text(244,7,.35f,orange,"HELD");
 if(fault_active){round_panel(270,3,20,22,5,berry);text(277,4,.56f,white,"!");}
 round_panel(294,9,16,9,4,ecu_session?(live_stale?orange:green):orange);
}
static void draw_note(const char *hint){
 rect(0,207,320,33,soil);rect(0,207,320,1,outline);
 char msg[90];snprintf(msg,sizeof(msg),"%.51s",ui_note);
 text(9,210,.38f,secondary,msg);
 text(9,223,.35f,bright,hint);
}
#define LIST_ROWS 4
#define LIST_ROW_H 37
#define LIST_ROW_Y 58
static void select_row(float y,bool active){
 round_panel(9,y+1,301,34,6,active?C2D_Color32(50,104,148,255):outline);
 round_panel(10,y+2,299,32,6,active?muted_leaf:panel);
 if(active)rect(11,y+9,3,18,green);
}
/* All folder icons are content-specific: no generic blue folder bitmap. */
static bool icon_has(const char*name,const char*needle){
 if(!name)return false;
 for(const char*s=name;*s;s++){
   const char *a=s,*b=needle;
   while(*a&&*b&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){a++;b++;}
   if(!*b)return true;
 }
 return false;
}
enum {ICON_POWER,ICON_FUEL,ICON_IGN,ICON_TRIGGER,ICON_SENSORS,ICON_THROTTLE,
      ICON_BOOST,ICON_COMMS,ICON_LOG,ICON_MAP,ICON_GRAPH,ICON_SEARCH,ICON_FAVORITES,ICON_SETTINGS};
static int category_icon(const char *name){
 if(icon_has(name,"fuel")||icon_has(name,"inject")||icon_has(name,"afr")||icon_has(name,"lambda"))return ICON_FUEL;
 if(icon_has(name,"ignition")||icon_has(name,"spark")||icon_has(name,"coil"))return ICON_IGN;
 if(icon_has(name,"trigger")||icon_has(name,"crank")||icon_has(name,"cam")||icon_has(name,"sync"))return ICON_TRIGGER;
 if(icon_has(name,"sensor")||icon_has(name,"input")||icon_has(name,"analog"))return ICON_SENSORS;
 if(icon_has(name,"idle")||icon_has(name,"throttle")||icon_has(name,"dbw"))return ICON_THROTTLE;
 if(icon_has(name,"boost")||icon_has(name,"turbo"))return ICON_BOOST;
 if(icon_has(name,"can")||icon_has(name,"network")||icon_has(name,"connection")||icon_has(name,"wi-fi"))return ICON_COMMS;
 if(icon_has(name,"log")||icon_has(name,"record")||icon_has(name,"replay"))return ICON_LOG;
 if(icon_has(name,"map")||icon_has(name,"table")||icon_has(name,"curve"))return ICON_MAP;
 if(icon_has(name,"graph")||icon_has(name,"gauge")||icon_has(name,"monitor")||icon_has(name,"channel")||icon_has(name,"dashboard")||icon_has(name,"instrument"))return ICON_GRAPH;
 if(icon_has(name,"search")||icon_has(name,"find"))return ICON_SEARCH;
 if(icon_has(name,"favorite")||icon_has(name,"quick"))return ICON_FAVORITES;
 return ICON_SETTINGS;
}
static void draw_category_icon(int kind,float x,float y){
 const u32 tint=kind==ICON_FUEL?green:kind==ICON_IGN?orange:
  kind==ICON_TRIGGER?petal:kind==ICON_BOOST?red:kind==ICON_COMMS?blue:bright;
 round_panel(x,y,29,29,6,C2D_Color32(32,58,83,255));
 const float cx=x+14.5f,cy=y+14.5f;
 switch(kind){
 case ICON_FUEL:
   C2D_DrawTriangle(cx,cy-10,tint,cx-7,cy+4,tint,cx+7,cy+4,tint,0);
   C2D_DrawCircleSolid(cx,cy+4,0,7,tint);break;
 case ICON_IGN:
   C2D_DrawTriangle(cx+1,cy-11,tint,cx-7,cy+2,tint,cx+1,cy+1,tint,0);
   C2D_DrawTriangle(cx-1,cy+11,tint,cx+8,cy-2,tint,cx,cy-1,tint,0);break;
 case ICON_TRIGGER:
   C2D_DrawCircleSolid(cx,cy,0,9,tint);
   C2D_DrawCircleSolid(cx,cy,0,5,panel);
   rect(cx-2,cy-12,4,3,tint);rect(cx-2,cy+9,4,3,tint);
   rect(cx-12,cy-2,3,4,tint);rect(cx+9,cy-2,3,4,tint);break;
 case ICON_SENSORS:
   C2D_DrawLine(cx-9,cy+8,tint,cx+9,cy-8,tint,2,0);
   C2D_DrawCircleSolid(cx-9,cy+8,0,3,tint);C2D_DrawCircleSolid(cx+9,cy-8,0,3,tint);break;
 case ICON_THROTTLE:
 case ICON_BOOST:
   C2D_DrawCircleSolid(cx,cy,0,9,tint);
   C2D_DrawCircleSolid(cx,cy,0,7,panel);
   C2D_DrawLine(cx,cy,tint,cx+5,cy-4,tint,2,0);break;
 case ICON_COMMS:
   C2D_DrawCircleSolid(cx-7,cy+6,0,3,tint);
   C2D_DrawCircleSolid(cx+7,cy+6,0,3,tint);
   C2D_DrawCircleSolid(cx,cy-7,0,3,tint);
   C2D_DrawLine(cx-7,cy+6,tint,cx,cy-7,tint,1.5f,0);
   C2D_DrawLine(cx,cy-7,tint,cx+7,cy+6,tint,1.5f,0);break;
 case ICON_LOG:rect(cx-8,cy-9,16,18,tint);rect(cx-5,cy-6,10,2,panel);
   rect(cx-5,cy-1,10,2,panel);rect(cx-5,cy+4,7,2,panel);break;
 case ICON_MAP:for(int n=0;n<3;n++){rect(cx-9,cy-8+n*7,18,3,tint);}
   rect(cx-2,cy-8,3,18,tint);break;
 case ICON_GRAPH:
   rect(cx-10,cy+5,5,4,tint);rect(cx-2,cy-1,5,10,tint);rect(cx+6,cy-8,5,17,tint);break;
 case ICON_SEARCH:
   C2D_DrawCircleSolid(cx-2,cy-2,0,7,tint);
   C2D_DrawCircleSolid(cx-2,cy-2,0,4,panel);
   C2D_DrawLine(cx+3,cy+3,tint,cx+9,cy+9,tint,2.4f,0);break;
 case ICON_FAVORITES:
   text(x+6,y+1,.70f,tint,"*");break;
 default:rect(cx-9,cy-6,18,3,tint);rect(cx-9,cy+2,18,3,tint);
   C2D_DrawCircleSolid(cx-3,cy-4.5f,0,2,panel);
   C2D_DrawCircleSolid(cx+4,cy+3.5f,0,2,panel);break;
 }
}
static void shorten(char*dst,int cap,const char*src,int max){
 if(!dst||cap<=0)return;
 if(!src)src="";
 /* memmove is required: several graph/replay paths shorten in place. */
 size_t n=strlen(src),copy=n;
 if(copy>=(size_t)cap)copy=(size_t)cap-1;
 memmove(dst,src,copy);
 dst[copy]=0;
 if(max>=cap)max=cap-1;
 if(max<3)return;
 if((int)n>max){dst[max-2]='.';dst[max-1]='.';dst[max]=0;}
}
static const char* home_tile_name(int i){
 if(ui_page==PAGE_VIEWS){
   static const char *apps[]={"DASHBOARD","LIVE GRAPHS","KNOCK","LOGGER", "REPLAY","CHANNELS", "DISPLAY","TUNE","SEARCH","WI-FI","SOUND","TRIGGER"};
   return i>=0&&i<12?apps[i]:"APP";
 }
 if(i==cat_total())return "FAVORITES";
 if(i==cat_total()+1)return "SEARCH";
 if(i==cat_total()+2)return "GAUGES";
 int a=cat_start(i);
 return a>=0?menus[a].label:"ECU";
}
static void draw_home_menu(void){
 const int total=ui_page==PAGE_VIEWS?12:cat_total()+3;
 if(total<=0)return;
 if(selected>=total)selected=total-1;
 if(selected<scroll_y)scroll_y=selected;
 if(selected>=scroll_y+LIST_ROWS)scroll_y=selected-LIST_ROWS+1;
 text(12,34,.55f,white,ui_page==PAGE_VIEWS?"WORKSPACE APPS":"ECU SYSTEMS");
 char count[30];snprintf(count,sizeof(count),"%d / %d",selected+1,total);
 text(261,37,.41f,secondary,count);
 for(int row=0;row<LIST_ROWS;row++){
   int i=scroll_y+row;if(i>=total)break;
   float y=LIST_ROW_Y+row*LIST_ROW_H;
   select_row(y,i==selected);
   const char *label=home_tile_name(i);
   int icon=(ui_page==PAGE_CATS&&i==cat_total())?ICON_FAVORITES:
     category_icon(label);
   draw_category_icon(icon,17,y+4);
   char shown[72];shorten(shown,sizeof(shown),label,23);
   text(54,y+5,.55f,i==selected?white:cream,shown);
   text(288,y+5,.55f,i==selected?green:secondary,">");
 }
 if(total>LIST_ROWS){
    float track=LIST_ROWS*LIST_ROW_H,thumb=fmaxf(16,track*(float)LIST_ROWS/(float)total);
    float pos=(float)scroll_y/(float)(total-LIST_ROWS)*(track-thumb);
    round_panel(313,LIST_ROW_Y,4,track,2,outline);
    round_panel(313,LIST_ROW_Y+pos,4,thumb,2,bright);
 }
 draw_note("A OPEN    B BACK    L/R GAUGES");
}
static void draw_workspace_list(void){
 int n=workspace_count(ui_page),root=ui_page==PAGE_HUB;
 if(root){
   text(13,33,.55f,white,"CHOOSE A WORKSPACE");
   const char *descriptions[]={"INI calibration and maps","Recorded log diagnostics",
                              "Capture and replay logs","Display, connection and tools"};
   for(int i=0;i<4;i++){
     float x=8+(i%2)*154,y=58+(i/2)*74;
     bool active=selected==i;
     round_panel(x,y,150,67,7,active?muted_leaf:panel);
     if(active)rect(x,y+7,4,51,green);
     int icon= i==0?ICON_MAP:i==1?ICON_GRAPH:i==2?ICON_LOG:ICON_SETTINGS;
     draw_category_icon(icon,x+10,y+9);
     text(x+46,y+8,.54f,active?white:cream,workspace_action(PAGE_HUB,i));
     char shortdesc[55];shorten(shortdesc,sizeof(shortdesc),descriptions[i],23);
     text(x+12,y+44,.32f,secondary,shortdesc);
   }
   draw_note("A OPEN  |  TOUCH A WORKSPACE  |  TOP: 6 GAUGES");
   return;
 }
 int rows=ui_page==PAGE_ANALYZE_HUB?5:LIST_ROWS;
 int row_y=ui_page==PAGE_ANALYZE_HUB?56:LIST_ROW_Y;
 int row_h=ui_page==PAGE_ANALYZE_HUB?30:LIST_ROW_H;
 text(11,30,.52f,white,workspace_title(ui_page));
 if(ui_page==PAGE_ANALYZE_HUB)
     text(12,44,.35f,replay_count?green:orange,replay_count?"RECORDED LOG READY":"OPEN A LOG TO ANALYZE");
 if(selected>=n)selected=n-1;if(selected<0)selected=0;
 if(selected<scroll_y)scroll_y=selected;
 if(selected>=scroll_y+rows)scroll_y=selected-rows+1;
 for(int r=0;r<rows;r++){
     int i=scroll_y+r;if(i>=n)break;
     float y=row_y+r*row_h;
     const char*label=workspace_action(ui_page,i);
     bool active=i==selected;
     round_panel(8,y,304,row_h-2,6,active?muted_leaf:panel);
     if(active)rect(8,y,4,row_h-2,green);
     draw_category_icon(category_icon(label),14,y+1);
     char shown[72];shorten(shown,sizeof(shown),label,29);
     text(51,y+6,.52f,active?white:cream,shown);
     text(287,y+4,.49f,active?green:secondary,">");
 }
 if(n>rows){
    float track=(float)(rows*row_h),thumb=fmaxf(14,track*rows/(float)n);
    float offset=(float)scroll_y/(float)(n-rows)*(track-thumb);
    round_panel(314,row_y,3,track,1,outline);
    round_panel(314,row_y+offset,3,thumb,1,bright);
 }
 draw_note("A OPEN  B BACK  |  L/R GAUGE LAYOUT");
}

/* Tune lower deck is CONTEXT and ACTIONS, not a second menu tree. */
static void tune_reference_strip(void){
 const int metrics[]={0,4,2};
 const char *labels[]={"RPM","CLT","AFR"};
 for(int i=0;i<3;i++){
    int m=metrics[i];float x=8+i*103;
    round_panel(x,32,98,55,4,panel);
    text(x+7,34,.47f,secondary,labels[i]);
    char buf[30]="--";
    if(has_value[m]&&isfinite(displayed[m]))
       snprintf(buf,sizeof(buf),m==0?"%.0f":m==4?"%.1f":"%.2f",displayed[m]);
    text(x+7,55,.77f,has_value[m]?(live_stale?orange:white):secondary,buf);
 }
}
static void draw_tune_preview_bottom(void){
 const int rows=7,rowheight=25,begin=30;
 /* Selected category is ALREADY named on top; spend lower LCD on its items. */
 int total=tune_preview_total(tune_bucket,tune_open_group);
 if(!menu_loaded){
    text(14,90,.43f,orange,"Waiting for ECU INI definition");
    text(14,113,.37f,secondary,"Connect Wi-Fi / cached ECU INI");
 }else if(!total){
    text(14,86,.45f,secondary,"No settings in this ECU category");
 }else{
    if(tune_lower_selected<0)tune_lower_selected=0;
    if(tune_lower_selected>=total)tune_lower_selected=total-1;
    if(tune_lower_selected<tune_lower_scroll)tune_lower_scroll=tune_lower_selected;
    if(tune_lower_selected>=tune_lower_scroll+rows)tune_lower_scroll=tune_lower_selected-rows+1;
    if(tune_lower_scroll<0)tune_lower_scroll=0;
    for(int r=0;r<rows;r++){
       int row=tune_lower_scroll+r;if(row>=total)break;
       int idx=tune_preview_index(tune_bucket,tune_open_group,row);
       if(idx<0)continue;
       const MenuItem *m=&menus[idx];bool focus=tune_lower_focus&&row==tune_lower_selected;
       float y=begin+r*rowheight;
       round_panel(6,y,307,23,3,focus?muted_leaf:panel);
       if(focus)rect(7,y+3,4,17,green);
       int map=find_table(m->target);
       draw_ts_icon(m->group==1?10:map>=0?11:12,7,y+1,.54f);
       char name[86];shorten(name,sizeof(name),m->label,33);
       text(34,y+2,.53f,focus?white:cream,name);
       if(m->group==1)text(297,y+3,.49f,focus?green:secondary,">");
        else if(*m->target&&favorite_has(m->target))text(296,y+3,.44f,bright,"*");
    }
    if(total>rows){
       float track=(float)(rows*rowheight),thumb=fmaxf(12.0f,track*rows/(float)total);
       float off=(float)tune_lower_scroll/(float)(total-rows)*(track-thumb);
       rect(315,begin,3,track,outline);rect(315,begin+off,3,thumb,bright);
    }
 }
 rect(0,210,320,1,outline);
 text(8,217,.37f,tune_lower_focus?green:bright,
      tune_lower_focus?"A OPEN  Y STAR  X FAVOURITES":"A SELECT  X FAVOURITES");
}
/* An idle instrument must not repeat an instrument visible above it.
 * Compare ECU byte identities in addition to names: INI aliases may have
 * different labels but map to the same underlying telemetry channel. */
static bool idle_same_channel(int a,int b){
 if(a<0||b<0||a>=chan_count||b>=chan_count)return false;
 const Field *x=&channels[a],*y=&channels[b];
 return !strcasecmp(x->name,y->name)||
   (x->offset==y->offset&&x->type==y->type&&x->kind==y->kind);
}
static bool idle_visible_above(int idx,bool home){
 if(home){
   /* Home top always displays slots 0..5. */
   for(int i=0;i<6;i++)if(idle_same_channel(idx,gauge_fields[i]))return true;
 }else{
   /* The tuning top rail always displays RPM, CLT and AFR. */
   const int reference[]={0,4,2};
   for(int j=0;j<3;j++){
      Field *f=lookup((char*)metric_fields[reference[j]],true);
      if(f&&idle_same_channel(idx,(int)(f-channels)))return true;
   }
 }
 return false;
}
static void idle_add_channel(int *out,int *count,int cap,int idx,bool home){
 if(*count>=cap||idx<0||idx>=chan_count)return;
 Field *f=&channels[idx];
 if(f->kind!=K_SCALAR||!f->validScale||f->offset<0||
    f->offset+dsize(f->type)>live_length)return;
 if(idle_visible_above(idx,home))return;
 for(int i=0;i<*count;i++)if(idle_same_channel(idx,out[i]))return;
 out[(*count)++]=idx;
}
static int idle_gauge_channels(int *out,int cap,bool home){
 int n=0;
 /* Preference slots 6..8 are not visible on Home; on Tune prefer familiar
  * secondary signals instead of repeating the RPM/CLT/AFR references. */
 if(home)for(int i=6;i<GAUGE_SLOTS;i++)idle_add_channel(out,&n,cap,gauge_fields[i],home);
 const int order[]={1,3,5,6,7,8,0,4,2};
 for(unsigned i=0;i<sizeof(order)/sizeof(order[0]);i++){
   Field *f=lookup((char*)metric_fields[order[i]],true);
   if(f)idle_add_channel(out,&n,cap,(int)(f-channels),home);
 }
 if(!home)for(int i=0;i<GAUGE_SLOTS;i++)idle_add_channel(out,&n,cap,gauge_fields[i],home);
 /* Only select recognizable additional physical channels, never raw state
  * codes or unknown binary items, when fewer than six references exist. */
 for(int i=0;i<selectable_count&&n<cap;i++){
   int idx=selectable_channels[i];const char *name=channels[idx].name;
   if(icon_has(name,"pressure")||icon_has(name,"temp")||
      icon_has(name,"voltage")||icon_has(name,"duty")||
      icon_has(name,"advance")||icon_has(name,"lambda")||
      icon_has(name,"battery")||icon_has(name,"throttle"))
      idle_add_channel(out,&n,cap,idx,home);
 }
 return n;
}
static void draw_tune_idle_gauges_bottom(void){
 bool home=ui_page==PAGE_HUB;
 int chosen[6],count=idle_gauge_channels(chosen,6,home);
 if(!count){
   text(11,95,.48f,secondary,"No additional ECU channels");
   text(11,122,.38f,secondary,"available without duplicates");
 }
 for(int i=0;i<count;i++){
   /* With three extra configured channels, give each a whole-width row. */
   bool wide=count<=3;
   float x=wide?7:6+(i%2)*156;
   float y=wide?33+i*56:32+(i/2)*57;
   float width=wide?306:152;
   round_panel(x,y,width,54,5,panel);
   if(i==0)rect(x,y+5,3,44,green);
   Field *f=&channels[chosen[i]];
   char label[68];pretty_label(label,sizeof(label),f->name);
   if(strlen(label)>(wide?33:19))label[wide?33:19]=0;
   text(x+9,y+2,.42f,secondary,label);
   double value=0;bool ok=channel_live_value(chosen[i],&value);
   char v[32]="--";
   if(ok)snprintf(v,sizeof(v),fabs(value)>=1000?"%.0f":fabs(value)>=100?"%.1f":"%.2f",value);
   text(x+9,y+20,.95f,ok?(live_stale?orange:white):secondary,v);
   if(*f->unit)text(x+width-70,y+23,.35f,secondary,f->unit);
 }
 text(10,216,.35f,bright,home?"ANY CONTROL RETURNS TO HOME":"ANY CONTROL RETURNS TO TUNING");
}

/* Resolve help from the exact INI field, map Z-value, or referenced dialog
 * control. Labels and menu groups themselves are not explanatory text. */
static const char *help_for_target(const char *target){
 const char *tip=help_find(target);
 if(tip)return tip;
 if(!target||!*target)return NULL;
 int table=find_table(target);
 if(table>=0){
    const TableDef *t=&tables[table];
    tip=help_find(t->z);
    if(tip)return tip;
 }
 for(int i=0;i<ref_count;i++)if(!strcasecmp(refs[i].owner,target)){
    tip=help_find(refs[i].ref);
    if(tip)return tip;
 }
 return NULL;
}
static const char *selected_help_text(void){
 if(ui_page==PAGE_GROUP){
    int idx=group_child_index(selected);
    return idx>=0?help_for_target(menus[idx].target):NULL;
 }
 if(ui_page==PAGE_DETAIL && selected>=0 && selected<result_count){
    const Resolved *r=&results[selected];
    if(r->type==6)return r->info;
     if(r->type==7)return help_find(r->command);
     if(r->field>=0&&r->field<cal_count && (r->type==0||r->type==1||r->type==5)){
       const char *tip=help_find(cal[r->field].name);
       if(tip)return tip;
    }
    return help_for_target(r->command);
 }
 return NULL;
}
/* Word-wrapped help with readable fixed font size; no tiny scaled paragraphs. */
static void draw_ini_help_lines(const char *help){
 char line[70];int used=0,rows=0;float y=125.f;
 const char *p=help;
 while(*p&&rows<5){
    while(*p==' ')p++;
    if(!*p)break;
    char word[70];int wn=0;
    while(*p&&*p!=' '){if(wn<(int)sizeof(word)-1)word[wn++]=*p;p++;}
    word[wn]=0;
    if(used && used+1+wn>43){
       text(10,y,.50f,cream,line);y+=17.f;rows++;used=0;
       if(rows>=5)break;
    }
    if(wn>43)wn=43;
    if(used){line[used++]=' ';}
    memcpy(line+used,word,(size_t)wn);used+=wn;line[used]=0;
 }
 if(used && rows<5)text(10,y,.50f,cream,line);
}
/* Only opened subfolders/dialog fields get context help. TunerStudio's
 * main categories and unopened Fuel/Setup/Ignition folders stay uncluttered. */
static void draw_subfolder_help_bottom(void){
 tune_reference_strip();
 const char *label=tune_top_name(selected);
 char shown[90];shorten(shown,sizeof(shown),label,37);
 text(10,91,.57f,white,shown);
 rect(10,117,300,1,outline);
 const char *help=selected_help_text();
 if(help&&*help)draw_ini_help_lines(help);
 else text(11,130,.47f,secondary,"No description provided by ECU INI");
 text(10,219,.45f,bright,"A OPEN / EDIT       B BACK");
}
static void draw_tune_navigation(void){
 int root=ui_page==PAGE_TUNE_TREE;
 const char *label=root?tune_bucket_names[selected>=0&&selected<TUNE_BUCKET_COUNT?selected:0]:tune_top_name(selected);
 char shortlabel[90];shorten(shortlabel,sizeof(shortlabel),label,37);
 text(11,35,.39f,secondary,root?"TUNERSTUDIO CATEGORY":"SELECTED ECU ITEM");
 text(11,53,.67f,white,shortlabel);
 rect(10,82,300,1,outline);
 int total=tune_top_count();
 char info[100];
 if(root)snprintf(info,sizeof(info),"%d %s in %s",total,
      tune_root_preview_category()>=0?"settings":"INI sections",
      tune_bucket_names[selected>=0&&selected<TUNE_BUCKET_COUNT?selected:0]);
 else if(ui_page==PAGE_TUNE_SECTION)snprintf(info,sizeof(info),"%d ECU folder(s); highlight and open",total);
 else if(ui_page==PAGE_MENU){
   int index=menu_index(selected);
   snprintf(info,sizeof(info),"%s  /  %d items",
       index>=0&&menus[index].group==1?"SUBFOLDER":"CALIBRATION",total);
 }else if(ui_page==PAGE_GROUP)snprintf(info,sizeof(info),"%d settings in this group",total);
 else if(ui_page==PAGE_DETAIL&&selected<result_count&&selected>=0){
   int fid=results[selected].field;
   if(fid>=0&&fid<cal_count){
      Field*f=&cal[fid];
      snprintf(info,sizeof(info),"%s  |  %s",f->kind==K_SCALAR?"SINGLE VALUE":f->kind==K_ARRAY?"TABLE / CURVE":"BITFIELD",*f->unit?f->unit:"INI VALUE");
   }else snprintf(info,sizeof(info),"INI dialog setting / action");
 }else if(ui_page==PAGE_MAPS)snprintf(info,sizeof(info),"INI maps available via the Tune tree");
 else snprintf(info,sizeof(info),"Browse the INI settings on top");
 char shown[95];shorten(shown,sizeof(shown),info,45);
 text(11,91,.42f,secondary,shown);
 bool empty=total<=0;
 round_panel(9,117,147,34,5,empty?outline:muted_leaf);
 text(25,126,.50f,empty?secondary:white,"A  OPEN SELECTED");
 round_panel(163,117,148,34,5,panel2);
 text(173,126,.41f,cream,"B  RETURN TO TREE");
 draw_note("A EDIT FIELD  |  B BACK  |  SELECT HOME");
}
static const char *const bts_audio_preview_names[BTS_SOUND_COUNT]={
 "Project Patta (Launch Control)", "BMW Bong (VVL)", "Asian Gong (4 cylinders / Honda)",
 "You Stupid Ni (Configuration Error)", "Bombaclat (MAP / Nitrous)",
 "Minecraft Firework (Limiter / Anti-Lag)", "Classic Hurt (Unsaved Edits)",
 "Jeopardy Theme (Burning)", "XP Level Up (Burn Verified)",
 "Fart With Reverb (Connected / Burn Fail)", "Real Hot Girl Ish (Coolant Hot)",
 "Shampoo Is Betta (Lambda Display)", "KidPix I Made a Boo Boo (Undo)",
 "Alex Jones Dracula (Cylinders > 6)"
};
/* A simple playable soundboard: select with D-pad or a single touch,
 * press A to replay. The event auto-play switch does not silence previews. */
static void draw_sound_test(void){
 char status[112];
 text(11,29,.59f,white,"SOUNDBOARD");
 if(!bts_ndsp_active)snprintf(status,sizeof(status),"DSP unavailable: 0x%08lX",(unsigned long)bts_ndsp_result);
 else snprintf(status,sizeof(status),"%u/%d LOADED   %s   AUTO: %s",
    bts_audio_loaded,BTS_SOUND_COUNT,bts_audio_ready?"READY":"FAIL",
    event_audio_enabled?"ON":"OFF");
 text(11,47,.43f,bts_audio_ready?green:orange,status);
 const int visible=6,row_h=24,top_y=66;
 if(selected<scroll_y)scroll_y=selected;
 if(selected>=scroll_y+visible)scroll_y=selected-visible+1;
 for(int row=0;row<visible;row++){
    int i=scroll_y+row;if(i>=BTS_SOUND_COUNT)break;
    float y=top_y+row*row_h;
    round_panel(7,y,306,row_h-1,4,i==selected?muted_leaf:panel);
    if(i==selected)rect(9,y+3,3,17,green);
    char title[92];shorten(title,sizeof(title),bts_audio_preview_names[i],35);
    text(18,y+3,.48f,i==selected?white:cream,title);
    text(291,y+4,.38f,i==selected?green:secondary,">");
 }
 if(sound_test_last>=0){
    snprintf(status,sizeof(status),"CUE %d: %s",sound_test_last+1,
      !sound_test_queued?bts_audio_error:(bts_audio_active?"PLAYING":"FINISHED"));
    text(10,205,.41f,sound_test_queued?green:orange,status);
 }
 snprintf(status,sizeof(status),"LAST CUE %d  DSP %lu  BLOCKS %u/%u",  bts_audio_last_cue>=0?bts_audio_last_cue+1:0,  (unsigned long)(bts_ndsp_active?ndspGetFrameCount():0),  bts_audio_chunks_completed,bts_audio_chunks_queued);
 text(10,215,.32f,secondary,status);
 text(10,226,.39f,bright,"A / TAP PLAY   Y STOP   X AUTO   B BACK");
}
static void bts_soundboard_play(int idx){
 if(idx<0||idx>=BTS_SOUND_COUNT)return;
 sound_test_last=idx;
 sound_test_queued=bts_audio_play(idx,true);
 if(!sound_test_queued)note("Sound not queued: check DSP and sample state");
}

/* Bottom touchscreen is the active configuration form. Even repeated
 * "Output" labels are unambiguous under the INI's named subdialog heading. */
static void draw_config_dialog_bottom(void){
 bool spatial=bts_spatial_layout();
 if(spatial){
    char title[84];shorten(title,sizeof(title),open_title,35);
    text(10,32,.56f,white,title);
    rect(9,54,302,1,outline);
    const int visible=6,top_y=58,row_h=25;
    if(selected<scroll_y)scroll_y=selected;
    if(selected>=scroll_y+visible)scroll_y=selected-visible+1;
    if(scroll_y<0)scroll_y=0;
    for(int row=0;row<visible;row++){
       int idx=scroll_y+row;if(idx>=result_count)break;
       const Resolved *r=&results[idx];
       int y=top_y+row*row_h;
       round_panel(8,y,304,23,4,idx==selected?muted_leaf:panel);
       if(idx==selected)rect(9,y+3,3,17,green);
       text(16,y+4,.30f,secondary,bts_region_label(bts_effective_region(idx)));
       char name[82];shorten(name,sizeof(name),r->label,22);
       text(68,y+4,.43f,idx==selected?white:cream,name);
       const char *action=r->type==3?
            (!strcmp(r->command,"cmd_test_cancel")?"ABORT":
             is_sensor_capture_command(r->command)?"CAPTURE":
             is_stim_action(r)?"TEST":"N/A"):
          r->type==8?"STATE":r->type==7?"GAUGE":
          r->type==4?"GRAPH":r->type==1||r->type==5?"PLOT":
          r->type==6?"INFO":"EDIT";
       round_panel(255,y+2,48,19,4,r->type==3?outline:panel2);
       text(262,y+5,.31f,r->type==3?orange:bright,action);
    }
    if(result_count>visible){
       float thumb=fmaxf(10.f,150.f*visible/(float)result_count);
       float offset=150.f-thumb;
       if(result_count>visible)offset*=scroll_y/(float)(result_count-visible);
       rect(315,58,3,150,outline);rect(315,58+offset,3,thumb,bright);
    }
    text(8,217,.34f,bright,"A OPEN   L/R PANELS   UP/DOWN CONTROLS   B BACK");
    return;
 }
 int group=config_group_of(selected);
 int at=config_group_start(group),until=config_group_end(at);
 if(at<0){text(11,100,.45f,orange,"No ECU controls in this panel");return;}
 char title[85];shorten(title,sizeof(title),config_group_label(at),35);
 text(11,32,.57f,white,title);
 char step[35];snprintf(step,sizeof(step),"%d/%d",group+1,config_group_count());
 text(280,34,.35f,secondary,step);
 rect(9,54,302,1,outline);
 const int visible=6,row_y=58,row_h=25;
 int local=selected-at,count=until-at;
 if(local<scroll_y)scroll_y=local;
 if(local>=scroll_y+visible)scroll_y=local-visible+1;
 if(scroll_y<0)scroll_y=0;
 for(int r=0;r<visible;r++){
    int i=at+scroll_y+r;if(i>=until)break;
    const Resolved *item=&results[i];
    bool focus=i==selected,button=item->type==3;
    int y=row_y+r*row_h;
    round_panel(8,y,304,23,4,focus?muted_leaf:panel);
    if(focus)rect(9,y+3,3,17,green);
    char name[80];shorten(name,sizeof(name),item->label,button?24:25);
    text(17,y+4,.46f,focus?white:cream,name);
    if(button){
       /* Actual button styling; explicit disabled state instead of an
        * unusable test button pretending it can actuate the ECU. */
       bool abort=!strcmp(item->command,"cmd_test_cancel");
       bool capture=is_sensor_capture_command(item->command);
       bool capture_ready=capture&&net_ok&&ecu_session&&ecu_signature[0]&&
                            menu_loaded&&!strcmp(ini_signature,ecu_signature);
       bool supported=abort || capture_ready ||
          (is_stim_action(item)&&
           (!strcmp(item->command,"cmd_disable_self_stim")?
            stim_stop_supported:stim_start_supported));
       round_panel(215,y+2,88,19,4,supported?muted_leaf:outline);
       text(222,y+5,.34f,supported?bright:orange,
            abort?"ABORT":capture?"CAPTURE":supported?"COMMAND":"NO BRIDGE");
    }else if(item->type==6){
       text(268,y+5,.32f,secondary,"INFO");
    }else if(item->type==7){
       double live=0.0;char display[27]="--";
       if(channel_live_value(item->field,&live))snprintf(display,sizeof(display),"%.1f",live);
       text(263,y+4,.43f,bright,display);
    }else {
       const char *kind=item->type==4?"GRAPH":item->type==1||item->type==5?"MAP":"EDIT";
       round_panel(255,y+2,48,19,4,panel2);
       text(261,y+5,.37f,bright,kind);
    }
 }
 if(count>visible){
    float thumb=fmaxf(11.f,150.f*visible/(float)count);
    float off=(float)scroll_y/(float)(count-visible)*(150.f-thumb);
    rect(314,58,3,150,outline);rect(314,58+off,3,thumb,bright);
 }
 text(8,217,.36f,bright,"A EDIT  L/R SECTIONS  Y SAVE  B BACK");
}
static void draw_list(void){
 if(workspace_page(ui_page)){draw_workspace_list();return;}
 if(ui_page==PAGE_TUNE_TREE){if(tune_screen_asleep)draw_tune_idle_gauges_bottom();else draw_tune_preview_bottom();return;}
 if(ui_page==PAGE_GROUP){draw_subfolder_help_bottom();return;}
 if(ui_page==PAGE_DETAIL){draw_config_dialog_bottom();return;}
 if(tune_context_page()){draw_tune_navigation();return;}
 if(ui_page==PAGE_CATS||ui_page==PAGE_VIEWS){draw_home_menu();return;}
 const char*title="ECU MENU";
 if(ui_page==PAGE_MENU){int k=cat_start(category);if(k>=0)title=menus[k].label;}
 else if(ui_page==PAGE_GROUP){if(group_item>=0)title=menus[group_item].label;}
 else if(ui_page==PAGE_CHOICES||ui_page==PAGE_DETAIL)title=open_title;
 else if(ui_page==PAGE_MAPS)title="ALL ECU MAPS";
 else if(ui_page==PAGE_FAVORITES)title="QUICK ACCESS";
 else if(ui_page==PAGE_SEARCH)title="FIND ECU SETTING";
 else if(ui_page==PAGE_VIEWS)title="VIEWS & LIVE INSTRUMENTS";
 else if(ui_page==PAGE_LOG_BROWSER)title="SAVED DATA LOGS";
 else if(ui_page==PAGE_COMPARE_PICK)title="CHOOSE REFERENCE LOG";
 else if(ui_page==PAGE_TRACE_MAPS)title="LOG TRACE / CHOOSE MAP";
 else if(ui_page==PAGE_CHANNELS)title="LIVE ECU CHANNELS";
 char compact[80];shorten(compact,sizeof(compact),title,35);
 text(11,34,.53f,white,compact);
 if(ui_page==PAGE_SEARCH){char find_title[75];snprintf(find_title,sizeof(find_title),"Find: %.32s",search_query);
    text(11,34,.49f,cream,find_title);}
 if(ui_page==PAGE_DETAIL&&!strcasecmp(open_title,"ECU stimulator")){
    rect(8,204,304,3,stim_preview?green:outline);
 }
 if(!menu_loaded && ui_page!=PAGE_VIEWS&&ui_page!=PAGE_LOG_BROWSER&&ui_page!=PAGE_COMPARE_PICK){draw_category_icon(ICON_COMMS,265,90);text(12,80,.50f,orange,"WAITING FOR ECU DEFINITION");
   text(12,103,.36f,secondary,"Auto-fetch / SD cache / Y retry");
   draw_note("B BACK  L/R GAUGES  SELECT DISPLAY");return;}
 int total=active_count();if(total<=0){text(12,91,.41f,orange,ui_page==PAGE_LOG_BROWSER?"NO SAVED LOGS YET":"NO CONTROLS IN THIS VIEW");
   draw_note(ui_page==PAGE_LOG_BROWSER?"RECORD + EXPORT CSV FIRST":"B BACK  X MAPS");return;}
 char count[32];snprintf(count,sizeof(count),"%d/%d",selected+1,total);
 text(269,37,.39f,secondary,count);
 const bool dense=(ui_page==PAGE_MENU||ui_page==PAGE_GROUP||ui_page==PAGE_DETAIL||ui_page==PAGE_MAPS);
 const int visible=dense?6:LIST_ROWS,rowheight=dense?25:LIST_ROW_H,starty=dense?50:LIST_ROW_Y;
 if(selected<scroll_y)scroll_y=selected;
 if(selected>=scroll_y+visible)scroll_y=selected-visible+1;
 for(int r=0;r<visible;r++){
   int i=scroll_y+r;if(i>=total)break;
   const char*label="";bool header=false;char tag[20]="";
   if(ui_page==PAGE_CATS){if(i==cat_total()+2){label="Views & Instruments";strcpy(tag,"VIEW");}
      else if(i==cat_total()+1){label="Find Setting Or Map";strcpy(tag,"FIND");}
      else if(i==cat_total()){label="Quick Access (Saved)";strcpy(tag,"FAV");}
      else {int idx=cat_start(i);if(idx>=0){label=menus[idx].label;snprintf(tag,sizeof(tag),"%d",i+1);}}}
   else if(ui_page==PAGE_VIEWS){
      static const char *names[]={"Live Dashboard","Live Graphs","Knock Monitor","SD Data Logger","Replay Saved Logs","All Live ECU Channels","Gauge Styles & Channels","ECU Tune Tree","Find ECU Setting","Wi-Fi & ECU Diagnostics","UI Sound Effects","Trigger Waveform (Offline)"};
      label=names[i];strcpy(tag,"OPEN");
      if(i==10){copy_text(tag,sizeof(tag),sfx_enabled?"ON":"OFF");}
      if(i==11)strcpy(tag,"SIM");
   }
   else if(ui_page==PAGE_LOG_BROWSER||ui_page==PAGE_COMPARE_PICK){
     label=log_filenames[i];strcpy(tag,!strcmp(replay_filename,label)?"BASE":"CSV");
   }
   else if(ui_page==PAGE_TRACE_MAPS){int id=trace_maps[i];label=*tables[id].label?tables[id].label:tables[id].id;strcpy(tag,"TRACE");}
   else if(ui_page==PAGE_CHANNELS){
      int idx=selectable_channels[i];label=channels[idx].name;strcpy(tag,"LIVE");
   }
   else if(ui_page==PAGE_MENU){int idx=menu_index(i);if(idx>=0&&idx<menu_count){MenuItem*m=&menus[idx];label=m->label;header=m->group==1;snprintf(tag,sizeof(tag),"%s",header?"GROUP":"OPEN");}}
   else if(ui_page==PAGE_GROUP){int idx=group_child_index(i);if(idx>=0){label=menus[idx].label;strcpy(tag,"OPEN");}}
   else if(ui_page==PAGE_CHOICES){label=choices[i].label;snprintf(tag,sizeof(tag),"%d",choices[i].value);}
   else if(ui_page==PAGE_MAPS){label=*tables[i].label?tables[i].label:tables[i].id;strcpy(tag,tables[i].curve?"CURVE":"MAP");}
   else if(ui_page==PAGE_FAVORITES){label=favorites[i].label;strcpy(tag,"SAVED");}
   else if(ui_page==PAGE_SEARCH){SearchHit *h=&search_hits[i];
      label=h->type==0?menus[h->index].label:(*tables[h->index].label?tables[h->index].label:tables[h->index].id);
      strcpy(tag,h->type==0?"INI":"MAP");}
   else if(ui_page==PAGE_DETAIL){label=results[i].label;
      if(results[i].type==3){
        if(!strcmp(results[i].command,"cmd_disable_self_stim"))
           strcpy(tag,stim_stop_supported?"STOP":"UPDATE");
        else if(!strcmp(results[i].command,"cmd_enable_self_stim"))
           strcpy(tag,stim_start_supported?"START":"UPDATE");
        else strcpy(tag,"UNAVAILABLE");
      }
      else if(results[i].type==4)strcpy(tag,"GRAPH");
      else if(results[i].type==6)strcpy(tag,"INFO");
      else if(results[i].type==7)strcpy(tag,"LIVE");
      else strcpy(tag,cal[results[i].field].kind==K_ARRAY?"TABLE":"FIELD");}
   float y=starty+r*rowheight;
   if(dense){
      round_panel(8,y,304,rowheight-2,4,i==selected?muted_leaf:panel);
      if(i==selected)rect(8,y+2,3,rowheight-6,green);
   }else select_row(y,i==selected);

   char pretty[90],name[84];pretty_label(pretty,sizeof(pretty),label);
   shorten(name,sizeof(name),pretty,27);
   if(!dense)draw_category_icon(category_icon(pretty),17,y+3);
   shorten(name,sizeof(name),pretty,dense?39:23);
   text(dense?16:54,y+(dense?3:5),dense?.43f:.54f,i==selected?white:cream,name);
   if(ui_page==PAGE_CHOICES)text(278,y+8,.38f,secondary,tag);
   if(ui_page==PAGE_MAPS&&favorite_has(tables[i].id))text(289,y+3,.43f,bright,"*");
   if(ui_page==PAGE_MENU){int ix=menu_index(i);if(ix>=0&&favorite_has(menus[ix].target))text(291,y+4,.40f,bright,"*");}
   if(ui_page==PAGE_GROUP){int ix=group_child_index(i);if(ix>=0&&favorite_has(menus[ix].target))text(291,y+4,.40f,bright,"*");}
 }
 if(total>visible){float track=visible*rowheight;float thumb=fmaxf(13.0f,track*(float)visible/(float)total);
    float pos=(float)scroll_y/(float)(total-visible)*(track-thumb);rect(313,starty,5,track,outline);rect(313,starty+pos,5,thumb,bright);}
 draw_note(ui_page==PAGE_CHOICES?"A STAGE   X UNDO   Y REDO   B BACK":
   ui_page==PAGE_DETAIL&&!strcasecmp(open_title,"ECU stimulator")?
     "ECU STIM CONTROL / BENCH SAFETY":
     ui_page==PAGE_VIEWS?"A OPEN  B BACK  SELECT BACK":
     ui_page==PAGE_TRACE_MAPS?"A TRACE MAP  B RETURN TO LOG":
     ui_page==PAGE_LOG_BROWSER?"A OPEN  Y DELETE  X REFRESH  B BACK":
     ui_page==PAGE_DETAIL?"A OPEN  Y STAR  B BACK":
      "A OPEN  B BACK  C-STICK FAST SCROLL");
}
static void map_touch_button(float x,float y,float w,const char*label,bool active);
static void draw_display(void){
 text(8,31,.52f,white,"Customize digital instruments");
 
 const int visible=LIST_ROWS,rowheight=LIST_ROW_H,starty=LIST_ROW_Y,total=10;
 if(selected<scroll_y)scroll_y=selected;
 if(selected>=scroll_y+visible)scroll_y=selected-visible+1;
 for(int r=0;r<visible;r++){
    int i=scroll_y+r;if(i>=total)break;
    float y=starty+r*rowheight;select_row(y,i==selected);
    char label[95],pretty[95];
    if(i==0)snprintf(label,sizeof(label),"Style: %s",gauge_styles[gauge_style]);
    else if(i<=9){
       pretty_label(pretty,sizeof(pretty),gauge_slot_label(i-1));
       char mode[20];if(gauge_decimals[i-1]<0)snprintf(mode,sizeof(mode),"AUTO");       else snprintf(mode,sizeof(mode),"%d DP",gauge_decimals[i-1]);
       snprintf(label,sizeof(label),"G%d %.22s [%s]",i,pretty,mode);
    }

    if(strlen(label)>34)label[34]=0;
    text(18,y+5,.53f,cream,label);
 }
 draw_note("A CHANNEL  X/Y DECIMALS  B BACK");
}
static void draw_channel_picker(void){
 text(8,27,.50f,cream,"Choose Real ECU Channel");
 
 if(selected<scroll_y)scroll_y=selected;
 if(selected>=scroll_y+LIST_ROWS)scroll_y=selected-LIST_ROWS+1;
 for(int i=0;i<LIST_ROWS;i++){
    int n=scroll_y+i;if(n>=selectable_count)break;
    Field*f=&channels[selectable_channels[n]];char pretty[86],label[86];
    pretty_label(pretty,sizeof(pretty),f->name);shorten(label,sizeof(label),pretty,31);
    float y=LIST_ROW_Y+i*LIST_ROW_H;select_row(y,n==selected);
    text(18,y+5,.53f,cream,label);
 }
 draw_note("A ASSIGN TO GAUGE  B BACK");
}
static void draw_graph_deck(void){
 text(8,32,.56f,cream,"Live Signal History");
 
 char time[90];snprintf(time,sizeof(time),"Window %d sec    RX %.1f Hz",graph_seconds,actual_hz);
 text(8,78,.48f,secondary,time);
 char grp[62];snprintf(grp,sizeof(grp),"Viewing gauges %d-%d of 9",graph_bank*3+1,graph_bank*3+3);
 text(8,104,.45f,cream,grp);
 text(8,132,.40f,secondary,"Configure all 9 in Gauge Settings");
 map_touch_button(8,165,146,"Prev Gauges",false);
 map_touch_button(166,165,146,"Next Gauges",false);
 draw_note("L/R GAUGES   X TIME   B BACK");
}
static void draw_recorder(void){
 text(10,30,.54f,cream,"TELEMETRY RECORDER");
 char line[100];
 snprintf(line,sizeof(line),"%s    %d / %d samples",log_recording?"Recording":"Stopped",log_count,LOG_MAX_SAMPLES);
 text(10,59,.45f,log_recording?bright:white,line);
 u32 elapsed=(u32)(log_recording?osGetTime()-log_begin_ms:
                        (log_count?log_buffer[log_count-1].t_ms:0));
 snprintf(line,sizeof(line),"Elapsed %02lu:%02lu  |  ECU %.1f Hz",
    (unsigned long)(elapsed/60000),(unsigned long)((elapsed/1000)%60),actual_hz);
 text(10,82,.43f,secondary,line);
 text(10,102,.40f,secondary,"Only actual valid ECU samples are stored");
 map_touch_button(8,129,145,log_recording?"STOP":"START",log_recording);
 map_touch_button(166,129,145,"EXPORT CSV",false);
 text(11,174,.42f,secondary,"A START/STOP   X EXPORT   Y CLEAR");
 text(11,192,.40f,secondary,"SD: /BrickECU/logs/");
 draw_note("B DISPLAY  |  RECORDS LATEST REAL DATA");
}
static void draw_diag(void){
 text(10,33,.54f,white,"WIRELESS / ECU STATUS");
 char line[135];
 snprintf(line,sizeof(line),"Wi-Fi: %.21s",wifi_ssid);
 text(11,54,.43f,wifi_candidate?green:orange,line);
 snprintf(line,sizeof(line),"Signal %lu  |  AC %s",(unsigned long)wifi_status,ac_ok?"OK":"ERROR");
 text(11,72,.43f,secondary,line);
 snprintf(line,sizeof(line),"Bridge: %s",HOST_IP);
 text(11,90,.43f,white,line);
 snprintf(line,sizeof(line),"ECU %s  |  INI %s",ecu_session?"ONLINE":"OFFLINE",menu_loaded?"READY":"MISSING");
 text(11,108,.43f,ecu_session?green:orange,line);
 snprintf(line,sizeof(line),"DS v3.4 | ESP %.12s",esp_firmware_build);
 text(11,126,.43f,white,line);
 snprintf(line,sizeof(line),"Write %s | Backup %s",esp_write_access_ready?"READY":"OFF",esp_backup_ready?"READY":"--");
 text(11,144,.43f,secondary,line);
 char compact[66];shorten(compact,sizeof(compact),net_status,34);
 text(11,162,.42f,secondary,compact);
 snprintf(line,sizeof(line),"RX %lu  |  Err %lu  |  HTTP %d",(unsigned long)successful_live,(unsigned long)live_errors,live_http_code);
 text(11,181,.42f,live_valid&&!live_stale?green:orange,line);
 draw_note("A STATUS  Y INI  X WIFI  L/R SLOT");
}
/* Live cursor is derived exclusively from the INI's xBins/yBins
 * output-channel names and the actual ECU live block. No guessed offsets. */
static double axis_value(int axis,int index);
static bool runtime_value(const char *name,double *value){
 if(!name||!*name||!live_valid||live_stale)return false;
 Field *f=lookup((char*)name,true);
 if(!f||!f->validScale||f->offset+dsize(f->type)>live_bytes)return false;
 double raw=get_number(live_data,live_bytes,f->offset,f->type);
 if(!isfinite(raw))return false;
 *value=raw*f->scale+f->translate;
 return isfinite(*value);
}
/* UI audio uses only fresh decoded INI output channels; never cached gauges,
 * guessed offsets or stale data. MAP level cues re-arm below 98 kPa;
 * coolant cues repeat every 5 seconds while fresh temperature exceeds 105 C. */
static bool bts_sound_runtime(const char *key,double *value,const char **unit){
 Field *f=lookup((char*)key,true);
 if(!f||!f->validScale||!runtime_value(key,value))return false;
 *unit=f->unit;return true;
}
static void bts_sound_telemetry(u64 now){
 if(!ecu_session||!live_valid||!live_received_at||now-live_received_at>900)return;
 double v=0;const char *unit="";
 if(bts_sound_runtime("MAPValue",&v,&unit)){
   if(bts_contains_ci(unit,"psi"))v*=6.894757293;
   else if(!bts_contains_ci(unit,"kpa"))v=NAN;
   if(isfinite(v)&&bts_level_cross(&bts_boost_level,v,100.0,98.0))
       bts_audio_play(BTS_SOUND_PRESSURE,event_audio_enabled);
 }
 if(bts_sound_runtime("coolant",&v,&unit)){
   if(bts_contains_ci(unit,"fahrenheit")||!strcasecmp(unit,"F")||bts_contains_ci(unit,"degF"))
      v=(v-32.0)*5.0/9.0;
   else if(!bts_contains_ci(unit,"celsius")&&!bts_contains_ci(unit,"degC")&&
           !bts_contains_ci(unit,"°C")&&strcasecmp(unit,"C"))v=NAN;
   if(isfinite(v)&&bts_level_repeat(&bts_coolant_level,v,105.0,now,5000))
       bts_audio_play(BTS_SOUND_COOLANT_HOT,event_audio_enabled);
 }
}
static bool axis_location(int axis,int n,double target,float *position,int *nearest){
 if(axis<0||axis>1||n<1||!axis_ready[axis])return false;
 double best=INFINITY;int bestid=-1;
 for(int i=0;i<n;i++){
   double val=axis_value(axis,i);
   if(!isfinite(val))continue;
   double err=fabs(val-target);
   if(err<best){best=err;bestid=i;}
 }
 if(bestid<0)return false;
 *nearest=bestid;*position=(float)bestid;
 for(int i=0;i<n-1;i++){
   double a=axis_value(axis,i),b=axis_value(axis,i+1);
   if(!isfinite(a)||!isfinite(b)||fabs(a-b)<1e-6)continue;
   if((target>=a&&target<=b)||(target>=b&&target<=a)){
       *position=(float)(i+(target-a)/(b-a));return true;
   }
 }
 return true;
}
static void trace_coverage_tick(Field *f){
 if(!replay_trace_active||!trace_show_coverage||trace_coverage_done||!f)return;
 int w=f->width,h=f->height;
 if(w<1||h<1||w*h>EDIT_CAP||!axis_ready[0]||(h>1&&!axis_ready[1]))return;
 /* Bound per-frame work when an INI defines very large axis arrays. */
 int budget=128;
 if(w+h>256)budget=8;
 else if(w+h>128)budget=32;
 else if(w+h>64)budget=64;
 int until=trace_coverage_at+budget;
 if(until>replay_count)until=replay_count;
 for(int r=trace_coverage_at;r<until;r++){
   LogSample *s=&replay_buffer[r];
   if(trace_log_x<0||trace_log_x>=LOG_FIELDS||!(s->valid&(1u<<trace_log_x)))continue;
   if(h>1&&(trace_log_y<0||trace_log_y>=LOG_FIELDS||!(s->valid&(1u<<trace_log_y))))continue;
   double x=s->values[trace_log_x],y=h>1?s->values[trace_log_y]:0;
   if(!isfinite(x)||!isfinite(y))continue;
   float px=0,py=0;int ix=0,iy=0;
   if(!axis_location(0,w,x,&px,&ix))continue;
   if(h>1&&!axis_location(1,h,y,&py,&iy))continue;
   int id=iy*w+ix;
   if(id<0||id>=EDIT_CAP)continue;
   if(trace_coverage[id]<UINT16_MAX)trace_coverage[id]++;
   if(trace_coverage[id]>trace_coverage_peak)trace_coverage_peak=trace_coverage[id];
   trace_valid_samples++;
 }
 trace_coverage_at=until;
 if(trace_coverage_at>=replay_count)trace_coverage_done=true;
}
static void update_map_trace(Field *f){
 trace_found=false;tracex=tracey=-1;
 if(current_table<0||!f)return;
 TableDef *t=&tables[current_table];
 trace_coverage_tick(f);
 double x=0,y=0;
 if(replay_trace_active){
   if(replay_position<0||replay_position>=replay_count||trace_log_x<0||trace_log_x>=LOG_FIELDS)return;
   LogSample*r=&replay_buffer[replay_position];
   /* A missing logged sample must remain missing, never substitute live data. */
   if(!(r->valid&(1u<<trace_log_x)))return;
   x=r->values[trace_log_x];
   if(f->height>1){
     if(trace_log_y<0||trace_log_y>=LOG_FIELDS||!(r->valid&(1u<<trace_log_y)))return;
     y=r->values[trace_log_y];
   }
 }else{
   if(!live_valid||live_stale||!runtime_value(t->xchan,&x))return;
   if(f->height>1&&!runtime_value(t->ychan,&y))return;
 }
 if(!axis_location(0,f->width,x,&trace_interp_x,&tracex))return;
 if(f->height>1){
   if(!axis_location(1,f->height,y,&trace_interp_y,&tracey))return;
 }else{tracey=0;trace_interp_y=0;}
 trace_raw_x=x;trace_raw_y=y;trace_found=true;
 if(replay_trace_active&&!replay_trace_centered){
   cell=tracey*f->width+tracex;
   replay_trace_centered=true;
 }
}
/* Cached numeric labels; the viewport shows at most 6x5 legible cells. */
static void rebuild_map_labels(Field *f){
 map_text_dirty=false;map_text_count=0;
 if(!map_textbuf||!f||!editor_loaded)return;
 C2D_TextBufClear(map_textbuf);
 int n=f->width*f->height;
 if(n>2048)n=2048;
 for(int i=0;i<n;i++){
   char label[40];double val=field_value(editor,editor_length,f,i);
   if(!isfinite(val))strcpy(label,"--");
   else if(fabs(val)<10000.0 && fabs(val-round(val))<0.049)
      snprintf(label,sizeof(label),"%.0f",val);
   else if(fabs(val)<999.95)
      snprintf(label,sizeof(label),"%.1f",val);
   else snprintf(label,sizeof(label),"%.0f",val);
   map_chars[i]=(uint8_t)strlen(label);
   C2D_TextParse(&map_texts[i],map_textbuf,label);
   C2D_TextOptimize(&map_texts[i]);
   map_text_count++;
  }
 for(int a=0;a<2;a++){
    axis_text_count[a]=0;
    if(!axis_ready[a]||axis_field[a]<0)continue;
    int n=cal[axis_field[a]].width*cal[axis_field[a]].height;
    if(n>256)n=256;
    for(int i=0;i<n;i++){
      char label[40];double v=axis_value(a,i);
      if(!isfinite(v))strcpy(label,"--");
      else if(fabs(v)<10000&&fabs(v-round(v))<0.049)snprintf(label,sizeof(label),"%.0f",v);
      else if(fabs(v)<1000)snprintf(label,sizeof(label),"%.1f",v);
      else snprintf(label,sizeof(label),"%.0f",v);
      axis_chars[a][i]=(uint8_t)strlen(label);
      C2D_TextParse(&axis_texts[a][i],map_textbuf,label);
      C2D_TextOptimize(&axis_texts[a][i]);axis_text_count[a]++;
    }
  }
}
static u32 heat_color(double value,double lo,double hi){
 if(!isfinite(value))return soil;
 float t=hi>lo?(float)((value-lo)/(hi-lo)):0.5f;
 if(t<0)t=0;if(t>1)t=1;
 /* Every background remains dark enough for the SAME white number color. */
 const int r[5]={19,25,26,99,151};
 const int g[5]={57,92,100,86,47};
 const int b[5]={100,114,85,47,62};
 float p=t*4;int i=(int)p;if(i>3)i=3;float a=p-i;
 return C2D_Color32((u8)(r[i]+(r[i+1]-r[i])*a),
                    (u8)(g[i]+(g[i+1]-g[i])*a),
                    (u8)(b[i]+(b[i+1]-b[i])*a),255);
}
static double axis_value(int axis,int index){
 if(axis<0||axis>1||!axis_ready[axis]||axis_field[axis]<0)return NAN;
 Field*f=&cal[axis_field[axis]];
 /* Unknown conditional axis modes must never be displayed as raw values. */
 return field_value(axis_data[axis],axis_len[axis],f,index);
}
/* Show fewer map cells at once so actual 3DS numerals stay legible.
 * Both axes scroll as the cursor approaches the visible edge. */
static void map_geometry(Field *f,int *cols,int *rows,int *base_col,int *base_row,
                         float *x,float *y,float *cw,float *ch){
 const int maxc=6,maxr=5;
 *cols=f->width<maxc?f->width:maxc;
 *rows=f->height<maxr?f->height:maxr;
 int rawx=map_focus==1?axis_cursor[0]:cell%f->width;
 int rawy=map_focus==2?axis_cursor[1]:cell/f->width;
 int focusx=to_view(0,rawx,f->width),focusy=to_view(1,rawy,f->height);
 int maxx=f->width-*cols,maxy=f->height-*rows;
 if(maxx<0)maxx=0;if(maxy<0)maxy=0;
 if(focusx<grid_first_col+2||focusx>=grid_first_col+*cols-2){
    grid_first_col=focusx-*cols/2;
 }
 if(focusy<grid_first_row+2||focusy>=grid_first_row+*rows-2){
    grid_first_row=focusy-*rows/2;
 }
 if(grid_first_col<0)grid_first_col=0;if(grid_first_col>maxx)grid_first_col=maxx;
 if(grid_first_row<0)grid_first_row=0;if(grid_first_row>maxy)grid_first_row=maxy;
 *base_col=grid_first_col;*base_row=grid_first_row;
 /* On short maps, grow the cells rather than shrink labels to nothing. */
 *cw=fminf(350.0f/(float)*cols,90.0f);
 *ch=fminf(184.0f/(float)*rows,46.0f);
 *x=47.0f+(350.0f-*cw*(*cols))*.5f;
 *y=11.0f+(184.0f-*ch*(*rows))*.5f;
}
static void stroke_cell(float x,float y,float w,float h,u32 c){
 rect(x,y,w,1.5f,c);rect(x,y+h-1.5f,w,1.5f,c);
 rect(x,y,1.5f,h,c);rect(x+w-1.5f,y,1.5f,h,c);
}
static void render_axis_label(int axis,int index,float x,float y,float w,float h,float scale){
 if(index<0||index>=axis_text_count[axis])return;
 float tw=0,th=0;
 C2D_TextGetDimensions(&axis_texts[axis][index],scale,scale,&tw,&th);
 C2D_DrawText(&axis_texts[axis][index],C2D_WithColor,x+(w-tw)*.5f,y+(h-th)*.5f,0,scale,scale,cream);
}
/* Real ECU-calibration perspective surface: each X/Y cell is a distinct heat-
 * coloured facet, with a visible elevation and depth-aware stereo.
 * Unlike the previous build, height does NOT include distant INI safety limits.
 * That compressed real variations into an apparently flat sheet. */
#define MAP3D_MAX 20
#define MAP3D_TRI_CAP ((MAP3D_MAX-1)*(MAP3D_MAX-1)*2)
typedef struct {float x,y,d,ht;u32 color;bool ok;} Map3dPoint;
typedef struct {int a,b,c;float depth;u32 color;} Map3dTri;
static Map3dPoint map3d_nodes[MAP3D_MAX*MAP3D_MAX];
static Map3dTri map3d_faces[MAP3D_TRI_CAP];
static int map3d_face_compare(const void *aa,const void *bb){
 const Map3dTri *a=(const Map3dTri*)aa,*b=(const Map3dTri*)bb;
 return a->depth>b->depth?-1:a->depth<b->depth?1:0;
}
static float map3d_normalize(double value,double lo,double hi){
 if(!isfinite(value)||!isfinite(lo)||!isfinite(hi)||hi-lo<1e-10)return 0.5f;
 float t=(float)((value-lo)/(hi-lo));return t<0?0:t>1?1:t;
}
static u32 map3d_heat(float t,float light){
 /* Five deliberately separated bins: colour belongs to the cell, NOT the
  * corner vertices, so each region remains identifiable during rotation. */
 static const int palette[5][3]={{49,113,210},{41,184,198},{73,207,114},
                                 {244,195,64},{240,83,65}};
 int bin=(int)floorf(t*5.f);if(bin>4)bin=4;if(bin<0)bin=0;
 int r=(int)(palette[bin][0]*light),g=(int)(palette[bin][1]*light),
     b=(int)(palette[bin][2]*light);
 if(r>255)r=255;if(g>255)g=255;if(b>255)b=255;
 return C2D_Color32(r,g,b,255);
}
static void map3d_project(Map3dPoint *p,float vx,float vy,float ht,
                          float ca,float sa,float cp,float sp,float disparity){
 /* Camera orbit about the CENTRE of the surface (not its zero-height floor).
  * Use one pixel/world-unit scale for both map axes: the old 132 vs 56
  * projection compressed the grid at overhead and underside angles.
  * At exact +/-90 degrees the XY table should be a normal, undistorted
  * overhead/underside view. Fade perspective there to avoid height-dependent
  * stretching of the grid and preserve stable top/bottom silhouettes. */
 float rx=vx*ca-vy*sa,depth=vx*sa+vy*ca;
 float altitude=(ht-.5f)*1.30f;
 float viewdepth=depth*cp+altitude*sp;
 float screenup=depth*sp-altitude*cp;
 float polar=sp*sp;
 float perspective_strength=.24f*(1.f-polar);
 float persp=3.50f/(3.50f+viewdepth*perspective_strength);
 const float pixels_per_unit=90.f;
 p->x=197.0f+rx*pixels_per_unit*persp+viewdepth*disparity;
 p->y=108.0f+screenup*pixels_per_unit*persp;
 p->d=viewdepth;p->ht=ht;p->ok=true;
}
static void draw_map_3d(Field *f){
 draw_depth_grid(400,240);
 int w=f->width,h=f->height;
 if(w<2||h<2){text(18,99,.53f,orange,"3D needs an X/Y table");return;}
 if(f->bytes!=w*h*dsize(f->type)||editor_length!=f->bytes){
    text(12,102,.49f,orange,"ECU data length differs from INI definition");return;
 }
 int nx=w<MAP3D_MAX?w:MAP3D_MAX,ny=h<MAP3D_MAX?h:MAP3D_MAX;
 double lo=INFINITY,hi=-INFINITY;
 /* Use the ACTUAL verified calibration span, never the broad ECU edit limits. */
 for(int i=0;i<w*h;i++){
    double v=field_value(original,editor_length,f,i);
    if(isfinite(v)){if(v<lo)lo=v;if(v>hi)hi=v;}
 }
 if(!isfinite(lo)||!isfinite(hi)){text(14,98,.53f,orange,"ECU map contains no valid values");return;}
 if(fabs(hi-lo)<1e-10){
    char explanation[100];snprintf(explanation,sizeof(explanation),"All %d ECU cells read %.3f",w*h,lo);
    text(18,89,.62f,orange,"FLAT ECU CALIBRATION");
    text(18,123,.46f,white,explanation);
    text(18,150,.40f,secondary,"No height differences to display");
    return;
 }
 /* Stable range comes from the original read, not one newly changed cell. */
 float ca=cosf(map_3d_yaw),sa=sinf(map_3d_yaw),cp=cosf(map_3d_pitch),sp=sinf(map_3d_pitch);
 /* Geometry must obey the SAME near/far eye convention as menu cards.
  * The old 11.5px/eye factor (and reversed near sign) made the surface hard
  * to fuse at full slider; depth now varies by a comfortable 3.2px/eye. */
 float disparity=bts_front(3.2f);
 for(int iy=0;iy<ny;iy++)for(int ix=0;ix<nx;ix++){
    int col=ix*(w-1)/(nx-1),row=iy*(h-1)/(ny-1);
    double value=field_value(editor,editor_length,f,row*w+col);
    Map3dPoint *p=&map3d_nodes[iy*nx+ix];p->ok=isfinite(value);
    if(!p->ok)continue;
    float ht=map3d_normalize(value,lo,hi);
    float vx=2.f*(float)col/(float)(w-1)-1.f;
    float vy=2.f*(float)row/(float)(h-1)-1.f;
    map3d_project(p,vx,vy,ht,ca,sa,cp,sp,disparity);
 }
 /* Fit the whole rotated surface into the available drawing area with a
  * UNIFORM scale. This avoids edge clipping and never squishes one axis.
  * Calculate bounds without stereo shift so BOTH eyes share identical
  * framing; otherwise per-eye auto-fitting would cancel real parallax. */
 float minx=INFINITY,maxx=-INFINITY,miny=INFINITY,maxy=-INFINITY;
 for(int i=0;i<nx*ny;i++){
    Map3dPoint *p=&map3d_nodes[i];if(!p->ok)continue;
    float neutral_x=p->x-p->d*disparity;
    if(neutral_x<minx)minx=neutral_x;
    if(neutral_x>maxx)maxx=neutral_x;
    if(p->y<miny)miny=p->y;
    if(p->y>maxy)maxy=p->y;
 }
 if(isfinite(minx)&&isfinite(miny)){
    const float width=365.f,height=183.f;
    float spanx=fmaxf(1.f,maxx-minx),spany=fmaxf(1.f,maxy-miny);
    float fit=fminf(1.f,fminf(width/spanx,height/spany));
    float cx=(minx+maxx)*.5f,cy=(miny+maxy)*.5f;
    for(int i=0;i<nx*ny;i++){
       Map3dPoint *p=&map3d_nodes[i];if(!p->ok)continue;
       p->x=197.f+(p->x-p->d*disparity-cx)*fit+p->d*disparity;
       p->y=109.f+(p->y-cy)*fit;
    }
 }
 /* The map is a SINGLE calibration surface. Never fabricate sidewalls down
  * to Z=0: those were the vertical streaks and malformed underside. */
 int nf=0;
 for(int y=0;y<ny-1;y++)for(int x=0;x<nx-1;x++){
    int a=y*nx+x,b=a+1,c=a+nx,d=c+1;
    Map3dPoint *pa=&map3d_nodes[a],*pb=&map3d_nodes[b],*pc=&map3d_nodes[c],*pd=&map3d_nodes[d];
    if(!(pa->ok&&pb->ok&&pc->ok&&pd->ok))continue;
    float heat=(pa->ht+pb->ht+pc->ht+pd->ht)*.25f;
    /* Local slopes affect brightness, while the 5-colour palette stays discrete. */
    float slope=fabsf(pa->ht+pc->ht-pb->ht-pd->ht)*.45f;
    float light=.88f+.20f*fminf(1.f,slope);
    u32 tone=map3d_heat(heat,light);
    map3d_faces[nf++]=(Map3dTri){a,b,c,(pa->d+pb->d+pc->d)/3.f,tone};
    map3d_faces[nf++]=(Map3dTri){c,b,d,(pc->d+pb->d+pd->d)/3.f,tone};
 }
 /* Render only true height-field facets. No zero-height perimeter walls. */
 qsort(map3d_faces,(size_t)nf,sizeof(map3d_faces[0]),map3d_face_compare);
 for(int i=0;i<nf;i++){
    Map3dTri *tri=&map3d_faces[i];
    Map3dPoint *a=&map3d_nodes[tri->a],*b=&map3d_nodes[tri->b],*c=&map3d_nodes[tri->c];
    C2D_DrawTriangle(a->x,a->y,tri->color,b->x,b->y,tri->color,c->x,c->y,tri->color,0);
 }
 /* Thin readable grid outlines: one visible boundary per real map cell. */
 for(int y=0;y<ny;y++)for(int x=0;x<nx;x++){
    Map3dPoint *a=&map3d_nodes[y*nx+x];if(!a->ok)continue;
    u32 line=C2D_Color32(14,28,45,245);
    if(x+1<nx){Map3dPoint *b=&map3d_nodes[y*nx+x+1];if(b->ok)
        C2D_DrawLine(a->x,a->y,line,b->x,b->y,line,.72f,0);}
    if(y+1<ny){Map3dPoint *b=&map3d_nodes[(y+1)*nx+x];if(b->ok)
        C2D_DrawLine(a->x,a->y,line,b->x,b->y,line,.72f,0);}
 }
 /* Highlight the REAL selected calibration cell, not a fabricated value.
  * Raised white outline and 3D parallax affect appearance only, never editor[]. */
 int col=cell%w,row=cell/w;
 int sx=(col*(nx-1)+(w-1)/2)/(w-1),sy=(row*(ny-1)+(h-1)/2)/(h-1);
 Map3dPoint *focus=&map3d_nodes[sy*nx+sx];
 if(focus->ok){
    float popx=bts_front(3.2f);
    /* A lifted cell has no vertical screen displacement when viewed directly
     * from above or below; depth is conveyed by stereo at those poles. */
    float raise=(16.0f+6.0f*stereo_strength)*fabsf(cp);
    u32 halo=C2D_Color32(100,238,245,225);
    u32 glow=C2D_Color32(255,255,255,245);
    int x0=sx>0?sx-1:sx,x1=sx+1<nx?sx+1:sx;
    int y0=sy>0?sy-1:sy,y1=sy+1<ny?sy+1:sy;
    /* A rectangular plate surrounds the hovered surface point and lifts
     * above it. Edges remain attached to genuine adjacent table values. */
    Map3dPoint *corners[4]={&map3d_nodes[y0*nx+x0],
                            &map3d_nodes[y0*nx+x1],
                            &map3d_nodes[y1*nx+x1],
                            &map3d_nodes[y1*nx+x0]};
    for(int i=0;i<4;i++){
       Map3dPoint *a=corners[i],*b=corners[(i+1)&3];
       if(!a->ok||!b->ok)continue;
       C2D_DrawLine(a->x+popx,a->y-raise,glow,
                    b->x+popx,b->y-raise,glow,2.1f,0);
       if(i==0||i==2)C2D_DrawLine(a->x,a->y,halo,
                       a->x+popx,a->y-raise,halo,1.4f,0);
    }
    C2D_DrawCircleSolid(focus->x+popx,focus->y-raise,0,8,soil);
    C2D_DrawCircleSolid(focus->x+popx,focus->y-raise,0,6,halo);
    C2D_DrawCircleSolid(focus->x+popx,focus->y-raise,0,3,glow);
 }
 /* Compact value strip and five-bin scale, not a giant map heading. */
 rect(0,207,400,33,soil);
 double val=field_value(editor,editor_length,f,cell);
 char line[125];
 snprintf(line,sizeof(line),"X%d/%d Y%d/%d  Z %.3f %s",col+1,w,row+1,h,val,f->unit);
 text(8,208,.42f,cream,line);
 snprintf(line,sizeof(line),"Z %.2f .. %.2f  |  HEIGHT: ACTUAL ECU SPREAD",lo,hi);
 text(8,224,.34f,bright,line);
 for(int i=0;i<5;i++)rect(308+i*16,25,15,5,map3d_heat((i+.5f)/5.f,1.f));
 text(314,32,.30f,secondary,"LOW TO HIGH");
}
static void draw_map_overview(Field *f){
 const int w=f->width,h=f->height;
 int cols,rows,base_col,base_row;float gx,gy,cw,ch;
 map_geometry(f,&cols,&rows,&base_col,&base_row,&gx,&gy,&cw,&ch);
 if(map_text_dirty)rebuild_map_labels(f);
 /* One font scale for ALL visible cells, and one per axis. No individual
  * numbers shrinking or growing as the cursor scrolls. */
 float cell_scale=.55f,axis_scale[2]={.46f,.46f};
 if(map_text_count){
   float maxwidth=0,maxheight=0;
   for(int row=0;row<rows;row++)for(int col=0;col<cols;col++){
     int xx=to_raw(0,base_col+col,w),yy=to_raw(1,base_row+row,h);
     int id=yy*w+xx;if(id<0||id>=map_text_count)continue;
     float tw=0,th=0;
     C2D_TextGetDimensions(&map_texts[id],.55f,.55f,&tw,&th);
     if(tw>maxwidth)maxwidth=tw;if(th>maxheight)maxheight=th;
   }
   float ratio=fminf((cw-3)/fmaxf(maxwidth,1),(ch-2)/fmaxf(maxheight,1));
   cell_scale*=fminf(1.0f,ratio);
 }
 for(int a=0;a<2;a++){
   float mxw=0,mxh=0;
   int count=a==0?cols:rows,base=a==0?base_col:base_row;
   int n=a==0?w:h;
   for(int i=0;i<count;i++){
     int raw=to_raw(a,base+i,n);
     if(raw<0||raw>=axis_text_count[a])continue;
     float tw=0,th=0;
     C2D_TextGetDimensions(&axis_texts[a][raw],.46f,.46f,&tw,&th);
     if(tw>mxw)mxw=tw;if(th>mxh)mxh=th;
   }
   float aw=a==0?cw:43.0f,ah=a==0?21.0f:ch;
   axis_scale[a]*=fminf(1.0f,fminf((aw-3)/fmaxf(mxw,1),(ah-2)/fmaxf(mxh,1)));
 }
 draw_depth_grid(400,240);
 if(current_table>=0){
   /* Visual columns/rows are independent of storage order. */
   rect(gx-44,gy+rows*ch+2,43,21,soil);
   text(gx-40,gy+rows*ch+5,.36f,secondary,"Y / X");
   for(int col=0;col<cols;col++){
      int visualx=base_col+col,ix=to_raw(0,visualx,w);
      float x=gx+col*cw,axisy=gy+rows*ch+2;
      rect(x+.3f,axisy,cw-.6f,21,axis_ready[0]?C2D_Color32(29,50,70,255):soil);
      render_axis_label(0,ix,x,axisy,cw,21,axis_scale[0]);
      if(map_focus==1&&ix==axis_cursor[0])stroke_cell(x,axisy,cw,21,bright);
   }
   for(int row=0;row<rows;row++){
      int visualy=base_row+row,iy=to_raw(1,visualy,h);
      float y=gy+row*ch;
      rect(gx-44,y+.3f,43,ch-.6f,axis_ready[1]?C2D_Color32(29,50,70,255):soil);
      render_axis_label(1,iy,gx-44,y,43,ch,axis_scale[1]);
      if(map_focus==2&&iy==axis_cursor[1])stroke_cell(gx-44,y,43,ch,bright);
   }
 }

 double mn=INFINITY,mx=-INFINITY;
 for(int i=0;i<w*h;i++){
   double v=field_value(editor,editor_length,f,i);
   if(isfinite(v)){if(v<mn)mn=v;if(v>mx)mx=v;}
 }
 if(!isfinite(mn)||!isfinite(mx)){
   text(25,100,.50f,berry,"Calibration data unavailable");return;}
 update_map_trace(f);
 for(int row=0;row<rows;row++)for(int col=0;col<cols;col++){
   int xx=to_raw(0,base_col+col,w),yy=to_raw(1,base_row+row,h),id=yy*w+xx;
   float x=gx+col*cw,y=gy+row*ch;
   double v=field_value(editor,editor_length,f,id);
   u32 tint=heat_color(v,mn,mx);
   if((map_diff_mode||snapshot_diff_mode) && editor_loaded){
      int bytes=dsize(f->type),off=id*bytes;
      const uint8_t *reference=snapshot_diff_mode&&snapshot_valid?snapshot_reference:original;
      bool diff=off>=0&&off+bytes<=editor_length&&memcmp(editor+off,reference+off,(size_t)bytes)!=0;
      tint=diff?C2D_Color32(50,77,117,255):panel;
   }
   if(replay_trace_active&&trace_show_coverage){
     if(trace_coverage_peak>0&&trace_coverage[id]>0){
        float strength=logf(1.0f+(float)trace_coverage[id])/logf(1.0f+(float)trace_coverage_peak);
        tint=heat_color(strength,0.0,1.0);
     }else tint=panel;
   }
   rect(x+.35f,y+.35f,cw-.7f,ch-.7f,tint);
   if(id<map_text_count){
      float scale=cell_scale;
      float ww=0,hh=0;C2D_TextGetDimensions(&map_texts[id],scale,scale,&ww,&hh);
      C2D_DrawText(&map_texts[id],C2D_WithColor,x+(cw-ww)*.5f,y+(ch-hh)*.5f,0,scale,scale,
          cream);
   }
   if(map_anchor_valid){
      BtsMapRegion range=map_selected_region();
      if(xx>=range.x0&&xx<=range.x1&&yy>=range.y0&&yy<=range.y1)
         stroke_cell(x+1.5f,y+1.5f,cw-3,ch-3,petal);
   }
   if((map_diff_mode||snapshot_diff_mode)&&editor_loaded){
      int off=id*dsize(f->type),bytes=dsize(f->type);
      const uint8_t *reference=snapshot_diff_mode&&snapshot_valid?snapshot_reference:original;
      if(off>=0&&off+bytes<=editor_length&&memcmp(editor+off,reference+off,(size_t)bytes))
         stroke_cell(x+3,y+3,cw-6,ch-6,orange);
   }
   /* The focused tile is drawn once more in the foreground below. */
 }
 /* The selected 2D calibration cell projects toward the viewer without
  * detaching its text, and is also legible with the 3D slider off. */
 if(map_focus==0&&cell>=0&&cell<w*h){
    int vx=to_view(0,cell%w,w)-base_col;
    int vy=to_view(1,cell/w,h)-base_row;
    if(vx>=0&&vx<cols&&vy>=0&&vy<rows){
       float sx=gx+vx*cw,sy=gy+vy*ch;
       float ox=bts_front(4.5f),oy=2.f+stereo_strength*1.5f;
       u32 tint=heat_color(field_value(editor,editor_length,f,cell),mn,mx);
       int bytes=dsize(f->type),off=cell*bytes;
       const uint8_t *reference=snapshot_diff_mode&&snapshot_valid?snapshot_reference:original;
       if((map_diff_mode||snapshot_diff_mode)&&editor_loaded)
          tint=off>=0&&off+bytes<=editor_length&&memcmp(editor+off,reference+off,(size_t)bytes)?C2D_Color32(50,77,117,255):panel;
       if(replay_trace_active&&trace_show_coverage){
          if(trace_coverage_peak>0&&trace_coverage[cell]>0){
             float strength=logf(1.f+(float)trace_coverage[cell])/logf(1.f+(float)trace_coverage_peak);
             tint=heat_color(strength,0.,1.);
          }else tint=panel;
       }
       round_panel(sx+1,sy+2,cw,ch,3,C2D_Color32(0,0,0,195));
       rect(sx+ox+.3f,sy-oy+.3f,cw-.6f,ch-.6f,tint);
       stroke_cell(sx+ox,sy-oy,cw,ch,bright);
       if(cell<map_text_count){
          float ww=0,hh=0;C2D_TextGetDimensions(&map_texts[cell],cell_scale,cell_scale,&ww,&hh);
          C2D_DrawText(&map_texts[cell],C2D_WithColor,sx+ox+(cw-ww)*.5f,
             sy-oy+(ch-hh)*.5f,0,cell_scale,cell_scale,white);
       }
    }
 }
 int view_tracex=to_view(0,tracex,w),view_tracey=to_view(1,tracey,h);
 if(trace_found&&view_tracex>=base_col&&view_tracex<base_col+cols&&view_tracey>=base_row&&view_tracey<base_row+rows){
   float tx=gx+(to_view_pos(0,trace_interp_x,w)-base_col+.5f)*cw;
   float ty=gy+(to_view_pos(1,trace_interp_y,h)-base_row+.5f)*ch;
   rect(tx-7,ty-2,14,4,soil);rect(tx-2,ty-7,4,14,soil);
   rect(tx-5,ty-1,10,2,cream);rect(tx-1,ty-5,2,10,cream);
 }
 rect(0,219,400,21,soil);
 char status[118];
 if(axis_field[1]>=0&&cal[axis_field[1]].dyn_axis_kind&&!cal[axis_field[1]].validScale)
   snprintf(status,sizeof(status),"Y AXIS: RAW UNTIL ECU UNIT SETTINGS LOAD");
 else if(replay_trace_active&&trace_show_coverage){
   if(trace_coverage_done)snprintf(status,sizeof(status),"LOG COVERAGE: %d VALID SAMPLES  X TO SWITCH",trace_valid_samples);
   else snprintf(status,sizeof(status),"ANALYZING LOG: %d / %d SAMPLES",trace_coverage_at,replay_count);
 }
 else if(trace_found&&replay_trace_active)snprintf(status,sizeof(status),"LOG %lus  X %.0f  Y %.1f   A JUMP",(unsigned long)(replay_trace_at/1000),trace_raw_x,trace_raw_y);
 else if(trace_found)snprintf(status,sizeof(status),"LIVE X %.0f  Y %.1f",trace_raw_x,trace_raw_y);
 else if(snapshot_diff_mode&&snapshot_valid)
   snprintf(status,sizeof(status),"SD SNAPSHOT #%04d DIFFERENCES  /  READ ONLY",snapshot_ref_number);
 else snprintf(status,sizeof(status),"TRACE %s",replay_trace_active?"LOG SAMPLE MISSING / AXES LOADING":(live_stale?"STALE":"WAITING"));
 text(7,222,.40f,trace_found?bright:secondary,status);
 text(244,222,.30f,cream,"RPM >  LOAD ^");
}
static void draw_curve_overview(Field *f){
 int n=f->width*f->height;
 draw_depth_grid(400,240);
 char title[80];shorten(title,sizeof(title),open_title,37);
 text(10,3,.59f,cream,title);
 text(10,31,.36f,secondary,"CURVE / SELECT A POINT / EXACT DECIMAL ENTRY");
 if(n<2){text(12,110,.43f,orange,"Curve needs at least two points");return;}
 /* Frozen scale: compare against the ORIGINAL verified ECU calibration, not
  * the moving edited min/max (which exaggerated tiny changes before). */
 double mn=INFINITY,mx=-INFINITY;
 for(int i=0;i<n;i++){
    double v=field_value(original,editor_length,f,i);
    if(isfinite(v)){if(v<mn)mn=v;if(v>mx)mx=v;}
 }
 if(!isfinite(mn)||!isfinite(mx)){text(12,100,.48f,orange,"No verified ECU curve data");return;}
 double center=(mn+mx)*.5;
 double span=fmax(mx-mn,fmax(1.0,fabs(center)*.25));
 double low=center-span*.70,high=center+span*.70;
 if(f->limitsKnown && f->high>f->low && isfinite(f->low)&&isfinite(f->high)
       && f->high-f->low < span*5.0){
    low=fmin(low,f->low);high=fmax(high,f->high);
 }
 float x=59,y=61,w=318,h=122;
 round_panel(x-3,y-3,w+6,h+6,4,panel);
 for(int j=0;j<=4;j++){
    float py=y+h*(float)j/4.0f;
    rect(x,py,w,1,outline);
    char tick[28];snprintf(tick,sizeof(tick),"%.2f",high-(high-low)*j/4.0);
    text(2,py-7,.37f,secondary,tick);
 }
 /* Baseline indicates which Y level is 0 if the ECU range includes it. */
 if(low<=0&&high>=0){float py=y+h*(float)(high/(high-low));rect(x,py,w,1,blue);}
 int active=cell;if(active<0)active=0;if(active>=n)active=n-1;
 float stepw=w/(float)(n-1);
 float marker_x=x+stepw*active;
 rect(marker_x-1,y,2,h,muted_leaf);
 float prevx=0,prevy=0;bool previous=false;
 for(int i=0;i<n;i++){
    double v=field_value(editor,editor_length,f,i);
    if(!isfinite(v)){previous=false;continue;}
    float px=x+stepw*i;
    float scaled=(float)((v-low)/(high-low));
    if(scaled<0)scaled=0;if(scaled>1)scaled=1;
    float py=y+(1-scaled)*h;
    if(previous){C2D_DrawLine(prevx,prevy,leaf,px,py,leaf,2.8f,0);}
    if(i!=active){
       C2D_DrawCircleSolid(px,py,0,4,bright);
       C2D_DrawCircleSolid(px,py,0,2,soil);
    }
    prevx=px;prevy=py;previous=true;
 }
 double selected_y=field_value(editor,editor_length,f,active);
 if(isfinite(selected_y)){
    float norm=(float)((selected_y-low)/(high-low));
    if(norm<0)norm=0;if(norm>1)norm=1;
    float py=y+(1-norm)*h;
    C2D_DrawCircleSolid(marker_x,py,0,10,outline);
    C2D_DrawCircleSolid(marker_x,py,0,7,orange);
    C2D_DrawCircleSolid(marker_x,py,0,3,soil);
    C2D_DrawLine(marker_x-12,py,cream,marker_x-8,py,cream,2,0);
    C2D_DrawLine(marker_x+8,py,cream,marker_x+12,py,cream,2,0);
 }
 /* Axis metadata originates in the INI; avoid claiming RPM when unavailable. */
 double xlo=axis_value(0,0),xhi=axis_value(0,n-1);
 char label[48];
 if(isfinite(xlo)){snprintf(label,sizeof(label),"%.1f",xlo);text(x,192,.35f,secondary,label);}
 if(isfinite(xhi)){snprintf(label,sizeof(label),"%.1f",xhi);text(332,192,.35f,secondary,label);}
 char units[40];copy_text(units,sizeof(units),f->unit);
 text(7,43,.34f,secondary,*units?units:"Y VALUE");
 if(axis_field[0]>=0)text(168,191,.35f,secondary,cal[axis_field[0]].unit);
 char info[130];double val=field_value(editor,editor_length,f,active),ax=axis_value(0,active);
 if(isfinite(ax)&&isfinite(val))snprintf(info,sizeof(info),"SELECTED %d/%d   X %.3f   Y %.4f %s",active+1,n,ax,val,units);
 else snprintf(info,sizeof(info),"SELECTED %d/%d   Y %.4f %s",active+1,n,val,units);
 rect(0,211,400,29,soil);
 text(9,216,.45f,cream,info);
}

static void map_touch_button(float x,float y,float w,const char*label,bool active);
static void draw_field(void){
 if(open_field<0||open_field>=cal_count)return;
 Field *f=&cal[open_field];
 char name[75];shorten(name,sizeof(name),open_title,39);text(9,30,.43f,white,name);
 char subtitle[120];snprintf(subtitle,sizeof(subtitle),"PAGE %d | %dx%d | %s",f->page,f->width,f->height,f->unit);
 text(9,46,.29f,secondary,subtitle);
 if(f->powerCycle)text(133,46,.31f,orange,"Power Cycle Required After Burn");
 if(!editor_loaded){text(12,95,.43f,orange,"WAITING FOR ECU DATA");draw_note("A RETRY  B BACK");return;}
 if(!f->validScale){text(12,85,.41f,orange,"INI expression unsupported");
    text(12,103,.33f,secondary,"Values never guessed");draw_note("B BACK");return;}
 int n=f->width*f->height;
 if(n==1){
   double v=field_value(editor,editor_length,f,0);char textval[90];
   if(!isfinite(v))strcpy(textval,"--");
   else if(f->kind==K_BITS){const char *label=choice_for_value((int)v);
     if(label)shorten(textval,sizeof(textval),label,27);
     else snprintf(textval,sizeof(textval),"Option %.0f",v);}
   else snprintf(textval,sizeof(textval),"%.3f %s",v,f->unit);
   rect(8,66,304,112,panel);
   text(17,73,.35f,secondary,f->kind==K_BITS?"INI OPTION":"ACTUAL ECU VALUE");
   text(17,101,strlen(textval)>22?.42f:.60f,cream,textval);
   text(17,148,.36f,green,(f->kind==K_BITS&&choice_count>0)?"A BROWSE OPTIONS":"SELECT EDIT  X/Y ADJUST");
 }else{
   text(15,90,.43f,orange,"Open map on upper screen");
 }
 if(!write_queued&&!write_inflight&&n==1){
    map_touch_button(8,181,145,"UNDO",false);
    map_touch_button(166,181,145,"REDO",false);
 }
 if(write_queued)text(9,187,.38f,bright,"BURN QUEUED / B TO CANCEL");
 if(write_inflight)text(9,187,.38f,bright,"BURN IN PROGRESS / CHECK STATUS BELOW");
 if(confirm_write){rect(0,190,320,19,berry);text(8,193,.32f,cream,"BENCH REVIEW A CONFIRM / B CANCEL");}
 if(confirm_discard){
   rect(4,175,312,60,berry);
   text(13,181,.50f,white,"UNBURNED CHANGES");
   text(13,205,.41f,white,"B DISCARD  |  A KEEP");return;
 }
 draw_note(staged?"A EXACT VALUE  X/Y +/-  L/R PRECISION":"SELECT EDIT  A REFRESH  B BACK");
}
static void mini_metric(int i,float x,float y,float w){
 rect(x,y,w,43,panel);
 char v[42];if(!has_value[i])strcpy(v,"--");
 else if(i==0)snprintf(v,sizeof(v),"%.0f",displayed[i]);
 else snprintf(v,sizeof(v),"%.1f",displayed[i]);
 text(x+5,y+2,.38f,secondary,metric_labels[i]);
 if(dreamfont_sheet)dream_text(x+5,y+13,v,.47f,w-9);
 else text(x+5,y+16,.71f,live_valid&&!live_stale?cream:secondary,v);
}
static void map_touch_button(float x,float y,float w,const char*label,bool active){
 round_panel(x,y,w,28,6,active?muted_leaf:panel2);
 text(x+10,y+5,.48f,active?bright:cream,label);
}
static void draw_map_tools(void){
 text(9,29,.52f,white,"MAP TOOLS / LOCAL EDITS");
 if(open_field<0)return;
 BtsMapRegion r=map_selected_region();
 char range[96];snprintf(range,sizeof(range),"%dx%d region  |  step %.3g  |  %s",
 r.x1-r.x0+1,r.y1-r.y0+1,steps[step_index],map_anchor_valid?"RANGE":"ONE CELL");
 text(9,48,.38f,secondary,range);
 int visible=4;if(map_tool_cursor<map_tool_scroll)map_tool_scroll=map_tool_cursor;
 if(map_tool_cursor>=map_tool_scroll+visible)map_tool_scroll=map_tool_cursor-visible+1;
 for(int n=0;n<visible;n++){
    int index=map_tool_scroll+n;if(index>=MAP_TOOL_COUNT)break;
    float y=67+n*34.0f;
    round_panel(8,y,304,30,5,index==map_tool_cursor?muted_leaf:panel);
    char label[70];snprintf(label,sizeof(label),"%02d  %s",index+1,map_tool_names[index]);
    text(15,y+6,.44f,index==map_tool_cursor?white:cream,label);
 }
 text(8,208,.36f,bright,"A APPLY  L/R STEP  B BACK");
 text(8,223,.31f,secondary,"All operations local until explicit BURN");
}
/* Lower touch screen stays a control deck; maps render ONLY on the upper LCD. */

static void draw_curve_deck(void){
 if(open_field<0||open_field>=cal_count)return;
 if(map_tools_open){draw_map_tools();return;}
 Field *f=&cal[open_field];int n=f->width*f->height;

 /* Selected point: both real axis and current output, at readable precision. */
 char summary[88];double x=axis_value(0,cell);
 double v=editor_loaded?field_value(editor,editor_length,f,cell):NAN;
 if(isfinite(v))snprintf(summary,sizeof(summary),"POINT %d/%d  Y %.4f %s",cell+1,n,v,f->unit);
 else snprintf(summary,sizeof(summary),"POINT %d/%d - ECU data unavailable",cell+1,n);
 text(10,49,.43f,bright,summary);
 char step[68];snprintf(step,sizeof(step),"FOCUS: %s   STEP %.3g  |  %s",map_focus==1?"X BREAKPOINT":"Y OUTPUT",steps[step_index],staged?"EDIT":"VIEW");
 text(10,67,.34f,secondary,step);
 int first=cell-2;if(first<0)first=0;
 if(first+5>n)first=n-5;
           if(first<0)first=0;
 for(int i=first;i<n&&i<first+5;i++){
   float y=82+(i-first)*18;
   round_panel(7,y,306,17,3,i==cell?muted_leaf:panel);
   double xx=axis_value(0,i),yy=field_value(editor,editor_length,f,i);
   char line[92];
   if(isfinite(xx)&&isfinite(yy))snprintf(line,sizeof(line),"%02d   X %.3f       Y %.4f",i+1,xx,yy);
   else if(isfinite(yy))snprintf(line,sizeof(line),"%02d   X --          Y %.4f",i+1,yy);
   else snprintf(line,sizeof(line),"%02d   ECU VALUE UNAVAILABLE",i+1);
   text(14,y+2,.42f,i==cell?white:cream,line);
 }
 map_touch_button(6,176,99,staged?"ENTER...":"EDIT",staged);
 map_touch_button(111,176,98,"UNDO",false);
 map_touch_button(216,176,98,"REDO",false);
 draw_note("UP X / DOWN Y   A EXACT   L/R PRECISION");
}
static void draw_map_deck(void){
 if(open_field<0||open_field>=cal_count)return;
 if(map_tools_open){draw_map_tools();return;}
 Field *f=&cal[open_field];
 if(map_3d_view){
   text(9,32,.46f,cream,"CIRCLE PAD: ROTATE   D-PAD: FINE   B: BACK");
   round_panel(8,57,304,122,6,panel);
   double value=editor_loaded?field_value(editor,editor_length,f,cell):NAN;
   char actual[100];
   if(isfinite(value))snprintf(actual,sizeof(actual),"Selected Z: %.3f %s",value,f->unit);
   else snprintf(actual,sizeof(actual),"Selected Z unavailable");
   text(16,76,.69f,white,actual);
   text(16,122,.44f,secondary,"ROTATE VIEW / NO ECU WRITES");
   text(16,151,.38f,green,"Uses calibration from this INI table");
   draw_note("B RETURNS TO TUNERSTUDIO TREE");
   return;
 }

 rect(291,30,19,3,blue);
 if(replay_trace_active){
   rect(7,47,306,43,panel);
   char recorded[130];snprintf(recorded,sizeof(recorded),"RECORDED @ %lus - LOG CURSOR",(unsigned long)(replay_trace_at/1000));
   text(14,55,.47f,bright,recorded);
   text(14,73,.34f,secondary,"Map is actual ECU calibration; trace is recorded data");
 }else{mini_metric(0,7,47,98);mini_metric(4,111,47,98);mini_metric(2,215,47,98);}
 Field *ff=NULL;uint8_t *buf=NULL;int len=0,index=0;
 bool available=active_focus_data(&ff,&buf,&len,&index);
 double v=available?field_value(buf,len,ff,index):NAN;
 char line[150];
 const char *label=map_focus==0?"MAP Z":map_focus==1?"X AXIS":"Y AXIS";
 if(isfinite(v))snprintf(line,sizeof(line),"%s [%d] = %.3f %s",label,index+1,v,ff->unit);
 else snprintf(line,sizeof(line),"%s [%d] - %s",label,focused_index()+1,map_focus?"LOADING / READ ONLY":"NO ECU DATA");
 text(9,98,.46f,white,line);
 char st[110];snprintf(st,sizeof(st),"%s    STEP %.3g    %s",staged?"STAGED":"VIEW",steps[step_index],live_stale?"STALE":"LIVE");
 text(9,120,.33f,staged?bright:secondary,st);
 if(replay_trace_active){
    char coverage[90];
    if(trace_show_coverage&&map_focus==0&&cell>=0&&cell<EDIT_CAP)
       snprintf(coverage,sizeof(coverage),"LOG COVERAGE: %u hits in selected cell",(unsigned)trace_coverage[cell]);
    else snprintf(coverage,sizeof(coverage),"X: LOG COVERAGE  |  SELECT: EDIT");
    text(8,179,.32f,bright,coverage);
 }
 if(f->powerCycle&&changed())text(9,131,.29f,orange,"Power cycle needed after a verified burn");
 if(write_queued)text(8,131,.36f,bright,"BURN QUEUED - B CANCEL");
 if(write_inflight)text(8,131,.36f,bright,"BURN / VERIFY RUNNING");
 map_touch_button(6,140,98,staged?"ENTER...":"EDIT",staged);
 map_touch_button(111,140,98,"- VALUE",false);
 map_touch_button(216,140,98,"+ VALUE",false);
 map_touch_button(5,174,102,"TOOLS",false);
 map_touch_button(111,174,102,"UNDO",false);
 map_touch_button(217,174,98,"BURN",staged&&changed());
 text(8,201,.31f,secondary,"L/R: PRECISION   A: EXACT ENTRY   B: BACK");
 if(confirm_discard){
   rect(4,174,312,63,berry);
   text(13,178,.51f,white,"UNBURNED CHANGES");
   text(13,199,.42f,white,"B: DISCARD     A: KEEP EDITING");
   return;
 }
 if(confirm_write){rect(0,211,320,29,berry);text(7,213,.34f,cream,"BENCH REVIEW - A CONFIRM / B CANCEL");}
 else{rect(0,211,320,29,soil);text(7,214,.31f,bright,"START REVIEW   B BACK  |  ALL REAL DATA");}
}
/* INI-driven knock visualizer: this uses only actual OutputChannels,
 * not the rusEFI plugin spectrogram packet format. No synthetic knock data. */
static bool knock_range(const char *name,float *lo,float *hi){
 for(int i=0;i<gauge_def_count;i++)if(!strcasecmp(gauges[i].channel,name)){
    double a=0,b=0;if(number(gauges[i].minimum,&a)&&number(gauges[i].maximum,&b)&&b>a){
       *lo=(float)a;*hi=(float)b;return true;
    }
 }
 return false;
}
static void knock_value_panel(const char *name,const char *title,float x,float y,float w){
 rect(x+1,y+2,w,46,outline);rect(x,y,w,45,panel);
 text(x+7,y+3,.43f,secondary,title);
 double v=0;char out[28]="--";
 bool present=runtime_value(name,&v);
 if(present)snprintf(out,sizeof(out),"%.1f",v);
 text(x+7,y+20,.65f,present?cream:orange,out);
}
static void draw_knock_top(void){
 draw_depth_grid(400,240);
 rect(0,0,400,29,soil);
 text(9,3,.65f,cream,"KNOCK MONITOR");
 text(250,7,.33f,live_stale?orange:bright,live_stale?"LAST SAMPLE":"ECU LIVE");
 knock_value_panel("m_knockLevel","Knock dBv",5,34,128);
 knock_value_panel("m_knockRetard","Retard deg",136,34,128);
 knock_value_panel("m_knockCount","Count",267,34,128);
 text(8,82,.39f,cream,knock_bank?"CYLINDERS 7-12":"CYLINDERS 1-6");
 for(int j=0;j<6;j++){
    int cyl=j+1+knock_bank*6;char name[32],label[32],vtext[25]="--";
    snprintf(name,sizeof(name),"m_knockCyl%d",cyl);snprintf(label,sizeof(label),"C%d",cyl);
    int col=j%3,row=j/3;float x=5.0f+col*132.0f,y=103.0f+row*54.0f;
    rect(x,y,126,50,panel);text(x+5,y+2,.38f,cream,label);
    double val=0;float mn=0,mx=0;
    bool available=runtime_value(name,&val),range=knock_range(name,&mn,&mx);
    if(available)snprintf(vtext,sizeof(vtext),"%.1f",val);
    text(x+37,y+2,.47f,available?bright:orange,vtext);
    rect(x+6,y+30,114,9,outline);
    if(available&&range){
       float ratio=((float)val-mn)/(mx-mn);
       if(ratio<0)ratio=0;if(ratio>1)ratio=1;
       rect(x+6,y+30,114*ratio,9,ratio>.8f?berry:leaf);
    }else if(available)text(x+8,y+29,.25f,orange,"INI range unavailable");
 }
 rect(4,220,392,17,soil);
 text(9,220,.33f,cream,"L/R CYL BANK  |  Levels from real INI data");
}
static void draw_knock_deck(void){
 text(9,29,.55f,cream,"Knock Monitor");
 text(9,55,.40f,secondary,"Live per-cylinder levels and retard");
 text(9,82,.38f,secondary,"Data read from real ECU channels");
 text(9,109,.35f,cream,"L/R: Switch cylinders 1-6 / 7-12");
 text(9,136,.35f,secondary,"Full frequency spectrogram: unavailable");
 text(9,154,.33f,secondary,"Requires plugin packet decoder on ESP");
 text(9,173,.32f,cream,live_stale?"LINK: Sample stale":(live_valid?"LINK: Streaming":"LINK: Waiting"));
 /* Two-second-scale waveform of actual received knock dBv readings.
  * Gaps are left blank; no synthetic data are ever plotted. */
 rect(138,170,172,34,panel);
 float minv=INFINITY,maxv=-INFINITY;
 for(int n=0;n<knock_count;n++){
    int a=(knock_head-knock_count+n+KNOCK_HISTORY)%KNOCK_HISTORY;
    if(knock_history_valid[a]){
       float v=knock_history[a];if(v<minv)minv=v;if(v>maxv)maxv=v;
    }
 }
 if(isfinite(minv)&&isfinite(maxv)){
    if(maxv-minv<.05f){minv-=.5f;maxv+=.5f;}
    bool prior=false;float px=0,py=0;
    for(int n=0;n<knock_count;n++){
       int a=(knock_head-knock_count+n+KNOCK_HISTORY)%KNOCK_HISTORY;
       if(!knock_history_valid[a]){prior=false;continue;}
       float x=140.0f+168.0f*(float)n/(float)(KNOCK_HISTORY-1);
       float y=199.0f-25.0f*(knock_history[a]-minv)/(maxv-minv);
       if(prior)C2D_DrawLine(px,py,bright,x,y,bright,1.3f,0);
       px=x;py=y;prior=true;
    }
 }else text(147,178,.28f,orange,"No knock samples");
 draw_note("B VIEWS  |  KNOCK TREND: REAL SAMPLES");
}
static void draw_dashboard_deck(void){
 text(9,32,.59f,white,"LIVE DASHBOARD");
 char line[76];snprintf(line,sizeof(line),"Layout: %s",gauge_styles[gauge_style]);
 text(9,56,.44f,cream,line);
 const char*status=live_stale?"HELD: awaiting fresh ECU sample":live_valid?"Streaming real ECU telemetry":"Waiting for ECU telemetry";
 text(9,76,.36f,live_stale?orange:secondary,status);
 const char *items[4]={"NEXT LAYOUT","LIVE / MAX / MIN","RESET STATS","CUSTOMIZE"};
 for(int i=0;i<4;i++){
   float x=7+(i%2)*155,y=107+(i/2)*46;
   round_panel(x,y,150,39,7,panel2);
   text(x+9,y+10,.43f,i==0?green:cream,items[i]);
 }
 draw_note("TAP ACTION  |  L/R CYCLE LAYOUTS");
}
static void draw_quality_top(void){
 draw_depth_grid(400,240);
 text(9,3,.57f,cream,"RECORDED DATA QUALITY / NOT ECU FAULTS");
 for(int ch=0;ch<LOG_FIELDS;ch++){
    float x=7.0f+(ch%3)*131.0f,y=34.0f+(ch/3)*65.0f;
    bool active=quality_channel==ch;
    round_panel(x,y,125,60,7,active?muted_leaf:panel);
    char label[70],num[36];shorten(label,sizeof(label),log_channel_names[ch],17);
    text(x+6,y+4,.38f,active?white:secondary,label);
    const BtsSignalQuality*q=&log_quality[ch];
    if(!q->valid){text(x+6,y+24,.50f,orange,"NO DATA");continue;}
    snprintf(num,sizeof(num),"%.1f%% VALID",q->total?100.0*(double)q->valid/q->total:0.0);
    text(x+6,y+24,.51f,q->missing?orange:green,num);
    snprintf(num,sizeof(num),"%u gaps / %u ms",q->long_gaps,q->maximum_observed_gap_ms);
    text(x+6,y+45,.30f,secondary,num);
 }
}
static void draw_quality_deck(void){
 text(10,30,.55f,white,"SAVED LOG / SAMPLE QUALITY");
 char line[128],name[70];shorten(name,sizeof(name),log_channel_names[quality_channel],26);
 text(12,56,.55f,cream,name);
 const BtsSignalQuality*q=&log_quality[quality_channel];
 round_panel(8,78,304,116,7,panel);
 snprintf(line,sizeof(line),"Recorded: %u   Valid: %u",q->total,q->valid);
 text(17,89,.45f,cream,line);
 snprintf(line,sizeof(line),"Unavailable samples: %u",q->missing);
 text(17,111,.45f,q->missing?orange:green,line);
 snprintf(line,sizeof(line),"Largest time gap: %lu ms",(unsigned long)q->maximum_observed_gap_ms);
 text(17,133,.45f,cream,line);
 snprintf(line,sizeof(line),"Long intervals: %u   Repeated times: %u",q->long_gaps,q->repeated_timestamps);
 text(17,155,.42f,cream,line);
 snprintf(line,sizeof(line),"Longest unchanged run: %u",q->longest_constant_run);
 text(17,177,.39f,secondary,line);
 text(8,203,.35f,bright,"L/R CHANNEL  A JUMP TO LARGEST GAP");
 text(8,216,.34f,bright,"X EXPORT  B BACK TO ANALYSIS");
 text(8,229,.30f,secondary,"Quality observations are not ECU errors");
}
static void draw_insights_top(void){
 draw_depth_grid(400,240);
 text(9,3,.56f,cream,"RECORDED DATA / ANALYSIS");
 if(insight_dirty)insights_refresh();
 if(!insight_valid){text(14,96,.55f,orange,"No valid log window");return;}
 BtsLogInsights *v=&insight_stats;
 char title[80];shorten(title,sizeof(title),log_channel_names[insight_channel],24);
 text(12,32,.55f,white,title);
 char brief[100];snprintf(brief,sizeof(brief),"%u/%u VALID   %.1f%%   %u GAPS",    v->valid,v->samples,v->samples?100.0*(double)v->valid/v->samples:0.0,v->gaps);
 text(12,56,.41f,v->missing?orange:bright,brief);
 round_panel(9,83,382,146,8,panel);
 if(!v->valid){text(20,116,.48f,orange,"No usable samples for this channel");return;}
 if(insight_view==0){
    text(17,88,.38f,secondary,"RECORDED HISTOGRAM / 12 BINS");
    unsigned peak=1;
    for(int i=0;i<BTS_HIST_BINS;i++)if(v->hist[i]>peak)peak=v->hist[i];
    for(int i=0;i<BTS_HIST_BINS;i++){
      float bh=98.0f*(float)v->hist[i]/(float)peak;
      rect(20+i*30,203-bh,24,bh,v->hist[i]?blue:outline);
    }
    snprintf(brief,sizeof(brief),"MIN %.2f     MED %.2f     MAX %.2f",v->minimum,v->median,v->maximum);
    text(18,210,.38f,cream,brief);
 }else{
    text(17,88,.38f,secondary,"RECORDED TRACE / TIME ORDER");
    double span=v->maximum-v->minimum;if(span<1e-9)span=1;
    bool joined=false;float px=0,py=0;
    uint32_t t0=insight_flat_times[0],t1=insight_flat_times[insight_source_count-1];
    for(int i=0;i<insight_source_count;i++){
       if(!insight_flat_valid[i]){joined=false;continue;}
       float x=18+357.0f*(float)(insight_flat_times[i]-t0)/(float)(t1>t0?t1-t0:1);
       float y=202-94.0f*(float)((insight_flat_values[i]-v->minimum)/span);
       if(joined)C2D_DrawLine(px,py,leaf,x,y,leaf,1.5f,0);
       px=x;py=y;joined=true;
    }
    snprintf(brief,sizeof(brief),"P05 %.2f   MEAN %.2f   P95 %.2f",v->p05,v->mean,v->p95);
    text(18,210,.38f,cream,brief);
 }
}
static void draw_insights_deck(void){
 text(9,31,.54f,white,"LOG INSIGHTS / NO ECU WRITES");
 if(insight_dirty)insights_refresh();
 if(!insight_valid){text(9,78,.43f,orange,"No recorded samples");draw_note("B RETURN");return;}
 char line[132];BtsLogInsights *v=&insight_stats;
 char channel[64];shorten(channel,sizeof(channel),log_channel_names[insight_channel],23);
 round_panel(8,57,304,39,6,panel);
 text(15,61,.39f,secondary,"CHANNEL / L-R TO CYCLE");
 text(15,76,.51f,cream,channel);
 snprintf(line,sizeof(line),"%s   |   %s",insight_window==0?"WHOLE LOG":insight_window==1?"LAST 10S":insight_window==2?"LAST 30S":"LAST 60S",
        insight_view==0?"HISTOGRAM":"TIME TRACE");
 text(11,104,.40f,bright,line);
 snprintf(line,sizeof(line),"Records %u    Valid %u    Missing %u",v->samples,v->valid,v->missing);
 text(11,128,.42f,cream,line);
 snprintf(line,sizeof(line),"Largest time gap: %u ms",v->max_gap_ms);
 text(11,149,.42f,cream,line);
 snprintf(line,sizeof(line),"Observed gaps: %u  /  %.1f sec",v->gaps,v->duration_ms/1000.0);
 text(11,170,.40f,cream,line);
 text(8,195,.36f,bright,"A EXTREME   DOWN LARGEST GAP");
 text(8,209,.35f,bright,"L/R CHANNEL  X PLOT  Y TIME");
 text(8,225,.31f,secondary,"START REPORT  SELECT QUALITY  B REPLAY");
}
static void draw_compare_top(void){
 draw_depth_grid(400,240);
 text(9,3,.56f,white,"LOG COMPARISON / REAL RECORDED DATA");
 if(!compare_buffer||!replay_count||compare_count<1){
    text(17,96,.51f,orange,"No reference log loaded");return;
 }
 char name[60];shorten(name,sizeof(name),log_channel_names[compare_channel],27);
 text(12,33,.54f,white,name);
 text(12,55,.39f,green,"BASE RECORDING");text(180,55,.39f,blue,"REFERENCE RECORDING");
 if(compare_ecu_identity==2)text(12,69,.34f,orange,"WARNING: ECU signatures DIFFER");
 else if(compare_ecu_identity==0)text(12,69,.33f,secondary,"ECU identity not available for both logs");
 else text(12,69,.33f,green,"Same recorded ECU signature");
 float start=0,end=(float)(replay_buffer[replay_count-1].t_ms-replay_buffer[0].t_ms);
 u32 cursor=replay_time_ms-replay_buffer[0].t_ms;
 if(compare_window){
    float half=compare_window==1?5000.0f:15000.0f;
    start=(float)cursor-half;if(start<0)start=0;
    end=(float)cursor+half;
    float maxdur=(float)(replay_buffer[replay_count-1].t_ms-replay_buffer[0].t_ms);
    if(end>maxdur)end=maxdur;
 }
 if(end<=start)end=start+1;
 double mn=INFINITY,mx=-INFINITY;
 int refch=compare_map[compare_channel];
 for(int i=0;i<replay_count;i++){
    LogSample *r=&replay_buffer[i];float t=(float)(r->t_ms-replay_buffer[0].t_ms);
    if(t<start||t>end)continue;
    if(r->valid&(1u<<compare_channel)){double v=r->values[compare_channel];if(isfinite(v)){if(v<mn)mn=v;if(v>mx)mx=v;}}
 }
 if(refch>=0){
    for(int i=0;i<compare_count;i++){
       LogSample *r=&compare_buffer[i];float t=(float)(r->t_ms-compare_buffer[0].t_ms);
       if(t<start||t>end)continue;
       if(r->valid&(1u<<refch)){double v=r->values[refch];if(isfinite(v)){if(v<mn)mn=v;if(v>mx)mx=v;}}
    }
 }
 round_panel(8,80,384,150,8,panel);
 if(!isfinite(mn)||!isfinite(mx)){
    text(18,128,.52f,orange,"No comparable valid values in this interval");return;
 }
 double span=mx-mn;if(span<1e-9)span=1.0;
 for(int g=0;g<5;g++)rect(18,94+g*26,364,1,outline);
 /* Every plotted point comes from a real recorded sample; gaps stay gaps. */
 for(int track=0;track<2;track++){
    const LogSample *samples=track?compare_buffer:replay_buffer;
    int count=track?compare_count:replay_count;
    int ch=track?refch:compare_channel;
    if(ch<0)continue;
    u32 first=samples[0].t_ms;
    bool last_valid=false;float px=0,py=0,pt=0;
    int stride=count>350?count/350:1;
    for(int i=0;i<count;i+=stride){
       const LogSample *r=&samples[i];
       float t=(float)(r->t_ms-first);
       if(t<start||t>end){last_valid=false;continue;}
       if(!(r->valid&(1u<<ch))||!isfinite(r->values[ch])){last_valid=false;continue;}
       float x=18+364.0f*(t-start)/(end-start);
       float y=207-106.0f*(r->values[ch]-mn)/span;
       if(last_valid&&t-pt<=BTS_COMPARE_GAP_MS*2){
          u32 c=track?blue:green;C2D_DrawLine(px,py,c,x,y,c,1.7f,0);
       }
       px=x;py=y;pt=t;last_valid=true;
    }
 }
 float xcursor=18+364.0f*((float)cursor-start)/(end-start);
 if(xcursor>=18&&xcursor<=382)rect(xcursor,93,1,118,orange);
 char line[105];snprintf(line,sizeof(line),"MIN %.2f    MAX %.2f    CURSOR %.1fs",mn,mx,cursor/1000.0f);
 text(20,213,.37f,secondary,line);
}
static void draw_compare_deck(void){
 text(10,30,.55f,white,"COMPARE RECORDED SESSIONS");
 if(!compare_buffer){text(10,85,.46f,orange,"Choose a second saved log");return;}
 char a[58],b[58];shorten(a,sizeof(a),replay_filename,26);shorten(b,sizeof(b),compare_filename,26);
 text(10,54,.37f,secondary,"BASE / A");text(88,54,.40f,cream,a);
 text(10,74,.37f,secondary,"REF / B");text(88,74,.40f,cream,b);
 BtsCompareStats *st=&compare_stats[compare_channel];
 round_panel(8,99,304,90,8,panel);
 char line[120];
 if(compare_map[compare_channel]<0){
    text(16,117,.43f,orange,"Channel not present in reference log");
 }else if(!st->paired){
    text(16,117,.43f,orange,"No samples aligned within 500 ms");
 }else{
    snprintf(line,sizeof(line),"PAIRED %u   UNPAIRED %u",st->paired,st->skipped);
    text(16,106,.44f,cream,line);
    snprintf(line,sizeof(line),"A %.3f     B %.3f",st->base_sum/st->paired,st->reference_sum/st->paired);
    text(16,130,.48f,cream,line);
    snprintf(line,sizeof(line),"DELTA %+0.3f   ABS AVG %.3f",st->delta_sum/st->paired,st->abs_delta_sum/st->paired);
    text(16,154,.43f,bright,line);
    snprintf(line,sizeof(line),"MAX ABS DIFFERENCE %.3f",st->maximum_abs_delta);
    text(16,174,.36f,secondary,line);
 }
 text(8,199,.35f,bright,"L/R CHANNEL   LEFT/RIGHT CURSOR");
 text(8,212,.34f,bright,"X ZOOM   Y CHANGE REFERENCE");
 text(8,226,.33f,secondary,"START EXPORT REPORT   B REPLAY");
}
static void draw_replay_top(void){
 draw_depth_grid(400,240);
 text(9,3,.53f,cream,"BRICK TUNER / LOG REPLAY");
 char head[75];snprintf(head,sizeof(head),"%d / %d",replay_position+1,replay_count);
 text(323,5,.34f,bright,head);
 if(!replay_count){text(30,93,.54f,orange,"No saved telemetry");return;}
 LogSample *r=&replay_buffer[replay_position];
 for(int i=0;i<LOG_FIELDS;i++){
   float x=5.0f+(i%3)*131.0f,y=30.0f+(i/3)*69.0f;
   rect(x,y,126,65,panel);
   char name[65],val[34]="--";
   pretty_label(name,sizeof(name),log_channel_names[i]);shorten(name,sizeof(name),name,17);
   text(x+6,y+3,.39f,secondary,name);
   if(r->valid&(1u<<i))snprintf(val,sizeof(val),fabsf(r->values[i])>=1000?"%.0f":"%.2f",r->values[i]);
   text(x+6,y+23,.78f,(r->valid&(1u<<i))?cream:orange,val);
 }
}
static void replay_action(int action){
 if(!replay_count){note("Open a recorded log first");return;}
 switch(action){
 case 0:replay_playing=!replay_playing;replay_wall_ms=osGetTime();break;
 case 1:compare_return_page=PAGE_REPLAY;compare_open_picker();break;
 case 2:analysis_return_page=PAGE_REPLAY;insights_open();break;
 case 3:trace_return_page=PAGE_REPLAY;replay_open_trace();break;
 case 4:{/* Export the first channel with actual samples from this saved log. */
   int valid=-1;
   for(int ch=0;ch<LOG_FIELDS && valid<0;ch++)
     for(int i=0;i<replay_count;i++)if(replay_buffer[i].valid&(1u<<ch)){valid=ch;break;}
   if(valid<0){note("No valid recorded channel for export");break;}
   insight_channel=valid;insight_window=0;insight_dirty=true;
   insights_export();break;
 }
 case 5:quality_return_page=PAGE_REPLAY;quality_open();break;
 }
}
static void draw_replay_deck(void){
 text(9,30,.55f,white,"RECORDED SESSION");
 char filename[60];shorten(filename,sizeof(filename),replay_filename,32);
 text(9,49,.42f,secondary,filename);
 if(!replay_count){draw_note("B BACK - OPEN A SAVED LOG");return;}
 u32 last=replay_buffer[replay_count-1].t_ms,first=replay_buffer[0].t_ms;
 char timing[92];snprintf(timing,sizeof(timing),"%02lu:%02lu / %02lu:%02lu  |  %d samples",
   (unsigned long)(replay_time_ms/60000),(unsigned long)(replay_time_ms/1000%60),
   (unsigned long)(last/60000),(unsigned long)(last/1000%60),replay_count);
 text(9,68,.43f,cream,timing);
 rect(9,92,302,12,outline);
 float progress=last>first?(float)(replay_time_ms-first)/(float)(last-first):1.f;
 if(progress<0)progress=0;
  if(progress>1)progress=1;
 rect(9,92,302*progress,12,blue);
 for(int i=0;i<replay_mark_count;i++){
    float pos=last>first?(float)(replay_marks[i]-first)/(float)(last-first):0;
    rect(9+302*pos-1,87,3,21,orange);
 }
 const char*name[6]={replay_playing?"PAUSE":"PLAY","COMPARE","ANALYZE","TRACE MAP","EXPORT","QUALITY"};
 for(int i=0;i<6;i++){
    int col=i%3,row=i/3;float x=7+col*103,y=120+row*39;
    round_panel(x,y,99,35,6,(i==0&&replay_playing)?muted_leaf:panel2);
    text(x+7,y+8,.43f,i==0?green:cream,name[i]);
 }
 draw_note("TAP ACTION  |  D-PAD SCRUB  SELECT MARK");
}
static void draw_replay_stats_top(void){
 draw_depth_grid(400,240);
 text(8,3,.52f,cream,"BRICK TUNER / RECORDED STATISTICS");
 for(int i=0;i<LOG_FIELDS;i++){
   float x=5.0f+(i%3)*131.0f,y=30.0f+(i/3)*69.0f;
   rect(x,y,126,65,i==replay_stat_cursor?muted_leaf:panel);
   char label[65],name[35];pretty_label(label,sizeof(label),log_channel_names[i]);shorten(name,sizeof(name),label,17);
   text(x+5,y+2,.38f,cream,name);
   ReplayStat *st=&replay_stats[i];
   if(st->count){
      char line[55];
      snprintf(line,sizeof(line),"Low %.2f",st->min);text(x+6,y+19,.34f,dew,line);
      snprintf(line,sizeof(line),"Avg %.2f",st->mean);text(x+6,y+33,.34f,bright,line);
      snprintf(line,sizeof(line),"High %.2f",st->max);text(x+6,y+47,.34f,petal,line);
   }else text(x+6,y+29,.40f,orange,"No samples");
 }
}
static void draw_replay_stats_deck(void){
 text(8,29,.58f,cream,"Session Summary");
 if(!replay_count){text(8,85,.42f,orange,"No recorded session loaded");return;}
 char label[90],shorter[80];pretty_label(label,sizeof(label),log_channel_names[replay_stat_cursor]);
 shorten(shorter,sizeof(shorter),label,30);text(8,57,.48f,cream,shorter);
 ReplayStat *st=&replay_stats[replay_stat_cursor];char line[110];
 if(!st->count)text(8,97,.42f,orange,"No actual readings for this channel");
 else{
    snprintf(line,sizeof(line),"Minimum: %.3f",st->min);text(8,90,.52f,dew,line);
    snprintf(line,sizeof(line),"Average: %.3f",st->mean);text(8,116,.52f,bright,line);
    snprintf(line,sizeof(line),"Maximum: %.3f",st->max);text(8,142,.52f,petal,line);
 }
 snprintf(line,sizeof(line),"%lu valid records / %d total",(unsigned long)st->count,replay_count);
 text(8,170,.38f,secondary,line);
 text(8,194,.37f,cream,"D-PAD: Choose one of nine channels");
 draw_note("A/B REPLAY  |  RECORDED DATA ONLY");
}
static void draw_channel_top(void){
 draw_depth_grid(400,240);
 text(8,2,.51f,cream,"LIVE ECU CHANNEL INSPECTOR");
 if(selectable_count<=0){text(15,88,.52f,orange,"Waiting for ECU INI");return;}
 int first=channel_cursor-2;if(first<0)first=0;
 if(first>selectable_count-5)first=selectable_count-5;
 if(first<0)first=0;
 for(int i=first;i<selectable_count&&i<first+5;i++){
   int idx=selectable_channels[i];Field *f=&channels[idx];
   float y=27+(i-first)*42;
   rect(6,y,388,40,i==channel_cursor?muted_leaf:panel);
   char label[78],shown[70],val[60]="--";
   pretty_label(label,sizeof(label),f->name);shorten(shown,sizeof(shown),label,30);
   text(13,y+5,.48f,cream,shown);
   double value=0;
   if(channel_live_value(idx,&value))snprintf(val,sizeof(val),"%.3f %s",value,f->unit);
   else snprintf(val,sizeof(val),"-- %s",f->unit);
   text(259,y+5,.34f,i==channel_cursor?bright:secondary,val);
 }
}
static void draw_channel_deck(void){
 text(8,27,.50f,cream,"All INI Output Channels");
 char line[100];snprintf(line,sizeof(line),"%d / %d",selectable_count?channel_cursor+1:0,selectable_count);
 text(260,29,.34f,bright,line);
 if(selectable_count<=0){text(9,90,.42f,orange,"No decoded ECU channels yet");draw_note("B VIEWS");return;}
 if(channel_cursor<channel_scroll)channel_scroll=channel_cursor;
 if(channel_cursor>=channel_scroll+6)channel_scroll=channel_cursor-5;
 for(int r=0;r<6;r++){
    int i=channel_scroll+r;if(i>=selectable_count)break;
    int idx=selectable_channels[i];Field *f=&channels[idx];
    float y=49+r*25;
    rect(8,y,303,23,i==channel_cursor?muted_leaf:panel);
    char label[80],shorter[64],value[38]="--";
    pretty_label(label,sizeof(label),f->name);shorten(shorter,sizeof(shorter),label,25);
    text(12,y+3,.43f,cream,shorter);
    double actual;
    if(channel_live_value(idx,&actual))snprintf(value,sizeof(value),"%.2f",actual);
    text(240,y+3,.40f,bright,value);
 }
 draw_note("D-PAD BROWSE  A INSPECT  B VIEWS");
}
/* Draws deterministic tooth states on the handheld GPU only. */
static void draw_trigger_trace(float left,float right,float high,float low,
                               const DreamTriggerWheel *wheel,bool cam){
 float previous=cam?(dream_cam_high(0)?high:low):
                       (dream_crank_high(0,wheel)?high:low);
 float step=(right-left)/320.0f;
 for(int i=1;i<=320;i++){
   double angle=((double)i/320.0)*720.0;
   float value=(cam?dream_cam_high(angle):dream_crank_high(angle,wheel))?high:low;
   float x=left+(float)i*step;
   C2D_DrawLine(x-step,previous,cam?petal:dew,x,value,cam?petal:dew,1.8f,0);
   previous=value;
 }
}
static void draw_trigger_top(void){
 const DreamTriggerWheel *wheel=&dream_wheels[stim_wheel];
 draw_depth_grid(400,240);
 text(12,3,.48f,cream,"TRIGGER WAVEFORM LAB");
 rect(12,33,376,21,panel);
 text(18,35,.39f,bright,"SOFTWARE MODEL / NO ECU OUTPUTS");
 char header[100];
 snprintf(header,sizeof(header),"Wheel %s   %d RPM   %s",wheel->name,stim_rpm,
          stim_preview?"ANIMATING":"PAUSED");
 text(14,61,.48f,cream,header);
 double live_rpm=0,errors=0,ratio=0;
 char measured[120];
 bool rpm_ok=live_valid&&!live_stale&&runtime_value("RPMValue",&live_rpm);
 bool error_ok=live_valid&&!live_stale&&runtime_value("totalTriggerErrorCounter",&errors);
 bool ratio_ok=live_valid&&!live_stale&&runtime_value("trgtriggerSyncGapRatio",&ratio);
 if(rpm_ok&&error_ok){
     snprintf(measured,sizeof(measured),"ECU LIVE: %.0f RPM / %.0f trigger errors",live_rpm,errors);
 }else if(rpm_ok){
     snprintf(measured,sizeof(measured),"ECU LIVE: %.0f RPM / Errors unavailable",live_rpm);
 }else snprintf(measured,sizeof(measured),"ECU LIVE: No current RPM sample");
 text(14,78,.34f,rpm_ok?bright:orange,measured);
 rect(12,90,376,112,panel);
 for(int x=0;x<5;x++)rect(20+(float)x*90,95,1,96,outline);
 text(20,94,.34f,bright,"CRANK");
 text(20,148,.34f,bright,"CAM");
 draw_trigger_trace(22,380,115,135,wheel,false);
 draw_trigger_trace(22,380,164,182,wheel,true);
 float cursor=22.0f+358.0f*(float)(stim_phase(osGetTime())/720.0);
 rect(cursor,92,2,101,leaf);
 if(ratio_ok){
    char v[100];snprintf(v,sizeof(v),"ECU trigger sync gap ratio: %.3f",ratio);
    text(14,206,.32f,secondary,v);
 }
 text(14,223,.31f,secondary,"Illustration only: 720 deg / two crank revolutions");
}
static void draw_trigger_deck(void){
 text(9,27,.57f,cream,"Offline Trigger Model");
 rect(8,52,304,32,panel);
 char wheel[80];snprintf(wheel,sizeof(wheel),"Crank: %s   Cam: 1 pulse / 720 deg",dream_wheels[stim_wheel].name);
 text(14,61,.41f,bright,wheel);
 char rpm[50];snprintf(rpm,sizeof(rpm),"PREVIEW RPM     %d",stim_rpm);
 text(14,95,.52f,cream,rpm);
 text(14,119,.41f,secondary,"UP/DOWN   RPM +/- 100");
 text(14,137,.41f,secondary,"LEFT/RIGHT   RPM +/- 10");
 text(14,155,.41f,secondary,"L/R   Wheel pattern");
 rect(8,179,304,25,stim_preview?muted_leaf:panel2);
 text(14,181,.43f,cream,stim_preview?"A   PAUSE LOCAL ANIMATION":"A   START LOCAL ANIMATION");
 draw_note("X RESET   B BACK  |  ECU UNTOUCHED");
}
/* This overlay demonstrates a confirmation sequence only: no physical output. */
static void draw_demo_confirm(void){
 if(!demo_confirm.phase)return;
 rect(0,0,320,240,soil);
 rect(7,7,306,226,orange);
 rect(11,11,298,218,bg);
 text(20,18,.53f,cream,sensor_capture_confirmation?"SENSOR CAPTURE - REAL ECU":"OUTPUT TEST - UI DEMO");
 rect(20,44,278,2,orange);
 char label[80];shorten(label,sizeof(label),demo_label,31);
 text(20,54,.48f,white,label);
 text(20,78,.35f,orange,sensor_capture_confirmation?
      "STOP ENGINE BEFORE SENSOR CALIBRATION.":"CONNECTED HARDWARE CAN BE DANGEROUS.");
 text(20,95,.35f,cream,sensor_capture_confirmation?
      "CAPTURES CURRENT TPS / PEDAL POSITION.":"THIS DEMO NEVER SENDS AN OUTPUT COMMAND.");
 text(20,112,.35f,secondary,sensor_capture_confirmation?
      "ECU ACK IS NOT CALIBRATION READBACK.":"Simulates the user's approval sequence.");
 round_panel(18,140,284,58,5,panel2);
 if(demo_confirm.phase==BTS_DEMO_ENABLE){
    text(27,147,.47f,bright,sensor_capture_confirmation?"STEP 1 / 2    A  ENABLE CAPTURE":"STEP 1 / 2    A  ENABLE DEMO");
    text(27,174,.37f,secondary,sensor_capture_confirmation?"Position sensor before confirming":"No ECU output will be activated");
 }else{
    text(27,147,.45f,bright,"STEP 2 / 2    HOLD L + R");
    text(27,174,.36f,secondary,demo_confirm.shoulders_released?
         "Both shoulder buttons: hold together":
         "Release both shoulder buttons first");
 }
 text(20,211,.37f,cream,sensor_capture_confirmation?"B CANCEL  |  Captures sensor value only":"B CANCEL  |  No hardware actuation");
}
static void draw_hazard_confirm(void){
 if(hazardous_test.kind==BRICK_TEST_NONE)return;
 /* Full-screen overlay on 320x240 lower display; no clipped title, no
  * background actions, and no tap or A-button activation. */
 rect(0,0,320,240,soil);
 rect(7,7,306,226,berry);
 rect(11,11,298,218,bg);
 text(19,18,.56f,cream,"ENGINE TEST WARNING");
 rect(19,45,281,2,orange);
 const char *what=hazardous_test.kind==BRICK_TEST_TRIGGER?"TRIGGER STIMULATOR":
                  hazardous_test.kind==BRICK_TEST_INJECTOR?"INJECTOR TEST":"IGNITION TEST";
 text(19,53,.43f,orange,what);
 text(19,82,.36f,cream,"CONNECTED HARDWARE MAY MOVE, SPARK,");
 text(19,98,.36f,cream,"INJECT FUEL OR DAMAGE THE ENGINE.");
 text(19,119,.35f,secondary,"Bench only: no ECU 12V power supply.");
 text(19,136,.35f,secondary,"Live RPM & battery checked again.");
 rect(19,160,282,43,panel);
 text(28,166,.39f,bright,hazardous_test.released?
      "HOLD L + R TO ACKNOWLEDGE":"RELEASE L + R FIRST");
 text(28,185,.33f,cream,"L+R authorizes one ECU Start request");
 text(19,211,.37f,cream,"B  CANCEL");
}
/* Top-screen fault banner is painted AFTER the active dashboard/map/graph,
 * so it remains visible regardless of the current view. It cannot be
 * dismissed by B; it clears only after two clean ECU error responses. */
static void fault_draw_lines(float x,float y,const char *message,int width,int maxlines,float scale){
 if(!message)return;
 const char *p=message;
 for(int line=0;line<maxlines&&*p;line++){
    char buf[70];int n=0,lastspace=-1;
    while(p[n]&&n<width&&n<(int)sizeof(buf)-1){if(p[n]==' ')lastspace=n;n++;}
    if(p[n]&&n==width&&lastspace>6)n=lastspace;
    memcpy(buf,p,(size_t)n);buf[n]=0;
    text(x,y+line*20,scale,cream,buf);
    p+=n;while(*p==' ')p++;
 }
}
static void draw_fault_top_overlay(void){
 if(!fault_active||!fault_payload_valid(fault_message))return;
 if(ui_page==PAGE_FAULT){
    draw_depth_grid(400,240);rect(0,0,400,30,berry);
    text(10,3,.72f,white,"ECU CONFIGURATION ERROR");
    fault_draw_lines(12,43,fault_message,36,7,.51f);
     bts_mascot_draw(282,38,.82f);
    text(12,205,.35f,bright,"A OPEN RELATED SETTING / B RETURN");
    if(fault_last_success && osGetTime()-fault_last_success>8000)
       text(12,224,.32f,orange,"Connection stale; error not confirmed cleared");
    return;
 }
 /* Non-modal status strip; never hides a map or obstructs +VALUE/Y. */
 rect(0,214,400,26,berry);
 rect(2,216,396,22,soil);
 text(7,218,.40f,orange,"ECU ERROR - SELECT+Y FOR FIX");
 if(bts_mascot_notice_active(osGetTime()))bts_mascot_draw(314,119,.56f);
 if(fault_last_success && osGetTime()-fault_last_success>8000)
    text(319,220,.30f,orange,"STALE");
}
static void draw_fault_deck(void){
 text(10,31,.51f,cream,fault_active?"ECU ERROR: REPAIR OPTIONS":"NO ACTIVE ECU ERROR");
 if(!fault_active){text(13,64,.39f,green,"ECU reports no active fault");draw_note("B BACK");return;}
 bool pin=fault_contains(fault_message,"pin")||fault_contains(fault_message,"conflict")||fault_contains(fault_message,"used by");
 text(10,53,.38f,secondary,!fault_link_count?"No exact INI match - browse related ECU categories":
      (pin?"A opens pin assignment / related INI setting":"A opens the linked ECU configuration setting"));
 for(int i=0;i<fault_link_count;i++){
    int y=62+i*18;
    rect(9,y,302,17,i==selected?muted_leaf:panel);
    char line[75];snprintf(line,sizeof(line),"%d. %.57s",i+1,fault_links[i].label);
    text(14,y+3,.42f,i==selected?bright:cream,line);
 }
 int y=62+fault_link_count*18;
 rect(9,y,302,17,selected==fault_link_count?muted_leaf:panel);
 text(14,y+3,.43f,selected==fault_link_count?bright:cream,"Browse related ECU tuning category");
 y+=18;
 rect(9,y,302,17,selected==fault_link_count+1?muted_leaf:panel);
 text(14,y+3,.40f,selected==fault_link_count+1?bright:cream,"Refresh current ECU error");
 text(11,224,.30f,secondary,"A OPEN/REFRESH  X RETRY  B BACK");
}

static void draw_power_cycle_bottom(void){
 if(!power_cycle_popup)return;
 rect(0,0,320,240,soil);rect(7,7,306,226,orange);rect(11,11,298,218,bg);
 text(22,26,.68f,orange,"POWER CYCLE REQUIRED");
 rect(20,55,281,2,orange);
 text(20,75,.50f,white,"CALIBRATION BURN VERIFIED");
 text(20,110,.46f,cream,"Turn ECU power OFF, then ON");
 text(20,132,.41f,secondary,"and wait for ECU reconnection.");
 rect(20,171,280,44,muted_leaf);
 text(40,180,.60f,bright,"A  ACKNOWLEDGE");
 text(34,217,.28f,secondary,"Manual acknowledgement; not power detection");
}
static void draw_power_cycle_top(void){
 if(!power_cycle_popup)return;
 rect(0,0,400,240,soil);
 rect(8,9,384,222,orange);rect(12,13,376,214,bg);
 text(32,37,.86f,orange,"ECU POWER CYCLE");
 text(45,77,.56f,white,"REQUIRED AFTER BURN");
 text(38,125,.49f,cream,"Switch ECU power OFF, then ON.");
 text(55,156,.40f,secondary,"This prompt stays until A is pressed.");
 text(83,195,.51f,bright,"LOWER SCREEN: PRESS A");
}
static void draw_bottom(void){draw_depth_grid(320,240);draw_topbar();
 /* When reading a tune rather than confirming edits, use the whole lower LCD
  * for live gauges after inactivity. Any control wakes the existing page. */
 if(tune_screen_asleep&&ui_page!=PAGE_TUNE_TREE&&ui_page!=PAGE_SOUND_TEST){
    draw_tune_idle_gauges_bottom();return;
 }
 if(ui_page==PAGE_FAULT)draw_fault_deck();else if(ui_page==PAGE_TRIGGER_SIM)draw_trigger_deck();else if(ui_page==PAGE_KNOCK)draw_knock_deck();else if(ui_page==PAGE_DASH)draw_dashboard_deck();
 else if(ui_page==PAGE_REPLAY)draw_replay_deck();else if(ui_page==PAGE_COMPARE)draw_compare_deck();else if(ui_page==PAGE_QUALITY)draw_quality_deck();else if(ui_page==PAGE_INSIGHTS)draw_insights_deck();else if(ui_page==PAGE_REPLAY_STATS)draw_replay_stats_deck();else if(ui_page==PAGE_CHANNELS)draw_channel_deck();
 else if(ui_page==PAGE_DIAG)draw_diag();else if(ui_page==PAGE_SOUND_TEST)draw_sound_test();else if(ui_page==PAGE_DISPLAY)draw_display();else if(ui_page==PAGE_GAUGE_PICK)draw_channel_picker();else if(ui_page==PAGE_GRAPH)draw_graph_deck();else if(ui_page==PAGE_INI_GRAPH)draw_ini_graph_deck();else if(ui_page==PAGE_RECORDER)draw_recorder();else if(in_map_view()&&current_table>=0&&tables[current_table].curve)draw_curve_deck();
 else if(in_map_view())draw_map_deck();else if(ui_page==PAGE_FIELD)draw_field();else draw_list();
 if(hazardous_test.kind)draw_hazard_confirm();
 if(demo_confirm.phase)draw_demo_confirm();
 if(log_delete_pending && ui_page==PAGE_LOG_BROWSER){
    round_panel(8,59,304,134,7,panel2);
    text(18,67,.62f,white,"DELETE THIS SAVED LOG?");
    text(18,101,.46f,cream,log_delete_name);
    text(18,139,.44f,orange,"A CONFIRM  /  B CANCEL");
    text(18,163,.36f,secondary,"Removes CSV, marks and session notes");
 }
 draw_power_cycle_bottom();}
static void touch_input(void){touchPosition tp;hidTouchRead(&tp);
 if(log_delete_pending && ui_page==PAGE_LOG_BROWSER){
   if(tp.py>=130&&tp.py<186){if(tp.px<162)log_delete_confirm();else log_delete_pending=false;}
   return;
 }
 if(ui_page==PAGE_LOG_BROWSER&&tp.py>=206){log_delete_request(selected);return;}
 if(tp.py<28){
    if(fault_active&&tp.px>265){fault_open_page();return;}
    if(tp.px<40)back();
     return;
 }
 if(ui_page==PAGE_SOUND_TEST){
    if(tp.py>=66 && tp.py<210){
       int idx=scroll_y+(tp.py-66)/24;
       if(idx>=0&&idx<BTS_SOUND_COUNT){selected=idx;bts_soundboard_play(idx);}
    }
    return;
 }
 if(ui_page==PAGE_TUNE_TREE){
    if(tp.py>=30&&tp.py<30+7*25){
       int idx=tune_lower_scroll+(tp.py-30)/25;
       if(idx<tune_preview_total(tune_bucket,tune_open_group)){
          if(tune_lower_focus && idx==tune_lower_selected)tune_enter_preview();
          else {
            /* Highlighting previews the row only. Play contextual audio when
             * the user opens it, never on touch-focus or cursor movement. */
            tune_lower_focus=true;tune_lower_selected=idx;
          }
       }
    }
    return;
 }
 if(ui_page==PAGE_DETAIL){
     if(tp.py>=58&&tp.py<208){
        bool spatial=bts_spatial_layout();
        int at=spatial?0:config_group_start(config_group_of(selected));
        int until=spatial?result_count:config_group_end(at);
        int hit=at+scroll_y+(tp.py-58)/25;
        if(hit>=at&&hit<until){
           if(selected==hit)enter_selected();else selected=hit;
        }
     }
     return;
  }
  if(ui_page==PAGE_GROUP){
     if(tp.py>=208)enter_selected();
     return;
  }
 if(tune_context_page()){
    if(tp.py>=117&&tp.py<151){if(tp.px<160)enter_selected();else back();}
    return;
 }
 if(workspace_page(ui_page)){
    int root=ui_page==PAGE_HUB;
    if(root){
       if(tp.px>=8&&tp.px<312&&tp.py>=58&&tp.py<206){
          int col=(tp.px-8)/154,row=(tp.py-58)/74;
          if(col<2&&row<2){int index=row*2+col;selected=index;workspace_enter(PAGE_HUB,index);}
       }
       return;
    }
    int row_y=ui_page==PAGE_ANALYZE_HUB?56:LIST_ROW_Y;
    int row_h=ui_page==PAGE_ANALYZE_HUB?30:LIST_ROW_H;
    int rows=ui_page==PAGE_ANALYZE_HUB?5:LIST_ROWS;
    if(tp.py>=row_y&&tp.py<row_y+rows*row_h){
       int hit=scroll_y+(tp.py-row_y)/row_h;
       if(hit>=0&&hit<workspace_count(ui_page)){
          selected=hit;workspace_enter(ui_page,hit);
       }
    }
    return;
 }
 if((ui_page==PAGE_MENU||ui_page==PAGE_GROUP||ui_page==PAGE_DETAIL) && tp.py>=50 && tp.py<50+6*25){
    int hit=scroll_y+(tp.py-50)/25;
    if(hit>=0&&hit<active_count()){if(hit==selected)enter_selected();else selected=hit;}
    return;
 }
 if(ui_page==PAGE_MAPS && tp.py>=50 && tp.py<50+6*25){
    int hit=scroll_y+(tp.py-50)/25;
    if(hit>=0&&hit<active_count()){selected=hit;enter_selected();}return;
 }
 if((ui_page==PAGE_CATS||ui_page==PAGE_VIEWS||ui_page==PAGE_TRACE_MAPS||ui_page==PAGE_COMPARE_PICK||
     ui_page==PAGE_LOG_BROWSER||ui_page==PAGE_MAPS) && tp.py>=LIST_ROW_Y && tp.py<LIST_ROW_Y+LIST_ROWS*LIST_ROW_H){
    int index=scroll_y+(tp.py-LIST_ROW_Y)/LIST_ROW_H;
    int total=active_count();
    if(index>=0&&index<total){
       bool direct=(ui_page==PAGE_TRACE_MAPS||ui_page==PAGE_COMPARE_PICK||ui_page==PAGE_LOG_BROWSER||ui_page==PAGE_MAPS);
       if(direct||selected==index){selected=index;enter_selected();}
       else selected=index;
    }
    return;
 }
 if(in_map_view()){
    if(map_3d_view){
        /* 3D entries are dedicated INI views, never a hidden 2D switch. */
        return;
    }
    if(current_table>=0&&tables[current_table].curve){
       if(tp.py>=82&&tp.py<172){int n=cal[open_field].width*cal[open_field].height;
          int first=cell-2;if(first<0)first=0;
          if(first+5>n)first=n-5;
           if(first<0)first=0;
          int hit=first+(tp.py-82)/18;if(hit>=0&&hit<n)cell=hit;
       }else if(tp.py>=176&&tp.py<205){
          if(tp.px<107){if(editor_loaded){if(staged)numeric_edit_prompt();else if(cal[open_field].readOnly)note("Read-only INI curve; view only");
                      else {staged=true;note("Local editing enabled");}}}
          else if(tp.px<212)edit_history_move(-1);else edit_history_move(1);
       }return;
    }
    if(map_tools_open){
       if(tp.py>=67&&tp.py<203){int i=map_tool_scroll+(tp.py-67)/34;
          if(i>=0&&i<MAP_TOOL_COUNT){if(i==map_tool_cursor)map_tools_apply();else map_tool_cursor=i;}}
       return;
    }
    if(tp.py>=174&&tp.py<203){
       if(tp.px<107){
          if(!editor_loaded){note("Read map calibration first");return;}
          map_tools_open=true;map_tool_cursor=map_tool_scroll=0;
       }else if(tp.px<213){edit_history_move(-1);}
       else if(staged&&changed()){
          confirm_write=true;note("BURN REVIEW: A confirms / B cancels");
       }else note("No pending changes to burn");
       return;
    }
    if(tp.py>=140&&tp.py<172){
      if(tp.px<107){if(editor_loaded){if(staged)numeric_edit_prompt();
         else if(cal[open_field].readOnly)note("Read-only INI map; view only");
         else {staged=true;confirm_write=false;note("Edit enabled locally; tap ENTER again");}}}
      else if(tp.px<212)adjust_cell(-1.0);
      else adjust_cell(1.0);
    }
    return;
 }
 if(ui_page==PAGE_TRIGGER_SIM){
   if(tp.py>=178&&tp.py<=209){stim_reanchor();stim_preview=!stim_preview;
     note(stim_preview?"Local animation started; ECU unchanged":"Local animation paused; ECU unchanged");}
   return;
 }
 if(ui_page==PAGE_REPLAY_STATS||ui_page==PAGE_INSIGHTS){return;}
 if(ui_page==PAGE_REPLAY){
   if(tp.py>=120&&tp.py<194&&tp.px>=7&&tp.px<312){
      int row=tp.py>=159?1:0,col=(tp.px-7)/103;
      int bx=7+col*103,by=120+row*39;
      if(col<3&&tp.px<bx+99&&tp.py>=by&&tp.py<by+35)
         replay_action(row*3+col);
   }else if(tp.py>=87&&tp.py<=108&&replay_count>0){
      u32 first=replay_buffer[0].t_ms,last=replay_buffer[replay_count-1].t_ms;
      float k=(tp.px-8)/304.0f;if(k<0)k=0;if(k>1)k=1;
      replay_seek(first+(u32)((last-first)*k));
   }else if(tp.py>=130&&tp.py<176){replay_playing=!replay_playing;replay_wall_ms=osGetTime();}
   return;
 }
 if(ui_page==PAGE_CHANNELS){
    if(tp.py>=49&&tp.py<199){int i=channel_scroll+(tp.py-49)/25;
       if(i>=0&&i<selectable_count){selected=channel_cursor=i;channel_live_extend();}}
    return;
 }
 if(ui_page==PAGE_DASH){
    if(tp.py>=107&&tp.py<192&&tp.px>=7&&tp.px<312){
       int row=tp.py>=153?1:0,col=tp.px>=162?1:0;
       int bx=7+col*155,by=107+row*46;
       if(tp.px>=bx+150||tp.py>=by+39)return;
       int index=row*2+col;
       if(index==0){gauge_style=(gauge_style+1)%7;save_gauge_preferences();}
       else if(index==1){gauge_stat_view=(gauge_stat_view+1)%4;}
       else if(index==2){for(int i=0;i<GAUGE_SLOTS;i++)gauge_stats_reset(i);}
       else {display_parent=PAGE_DASH;ui_page=PAGE_DISPLAY;selected=scroll_y=0;}
    }
    return;
 }
 if(ui_page==PAGE_FIELD){
    if(open_field>=0&&open_field<cal_count&&cal[open_field].width*cal[open_field].height==1&&
       tp.py>=181&&tp.py<209&&editor_loaded&&!confirm_discard&&!confirm_write&&
       !write_queued&&!write_inflight){
       if(tp.px<159)edit_history_move(-1);else edit_history_move(1);
    }
    return;
 }
 if(ui_page==PAGE_RECORDER){
    if(tp.py>=129&&tp.py<157){
      if(tp.px<159){if(log_recording){log_recording=false;note("Recording stopped");}
        else {start_logging();}}
      else if(!log_recording)export_csv_log();
    }
    return;
 }
 if(ui_page==PAGE_INI_GRAPH){
   if(tp.py>=183&&tp.py<215){cycle_ini_graph(tp.px<160?-1:1);}
   return;
 }
 if(ui_page==PAGE_GRAPH){
   if(tp.py>=165&&tp.py<198)graph_bank=(graph_bank+(tp.px<160?2:1))%3;
   return;
 }
 if(ui_page==PAGE_DISPLAY||ui_page==PAGE_GAUGE_PICK){
   int total=active_count();if(tp.py>=LIST_ROW_Y&&tp.py<LIST_ROW_Y+LIST_ROWS*LIST_ROW_H){
     int i=scroll_y+(tp.py-LIST_ROW_Y)/LIST_ROW_H;
     if(i<total){if(selected==i)enter_selected();else selected=i;}}
   return;
 }
 int total=active_count();
 if(total>LIST_ROWS&&tp.px>=305&&tp.py>=LIST_ROW_Y&&tp.py<LIST_ROW_Y+LIST_ROWS*LIST_ROW_H){
    int last=total-LIST_ROWS;
    scroll_y=(int)(((float)(tp.py-LIST_ROW_Y)/(float)(LIST_ROWS*LIST_ROW_H))*last+.5f);
    if(scroll_y<0)scroll_y=0;
     if(scroll_y>last)scroll_y=last;
    selected=scroll_y;return;
 }
 if(total&&tp.py>=LIST_ROW_Y&&tp.py<LIST_ROW_Y+LIST_ROWS*LIST_ROW_H){
   int i=scroll_y+(tp.py-LIST_ROW_Y)/LIST_ROW_H;
   if(i<total){if(selected==i)enter_selected();else selected=i;}}

}
static void tune_switch_bucket(int direction){
 if(!(ui_page==PAGE_MENU||ui_page==PAGE_GROUP||ui_page==PAGE_DETAIL||ui_page==PAGE_MAPS))return;
 int next=tune_bucket;
 for(int i=0;i<TUNE_BUCKET_COUNT;i++){
   next=(next+direction+TUNE_BUCKET_COUNT)%TUNE_BUCKET_COUNT;
   if(tune_section_count(next)>0)break;
 }
 if(tune_section_count(next)<=0)return;
 nav_capture();
 tune_bucket=next;ui_page=PAGE_TUNE_TREE;selected=next;scroll_y=0;
 enter_selected();
}
static void handle_keys(u32 k){
 if(demo_confirm.phase){
    if(osGetTime()-demo_started_ms>20000){
       bts_demo_reset(&demo_confirm);
       sensor_capture_confirmation=false;
       note("Confirmation expired; no command was sent");
       return;
    }
    int result=bts_demo_input(&demo_confirm,k,hidKeysHeld(),
                              KEY_A,KEY_L,KEY_R,KEY_B);
    if(result<0){
       sensor_capture_confirmation=false;
       note("Confirmation cancelled; no command sent");
    }else if(result==2)
       note("Enabled: release L/R, then hold both to confirm");
    else if(result>0){
       if(sensor_capture_confirmation){
          sensor_capture_confirmation=false;
          if(!net_ok||!ecu_session||!ecu_signature[0]||!menu_loaded||
             strcmp(ini_signature,ecu_signature)||sensor_capture_pending||
             sensor_capture_inflight){
             note("Capture cancelled: ECU session changed or request busy");
          }else{
             copy_text(sensor_capture_command,sizeof(sensor_capture_command),demo_command);
             sensor_capture_pending=true;
             note("Sensor capture queued: awaiting real ECU acknowledgement");
          }
       }else{
          demo_completed_count++;
          note("LOCAL OUTPUT DEMO COMPLETE; NO HARDWARE COMMAND SENT");
          if(net_ok&&ecu_session&&ecu_signature[0]&&menu_loaded&&
             !strcmp(ecu_signature,ini_signature)&&!command_inspect_pending){
             copy_text(command_inspect_name,sizeof(command_inspect_name),demo_command);
             command_inspect_pending=true;
          }
       }
    }
    return;
 }

 if(log_delete_pending){
    if(k&KEY_A)log_delete_confirm();
    else if(k&KEY_B){log_delete_pending=false;note("Log deletion cancelled");}
    return;
 }
 if(power_cycle_popup){
   if(k&KEY_A){power_cycle_popup=false;fault_poll_requested=true;
      status_requested=true;note("Power-cycle reminder acknowledged; checking ECU status");}
   return;
 }
 if(hazardous_test.kind){
   /* Handle warning BEFORE navigation/touch/other commands. Two shoulder
    * buttons only acknowledge it; they never authorize physical actuation. */
   int r=brick_test_confirm_input(&hazardous_test,k,hidKeysHeld(),KEY_L,KEY_R,KEY_B);
   if(r<0){stim_token[0]=0;note("Test cancelled; no ECU Start sent");}
   else if(r>0){
      if(stim_token[0] && stim_start_supported && net_ok && ecu_session){
         stim_pending=STIM_START;
         note("L+R confirmed. ECU safety recheck and real Start queued...");
      }else note("No prepared bench token; ECU Start blocked");
   }
   return;
 }
 /* Permit cancelling an unstarted queue if the Wi-Fi link is unavailable;
  * never interrupt a write that has already started on the worker. */
 if(confirm_discard){
   if(k&KEY_B){back();return;}
   if(k&KEY_A){confirm_discard=false;note("Local changes kept; continue tuning or burn");}
   return;
 }
 if(write_queued){
   if(k&KEY_B){write_queued=false;bts_audio_stop_burn_loop();
      note("Queued burn cancelled; staged edits retained");}
   return;
 }
 if(write_inflight)return;
 if(map_tools_open){
    if(k&KEY_B){map_tools_open=false;return;}
    if(k&(KEY_DUP|KEY_CPAD_UP)){if(map_tool_cursor>0)map_tool_cursor--;}
    if(k&(KEY_DDOWN|KEY_CPAD_DOWN)){if(map_tool_cursor<MAP_TOOL_COUNT-1)map_tool_cursor++;}
    if(k&KEY_L)step_index=(step_index+TUNE_STEP_COUNT-1)%TUNE_STEP_COUNT;
    if(k&KEY_R)step_index=(step_index+1)%TUNE_STEP_COUNT;
    if(k&KEY_A)map_tools_apply();
    if(k&KEY_TOUCH)touch_input();
    return;
 }
 /* Y is +VALUE inside map editors. Fault shortcuts must never steal it. */
 if(fault_active && (k&KEY_Y) && (hidKeysHeld()&KEY_SELECT) && ui_page!=PAGE_FAULT){fault_open_page();return;}
 if(k&KEY_TOUCH && ui_page==PAGE_FAULT){
   touchPosition tp;hidTouchRead(&tp);
   if(tp.py<28&&tp.px<42){back();return;}
   int n=fault_link_count+2;
   if(tp.py>=62&&tp.py<62+n*18){int hit=(tp.py-62)/18;
      if(hit>=0&&hit<n){selected=hit;enter_selected();}}
   return;
 }
 if(k&KEY_TOUCH){if(ui_page!=PAGE_SOUND_TEST)sfx_play(0);touch_input();}
 if(k&KEY_B){if(ui_page!=PAGE_SOUND_TEST)sfx_play(2);back();return;}
 if(ui_page==PAGE_HUB){
    if(k&(KEY_DLEFT|KEY_CPAD_LEFT))if(selected%2==1)selected--;
    if(k&(KEY_DRIGHT|KEY_CPAD_RIGHT))if(selected%2==0)selected++;
    if(k&(KEY_DUP|KEY_CPAD_UP))if(selected>=2)selected-=2;
    if(k&(KEY_DDOWN|KEY_CPAD_DOWN))if(selected<=1)selected+=2;
    if(k&KEY_A)enter_selected();
    return;
 }
 if(ui_page==PAGE_FAULT){
   int n=fault_link_count+2;
     if(k&(KEY_DUP|KEY_CPAD_UP))if(selected>0)selected--;
   if(k&(KEY_DDOWN|KEY_CPAD_DOWN))if(selected<n-1)selected++;
   if(k&KEY_A)enter_selected();
   if(k&KEY_X){fault_poll_requested=true;note("Refreshing ECU fault status...");}
   return;
 }
 if(ui_page==PAGE_TRIGGER_SIM){
   if(k&KEY_A){stim_reanchor();stim_preview=!stim_preview;
      note(stim_preview?"Local waveform running; ECU untouched":"Local waveform paused");}
   if(k&KEY_X){stim_preview=false;stim_anchor_angle=0;stim_anchor_time=osGetTime();
      stim_rpm=800;stim_wheel=0;note("Offline preview reset; ECU unchanged");}
   if(k&KEY_L){stim_wheel=(stim_wheel+DREAM_WHEEL_COUNT-1)%DREAM_WHEEL_COUNT;}
   if(k&KEY_R){stim_wheel=(stim_wheel+1)%DREAM_WHEEL_COUNT;}
   if(k&(KEY_DUP|KEY_CPAD_UP)){stim_reanchor();stim_rpm=dream_clamp_rpm(stim_rpm+100);}
   if(k&(KEY_DDOWN|KEY_CPAD_DOWN)){stim_reanchor();stim_rpm=dream_clamp_rpm(stim_rpm-100);}
   if(k&(KEY_DLEFT|KEY_CPAD_LEFT)){stim_reanchor();stim_rpm=dream_clamp_rpm(stim_rpm-10);}
   if(k&(KEY_DRIGHT|KEY_CPAD_RIGHT)){stim_reanchor();stim_rpm=dream_clamp_rpm(stim_rpm+10);}
   return;
 }
 if(ui_page==PAGE_KNOCK){if(k&KEY_L){knock_bank=0;sfx_play(0);}if(k&KEY_R){knock_bank=1;sfx_play(0);}return;}
 if(ui_page==PAGE_DASH){
    if(k&KEY_Y){gauge_stat_view=(gauge_stat_view+1)%4;note("Digital gauge display: live / max / min / mean");}
    if(k&KEY_X){for(int i=0;i<GAUGE_SLOTS;i++)gauge_stats_reset(i);note("Gauge historical statistics reset");}
    if(k&KEY_L){gauge_style=(gauge_style+6)%7;save_gauge_preferences();sfx_play(0);}
    if(k&KEY_R){gauge_style=(gauge_style+1)%7;save_gauge_preferences();sfx_play(0);}
    if(k&KEY_SELECT){ui_page=PAGE_HUB;selected=scroll_y=0;sfx_play(1);}
    return;
 }
 if(ui_page==PAGE_DISPLAY && selected>=1 && selected<=9 && (k&(KEY_X|KEY_Y))){
   int i=selected-1;
   /* Cycle AUTO, 0, 1, 2 and 3 decimal places, saved to SD. */
   int p=gauge_decimals[i];
   if(k&KEY_X)p=p==3?-1:p+1;
   else p=p==-1?3:p-1;
   gauge_decimals[i]=p;save_gauge_preferences();
   note("Gauge numeric precision saved to SD");return;
 }
 if(ui_page==PAGE_REPLAY_STATS){
   if(k&(KEY_DLEFT|KEY_CPAD_LEFT)){if(replay_stat_cursor>0)replay_stat_cursor--;}
   if(k&(KEY_DRIGHT|KEY_CPAD_RIGHT)){if(replay_stat_cursor<8)replay_stat_cursor++;}
   if(k&(KEY_DUP|KEY_CPAD_UP)){if(replay_stat_cursor>=3)replay_stat_cursor-=3;}
   if(k&(KEY_DDOWN|KEY_CPAD_DOWN)){if(replay_stat_cursor<6)replay_stat_cursor+=3;}
   if(k&KEY_A){ui_page=PAGE_REPLAY;replay_playing=false;}
   return;
 }
 if(ui_page==PAGE_QUALITY){
    if(k&KEY_L)quality_channel=(quality_channel+LOG_FIELDS-1)%LOG_FIELDS;
    if(k&KEY_R)quality_channel=(quality_channel+1)%LOG_FIELDS;
    if(k&KEY_A)quality_jump_gap();
    if(k&KEY_X)quality_export();
    return;
 }
 if(ui_page==PAGE_INSIGHTS){
    if(k&KEY_SELECT){quality_return_page=PAGE_INSIGHTS;quality_open();return;}
    if(k&KEY_L){insight_channel=(insight_channel+LOG_FIELDS-1)%LOG_FIELDS;insight_dirty=true;}
    if(k&KEY_R){insight_channel=(insight_channel+1)%LOG_FIELDS;insight_dirty=true;}
    if(k&KEY_X)insight_view^=1;
    if(k&KEY_Y){insight_window=(insight_window+1)%4;insight_dirty=true;}
    if(k&KEY_A){insights_jump_event(false);return;}
    if(k&KEY_DDOWN){insights_jump_event(true);return;}
    if(k&KEY_START)insights_export();
    return;
 }
 if(ui_page==PAGE_REPLAY){
   if(k&KEY_START){analysis_return_page=PAGE_REPLAY;insights_open();return;}
   if(k&KEY_Y){trace_return_page=PAGE_REPLAY;replay_open_trace();return;}
   if(k&KEY_SELECT){replay_mark_toggle();return;}
   if(k&(KEY_DUP|KEY_CPAD_UP)){replay_mark_jump(-1);return;}
   if(k&(KEY_DDOWN|KEY_CPAD_DOWN)){replay_mark_jump(1);return;}
   if(k&KEY_X){if(hidKeysHeld()&KEY_L){compare_return_page=PAGE_REPLAY;compare_open_picker();return;}
      stats_return_page=PAGE_REPLAY;replay_playing=false;ui_page=PAGE_REPLAY_STATS;return;}
   if(k&KEY_A){replay_playing=!replay_playing;replay_wall_ms=osGetTime();}
   if(k&KEY_L){replay_speed=replay_speed==1?4:replay_speed/2;replay_wall_ms=osGetTime();}
   if(k&KEY_R){replay_speed=replay_speed==4?1:replay_speed*2;replay_wall_ms=osGetTime();}
   if(k&(KEY_DLEFT|KEY_CPAD_LEFT)){u32 t=replay_time_ms;replay_seek(t>=1000?t-1000:0);}
   if(k&(KEY_DRIGHT|KEY_CPAD_RIGHT)){u32 t=replay_time_ms;replay_seek(t>UINT32_MAX-1000?UINT32_MAX:t+1000);}
   return;
 }
 if(ui_page==PAGE_TRACE_MAPS){
   if(k&(KEY_DUP|KEY_CPAD_UP)){if(selected>0)selected--;}
   if(k&(KEY_DDOWN|KEY_CPAD_DOWN)){if(selected<trace_map_count-1)selected++;}
   if(k&KEY_A){nav_capture();replay_open_selected_map(selected);}
   return;
 }
 if(ui_page==PAGE_COMPARE){
   if(k&KEY_L){compare_channel=(compare_channel+LOG_FIELDS-1)%LOG_FIELDS;}
   if(k&KEY_R){compare_channel=(compare_channel+1)%LOG_FIELDS;}
   if(k&KEY_X){compare_window=(compare_window+1)%3;}
   if(k&KEY_Y){compare_open_picker();return;}
   if(k&KEY_START){compare_export();return;}
   if(k&(KEY_DLEFT|KEY_CPAD_LEFT)){
      u32 t=replay_time_ms;replay_seek(t>=1000?t-1000:0);
   }
   if(k&(KEY_DRIGHT|KEY_CPAD_RIGHT)){
      u32 t=replay_time_ms;replay_seek(t>UINT32_MAX-1000?UINT32_MAX:t+1000);
   }
   return;
 }
 if(ui_page==PAGE_COMPARE_PICK){
    if(k&(KEY_DUP|KEY_CPAD_UP)){if(selected>0)selected--;}
    if(k&(KEY_DDOWN|KEY_CPAD_DOWN)){if(selected<log_file_count-1)selected++;}
    if(k&KEY_A){compare_pick_selected=selected;compare_open_log(selected);}
    return;
 }
 if(ui_page==PAGE_LOG_BROWSER){
   if(k&(KEY_DUP|KEY_CPAD_UP)){if(selected>0)selected--;}
   if(k&(KEY_DDOWN|KEY_CPAD_DOWN)){if(selected<log_file_count-1)selected++;}
   log_browser_selected=selected;
   if(k&KEY_Y){log_delete_request(selected);return;}
   if(k&KEY_A){open_saved_log(selected);}
   if(k&KEY_X)refresh_log_files();
   return;
 }
 if(ui_page==PAGE_CHANNELS){
   if(k&(KEY_DUP|KEY_CPAD_UP)){if(channel_cursor>0)channel_cursor--;}
   if(k&(KEY_DDOWN|KEY_CPAD_DOWN)){if(channel_cursor<selectable_count-1)channel_cursor++;}
   channel_live_extend();selected=channel_cursor;
   return;
 }
 if(ui_page==PAGE_RECORDER){
   if(k&KEY_A){
     if(log_recording){log_recording=false;note("Recording stopped. X exports CSV to SD");}
     else {start_logging();}
   }
   if((k&KEY_X)&&!log_recording)export_csv_log();
   if((k&KEY_Y)&&!log_recording){log_count=0;note("Log samples cleared");}
   return;
 }
 if(ui_page==PAGE_FIELD&&replay_trace_active&&!staged&&(k&KEY_X)&&!(hidKeysHeld()&KEY_L)){
    trace_show_coverage=!trace_show_coverage;
    note(trace_show_coverage?"Building real log coverage heatmap (incremental)":"Showing actual ECU calibration heatmap");
    return;
 }
 if(ui_page==PAGE_CHOICES&&(k&(KEY_X|KEY_Y))){
    edit_history_move((k&KEY_X)?-1:1);return;
 }
 if(ui_page==PAGE_FIELD&&(hidKeysHeld()&KEY_L)&&(k&(KEY_X|KEY_Y))){
    edit_history_move((k&KEY_X)?-1:1);return;
 }
 if(ui_page==PAGE_TUNE_TREE){
   if(k&KEY_SELECT){ui_page=PAGE_HUB;selected=scroll_y=0;tune_reset_preview();return;}
   if(k&KEY_X){
      nav_capture();favorites_parent=PAGE_TUNE_TREE;
      tune_from_tree=false;favorites_origin=false;search_origin=false;
      ui_page=PAGE_FAVORITES;selected=scroll_y=0;return;
   }
   if(k&KEY_Y){
      if(!tune_lower_focus){note("Focus a lower-screen setting first");return;}
      int idx=tune_preview_index(tune_bucket,tune_open_group,tune_lower_selected);
      if(idx>=0&&idx<menu_count&&menus[idx].group!=1&&*menus[idx].target)
          favorite_toggle(menus[idx].target,menus[idx].label);
      else note("Only INI settings can be starred");
      return;
   }
   if(!tune_lower_focus){
      int count=tune_visible_buckets();
      int prev=tune_bucket;
      if(k&(KEY_DUP|KEY_CPAD_UP|KEY_CSTICK_UP))tune_bucket-=2;
      if(k&(KEY_DDOWN|KEY_CPAD_DOWN|KEY_CSTICK_DOWN))tune_bucket+=2;
      if(k&(KEY_DLEFT|KEY_CPAD_LEFT|KEY_L|KEY_CSTICK_LEFT))tune_bucket--;
      if(k&(KEY_DRIGHT|KEY_CPAD_RIGHT|KEY_R|KEY_CSTICK_RIGHT))tune_bucket++;
      if(tune_bucket<0)tune_bucket=0;
      if(tune_bucket>=count)tune_bucket=count-1;
      if(prev!=tune_bucket){tune_lower_selected=tune_lower_scroll=0;tune_open_group=-1;}
      selected=tune_bucket;
   }else{
      int count=tune_preview_total(tune_bucket,tune_open_group);
      if(k&(KEY_DUP|KEY_CPAD_UP))tune_lower_selected--;
      if(k&(KEY_DDOWN|KEY_CPAD_DOWN))tune_lower_selected++;
      if(k&(KEY_CSTICK_UP|KEY_L))tune_lower_selected-=5;
      if(k&(KEY_CSTICK_DOWN|KEY_R))tune_lower_selected+=5;
      if(tune_lower_selected<0)tune_lower_selected=0;
      if(tune_lower_selected>=count)tune_lower_selected=count>0?count-1:0;
      /* No contextual sound on D-pad, Circle Pad, or C-stick selection.
       * Selection is free to browse; A opens an entry and triggers its cue. */
   }
   if(k&KEY_A){sfx_play(1);tune_enter_preview();}
   return;
 }
 if(in_map_view()&&map_3d_view&&(k&(KEY_X|KEY_Y)))return;
 if(k&KEY_Y){if(ui_page==PAGE_VIEWS)return;if(ui_page==PAGE_CHOICES)return;
  if(ui_page==PAGE_DETAIL){toggle_selected_favorite();return;}
 if(ui_page==PAGE_CATS||ui_page==PAGE_SEARCH){search_prompt();return;}
 if(ui_page==PAGE_MENU||ui_page==PAGE_GROUP||ui_page==PAGE_MAPS||ui_page==PAGE_FAVORITES){toggle_selected_favorite();return;}
 if(ui_page==PAGE_FIELD){if(staged)adjust_cell(1.0);else {pending_cal_field=open_field;cal_requested=true;note("Queued calibration refresh");}}else {ini_refresh=true;note("Queued INI refresh (SD cache retained)");}return;}
 if(k&KEY_X){if(ui_page==PAGE_VIEWS)return;if(ui_page==PAGE_CHOICES)return;
 if(ui_page==PAGE_FIELD){if(staged)adjust_cell(-1.0);else {pending_cal_field=open_field;cal_requested=true;note("Queued calibration refresh");}}else if(ui_page==PAGE_DIAG){worker_reconnect=true;note("Wi-Fi reconnect queued");}else {note("Use Tune tree to access ECU maps and curves");}return;}
 if(ui_page!=PAGE_FIELD && (k&KEY_SELECT) && !write_queued && !write_inflight){
     if(ui_page!=PAGE_HUB){nav_capture();ui_page=PAGE_HUB;selected=scroll_y=0;sfx_play(1);return;}
 }
 if(ui_page==PAGE_INI_GRAPH){
    if(k&KEY_L)cycle_ini_graph(-1);
    if(k&KEY_R)cycle_ini_graph(1);
    if(k&KEY_X)graph_seconds=graph_seconds==20?5:graph_seconds==5?10:20;
    return;
 }
 if(ui_page==PAGE_GRAPH){
    if(k&KEY_L)graph_bank=(graph_bank+2)%3;
    if(k&KEY_R)graph_bank=(graph_bank+1)%3;
    if(k&KEY_X)graph_seconds=graph_seconds==5?10:graph_seconds==10?20:5;
    return;
 }
 if(ui_page==PAGE_DETAIL){
    bool spatial=bts_spatial_layout();
    if(spatial){
       if(k&(KEY_L|KEY_DLEFT|KEY_CPAD_LEFT))selected=bts_next_region_field(-1);
       if(k&(KEY_R|KEY_DRIGHT|KEY_CPAD_RIGHT))selected=bts_next_region_field(1);
    }else{
       if(k&(KEY_L|KEY_DLEFT|KEY_CPAD_LEFT))config_group_shift(-1);
       if(k&(KEY_R|KEY_DRIGHT|KEY_CPAD_RIGHT))config_group_shift(1);
    }
    int group=config_group_of(selected);
    int at=spatial?0:config_group_start(group);
    int end=spatial?result_count:config_group_end(at);
    if(k&(KEY_DUP|KEY_CPAD_UP))if(selected>at)selected--;
    if(k&(KEY_DDOWN|KEY_CPAD_DOWN))if(selected<end-1)selected++;
    if(k&KEY_CSTICK_UP)selected=selected-5>=at?selected-5:at;
    if(k&KEY_CSTICK_DOWN)selected=selected+5<end?selected+5:end-1;
    if(k&KEY_A){sfx_play(1);enter_selected();}
    return;
 }
 if(ui_page==PAGE_TUNE_SECTION){
    /* Compatibility with older saved screens: take users into the new tree. */
    ui_page=PAGE_TUNE_TREE;selected=tune_bucket;tune_reset_preview();return;
 }
 if((ui_page==PAGE_MENU||ui_page==PAGE_GROUP||ui_page==PAGE_DETAIL||ui_page==PAGE_MAPS)&&
    (k&(KEY_L|KEY_R)) && !tune_from_tree){tune_switch_bucket(k&KEY_L?-1:1);return;}
 if(ui_page!=PAGE_VIEWS&&ui_page!=PAGE_DIAG&&ui_page!=PAGE_GAUGE_PICK&&ui_page!=PAGE_FIELD && (k&(KEY_L|KEY_R))){
    gauge_style=(gauge_style+((k&KEY_L)?6:1))%7;
    save_gauge_preferences();
    if(ui_page!=PAGE_DISPLAY)note("Gauge layout updated - SELECT for display setup");
    return;
 }
 if(ui_page==PAGE_FIELD){
 if(in_map_view()&&current_table>=0&&!tables[current_table].curve){
#ifdef KEY_ZR
    /* INI determines the map view. ZR no longer creates a duplicate. */
#endif
    if(map_3d_view){
        if(k&(KEY_DUP|KEY_CPAD_UP|KEY_CSTICK_UP))map_3d_pitch+=.05f;
        if(k&(KEY_DDOWN|KEY_CPAD_DOWN|KEY_CSTICK_DOWN))map_3d_pitch-=.05f;
        if(k&(KEY_DLEFT|KEY_CPAD_LEFT|KEY_CSTICK_LEFT))map_3d_yaw-=.07f;
        if(k&(KEY_DRIGHT|KEY_CPAD_RIGHT|KEY_CSTICK_RIGHT))map_3d_yaw+=.07f;
        if(map_3d_pitch<MAP3D_PITCH_MIN)map_3d_pitch=MAP3D_PITCH_MIN;
        if(map_3d_pitch>MAP3D_PITCH_MAX)map_3d_pitch=MAP3D_PITCH_MAX;
        /* Viewing is read-only: A does not silently open another editor. */
        return;
    }
 }
 if(k&KEY_CSTICK_LEFT)step_index=(step_index+TUNE_STEP_COUNT-1)%TUNE_STEP_COUNT;
 if(k&KEY_CSTICK_RIGHT)step_index=(step_index+1)%TUNE_STEP_COUNT;
 if((k&KEY_CSTICK_UP)&&cal[open_field].width*cal[open_field].height>1)
    move_cell(current_table>=0&&tables[current_table].curve?-5:0,
              current_table>=0&&tables[current_table].curve?0:-5);
 if((k&KEY_CSTICK_DOWN)&&cal[open_field].width*cal[open_field].height>1)
    move_cell(current_table>=0&&tables[current_table].curve?5:0,
              current_table>=0&&tables[current_table].curve?0:5);
 if((k&KEY_SELECT)&&editor_loaded){
    if(cal[open_field].readOnly){note("Read-only INI field; view only");}
    else {staged=!staged;confirm_write=false;
       note(staged?"Edit staged locally; ECU unchanged":"Edit mode off; staged bytes retained");}
 }
 if(k&(KEY_DUP|KEY_CPAD_UP))move_cell(0,-1);
 if(k&(KEY_DDOWN|KEY_CPAD_DOWN))move_cell(0,1);
 if(k&(KEY_DLEFT|KEY_CPAD_LEFT))move_cell(-1,0);
 if(k&(KEY_DRIGHT|KEY_CPAD_RIGHT))move_cell(1,0);
 if(k&KEY_L)step_index=(step_index+TUNE_STEP_COUNT-1)%TUNE_STEP_COUNT;
  if(k&KEY_R)step_index=(step_index+1)%TUNE_STEP_COUNT;
 if(k&KEY_A){
     if(confirm_write)request_burn();
     else if(cal[open_field].width*cal[open_field].height>1){
        if(replay_trace_active&&!staged&&trace_found){
           Field *f=&cal[open_field];
           if(tracex>=0&&tracex<f->width&&tracey>=0&&tracey<f->height){
              cell=tracey*f->width+tracex;map_focus=0;
              note("Cursor positioned at the real log-traced cell; SELECT to edit");
           }else note("Log cursor outside current table");
        }else if(staged)numeric_edit_prompt();
        else note("Full map shown; SELECT enables local editing");
     }else if(cal[open_field].kind==K_BITS&&choice_count>0&&editor_loaded){
        int current=(int)field_value(editor,editor_length,&cal[open_field],0);        selected=scroll_y=0;
        for(int i=0;i<choice_count;i++)if(choices[i].value==current){selected=i;break;}
        if(selected>6)scroll_y=selected-6;
        ui_page=PAGE_CHOICES;
     }else if(staged){numeric_edit_prompt();}
     else if(!changed()){
        pending_cal_field=open_field;cal_requested=true;note("Queued calibration refresh");
     }else note("START for bench write confirmation");
 }
 if((k&KEY_START)&&staged&&changed()){confirm_write=true;note("A confirms BENCH write; B cancels");}
 return;}
 if(ui_page==PAGE_SOUND_TEST){
    if(k&(KEY_DUP|KEY_CPAD_UP|KEY_CSTICK_UP))if(selected>0)selected--;
    if(k&(KEY_DDOWN|KEY_CPAD_DOWN|KEY_CSTICK_DOWN))if(selected<BTS_SOUND_COUNT-1)selected++;
    if(k&KEY_A)bts_soundboard_play(selected);
    if(k&KEY_Y && !bts_audio_burn_loop){bts_audio_stop();sound_test_queued=false;
       note("Soundboard preview stopped");}
    if(k&KEY_X){event_audio_enabled=!event_audio_enabled;save_gauge_preferences();
       note(event_audio_enabled?"Conditional event sounds enabled":"Conditional event sounds disabled");}
    return;
 }
 if(ui_page==PAGE_DIAG){if(k&KEY_A)status_requested=true;if(k&KEY_L){wifi_slot=wifi_slot<=0?2:wifi_slot-1;worker_reconnect=true;}if(k&KEY_R){wifi_slot=wifi_slot>=2?0:wifi_slot+1;worker_reconnect=true;}return;}
 int count=active_count();
 if(k&KEY_CSTICK_UP)selected=selected>5?selected-5:0;
 if(k&KEY_CSTICK_DOWN)selected=selected+5<count?selected+5:(count>0?count-1:0);
 if(k&(KEY_DUP|KEY_CPAD_UP)){if(selected>0)selected--;}
 if(k&(KEY_DDOWN|KEY_CPAD_DOWN)){if(selected<count-1)selected++;}
 if(k&(KEY_DLEFT|KEY_CPAD_LEFT)){
    if(ui_page==PAGE_MENU&&menu_return_page==PAGE_TUNE_TREE){tune_switch_bucket(-1);return;}
 }
 if(k&(KEY_DRIGHT|KEY_CPAD_RIGHT)){
    if(ui_page==PAGE_MENU&&menu_return_page==PAGE_TUNE_TREE){tune_switch_bucket(1);return;}
 }
 if(k&KEY_A){sfx_play(1);enter_selected();}
 /* L/R are reserved for quick gauge layout cycling outside map editing. */
}
/* Network thread owns every socket request; only one is in flight.
 * HTTP buffers stay exclusively owned by it until TASK_COMPLETED is consumed
 * by the drawing thread. No selection input can block on network latency. */
static bool task_submit(int type,int page,int offset,int len,int field){
 bool ok=false;
 LightLock_Lock(&task_lock);
 if(task_state==0 && !worker_stop){
   task_type=type;task_page=page;task_offset=offset;task_len=len;task_field=field;
   task_state=1;ok=true;
 }
 LightLock_Unlock(&task_lock);
 return ok;
}
static bool task_idle(void){bool ok;LightLock_Lock(&task_lock);ok=task_state==0;LightLock_Unlock(&task_lock);return ok;}
static void net_thread(void *arg){
 (void)arg;
 if(ac_ok)connect_wifi(-1);
 u64 last_wifi_poll=0;
 while(!worker_stop){
    u64 now=osGetTime();
    if(ac_ok&&now-last_wifi_poll>1300){last_wifi_poll=now;update_wifi_state();}
    if(worker_reconnect){
        worker_reconnect=false;
        if(ac_ok)connect_wifi(wifi_slot);
    }else if(ac_ok&&!wifi_candidate&&!wifi_busy&&now-wifi_last_attempt>16000){
        connect_wifi(wifi_slot);
    }
    int type=TASK_IDLE,page=0,off=0,len=0;
    LightLock_Lock(&task_lock);
    if(task_state==1){task_state=2;type=task_type;page=task_page;off=task_offset;len=task_len;}
    LightLock_Unlock(&task_lock);
    if(type!=TASK_IDLE){
       int result=-1;
       if(type==TASK_STIM_PREPARE||type==TASK_STIM_START||type==TASK_STIM_STOP){
          stim_failure[0]=0;
          result=stim_http_action(type);
          if(result<0)snprintf(stim_failure,sizeof(stim_failure),"%.150s",ui_note);
       }else if(type==TASK_SENSOR_CAPTURE){
          char escaped_signature[320],escaped_command[160],payload[600];
          bool valid=is_sensor_capture_command(sensor_capture_command)&&
                     ecu_session&&ecu_signature[0]&&
                     !strcmp(ecu_signature,ini_signature)&&
                     escape_json_string(ecu_signature,escaped_signature,sizeof(escaped_signature))&&
                     escape_json_string(sensor_capture_command,escaped_command,sizeof(escaped_command));
          if(!valid){
             result=-1;
             copy_text(sensor_capture_error,sizeof(sensor_capture_error),
                       "Sensor capture cancelled: ECU signature or INI changed");
          }else{
             snprintf(payload,sizeof(payload),
                "{\"signature\":\"%s\",\"command\":\"%s\"}",
                escaped_signature,escaped_command);
             result=post_api("/api/sensors/capture",payload,11000);
             bool ack=false;
             if(result>0&&json_bool(http_body(),"command_acknowledged",&ack)&&ack)
                 result=1;
             else {
                 result=-1;
                 copy_text(sensor_capture_error,sizeof(sensor_capture_error),
                           "ECU sensor capture not acknowledged; verify settings");
             }
          }
       }else if(type==TASK_TEST_ABORT){
          char escaped[320],payload[400];
          if(!ecu_signature[0]||
             !escape_json_string(ecu_signature,escaped,sizeof(escaped))){
             result=-1;
             copy_text(test_abort_error,sizeof(test_abort_error),"Missing ECU signature");
          }else{
             snprintf(payload,sizeof(payload),"{\"signature\":\"%s\"}",escaped);
             result=post_api("/api/test/abort",payload,9000);
             bool ack=false,verified=false;
             if(result>0&&
                json_bool(http_body(),"command_acknowledged",&ack)&&ack&&
                json_bool(http_body(),"running_state_verified",&verified)&&!verified){
                result=1;
             }else{
                result=-1;
                copy_text(test_abort_error,sizeof(test_abort_error),
                   "ECU did not acknowledge abort; output state UNKNOWN");
             }
          }
       }else if(type==TASK_COMMAND_INSPECT){
           char path[128];
           snprintf(path,sizeof(path),"/api/ts/inspect?name=%s",command_inspect_name);
           result=get_request(path);
           if(result>0&&last_http==200){
               bool declared=false,visible=false,actuated=true;
               char *body=http_body();
               if(!body||!json_bool(body,"declared_in_ini",&declared)||
                  !json_bool(body,"shown_as_button",&visible)||
                  !json_bool(body,"ecu_command_sent",&actuated)||actuated)
                   result=-1;
               else{
                   command_inspect_defined=declared;
                   command_inspect_visible=visible;
               }
           }else result=-1;
       }else if(type==TASK_WRITE){
          perform_queued_write();result=write_verified?1:0;
       }else if(type==TASK_INI_FLAGS){
          BtsIniFlags latest={0};int got=0;
          for(int i=0;i<BTS_INI_FLAG_COUNT;i++){
             double value=0;
             if(read_cal_constant(bts_ini_flag_names[i],&value)){
                latest.values[i]=value;latest.known[i]=true;got++;
             }
          }
          task_ini_flags=latest;result=got?1:-1;
       }else if(type==TASK_RPM_LIMIT){
          double limit=0;
          if(read_cal_constant("rpmHardLimit",&limit)&&limit>0){
             rpm_limit_value=limit;result=1;
          }else result=-1;
       }else if(type==TASK_ERROR){
          result=post_ts("e","");
          if(result>0){
             uint8_t raw[BRICK_FAULT_MAX];
             int n=get_hex_reply(raw,BRICK_FAULT_MAX-1);
             if(n<0){result=-1;}
             else {
                int out=0;bool valid=true,terminated=false;
                for(int i=0;i<n;i++){
                   unsigned char c=raw[i];
                   if(c==0){terminated=true;continue;}
                   if(terminated){valid=false;break;}
                   if(c>=32&&c<127)fault_task_message[out++]=(char)c;
                   else if(c=='\r'||c=='\n'||c=='\t')fault_task_message[out++]=' ';
                   else {valid=false;break;}
                }
                fault_task_message[out]=0;
                result=valid&&fault_payload_valid(fault_task_message)?1:-1;
             }
          }
       }else if(type==TASK_STATUS)result=get_request("/api/status");
       else if(type==TASK_INI)result=get_request("/api/ini/file");
       else if(type==TASK_LIVE){
          char path[96];snprintf(path,sizeof(path),"/api/live?length=%d&hz=50",len);
          result=get_request(path);
       }else if(type==TASK_CAL||type==TASK_AXIS_X||type==TASK_AXIS_Y||type==TASK_PANEL_PREVIEW){
          result=read_bytes(page,off,task_calibration,len);

       }
       LightLock_Lock(&task_lock);task_result=result;task_state=3;LightLock_Unlock(&task_lock);
    }else svcSleepThread(10000000LL);
 }
}
static void complete_task(void){
 int type=TASK_IDLE,result=-1,field=-1;
 LightLock_Lock(&task_lock);
 if(task_state==3){type=task_type;result=task_result;field=task_field;}
 LightLock_Unlock(&task_lock);
 if(type==TASK_IDLE)return;
 u64 now=osGetTime();
 switch(type){
 case TASK_STIM_PREPARE:
   if(result>0){
      warn_before_hazardous_test(BRICK_TEST_TRIGGER);
      char msg[144];snprintf(msg,sizeof(msg),"Bench checks OK: %.0f RPM, %.2f V. Release then press L+R",stim_last_rpm,stim_last_vbat);
      note(msg);
   }else {stim_token[0]=0;note(stim_failure[0]?stim_failure:"ECU preflight failed; check the ESP32 error above");}
   break;
 case TASK_STIM_START:
   stim_on_by_bridge=result>0;
   note(result>0?"ECU internal trigger Start ACKNOWLEDGED. Use Stop to disable":
      (stim_failure[0]?stim_failure:"ECU Start not acknowledged; state UNKNOWN, use Stop"));
   status_requested=true;
   break;
 case TASK_STIM_STOP:
   if(result>0){stim_on_by_bridge=false;stim_token[0]=0;note("ECU internal trigger Stop ACKNOWLEDGED");}
   else note(stim_failure[0]?stim_failure:"ECU Stop not acknowledged; stimulation state UNKNOWN");
   status_requested=true;
   break;
 case TASK_WRITE:
   if(result>0&&write_verified)ini_drop_dynamic_conversions();
   write_inflight=false;
   bts_audio_stop_burn_loop();
   if(result>0&&write_verified)bts_audio_play(BTS_SOUND_BURN_OK,event_audio_enabled);
   else bts_audio_play(BTS_SOUND_CONNECTED,event_audio_enabled); /* fart with reverb */
   /* Keep perform_queued_write()'s exact verified/power-cycle/error text. */
   /* On failure the modified bytes remain staged for inspection/retry. */
   break;
 case TASK_INI_FLAGS:
   if(result>0&&ecu_session&&*ini_signature&&!strcmp(ecu_signature,ini_signature)){
      ini_flags=task_ini_flags;
      copy_text(ini_flags_signature,sizeof(ini_flags_signature),ecu_signature);
      ini_resolve_all_numeric_fields();map_text_dirty=true;
      note("ECU unit modes loaded; INI conversions updated");
   }else{
      ini_flags_requested=false;ini_flags_retry_at=now+10000;
   }
   break;
 case TASK_SENSOR_CAPTURE:
   sensor_capture_inflight=false;
   if(result>0){
      note("ECU SENSOR CAPTURE ACK: reopen settings, verify values, then burn");
      status_requested=true;
      fault_poll_requested=true;
   }else note(sensor_capture_error[0]?sensor_capture_error:
              "Sensor capture failed; calibration state unknown");
   break;
 case TASK_TEST_ABORT:
   note(result>0?
      "ECU test abort ACKNOWLEDGED; verify actual hardware state":
      (test_abort_error[0]?test_abort_error:
       "ECU abort failed; physical output state UNKNOWN"));
   status_requested=true;
   break;
 case TASK_COMMAND_INSPECT:
   if(result>0){
       note(command_inspect_defined&&command_inspect_visible?
          "INI command validated; hardware NOT actuated":
          "Command not defined or not shown by matching ECU INI");
   }else note("Bridge command inspection unavailable; no ECU output sent");
   break;
 case TASK_RPM_LIMIT:
   rpm_limit_known=result>0;
   rpm_limit_retry_at=now+(result>0?60000:15000);
   rpm_limit_requested=result>0;
   break;
 case TASK_ERROR:
   if(result>0){
     /* Remember the exact controller that produced the message. */
     if(ecu_signature[0])snprintf(fault_ecu_signature,sizeof(fault_ecu_signature),"%s",ecu_signature);
     fault_last_success=now;fault_accept_message(fault_task_message);
   }
   break;
 case TASK_STATUS:
   if(result>0)decode_status();
   break;
 case TASK_INI:
   if(result>0)decode_ini();
   else {note("INI download failed - retrying later");ini_retry_at=now+10000;}
   break;
 case TASK_LIVE:
   live_http_code=result>0?last_http:0;
   if(result>0)decode_live();
   else live_errors++;
   break;
 case TASK_AXIS_X:case TASK_AXIS_Y:{
   int a=(type==TASK_AXIS_X)?0:1;
   if(field==axis_field[a]&&field>=0&&field<cal_count&&ui_page==PAGE_FIELD){
     if(result>0&&result==cal[field].bytes&&result<=512){
       memcpy(axis_data[a],task_calibration,(size_t)result);
       memcpy(axis_original[a],task_calibration,(size_t)result);
       axis_len[a]=result;axis_ready[a]=true;

       map_text_dirty=true;
     }
   }
   break;
 }
 case TASK_PANEL_PREVIEW:
   for(int j=0;j<BTS_PANEL_PREVIEW_COUNT;j++){
       BtsPanelPreview *v=&panel_previews[j];
       if(v->field==field&&v->length>0&&
          result==v->length&&result<=EDIT_CAP&&ui_page==PAGE_DETAIL){
          memcpy(v->bytes,task_calibration,(size_t)result);
          v->ready=true;
       }
   }
   break;
 case TASK_CAL:
   if(field==open_field && field>=0 && field<cal_count){
      Field *f=&cal[field];
      if(result>0&&result<=EDIT_CAP&&result==f->bytes){
         memcpy(editor,task_calibration,(size_t)result);
         memcpy(original,task_calibration,(size_t)result);
         editor_length=result;editor_loaded=true;map_text_dirty=true;
         note("ECU field loaded - SELECT stages local edits");
      }else note("ECU calibration read failed - retry A");
   }
   break;
 default:break;
 }
 LightLock_Lock(&task_lock);task_state=0;task_type=TASK_IDLE;LightLock_Unlock(&task_lock);
}
static void run_scheduler(u64 now){
 if(!net_ok||!wifi_candidate||!task_idle()||work_suspended)return;
 /* A queued safety abort takes priority over tuning, writes and telemetry. */
 if(test_abort_pending){
    if(task_submit(TASK_TEST_ABORT,0,0,0,-1)){
       test_abort_pending=false;
       test_abort_error[0]=0;
    }
    return;
 }
 /* Sensor capture is a single-shot, non-actuating calibration command. */
 if(sensor_capture_pending){
    if(task_submit(TASK_SENSOR_CAPTURE,0,0,0,-1)){
       sensor_capture_pending=false;
       sensor_capture_inflight=true;
       sensor_capture_error[0]=0;
    }
    return;
 }
 /* User safety actions outrank streaming telemetry and queued reads. */
 if(stim_pending!=STIM_NONE){
    int operation=stim_pending;
    int task=operation==STIM_PREPARE?TASK_STIM_PREPARE:
             operation==STIM_START?TASK_STIM_START:TASK_STIM_STOP;
    if(task_submit(task,0,0,0,-1))stim_pending=STIM_NONE;
    return;
 }
 /* Guaranteed priority over 50 Hz live polling: user does not re-press Burn. */
 if(write_queued){
    if(task_submit(TASK_WRITE,0,0,0,-1)){
       write_queued=false;write_inflight=true;
       note("Bench burn in progress; checking authorization and ECU data");
    }
    return;
 }
 if(command_inspect_pending){
    if(task_submit(TASK_COMMAND_INSPECT,0,0,0,-1))
       command_inspect_pending=false;
    return;
 }
 if(menu_loaded&&ecu_session&&*ecu_signature&&!strcmp(ini_signature,ecu_signature)&&
    !ini_flags_requested&&now>=ini_flags_retry_at){
    if(task_submit(TASK_INI_FLAGS,0,0,0,-1)){
        ini_flags_requested=true;ini_flags_retry_at=now+15000;
    }
    return;
 }
 if(!rpm_limit_requested&&menu_loaded&&ecu_session&&now>=rpm_limit_retry_at){
    if(task_submit(TASK_RPM_LIMIT,0,0,0,-1)){
       rpm_limit_requested=true;rpm_limit_retry_at=now+15000;
    }
    return;
 }
 if(cal_requested&&pending_cal_field>=0&&pending_cal_field<cal_count){
    Field *f=&cal[pending_cal_field];
    if(f->bytes>0&&f->bytes<=EDIT_CAP&&task_submit(TASK_CAL,f->page,f->offset,f->bytes,pending_cal_field)){
       cal_requested=false;pending_cal_field=-1;
    }
    return;
 }
 /* Populate composite TunerStudio screen widgets from genuine ECU reads.
  * This task cannot stage edits or issue a physical output command. */
 if(ui_page==PAGE_DETAIL&&bts_spatial_layout()&&menu_loaded&&ecu_session&&
    ecu_signature[0]&&!strcmp(ini_signature,ecu_signature)){
   for(int j=0;j<BTS_PANEL_PREVIEW_COUNT;j++){
      BtsPanelPreview *v=&panel_previews[j];
      if(v->field<0||v->ready||v->attempts>=2)continue;
      const Field *f=&cal[v->field];
      if(task_submit(TASK_PANEL_PREVIEW,f->page,f->offset,v->length,v->field)){
         v->attempts++;
         return;
      }
   }
 }
 if(ui_page==PAGE_FIELD&&editor_loaded&&current_table>=0){
    for(int a=0;a<2;a++)if(axis_field[a]>=0&&!axis_ready[a]&&axis_tries[a]<3){
       Field *f=&cal[axis_field[a]];
       if(task_submit(a==0?TASK_AXIS_X:TASK_AXIS_Y,f->page,f->offset,f->bytes,axis_field[a])){
          axis_tries[a]++;return;
       }
    }
 }
 if((ini_refresh||(!menu_loaded&&now>=ini_retry_at))&&task_submit(TASK_INI,0,0,0,-1)){
    ini_refresh=false;ini_retry_at=now+12000;
    return;
 }
 if((status_requested||now-status_last_enqueue>=(fault_active?1250U:8000U))&&task_submit(TASK_STATUS,0,0,0,-1)){
    status_last_enqueue=now;status_requested=false;return;
 }
 /* Poll the exact TunerStudio firmware/config error command without
  * interrupting writes or trigger-simulator control. Priority over gauge
  * snapshots (which are cached on the ESP32), not over user operations. */
 if(ecu_session && (fault_poll_requested || now-fault_last_enqueue>=(fault_active?1700U:3000U))){
    if(task_submit(TASK_ERROR,0,0,0,-1)){
       fault_last_enqueue=now;fault_poll_requested=false;
    }
    return;
 }
 /* Keep the ESP32 telemetry subscription alive. Reading the latest snapshot
  * does NOT trigger a fresh USB query per HTTP request in BrickOS. */
 int live_delay=(live_http_code==503)?350:32;
 if(menu_loaded && ecu_session && !strcmp(ini_signature,ecu_signature) && now-live_last_enqueue>=(u64)live_delay){
    if(task_submit(TASK_LIVE,0,0,live_length,-1))live_last_enqueue=now;
 }
}
int main(void){
 gfxInitDefault();gfxSet3D(true);C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);C2D_Init(C2D_DEFAULT_MAX_OBJECTS);C2D_Prepare();
 top=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);top_right=C2D_CreateScreenTarget(GFX_TOP,GFX_RIGHT);bottom=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
 dreamfont_sheet=C2D_SpriteSheetLoadFromMem(dreamfont_start,(size_t)((uintptr_t)dreamfont_end-(uintptr_t)dreamfont_start));
 ts_icon_sheet=C2D_SpriteSheetLoadFromMem(ts_icons_start,(size_t)((uintptr_t)ts_icons_end-(uintptr_t)ts_icons_start));
 textbuf=C2D_TextBufNew(8500);map_textbuf=C2D_TextBufNew(9500);
 /* High contrast, accessible dark dashboard palette. */
 bg=C2D_Color32(2,4,7,255);panel=C2D_Color32(13,18,24,242);
 panel2=C2D_Color32(19,25,32,244);soil=C2D_Color32(8,11,16,255);
 white=C2D_Color32(248,251,252,255);cream=white;
 secondary=C2D_Color32(172,188,202,255);green=C2D_Color32(104,235,186,255);
 leaf=C2D_Color32(65,161,179,255);bright=C2D_Color32(151,231,245,255);
 blue=C2D_Color32(91,192,219,255);dew=blue;
 orange=C2D_Color32(253,183,98,255);petal=C2D_Color32(125,189,235,255);
 red=C2D_Color32(240,102,112,255);berry=red;
 outline=C2D_Color32(42,53,63,255);muted_leaf=C2D_Color32(27,49,57,246);
 ptmu_ok=R_SUCCEEDED(ptmuInit());
 /* HID sensor updates are independent of tuning/network reads; if either
  * sensor is unavailable the dashboard keeps its subtle idle float. */
 bts_accel_ok=R_SUCCEEDED(HIDUSER_EnableAccelerometer());
 bts_gyro_ok=R_SUCCEEDED(HIDUSER_EnableGyroscope());
 if(bts_gyro_ok){float coeff=0.f;
    if(R_SUCCEEDED(HIDUSER_GetGyroscopeRawToDpsCoefficient(&coeff))&&isfinite(coeff)&&coeff>0.f&&coeff<2.f)
        bts_gyro_dps_per_raw=coeff;
 }

 if(ptmu_ok)update_system_battery(osGetTime());
 LightLock_Init(&task_lock);
 load_gauge_preferences();load_favorites();sfx_init();bts_mascot_init();sfx_play(1);
 if(!load_ini_cache()){note("No SD INI cache; auto-download when connected");ini_retry_at=0;}
 response=(char*)malloc(RESPONSE_CAP);soc_mem=(u32*)memalign(0x1000,SOCKET_MEM);
 Result ar=acInit();ac_ok=R_SUCCEEDED(ar);
 if(!ac_ok)snprintf(net_status,sizeof(net_status),"AC error 0x%08lX",(unsigned long)ar);
 if(response&&soc_mem){Result r=socInit(soc_mem,SOCKET_MEM);net_ok=R_SUCCEEDED(r);
    if(!net_ok)snprintf(net_status,sizeof(net_status),"SOC error 0x%08lX",(unsigned long)r);}
 else note("Out of memory for network buffers");
 if(net_ok&&ac_ok){worker=threadCreate(net_thread,NULL,64*1024,0x31,0,false);
   if(!worker){net_ok=false;note("Network worker failed to start");}}
 u64 next_render=0;
 while(aptMainLoop()){
  bts_audio_pump(); /* keep event PCM streaming on every UI frame */
    hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();
    u64 input_now=osGetTime();
    bool woke_idle_screen=false;
    if(!tune_last_activity)tune_last_activity=input_now;
    circlePosition active_pad;hidCircleRead(&active_pad);
    if(down || held || abs((int)active_pad.dx)>55 || abs((int)active_pad.dy)>55){
        tune_last_activity=input_now;
        if(tune_screen_asleep){
            tune_screen_asleep=false;
            /* The first button/touch only wakes the page: it cannot fire a
             * hidden tune edit or enter an INI item while gauges are shown. */
            down=0;
            woke_idle_screen=true;
        }
    }
    bool idle_tune_page=(ui_page==PAGE_HUB||ui_page==PAGE_TUNE_TREE||ui_page==PAGE_DETAIL||
        ui_page==PAGE_MENU||ui_page==PAGE_GROUP||ui_page==PAGE_FIELD);
    /* Never cover a local edit, verification, warning or power-cycle request. */
    bool must_keep_controls=staged||confirm_write||confirm_discard||
        write_queued||write_inflight||map_tools_open||power_cycle_popup||
        hazardous_test.kind||ui_page==PAGE_CHOICES||
        (ui_page==PAGE_FIELD&&!editor_loaded);
    tune_screen_asleep=idle_tune_page&&!must_keep_controls&&
        input_now-tune_last_activity>=10000;
    if((down&KEY_START)&&!hazardous_test.kind&&ui_page==PAGE_CATS)break;
    /* START is never a hidden global jump: workspace actions are visible. */
    handle_keys(down);
    /* Smooth D-pad repeats, independent of network traffic. */
    static u32 last_held=0;static u64 last_repeat=0;
    u64 now=osGetTime();
    update_system_battery(now);
    u32 nav=held&(KEY_DUP|KEY_DDOWN|KEY_DLEFT|KEY_DRIGHT);
    if(nav!=last_held){last_held=nav;last_repeat=now+270;}
    else if(nav && now>=last_repeat){handle_keys(nav);last_repeat=now+70;}
    /* New 3DS C-stick: repeat quick category/point/precision moves without
     * stealing A/X/Y edit keys or the original 3DS D-pad controls. */
    static u32 last_stick=0;static u64 stick_repeat=0;
    u32 stick=held&(KEY_CSTICK_UP|KEY_CSTICK_DOWN|KEY_CSTICK_LEFT|KEY_CSTICK_RIGHT);
    if(stick!=last_stick){last_stick=stick;stick_repeat=now+340;}
    else if(stick && now>=stick_repeat){handle_keys(stick);stick_repeat=now+150;}
    /* Circle pad analog navigation: fast when tilted far, precise when gentle. */
    circlePosition cp;hidCircleRead(&cp);
    int ax=abs((int)cp.dx),ay=abs((int)cp.dy);
    if(map_3d_view && in_map_view() && ax+ay>40 && !woke_idle_screen){
       map_3d_yaw+=(float)cp.dx*.00016f;
       map_3d_pitch+=(float)cp.dy*.00012f;
       if(map_3d_pitch<MAP3D_PITCH_MIN)map_3d_pitch=MAP3D_PITCH_MIN;
       if(map_3d_pitch>MAP3D_PITCH_MAX)map_3d_pitch=MAP3D_PITCH_MAX;
       pad_next_at=now+120;
    }else if((ax>65||ay>65) && !woke_idle_screen){
       if(now>=pad_next_at){
          u32 direction=0;
          if(ax>=ay)direction=cp.dx>0?KEY_CPAD_RIGHT:KEY_CPAD_LEFT;
          else direction=cp.dy>0?KEY_CPAD_UP:KEY_CPAD_DOWN;
          handle_keys(direction);
          pad_next_at=now+(ax>130||ay>130?60:125);
       }
    }else pad_next_at=now;
    complete_task();
    if(!wifi_candidate && fault_wifi_seen){
       /* The next connected /api/status must establish a NEW session before
        * old ECU configuration errors can be displayed again. */
       fault_session_was_lost=true;
    }
    if(wifi_candidate&&!fault_wifi_seen){
       /* ESP Wi-Fi can reconnect while the last status still says ECU online. */
       fault_recheck_after_reconnect=true;fault_poll_requested=true;
       status_requested=true;fault_last_enqueue=0;
    }
    fault_wifi_seen=wifi_candidate;
    if(ecu_session && live_valid && !connected_sfx_played){
       connected_sfx_played=true;
       bts_audio_play(BTS_SOUND_CONNECTED,event_audio_enabled);
    }else if(!ecu_session)connected_sfx_played=false;
    replay_update(now);
    /* Brief HTTP gaps never erase a real instrument reading. Mark it stale,
     * retain its last known value, clear only after 12 s of silence. */
    /* Never blank real gauges just because a wireless frame was missed.
     * Keep last-good data until a replacement arrives; mark its age instead. */
    live_stale=live_valid && gauge_hold_is_stale(now,live_received_at,LIVE_STALE_AFTER_MS);
    if(!ecu_session){bts_boost_level.high=false;bts_coolant_level.high=false;bts_coolant_level.last_ms=0;}
    else bts_sound_telemetry(now);
    run_scheduler(now);
    if(now>=next_render){
       next_render=now+33; /* roughly 30 FPS on the original 2011 3DS */
       bts_card_motion_update(now);
       stereo_strength=osGet3DSliderState();
       if(stereo_strength<0)stereo_strength=0;
       if(stereo_strength>1)stereo_strength=1;
       C2D_TextBufClear(textbuf);C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
       /* When 3D is off, render one eye for higher frame rate. */
       int eye_count=stereo_strength>.001f?2:1;
       for(int eye=0;eye<eye_count;eye++){
          stereo_eye=eye;
          C3D_RenderTarget *t=eye?top_right:top;
          if(!t)continue;
          C2D_TargetClear(t,bg);C2D_SceneBegin(t);
          bts_top_panel_3d=true;
          bts_text_screen_width=400.f;
       if(in_map_view()){
          Field *f=&cal[open_field];
          if(!editor_loaded){draw_depth_grid(400,240);text(22,85,.62f,orange,"Reading ECU map...");
             text(22,114,.42f,secondary,"No fabricated map values");}
          else if(!f->validScale){draw_depth_grid(400,240);text(20,90,.48f,orange,"INI field scaling unavailable");}
          else if(current_table>=0&&tables[current_table].curve)draw_curve_overview(f);
          else if(map_3d_view)draw_map_3d(f);
          else draw_map_overview(f);
       }else if(ui_page==PAGE_FIELD||ui_page==PAGE_CHOICES)draw_scalar_top();else if(ui_page==PAGE_TRIGGER_SIM)draw_trigger_top();else if(ui_page==PAGE_REPLAY||ui_page==PAGE_TRACE_MAPS)draw_replay_top();else if(ui_page==PAGE_COMPARE)draw_compare_top();else if(ui_page==PAGE_QUALITY)draw_quality_top();else if(ui_page==PAGE_INSIGHTS)draw_insights_top();else if(ui_page==PAGE_REPLAY_STATS)draw_replay_stats_top();else if(ui_page==PAGE_CHANNELS)draw_channel_top();
       else if(ui_page==PAGE_KNOCK)draw_knock_top();else if(ui_page==PAGE_GRAPH)draw_live_graph_top();else if(ui_page==PAGE_INI_GRAPH)draw_ini_graph_top();
       else if(tune_context_page())draw_tune_context_top();
       else home_dashboard();
       draw_fault_top_overlay();
       draw_power_cycle_top();
       }
       C2D_TargetClear(bottom,bg);C2D_SceneBegin(bottom);
       bts_top_panel_3d=false;
       bts_text_screen_width=320.f;draw_bottom();
       /* Bottom screen launches normally; no legacy boot/status splash. */
       C3D_FrameEnd(0);
    }else svcSleepThread(1000000LL);
 }
 worker_stop=true;
 if(worker){threadJoin(worker,UINT64_MAX);threadFree(worker);}
 if(net_ok) socExit();
 if(ac_ok) acExit();
 if(bts_gyro_ok) HIDUSER_DisableGyroscope();
 if(bts_accel_ok) HIDUSER_DisableAccelerometer();
 if(ptmu_ok) ptmuExit();
 free(response);
 free(soc_mem);
 if(ini_source)free(ini_source);
 bts_mascot_shutdown();
 sfx_close();
 if(ts_icon_sheet)C2D_SpriteSheetFree(ts_icon_sheet);
 if(dreamfont_sheet)C2D_SpriteSheetFree(dreamfont_sheet);
 if(map_textbuf)C2D_TextBufDelete(map_textbuf);
 C2D_TextBufDelete(textbuf);C2D_Fini();C3D_Fini();gfxExit();return 0;
}