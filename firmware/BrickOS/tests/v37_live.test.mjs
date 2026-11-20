import assert from 'node:assert/strict';
import test from 'node:test';
import {decodeLiveFrame,parseIni,getValue} from '../web/core.mjs';
import {readFileSync} from 'node:fs';

function frame(payload,seq=9){
 const result=new Uint8Array(16+payload.length),view=new DataView(result.buffer);
 result.set([66,76,86,49]);view.setUint32(4,seq,true);view.setUint32(8,14500,true);
 view.setUint16(12,payload.length,true);view.setUint16(14,3,true);
 result.set(payload,16);return result;
}

test('little endian BLV1 binary telemetry decodes without JSON or hex overhead',()=>{
 const payload=Uint8Array.from([9,8,7,6,5,4]);
 const data=decodeLiveFrame(frame(payload,0x12345678));
 assert.equal(data.sequence,0x12345678);
 assert.equal(data.timestamp,14500);
 assert.equal(data.length,6);
 assert.equal(data.usbDurationMs,3);
 assert.deepEqual(data.bytes,payload);
});
test('BLV1 refuses corrupted, truncated and over-sized data',()=>{
 const sample=frame(Uint8Array.from([0,0,0,0]));
 assert.throws(()=>decodeLiveFrame(sample.slice(0,18)),/Incomplete|Invalid/);
 const corrupted=sample.slice();corrupted[0]=0;
 assert.throws(()=>decodeLiveFrame(corrupted),/header/);
 const bad=sample.slice();bad[12]=255;
 assert.throws(()=>decodeLiveFrame(bad),/Incomplete/);
});
test('generated Proteus F7 runtime offsets are extracted directly from the INI',()=>{
 const ini=readFileSync(new URL('./fixtures/rusefi_proteus_f7.ini',import.meta.url),'utf8');
 const def=parseIni(ini),rpm=def.channels.find(x=>x.name==='RPMValue');
 const raw=new Uint8Array(768),v=new DataView(raw.buffer);
 assert.ok(rpm.offset+rpm.byteLength<=768);
 v.setUint16(rpm.offset,2000,true);
 const read=getValue(decodeLiveFrame(frame(raw)).bytes,rpm,0,def.endianness);
 assert.equal(read,2000*rpm.scale+rpm.translate);
});

test('BLV1 supports a complete 2248-byte Proteus runtime output capture',()=>{
 const payload=new Uint8Array(2248);payload[2200]=0x42;
 const decoded=decodeLiveFrame(frame(payload,12));
 assert.equal(decoded.length,2248);
 assert.equal(decoded.bytes[2200],0x42);
});
