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
