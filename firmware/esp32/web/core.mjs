/* BrickOS rusEFI definition and calibration codec. No external dependencies.
 * Numeric expressions in generated INI files are evaluated by a small arithmetic
 * parser, never eval(), Function(), or browser JavaScript execution.
 */
export const TYPES = {U08:1,S08:1,U16:2,S16:2,U32:4,S32:4,F32:4,F64:8};
export function splitArgs(s) {
  const out=[];let cur='',depth=0,quote=false,escape=false;
  for(const c of s){
    if(c==='"'&&!escape) quote=!quote;
    if(!quote){if('[{('.includes(c))depth++;if(']})'.includes(c))depth=Math.max(0,depth-1);}
    if(c===','&&!quote&&!depth){out.push(cur.trim());cur='';continue}
    cur+=c;escape=c==='\\'&&!escape;if(c!=='\\')escape=false;
  }
  out.push(cur.trim());return out;
}
function stripComment(s){let quote=false,escape=false;for(let i=0;i<s.length;i++){
  const c=s[i];if(c==='"'&&!escape)quote=!quote;
  if(c===';'&&!quote)return s.slice(0,i).trim();escape=c==='\\'&&!escape;if(c!=='\\')escape=false;
}return s.trim()}
function clean(s){return String(s||'').trim().replace(/^"|"$/g,'').trim()}
export function mathValue(input,macros={}) {
  if(input===undefined||input===null)return null;
  let expr=String(input).trim().replace(/^\{(.*)\}$/,'$1');
  if(!expr || expr.includes('@@') || /[<>=?!&|;:`]/.test(expr))return null;
  let pos=0;
  function space(){while(/\s/.test(expr[pos]||''))pos++}
  function primary(){space();const c=expr[pos];
    if(c==='+'||c==='-'){pos++;const v=primary();return v===null?null:(c==='-'?-v:v)}
    if(c==='('){pos++;const v=add();space();if(expr[pos++]!==')')throw Error('Expected )');return v}
    const rem=expr.slice(pos);
    const number=rem.match(/^(?:0x[0-9a-f]+|(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)/i);
    if(number){pos+=number[0].length;return Number(number[0])}
    const word=rem.match(/^[A-Za-z_][A-Za-z0-9_]*/);
    if(word){pos+=word[0].length;if(Object.prototype.hasOwnProperty.call(macros,word[0])){
      const v=mathValue(macros[word[0]],{});if(v!==null)return v;
    }throw Error('Unresolved symbol')}
    throw Error('Invalid expression');
  }
  function multiply(){let v=primary();for(;;){space();let op=expr[pos];if(op!=='*'&&op!=='/')break;pos++;let b=primary();v=op==='*'?v*b:v/b;}return v}
  function add(){let v=multiply();for(;;){space();let op=expr[pos];if(op!=='+'&&op!=='-')break;pos++;let b=multiply();v=op==='+'?v+b:v-b;}return v}
  try{const n=add();space();return pos===expr.length&&Number.isFinite(n)?n:null}catch{return null}
}
export function parseIni(text) {
  let section='',page=1;const macros={},constantLines=[],channelLines=[],tables=[],rawSections={};
  const lines=String(text).replace(/\r/g,'').split('\n');
  for(const original of lines){let line=stripComment(original);if(!line)continue;
    if(/^#define\s+/i.test(line)){
      const m=line.match(/^#define\s+(\w+)\s*=\s*(.*)$/);if(m)macros[m[1]]=clean(m[2]);continue;
    }
    const sec=line.match(/^\[([^\]]+)\]/);if(sec){section=sec[1].trim().toLowerCase();continue}
    const eq=line.indexOf('=');if(eq<1)continue;
    const name=line.slice(0,eq).trim(),value=line.slice(eq+1).trim();
    if(!rawSections[section])rawSections[section]={};
    if(rawSections[section][name]===undefined)rawSections[section][name]=value;
    if(section==='constants' && name.toLowerCase()==='page') {const n=mathValue(value,macros);if(n>0)page=n;continue}
    if(section==='constants')constantLines.push({name,value,page});
    if(section==='outputchannels')channelLines.push({name,value,page:0});
    if(section==='tableeditor' && name.toLowerCase()==='table')tables.push(clean(value));
  }
  const constants=rawSections.constants||{},channels=rawSections.outputchannels||{};
  const listVal=(str)=>splitArgs(str||'').map(v=>mathValue(v,macros));
  const sizes=listVal(constants.pageSize);
  // rusEFI's generated INI uses quoted escaped byte pairs ("\\x00\\x01")
  // rather than numeric page IDs. The %2i packet identifier is little endian.
  const ids=splitArgs(constants.pageIdentifier||'').map(item=>{
    const numeric=mathValue(item,macros);if(numeric!==null)return numeric;
    const raw=clean(item),match=raw.match(/^\\x([0-9a-f]{2})\\x([0-9a-f]{2})$/i);
    return match?parseInt(match[1],16)|(parseInt(match[2],16)<<8):null;
  });
  let all=[];
  function parseField(item,scope){
    const vals=splitArgs(item.value),kind=clean(vals[0]).toLowerCase();
    if(!['scalar','array','bits'].includes(kind))return null;
    const type=clean(vals[1]).toUpperCase();if(!TYPES[type])return null;
    const offset=mathValue(vals[2],macros);if(offset===null||offset<0||!Number.isInteger(offset))return null;
    let width=1,height=1,index=3;
    if(kind==='array'){
      const match=(vals[3]||'').match(/\[\s*(\d+)\s*(?:x\s*(\d+)\s*)?\]/i);
      if(!match)return null;
      width=Number(match[1]);height=Number(match[2]||1);index=4;
      if(width*height>16384)return null;
    }
    let bitRange=null;
    if(kind==='bits'){
      const m=(vals[3]||'').match(/\[\s*(\d+)\s*(?::\s*(\d+))?\s*\]/);
      if(m){bitRange=[+m[1],+(m[2]||m[1])];}index=4;
    }
    const options=kind==='bits'?vals.slice(index).map(clean):[];
    const units=kind==='bits'?'':clean(vals[index]);
    // For scalar/array, scale and translate follow units. A runtime expression is not safe to write.
    const scale=kind==='bits'?1:mathValue(vals[index+1],macros);
    const translate=kind==='bits'?0:mathValue(vals[index+2],macros);
    const field={name:item.name,kind,type,offset,page:item.page,scope,
      units,options,scale,translate,bitRange,width,height,byteLength:width*height*TYPES[type],
      editable:scope==='cal' && (kind==='bits'||(scale!==null&&scale!==0&&translate!==null))};
    return field;
  }
  const cal=constantLines.map(x=>parseField(x,'cal')).filter(Boolean);
  const out=channelLines.map(x=>parseField(x,'output')).filter(Boolean);
  const signature=clean((rawSections.tunerstudio||{}).signature||(rawSections.megatune||{}).signature||'');
  return {signature,cal,channels:out,macros,pageSizes:sizes,pageIdentifiers:ids,
    pageBurnable:splitArgs(constants.burnCommand||'').map(x=>/^B%2i$/i.test(clean(x))),
    pageWritable:splitArgs(constants.pageChunkWrite||'').map(x=>/^C%2i%2o%2c%v$/i.test(clean(x))),
    blockingFactor:mathValue(constants.blockingFactor,macros),
    outputSize:mathValue(channels.ochBlockSize,macros),
    endianness:/big/i.test(constants.endianness||'')?'big':'little',tables,
    warnings:[...(signature?[]:['No ECU signature declared']),...(out.length?[]:['No output channels parsed']),...(cal.length?[]:['No calibration fields parsed'])]};
}
export function hex(bytes){return Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('')}
export function unhex(str){if(!/^(?:[0-9a-f]{2})*$/i.test(str||''))throw Error('Invalid hex');return Uint8Array.from(str.match(/../g)||[],v=>parseInt(v,16))}
export function le16(x){return [x&255,(x>>8)&255]}
export function pageId(def,page){
  if(!Number.isInteger(page)||page<1||page>(def.pageSizes?.length||0))throw Error('Unknown ECU calibration page');
  const id=def.pageIdentifiers?.[page-1];
  if(!Number.isInteger(id)||id<0||id>65535)throw Error('Invalid INI page identifier');
  return id;
}
export function readArgs(def,page,offset,count){if(!Number.isInteger(offset)||!Number.isInteger(count)||offset<0||offset>65535||count<1||count>1024||offset+count>65536)throw Error('Read out of range');return hex([...le16(pageId(def,page)),...le16(offset),...le16(count)])}
export function outputArgs(offset,count){if(offset<0||count<1||count>1024||offset+count>65536)throw Error('Output range invalid');return hex([...le16(offset),...le16(count)])}
export function writeArgs(def,page,offset,data){if(!Number.isInteger(offset)||data.length<1||data.length>512||offset<0||offset+data.length>65536)throw Error('Write out of range');return hex([...le16(pageId(def,page)),...le16(offset),...le16(data.length),...data])}
export function burnArgs(def,page){return hex(le16(pageId(def,page)))}
export function getValue(bytes,field,index=0,endianness='little'){
  const n=TYPES[field.type],off=field.offset+index*n;
  if(off<0||off+n>bytes.length)return null;
  const dv=new DataView(bytes.buffer,bytes.byteOffset+off,n),little=endianness!=='big';
  let raw;
  switch(field.type){case 'U08':raw=dv.getUint8(0);break;case 'S08':raw=dv.getInt8(0);break;
  case 'U16':raw=dv.getUint16(0,little);break;case 'S16':raw=dv.getInt16(0,little);break;
  case 'U32':raw=dv.getUint32(0,little);break;case 'S32':raw=dv.getInt32(0,little);break;
  case 'F32':raw=dv.getFloat32(0,little);break;case 'F64':raw=dv.getFloat64(0,little);break;default:return null}
  if(field.kind==='bits'&&field.bitRange){const [a,b]=field.bitRange;return Math.floor(raw/2**a)%2**(b-a+1)}
  if(field.scale===null||field.translate===null)return raw;
  return raw*field.scale+field.translate;
}
export function putValue(bytes,field,index,value,endianness='little'){
  if(!field.editable)throw Error('Field is not safely editable');
  const n=TYPES[field.type],off=field.offset+index*n;
  if(off<0||off+n>bytes.length)throw Error('Write outside page buffer');
  const dv=new DataView(bytes.buffer,bytes.byteOffset+off,n),little=endianness!=='big';
  if(!Number.isFinite(value))throw Error('Invalid numeric value');
  let raw;
  if(field.kind==='bits' && field.bitRange){const [a,b]=field.bitRange;
    const old=getValue(bytes,{...field,kind:'scalar',scale:1,translate:0},index,endianness);
    const span=2**(b-a+1),bit=2**a;
    if(!Number.isInteger(value)||value<0||value>=span)throw Error('Bit value outside field range');
    raw=old-(Math.floor(old/bit)%span)*bit+value*bit;
  } else raw=(value-field.translate)/field.scale;
  if(!field.type.startsWith('F')){
    if(!Number.isInteger(value) && field.kind==='bits')throw Error('Bit value must be integer');
    raw=Math.round(raw);
    const range={U08:[0,255],S08:[-128,127],U16:[0,65535],S16:[-32768,32767],U32:[0,4294967295],S32:[-2147483648,2147483647]}[field.type];
    if(!Number.isFinite(raw)||raw<range[0]||raw>range[1])throw Error('Value outside field range');
  }
  switch(field.type){case 'U08':dv.setUint8(0,raw);break;case 'S08':dv.setInt8(0,raw);break;
  case 'U16':dv.setUint16(0,raw,little);break;case 'S16':dv.setInt16(0,raw,little);break;
  case 'U32':dv.setUint32(0,raw,little);break;case 'S32':dv.setInt32(0,raw,little);break;
  case 'F32':dv.setFloat32(0,raw,little);break;case 'F64':dv.setFloat64(0,raw,little);break}
}
// TunerStudio (ECU) calibration fields reference absolute offsets within each page.
export function relative(field,rangeStart){return {...field,offset:field.offset-rangeStart}}
export function crc32(buf){let c=0xffffffff;for(const b of buf){c^=b;for(let k=0;k<8;k++)c=(c>>>1)^((c&1)?0xedb88320:0)}return (c^0xffffffff)>>>0}
export function readBLog(buffer){const b=new Uint8Array(buffer),d=new DataView(buffer);if(b.length<12||String.fromCharCode(...b.slice(0,6))!=='BRLOG1')throw Error('Invalid BrickOS binary log');const offset=d.getUint16(6,true),size=d.getUint16(8,true),interval=d.getUint16(10,true);if(size<1||size>512)throw Error('Invalid record length');const records=[];for(let i=12;i+4+size<=b.length;i+=4+size){records.push({ms:d.getUint32(i,true),bytes:b.slice(i+4,i+4+size)})}return {offset,size,interval,records,truncated:(b.length-12)%(4+size)!==0}}


// Portable staged calibration patch. Does NOT contain executable code and can
// only be applied to the exact matching ECU definition, field and original data.
export function makePatch(def,field,original,modified){
  if(!def.signature || original.length!==modified.length || original.length!==field.byteLength)
    throw Error('Cannot export an incomplete calibration patch');
  const patch={format:'BrickOS-calibration-patch',version:1,
    signature:def.signature,page:field.page,field:field.name,offset:field.offset,
    type:field.type,length:field.byteLength,original:hex(original),modified:hex(modified)};
  patch.crc32=crc32(new TextEncoder().encode(JSON.stringify(patch)));
  return patch;
}
export function validatePatch(def,field,original,patch){
  if(!patch || patch.format!=='BrickOS-calibration-patch' || patch.version!==1)
    throw Error('Unsupported tune patch format');
  if(!def.signature || def.signature!==patch.signature || field.page!==patch.page ||
      field.name!==patch.field || field.offset!==patch.offset ||
      field.type!==patch.type || field.byteLength!==patch.length)
    throw Error('Patch does not match this ECU firmware and field');
  const copy={...patch};delete copy.crc32;
  if(crc32(new TextEncoder().encode(JSON.stringify(copy)))!==patch.crc32)
    throw Error('Patch checksum mismatch');
  if(hex(original)!==patch.original)
    throw Error('ECU field differs from patch source; refusing stale patch');
  const data=unhex(patch.modified);
  if(data.length!==field.byteLength)throw Error('Patch data size mismatch');
  return data;
}

/* Exact rusEFI official INI resolver. Never guess a target from the board name. */
export function iniUrlForSignature(value){
  const signature=String(value||'').trim();
  // Official rusEFI signature structure: white-label branch.year.month.day.board.hash.
  const m=signature.match(/^rusEFI ([A-Za-z0-9_-]+)\.(\d{4})\.(\d{2})\.(\d{2})\.([A-Za-z0-9_-]+)\.(\d+)$/);
  if(!m)return null; // Custom white labels require a separately published definition.
  return `https://rusefi.com/online/ini/rusefi/${m[1]}/${m[2]}/${m[3]}/${m[4]}/${m[5]}/${m[6]}.ini`;
}
export function matchingIni(text, signature, parseIni){
  const def=parseIni(text);
  if(!def.signature || def.signature!==signature)throw Error('INI signature differs from live ECU: expected '+signature+'; received '+(def.signature||'missing'));
  if(!def.channels.length || !def.cal.length)throw Error('Incomplete rusEFI INI definition');
  return def;
}

// TunerStudio menu/dialog layout parser (read-only navigation metadata)
const tsClean=s=>String(s||'').trim().replace(/^"|"$/g,'').replace(/&/g,'');
function lines(text,section){let active=false,out=[];for(const line of String(text||'').replace(/\r/g,'').split('\n')){const h=line.match(/^\s*\[([^\]]+)\]/);if(h){active=h[1].toLowerCase()===section.toLowerCase();continue}if(active){const s=line.trim();if(s&&!s.startsWith(';')&&!s.startsWith('#'))out.push(s)}}return out}
function args(raw){return splitArgs(raw).map(tsClean)}
export function tunerStructure(text){
 const roots=[];let category=null,group=null;
 for(const line of lines(text,'Menu')){
  if(/^menuDialog\s*=/i.test(line)){category=null;group=null;continue}
  const m=line.match(/^(menu|groupMenu|subMenu|groupChildMenu)\s*=\s*(.*)$/i);if(!m)continue;
  const kind=m[1].toLowerCase(),a=args(m[2]);
  if(kind==='menu'){category={name:a[0],groups:[],items:[],entries:[]};roots.push(category);group=null}
  if(!category)continue;
  if(kind==='groupmenu'){group={name:a[0],items:[]};category.groups.push(group);category.entries.push({kind:'group',value:group})}
  if(kind==='submenu'||kind==='groupchildmenu'){
   if(!a[0]||a[0].startsWith('std_separator'))continue;
   const item={id:a[0],label:a[1]||a[0],section:category.name,group:kind==='groupchildmenu'&&group?group.name:null};
   if(kind==='groupchildmenu'&&group)group.items.push(item);else {category.items.push(item);category.entries.push({kind:'item',value:item});}
  }
 }
 const tableMap=new Map();let table=null;
 for(const line of lines(text,'TableEditor')){
  const m=line.match(/^(table|xBins|yBins|zBins)\s*=\s*(.*)$/i);if(!m)continue;
  if(m[1].toLowerCase()==='table'){
   const a=args(m[2]);table={name:a[2]||a[0],id:a[0],mapId:a[1],z:'',x:'',y:''};tableMap.set(a[0],table);if(a[1])tableMap.set(a[1],table);
  }else if(table){const a=args(m[2]);const prop=m[1].toLowerCase()[0];table[prop]=a[0]||''}
 }
 const dialogs=new Map();let dialog=null;
 for(const line of lines(text,'UserDefined')){
  const m=line.match(/^(dialog|field|panel)\s*=\s*(.*)$/i);if(!m)continue;
  const a=args(m[2]),kind=m[1].toLowerCase();
  if(kind==='dialog'){dialog={id:a[0],label:a[1]||a[0],fields:[],panels:[]};dialogs.set(a[0],dialog)}
  else if(dialog&&kind==='field'&&a.length>1&&/^[A-Za-z_]\w*$/.test(a[1]))dialog.fields.push({label:a[0],name:a[1]});
  else if(dialog&&kind==='panel'&&a[0])dialog.panels.push(a[0]);
 }
 const byId=new Map();for(const cat of roots){for(const item of cat.items)byId.set(item.id,item);for(const g of cat.groups)for(const item of g.items)byId.set(item.id,item)}
 function resolve(id,depth=0,seen=new Set()){
  if(depth>7||seen.has(id))return {fields:[],tables:[]};seen.add(id);
  const foundTables=[],foundFields=[];const tb=tableMap.get(id);
  if(tb&&tb.z)foundTables.push(tb);
  const d=dialogs.get(id);
  if(d){foundFields.push(...d.fields);for(const p of d.panels){const child=resolve(p,depth+1,new Set(seen));foundFields.push(...child.fields);foundTables.push(...child.tables)}}
  return {fields:[...new Map(foundFields.map(f=>[f.name,f])).values()],tables:[...new Map(foundTables.map(t=>[t.id,t])).values()]};
 }
 return {categories:roots,itemById:byId,resolve,tables:[...new Set(tableMap.values())],tableCount:new Set([...tableMap.values()]).size,dialogCount:dialogs.size};
}

// BLV1: 16-byte little-endian header, then raw rusEFI output channel data.
// Exact framing checks prevent stale/corrupted telemetry from reaching gauges.
export function decodeLiveFrame(buffer) {
  const bytes=buffer instanceof Uint8Array?buffer:new Uint8Array(buffer);
  if(bytes.length<20 || bytes[0]!==66 || bytes[1]!==76 || bytes[2]!==86 || bytes[3]!==49)
    throw Error('Invalid BLV1 telemetry header');
  const data=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
  const sequence=data.getUint32(4,true),timestamp=data.getUint32(8,true);
  const length=data.getUint16(12,true),usbDurationMs=data.getUint16(14,true);
  if(length<4||length>3072||bytes.length!==16+length)
    throw Error('Incomplete telemetry frame');
  return {sequence,timestamp,length,usbDurationMs,bytes:bytes.slice(16)};
}

/** Construct a minimal write plan from multiple staged fields. No ECU communication.
 * Overlapping edits are only valid if they agree on original and desired bytes.
 * Chunks are maximally 128 bytes, avoiding large packets and unnecessary writes.
 */
export function calibrationWritePlan(entries,maxChunk=128){
  if(!Number.isInteger(maxChunk)||maxChunk<1||maxChunk>512)throw Error('Invalid write chunk size');
  const pages=new Map();
  for(const e of entries){
    if(!e?.field||!Number.isInteger(e.start)||e.start<0||!(e.original instanceof Uint8Array)||!(e.current instanceof Uint8Array)||e.original.length!==e.current.length)throw Error('Invalid staged calibration field');
    let bytes=pages.get(e.field.page);if(!bytes){bytes=new Map();pages.set(e.field.page,bytes)}
    for(let i=0;i<e.original.length;i++){
      if(e.original[i]===e.current[i])continue;
      const pos=e.start+i,old=e.original[i],desired=e.current[i];
      if(pos>65535)throw Error('Calibration byte outside page');
      const found=bytes.get(pos);
      if(found&&(found.original!==old||found.modified!==desired))throw Error('Overlapping edits conflict at page '+e.field.page+', byte '+pos);
      bytes.set(pos,{original:old,modified:desired});
    }
  }
  const plan=[];
  for(const [page,bytes] of [...pages].sort((a,b)=>a[0]-b[0])){
    const sorted=[...bytes].sort((a,b)=>a[0]-b[0]);
    for(let i=0;i<sorted.length;){
      const offset=sorted[i][0],original=[],modified=[];
      do{original.push(sorted[i][1].original);modified.push(sorted[i][1].modified);i++}
      while(i<sorted.length&&sorted[i][0]===offset+modified.length&&modified.length<maxChunk);
      plan.push({page,offset,original:Uint8Array.from(original),modified:Uint8Array.from(modified)});
    }
  }
  return plan;
}
