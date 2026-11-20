/* BrickOS Guided Tuning — read-only phone UI. No ECU write affordances. */
export const GUIDE_TABS=[['next','Next'],['systems','Systems'],['evidence','Evidence'],['atlas','INI Atlas'],['history','History']];
const esc=x=>String(x??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;','\'':'&#39;'}[c]));
const fmt=n=>typeof n==='number'&&Number.isFinite(n)?(Math.abs(n)<20?n.toFixed(2):n.toFixed(1)):'Unavailable';
const pill=(value,style='')=>`<span class="guide-pill ${style}">${esc(value)}</span>`;
function stat(label,value,detail=''){return `<div class="guide-stat"><small>${esc(label)}</small><strong>${esc(value)}</strong><span>${esc(detail)}</span></div>`}
export function renderGuide(engine,{tab='next',search='',status=null,menuLinks={}}={}){
 const report=engine.report();const canLearn=!!status?.ecu_session_connected&&!!engine.def&&report.fresh;
 const live=report.fresh?'LIVE ECU DATA':status?.ecu_session_connected?'WAITING FOR LIVE DATA':'NO ECU / READ-ONLY';
 const top=`<div class="guide-heading"><div><span class="app-section">BRICKOS / SEPARATE GUIDED WORKSPACE</span><h1>Guided Calibration</h1><p>Follow the active ECU definition from first checks to final review, without changing Manual Tuning.</p></div>
   <div class="guide-top-state">${pill(live,report.fresh?'good':'warn')}${pill(report.mode.toUpperCase())}</div></div>
  <div class="guide-warning">Analysis only. This release observes existing telemetry and produces review findings. It cannot automatically edit ECU RAM, ignition timing, or flash. Do not use a phone while operating a vehicle.</div>
  <nav class="guide-tabs" aria-label="Guided Tuning areas">${GUIDE_TABS.map(([id,name])=>`<button type="button" data-guide-tab="${id}" aria-current="${tab===id?'page':'false'}" class="${tab===id?'selected':''}">${name}</button>`).join('')}</nav>`;
 const control=`<div class="guide-session"><div><strong>Observation session</strong><small>Phone-visible session; ECU transport is unchanged. Live learning stops on disconnect.</small></div>
 <div class="guide-actions"><button type="button" class="btn ghost" data-guide-mode="observe" ${report.mode==='observe'?'disabled':''}>Observe</button><button type="button" class="btn primary" data-guide-mode="learn" ${!canLearn||report.mode==='learn'?'disabled':''}>Learn evidence</button><button type="button" class="btn ghost" data-guide-mode="paused" ${report.mode==='paused'?'disabled':''}>Pause</button></div></div>`;
 let body='';
 if(tab==='next'){
  const p=report.primary;
  body=`<section class="guide-focus"><div class="guide-eyebrow">NEXT PRIORITY / ${esc(p.stage.toUpperCase())}</div><h2>${esc(p.title)}</h2><p>${esc(p.why)}</p><div class="guide-meta">${pill(report.lastReason||'No samples')} ${pill(`${report.accepted} qualifying observations`)} ${pill(`${report.definitionCount} indexed INI parameters`)}</div>
   <div class="guide-actions"><button type="button" class="btn primary" data-guide-tab="evidence">See the evidence</button><button type="button" class="btn ghost" data-guide-tab="systems">Explore all stages</button><button type="button" class="btn ghost" data-guide-tab="atlas">Find setting in INI</button></div></section>
   <div class="guide-stats">${stat('ECU definition',engine.signature?'Matched':'Unavailable',engine.signature||'Matching INI needed')}${stat('Telemetry',live,report.ageMs!==null?`${report.ageMs} ms since received`:'No sample received')}${stat('Accepted',String(report.accepted),`${report.total} snapshots observed`)}</div>
   <section class="guide-panel"><h2>Why this is next</h2><p>Tasks are ordered by identity, sensor validity, missing prerequisites and available evidence. An observation is not proof the calibration is safe or optimal.</p><div class="guide-systems-compact">${report.stages.slice(0,6).map(s=>`<button type="button" data-guide-stage="${s.id}"><span>${esc(s.title)}</span>${pill(s.state)}</button>`).join('')}</div></section>`;
 }
 if(tab==='systems'){
   body=`<section class="guide-panel"><h2>Full calibration journey</h2><p>Select a stage to see the known evidence, prerequisites and unresolved configuration questions. No automatic mechanical operations are performed.</p>
    <div class="guide-steps">${report.stages.map((s,i)=>`<article class="guide-step"><span class="guide-step-number">${i+1}</span><div><h3>${esc(s.title)}</h3><p>${esc(s.description)}</p><small>${esc(s.detail)}</small></div>${pill(s.state)}</article>`).join('')}</div></section>`;
 }
 if(tab==='evidence'){
  const v=report.values;
  const reject=Object.entries(report.rejections).sort((a,b)=>b[1]-a[1]);
  const grid=report.tiles.map(t=>`<div class="guide-heat-cell ${t.hits?'seen':''}" title="${t.hits} qualifying samples" aria-label="Region ${t.index+1}: ${t.hits} observations"><small>${t.hits||'·'}</small></div>`).join('');
  body=`<div class="guide-stats">${stat('Engine RPM',fmt(v.rpm),'INI channel')}${stat('MAP',fmt(v.map),'kPa, if available')}${stat('TPS',fmt(v.tps),'% if available')}${stat('Coolant',fmt(v.clt),'INI units')}${stat('Measured λ',fmt(v.lambda),'Sensor validity required')}${stat('Target λ',fmt(v.target),'From ECU channel')}${stat('Gego',fmt(v.gego),'100 is neutral')}${stat('STFT bank 1',fmt(v.stft1),'INI-native display')}${stat('LTFT bank 1',fmt(v.ltft1),'INI-native display')}</div>
  <section class="guide-panel"><h2>RPM / load evidence coverage</h2><p>Exploratory observations only. This is not the ECU VE map, not a safety indicator, and not a permission to change cells. Each square shows counts in an 8×8 measurement bucket.</p><div class="guide-heat-wrap"><div class="guide-heat-axis">Higher load ↑</div><div class="guide-heatmap">${grid}</div><div class="guide-heat-axis">RPM →</div></div>
   <div class="guide-actions">${pill(`${report.accepted} accepted`,'good')}${pill(`${Math.max(0,report.total-report.accepted)} excluded`)}${pill(report.lastReason)}</div>
   <h3>Why samples were excluded</h3><div class="guide-reasons">${reject.map(([reason,count])=>`<div><span>${esc(reason)}</span><b>${count}</b></div>`).join('')||'<p>No exclusions recorded yet.</p>'}</div></section>
  <section class="guide-panel"><details><summary>Evidence collection filters</summary><p>These settings only change which data is included in the analysis. They do not modify ECU limits or tune tables.</p><div class="guide-filter-grid">${Object.entries(report.filters).map(([name,value])=>`<label>${esc(name)}<input type="number" step="any" data-guide-filter="${esc(name)}" value="${esc(value)}"></label>`).join('')}</div><button class="btn primary" type="button" data-guide-save-filters>Apply analysis filters (clears collected evidence)</button></details></section>`;
 }
 if(tab==='atlas'){
  const term=search.toLowerCase().trim();const all=engine.inventory;const shown=term?all.filter(f=>(f.name+' '+f.units+' '+f.kind).toLowerCase().includes(term)):all;
  body=`<section class="guide-panel"><h2>INI Configuration Atlas</h2><p>${all.length} parsed configuration fields. This is a field inventory, not a claim that every active strategy or UI condition is resolved. Unknown semantics require manual review.</p>
  <label for="guide-search">Search all decoded fields</label><input id="guide-search" class="search guide-search" placeholder="Type a field name, unit, or type" value="${esc(search)}" autocomplete="off">
  <div class="guide-atlas-count">${shown.length} matching fields · showing first 100</div><div class="guide-atlas-list">${shown.slice(0,100).map(f=>`<article class="guide-field"><div><strong>${esc(f.label)}</strong><small>${esc(f.name)} · page ${f.page} · ${f.kind} ${f.type} ${esc(f.units)}</small></div><button type="button" class="btn ghost" data-guide-manual="${esc(f.name)}" ${menuLinks[f.name]?'':'disabled'} title="${menuLinks[f.name]?'Open original INI-defined editor':'This field has no resolved menu item'}">${menuLinks[f.name]?'Open in Tune':'No menu link'}</button></article>`).join('')||'<p>No matching fields.</p>'}</div></section>`;
 }
 if(tab==='history'){
  body=`<section class="guide-panel"><div class="guide-heading"><h2>Evidence and session history</h2><button class="btn ghost" data-guide-export type="button">Export review JSON</button></div><p>This session history is local to this browser view and is not an ECU calibration backup.</p>
   <div class="guide-timeline">${report.events.map(e=>`<article><time>${esc(new Date(e.time).toLocaleString())}</time><strong>${esc(e.type)}</strong><p>${esc(e.message)}</p></article>`).join('')||'<p>No events recorded.</p>'}</div></section>`;
 }
 return top+control+body;
}
