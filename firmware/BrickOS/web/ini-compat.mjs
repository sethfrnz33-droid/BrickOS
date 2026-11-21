/* Read-only TunerStudio INI interoperability inventory.
 * Describing a definition is NOT implementing its serial protocol.
 * Unknown directives remain unknown; no executable command templates.
 */
export const INI_COMPAT_SCHEMA='brickos.ini.compatibility.v1';
const SECTION=/^\s*\[([^\]\r\n]+)\]/gm;
const DIRECTIVE=/^\s*([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(.+)$/gm;
export function inspectIniCompatibility(raw,definition=null) {
 const text=String(raw||'');
 const sections=[];let match;
 while((match=SECTION.exec(text))!==null&&sections.length<200)sections.push(match[1].trim());
 const names=new Set(sections.map(x=>x.toLowerCase()));
 const tuning=names.has('tunerstudio')||names.has('megatune');
 const declarations=new Map();
 while((match=DIRECTIVE.exec(text))!==null){
  const name=match[1].toLowerCase();
  if(!declarations.has(name))declarations.set(name,match[2].replace(/\s*;.*$/,'').trim().slice(0,400));
 }
 const signature=String(definition?.signature||'').trim();
 const source=signature+' '+(declarations.get('signature')||'');
 // Identification based on source text only; it never authorizes communication.
 const family=/rusefi/i.test(source)?'rusEFI':/speeduino/i.test(source)?'Speeduino':
   /megasquirt|ms2extra|ms3|ms1\/extra|microsquirt/i.test(source)?'MegaSquirt':'Not identified';
 const required=['querycommand','pageidentifier','pagereadcommand','pagevaluewrite','pagechunkwrite','burncommand','ochgetcommand'];
 const protocol=Object.fromEntries(required.map(n=>[n,declarations.has(n)]));
 const warnings=[];
 if(!tuning)warnings.push('No TunerStudio/MegaTune section found.');
 if(!names.has('constants'))warnings.push('Calibration field section missing.');
 if(!names.has('outputchannels'))warnings.push('Live reading section missing.');
 if(!definition?.signature)warnings.push('Firmware identity was not decoded from this definition.');
 if(family!=='rusEFI')warnings.push('This ECU family needs its own verified communication adapter; it cannot use the existing BrickOS rusEFI USB command path by default.');
 if(sections.length===200)warnings.push('Section inventory truncated.');
 return {schema:INI_COMPAT_SCHEMA,family_guess:family,signature:signature||null,sections:sections.length,
  sections_present:{tunerstudio:tuning,constants:names.has('constants'),output_channels:names.has('outputchannels')},
  declared_protocol_features:protocol,calibration_fields:definition?.cal?.length??null,
  output_fields:definition?.channels?.length??null,mode:'definition_inspection_only',
  transport_compatible:false,read_supported:false,write_supported:false,
  warnings};
}

/* Browse names in an arbitrary TunerStudio definition locally on the phone.
 * The browser does NOT activate the definition, read ECU memory or enable writes.
 * Partial sections are clearly a preview, never complete parse certification. */
export function scanIniText(raw,{maxFields=2200}={}){
 const text=String(raw||'');
 if(text.length>1400000)throw Error('INI preview is too large (limit 1.4 million characters)');
 const limit=Math.max(1,Math.min(2200,maxFields|0));
 let section='',truncated=false;
 const fields=[],seen=new Set(),sections=new Set();
 const lines=text.replace(/\r/g,'').split('\n');
 for(const line of lines){
  const header=/^\s*\[([^\]\n]+)\]\s*(?:;.*)?$/.exec(line);
  if(header){section=header[1].trim();sections.add(section);continue;}
  if(!/^(Constants|OutputChannels)$/i.test(section))continue;
  if(/^\s*(?:[;#]|$)/.test(line))continue;
  const m=/^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)/.exec(line);
  if(!m)continue;
  const id=m[1],key=section+':'+id;
  if(seen.has(key))continue;
  seen.add(key);
  const rawKind=m[2].trim().split(',')[0].replace(/[;].*$/,'').trim().slice(0,28);
  // Only a name, section, and unexecuted field-kind token are retained.
  if(fields.length>=limit){truncated=true;continue;}
  const label=id.replace(/([a-z0-9])([A-Z])/g,'$1 $2').replace(/[_-]/g,' ');
  fields.push({name:id,label,section,declared_kind:rawKind,verified:false});
 }
 return {schema:'brickos.ini.text-preview.v1',sections:[...sections],fields,truncated,
  status:'names_only_unverified',read_enabled:false,write_enabled:false};
}
