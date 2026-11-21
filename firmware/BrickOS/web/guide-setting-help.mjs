/* Human-language, read-only setting help. No hardcoded ECU offsets.
 * A label is a clue, never proof of firmware behavior or a calibration value.
 * All returned terms are display text, not executable controller commands.
 */
export const SETTING_HELP_SCHEMA='brickos.guide.setting-help.v1';

const RULES=[
 {id:'injector_flow',re:/\b(injector|inj)\b.*\b(flow|size|cc|capacity)|\b(flow|size)\b.*\binjector\b/i,
  title:'Injector flow rate',meaning:'This usually describes how much fuel an injector can deliver under stated pressure and test conditions.',check:'Match the injector part number, published characterization, fuel type, and actual pressure arrangement. The correct number cannot be inferred from the setting name.'},
 {id:'injector_deadtime',re:/\b(dead\s*time|deadtime|battery\s*correction|opening\s*time|injector\s*lag|inj\s*lag)\b/i,
  title:'Injector opening time',meaning:'This commonly compensates for the time an injector takes to open, often varying with battery voltage.',check:'Check the injector characterization and the exact controller strategy; a convenient-looking number is not evidence that the setting is correct.'},
 {id:'fuel_ve',re:/\b(volumetric\s*efficiency|ve\s*(table|map)|veTable|fuel\s*table)\b/i,
  title:'Fuel table',meaning:'This is commonly a table describing how fueling changes with engine speed and load.',check:'Confirm which fuel strategy and which table are active before interpreting its axes or cells.'},
 {id:'afr_lambda',re:/\b(afr|lambda|stoich|ego\s*target|mixture\s*target)\b/i,
  title:'Fuel mixture target or measurement',meaning:'Lambda compares the measured or requested mixture with the fuel’s stoichiometric reference. AFR depends on the fuel used.',check:'Determine whether this field is a target, a reading, a correction, or a sensor status; they are not interchangeable.'},
 {id:'fuel_pressure',re:/\b(fuel\s*pressure|rail\s*pressure|fuelPressure|fp\s*sensor)\b/i,
  title:'Fuel pressure',meaning:'This may describe a fuel-pressure reading, a configured sensor, or a pressure-dependent correction.',check:'A named pressure setting does not establish that a sensor is installed or that pressure has been measured.'},
 {id:'ignition',re:/\b(ignition|spark|advance|dwell|timing)\b/i,
  title:'Ignition timing or coil control',meaning:'This is likely part of how the controller schedules spark or drives ignition hardware.',check:'Identify whether it is a commanded target, measured result, correction, or coil-driver parameter; do not assume the field alone proves physical timing.'},
 {id:'trigger',re:/\b(trigger|crank|cam\b|sync|tooth|decoder)\b/i,
  title:'Crank/cam synchronization',meaning:'This probably relates to how the controller identifies crankshaft position or engine phase.',check:'Confirm the installed sensor and wheel arrangement and the firmware-specific decoding mode.'},
 {id:'idle',re:/\b(idle|iac|idle\s*air|idle\s*rpm)\b/i,
  title:'Idle control',meaning:'This appears related to the controller’s requested idle speed or the way it adjusts airflow at idle.',check:'Check whether the active hardware uses an idle valve, electronic throttle, or a different control method.'},
 {id:'coolant',re:/\b(clt|coolant|engine\s*temp|warmup|wue)\b/i,
  title:'Coolant temperature or warmup',meaning:'This likely uses engine temperature to report a measurement or adjust operation as the engine warms.',check:'Confirm the sensor scaling and whether the field is a reading, target, or compensation.'},
 {id:'tps',re:/\b(tps|throttle|pedal|dbw|etb)\b/i,
  title:'Throttle or pedal',meaning:'This probably refers to a throttle position, pedal position, or related control setting.',check:'Distinguish measured values from commands, calibration values, and hardware limits.'},
 {id:'map',re:/\b(map\b|manifold|boost|baro)\b/i,
  title:'Manifold pressure or boost',meaning:'This commonly relates to the air-pressure signal or a boost-control parameter.',check:'Check the units and whether the name represents a physical sensor, estimate, target, or control output.'},
 {id:'knock',re:/\bknock\b/i,
  title:'Knock sensing or correction',meaning:'This is commonly connected to knock detection, diagnostic counters, or timing correction.',check:'A low or zero displayed value is not proof of knock-free operation or sensor quality.'},
 {id:'correction',re:/\b(trim|correction|compensation|stft|ltft|gego|enrichment)\b/i,
  title:'Engine correction',meaning:'This sounds like a correction applied on top of a base calculation.',check:'Check whether it adds, multiplies, or reports an adjustment and when the firmware enables it.'},
 {id:'sensor',re:/\b(sensor|calibration|voltage|adc|input)\b/i,
  title:'Sensor input or calibration',meaning:'This may select a sensor type, its voltage conversion, or the ECU input used to read it.',check:'Check the physical sensor and input wiring separately from its software definition.'},
];
export function explainSetting(field) {
 const label=String(field?.label||field?.name||'this setting').slice(0,140);
 const identifier=String(field?.name||'').slice(0,140);
 const words=(label+' '+identifier).replace(/([a-z])([A-Z])/g,'$1 $2').replace(/[_-]/g,' ');
 const rule=RULES.find(r=>r.re.test(words));
 const units=typeof field?.units==='string'&&field.units.length<24?field.units:'';
 const kind=field?.kind==='array'?'a table or list':field?.kind==='scalar'?'a single setting':'an ECU-defined field';
 const prefix=rule?rule.meaning:'I recognize the name, but it is not specific enough to explain its function reliably.';
 const purpose=rule?.title||'Unknown setting';
 const check=rule?.check||'Look at the original help text and the active strategy for this ECU. I will not guess what an unfamiliar field does.';
 return {schema:SETTING_HELP_SCHEMA,label,purpose,summary:prefix,check,
  type:kind,units,recognized:!!rule,
  certainty:rule?'possible_from_name_only':'unknown_from_name',
  can_recommend_value:false};
}
const WORDS=s=>String(s||'').toLowerCase().replace(/([a-z])([A-Z])/g,'$1 $2').replace(/[^a-z0-9]+/g,' ').trim().split(/\s+/).filter(x=>x.length>1&&!['what','does','mean','the','this','setting','please','could','would','about','tell','show','help','with','how'].includes(x));
export function findSettings(question,inventory=[],limit=5) {
 const q=WORDS(question),out=[];
 if(!q.length)return [];
 const list=Array.isArray(inventory)?inventory:[];
 for(let i=0;i<list.length;i++){
  const f=list[i];const label=String(f?.label||f?.name||'');
  if(!label)continue;
  const n=WORDS(label+' '+String(f?.name||''));
  const score=q.reduce((sum,w)=>sum+(n.includes(w)?4:n.some(t=>t.startsWith(w)||w.startsWith(t))?1:0),0);
  if(score>0)out.push({index:i,field:f,score});
 }
 return out.sort((a,b)=>b.score-a.score||a.index-b.index).slice(0,Math.max(1,Math.min(12,limit)));
}
const TOPICS=[
 {re:/\b(ve|fuel\s*table|fueling|injectors?|dead\s*time|lambda|afr|mixture|fuel\s*pressure)\b/i,stage:'fuel',
  text:'Fueling has a few separate pieces: injector behavior, fuel supply, the base fuel calculation, and any active corrections. Which one are you looking at? We can find the exact settings your ECU exposes.'},
 {re:/\b(spark|ignition|dwell|timing|knock)\b/i,stage:'spark',
  text:'Ignition settings can describe the base timing map, corrections, trigger reference, or coil dwell. We should work out which one you mean before interpreting a value.'},
 {re:/\b(idle|iac|rpm\s*hunt|stall)\b/i,stage:'idle',
  text:'Idle can change with airflow, fueling, engine load, and feedback control. First we should figure out which idle system this ECU uses.'},
 {re:/\b(sync|trigger|crank|cam|tooth)\b/i,stage:'trigger',
  text:'Crank and cam signals tell the ECU where the engine is in its cycle. The important distinction is what the ECU is configured to expect versus what is physically installed.'},
 {re:/\b(sensor|calibrat|coolant|throttle|tps|map|pedal)\b/i,stage:'sensors',
  text:'A sensor needs the right input and the right calibration to make its reading meaningful. Tell me which sensor or setting you want to understand.'},
];
export function answerGeneralQuestion(question) {
 const q=String(question||'').trim();
 const hit=TOPICS.find(t=>t.re.test(q));
 return hit?{text:hit.text,stage:hit.stage}:{
  text:'Tell me the name of a setting, or what you want to understand: fueling, ignition, sensors, idle, or crank and cam signals. I can explain concepts and look up settings in the loaded ECU definition, but I cannot invent an answer about your engine.',stage:null};
}
