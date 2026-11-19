/* BrickOS offline engine-build policy prototype v1.
 * No hardware I/O, ECU commands, persistence or tuner output.
 * All "eligibility" labels here mean REVIEW-ONLY expert applicability.
 */
export const PROFILE_SCHEMA = 'brickos.engine.profile.v1';
const CHOICES = Object.freeze({
  injection_architecture:['pfi','gdi','mixed','unknown'],
  pump_control:['relay','pwm_compatible_controller','independent_oem_controller','other','unknown'],
  pressure_regulation:['mechanical_manifold_reference','fixed_pressure_reference','electronic_closed_loop','no_confirmed_regulator','hybrid','unknown'],
  rail_pressure_sensor:['none','ecu_sensor','external_gauge','both','unknown'],
  pressure_reference:['gauge','absolute','differential','unknown'],
  fuel_type:['gasoline','ethanol_blend','other','unknown'],
  aspiration:['naturally_aspirated','turbocharged','supercharged','other','unknown'],
  independent_pressure_check:['documented','reported_checked','not_checked','unknown'],
  physical_timing_reference_check:['documented','reported_checked','not_checked','unknown'],
  wideband_equipment:['present','not_present','unknown'],
  professional_instrumentation:['available','unavailable','unknown']
});
const NUMERIC = new Set(['cylinders','displacement_l']);
const TEXT = new Set(['injector_data','engine_family']);
export const PROFILE_FIELDS = Object.freeze([...Object.keys(CHOICES),...NUMERIC,...TEXT]);
const SOURCES = new Set(['unknown','user_reported','ini_configuration','ecu_observed','independent_documented']);
const VERIFICATIONS = new Set(['unverified','reported_checked','documented_external']);
const UNKNOWN_FACT = Object.freeze({value:null,source:'unknown',verification:'unverified',evidence_ref:null});
const val = (p,k) => p?.facts?.[k]?.value ?? null;
const known = (v) => v!==null&&v!==undefined&&v!=='unknown'&&v!=='';
const status=(p,k)=>known(val(p,k))?'present':'unknown';
const question=(id,why)=>({id,why,answer_source:'engine_build_archive'});

function checkValue(key,value){
  if(!PROFILE_FIELDS.includes(key))throw Error('Unknown profile field: '+key);
  if(value===null)return;
  if(CHOICES[key]){if(!CHOICES[key].includes(value))throw Error('Invalid choice for '+key);return;}
  if(NUMERIC.has(key)){if(typeof value!=='number'||!Number.isFinite(value)||value<=0||(key==='cylinders'&&!Number.isInteger(value)))throw Error('Invalid number for '+key);return;}
  if(TEXT.has(key)){if(typeof value!=='string'||value.length>500)throw Error('Invalid text for '+key);return;}
}

export function newEngineProfile(id,alias='',at=''){
  if(typeof id!=='string'||!/^[A-Za-z0-9_-]{1,80}$/.test(id))throw Error('Opaque engine profile ID required');
  if(typeof alias!=='string'||alias.length>120)throw Error('Invalid profile alias');
  const facts={};
  for(const k of PROFILE_FIELDS)facts[k]={...UNKNOWN_FACT};
  return {schema:PROFILE_SCHEMA,profile_id:id,alias,revision:0,created_at:at,updated_at:at,facts,ecu_links:[],events:[]};
}

export function recordBuildFact(profile,key,value,{source='user_reported',verification='unverified',evidence_ref=null,at=''}={}){
  if(profile?.schema!==PROFILE_SCHEMA)throw Error('Unsupported engine profile schema');
  checkValue(key,value);
  if(!SOURCES.has(source)||!VERIFICATIONS.has(verification))throw Error('Invalid fact provenance');
  if(verification==='documented_external'&&(source!=='independent_documented'||typeof evidence_ref!=='string'||evidence_ref.trim().length===0))
    throw Error('Independent documentation reference required');
  if(verification!=='documented_external'&&source==='independent_documented')throw Error('Independent documentation needs evidence');
  const previous=profile.facts[key]||UNKNOWN_FACT;
  const next={value,source,verification,evidence_ref:verification==='documented_external'?evidence_ref:null};
  const newRevision=profile.revision+1;
  return {...profile,revision:newRevision,updated_at:at,
    facts:{...profile.facts,[key]:next},
    events:[...profile.events,{kind:'build_fact',key,previous,new_value:next,revision:newRevision,at}]};
}

export function linkECUDefinition(profile,{signature,ini_fingerprint,at=''}={}){
  if(profile?.schema!==PROFILE_SCHEMA||!signature||!ini_fingerprint)throw Error('Matching ECU signature and INI hash required');
  const links=[...profile.ecu_links,{signature,ini_fingerprint,at}];
  return {...profile,ecu_links:links,revision:profile.revision+1,updated_at:at,events:[...profile.events,
    {kind:'ecu_link',signature,ini_fingerprint,revision:profile.revision+1,at}]};
}

export function evaluateEngineProfile(profile,{ecuInjection=null,definitionMatched=false}={}){
  if(profile?.schema!==PROFILE_SCHEMA)throw Error('Unsupported engine profile schema');
  const findings=[],questions=[],experts={config:'explain',fuel:'hold',spark:'review_only',starting:'review_only'};
  const add=(code,description,blocking=false)=>findings.push({code,description,blocking});
  const ask=(id,why)=>{if(!questions.some(q=>q.id===id))questions.push(question(id,why));};
  const injection=val(profile,'injection_architecture');
  if(!known(injection)){add('INJECTION_UNKNOWN','Injection architecture has not been identified.',true);ask('injection_architecture','PFI, GDI and mixed systems have different supported capabilities.');}
  else if(injection==='gdi'||injection==='mixed'){
    add('UNSUPPORTED_GDI','BrickOS Guided Tuning does not support GDI or mixed PFI/GDI calibration.',true);
    experts.fuel='unsupported_gdi';
  }else if(injection==='pfi'){
    experts.fuel='pfI_review_only';
    if(!definitionMatched){add('ECU_DEFINITION_NOT_MATCHED','No verified matching ECU/INI is provided for fuel interpretation.',true);experts.fuel='hold';}
    if(ecuInjection&&ecuInjection!=='pfi'){add('INJECTION_CONFLICT','ECU configuration disagrees with recorded PFI hardware; reconciliation required.',true);experts.fuel='hold';}
    if(!known(val(profile,'pump_control')))ask('pump_control','Pump command method is independent from pressure regulation.');
    const pump=val(profile,'pump_control'),reg=val(profile,'pressure_regulation'),pressureSensor=val(profile,'rail_pressure_sensor');
    if(!known(reg)){add('PRESSURE_REGULATION_UNKNOWN','Pump command type does not identify the pressure regulation strategy.',true);ask('pressure_regulation','PWM does not prove that a mechanical regulator is absent.');}
    if(!known(pressureSensor))ask('rail_pressure_sensor','Available pressure instrumentation determines which conclusions are measurable.');
    if((pressureSensor==='ecu_sensor'||pressureSensor==='both')&&!known(val(profile,'pressure_reference')))ask('pressure_reference','Gauge, absolute and differential pressure cannot be interchanged.');
    if(reg==='no_confirmed_regulator')add('PRESSURE_CONTROL_UNVERIFIED','No regulator was confirmed. This does not establish rail pressure or closed-loop control.',true);
    if(pump==='pwm_compatible_controller'&&reg==='mechanical_manifold_reference')add('PWM_AND_MECHANICAL','PWM pump command and a mechanical regulator can coexist.',false);
    if(pump==='pwm_compatible_controller'&&reg==='electronic_closed_loop')add('PWM_PRESSURE_STRATEGY','Pressure-feedback configuration and independent measurement evidence require review.',false);
    if(pressureSensor==='none'||pressureSensor==='external_gauge')add('NO_ECU_RAIL_PRESSURE','Do not infer measured rail pressure from pump duty; ECU pressure tracking unavailable.',true);
    const check=profile.facts.independent_pressure_check;
    if(check?.value!=='documented'||check.verification!=='documented_external'){
      add('PRESSURE_VERIFICATION_REQUIRED','Independent fuel-pressure evidence has not been documented.',true);
      ask('independent_pressure_check','Fuel pressure remains relevant even if a pump is PWM-controlled.');
    }
    if(!known(val(profile,'injector_data')))ask('injector_data','Injector characterization is independent from VE observations.');
    if(definitionMatched&&!findings.some(f=>f.blocking))add('PFI_REVIEW_CANDIDATE','Profile data supports review-only PFI analysis, not automatic ECU writes.',false);
  }
  return {schema:'brickos.guide.profile.assessment.v1',profile_id:profile.profile_id,profile_revision:profile.revision,
    read_only:true,auto_write_enabled:false,experts,findings,questions,next_question:questions[0]||null,
    summary_status:findings.some(f=>f.blocking)?'needs_reconciliation_or_evidence':'review_only',definition_matched:!!definitionMatched};
}
