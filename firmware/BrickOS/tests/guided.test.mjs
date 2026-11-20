import test from 'node:test';
import assert from 'node:assert/strict';
import {parseIni} from '../web/core.mjs';
import {GuidedAnalyzer,GUIDE_STAGES,guideRequiredBytes,fieldInventory,validateGuideFilters} from '../web/guided.mjs';
import {renderGuide} from '../web/guided-ui.mjs';

const mini=`[TunerStudio]\nsignature = "rusEFI mock.2026.11.19.proto.100"\n[Constants]\npageSize = 1024\npageIdentifier = 0x0000\npage = 1\nveTable = array, U08, 3, [4x4], "", 1, 0\n[OutputChannels]\nochBlockSize = 48\nRPMValue = scalar, U16, 0, "RPM", 1, 0\nTPSValue = scalar, U16, 2, "%", 0.01, 0\nMAPValue = scalar, U16, 4, "kPa", 0.1, 0\ncoolant = scalar, S16, 6, "°C", 0.1, 0\nlambdaValue = scalar, U16, 8, "", 0.0001, 0\ntargetLambda = scalar, U16, 10, "", 0.0001, 0\nGego = scalar, U16, 12, "%", 0.01, 0\nlambdaCurrentlyGood = bits, U08, 14, [0:0]\n`;
const def=parseIni(mini);
function sample({rpm=1100,tps=1,map=45,clt=80,lambda=1,target=1,gego=100,good=1}={}){
 const a=new Uint8Array(48),v=new DataView(a.buffer);v.setUint16(0,rpm,true);v.setUint16(2,Math.round(tps*100),true);v.setUint16(4,Math.round(map*10),true);v.setInt16(6,Math.round(clt*10),true);v.setUint16(8,Math.round(lambda*10000),true);v.setUint16(10,Math.round(target*10000),true);v.setUint16(12,Math.round(gego*100),true);v.setUint8(14,good);return a;
}
test('matched INI inventory and available channel length',()=>{
 const g=new GuidedAnalyzer();assert.equal(g.attach(def,def.signature),true);assert.equal(g.inventory.length,1);
 assert.equal(fieldInventory(def)[0].id,'1:3:veTable');assert.equal(guideRequiredBytes(def),15);
 assert.equal(g.attach(def,'another ECU'),false);assert.equal(g.inventory.length,0);
});
test('stage coverage includes complete lifecycle and no automatic writes',()=>{
 assert.equal(GUIDE_STAGES.length,12);const g=new GuidedAnalyzer();g.attach(def,def.signature);
 const r=g.report();assert.equal(r.stages.length,12);assert.ok(!('write' in g));
});
test('sample eligibility requires stable independent history and enabled learning',()=>{
 const g=new GuidedAnalyzer();g.attach(def,def.signature);g.setMode('learn');
 let now=100000;for(let i=0;i<30;i++){g.observe(sample(),now,i+1);now+=100}
 const r=g.report(now);assert.ok(r.accepted>1,JSON.stringify(r.rejections));assert.ok(r.tiles.some(x=>x.hits>0));
 assert.equal(r.values.lambda,1);assert.equal(r.values.gego,100);
});
test('discards lambda invalid and active trim rather than treating on-target lambda as correct VE',()=>{
 const g=new GuidedAnalyzer();g.attach(def,def.signature);g.setMode('learn');let now=10000;
 for(let i=0;i<20;i++){g.observe(sample({lambda:1,gego:108,good:1}),now++,i)}
 assert.equal(g.accepted,0);assert.ok(g.rejections['closed-loop Gego compensation']>0);
 const h=new GuidedAnalyzer();h.attach(def,def.signature);h.setMode('learn');h.observe(sample({good:0}),now,10);
 assert.equal(h.accepted,0);assert.ok(h.rejections['lambda sensor not ready']>0);
});
test('discards transients and missing lambda and does not silently infer missing trim',()=>{
 const g=new GuidedAnalyzer();g.attach(def,def.signature);g.setMode('learn');g.observe(sample(),1000,1);
 g.observe(sample({rpm:2000,tps:30,map:110}),1100,2);
 assert.ok(g.rejections['RPM transient']>0);assert.ok(g.rejections['TPS transient']>0);
 const withoutGego={...def,channels:def.channels.filter(x=>x.name!=='Gego')};const h=new GuidedAnalyzer();h.attach(withoutGego,def.signature);h.setMode('learn');h.observe(sample(),1000,1);
 assert.equal(h.accepted,0);assert.ok(h.rejections['Gego correction unavailable']>0);
});
test('filters validate ranges and reset collected evidence',()=>{
 const g=new GuidedAnalyzer();g.attach(def,def.signature);g.setMode('learn');g.observe(sample(),1000,1);
 assert.throws(()=>validateGuideFilters({rpmMin:1000,rpmMax:500}),/maximum/);
 assert.throws(()=>validateGuideFilters({settleMs:-1}),/Invalid/);
 g.setFilters({rpmMin:500,rpmMax:5000});assert.equal(g.total,0);assert.equal(g.mode,'observe');
});
test('phone render is separate, accessible and does not provide edit/burn actions',()=>{
 const g=new GuidedAnalyzer();g.attach(def,def.signature);
 for(const tab of ['next','systems','evidence','atlas','history']){
  const html=renderGuide(g,{tab,status:{ecu_session_connected:true}});
  assert.ok(html.includes('Guided Calibration'));assert.ok(html.includes('read-only')||html.includes('Read-only')||html.includes('observe'));
  assert.ok(!html.includes('data-action="burn"'));
 }
 assert.match(renderGuide(g,{tab:'atlas'}),/ve Table/);
});
