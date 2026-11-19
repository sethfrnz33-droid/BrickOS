import test from 'node:test';
import assert from 'node:assert/strict';
import {INI_FAST_SCHEMA,validLocalDefinition,readLocalIni,writeLocalIni,clearLocalIni} from '../web/ini-fast-cache.mjs';

const hash='0123456789abcdef'.repeat(4);
const other='abcdef0123456789'.repeat(4);
const sig='rusEFI master.2026.11.09.proteus_f7.2735508644';
const rawIni=[
  '; generated ini fixture','[MegaTune]','someFlag = 1',
  '[TunerStudio]','signature= "'+sig+'"',
  '[Constants]','nPages=1','pageSize=100','page=1','foo = scalar, U16, 0, "RPM", 1, 0',
  '[OutputChannels]','ochBlockSize=40','bar = scalar, U16, 0, "RPM", 1, 0'
].join('\n');
const definition={signature:sig,cal:[{name:'foo',offset:0,page:1,type:'U16'}],
 channels:[{name:'bar',offset:0,type:'U16'}],pageSizes:[100],pageIdentifiers:[0]};
const record={schema:INI_FAST_SCHEMA,key:'single',hash,rawIni,definition,size:945000};

test('saved parsed INI is reused only with same content hash and exact ECU signature',()=>{
 assert.equal(validLocalDefinition(record,hash,sig,945000),true);
 assert.equal(validLocalDefinition(record,other,sig,945000),false);
 assert.equal(validLocalDefinition(record,hash,'rusEFI wrong ECU',945000),false);
 assert.equal(validLocalDefinition(record,hash,sig,945001),false);
});
test('source text and compiled object cannot silently disagree',()=>{
 assert.equal(validLocalDefinition({...record,definition:{...definition,signature:'bad'}},hash),false);
 assert.equal(validLocalDefinition({...record,rawIni:'garbage data'},hash),false);
 assert.equal(validLocalDefinition({...record,definition:{...definition,channels:[]}},hash),false);
 assert.equal(validLocalDefinition({...record,schema:99},hash),false);
});
test('malformed hashes and missing saved data never become trusted INI',()=>{
 assert.equal(validLocalDefinition(record,'deadbeef',sig),false);
 assert.equal(validLocalDefinition(null,hash),false);
 assert.equal(validLocalDefinition({...record,hash:''},hash),false);
});
test('on devices without IndexedDB, loader fails safely and network fallback remains possible',async()=>{
 assert.equal(await readLocalIni(hash,sig),null);
 assert.equal(await writeLocalIni(hash,rawIni,definition,rawIni.length),false);
 await clearLocalIni();
});
