import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';

const manifest=JSON.parse(fs.readFileSync(new URL('../../../design/research/source-role-candidates.v1.json',import.meta.url),'utf8'));
const guideSource=fs.readFileSync(new URL('../web/guided.mjs',import.meta.url),'utf8');
const begin=guideSource.indexOf('const ALIASES=');
const end=guideSource.indexOf('};',begin);
assert.ok(begin>=0&&end>begin,'Current guide candidate aliases must be discoverable');
const aliasText=guideSource.slice(begin,end+2);
const currentAliases=new Map();
for(const match of aliasText.matchAll(/([A-Za-z][A-Za-z0-9]*):\[([^\]]*)\]/g)){
 const names=Array.from(match[2].matchAll(/'([^']+)'/g),m=>m[1]);
 currentAliases.set(match[1],names);
}

test('role candidate ledger is research only; never a source of calibrated ECU commands',()=>{
 assert.equal(manifest.schema,'brickos.research.role-candidates.v1');
 assert.equal(manifest.status,'RESEARCH_ONLY_NOT_RUNTIME_BINDINGS');
 assert.equal(manifest.rules.requires_signature_match,true);
 assert.equal(manifest.rules.never_auto_bind_by_fuzzy_label,true);
 assert.equal(manifest.rules.never_fabricate_missing_value,true);
 assert.equal(manifest.rules.gdi_fuel_guidance,'unsupported');
 assert.equal(manifest.rules.writes,'not authorized');
});
test('all 25 current guide aliases are documented without claiming target verification',()=>{
 const fromSource=manifest.roles.filter(x=>x.kind==='observed_output_candidate');
 assert.equal(fromSource.length,currentAliases.size);
 for(const role of fromSource){
   assert.deepEqual(role.candidate_aliases,currentAliases.get(role.role),role.role);
   assert.equal(role.binding_status,'unverified_on_target',role.role);
   assert.equal(role.permission,'read_only',role.role);
   assert.equal(role.units,'from_matched_generated_ini',role.role);
 }
});
test('added source hints are never quietly treated as validated live output channels',()=>{
 const ids=new Set();
 for(const role of manifest.roles){
   assert.match(role.role,/^[A-Za-z][A-Za-z0-9]+$/);
   assert.ok(!ids.has(role.role),role.role);
   ids.add(role.role);
   assert.equal(role.value_if_missing,null);
   assert.equal(role.binding_status,'unverified_on_target');
   assert.equal(role.permission,'read_only');
   assert.ok(role.role_semantics.length>15);
 }
 assert.ok(ids.has('engineStateBits'));
 assert.ok(ids.has('activeVeTableAndAxes'));
 assert.ok(ids.has('pressureSensorMode'));
 assert.ok(ids.has('triggerToothData'));
 for(const role of manifest.roles.filter(x=>x.kind!=='observed_output_candidate'))
   assert.equal(role.candidate_aliases.length,0,'new source hints must not be runnable aliases');
});
