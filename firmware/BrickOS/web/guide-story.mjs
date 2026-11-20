import {analyzeSeth,sethReasoningSummary,normalizeReasoningStage} from './guide-reasoning.mjs';

/* BrickOS portable Seth conversation interpreter — no DOM, no ECU writes.
 * Loaded from the ESP-served versioned JSON dialogue catalogue. Any future DS
 * client can render returned packets without embedding English dialogue or
 * rusEFI INI offsets in its executable.
 */
export const STORY_SCHEMA='brickos.guide.dialog.v1';
export const STORY_STAGE_HINTS=Object.freeze({
 identity:/configuration|firmware|engine|trigger|sensor/i,
 sensors:/sensor|calibrat|inputs?|temperature|map|tps|lambda|wideband|pressure/i,
 trigger:/trigger|crank|cam|synchron|tooth|ignition.*reference/i,
 starting:/crank|start|prime|after.?start|ase|wue/i,
 warmup:/warmup|enrich|temperature|cold.?start/i,
 idle:/idle|iac|stepper|closed.?loop/i,
 fuel:/\bve\b|fuel|inject|lambda|afr|stoich/i,
 trims:/trim|closed.?loop|ego|fuel.*correction/i,
 transients:/accel|transient|decel|fuel.?cut|tps.?dot/i,
 spark:/ignit|spark|knock|timing/i,
 advanced:/boost|vvt|vva|flex|ethanol|dbw|throttle|can.?bus/i,
 validation:/diagnostic|view|status|warning|fault/i
});
const has=(o,k)=>Object.prototype.hasOwnProperty.call(o,k);
export function validateStoryCatalog(c){
 if(!c||c.schema!==STORY_SCHEMA||typeof c.version!=='string'||!c.nodes||!has(c.nodes,c.entry))throw Error('Invalid or unsupported guide catalogue');
 if(c.chapter_checks){
  if(!c.tour_steps||typeof c.chapter_checks!=='object'||Array.isArray(c.chapter_checks))throw Error('Chapter checks require a tour');
  for(const step of c.tour_steps){
   const qs=c.chapter_checks[step.id];if(!Array.isArray(qs)||qs.length===0||qs.length>25)throw Error('Missing questions for '+step.id);
   const seen=new Set();for(const q of qs){
    if(!/^[a-z0-9_]+$/.test(q.id||'')||seen.has(q.id)||typeof q.prompt!=='string'||!q.prompt||typeof q.why!=='string'||!q.why||!Array.isArray(q.roles)||!q.roles.every(x=>typeof x==='string')||typeof q.manual_stage!=='string'||typeof q.requires_external!=='boolean')throw Error('Bad question '+step.id);
    seen.add(q.id);
   }
  }
 }
 if(c.tour_steps){
  if(!Array.isArray(c.tour_steps)||c.tour_steps.length!==10)throw Error('Guide tour requires ten steps');
  const seen=new Set();
  for(const [i,step] of c.tour_steps.entries()){
   if(step.step!==i+1||typeof step.node!=='string'||!has(c.nodes,step.node)||seen.has(step.node))throw Error('Invalid ten-step tour order');
   seen.add(step.node);
  }
 }
 for(const [id,n] of Object.entries(c.nodes)){
  if(n.id!==id||n.speaker!=='seth'||typeof n.text!=='string'||!Array.isArray(n.choices))throw Error('Bad dialogue node '+id);
  for(const ch of n.choices){if(!ch.label||typeof ch.go!=='string')throw Error('Bad choice '+id);
   if(!has(c.nodes,ch.go)&&ch.go!=='back'&&ch.go!=='resume.last'&&!ch.go.startsWith('workspace.')&&!ch.go.startsWith('action.manual.')&&!/^field\.\d+$/.test(ch.go)&&!ch.go.startsWith('action.field.')&&!/^check\.[a-z_]+\.\d+$/.test(ch.go))throw Error('Broken dialogue link '+id+' -> '+ch.go);
  }
 }
 return c;
}
export function findManualTarget(stage,menuTree){
 if(!stage||!menuTree?.itemById||!STORY_STAGE_HINTS[stage])return null;
 const pattern=STORY_STAGE_HINTS[stage];
 for(const item of menuTree.itemById.values()){
  if(pattern.test(String(item.label||'')+' '+String(item.group||'')))return {id:item.id,label:item.label||item.id};
 }
 return null;
}
export class GuideStory {
 constructor(catalog=null){this.catalog=null;this.nodeId='welcome';this.stack=[];this.definitionKey='';this.lastVisited='';this.history=[];this.checkAnswers={};if(catalog)this.load(catalog)}
 load(c){this.catalog=validateStoryCatalog(c);this.nodeId=c.entry;this.stack=[];this.lastVisited='';this.history=[];this.checkAnswers={};this.definitionKey='';}
 isCheckNode(id){const m=/^check\.([a-z_]+)\.(\d+)$/.exec(id||'');return !!(m&&this.catalog?.chapter_checks?.[m[1]]?.[Number(m[2])]);}
 save(){if(!this.definitionKey||typeof localStorage==='undefined')return;try{localStorage.setItem('brickos.guide.state.'+this.definitionKey,JSON.stringify({version:this.catalog.version,lastVisited:this.lastVisited,answers:this.checkAnswers}));}catch{}}
 bindDefinition(key){const k=key||'';if(k!==this.definitionKey){this.definitionKey=k;this.nodeId=this.catalog?.entry||'welcome';this.stack=[];this.lastVisited='';this.history=[];this.checkAnswers={};
   if(k&&typeof localStorage!=='undefined')try{const raw=JSON.parse(localStorage.getItem('brickos.guide.state.'+k)||'null');if(raw?.version===this.catalog?.version){if(has(this.catalog.nodes,raw.lastVisited)||this.isCheckNode(raw.lastVisited)||/^diagnosis\.(?:question\.)?[a-z_]+$/.test(raw.lastVisited))this.lastVisited=raw.lastVisited;for(const [id,v] of Object.entries(raw.answers||{})){const m=/^([a-z_]+)\.([a-z0-9_]+)$/.exec(id);const q=this.catalog.chapter_checks?.[m?.[1]]?.find(x=>x.id===m?.[2]);if(q&&['reported','unknown'].includes(v))this.checkAnswers[id]=v;}}}catch{}
  }return k;}
 canResume(){return !!this.lastVisited&&this.lastVisited!==this.catalog?.entry&&!!this.definitionKey;}
 go(id){if(!this.catalog)throw Error('Guide catalogue unavailable');
  if(id==='back'){this.nodeId=this.stack.pop()||this.catalog.entry;return {type:'node',node:this.nodeId};}
  if(id==='resume.last'){this.nodeId=this.canResume()?this.lastVisited:'tour.start';return {type:'node',node:this.nodeId};}
  if(id.startsWith('answer.')){
   const m=/^answer\.([a-z_]+)\.(\d+)\.(reported|unknown)$/.exec(id);
   if(!m||!this.isCheckNode('check.'+m[1]+'.'+m[2]))throw Error('Unknown question response');
   const q=this.catalog.chapter_checks[m[1]][Number(m[2])];this.checkAnswers[m[1]+'.'+q.id]=m[3];
   this.save();return {type:'node',node:this.nodeId,answer:m[3]};
  }
  if(id.startsWith('workspace.'))return {type:'workspace',tab:id.slice(10)};
  if(id.startsWith('action.field.'))return {type:'manual_field',index:Number(id.slice(13))};
  if(id.startsWith('action.manual.'))return {type:'manual',stage:id.slice(14)};
  if(!has(this.catalog.nodes,id)&&!/^field\.\d+$/.test(id)&&!this.isCheckNode(id)&&!/^why\.[a-z_]+\.\d+$/.test(id)&&!/^diagnosis\.(?:question\.)?[a-z_]+$/.test(id))throw Error('Unknown dialogue step');
  if(id!==this.nodeId&&this.stack.length<90)this.stack.push(this.nodeId);
  this.nodeId=id;
  if(id!=='welcome'&&id!=='resume'&&id!=='targeted'&&id!=='tour.start'&&id!=='tour.map')this.lastVisited=id;
  this.history.push(id);if(this.history.length>200)this.history.shift();
  if(this.definitionKey&&this.canResume())this.save();
  return {type:'node',node:id};
 }
 packet({engine=null,menuTree=null,menuLinks={},ecuKey=''}={}){
  if(!this.catalog)return {node_id:'loading',text:'Loading Seth’s dialogue from the ESP…',choices:[],status:'loading'};
  if(ecuKey!==this.definitionKey)this.bindDefinition(ecuKey);
  let n=this.catalog.nodes[this.nodeId]||this.catalog.nodes[this.catalog.entry];
  const all=engine?.inventory||[];
  if(/^field\.\d+$/.test(this.nodeId)){
   const i=Number(this.nodeId.slice(6));const f=all[i];
   if(f){const explicit=menuLinks[f.name];const fieldType=[f.kind,f.type,f.units].filter(Boolean).join(' / ');
    n={id:this.nodeId,speaker:'seth',tone:'thoughtful',tag:'INI '+(i+1)+' / '+all.length,
     text:`This is ${f.label||f.name}, stored on page ${f.page}. The INI identifies it as ${fieldType||'a calibration field'}. I can show its original editor when the INI menu resolves it; the field definition alone does not prove what the setting should be.`,
     choices:[...(i>0?[{label:'Previous INI field',go:'field.'+(i-1)}]:[]),
      ...(i+1<all.length?[{label:'Next INI field',go:'field.'+(i+1)}]:[]),
      {label:'Open original field editor',go:'action.field.'+i},
      {label:'Search INI Atlas',go:'workspace.atlas'},
      {label:'Back to guide home',go:'welcome'}],_field:f,_menu:explicit||null};}
   else n={id:this.nodeId,speaker:'seth',text:'No further decoded calibration fields are available. This is not a claim that all INI conditions or control strategies are fully understood.',choices:[{label:'Open full INI Atlas',go:'workspace.atlas'},{label:'Guide home',go:'welcome'}]};
  }
  // Chapter checks are catalogued with INI role names; clients render data packets.
  const match=/^(check|why)\.([a-z_]+)\.(\d+)$/.exec(this.nodeId);
  let activeCheck=null,activeStep=null,answer=null;
  if(match){
   const [_,kind,stage,indexText]=match;const index=Number(indexText);
   activeStep=this.catalog.tour_steps?.find(x=>x.id===stage);
   activeCheck=this.catalog.chapter_checks?.[stage]?.[index];
   if(!activeStep||!activeCheck)throw Error('Unknown chapter question');
   answer=this.checkAnswers[stage+'.'+activeCheck.id]||'unreviewed';
   const next=this.catalog.chapter_checks[stage][index+1];
   const finish=next?'check.'+stage+'.'+(index+1):activeStep.node;
   const why=kind==='why';
   n={id:this.nodeId,speaker:'seth',tone:why?'thoughtful':'normal',
    tag:`${String(activeStep.step).padStart(2,'0')} / ${activeStep.title} · QUESTION ${index+1} / ${this.catalog.chapter_checks[stage].length}`,
    stage:activeStep.expert,manual:activeCheck.manual_stage,
    text:why?`${activeCheck.why} This is a review question, not an automatic test or permission to change calibration values.`:
      `${activeCheck.prompt} ${activeCheck.why} ${activeCheck.roles.length?'ECU evidence: {{available:'+activeCheck.roles.join('|')+'}}.':''}`,
    choices:[
     {label:activeCheck.requires_external?'I checked it independently':'I reviewed the available evidence',go:'answer.'+stage+'.'+index+'.reported'},
     {label:'I don’t know / not checked',go:'answer.'+stage+'.'+index+'.unknown'},
     {label:why?'Return to question':'Why does this matter?',go:why?'check.'+stage+'.'+index:'why.'+stage+'.'+index},
     {label:'Show measured evidence',go:'workspace.evidence'},
     {label:'Open related original setting',go:'action.manual.'+activeCheck.manual_stage},
     {label:next?'Next question':'Back to this chapter',go:finish}
    ]};
  }
  // Diagnostic insights are derived from INI-resolved telemetry and kept separate
  // from catalogue story text. They never claim mechanical verification.
  if(/^diagnosis\.(?:question\.)?[a-z_]+$/.test(this.nodeId)){
   const asking=this.nodeId.startsWith('diagnosis.question.');
   const raw=this.nodeId.replace(/^diagnosis\.(?:question\.)?/, '');
   const key=normalizeReasoningStage(raw);
   const reasoning=analyzeSeth(engine,{stage:key});
   const question=reasoning.questions[0];
   n={id:this.nodeId,speaker:'seth',tone:'thoughtful',tag:'SETH / REASONING',stage:key,
    text:asking?(question?`${question.prompt} ${question.reason} If the ECU cannot answer it, we'll keep it unresolved.`:'I have no additional supported question right now. The missing evidence is listed below.')
               :sethReasoningSummary(reasoning),
    choices:[
      {label:'Show my reasoning',go:'diagnosis.'+key},
      {label:'What would distinguish the causes?',go:'diagnosis.question.'+key},
      {label:'Inspect live evidence',go:'workspace.evidence'},
      {label:'Open related original ECU setting',go:'action.manual.'+key},
      {label:'Return to previous topic',go:'back'},
      {label:'Guide home',go:'welcome'}
    ],_reasoning:reasoning};
  }
  // Relevant dialogue pages expose an optional live reasoning branch.
  // Full Tour and Quick Help both route into the same inference engine.
  const inferred=/^(?:topics|issue)\.(start|idle|fuel|sensors|trigger|spark|warmup|trims|transients)(?:\.|$)/.exec(n.id||'');
  const stageHint=n.stage||activeStep?.expert||(inferred?.[1]==='start'?'starting':inferred?.[1]);
  if(stageHint&&!n._reasoning){
   const key=normalizeReasoningStage(stageHint);
   if(/^(identity|sensors|trigger|starting|warmup|idle|fuel|trims|transients|spark|advanced|validation)$/.test(key)&&!n.choices.some(c=>c.go==='diagnosis.'+key))
    n={...n,choices:[...n.choices,{label:'Seth, investigate this using ECU evidence',go:'diagnosis.'+key}]};
  }
  const r=engine?.report?.()||{};const fields=engine?.fields||{};const values=r.values||{};
  const available=roles=>roles.map(role=>{
   const field=fields[role];if(!field)return role+': not exposed by matched INI';
   const v=values[role];if(!r.fresh||typeof v!=='number'||!Number.isFinite(v))return role+': defined, live value unknown';
   const u=field.units||'';return role+': '+(Math.round(v*100)/100)+(u?' '+u:'');
  }).join('; ');
  const text=n.text.replace(/\{\{available:([^{}]+)\}\}/g,(_,keys)=>available(keys.split('|').filter(Boolean)));
  const target=n._field?(n._menu?{id:n._menu,label:n._field.label}:null):findManualTarget(n.manual||n.stage,menuTree);
  const chapter=activeStep||this.catalog.tour_steps?.find(step=>step.node===n.id)||null;
  const progress=activeStep?{index:Number(match[3])+1,total:this.catalog.chapter_checks[activeStep.id].length,status:answer,status_meaning:answer==='reported'?'User-reported review only; not independently verified by BrickOS':answer==='unknown'?'Needs review; no correctness claim':'Not yet reviewed',requires_external:activeCheck.requires_external}:null;
  const questions=chapter?this.catalog.chapter_checks?.[chapter.id]||[]:[];
  const counts={total:questions.length,reported:0,unknown:0,unreviewed:0};
  for(const q of questions){const st=this.checkAnswers[chapter.id+'.'+q.id];if(st==='reported')counts.reported++;else if(st==='unknown')counts.unknown++;else counts.unreviewed++;}
  return {schema:this.catalog.schema,catalog_version:this.catalog.version,node_id:n.id,speaker:n.speaker,tone:n.tone,
   title:n.tag||'Guided Tuning',text,stage:n.stage||null,tour_step:chapter?.step||null,tour_total:this.catalog.tour_steps?.length||null,tour_title:chapter?.title||null,definition_key:ecuKey||null,
   freshness:r.fresh===true?'live':r.fresh===false?'stale':'unavailable',
   choices:n.choices.map((ch,i)=>({id:n.id+':'+i,label:ch.label,go:ch.go,enabled:(!ch.go.startsWith('action.manual.')&&!ch.go.startsWith('action.field.'))||!!target,
    blocked_reason:(ch.go.startsWith('action.manual.')||ch.go.startsWith('action.field.'))&&!target?'No matching menu entry in the active INI':null})),
   manual_target:target,field_id:n._field?.id||null,check:progress,chapter_progress:chapter?counts:null,can_back:this.stack.length>0,can_resume:this.canResume(),
   voice:this.catalog.character?.voice_asset||null,
   assets:{head:this.catalog.character?.sprite_head,jaw:this.catalog.character?.sprite_jaw,mouth:this.catalog.character?.sprite_mouth},
   reasoning:n._reasoning||null,read_only:true};
 }
}
