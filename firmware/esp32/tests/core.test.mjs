import assert from 'node:assert/strict';
import test from 'node:test';
import {parseIni,mathValue,splitArgs,hex,unhex,le16,pageId,readArgs,outputArgs,writeArgs,burnArgs,getValue,putValue,relative,readBLog,crc32,makePatch,validatePatch} from '../web/core.mjs';
const ini=`[TunerStudio]\nsignature = "rusEFI sample-123"\n[Constants]\nendianness = little\npageSize = 8192, 1024\npageIdentifier = 0x0000, 0x0100\nblockingFactor = 512\npage = 1\nfuelTable = array, U16, 200, [3x2], "ms", 0.01, 0, 0, 655.35, 2\nignitionAdvance = scalar, S16, 450, "deg", 0.1, -2, -30, 60\ncompressorEnabled = bits, U08, 500, [2:2], "off", "on"\npage = 2\nsecondaryTable = array, U08, 16, [2x2], "percent", 0.5, 0\n[OutputChannels]\nochBlockSize = 256\nrpm = scalar, U16, 0, "rpm", 1, 0\nmap = scalar, U16, 2, "kPa", 0.1, 0\n`;

test('numeric INI expression safe arithmetic',()=>{
 assert.equal(mathValue('1/16'),0.0625);
 assert.equal(mathValue('0x100'),256);
 assert.equal(mathValue('(10+2)*5'),60);
 assert.equal(mathValue('process.exit(1)'),null);
 assert.equal(mathValue('1;window.alert(1)'),null);
});
test('parse generated ECU definition metadata and channels',()=>{
 const d=parseIni(ini);assert.equal(d.signature,'rusEFI sample-123');assert.equal(d.cal.length,4);
 assert.equal(d.channels.length,2);assert.equal(d.cal[0].byteLength,12);
 assert.equal(d.pageSizes[0],8192);assert.equal(d.pageIdentifiers[1],256);
 assert.equal(d.outputSize,256);assert.equal(d.cal[3].page,2);
});
test('page and output command arguments',()=>{
 const d=parseIni(ini);
 assert.equal(readArgs(d,1,200,12),'0000c8000c00');
 assert.equal(readArgs(d,2,16,4),'000110000400');
 assert.equal(outputArgs(0,192),'0000c000');
 assert.equal(burnArgs(d,2),'0001');
 assert.equal(writeArgs(d,1,200,new Uint8Array([1,2,3])),'0000c8000300010203');
});
test('decoded scaled values and staging writes',()=>{
 const d=parseIni(ini);const f=d.cal.find(x=>x.name==='fuelTable');const bytes=new Uint8Array(12);
 const local=relative(f,200);putValue(bytes,local,0,2.23,d.endianness);putValue(bytes,local,5,13.55,d.endianness);
 assert.equal(getValue(bytes,local,0,d.endianness),2.23);
 assert.equal(getValue(bytes,local,5,d.endianness),13.55);
 assert.equal(hex(bytes.slice(0,2)),'df00');
 const negative=new Uint8Array(2);const ign=relative(d.cal.find(x=>x.name==='ignitionAdvance'),450);
 putValue(negative,ign,0,-4.5,'little');assert.equal(getValue(negative,ign,0,'little'),-4.5);
});
test('bitfield values do not clobber adjacent bits',()=>{
 const d=parseIni(ini);const f=relative(d.cal.find(x=>x.name==='compressorEnabled'),500);
 const b=new Uint8Array([0b10100011]);putValue(b,f,0,1);assert.equal(b[0],0b10100111);
 putValue(b,f,0,0);assert.equal(b[0],0b10100011);
});
test('CRC32 matches reference standard',()=>{assert.equal(crc32(new TextEncoder().encode('123456789')),0xcbf43926)});
test('decode stamped BRLOG1 data',()=>{
 const buf=new Uint8Array(12+4+4);buf.set(new TextEncoder().encode('BRLOG1'));let view=new DataView(buf.buffer);
 view.setUint16(6,0,true);view.setUint16(8,4,true);view.setUint16(10,100,true);
 view.setUint32(12,123456,true);view.setUint16(16,3100,true);view.setUint16(18,900,true);
 const d=readBLog(buf.buffer);assert.equal(d.records.length,1);assert.equal(d.records[0].ms,123456);
 assert.equal(d.records[0].bytes[0],0x1c);
});
test('reject malformed read ranges',()=>{assert.throws(()=>readArgs(parseIni(ini),1,65530,16));assert.throws(()=>readArgs(parseIni(ini),1,0,1025))});

test('portable patch can be round-tripped safely',()=>{
 const def=parseIni(ini),field=def.cal.find(f=>f.name==='fuelTable');
 const original=new Uint8Array(field.byteLength),modified=original.slice();
 modified[0]=7;modified[5]=18;
 const patch=makePatch(def,field,original,modified);
 assert.deepEqual(validatePatch(def,field,original,JSON.parse(JSON.stringify(patch))),modified);
});
test('patch rejects different ECU, different field, stale source, and tampered payload',()=>{
 const def=parseIni(ini),field=def.cal.find(f=>f.name==='fuelTable');
 const original=new Uint8Array(field.byteLength),modified=original.slice();modified[2]=23;
 const patch=makePatch(def,field,original,modified);
 assert.throws(()=>validatePatch({...def,signature:'another ECU'},field,original,patch),/match/);
 assert.throws(()=>validatePatch(def,def.cal[1],original,patch),/match/);
 const changed=original.slice();changed[1]=1;
 assert.throws(()=>validatePatch(def,field,changed,patch),/differs/);
 assert.throws(()=>validatePatch(def,field,original,{...patch,modified:'bad'}),/checksum/);
});
