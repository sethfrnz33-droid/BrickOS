#include "signature.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    char hash[11];
    assert(bt_signature_hash_copy("rusEFI master.2026.11.18.brick.1293678056",hash));
    assert(!strcmp(hash,"1293678056"));
    assert(bt_signature_matches("rusEFI master.2026.11.18.brick.1293678056", "BrickECU alt.2025.04.12.other.1293678056"));
    assert(bt_signature_matches("any_prefix_1293678056", "different prefix.1293678056"));
    assert(bt_signature_matches("rusEFI master.2026.11.18.brick.1293678056\r\n", "brick_ecu.1293678056.ini"));
    assert(bt_signature_matches("rusEFI master.2026.11.18.brick.1293678056", " \"alt.1293678056\" "));
    assert(!bt_signature_matches("A.1293678056", "B.1293678057"));
    assert(!bt_signature_matches("A.0000123456", "B.123456"));
    assert(!bt_signature_matches("A.1293678056", "B.12936780560"));
    assert(!bt_signature_matches("A.1293678056", "B.1293678056oops"));
    assert(!bt_signature_matches("A.1293678056", "B.2026.11.18"));
    assert(!bt_signature_matches("A.12345678901", "B.12345678901"));
    assert(bt_signature_matches("A.12345678901", "A.12345678901"));
    assert(!bt_signature_matches("", "B.1293678056"));
    assert(bt_signature_matches("legacy-format", "legacy-format"));
    assert(!bt_signature_matches("legacy-format", "other-format"));
    puts("PASS: source names ignored; strict final layout hashes, wrappers and mismatch safety");
}
