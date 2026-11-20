/* BrickOS Seth diagnostic reasoning v1: deterministic, read-only, INI-gated.
 * No ECU communication, tuning write, start, burn, or actuator command exists.
 * Facts, inferences, alternative explanations, and missing evidence stay separate.
 * Uses the actual channel mapping resolved by GuidedAnalyzer from the matched INI.
 */
export const REASONING_SCHEMA='brickos.guide.reasoning.v1';
const finite=x=>typeof x==='number'&&Number.isFinite(x);
const av=(report,role)=>finite(report?.values?.[role])?report.values[role]:null;
const limit=(x,a,b)=>Math.min(b,Math.max(a,x));
const STAGE_ALIASES=Object.freeze({configuration:'identity',references:'trigger',baseline:'fuel',startup:'starting',steady_fuel:'fuel',steady_ignition:'spark',full_power:'validation',confirm:'validation',connection:'identity',wiring:'sensors'});
export function normalizeReasoningStage(s){
 const k=String(s||'').replace(/[^a-z_]/g,'').toLowerCase();
 return STAGE_ALIASES[k]||k||'identity';
}
const D={
 identity:{title:'ECU identity & definition',roles:['rpm'],external:['Check ECU firmware and matching generated INI definition']},
 sensors:{title:'Sensor integrity',roles:['clt','tps','map','lambda','lambdaGood'],external:['Confirm sensor types, wiring, and scaling independently']},
 trigger:{title:'Crank/cam synchronization',roles:['rpm','sync'],external:['Verify physical trigger installation and ignition reference with qualified equipment']},
 starting:{title:'Starting and after-start',roles:['rpm','clt','battery'],external:['Review physical first-start preconditions with qualified support']},
 warmup:{title:'Warmup behavior',roles:['clt','lambda','lambdaGood'],external:['Collect separately verified events at different initial temperatures']},
 idle:{title:'Idle stability',roles:['rpm','idleTarget','idleCommand','tps','map','lambda'],external:['Confirm idle actuator and physical load behavior']},
 fuel:{title:'Base fueling and VE',roles:['rpm','map','lambda','target','lambdaGood','gego','stft1','ltft1'],external:['Verify injector characteristics and fuel pressure where applicable']},
 trims:{title:'Closed-loop fuel corrections',roles:['lambda','target','gego','stft1','stft2','ltft1','ltft2'],external:['Confirm feedback state and trim scaling for this exact firmware']},
 transients:{title:'Fuel transients',roles:['rpm','tps','map','lambda','target'],external:['Distinguish steady conditions from after-start, enrichment and fuel-cut events']},
 spark:{title:'Ignition evidence',roles:['rpm','spark','knock','clt'],external:['Qualified external torque and timing verification is required before calling timing optimal']},
 advanced:{title:'Optional ECU strategies',roles:['rpm','map','tps'],external:['Verify which optional strategies are enabled in the current calibration']},
 validation:{title:'Final calibration review',roles:['rpm','clt','lambda','spark','knock'],external:['Document independent professionally supervised validation and remaining limitations']}
};
const QUESTIONS={
 definition:{id:'definition',prompt:'Can we confirm the exact ECU firmware signature and generated INI match?',role:null,method:'system',reason:'Wrong definitions make channel addresses and diagnoses unreliable.',weight:100},
 fresh:{id:'fresh',prompt:'Can Seth get fresh ECU telemetry again?',role:null,method:'system',reason:'Stale readings cannot justify new diagnostic conclusions.',weight:98},
 sync:{id:'sync',prompt:'Does this firmware expose a live trigger-error or synchronization channel?',role:'sync',method:'ecu',reason:'It helps distinguish reported sync problems from other starting symptoms.',weight:84},
 idleTarget:{id:'idleTarget',prompt:'Does the INI expose actual and target idle RPM channels?',role:'idleTarget',method:'ecu',reason:'Without target RPM, idle regulation error cannot be quantified.',weight:82},
 idleCommand:{id:'idleCommand',prompt:'Does the INI expose the active idle actuator command and its units?',role:'idleCommand',method:'ecu',reason:'It helps distinguish controller response from external disturbances.',weight:80},
 lambdaGood:{id:'lambdaGood',prompt:'Can the ECU report whether the wideband reading is valid and ready?',role:'lambdaGood',method:'ecu',reason:'A plausible number alone does not establish sensor readiness.',weight:96},
 target:{id:'target',prompt:'Which measured target-lambda channel is active for the current fuel strategy?',role:'target',method:'ecu',reason:'The intended target must be known before comparing the mixture.',weight:90},
 gego:{id:'gego',prompt:'Which closed-loop correction is active, and what does neutral mean on this firmware?',role:'gego',method:'ecu',reason:'Closed-loop feedback can conceal base fuel-model errors.',weight:88},
 bankTrims:{id:'bankTrims',prompt:'Are both bank-specific STFT/LTFT channels and their scaling available?',role:'stft1',method:'ecu',reason:'A single correction value cannot represent independent banks.',weight:79},
 spark:{id:'spark',prompt:'Which channel reports actual applied spark, including corrections?',role:'spark',method:'ecu',reason:'The commanded base table alone does not describe delivered ignition timing.',weight:83},
 knock:{id:'knock',prompt:'Are the ECU knock reports and related sensor health channels available?',role:'knock',method:'ecu',reason:'Absence of a knock value is not absence of knocking.',weight:85},
 physical:{id:'physical',prompt:'Has the physical hardware or calibration reference been independently verified?',role:null,method:'external',reason:'ECU-reported configuration cannot prove hardware installation or measurement accuracy.',weight:65},
 events:{id:'events',prompt:'Do we have repeated, comparable observation windows for this operating state?',role:null,method:'evidence',reason:'One transient snapshot cannot establish a persistent cause.',weight:72},
 correction:{id:'correction',prompt:'Can we determine whether closed-loop or transient correction was active?',role:null,method:'evidence',reason:'Multiple ECU strategies may alter a measured result.',weight:78}
};
const STATEMENT=(id,title,observation,causes,missing,kind='hypothesis',confidence='tentative',severity='review',contrary=[])=>({id,title,observation,causes,missing,kind,confidence,severity,contrary});
function observed(xs,k){return xs.filter(x=>finite(x?.values?.[k]));}
function span(xs){return xs.length?xs[xs.length-1].ms-xs[0].ms:0;}
function coverage(xs,role){const v=observed(xs,role);return {samples:v.length,seconds:Math.round(span(v)/100)/10};}
function windowFor(report,now){return (report?.recent||[]).filter(x=>finite(x.ms)&&x.ms<=now&&now-x.ms<=30000&&x.values&&typeof x.values==='object').sort((a,b)=>a.ms-b.ms).slice(-160);}
function repeatable(xs,role,min=7,minMs=1800){const a=observed(xs,role);return a.length>=min&&span(a)>=minMs;}
function median(nums){if(!nums.length)return null;const sorted=nums.slice().sort((a,b)=>a-b);return sorted.length%2?sorted[(sorted.length-1)/2]:(sorted[sorted.length/2-1]+sorted[sorted.length/2])/2;}
function rmsVariation(nums){if(!nums.length)return null;const mean=nums.reduce((a,b)=>a+b,0)/nums.length;return Math.sqrt(nums.reduce((a,b)=>a+(b-mean)**2,0)/nums.length);}
function monotonicIncrease(xs,role){const a=observed(xs,role);if(a.length<2)return false;let deltas=0;for(let i=1;i<a.length;i++)if(a[i].values[role]>a[i-1].values[role])deltas++;return deltas>=2&&a[a.length-1].values[role]>a[0].values[role];}
export function analyzeSeth(engine,{stage='identity',now=Date.now()}={}){
 const key=normalizeReasoningStage(stage);const spec=D[key]||D.identity;const report=engine?.report?.(now)||{};
 const rawFields=engine?.fields||{};const fields=Object.fromEntries(Object.entries(rawFields).map(([k,v])=>[k,!!v]));
 const matched=!!engine?.def&&!!engine?.signature&&!!report.signature&&engine.signature===report.signature;
 const fresh=matched&&report.fresh===true;
 const recent=matched?windowFor(report,now):[];
 const facts=[];const hypotheses=[];const contradictions=[];const missing=[];const selectedQuestions=[];
 const addQuestion=(q)=>{if(!selectedQuestions.some(x=>x.id===q.id))selectedQuestions.push({...q});};
 const missingRole=(role)=>{if(!fields[role]){missing.push({role,reason:'Not resolved by matched INI',source:'ini'});return true;}
  if(!fresh||av(report,role)===null){missing.push({role,reason:'No fresh decoded ECU reading',source:'telemetry'});return true;}return false;};
 const addFact=(code,text,roles=[])=>facts.push({code,text,roles,source:'ecu',verified_as:'reported',fresh});
 const quality={definition:matched?'matched':'missing',telemetry:fresh?'fresh':'stale',historySamples:recent.length,historySpanMs:span(recent),repeatableWindows:false,measurementTrust:'not established'};
 if(!matched){
  addQuestion(QUESTIONS.definition);
  hypotheses.push(STATEMENT('identity-unverified','ECU definition not verified','INI and ECU identity are not jointly confirmed.',
   ['No matching definition','ECU disconnected or changed'],['Matched signature and calibration-layout identity'],'blocker','none','blocking'));
 }else if(!fresh){
  addQuestion(QUESTIONS.fresh);
  hypotheses.push(STATEMENT('telemetry-stale','Live data unavailable','The most recent snapshot is stale or missing.',
    ['Communication interrupted','Sampling not yet established'],['Fresh ECU output sample'],'blocker','none','blocking'));
 }else{
  addFact('definition-match',`ECU signature matches the decoded definition: ${engine.signature}.`);
  addFact('telemetry-fresh','ECU telemetry is current, but physical sensor accuracy is not independently verified.');
  for(const role of spec.roles)missingRole(role);
  if(finite(av(report,'rpm')))addFact('rpm',`ECU reports ${Math.round(av(report,'rpm'))} RPM.`,['rpm']);
  if(fields.lambdaGood&&av(report,'lambdaGood')===0){
   hypotheses.push(STATEMENT('lambda-not-ready','Wideband not reported ready','The ECU reports the lambda validity flag as false.',
    ['Sensor is warming up','Sensor/controller fault','Wiring or configuration discrepancy'],['Check wideband readiness and associated ECU faults'],'observation','observed','blocking'));
   addQuestion(QUESTIONS.lambdaGood);
  }
  if(key==='idle'){
   const idleStateKnown=fields.idleActive&&av(report,'idleActive')!==null;
   const idleActive=!idleStateKnown||av(report,'idleActive')!==0;
   if(idleStateKnown&&av(report,'idleActive')===0){
     addFact('idle-inactive','ECU reports that its idling-state flag is not active.', ['idleActive']);
     hypotheses.push(STATEMENT('idle-mode-inactive','Idle controller not reported active','The ECU currently reports an inactive idling state; evaluating closed-loop idle error would be misleading.',
       ['Operating condition outside idle mode','ECU idle-entry conditions not met'],['Verify current operating state and the configured idle-entry conditions'],
       'observation','observed','review'));
   }
   const a=idleActive?recent.filter(x=>finite(x.values.rpm)&&finite(x.values.idleTarget)&&(!fields.idleActive||x.values.idleActive===1)):[];
   if(repeatable(a,'rpm')&&span(a)>=2200){
    const errors=a.map(x=>x.values.rpm-x.values.idleTarget);
    const medianError=median(errors);const rpmStd=rmsVariation(a.map(x=>x.values.rpm));
    quality.repeatableWindows=true;
    if(Math.abs(medianError)>100){
      addFact('idle-rpm-delta',`Across ${a.length} sampled points, median actual-minus-target idle error was ${Math.round(medianError)} RPM.`,['rpm','idleTarget']);
      hypotheses.push(STATEMENT('idle-target-mismatch','Persistent RPM target deviation','Observed RPM was persistently different from the reported target under the sampled conditions.',
        ['Insufficient actuator authority','Unmodeled engine load','Combustion or fueling variation','Control strategy not actually active'],
        ['Confirmed ECU idle-active flag','Actuator output and physical response','Comparable no-load observations'],
        'hypothesis','window-pattern','review'));
      if(!fields.idleCommand)addQuestion(QUESTIONS.idleCommand);
      addQuestion(QUESTIONS.physical);
    }
    if(rpmStd>65){
      addFact('rpm-variation',`Observed RPM variation is about ${Math.round(rpmStd)} RPM standard deviation in this window.`,['rpm']);
      hypotheses.push(STATEMENT('idle-variation','Idle RPM variation detected','RPM varies, but the cause is not isolated.',
        ['Idle control response','Changing engine loads','Fuel or ignition variation','Sensor noise'],
        ['Timestamp-aligned actuator command, ignition, load and lambda data'],
        'hypothesis','window-pattern','review'));
    }
   }else addQuestion(QUESTIONS.events);
   if(!fields.idleTarget)addQuestion(QUESTIONS.idleTarget);
   if(!fields.idleCommand)addQuestion(QUESTIONS.idleCommand);
   if(repeatable(recent,'rpm')&&recent.length>8){
    const commands=recent.filter(x=>finite(x.values.idleCommand));const u=rawFields.idleCommand?.units||'';
    if(commands.length>=8&&/%/.test(u)&&commands.filter(x=>x.values.idleCommand>=95).length>=Math.ceil(commands.length*.8)){
       hypotheses.push(STATEMENT('idle-command-near-ceiling','Idle command near displayed upper percentage','The reported command was near the displayed percentage ceiling for most samples.',
        ['Configured command limit reached','Actuator/hardware constraint','Higher than expected external load'],
        ['Verify that this percentage is the active actuator command and check physical response'],
        'hypothesis','tentative','review'));
    }
   }
  }
  if(key==='trigger'||key==='starting'){
    if(monotonicIncrease(recent,'sync')){
      hypotheses.push(STATEMENT('sync-counter-increase','Trigger-error counter increased','The INI-decoded trigger-error counter increased during recent observations; that does not identify a mechanical cause.',
        ['Sensor/wiring issue','Trigger configuration mismatch','Signal integrity issue'],
        ['Relevant ECU fault flags and independent physical trigger verification'],
        'observation','window-pattern','blocking'));
      addQuestion(QUESTIONS.physical);
    }
    if(!fields.sync)addQuestion(QUESTIONS.sync);
    if(key==='starting'&&!repeatable(recent,'rpm'))addQuestion(QUESTIONS.events);
  }
  if(key==='fuel'||key==='trims'||key==='transients'){
    if(!fields.target)addQuestion(QUESTIONS.target);
    if(!fields.lambdaGood)addQuestion(QUESTIONS.lambdaGood);
    if(!fields.gego)addQuestion(QUESTIONS.gego);
    const a=recent.filter(x=>finite(x.values.lambda)&&finite(x.values.target)&&finite(x.values.gego)&&x.values.target>0);
    if(a.length>=7&&span(a)>=1800){
      const onTarget=a.filter(x=>Math.abs(x.values.lambda-x.values.target)<=.035);
      const correcting=a.filter(x=>Math.abs(x.values.gego-100)>=2.5);
      if(onTarget.length>=Math.ceil(a.length*.65)&&correcting.length>=Math.ceil(a.length*.65)){
        contradictions.push({id:'lambda-trim-masking',evidence:'Lambda stayed near its reported target while Gego deviated from its 100-neutral scale.',meaning:'Closed-loop compensation can mask an inaccurate base fueling model.',resolved:false});
        hypotheses.push(STATEMENT('feedback-masked-base-fuel','Closed-loop correction may mask base fueling','Lambda near target coincides with a non-neutral Gego correction.',
          ['Base fuel model deviation','Transient compensation','Sensor or injector characterization differences'],
          ['Verify feedback applicability, bank assignment, STFT/LTFT semantics and steady-state validity'],
          'hypothesis','window-pattern','review'));
        addQuestion(QUESTIONS.correction);
      }
    }else addQuestion(QUESTIONS.events);
    if(key==='trims'&&(!fields.stft1||!fields.ltft1))addQuestion(QUESTIONS.bankTrims);
    if(key==='transients')addQuestion(QUESTIONS.correction);
  }
  if(key==='spark'){
    if(!fields.spark)addQuestion(QUESTIONS.spark);
    if(!fields.knock)addQuestion(QUESTIONS.knock);
    if(repeatable(recent,'knock')&&monotonicIncrease(recent,'knock')){
      hypotheses.push(STATEMENT('knock-signal-trend','Reported knock-related channel increased','A knock-related INI channel rose during the observation window. The channel type and units require confirmation.',
        ['Actual knock intervention','Cumulative count variation','Sensor/algorithm behavior'],
        ['Clarify channel meaning, ECU fault state and independent specialist assessment'],
        'observation','tentative','blocking'));
    }
    hypotheses.push(STATEMENT('mbt-unmeasured','Best-torque ignition timing is not measured','MAP, airflow, accelerometer data, and absence of reported knock cannot establish MBT.',
      ['External torque evidence required'],['Professionally obtained instrumented torque-quality evidence'],
      'limitation','none','review'));
    addQuestion(QUESTIONS.physical);
  }
  if(key==='sensors'&&repeatable(recent,'clt')){
    const a=observed(recent,'clt');let suspicious=0;
    for(let i=1;i<a.length;i++){
      const dt=(a[i].ms-a[i-1].ms)/1000;
      if(dt>0&&dt<1.5&&Math.abs(a[i].values.clt-a[i-1].values.clt)/dt>20)suspicious++;
    }
    if(suspicious>=2){
      hypotheses.push(STATEMENT('coolant-jumps','Rapid reported coolant temperature changes','More than one abrupt coolant measurement change was observed.',
        ['Electrical/sensor noise','Intermittent connection','Incorrect temperature conversion'],
        ['Sensor type and calibration, reference and external physical verification'],
        'hypothesis','window-pattern','review'));
      addQuestion(QUESTIONS.physical);
    }
  }
  if(key==='warmup'||key==='advanced'||key==='identity'||key==='validation')addQuestion(QUESTIONS.physical);
 }
 if(missing.length&&matched){
  const filtered=missing.filter(m=>m.reason.includes('Not resolved'));
  if(filtered.length){
   hypotheses.push(STATEMENT('missing-channel-data','Some diagnostic inputs are unavailable',
     `The matched INI did not resolve ${filtered.map(x=>x.role).join(', ')}.`,
     ['Firmware does not expose the channel','The enabled strategy uses a different channel','Definition is incomplete'],
     ['Inspect output channels and active ECU strategy'], 'limitation','none','review'));
  }
 }
 if(!hypotheses.length){
  hypotheses.push(STATEMENT('no-isolated-diagnosis','No cause isolated from available evidence',
   'The available measurements do not establish a specific fault or calibration correction.',
   ['The system may be operating normally','Relevant channels may be missing','Conditions may not yet be comparable'],
   ['Additional comparable observations and physical verification'],'limitation','none','info'));
 }
 // Information gain is an explicit heuristic, not a statistical probability.
 const stagePriority={idle:['idleTarget','idleCommand','events'],trigger:['sync','physical'],starting:['sync','events','physical'],spark:['spark','knock','physical'],fuel:['lambdaGood','target','gego','events','correction'],trims:['gego','bankTrims','correction'],transients:['events','correction'],sensors:['lambdaGood','physical']};
 const priority=stagePriority[key]||[];
 if(fresh)for(const q of Object.values(QUESTIONS)){
  if(q.role&&!fields[q.role]&&spec.roles.includes(q.role))addQuestion(q);
 }
 selectedQuestions.sort((a,b)=>(b.weight+(priority.includes(b.id)?25:0))-(a.weight+(priority.includes(a.id)?25:0))||a.id.localeCompare(b.id));
 const questions=selectedQuestions.slice(0,4).map(q=>({...q,answersFromEcu:q.role?!!fields[q.role]:false}));
 const blocking=hypotheses.some(x=>x.severity==='blocking');
 const confidence=(!fresh?'unavailable':quality.repeatableWindows?'observed within one window':'limited evidence');
 quality.measurementTrust='ECU-reported only; sensor/physical verification separate';
 return {schema:REASONING_SCHEMA,stage:key,title:spec.title,definition_key:engine?.signature||null,
  freshness:fresh?'live':matched?'stale':'definition unavailable',confidence,blocking,quality,facts,
  hypotheses,contradictions,missing_inputs:missing,questions,read_only:true,unsupported_inferences:['safe ignition advance','engine mechanical condition','physical wiring correctness','MBT from MAP or motion alone']};
}

export function sethReasoningSummary(d){
 if(!d||d.schema!==REASONING_SCHEMA)throw Error('Invalid reasoning report');
 const h=d.hypotheses[0];
 if(d.freshness!=='live')return `I can't trust the live evidence yet. ${h?.observation||'We need the ECU identity and fresh data.'} I'd rather admit I'm missing information than invent a cause from inside this box.`;
 const pre=d.contradictions.length?"I've got a contradiction worth investigating. ":d.blocking?'Something needs verification before we go further. ':"Here's what the available evidence says. ";
 return `${pre}${h.title}: ${h.observation} ${h.missing.length?'What I still need: '+h.missing[0]+'.':''}`;
}
