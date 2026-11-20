import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {GuideStory,validateStoryCatalog,findManualTarget,STORY_STAGE_HINTS} from '../web/guide-story.mjs';
import {renderSethStory} from '../web/guide-story-ui.mjs';
const catalog=JSON.parse(fs.readFileSync(new URL('../web/guide-catalog.json',import.meta.url),'utf8'));
const engine={fields:{rpm:{name:'RPMValue',units:'RPM'},lambda:{name:'lambdaValue',units:''}},report:()=>({fresh:true,values:{rpm:1000,lambda:1}})};
const menuTree={itemById:new Map([['fuel:ve',{id:'fuel:ve',label:'VE Table',group:'Fuel'}],['idle:control',{id:'idle:control',label:'Idle Control',group:'Idle'}]])};
test('full versioned catalogue is connected and portable',()=>{
 validateStoryCatalog(catalog);assert.ok(Object.keys(catalog.nodes).length>=80);
 for(const n of Object.values(catalog.nodes))for(const c of n.choices)assert.ok(c.label.length>3);
});
test('full tour and targeted help independently navigate without fixed ECU offsets',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('sample-ECU-hash-A');s.go('tour.start');s.go('tour.identity');
 assert.equal(s.packet({engine,menuTree,ecuKey:'sample-ECU-hash-A'}).node_id,'tour.identity');
 s.go('targeted');s.go('topics.idle');s.go('issue.idle.surge');
 assert.match(s.packet({engine,menuTree,ecuKey:'sample-ECU-hash-A'}).text,/idle actuator/);
});
test('changing definition invalidates last step; no stale INI binding',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('ECU-A');s.go('tour.idle');s.bindDefinition('ECU-B');
 assert.equal(s.packet({ecuKey:'ECU-B'}).node_id,'welcome');assert.equal(s.canResume(),false);
});
test('missing INI role stays explicitly unavailable',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('ECU-A');s.go('evidence.idle');const p=s.packet({engine,menuTree,ecuKey:'ECU-A'});
 assert.match(p.text,/not exposed by matched INI/);assert.match(p.text,/rpm: 1000 RPM/);
});
test('no false sensor reading when source stale',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('ECU-A');s.go('evidence.fuel');const p=s.packet({engine:{...engine,report:()=>({fresh:false,values:{rpm:5000,lambda:2}})},ecuKey:'ECU-A'});
 assert.match(p.text,/live value unknown/);assert.doesNotMatch(p.text,/lambda: 2/);
});
test('manual navigation accepts only actual resolved INI menu targets',()=>{
 assert.equal(findManualTarget('fuel',menuTree)?.id,'fuel:ve');assert.equal(findManualTarget('spark',menuTree),null);
 const s=new GuideStory(catalog);s.bindDefinition('ECU-A');s.go('manual.spark');const p=s.packet({engine,menuTree,ecuKey:'ECU-A'});
 assert.equal(p.choices.find(x=>x.go.startsWith('action.manual.')).enabled,false);
});
test('user-facing text is escaped before HTML rendering, and no ECU write actions exist',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('ECU-A');s.go('evidence.idle');const p=s.packet({engine,menuTree,ecuKey:'ECU-A'});
 p.text='<script>bad()</script>';const html=renderSethStory(p);
 assert.ok(html.includes('&lt;script&gt;'));assert.ok(!html.includes('<script>bad()'));
 assert.ok(!html.includes('data-guide-mode="burn"'));assert.match(html,/seth-mouth/);
});
test('back, resume, refusal of unknown node and no implicit mutation',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('ECU-A');s.go('targeted');s.go('topics.start');s.go('back');assert.equal(s.nodeId,'targeted');
 assert.throws(()=>s.go('random.unverified.target'),/Unknown dialogue step/);s.go('topics.idle');s.go('welcome');s.go('resume.last');assert.equal(s.nodeId,'topics.idle');
 assert.equal(typeof s.applyCalibration,'undefined');
});
test('ESP static asset routes have no POST write paths',()=>{
 const main=fs.readFileSync(new URL('../main/main.c',import.meta.url),'utf8');
 assert.match(main,/\/api\/guide\/v1\/catalog/);
 assert.match(main,/\/guide\/voice\.mp3/);assert.match(main,/brick_asset_seth_jaw/);
 const routes=main.match(/\{\.uri="\/api\/guide[^\n]+/g)||[];assert.equal(routes.length,1);assert.match(routes[0],/HTTP_GET/);
});
test('generic field-by-field tour includes unknown INI fields without inventing interpretations',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('ECU-A');s.go('atlas.entry');s.go('field.0');
 const e={...engine,inventory:[{name:'mysteryFutureSetting',label:'mystery future setting',id:'9:123:mysteryFutureSetting',page:9,kind:'scalar',type:'U16',units:'%'},{name:'veTable',label:'VE table',id:'1:3:veTable',page:1,kind:'array',type:'U08',units:''}]};
 const p=s.packet({engine:e,menuTree,menuLinks:{veTable:'fuel:ve'},ecuKey:'ECU-A'});
 assert.match(p.text,/mystery future setting/);assert.match(p.text,/does not prove what the setting should be/);
 assert.equal(p.choices.find(x=>x.go==='action.field.0').enabled,false);
 s.go('field.1');const q=s.packet({engine:e,menuTree,menuLinks:{veTable:'fuel:ve'},ecuKey:'ECU-A'});
 assert.equal(q.manual_target.id,'fuel:ve');assert.equal(q.field_id,'1:3:veTable');
});

test('catalogue role mappings can be replaced with a new INI variant without new client code',async()=>{
 const {GuidedAnalyzer}=await import('../web/guided.mjs');const g=new GuidedAnalyzer();
 g.setRoleAliases({rpm:['newFirmwareRpmName']});assert.equal(g.roleAliases.rpm[0],'newFirmwareRpmName');
 assert.throws(()=>g.setRoleAliases({rpm:[null]}),/Invalid role alias/);
});

test('ten published conceptual stages form the default full-tour route',()=>{
 assert.equal(catalog.tour_steps.length,10);
 assert.deepEqual(catalog.tour_steps.map(s=>s.step),[1,2,3,4,5,6,7,8,9,10]);
 const s=new GuideStory(catalog);s.bindDefinition('ECU-A');s.go('tour.start');
 assert.equal(catalog.nodes['tour.start'].choices[0].go,catalog.tour_steps[0].node);
 for(let i=0;i<10;i++){
  const step=catalog.tour_steps[i];s.go(step.node);const packet=s.packet({engine,menuTree,ecuKey:'ECU-A'});
  assert.equal(packet.tour_step,i+1);assert.equal(packet.tour_total,10);
  assert.equal(packet.tour_title,step.title);assert.ok(packet.read_only);
  if(i<9)assert.ok(catalog.nodes[step.node].choices.some(c=>c.go===catalog.tour_steps[i+1].node));
  assert.match(renderSethStory(packet),new RegExp('STEP '+(i+1)+' / 10'));
 }
});
test('quick help stays independent and the HPA tour adapts to INI evidence availability',()=>{
 const s=new GuideStory(catalog);s.bindDefinition('ECU-A');s.go('tour.base_tables');
 const p=s.packet({engine,menuTree,ecuKey:'ECU-A'});
 assert.equal(p.tour_step,3);assert.ok(p.choices.some(c=>c.go==='manual.fuel'));
 s.go('targeted');s.go('topics.idle');
 assert.equal(s.packet({engine,menuTree,ecuKey:'ECU-A'}).tour_step,null);
 s.go('tour.reference');const q=s.packet({engine,menuTree,ecuKey:'ECU-A'});
 assert.equal(q.tour_step,4);assert.match(q.text,/external verification/);
 const missing=s.packet({engine:{fields:{},report:()=>({fresh:false,values:{}})},ecuKey:'ECU-A'});
 assert.equal(missing.tour_step,4);assert.equal(missing.manual_target,null);
});
