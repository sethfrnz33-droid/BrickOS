import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {explainSetting,findSettings,answerGeneralQuestion} from '../web/guide-setting-help.mjs';
import {inspectIniCompatibility} from '../web/ini-compat.mjs';
import {GuideStory,validateStoryCatalog} from '../web/guide-story.mjs';
import {renderSethStory} from '../web/guide-story-ui.mjs';
const root=new URL('../web/',import.meta.url);
const catalog=JSON.parse(fs.readFileSync(new URL('guide-catalog.json',root),'utf8'));
const source=n=>fs.readFileSync(new URL(n,root),'utf8');

test('Seth is simply Seth; no clone identity in user-facing introduction',()=>{
 validateStoryCatalog(catalog);
 assert.doesNotMatch(catalog.nodes.welcome.text,/clone|ECU-bound|stuck/i);
 assert.doesNotMatch(catalog.character.description,/clone|trapped/i);
 assert.equal(catalog.tour_steps.length,10);
 assert.equal(Object.values(catalog.chapter_checks).reduce((sum,x)=>sum+x.length,0),30);
 const html=renderSethStory(new GuideStory(catalog).packet());
 assert.doesNotMatch(html,/Resident ECU clone/);
 assert.match(html,/data-seth-question-form/);
});
test('setting meanings are conservative and do not invent values',()=>{
 const injector=explainSetting({name:'injectorDeadTime',label:'Injector Dead Time',units:'ms',kind:'scalar'});
 assert.equal(injector.recognized,true);
 assert.equal(injector.can_recommend_value,false);
 assert.match(injector.check,/characterization/);
 const unknown=explainSetting({name:'proprietaryRegQ',label:'Mystery Setting'});
 assert.equal(unknown.recognized,false);
 assert.equal(unknown.certainty,'unknown_from_name');
 assert.doesNotMatch(unknown.summary,/\d+(?:\.\d+)?\s*(?:ms|cc|kPa)/i);
});
test('question matches only presented field names and offers a fall-through',()=>{
 const fields=[{name:'injectorDeadTime',label:'Injector Dead Time'},{name:'coolantSensor',label:'Coolant Temperature'}];
 const matches=findSettings('Can you explain injector dead time?',fields);
 assert.equal(matches[0].index,0);
 assert.equal(answerGeneralQuestion('What is VE?').stage,'fuel');
 assert.equal(answerGeneralQuestion('I have an entirely new ECU setting').stage,null);
});
test('Seth never turns a guessed field name into an executable editor action',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('mock');
 const e={inventory:[{name:'injectorDeadTime',label:'Injector Dead Time',kind:'scalar',id:'1:1:dt'}],
  fields:{},report:()=>({fresh:false,values:{}})};
 assert.ok(s.ask('What is injector dead time?'));
 const q=s.packet({engine:e,ecuKey:'mock'});
 assert.equal(q.node_id,'ask');
 assert.ok(q.choices.some(x=>x.go==='field.0'));
 s.go('field.0');
 const p=s.packet({engine:e,ecuKey:'mock'});
 assert.match(p.text,/injector/i);
 assert.equal(p.manual_target,null);
 assert.equal(p.choices.find(x=>x.go==='action.field.0').enabled,false);
});
test('families recognized only as definition labels, never automatically eligible for USB writes',()=>{
 for(const [signature,family] of [
  ['rusEFI master.2026.11.09.proteus_f7','rusEFI'],
  ['speeduino 2025','Speeduino'],
  ['MS3-Extra 1.6','MegaSquirt'],
  ['Custom TunerStudio','Not identified']
 ]){
  const ini='[TunerStudio]\nsignature = "'+signature+'"\n[Constants]\nnPages = 3\n[OutputChannels]\nrpm = scalar, U16, 0, "rpm", 1, 0';
  const data=inspectIniCompatibility(ini,{signature,cal:[],channels:[]});
  assert.equal(data.family_guess,family);
  assert.equal(data.mode,'definition_inspection_only');
  assert.equal(data.transport_compatible,false);
  assert.equal(data.write_supported,false);
 }
});
test('phone and ESP assets include new modules and animation respects reduced motion',()=>{
 const app=source('app.js'),voice=source('guide-voice.mjs'),css=source('app.css');
 const root=fs.readFileSync(new URL('../main/main.c',import.meta.url),'utf8');
 const generator=fs.readFileSync(new URL('../tools/generate_web_assets.py',import.meta.url),'utf8');
 for(const x of ['/guide-setting-help.mjs','/ini-compat.mjs'])assert.ok(root.includes(x));
 for(const x of ['guide-setting-help.mjs','ini-compat.mjs'])assert.ok(generator.includes(x));
 assert.match(app,/STORY\.ask\(text\)/);
 assert.match(app,/inspectIniCompatibility\(text,def\)/);
 assert.match(voice,/prefers-reduced-motion/);
 assert.match(css,/seth-portrait\[data-mood="bored"\]/);
});
