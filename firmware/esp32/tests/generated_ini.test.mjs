import assert from 'node:assert/strict';
import test from 'node:test';
import {readFileSync} from 'node:fs';
import {parseIni, matchingIni} from '../web/core.mjs';
const path = new URL('./fixtures/rusefi_proteus_f7.ini', import.meta.url);
test('actual generated rusEFI Proteus F7 INI parses and signature resolves', () => {
 const contents = readFileSync(path, 'utf8');
 const sig='rusEFI master.2026.11.09.proteus_f7.2735508644';
 assert.ok(Buffer.byteLength(contents)>750000);
 const parsed=matchingIni(contents,sig,parseIni);
 assert.equal(parsed.signature,sig);
 assert.ok(parsed.cal.length>100);
 assert.ok(parsed.channels.length>30);
 assert.equal(parsed.pageSizes[0],17816);
 assert.equal(parsed.pageSizes[4],48000);
 assert.throws(()=>matchingIni(contents,'rusEFI wrong-firmware',parseIni),/signature differs/);
});
