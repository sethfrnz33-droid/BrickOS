#include "tuner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char**argv){
    assert(argc==2);
    char help[1024];
    bt_ini_help_index_reset();
    assert(bt_ini_context_help(argv[1],"launchRpm",help,sizeof help));
    assert(strstr(help,"RPM")&&strstr(help,"launch"));
    char saved[1024];strcpy(saved,help);
    for(int i=0;i<1000;i++){
        assert(bt_ini_context_help(argv[1],"launchRpm",help,sizeof help));
        assert(!strcmp(help,saved));
    }
    assert(!bt_ini_context_help(argv[1],"definitelyNotARealINISetting",help,sizeof help));
    assert(!help[0]);
    bt_ini_help_index_reset();
    assert(bt_ini_context_help(argv[1],"maxAcRpm",help,sizeof help));
    assert(strstr(help,"RPM"));
    assert(bt_ini_context_help(argv[1],"engineType",help,sizeof help));
    assert(strstr(help,"rusefi.com"));
    puts("PASS DSi v28: indexed SD help exact lookups; misses; reuse; cache reset");
}
