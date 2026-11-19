import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';

const catalog=JSON.parse(fs.readFileSync(new URL('../../../design/education/lesson-catalog.v1.json',import.meta.url),'utf8'));
const hardware=JSON.parse(fs.readFileSync(new URL('../../../design/engine-build/profile-contract.v1.json',import.meta.url),'utf8'));

test('education catalog is an original, design-only portable source',()=>{
  assert.equal(catalog.schema,'brickos.guide.education.catalog.v1');
  assert.equal(catalog.status,'DESIGN_ONLY_OFFLINE_SIMULATION_NOT_ESP_READER');
  assert.equal(catalog.chapter_order.length,10);
  assert.equal(new Set(catalog.chapter_order).size,10);
  assert.ok(catalog.lessons.length>=20);
  assert.equal(catalog.lesson_rules.check_answers,'education_only_not_hardware_validation');
  assert.equal(hardware.schema,'brickos.engine.profile.contract.v1');
});
test('all lessons have stable IDs, four depth variants and explanations',()=>{
  const ids=new Set(catalog.lessons.map(l=>l.id));
  assert.equal(ids.size,catalog.lessons.length);
  for(const l of catalog.lessons){
    assert.match(l.id,/^[a-z][a-z0-9_]+$/);
    assert.ok(catalog.chapter_order.includes(l.chapter),l.id);
    assert.equal(l.no_calibration_actions,true,l.id);
    assert.equal(l.check.kind,'concept_only',l.id);
    assert.equal(l.check.correct_index>=0&&l.check.correct_index<l.check.options.length,true,l.id);
    assert.equal(l.check.options.length,3,l.id);
    assert.ok(l.check.prompt.trim().length>12,l.id);
    assert.ok(l.check.explanation.trim().length>16,l.id);
    assert.ok(l.if_confused?.alternate?.trim().length>20,l.id);
    assert.match(l.unavailable_fallback,/does not currently expose verified matching data/);
    for(const depth of ['quick','guided','lesson','technical'])assert.ok(l.variants[depth]?.length>20,l.id+': '+depth);
    assert.ok(Array.isArray(l.semantic_roles));
    for(const p of l.prerequisites) assert.ok(ids.has(p),l.id+' -> '+p);
  }
});
test('prerequisite graph is cycle-free and every chapter has a lesson',()=>{
  const seen=new Map(),lookup=new Map(catalog.lessons.map(l=>[l.id,l]));
  const visit=id=>{
    assert.notEqual(seen.get(id),1,'cycle at '+id);
    if(seen.get(id)===2)return;
    seen.set(id,1);
    for(const dep of lookup.get(id).prerequisites)visit(dep);
    seen.set(id,2);
  };
  for(const id of lookup.keys())visit(id);
  for(const chapter of catalog.chapter_order)assert.ok(catalog.lessons.some(l=>l.chapter===chapter),chapter);
});
test('critical learning distinctions are represented without unsupported engine conclusions',()=>{
  const ids=new Set(catalog.lessons.map(x=>x.id));
  for(const id of ['pump_vs_regulator','pressure_reference','gdi_not_supported',
    'fuel_trim_masking','lambda_vs_target','trigger_sync','knock_not_mbt','final_audit'])assert.ok(ids.has(id));
  const p=catalog.lessons.find(x=>x.id==='pump_vs_regulator');
  assert.match(p.variants.guided,/not whether a regulator exists/i);
  const gdi=catalog.lessons.find(x=>x.id==='gdi_not_supported');
  assert.match(gdi.variants.quick,/does not support GDI/i);
});
