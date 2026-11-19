#ifndef BTS_DSI_TUNER_H
#define BTS_DSI_TUNER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BTS_MAX_CHANNELS 1500
#define BTS_MAX_CONSTANTS 2700
#define BTS_MAX_MAPS 350
#define BTS_MAX_MENUS 1800
#define BTS_MAX_REFS 6000
#define BTS_MAX_INI_GRAPHS 160
#define BTS_MAX_GRAPH_LINES 640
#define BTS_MAX_GAUGES 550
#define BTS_LIVE_CAP 3072
#define BTS_MAP_CELLS 2048

typedef enum { BT_U08, BT_S08, BT_U16, BT_S16, BT_U32, BT_S32, BT_F32, BT_F64, BT_UNKNOWN } BtType;
typedef struct {
    char name[64], unit[26];
    int page, offset, width, height, bit_lo, bit_hi, kind;
    BtType type;
    double scale, translate, low, high;
    bool scaled, limits_known, read_only, power_cycle;
} BtField;
typedef struct {
    char name[64], alt[64], label[76], x[64], y[64], z[64], xchan[64], ychan[64];
    char x_label[64], y_label[64], help_topic[64];
    int series_index;
    bool curve, x_readonly, y_readonly;
} BtMap;
typedef struct {
    char label[72], target[64], condition[80];
    int group; /* 0 category, 1 folder, 2 child, 3 entry */
} BtMenu;
typedef struct {
    char owner[64], name[64], label[72], detail[120], condition[80];
    char placement[8]; /* TunerStudio North/South/East/West/Center; layout, not a field */
    int kind; /* 0 field, 1 panel, 2 command button, 3 read-only field, 4 runtime, 5 text/help, 6 liveGraph, 7 gauge */
} BtDialogRef;
typedef struct {
    char name[64], channel[64], label[72], unit[40];
    double min_value, max_value;
    bool range_known;
} BtGauge;
typedef struct {char owner[64], name[64], title[72]; int first_line, line_count;} BtIniGraph;
typedef struct {char channel[64]; int graph;} BtGraphLine;
typedef struct {
    BtField *channels, *constants;
    BtMap *maps;
    BtMenu *menus;
    BtDialogRef *refs;
    BtIniGraph *graphs;
    BtGraphLine *graph_lines;
    BtGauge *gauges;
    int channel_capacity, constant_capacity, map_capacity, menu_capacity, ref_capacity, graph_capacity, graph_line_capacity, gauge_capacity;
    int channel_count, constant_count, map_count, menu_count, ref_count, graph_count, graph_line_count, gauge_count;
    int page_ids[64], page_sizes[64], page_count, current_page;
    bool page_writeable[64], page_burnable[64], truncated, out_of_memory;
    char signature[160], section[32], current_dialog[64];
    bool big_endian, had_constants, had_outputs, had_signature, in_main_menu, valid;
    int last_map, last_graph, line_len;
    bool line_overflow;
    char line[8192];
} BtDefinition;

typedef struct {
    uint32_t sequence;
    uint8_t bytes[BTS_LIVE_CAP];
    int length;
    bool valid;
} BtLive;

typedef struct {
    double cells[BTS_MAP_CELLS], xbins[64], ybins[64];
    int width, height, x_count, y_count;
    uint8_t baseline[8192], edited[8192];
    /* X/Y axis snapshots are independent ECU constants and must be staged
     * and verified separately, never spliced into Z's backing page region. */
    uint8_t axis_baseline[2][512], axis_edited[2][512];
    int axis_length[2];
    bool axis_dirty[2];
    int raw_length;
    bool has_x, has_y, valid, dirty, confirm_burn;
} BtTable;

typedef struct {
    char unit[128], scale[128], translate[128], low[128], high[128];
} BtIniFormat;
/* Extract ECU-defined numeric formatting and computed channels from the actual INI. */
bool bt_ini_field_format(const char *path,const char *name,BtIniFormat *format);
/* Constant arrays without an ECU address, explicitly seeded by [ConstantsExtensions]. */
bool bt_ini_virtual_array(const char *path,const char *name,double *values,int capacity,int *count);
bool bt_ini_rhs(const char *path,const char *section,const char *name,char *out,size_t cap);
typedef bool (*BtVarLookup)(const char *name,void *ctx,double *out);
bool bt_numeric_eval(const char *expression,BtVarLookup variable,void *ctx,double *out);

/* Load explicit TunerStudio bits option labels from the on-SD INI on demand.
 * Returns zero if options are expressed with an unsupported macro: never guess. */
int bt_bit_choices(const char *ini_path,const char *field_name,char labels[][36],int cap);
/* INI supplied [SettingContextHelp] tooltip text, never synthetic prose. */
bool bt_ini_context_help(const char *ini_path,const char *target,char *text,size_t cap);
/* INI [UserDefined] help topics: help=topic then text=... entries. */
bool bt_ini_topic_help(const char *ini_path,const char *topic,char *out,size_t cap);
void bt_ini_reset(BtDefinition *d);
void bt_ini_feed(BtDefinition *d, const uint8_t *bytes, size_t length);
bool bt_ini_finish(BtDefinition *d);
const BtField *bt_field(const BtDefinition *d, const char *name, bool runtime);
const BtGauge *bt_gauge(const BtDefinition *d,const char *name);
int bt_type_size(BtType type);
bool bt_field_value(const BtDefinition *d, const BtField *f, const uint8_t *data, int length, int index, double *result);
bool bt_encode_value(const BtDefinition *d,const BtField *f, uint8_t *data,int len,int index,double value);
bool bt_live_decode(BtLive *live, const uint8_t *body, size_t bodylen);
/* A valid HTTP response with unchanged sample sequence is NOT a transport error. */
bool bt_live_decode_http(BtLive *live, const uint8_t *body, size_t bodylen, bool *changed);
int bt_required_live_length(const BtDefinition *d, const char *const *names, int count);
#endif
