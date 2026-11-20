import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

const a=JSON.parse(readFileSync(new URL('../../../design/ini/proteus-f7-2026-11-09-source-audit.v1.json',import.meta.url),'utf8'));
const typeBytes={U08:1,S08:1,U16:2,S16:2,U32:4,S32:4,F32:4};
const fields=a.primary_calibration;

test('actual supplied-generated-INI source metadata stays definition-only',()=>{
 assert.equal(a.schema,'brickos.ini.source.audit.v1');
 assert.equal(a.status,'SOURCE_VERIFIED_INI_ONLY_NOT_MATCHED_TO_CONNECTED_ECU');
 assert.match(a.source.sha256,/^[a-f0-9]{64}$/);
 assert.equal(a.source.signature,'rusEFI master.2026.11.09.proteus_f7.2735508644');
 assert.equal(a.source.bytes,943745);
 assert.equal(a.inventory.typed_calibration_definitions,1977);
 assert.equal(a.inventory.typed_output_definitions,1081);
 assert.equal(a.inventory.sections,24);
 assert.equal(a.transport.och_block_size_bytes,2248);
 assert.equal(a.transport.requested_tunerstudio_record_rate_hz,100);
 assert.ok(a.transport.max_configuration_read_chunk_bytes<a.transport.och_block_size_bytes);
 assert.match(a.transport.notes,/No actual sustained/);
});
test('each declared canonical table fits its own actual INI page, with no invented 8x8 VE',()=>{
 const sizes=new Map(a.pages.map(p=>[p.ordinal,p.bytes]));
 for(const [id,f] of Object.entries(fields)){
  const t=f.type.split(' ').at(-1);
  const elem=typeBytes[t];
  if(!elem||!Array.isArray(f.shape)||!f.shape.every(Number.isInteger))continue;
  const extent=f.shape.reduce((v,x)=>v*x,1)*elem;
  assert.ok(f.offset>=0&&f.offset+extent<=sizes.get(f.page),id+' exceeds page '+f.page);
 }
 assert.deepEqual(fields.veTable.shape,[16,16]);
 assert.deepEqual(fields.idleVeTable.shape,[4,4]);
 assert.deepEqual(fields.secondVeTable.shape,[16,16]);
 assert.deepEqual(fields.lambdaTable.shape,[16,16]);
 assert.deepEqual(fields.ltft_table_bank1.shape,[16,16]);
 assert.deepEqual(fields.ltft_table_bank2.shape,[16,16]);
 assert.equal(a.pages.find(p=>p.ordinal===3).burn_declared,false);
 assert.equal(a.pages.find(p=>p.ordinal===2).burn_declared,false);
 assert.equal(fields.secondVeTable.page,4);
});
test('display aliases refer to the same byte storage only once',()=>{
 assert.equal(a.display_aliases.length,4);
 const map=new Map();
 for(const x of a.display_aliases){
  assert.ok(x.bytes>0);
  assert.ok(!map.has(x.alias),x.alias);
  map.set(x.alias,x);
 }
 assert.equal(map.get('wueAnalyzeTargetTable').same_storage,'lambdaTable');
 assert.equal(map.get('wueAnalyzeTargetTable').offset,fields.lambdaTable.offset);
});
test('fuel evidence, spark, sync and pressure remain distinct semantic sources',()=>{
 const role=a.output_examples;
 assert.ok(role.stft1.scale.includes('−1'));
 assert.notEqual(role.stft1.scale,role.gego.scale);
 assert.equal(role.engineBits.offset,814);
 assert.equal(role.triggerErrors.offset,116);
 assert.equal(role.baseSpark.offset,984);
 assert.equal(role.actualSpark.offset,986);
 assert.equal(role.knockRetard.offset,1036);
 assert.equal(role.lowFuelPressure.offset,106);
 assert.equal(role.pressureTarget.offset,1342);
 assert.equal(role.pumpDuty.offset,1340);
 assert.ok(a.guide_research_conclusions.some(x=>x.includes('GDI')));
});
test('nonstandard source declarations remain explicitly unparsed for review',()=>{
 assert.equal(a.nonstandard_fields.length,1);
 assert.equal(a.nonstandard_fields[0].names.length,5);
 assert.equal(a.special_logger_definitions.length,3);
 assert.ok(a.inventory.command_buttons>=100);
});
