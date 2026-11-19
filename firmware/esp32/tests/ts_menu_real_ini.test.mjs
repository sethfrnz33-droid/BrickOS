import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {tunerStructure,parseIni} from '../web/core.mjs';
const ini=readFileSync(new URL('./fixtures/proteus_f7_generated.ini', import.meta.url),'utf8');
const struct=tunerStructure(ini),def=parseIni(ini);
test('menu comes from source INI with real Fuel and Setup menus',()=>{
 assert.ok(struct.categories.some(c=>c.name==='Fuel'));
 assert.ok(struct.categories.some(c=>c.name==='Setup'));
 assert.ok(struct.itemById.size>250);
});
test('VE menu resolves to its precise table and calibration field',()=>{
 const entry=struct.resolve('veTableDialog');
 assert.ok(entry.tables.some(x=>x.z==='veTable'));
 assert.ok(def.cal.some(x=>x.name==='veTable'&&x.height>1));
});
test('trigger and injector dialogs resolve their actual fields',()=>{
 assert.ok(struct.resolve('triggerConfiguration').fields.length>=10);
 assert.ok(struct.resolve('injectorConfig').fields.some(x=>x.name==='injector_flow'));
});
test('unknown dialog does not invent calibration fields',()=>{
 assert.deepEqual(struct.resolve('nonexistent_dialog_xyz'),{fields:[],tables:[]});
});
