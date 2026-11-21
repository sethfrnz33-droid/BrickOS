import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {GuideStory,validateStoryCatalog} from '../web/guide-story.mjs';
import {renderSethStory} from '../web/guide-story-ui.mjs';
const catalog=validateStoryCatalog(JSON.parse(fs.readFileSync(new URL('../web/guide-catalog.json',import.meta.url),'utf8')));
const engine={fields:{rpm:{units:'RPM'}},inventory:[],report:()=>({fresh:false,values:{rpm:900}})};
test('all ten chapters contain three documented INI-aware questions',()=>{
 assert.equal(Object.keys(catalog.chapter_checks).length,10);
 for(const step of catalog.tour_steps){
  const questions=catalog.chapter_checks[step.id];assert.equal(questions.length,3);
  assert.ok(catalog.nodes[step.node].choices.some(c=>c.go==='check.'+step.id+'.0'));
  for(const q of questions){assert.ok(q.prompt&&q.why&&Array.isArray(q.roles));}
 }
});
test('every question returns a portable read-only packet without invented live data',()=>{
 for(const step of catalog.tour_steps){
  const s=new GuideStory(catalog);s.bindDefinition('ECU:INI-HASH-A');s.go('check.'+step.id+'.0');
  const p=s.packet({engine,ecuKey:'ECU:INI-HASH-A'});
  assert.equal(p.tour_step,step.step);assert.equal(p.check.index,1);assert.equal(p.check.total,3);
  assert.equal(p.check.status,'unreviewed');assert.equal(p.read_only,true);
  assert.ok(!p.choices.some(c=>/burn|write|engine.start/.test(c.go)));
 }
});
test('reported reviews are never confused with physical ECU verification',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('ECU:INI-HASH-A');s.go('check.references.0');
 s.go('answer.references.0.reported');let p=s.packet({engine,ecuKey:'ECU:INI-HASH-A'});
 assert.equal(p.check.status,'reported');assert.equal(p.check.requires_external,true);
 assert.match(p.check.status_meaning,/not independently verified/);
 assert.match(renderSethStory(p),/not ECU verification/);
 s.go('answer.references.0.unknown');p=s.packet({engine,ecuKey:'ECU:INI-HASH-A'});
 assert.equal(p.check.status,'unknown');
});
test('explanations, question flow and chapter summary navigate without tuning writes',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('ECU:INI-HASH-A');s.go('tour.idle');s.go('check.idle.0');
 s.go('why.idle.0');assert.match(s.packet({engine,ecuKey:'ECU:INI-HASH-A'}).text,/We can talk it through/);
 s.go('check.idle.1');s.go('answer.idle.1.unknown');s.go('check.idle.2');
 assert.equal(s.packet({engine,ecuKey:'ECU:INI-HASH-A'}).check.index,3);
 s.go('tour.idle');assert.equal(s.packet({engine,ecuKey:'ECU:INI-HASH-A'}).chapter_progress.unknown,1);
 assert.equal(typeof s.applyCalibration,'undefined');
});
test('new definition discards old question claims and answer state',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('ECU:INI-HASH-A');s.go('check.trigger.1');s.go('answer.trigger.1.reported');
 assert.equal(s.packet({engine,ecuKey:'ECU:INI-HASH-A'}).check.status,'reported');
 s.bindDefinition('ECU:INI-HASH-B');s.go('check.trigger.1');
 assert.equal(s.packet({engine,ecuKey:'ECU:INI-HASH-B'}).check.status,'unreviewed');
});
test('missing INI inputs remain missing; unsupported responses are rejected',()=>{
 const s=new GuideStory(catalog);s.go('check.steady_fuel.0');const p=s.packet({engine,ecuKey:''});
 assert.match(p.text,/not available yet/);assert.equal(p.read_only,true);
 assert.throws(()=>s.go('answer.trigger.99.reported'),/Unknown question response/);
 const bad=structuredClone(catalog);bad.chapter_checks.idle[0].id='';
 assert.throws(()=>validateStoryCatalog(bad),/Bad question/);
});
