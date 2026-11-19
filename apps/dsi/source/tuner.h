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
#define BTS_LIVE_CAP 3072
#define BTS_MAP_CELLS 2048

typedef enum { BT_U08, BT_S08, BT_U16, BT_S16, BT_U32, BT_S32, BT_F32, BT_F64, BT_UNKNOWN } BtType;
typedef struct {
    char name[64], unit[26];
    int page, offset, width, height, bit_lo, bit_hi, kind;
    BtType type;
    double scale, translate, low, high;
    bool scaled, limits_known;
} BtField;
typedef struct {
    char name[64], alt[64], label[76], x[64], y[64], z[64], xchan[64], ychan[64];
    bool curve;
} BtMap;
typedef struct {
    char label[72], target[64];
    int group; /* 0 category, 1 folder, 2 child, 3 entry */
} BtMenu;
typedef struct {
    char owner[64], name[64], label[72];
    int kind; /* 0 scalar/array, 1 nested panel */
} BtDialogRef;
typedef struct {
    BtField *channels, *constants;
    BtMap *maps;
    BtMenu *menus;
    BtDialogRef *refs;
    int channel_capacity, constant_capacity, map_capacity, menu_capacity, ref_capacity;
    int channel_count, constant_count, map_count, menu_count, ref_count;
    int page_ids[64], page_sizes[64], page_count, current_page;
    bool page_writeable[64], page_burnable[64], truncated, out_of_memory;
    char signature[160], section[32], current_dialog[64];
    bool big_endian, had_constants, had_outputs, had_signature, in_main_menu, valid;
    int last_map, line_len;
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
    int raw_length;
    bool has_x, has_y, valid, dirty, confirm_burn;
} BtTable;

/* Load explicit TunerStudio bits option labels from the on-SD INI on demand.
 * Returns zero if options are expressed with an unsupported macro: never guess. */
int bt_bit_choices(const char *ini_path,const char *field_name,char labels[][36],int cap);
void bt_ini_reset(BtDefinition *d);
void bt_ini_feed(BtDefinition *d, const uint8_t *bytes, size_t length);
bool bt_ini_finish(BtDefinition *d);
const BtField *bt_field(const BtDefinition *d, const char *name, bool runtime);
int bt_type_size(BtType type);
bool bt_field_value(const BtDefinition *d, const BtField *f, const uint8_t *data, int length, int index, double *result);
bool bt_encode_value(const BtDefinition *d,const BtField *f, uint8_t *data,int len,int index,double value);
bool bt_live_decode(BtLive *live, const uint8_t *body, size_t bodylen);
/* A valid HTTP response with unchanged sample sequence is NOT a transport error. */
bool bt_live_decode_http(BtLive *live, const uint8_t *body, size_t bodylen, bool *changed);
int bt_required_live_length(const BtDefinition *d, const char *const *names, int count);
#endif
