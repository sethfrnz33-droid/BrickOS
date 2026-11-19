import assert from 'node:assert/strict';
import test from 'node:test';
import {iniUrlForSignature, matchingIni} from '../web/core.mjs';
import {parseIni} from '../web/core.mjs';
test('exact official archive URL from signature',()=>{
 assert.equal(iniUrlForSignature('rusEFI master.2024.02.02.alphax-4chan.1293678056'), 'https://rusefi.com/online/ini/rusefi/master/2024/02/02/alphax-4chan/1293678056.ini');
 assert.equal(iniUrlForSignature('rusEFI master.2026.08.10.uaefi.328449289'), 'https://rusefi.com/online/ini/rusefi/master/2026/08/10/uaefi/328449289.ini');
 assert.equal(iniUrlForSignature('rusEFI invalid data'),null);
 assert.equal(iniUrlForSignature('rusEFI master.2026.08.10.uaefi.328449289?foo'),null);
});
test('rejects stale INI even when it parses',()=>{
 const raw=`[MegaTune]\nsignature = "rusEFI master.2026.08.10.uaefi.328449289"\n[Constants]\npage = 1\nmap = scalar, U08, 0, "", 1, 0\n[OutputChannels]\nrpm = scalar, U08, 0, "", 1, 0\n`;
 assert.equal(matchingIni(raw,'rusEFI master.2026.08.10.uaefi.328449289',parseIni).signature,'rusEFI master.2026.08.10.uaefi.328449289');
 assert.throws(()=>matchingIni(raw,'rusEFI master.2026.08.11.uaefi.328449289',parseIni),/signature differs/);
});
