/* BrickOS browser Guided Tuning research engine.
 * Read-only: never emits TS C/B requests or claims to establish safe calibration.
 * Accepts only decoded output-channel definitions from a verified ECU-matched INI.
 */
import {TYPES,getValue} from './core.mjs';

export const GUIDE_STAGES = Object.freeze([
 ['identity','ECU & definition','Verify the ECU identity, selected firmware definition and data freshness.'],
 ['sensors','Sensors & hardware','Confirm available sensors and identify missing physical verification.'],
 ['sync','Trigger & synchronization','Review trigger synchronization and physical ignition-reference evidence.'],
 ['starting','Starting','Collect and compare engine start events across known conditions.'],
 ['warmup','Warmup','Collect temperature-dependent evidence with confirmed sensor readiness.'],
 ['idle','Idle system','Separate airflow, fuel, spark, actuator and feedback observations.'],
 ['fuel','VE & base fuel','Assess stable lambda evidence and fuel-trim masking.'],
 ['trims','STFT / LTFT','Inspect fuel corrections by bank without conflating their scales.'],
 ['transients','Transient behavior','Separate throttle movement and fuel-cut recovery from steady fueling.'],
 ['ignition','Smart Spark','Review available ignition corrections and knock evidence; external torque validation remains necessary.'],
 ['advanced','Advanced features','Discover installed and enabled optional ECU strategies.'],
 ['validation','Final verification','Review the evidence history and unresolved commissioning requirements.']
]);

const ALIASES={
 rpm:['RPMValue','rpm','engineRpm'],tps:['TPSValue','tps','throttlePosition'],
 map:['MAPValue','map','mapValue'],clt:['coolant','CLT','clt','coolantTemp'],
 lambda:['lambdaValue','lambda1','lambda','veAnalyzeAfrLambda1'],
 target:['targetLambda','lambdaTarget','targetMixtureFinalTarget'],
 lambdaGood:['lambdaCurrentlyGood'],
 gego:['Gego'],stft1:['stftCorrection1'],stft2:['stftCorrection2'],
 ltft1:['ltftCorrection1'],ltft2:['ltftCorrection2'],
 battery:['VBatt','batteryVoltage'],spark:['actualLastInjectionIgnitionAngle','ignitionAdvance','timingAdvance','actualIgnitionAdvance'],
 knock:['knockRetard','knockCount','knockRetardMax','m_knockCount','m_maximumRetard'],
 idleTarget:['idleTargetRpm','targetIdleRpm','idleTarget'],idleCommand:['idleAirValvePosition','idlePosition','currentIdlePosition'],
 idleActive:['isIdling'],idleClosedLoop:['isIdleClosedLoop'],isCranking:['isCranking'],fuelCutReason:['fuelCutReason'],
 speed:['vehicleSpeedKph'],fuelPressure:['fuelPressure'],oilPressure:['oilPressure'],
 sync:['triggerErrorCounter','triggerSynchronizationError','totalTriggerErrorCounter']
};
export const DEFAULT_FILTERS=Object.freeze({rpmMin:0,rpmMax:20000,tpsMin:0,tpsMax:100,loadMin:0,loadMax:500,cltMin:-40,cltMax:200,
  maxRpmChange:180,maxTpsChange:3,maxLoadChange:8,settleMs:1200,minVisits:6});
const CONFIG_KEYS=Object.keys(DEFAULT_FILTERS);
function finite(v){return typeof v==='number'&&Number.isFinite(v)}
function clamp(n,min,max){return Math.min(max,Math.max(min,n))}
function inferField(def,names){for(const name of names){const f=(def?.channels||[]).find(x=>x.name===name&&x.kind!=='array'&&x.scale!==null&&x.translate!==null&&TYPES[x.type]);if(f)return f}return null}
export function validateGuideFilters(next){const cfg={...DEFAULT_FILTERS};for(const key of CONFIG_KEYS){
  const v=Number(next?.[key]??cfg[key]);if(!Number.isFinite(v))throw Error(key+' must be a finite number');cfg[key]=v;
 }
 if(cfg.rpmMax<=cfg.rpmMin||cfg.tpsMax<=cfg.tpsMin||cfg.loadMax<=cfg.loadMin||cfg.cltMax<=cfg.cltMin)throw Error('Each maximum must exceed its minimum');
 if(cfg.maxRpmChange<0||cfg.maxTpsChange<0||cfg.maxLoadChange<0||cfg.settleMs<0||cfg.settleMs>60000||!Number.isInteger(cfg.minVisits)||cfg.minVisits<1||cfg.minVisits>1000)throw Error('Invalid stability or evidence requirement');
 return cfg;
}
export function guideFields(def,aliases=ALIASES){return Object.fromEntries(Object.entries(aliases).map(([role,names])=>[role,inferField(def,names)]))}
export function guideRequiredBytes(def){return Math.max(4,...Object.values(guideFields(def)).filter(Boolean).map(f=>f.offset+TYPES[f.type]));}
function displayLabel(name){return String(name||'').replace(/([a-z])([A-Z])/g,'$1 $2').replace(/_/g,' ')}
export function fieldInventory(def){return (def?.cal||[]).map(f=>({id:`${f.page}:${f.offset}:${f.name}`,name:f.name,label:displayLabel(f.name),page:f.page,
 kind:f.kind,type:f.type,units:f.units||'',editable:!!f.editable,scope:'cal',status:'Needs review'}));}

export class GuidedAnalyzer {
 constructor(){this.filters={...DEFAULT_FILTERS};this.roleAliases=ALIASES;this.reset(true)}
 setRoleAliases(aliases){if(!aliases||typeof aliases!=='object')throw Error('Invalid field role registry');
  const filtered={...ALIASES};for(const [role,names] of Object.entries(aliases)){if(!Array.isArray(names)||names.length>30||!names.every(n=>typeof n==='string'&&n.length>0&&n.length<=100))throw Error('Invalid role alias mapping');filtered[role]=names.slice();}
  this.roleAliases=filtered;if(this.def){this.fields=guideFields(this.def,this.roleAliases);this.reset(false);this.record('definition','Semantic role registry updated; previous observations cleared');}
 }
 reset(forgetDefinition=false){if(forgetDefinition){this.def=null;this.signature='';this.fields={};this.inventory=[];}
  this.mode='observe';this.lastSeq=-1;this.last=null;this.values={};this.validMs=0;this.total=0;this.accepted=0;
  this.rejections={};this.tiles=Array.from({length:64},()=>({hits:0,ratioSum:0,ratioSq:0}));this.events=[];this.settledSince=0;this.lastReason='Waiting for telemetry';this.recent=[];this.lastCaptureMs=0;}
 attach(def,signature){if(!def||!signature||def.signature!==signature){this.reset(true);return false;}
  if(this.signature!==signature||this.def!==def){this.reset(true);this.def=def;this.signature=signature;this.fields=guideFields(def,this.roleAliases);this.inventory=fieldInventory(def);this.record('definition','Matched ECU definition: '+signature)}
  return true;
 }
 record(type,message,time=Date.now()){this.events.unshift({type,message,time,signature:this.signature});if(this.events.length>100)this.events.pop()}
 setFilters(filters){const next=validateGuideFilters(filters);this.filters=next;this.reset(false);this.record('filters','Evidence boundaries changed; previous observations cleared');}
 setMode(mode){if(!['observe','learn','paused'].includes(mode))throw Error('Invalid guide mode');if(mode==='learn'&&(!this.def||!this.signature))throw Error('A verified ECU definition is required');
   if(this.mode!==mode){this.mode=mode;this.record('session','Guided browser analysis: '+mode)}
 }
 observe(bytes,ms=Date.now(),sequence=null){if(!this.def||!bytes||this.mode==='paused')return;
   if(sequence!==null&&sequence===this.lastSeq)return;this.lastSeq=sequence;
   const values={};for(const [k,f] of Object.entries(this.fields)){const v=f?getValue(bytes,f,0,this.def.endianness):null;values[k]=finite(v)?v:null;}
   this.values=values;this.validMs=ms;this.total++;
   // Bounded time-window history for read-only, explainable diagnostic reasoning.
   // Keep the actual INI-resolved role values; avoid a second ECU poll for Seth.
   if(!this.lastCaptureMs||ms-this.lastCaptureMs>=100){
    if(this.lastCaptureMs&&ms<=this.lastCaptureMs)this.recent=[];
    this.lastCaptureMs=ms;this.recent.push({ms,values:{...values}});
    while(this.recent.length>160||this.recent.length&&ms-this.recent[0].ms>30000)this.recent.shift();
   }
   const reasons=[];const cfg=this.filters;
   for(const name of ['rpm','tps','map','lambda','target'])if(values[name]===null)reasons.push(name+' unavailable');
   if(values.lambdaGood!==null&&values.lambdaGood===0)reasons.push('lambda sensor not ready');
   if(values.lambda!==null&&(values.lambda<=0||values.lambda>3))reasons.push('invalid measured lambda');
   if(values.target!==null&&(values.target<=0||values.target>3))reasons.push('invalid target lambda');
   if(values.rpm!==null&&(values.rpm<cfg.rpmMin||values.rpm>cfg.rpmMax))reasons.push('RPM outside selected region');
   if(values.tps!==null&&(values.tps<cfg.tpsMin||values.tps>cfg.tpsMax))reasons.push('TPS outside selected region');
   if(values.map!==null&&(values.map<cfg.loadMin||values.map>cfg.loadMax))reasons.push('load outside selected region');
   if(values.clt!==null&&(values.clt<cfg.cltMin||values.clt>cfg.cltMax))reasons.push('coolant outside selected region');
   if(values.gego!==null&&Math.abs(values.gego-100)>1)reasons.push('closed-loop Gego compensation');
   // STFT/LTFT per-bank channels can have firmware-specific display transforms.
   // Preserve their INI-decoded readings, but do not apply a guessed neutral
   // convention or fold them into a VE correction estimate.
   if(values.gego===null)reasons.push('Gego correction unavailable');
   if(this.last){const deltaMs=ms-this.last.ms;if(deltaMs<=0||deltaMs>1500)reasons.push('telemetry gap');
    else for(const [k,max,desc] of [['rpm',cfg.maxRpmChange,'RPM transient'],['tps',cfg.maxTpsChange,'TPS transient'],['map',cfg.maxLoadChange,'load transient']]){
      if(values[k]!==null&&this.last.values[k]!==null&&Math.abs(values[k]-this.last.values[k])*1000/deltaMs>max)reasons.push(desc);
    }
   }
   if(reasons.length){this.settledSince=0;this.lastReason=reasons[0];for(const reason of reasons)this.rejections[reason]=(this.rejections[reason]||0)+1;}
   else {if(!this.settledSince)this.settledSince=ms;if(ms-this.settledSince<cfg.settleMs){this.lastReason='steady-state dwell pending';this.rejections[this.lastReason]=(this.rejections[this.lastReason]||0)+1;}
    else if(this.mode==='learn'){
      const r=clamp(Math.floor((values.rpm-cfg.rpmMin)/(cfg.rpmMax-cfg.rpmMin)*8),0,7);
      const l=clamp(Math.floor((values.map-cfg.loadMin)/(cfg.loadMax-cfg.loadMin)*8),0,7);
      const tile=this.tiles[(7-l)*8+r];const ratio=values.lambda/values.target;tile.hits++;tile.ratioSum+=ratio;tile.ratioSq+=ratio*ratio;this.accepted++;this.lastReason='valid observation';
    }else this.lastReason='eligible; learning not started';
   }
   this.last={ms,values};
 }
 report(now=Date.now()){const fresh=!!this.def&&this.validMs>0&&(now-this.validMs)<1800;
   const v=this.values;const stages=GUIDE_STAGES.map(([id,title,description])=>{
     let state='not assessed',detail='No verified evidence for this subsystem';
     if(id==='identity'){state=this.def&&this.signature?'definition matched':'blocked';detail=this.signature||'Connect ECU and verify generated INI';}
     else if(!this.def){state='blocked';detail='Matched ECU definition required';}
     else if(id==='fuel'){state=this.accepted>=this.filters.minVisits?'observations available':fresh?'needs evidence':'waiting for data';detail=`${this.accepted} qualifying observations; no ECU calibration changes made`;}
     else if(id==='trims'){state=this.fields.gego||this.fields.stft1?'inspect':'unavailable';detail='Feedback corrections must be interpreted separately by bank and firmware';}
     else if(id==='sensors'){state=fresh?'inspect':'waiting for data';detail='Recorded sensor channels do not prove physical wiring or calibration';}
     else if(id==='ignition'){state='external validation';detail='No ignition advance recommendation from indirect measurements alone';}
     else {const keys={sync:['sync'],starting:['rpm','clt'],warmup:['clt'],idle:['rpm','idleTarget','idleCommand'],transients:['tps','map'],advanced:[],validation:[]}[id]||[];
       const found=keys.filter(k=>this.fields[k]);state=keys.length&&!found.length?'unsupported':'review';detail=found.length?'Available channels: '+found.join(', '):'Configuration and external verification needed';}
     return {id,title,description,state,detail};
   });
   const prioritized=[];
   if(!this.def)prioritized.push({id:'ini',title:'Load the verified ECU definition',why:'No INI matched to the live ECU identity.',stage:'identity'});
   else if(!fresh)prioritized.push({id:'freshness',title:'Verify the live ECU telemetry',why:'There is no fresh output-channel snapshot.',stage:'sensors'});
   else if(!this.fields.lambda||!this.fields.target)prioritized.push({id:'lambda',title:'Check measured and target lambda channels',why:'The matching INI does not expose all inputs required for fuel analysis.',stage:'fuel'});
   else if(v.lambdaGood===0)prioritized.push({id:'wideband',title:'Check wideband sensor validity',why:'The ECU currently reports lambda not ready.',stage:'sensors'});
   else if(this.lastReason!=='valid observation')prioritized.push({id:'filters',title:'Review excluded fuel observations',why:this.lastReason,stage:'fuel'});
   else prioritized.push({id:'review',title:'Review the collected evidence',why:'Some stable observations are available; no automatic ECU changes have been made.',stage:'fuel'});
   return {mode:this.mode,signature:this.signature,definitionCount:this.inventory.length,fresh,ageMs:this.validMs?now-this.validMs:null,values:{...v},stages,
     primary:prioritized[0],total:this.total,accepted:this.accepted,rejections:{...this.rejections},lastReason:this.lastReason,
     tiles:this.tiles.map((t,index)=>({index,hits:t.hits,meanRatio:t.hits?t.ratioSum/t.hits:null,confidence:t.hits>=this.filters.minVisits?'observed':'insufficient'})),
     events:this.events.slice(0,35),recent:this.recent.slice(),filters:{...this.filters},requiredBytes:this.def?guideRequiredBytes(this.def):4};
 }
}
