import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {parseIni,readArgs,burnArgs,calibrationWritePlan,putValue,getValue,relative,tunerStructure} from '../web/core.mjs';
const ini=readFileSync(new URL('./fixtures/proteus_f7_generated.ini',import.meta.url),'utf8');
const def=parseIni(ini),struct=tunerStructure(ini);
test('generated 2026 rusEFI escaped page identifiers form correct binary protocol bytes',()=>{
 assert.deepEqual(def.pageIdentifiers,[0,256,512,768,1024]);
 assert.equal(readArgs(def,2,0,64),'000100004000');
 assert.equal(burnArgs(def,5),'0004');
 assert.deepEqual(def.pageBurnable,[true,false,false,true,true]);
 assert.ok(def.pageWritable.every(Boolean));
});
test('VE axes are sourced from exact table editor definitions',()=>{
 const t=struct.tables.find(t=>t.z==='veTable');
 assert.equal(t.x,'veRpmBins');assert.equal(t.y,'veLoadBins');
 const f=def.cal.find(f=>f.name==='veTable');
 assert.equal(f.width,16);assert.equal(f.height,16);
 assert.equal(def.cal.find(f=>f.name===t.x).width,f.width);
 assert.equal(def.cal.find(f=>f.name===t.y).width,f.height);
});
test('multi-field staged writes merge into one page and preserve original bytes',()=>{
 const one={field:{page:1},start:10,original:Uint8Array.from([0,0,0,0,0]),current:Uint8Array.from([0,4,5,0,0])};
 const two={field:{page:1},start:20,original:Uint8Array.from([1,1,1]),current:Uint8Array.from([3,1,5])};
 const untouched={field:{page:2},start:0,original:new Uint8Array(4),current:new Uint8Array(4)};
 const plan=calibrationWritePlan([two,untouched,one]);
 assert.deepEqual(plan.map(e=>[e.page,e.offset,e.original.length]),[[1,11,2],[1,20,1],[1,22,1]]);
 assert.deepEqual([...plan[0].original],[0,0]);
 assert.deepEqual([...plan[0].modified],[4,5]);
});
test('write plan chunking and overlap conflicts are verified before any ECU transmission',()=>{
 const original=new Uint8Array(260),current=new Uint8Array(260).fill(2);
 const p=calibrationWritePlan([{field:{page:1},start:20,original,current}]);
 assert.deepEqual(p.map(x=>x.modified.length),[128,128,4]);
 assert.throws(()=>calibrationWritePlan([
  {field:{page:1},start:4,original:new Uint8Array([0]),current:new Uint8Array([1])},
  {field:{page:1},start:4,original:new Uint8Array([0]),current:new Uint8Array([2])},
 ]),/Overlapping/);
 assert.throws(()=>calibrationWritePlan([{field:{page:1},start:65535,original:new Uint8Array(2),current:Uint8Array.from([1,1])}]),/outside page/);
});
test('axis raw fallback preserves bytes when INI conversion depends on ECU settings',()=>{
 const axis=def.cal.find(x=>x.name==='veLoadBins');assert.equal(axis.scale,null);
 const usable={...axis,scale:1,translate:0,editable:true};
 const bytes=new Uint8Array(axis.byteLength),f=relative(usable,axis.offset);
 putValue(bytes,f,3,187,def.endianness);assert.equal(getValue(bytes,f,3,def.endianness),187);
});
