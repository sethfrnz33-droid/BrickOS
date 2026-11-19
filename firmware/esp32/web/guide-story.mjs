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
 for(const [id,n] of Object.entries(c.nodes)){
  if(n.id!==id||n.speaker!=='seth'||typeof n.text!=='string'||!Array.isArray(n.choices))throw Error('Bad dialogue node '+id);
  for(const ch of n.choices){if(!ch.label||typeof ch.go!=='string')throw Error('Bad choice '+id);
   if(!has(c.nodes,ch.go)&&ch.go!=='back'&&ch.go!=='resume.last'&&!ch.go.startsWith('workspace.')&&!ch.go.startsWith('action.manual.')&&!/^field\.\d+$/.test(ch.go)&&!ch.go.startsWith('action.field.'))throw Error('Broken dialogue link '+id+' -> '+ch.go);
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
 constructor(catalog=null){this.catalog=null;this.nodeId='welcome';this.stack=[];this.definitionKey='';this.lastVisited='';this.history=[];if(catalog)this.load(catalog)}
 load(c){this.catalog=validateStoryCatalog(c);this.nodeId=c.entry;this.stack=[];this.lastVisited='';this.history=[];}
 bindDefinition(key){const k=key||'';if(k!==this.definitionKey){this.definitionKey=k;this.nodeId=this.catalog?.entry||'welcome';this.stack=[];this.lastVisited='';this.history=[];
   if(k&&typeof localStorage!=='undefined')try{const raw=JSON.parse(localStorage.getItem('brickos.guide.state.'+k)||'null');if(raw?.version===this.catalog?.version&&has(this.catalog.nodes,raw.lastVisited))this.lastVisited=raw.lastVisited;}catch{}
  }return k;}
 canResume(){return !!this.lastVisited&&this.lastVisited!==this.catalog?.entry&&!!this.definitionKey;}
 go(id){if(!this.catalog)throw Error('Guide catalogue unavailable');
  if(id==='back'){this.nodeId=this.stack.pop()||this.catalog.entry;return {type:'node',node:this.nodeId};}
  if(id==='resume.last'){this.nodeId=this.canResume()?this.lastVisited:'tour.start';return {type:'node',node:this.nodeId};}
  if(id.startsWith('workspace.'))return {type:'workspace',tab:id.slice(10)};
  if(id.startsWith('action.field.'))return {type:'manual_field',index:Number(id.slice(13))};
  if(id.startsWith('action.manual.'))return {type:'manual',stage:id.slice(14)};
  if(!has(this.catalog.nodes,id)&&!/^field\.\d+$/.test(id))throw Error('Unknown dialogue step');
  if(id!==this.nodeId&&this.stack.length<90)this.stack.push(this.nodeId);
  this.nodeId=id;
  if(id!=='welcome'&&id!=='resume'&&id!=='targeted'&&id!=='tour.start'&&id!=='tour.map')this.lastVisited=id;
  this.history.push(id);if(this.history.length>200)this.history.shift();
  if(this.definitionKey&&this.canResume()&&typeof localStorage!=='undefined')try{localStorage.setItem('brickos.guide.state.'+this.definitionKey,JSON.stringify({version:this.catalog.version,lastVisited:this.lastVisited}));}catch{}
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
  const r=engine?.report?.()||{};const fields=engine?.fields||{};const values=r.values||{};
  const available=roles=>roles.map(role=>{
   const field=fields[role];if(!field)return role+': not exposed by matched INI';
   const v=values[role];if(!r.fresh||typeof v!=='number'||!Number.isFinite(v))return role+': defined, live value unknown';
   const u=field.units||'';return role+': '+(Math.round(v*100)/100)+(u?' '+u:'');
  }).join('; ');
  const text=n.text.replace(/\{\{available:([^{}]+)\}\}/g,(_,keys)=>available(keys.split('|').filter(Boolean)));
  const target=n._field?(n._menu?{id:n._menu,label:n._field.label}:null):findManualTarget(n.manual||n.stage,menuTree);
  return {schema:this.catalog.schema,catalog_version:this.catalog.version,node_id:n.id,speaker:n.speaker,tone:n.tone,
   title:n.tag||'Guided Tuning',text,stage:n.stage||null,definition_key:ecuKey||null,
   freshness:r.fresh===true?'live':r.fresh===false?'stale':'unavailable',
   choices:n.choices.map((ch,i)=>({id:n.id+':'+i,label:ch.label,go:ch.go,enabled:(!ch.go.startsWith('action.manual.')&&!ch.go.startsWith('action.field.'))||!!target,
    blocked_reason:(ch.go.startsWith('action.manual.')||ch.go.startsWith('action.field.'))&&!target?'No matching menu entry in the active INI':null})),
   manual_target:target,field_id:n._field?.id||null,can_back:this.stack.length>0,can_resume:this.canResume(),
   voice:this.catalog.character?.voice_asset||null,
   assets:{head:this.catalog.character?.sprite_head,jaw:this.catalog.character?.sprite_jaw,mouth:this.catalog.character?.sprite_mouth},
   read_only:true};
 }
}
