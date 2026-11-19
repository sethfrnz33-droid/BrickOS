import test from 'node:test';
import assert from 'node:assert/strict';
import {analyzeSeth,normalizeReasoningStage,sethReasoningSummary} from '../web/guide-reasoning.mjs';

const now=100000;
function fixture({roles={},values={},recent=[],fresh=true,matched=true}={}){
 const v={rpm:950,clt:80,tps:1,map:45,lambda:1,target:1,gego:100,...values};
 const fields=Object.fromEntries(Object.keys({...v,...roles}).filter(k=>roles[k]!==false).map(k=>[k,{units:k==='idleCommand'?'%':''}]));
 return {def:matched?{signature:'A'}:null,signature:matched?'A':'',fields,report:()=>({signature:matched?'A':'',fresh,values:v,recent})};
}
const history=(values,length=12,interval=350,start=92000)=>Array.from({length},(_,i)=>({ms:start+i*interval,values:typeof values==='function'?values(i):{...values}}));
test('normalizes HPA chapters to the underlying expert',()=>{
 assert.equal(normalizeReasoningStage('steady_fuel'),'fuel');assert.equal(normalizeReasoningStage('references'),'trigger');
 assert.equal(normalizeReasoningStage('steady_ignition'),'spark');
});
test('INI mismatch stops diagnosis and asks for matching definition',()=>{
 const x=analyzeSeth(fixture({matched:false}),{stage:'idle',now});
 assert.equal(x.freshness,'definition unavailable');assert.equal(x.blocking,true);
 assert.equal(x.questions[0].id,'definition');assert.equal(x.read_only,true);
 assert.ok(!x.facts.length);
});
test('stale data cannot become a new live hypothesis',()=>{
 const x=analyzeSeth(fixture({fresh:false}),{stage:'idle',now});
 assert.equal(x.freshness,'stale');assert.equal(x.confidence,'unavailable');assert.equal(x.questions[0].id,'fresh');
});
test('missing INI role leaves uncertainty, no numeric assumptions',()=>{
 const x=analyzeSeth(fixture({roles:{idleTarget:false,idleCommand:false}}),{stage:'idle',now});
 assert.ok(x.missing_inputs.some(e=>e.role==='idleTarget'&&e.source==='ini'));
 assert.ok(x.questions.some(e=>e.id==='idleTarget'));assert.ok(x.questions.some(e=>e.id==='idleCommand'));
});
test('recurring RPM target misses produce bounded alternative explanations, not automatic PID edit',()=>{
 const recent=history(i=>({rpm:800+(i%2)*12,idleTarget:950,idleCommand:45,tps:1,map:42,lambda:1}));
 const x=analyzeSeth(fixture({values:recent.at(-1).values,recent}),{stage:'idle',now});
 const h=x.hypotheses.find(h=>h.id==='idle-target-mismatch');
 assert.ok(h);assert.equal(h.confidence,'window-pattern');assert.ok(h.causes.length>=3);
 assert.match(h.missing.join(' '),/actuator/i);
 assert.equal(x.read_only,true);
});
test('single transient is never called a repeatable idle fault',()=>{
 const recent=history(i=>({rpm:i===5?500:950,idleTarget:950,idleCommand:30}),7,100,99000);
 const x=analyzeSeth(fixture({recent}),{stage:'idle',now});
 assert.ok(!x.hypotheses.some(h=>h.id==='idle-target-mismatch'));
});
test('trim masking is a contradiction, not proof of optimal VE',()=>{
 const recent=history(()=>({rpm:1700,tps:1,map:40,lambda:1,target:1,gego:109}));
 const x=analyzeSeth(fixture({recent}),{stage:'fuel',now});
 assert.ok(x.contradictions.some(c=>c.id==='lambda-trim-masking'));
 assert.ok(x.hypotheses.some(h=>h.id==='feedback-masked-base-fuel'));
 assert.match(sethReasoningSummary(x),/contradiction/i);
});
test('trigger error counter change is flagged as observation, not mechanical diagnosis',()=>{
 const recent=history(i=>({rpm:900,sync:i<6?2:i<9?3:4}));
 const x=analyzeSeth(fixture({recent}),{stage:'trigger',now});
 const h=x.hypotheses.find(h=>h.id==='sync-counter-increase');
 assert.ok(h);assert.equal(h.kind,'observation');assert.ok(h.causes.length>=2);
});
test('spark analysis never suggests timing advance from knock or MAP alone',()=>{
 const recent=history(i=>({spark:18,knock:i<6?0:1,rpm:3000,map:110}));
 const x=analyzeSeth(fixture({recent}),{stage:'spark',now});
 assert.ok(x.hypotheses.some(h=>h.id==='mbt-unmeasured'));
 assert.ok(x.unsupported_inferences.includes('MBT from MAP or motion alone'));
 assert.ok(!JSON.stringify(x).includes('Apply +1'));
});
test('uncertain data gives question prioritization without fake percentages',()=>{
 const x=analyzeSeth(fixture({roles:{lambdaGood:false,target:false,gego:false}}),{stage:'fuel',now});
 assert.ok(x.questions.length>=2);assert.ok(x.questions.length<=4);
 assert.ok(x.questions.every(q=>typeof q.reason==='string'&&typeof q.weight==='number'));
 assert.ok(!('probability' in x));
});
import fs from 'node:fs';
import {GuideStory,validateStoryCatalog} from '../web/guide-story.mjs';
import {renderSethStory} from '../web/guide-story-ui.mjs';
test('Seth Full Tour and Quick Help both open same INI-gated reasoning surface',()=>{
 const catalog=validateStoryCatalog(JSON.parse(fs.readFileSync(new URL('../web/guide-catalog.json',import.meta.url),'utf8')));
 const story=new GuideStory(catalog);story.bindDefinition('A:ini');story.go('tour.idle');
 const e=fixture({roles:{idleTarget:false}});
 let p=story.packet({engine:e,ecuKey:'A:ini'});
 assert.ok(p.choices.some(c=>c.go==='diagnosis.idle'));
 story.go('diagnosis.idle');p=story.packet({engine:e,ecuKey:'A:ini'});
 assert.equal(p.reasoning.schema,'brickos.guide.reasoning.v1');assert.equal(p.read_only,true);
 assert.match(renderSethStory(p),/What Seth found/);
 story.go('diagnosis.question.idle');p=story.packet({engine:e,ecuKey:'A:ini'});
 assert.ok(p.text.length>50);
 story.go('topics.idle');p=story.packet({engine:e,ecuKey:'A:ini'});
 assert.ok(p.choices.some(c=>c.go==='diagnosis.idle'));
});
test('output escapes sensor-derived values, never renders unsanitized HTML',()=>{
 const story=new GuideStory(validateStoryCatalog(JSON.parse(fs.readFileSync(new URL('../web/guide-catalog.json',import.meta.url),'utf8'))));
 story.bindDefinition('test');story.go('diagnosis.idle');const e=fixture();e.signature='<img src=x onerror=alert(1)>';e.report=()=>({signature:e.signature,fresh:true,values:{rpm:900},recent:[]});
 const p=story.packet({engine:e,ecuKey:'test'}),html=renderSethStory(p);
 assert.ok(html.includes('&lt;img'));assert.ok(!html.includes('<img src=x onerror'));
});
import {parseIni} from '../web/core.mjs';
import {guideFields} from '../web/guided.mjs';
test('real rusEFI-generated INI exposes active idle and trigger signal aliases',()=>{
 const ini=fs.readFileSync(new URL('./fixtures/proteus_f7_generated.ini',import.meta.url),'utf8');
 const fields=guideFields(parseIni(ini));
 assert.equal(fields.idleTarget?.name,'idleTarget');
 // Float field omits explicit transform in this INI; current parser must not guess.
 assert.equal(fields.idleCommand,null);
 assert.equal(fields.idleActive?.name,'isIdling');
 assert.equal(fields.sync?.name,'totalTriggerErrorCounter');
});
test('idle mismatch is not blamed on idle controller when ECU reports not idling',()=>{
 const recent=history(()=>({rpm:700,idleTarget:950,idleActive:0,idleCommand:50}));
 const x=analyzeSeth(fixture({values:recent.at(-1).values,recent}),{stage:'idle',now});
 assert.ok(x.hypotheses.some(h=>h.id==='idle-mode-inactive'));
 assert.ok(!x.hypotheses.some(h=>h.id==='idle-target-mismatch'));
});
