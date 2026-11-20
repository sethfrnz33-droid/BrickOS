/* Fast, read-only browser cache for a definition already supplied by the ESP.
 * The ESP's SHA-256 of its saved SD-imported INI is authoritative for
 * invalidation, never a guessed filename, size alone, or old ECU signature.
 * The browser must still compare parsed signature to the live ECU before tuning.
 * IndexedDB is optional; private browsing/quota failures fall back to ESP.
 */
export const INI_FAST_SCHEMA=1;
const DB='brickos-local-ini-v1',STORE='definitions',KEY='single';
const validHash=h=>typeof h==='string'&&/^[0-9a-f]{64}$/i.test(h);
function sourceSignature(text) {
  if(typeof text!=='string'||text.length<100)return '';
  const head=text.slice(0,50000);
  const match=head.match(/(?:^|\n)\s*\[TunerStudio\]([\s\S]*?)(?=\n\s*\[|$)/i);
  const sig=match?.[1].match(/(?:^|\n)\s*signature\s*=\s*["']?([^\r\n;"']+)/i);
  return sig?.[1]?.trim()||'';
}
export function validLocalDefinition(item, hash, signature='', size=0) {
  if(!validHash(hash)||!item||item.schema!==INI_FAST_SCHEMA||
     item.key!==KEY||item.hash!==hash||
     !item.definition||!Array.isArray(item.definition.cal)||!Array.isArray(item.definition.channels)||
     !item.definition.cal.length||!item.definition.channels.length||
     typeof item.rawIni!=='string'||item.rawIni.length<100||
     !Number.isInteger(item.size)||item.size<100)return false;
  if(size>0&&item.size!==size)return false;
  const declared=sourceSignature(item.rawIni);
  if(!declared||item.definition.signature!==declared)return false;
  if(signature&&declared!==signature)return false;
  return true;
}
function openDB() {
  if(typeof indexedDB==='undefined')return Promise.resolve(null);
  return new Promise(resolve=>{
    let settled=false;
    const done=db=>{if(settled){if(db)db.close();return;}settled=true;resolve(db);};
    try{
      const req=indexedDB.open(DB,1);
      req.onupgradeneeded=()=>{if(!req.result.objectStoreNames.contains(STORE))req.result.createObjectStore(STORE,{keyPath:'key'});};
      req.onsuccess=()=>done(req.result);
      req.onerror=()=>done(null);
      req.onblocked=()=>done(null);
    }catch{done(null)}
  });
}
export async function readLocalIni(hash,signature='',size=0) {
  if(!validHash(hash))return null;
  const db=await openDB();if(!db)return null;
  try{
    const item=await new Promise(resolve=>{
      const tx=db.transaction(STORE,'readonly');
      const req=tx.objectStore(STORE).get(KEY);
      req.onsuccess=()=>resolve(req.result||null);req.onerror=()=>resolve(null);
      tx.onabort=()=>resolve(null);
    });
    return validLocalDefinition(item,hash,signature,size)?item:null;
  }catch{return null}finally{db.close()}
}
export async function writeLocalIni(hash,rawIni,definition,size) {
  if(!validHash(hash)||!validLocalDefinition(
    {schema:INI_FAST_SCHEMA,key:KEY,hash,rawIni,definition,size},hash,'',size))return false;
  const db=await openDB();if(!db)return false;
  try{
    return await new Promise(resolve=>{
      const tx=db.transaction(STORE,'readwrite');
      tx.objectStore(STORE).put({schema:INI_FAST_SCHEMA,key:KEY,hash,rawIni,definition,size});
      tx.oncomplete=()=>resolve(true);tx.onerror=()=>resolve(false);tx.onabort=()=>resolve(false);
    });
  }catch{return false}finally{db.close()}
}
export async function clearLocalIni(){
  const db=await openDB();if(!db)return;
  try{await new Promise(resolve=>{
    const tx=db.transaction(STORE,'readwrite');tx.objectStore(STORE).delete(KEY);
    tx.oncomplete=()=>resolve();tx.onerror=()=>resolve();tx.onabort=()=>resolve();
  })}catch{}finally{db.close()}
}
