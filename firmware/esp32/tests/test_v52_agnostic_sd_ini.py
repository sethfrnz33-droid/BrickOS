"""Host regression: import any single structurally valid ECU SD INI.

Compile the exact production SD import function with host filesystem stubs;
verify that a different signature imports while invalid data cannot replace
the last working cached INI.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
cache = (root / "main/ini_cache.c").read_text()
sd = (root / "main/usb_sd_ini.c").read_text()
header = (root / "main/ini_cache.h").read_text()

start = cache.index("esp_err_t ini_cache_import_sd_file(")
end = cache.index("/* The two channels", start)
impl = cache[start:end]
assert "exact_signature(" not in impl, "Generic SD import must not require matching signature"
assert "valid_definition(source_path)" in impl
assert "commit_file()" in impl
assert "ini_cache_import_sd_file" in header
assert "ini_cache_definition_file_valid" in header
assert "ini_cache_import_exact_file" in cache
assert "ini_cache_import_exact_file" in sd
assert "ini_cache_import_sd_file" in sd
task = sd[sd.index("static void sd_ini_task"):sd.index("esp_err_t brick_sd_ini_host_install")]
assert "if (s_device_present && due &&" in task
assert "vcp_gateway_usb_ready() && signature[0]" not in task
assert "Multiple valid INIs" in sd
assert "format_if_mount_failed = false" in sd

harness = r'''
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <assert.h>
#define ESP_OK 0
#define ESP_FAIL 1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_INVALID_SIZE 3
#define ESP_ERR_INVALID_RESPONSE 4
#define INI_MAX_BYTES 1200000
#define INI_PATH "test_saved.ini"
#define INI_TEMP "test_temp.ini"
typedef int esp_err_t;
static void *lock;
static void xSemaphoreGive(void *l){(void)l;}
static bool acquire(void){return true;}
static bool valid_definition(const char *path){
    FILE *f=fopen(path,"rb");if(!f)return false;
    char buf[8192];size_t n=fread(buf,1,sizeof(buf)-1,f);fclose(f);buf[n]=0;
    return strstr(buf,"[Constants]")!=NULL && strstr(buf,"[OutputChannels]")!=NULL;
}
static esp_err_t commit_file(void){
    if(!valid_definition(INI_TEMP))return ESP_ERR_INVALID_RESPONSE;
    return rename(INI_TEMP,INI_PATH)==0?ESP_OK:ESP_FAIL;
}
'''
footer = r'''
static void put(const char *p,const char *data){FILE *f=fopen(p,"wb");assert(f);assert(fwrite(data,1,strlen(data),f)==strlen(data));assert(fclose(f)==0);}
static bool same(const char *p,const char *data){char b[4096]={0};FILE *f=fopen(p,"rb");if(!f)return false;size_t n=fread(b,1,sizeof(b),f);fclose(f);return n==strlen(data)&&memcmp(b,data,n)==0;}
int main(void){
 const char *valid="[TunerStudio]\\nsignature=some-other-ECU-build\\n[Constants]\\nx=1\\n[OutputChannels]\\ny=2\\n";
 put("candidate.ini",valid);
 assert(ini_cache_import_sd_file("candidate.ini")==ESP_OK);
 assert(same(INI_PATH,valid));
 put("candidate.ini","[TunerStudio]\\nsignature=no-data\\n[Constants]\\nfoo=1\\n");
 assert(ini_cache_import_sd_file("candidate.ini")==ESP_ERR_INVALID_RESPONSE);
 assert(same(INI_PATH,valid));
 assert(ini_cache_import_sd_file("missing.ini")!=ESP_OK);
 assert(same(INI_PATH,valid));
 FILE *f=fopen("candidate.ini","wb");assert(f);
 for(int i=0;i<1201;i++){char k[1000]={0};assert(fwrite(k,1,sizeof(k),f)==sizeof(k));}
 assert(fclose(f)==0);
 assert(ini_cache_import_sd_file("candidate.ini")==ESP_ERR_INVALID_SIZE);
 assert(same(INI_PATH,valid));
 puts("PASS: arbitrary INI accepted; invalid/oversized copies preserve previous cache");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="brick_ini_any_") as tmp:
    tmp = Path(tmp)
    (tmp/"test.c").write_text(harness+impl+footer)
    subprocess.run(["cc","-std=gnu17","-O2","-Wall","-Wextra","-Werror",str(tmp/"test.c"),"-o",str(tmp/"test")],check=True)
    subprocess.run([str(tmp/"test")],cwd=tmp,check=True)
print("PASS: USB SD import fallback independent of signature, write guards unchanged")
