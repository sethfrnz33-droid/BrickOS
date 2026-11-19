#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "web_assets.h"

struct asset {const char *name; const uint8_t *data; const size_t *length;};

int main(void) {
    const struct asset files[] = {
        {"web/index.html", brick_asset_html, &brick_asset_html_len},
        {"web/app.css", brick_asset_css, &brick_asset_css_len},
        {"web/app.js", brick_asset_js, &brick_asset_js_len},
        {"web/core.mjs", brick_asset_core, &brick_asset_core_len}
    };
    for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); ++i) {
        FILE *fp = fopen(files[i].name, "rb");
        if (!fp) { perror(files[i].name); return 1; }
        if (fseek(fp, 0, SEEK_END) != 0) return 2;
        long size = ftell(fp);
        if (size < 0 || fseek(fp, 0, SEEK_SET) != 0) return 3;
        if ((size_t)size != *files[i].length) {
            fprintf(stderr,"size mismatch: %s (%ld != %zu)\n", files[i].name, size, *files[i].length);
            fclose(fp); return 4;
        }
        uint8_t *copy = malloc((size_t)size+1);
        if (!copy) return 5;
        size_t read_len = fread(copy, 1, (size_t)size, fp);
        fclose(fp);
        if (read_len != (size_t)size || memcmp(copy, files[i].data, read_len) != 0 || files[i].data[read_len] != 0) {
            fprintf(stderr,"data mismatch: %s\n",files[i].name);free(copy);return 6;
        }
        printf("PASS %s %zu bytes\n", files[i].name, read_len);
        free(copy);
    }
    return 0;
}
