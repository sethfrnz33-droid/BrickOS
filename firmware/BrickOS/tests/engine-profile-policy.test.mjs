import test from 'node:test';
import assert from 'node:assert/strict';
import {PROFILE_SCHEMA,PROFILE_FIELDS,newEngineProfile,recordBuildFact,
  linkECUDefinition,evaluateEngineProfile} from '../../../prototype/engine-profile-policy.mjs';

const set=(p,key,value,extra={})=>recordBuildFact(p,key,value,extra);
const codes=(p,opts={})=>evaluateEngineProfile(p,opts).findings.map(x=>x.code);

test('engine profile is a durable separate object with unknown facts and no ECU writes',()=>{
 const p=newEngineProfile('project-1','My engine','2026-11-19T00:00:00Z');
 assert.equal(p.schema,PROFILE_SCHEMA);
 assert.ok(PROFILE_FIELDS.length>=15);
 assert.equal(p.facts.injection_architecture.value,null);
 assert.equal(evaluateEngineProfile(p).next_question.id,'injection_architecture');
 assert.equal(evaluateEngineProfile(p).auto_write_enabled,false);
 assert.equal(evaluateEngineProfile(p).read_only,true);
});
test('GDI and mixed GDI never enter PFI guide/autotune even if ECU definition matches',()=>{
 for(const kind of ['gdi','mixed']){
  let p=newEngineProfile('engine-'+kind);
  p=set(p,'injection_architecture',kind);
  let d=evaluateEngineProfile(p,{ecuInjection:'pfi',definitionMatched:true});
  assert.equal(d.experts.fuel,'unsupported_gdi');
  assert.ok(d.findings.some(x=>x.code==='UNSUPPORTED_GDI'&&x.blocking));
  assert.equal(d.auto_write_enabled,false);
  assert.equal(d.questions.length,0);
 }
});
test('PWM pump control alone never assumes there is no mechanical regulator',()=>{
 let p=newEngineProfile('pwm-test');
 p=set(p,'injection_architecture','pfi');
 p=set(p,'pump_control','pwm_compatible_controller');
 const r=evaluateEngineProfile(p,{ecuInjection:'pfi',definitionMatched:true});
 assert.equal(p.facts.pressure_regulation.value,null);
 assert.ok(r.questions.some(q=>q.id==='pressure_regulation'));
 assert.ok(codes(p,{ecuInjection:'pfi',definitionMatched:true}).includes('PRESSURE_REGULATION_UNKNOWN'));
 assert.ok(!codes(p,{ecuInjection:'pfi',definitionMatched:true}).includes('PRESSURE_CONTROL_UNVERIFIED'));
});
test('PWM with mechanical regulator preserves both realities and requires independent pressure proof',()=>{
 let p=newEngineProfile('pwm-with-regulator');
 p=set(p,'injection_architecture','pfi');
 p=set(p,'pump_control','pwm_compatible_controller');
 p=set(p,'pressure_regulation','mechanical_manifold_reference');
 p=set(p,'rail_pressure_sensor','ecu_sensor');
 p=set(p,'pressure_reference','differential');
 const d=evaluateEngineProfile(p,{ecuInjection:'pfi',definitionMatched:true});
 assert.ok(d.findings.some(x=>x.code==='PWM_AND_MECHANICAL'));
 assert.ok(d.findings.some(x=>x.code==='PRESSURE_VERIFICATION_REQUIRED'));
 assert.equal(d.experts.fuel,'pfi_review_only');
 assert.equal(d.auto_write_enabled,false);
});
test('No known regulator or rail sensor never makes pressure verified',()=>{
 let p=newEngineProfile('no-regulator');
 p=set(p,'injection_architecture','pfi');
 p=set(p,'pump_control','pwm_compatible_controller');
 p=set(p,'pressure_regulation','no_confirmed_regulator');
 p=set(p,'rail_pressure_sensor','none');
 const d=evaluateEngineProfile(p,{ecuInjection:'pfi',definitionMatched:true});
 assert.ok(d.findings.some(x=>x.code==='PRESSURE_CONTROL_UNVERIFIED'&&x.blocking));
 assert.ok(d.findings.some(x=>x.code==='NO_ECU_RAIL_PRESSURE'&&x.blocking));
 assert.equal(d.summary_status,'needs_reconciliation_or_evidence');
});
test('User reports never impersonate independent pressure documentation',()=>{
 let p=newEngineProfile('proof');
 assert.throws(()=>set(p,'independent_pressure_check','documented',{source:'user_reported',verification:'documented_external',evidence_ref:'a'}),/Independent documentation/);
 assert.throws(()=>set(p,'independent_pressure_check','documented',{source:'independent_documented',verification:'documented_external'}),/Independent documentation/);
 p=set(p,'independent_pressure_check','reported_checked',{source:'user_reported',verification:'reported_checked'});
 assert.equal(p.facts.independent_pressure_check.verification,'reported_checked');
 p=set(p,'independent_pressure_check','documented',{source:'independent_documented',verification:'documented_external',evidence_ref:'external-record-7'});
 assert.equal(p.facts.independent_pressure_check.verification,'documented_external');
 assert.equal(p.events.length,2);
});
test('New INI linked to same engine never destroys its build facts',()=>{
 let p=newEngineProfile('my-engine');
 p=set(p,'injection_architecture','pfi');
 p=set(p,'engine_family','anonymized project engine');
 const before=p.facts.engine_family;
 p=linkECUDefinition(p,{signature:'ECU-SIGNATURE-A',ini_fingerprint:'ini-hash-A'});
 p=linkECUDefinition(p,{signature:'ECU-SIGNATURE-B',ini_fingerprint:'ini-hash-B'});
 assert.equal(p.facts.engine_family,before);
 assert.equal(p.ecu_links.length,2);
 assert.equal(p.revision,4);
});
test('ECU injection conflict blocks PFI assumptions without overwriting user hardware fact',()=>{
 let p=newEngineProfile('disagreement');
 p=set(p,'injection_architecture','pfi');
 const d=evaluateEngineProfile(p,{ecuInjection:'gdi',definitionMatched:true});
 assert.ok(d.findings.some(x=>x.code==='INJECTION_CONFLICT'&&x.blocking));
 assert.equal(d.experts.fuel,'hold');
 assert.equal(p.facts.injection_architecture.value,'pfi');
});
test('A documented PFI mechanical profile remains read-only even with evidence',()=>{
 let p=newEngineProfile('pfi-known');
 for(const [k,v] of [['injection_architecture','pfi'],['pump_control','relay'],
  ['pressure_regulation','mechanical_manifold_reference'],['rail_pressure_sensor','ecu_sensor'],
  ['pressure_reference','differential'],['injector_data','injector-reference-1']])p=set(p,k,v);
 p=set(p,'independent_pressure_check','documented',{source:'independent_documented',verification:'documented_external',evidence_ref:'pressure-test-record'});
 const d=evaluateEngineProfile(p,{ecuInjection:'pfi',definitionMatched:true});
 assert.equal(d.summary_status,'review_only');
 assert.equal(d.experts.fuel,'pfi_review_only');
 assert.equal(d.auto_write_enabled,false);
});
