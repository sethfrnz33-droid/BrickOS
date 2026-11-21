import {tunerStructure,iniUrlForSignature,matchingIni,parseIni,hex,unhex,le16,pageId,readArgs,outputArgs,writeArgs,burnArgs,getValue,putValue,relative,readBLog,TYPES,makePatch,validatePatch,decodeLiveFrame,calibrationWritePlan,crc32} from './core.mjs';
import {GuidedAnalyzer,guideRequiredBytes} from './guided.mjs';
import {renderGuide} from './guided-ui.mjs';
import {GuideStory,findManualTarget} from './guide-story.mjs';
import {renderSethStory} from './guide-story-ui.mjs';
import {SethSpeech} from './guide-voice.mjs';
import {readLocalIni,writeLocalIni,clearLocalIni} from './ini-fast-cache.mjs';

const $=x=>document.querySelector(x);
const escapeHTML=x=>String(x??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const cleanLabel=x=>String(x).replace(/([a-z])([A-Z])/g,'$1 $2').replace(/_/g,' ');
// Presentation names only. All data comes from the original INI field addresses.
const channelTitle=n=>({RPMValue:'Engine RPM',MAPValue:'Manifold Pressure',TPSValue:'Throttle Position',coolant:'Coolant Temperature',intake:'Intake Temperature',VBatt:'Battery Voltage',lambdaValue:'Measured Lambda',AFRValue:'Measured AFR',vehicleSpeedKph:'Vehicle Speed'}[n]||cleanLabel(n));
const format=x=>x===null||!Number.isFinite(x)?'—':(Math.abs(x)<20 && x!==Math.round(x)?x.toFixed(2):x.toFixed(Math.abs(x)>999?0:1));
const GUIDE = new GuidedAnalyzer();
const STORY = new GuideStory();
const SETH = new SethSpeech();
const S={route:'overview',status:null,ecuSignature:'',connected:false,definition:null,rawIni:'',
        latest:null,latestMs:0,liveBusy:false,liveHz:20,liveMode:'fast',selectedChannel:'rpm',liveSeq:0,liveRttMs:0,liveFallback:false,liveFailureCount:0,liveRxCount:0,liveWindowMs:0,liveDisplayHz:0,liveThroughput:0,liveDurationMs:0,scopeChannels:['rpm','map','tps'],
        search:'',tableSearch:'',tsSelected:'',tsOpen:{},tsStructure:null,settingsCategory:'All',tableCategory:'All',editor:null,field:null,selectStart:0,selection:[0],
        undo:[],redo:[],backups:new Set(),logs:[],viewer:null,viewChannel:'rpm',trace:[],
        signatureMismatch:false,iniRevision:'',iniResolveState:'Not connected',iniExpectedUrl:'',iniBusy:false,iniImportProgress:'Ready — select any plain-text INI file, regardless of its filename.',working:false,firmwareVersion:'',ecuErrorMessage:'',lastWriteError:'',lastWritePhase:'',statusTransportError:''};

// Edits are device-local and transient until the user explicitly verifies/burns.
S.edits=new Map();S.savedBackups=new Map();S.editorAxes=null;S.editorTable=null;S.axisSelection=null;
const entryKey=(f)=>`${f.page}:${f.name}:${f.offset}`;
const staged=()=>[...S.edits.values()].filter(e=>e.current.some((v,i)=>v!==e.original[i]));
const modifiedBytes=()=>staged().reduce((n,e)=>n+e.current.reduce((m,v,i)=>m+(v!==e.original[i]),0),0);
// The editor stages changes locally; only a successful flash verification saves them on ECU.
const pendingOnPage=page=>staged().filter(e=>e.field.page===page);
S.burnState={active:false,visible:false,step:0,title:'',detail:'',started:0,result:'',success:false};
const BURN_STEPS=['Backup','Compare','Write','Flash','Verify'];
let burnClock=null;
function drawBurnStatus(){
 const box=$('#burn-flow'),b=S.burnState;if(!box)return;
 if(!b.visible){box.hidden=true;box.innerHTML='';return;}
 box.hidden=false;
 const elapsed=b.started?((Date.now()-b.started)/1000).toFixed(1)+'s':'—';
 box.innerHTML=`<section class="burn-dialog" role="dialog" aria-modal="true" aria-label="ECU burn status"><div class="burn-kicker">BRICKOS / ECU FLASH</div><h2>${escapeHTML(b.title)}</h2><p class="burn-detail">${escapeHTML(b.detail)}</p><div class="burn-steps">${BURN_STEPS.map((label,i)=>`<span class="${b.step>i+1?'done':b.step===i+1?(b.active?'now':'done'):''}">${label}</span>`).join('')}</div><div class="burn-track"><div style="width:${Math.min(100,Math.max(0,(b.step-1)/4*100))}%"></div></div><div class="burn-footer"><span>${b.active?'Processing · ':'Elapsed · '}<strong id="burn-elapsed">${elapsed}</strong></span>${b.active?'<span class="burn-pulse">ECU communication active</span>':button('burn-dismiss','Close','primary')}</div>${!b.active?`<p class="burn-result ${b.success?'good':'failed'}">${escapeHTML(b.result)}</p>`:''}</section>`;
}
function beginBurn(count,bytes){
 S.burnState={active:true,visible:true,step:1,title:'Preparing burn',detail:`${count} fields · ${bytes} changed bytes. Saving fresh ECU backups first.`,started:Date.now(),result:'',success:false};
 drawBurnStatus();clearInterval(burnClock);burnClock=setInterval(()=>{const e=$('#burn-elapsed');if(e&&S.burnState.active)e.textContent=((Date.now()-S.burnState.started)/1000).toFixed(1)+'s';},150);
}
function burnStep(step,title,detail){if(!S.burnState.active)return;Object.assign(S.burnState,{step,title,detail});drawBurnStatus()}
function endBurn(success,message){clearInterval(burnClock);Object.assign(S.burnState,{active:false,visible:true,success,step:success?5:S.burnState.step,title:success?'Burn complete':'Burn stopped',detail:success?'ECU readback verified after the flash operation.':'An ECU operation did not complete; check the stage below.',result:message});drawBurnStatus();if(success)setTimeout(()=>{if(!S.burnState.active&&S.burnState.success){S.burnState.visible=false;drawBurnStatus()}},1800);}
function confirmLeaveEditor(nextKey=''){
 if(S.working){toast('ECU operation in progress; wait for the result before navigating',true);return false}
 if(!S.editor||nextKey===S.editor.key)return true;
 const count=pendingOnPage(S.editor.field.page).length;
 if(!count)return true;
 return confirm(`${count} field(s) on this ECU page have changes that are NOT saved.\n\nBurn changes to save them to ECU flash. If you leave now they stay staged in this browser session only, and can be lost on reload or disconnect.\n\nLeave without burning?`);
}
window.addEventListener('beforeunload',e=>{if(staged().length||S.working){e.preventDefault();e.returnValue='';}});

function clearCalibrationSession(){S.edits.clear();S.backups.clear();S.editor=null;S.editorAxes=null;S.editorTable=null;S.axisSelection=null;S.undo=[];S.redo=[]}
// Device-local workspaces: no external network and no reliance on internet access.
const PREF_KEY='brickos.workspace.v38';
function readPrefs(){try{const p=JSON.parse(localStorage.getItem(PREF_KEY)||'{}');return p&&typeof p==='object'?p:{}}catch{return {}}}
const PREF=readPrefs();
S.gaugeSlots=Array.isArray(PREF.gauges)?PREF.gauges.filter(g=>g&&typeof g.name==='string').slice(0,16):[];
S.dashboardLayout=['split','gauges','scope'].includes(PREF.layout)?PREF.layout:'split';
S.scopeNames=Array.isArray(PREF.scope)?PREF.scope.filter(x=>typeof x==='string').slice(0,3):[];
S.tuneFavorites=Array.isArray(PREF.favorites)?PREF.favorites.filter(x=>typeof x==='string').slice(0,24):[];
S.tuneRecent=Array.isArray(PREF.recent)?PREF.recent.filter(x=>typeof x==='string').slice(0,12):[];
S.guideTab='next';S.guideSearch='';S.guideLastRender=0;S.guideMenuLinks={};S.guideScreen='story';S.guidePacket=null;S.guideDefinitionHash='';
S.tuneCategory='';S.tuneSearch='';S.tuneBrowserOpen=true;S.gaugeEdit=null;S.commandOpen=false;S.commandSearch='';
function savePrefs(){try{localStorage.setItem(PREF_KEY,JSON.stringify({gauges:S.gaugeSlots,layout:S.dashboardLayout,scope:S.scopeNames,favorites:S.tuneFavorites,recent:S.tuneRecent}))}catch{toast('Device preference storage unavailable; layout may not persist',true)}}
function navHighlight(){document.querySelectorAll('#nav [data-route]').forEach(b=>b.classList.toggle('active',b.dataset.route===(['tables','settings','tuner'].includes(S.route)?'tuning':S.route)))}

const page=$('#page');
function toast(message,isError=false){const p=$('#toast');p.className=isError?'error':'';p.textContent=message;p.style.display='block';clearTimeout(toast.timer);toast.timer=setTimeout(()=>p.style.display='none',5500)}
function button(action,label,style='',extra=''){return `<button class="btn ${style}" data-action="${action}" ${extra}>${escapeHTML(label)}</button>`}
function pageHeader(index,title,subtitle,extra=''){return `<div class="page-top"><div><p class="eyebrow">BRICKOS / ${index}</p><h1>${escapeHTML(title)}</h1><p class="sub">${escapeHTML(subtitle)}</p></div><div class="btnrow">${extra}</div></div>`}
function notice(msg,style=''){return `<div class="notice ${style}">${msg}</div>`}
function stat(label,value,unit='',sub=''){return `<div class="panel stat"><h3>${escapeHTML(label)}</h3><div class="value">${escapeHTML(value)}<small> ${escapeHTML(unit)}</small></div><p class="tiny">${escapeHTML(sub)}</p></div>`}
function dataRow(a,b,style=''){return `<div class="data-row"><span>${escapeHTML(a)}</span><strong class="${style}">${escapeHTML(b)}</strong></div>`}
function hasDef(){return !!S.definition}
function safeSignature(){return S.ecuSignature && S.definition && S.definition.signature && S.ecuSignature===S.definition.signature}
function connected(){return S.connected && !!S.ecuSignature}

function rebuildTs(){S.tsStructure=S.rawIni?tunerStructure(S.rawIni):null;S.guideDefinitionHash=S.rawIni?crc32(new TextEncoder().encode(S.rawIni)).toString(16):'';refreshGuideLinks();drawTsTree()}
function menuItems(cat){return cat.entries.flatMap(e=>e.kind==='item'?[e.value]:e.value.items)}
function tuneCategoryDefault(){const cats=S.tsStructure?.categories||[];return cats.find(c=>/fuel/i.test(c.name))?.name||cats[0]?.name||''}
function tuneItemRow(item,section){return `<div class="tune-result ${S.tsSelected===item.id?'selected':''}"><button class="tune-result-main" data-ts-item="${escapeHTML(item.id)}"><span class="result-icon">${/table|map|ve|spark|ignit|lambda/i.test(item.label)?'▦':'◫'}</span><span class="result-text"><strong>${escapeHTML(item.label)}</strong><small>${escapeHTML(item.group||section)}</small></span><span class="result-arrow">›</span></button><button class="tune-star ${S.tuneFavorites.includes(item.id)?'on':''}" data-tune-star="${escapeHTML(item.id)}" aria-label="${S.tuneFavorites.includes(item.id)?'Remove favorite':'Add favorite'}">${S.tuneFavorites.includes(item.id)?'★':'☆'}</button></div>`}
function drawTsTree(){
 const root=$('#ts-tree'),catsEl=$('#tune-categories');if(!root)return;
 if(!S.tsStructure){root.innerHTML='<div class="tune-empty">ECU menu becomes available after the firmware-matched INI loads.</div>';return}
 const cats=S.tsStructure.categories;
 if(!S.tuneCategory||!cats.some(c=>c.name===S.tuneCategory))S.tuneCategory=tuneCategoryDefault();
 if(catsEl)catsEl.innerHTML=cats.map(c=>`<button data-tune-category="${escapeHTML(c.name)}" class="tune-category ${S.tuneCategory===c.name?'active':''}">${escapeHTML(c.name)}<span>${menuItems(c).length}</span></button>`).join('');
 const query=S.tuneSearch.toLowerCase().trim();let entries=[];
 if(query){for(const cat of cats)for(const item of menuItems(cat))if((item.label+' '+item.id+' '+cat.name+' '+(item.group||'')).toLowerCase().includes(query))entries.push({item,section:cat.name});}
 else {const cat=cats.find(c=>c.name===S.tuneCategory);if(cat)entries=menuItems(cat).map(item=>({item,section:cat.name}));}
 const top=[];
 if(!query){const items=S.tsStructure.itemById;
   const fav=S.tuneFavorites.map(id=>items.get(id)).filter(Boolean).slice(0,6);
   if(fav.length)top.push(`<div class="tune-list-label">FAVORITES</div>${fav.map(i=>tuneItemRow(i,i.section)).join('')}`);
   const recent=S.tuneRecent.map(id=>items.get(id)).filter(i=>i&&!fav.some(f=>f.id===i.id)).slice(0,4);
   if(recent.length)top.push(`<div class="tune-list-label">RECENTLY OPENED</div>${recent.map(i=>tuneItemRow(i,i.section)).join('')}`);
 }
 root.innerHTML=top.join('')+`<div class="tune-list-label">${query?'SEARCH RESULTS':escapeHTML(S.tuneCategory)} <span>${entries.length}</span></div>`+
   (entries.length?entries.map(({item,section})=>tuneItemRow(item,section)).join(''):'<div class="tune-empty">No matching firmware menus. Try another system or search.</div>');
}
function chooseTsItem(id){if(!S.tsStructure?.itemById.has(id)||!confirmLeaveEditor())return;if(GUIDE.mode==='learn')GUIDE.setMode('paused');S.commandOpen=false;S.tsSelected=id;S.route='tuning';S.editor=null;S.field=null;S.tuneBrowserOpen=false;
 S.tuneRecent=[id,...S.tuneRecent.filter(x=>x!==id)].slice(0,12);savePrefs();render();window.scrollTo(0,0)}
function commandResults(){const q=S.commandSearch.trim().toLowerCase();const routes=[['overview','Dashboard'],['guided','Guided Calibration'],['tuning','Tuning library'],['gauges','Gauge studio'],['tables','All maps'],['settings','All parameters'],['logs','Logging and files'],['system','ECU diagnostics']];
 const routeMatches=routes.filter(([key,label])=>!q||label.toLowerCase().includes(q)).map(([key,label])=>`<button data-command-route="${key}" class="command-result"><span>⌗</span><strong>${escapeHTML(label)}</strong><small>APP</small></button>`);
 const ts=S.tsStructure?.categories.flatMap(cat=>menuItems(cat).map(item=>({cat:cat.name,item})))||[];
 const items=ts.filter(({cat,item})=>!q?S.tuneFavorites.includes(item.id):(`${cat} ${item.label} ${item.id}`).toLowerCase().includes(q)).slice(0,50);
 return `<div class="command-heading">${q?'MATCHING SECTIONS':'WORKSPACES'}</div>${routeMatches.join('')}<div class="command-heading">${q?'FIRMWARE TUNING SCREENS':'FAVORITE TUNING SCREENS'} <span>${items.length}</span></div>${items.map(({cat,item})=>`<button data-command-item="${escapeHTML(item.id)}" class="command-result"><span>▦</span><strong>${escapeHTML(item.label)}</strong><small>${escapeHTML(cat)}</small></button>`).join('')||'<div class="command-empty">Search the full firmware menu by name, category, or editor identifier.</div>'}`;
}
function commandPalette(){return `<div class="command-backdrop" id="command-backdrop"><section class="command-panel" role="dialog" aria-modal="true" aria-label="Find any BrickOS screen"><div class="command-top"><span>⌕</span><input type="search" id="command-search" class="search" autocomplete="off" placeholder="Find VE table, ignition, logging, gauges…" value="${escapeHTML(S.commandSearch)}"><button data-action="close-command">ESC</button></div><div class="command-results" id="command-results">${commandResults()}</div><div class="command-foot">Search every menu from the connected ECU's own INI. Shortcut: Ctrl / Cmd + K</div></section></div>`}
function showCommand(){S.commandOpen=true;S.commandSearch='';render();$('#command-search')?.focus()}
function tuning(){const st=S.tsStructure;
 return `<div class="app-page-head"><div><span class="app-section">ECU CONFIGURATION / FIRMWARE DEFINED</span><h1>Tuning</h1><p>Browse the actual menus from this ECU's generated rusEFI INI.</p></div><div class="app-head-actions"><button class="btn ghost" data-route="tables">All maps</button><button class="btn ghost" data-route="settings">All parameters</button></div></div>`+
 (staged().length&&!S.editor?notice(`<strong>${staged().length} unburned tuning changes</strong> — these are staged only, and will not be saved to the ECU until you open a setting and tap Burn changes.`, 'warn'):'')+
 `<div class="tune-shell ${S.tuneBrowserOpen?'browse-open':''}"><aside class="tune-browser" aria-label="TunerStudio ECU menu"><div class="tune-browser-title"><strong>ECU menu</strong><small>${st?.itemById.size||0} screens</small></div><label class="tune-search-label" for="tune-search">Find a tuning screen</label><input type="search" id="tune-search" class="search" placeholder="Search ignition, VE, injector…" value="${escapeHTML(S.tuneSearch)}" autocomplete="off"><div id="tune-categories" class="tune-categories" aria-label="Firmware categories"></div><div id="ts-tree" class="ts-tree"></div></aside>`+
 `<section class="tune-content"><div class="tune-content-bar"><button class="btn ghost browse-button" data-action="tune-browse">${S.tuneBrowserOpen?'Close menu':'Browse ECU menu'}</button><span>${S.tsSelected?escapeHTML(st?.itemById.get(S.tsSelected)?.section||'Tuning'):'SELECT A SCREEN'}</span><button class="btn ghost" data-route="overview">Dashboard</button></div>${S.tsSelected?tuner():`<div class="tune-start"><div class="tune-start-mark">▦</div><h2>Your ECU, organized.</h2><p>Choose a firmware menu on the left. Maps and setting screens open here, with all addresses and scaling coming from the verified INI.</p><div class="tune-start-tiles">${(st?.categories||[]).slice(0,8).map(cat=>`<button data-tune-category="${escapeHTML(cat.name)}">${escapeHTML(cat.name)} <span>${menuItems(cat).length} screens ›</span></button>`).join('')}</div></div>`}</section></div>`;
}
function tuner(){const st=S.tsStructure,item=st?.itemById.get(S.tsSelected);
 if(S.editor)return editorView();
 if(!item)return pageHeader('TUNE','Tuning','Select Fuel, Ignition, Setup or another item from the firmware menu')+statusMessage()+`<div class="welcome-tuner"><h2>ECU tuning workspace</h2><p>The left menu comes directly from this ECU’s generated rusEFI definition. Choose an item to open its calibration screen.</p>${button('load-ini','Reload ECU definition','ghost')}</div>`;
 const res=st.resolve(item.id);const fields=res.fields.map(f=>({...f,cal:(S.definition?.cal||[]).find(c=>c.name===f.name)})).filter(f=>f.cal);
 const tableEntries=res.tables.map(t=>({...t,cal:(S.definition?.cal||[]).find(c=>c.name===t.z)}));
 const desc=item.section+(item.group?' / '+item.group:'');
 return pageHeader('TUNE',item.label,desc,button('ping','ECU status','ghost'))+statusMessage()+`<div class="ts-breadcrumb">${escapeHTML(desc)} <b>›</b> ${escapeHTML(item.label)}</div>`+
 (tableEntries.length?`<div class="ts-dialog-panel"><div class="ts-dialog-heading">CALIBRATION TABLES <span>${tableEntries.length}</span></div><div class="ts-map-cards">${tableEntries.map(t=>`<button class="ts-map-card" ${t.cal?`data-load-field="${escapeHTML(t.cal.name)}" data-field-page="${t.cal.page}" data-table-id="${escapeHTML(t.id)}"`: 'disabled'}><span class="ts-map-icon">▦</span><span><strong>${escapeHTML(t.name)}</strong><small>${t.cal?`${t.cal.width} × ${t.cal.height} · ${escapeHTML(t.cal.units)}`:'Field unavailable in current INI'}</small><small>${escapeHTML(t.x||'')} / ${escapeHTML(t.y||'')}</small></span><b>Open ›</b></button>`).join('')}</div></div>`:'')+
 (fields.length?`<div class="ts-dialog-panel"><div class="ts-dialog-heading">SETTINGS <span>${fields.length}</span></div><div class="ts-settings-grid">${fields.map(f=>`<button class="ts-setting" data-load-field="${escapeHTML(f.cal.name)}" data-field-page="${f.cal.page}"><span class="ts-setting-name">${escapeHTML(f.label||cleanLabel(f.cal.name))}</span><span class="ts-setting-detail">${escapeHTML(f.cal.name)} <b>›</b></span></button>`).join('')}</div></div>`:'')+
 (!tableEntries.length&&!fields.length?`<div class="ts-dialog-panel"><h2>${escapeHTML(item.label)}</h2><p class="muted">This INI menu item references a dialog or editor that BrickOS cannot resolve to an ECU field yet. No unrelated calibration values are substituted.</p><p class="tiny">Dialog reference: ${escapeHTML(item.id)}</p></div>`:'');
}

function markHeader(){
 const chip=$('#connection-led');chip.classList.toggle('good',connected());chip.querySelector('span').textContent=connected()?'ECU CONNECTED':'NO ECU';
 $('#signature').textContent=S.ecuSignature||'Waiting for rusEFI';
 const side=$('#side-definition');if(side)side.textContent=hasDef()?`${S.definition.cal.length} CALIBRATION FIELDS`:'LOAD AN ECU DEFINITION';
 if(hasDef()&&!S.tsStructure)rebuildTs();navHighlight();
 const tuneButton=$('#nav [data-route="tuning"]');if(tuneButton){const pending=staged().length;tuneButton.classList.toggle('has-staged',pending>0);tuneButton.dataset.pending=String(pending);tuneButton.title=pending?`${pending} unsaved tuning changes — Burn to save`:'ECU tuning';}
}
function navTo(route){if(!confirmLeaveEditor())return false;if(S.route==='guided'&&route!=='guided'){SETH.stop();if(GUIDE.mode==='learn')GUIDE.setMode('paused');}S.commandOpen=false;S.route=route;S.search='';S.editor=null;S.field=null;
 if(route==='tuning'){S.tuneBrowserOpen=true;}else if(route!=='tables'&&route!=='settings')S.tsSelected='';
 render();window.scrollTo({top:0,behavior:'instant'});return true;
}
function statusMessage(){
  if(!S.status)return notice('The ESP32 web server has not responded. The interface can be previewed, but no ECU data is available.','warn');
  if(!connected())return notice('No verified rusEFI USB connection. Check USB host enumeration and the STM32 CDC data lines. BrickOS queries the ECU directly; no TCP or CAN tuning is used.','warn');
  if(!hasDef())return notice('ECU connected. '+escapeHTML(S.iniResolveState)+'. Exact INI: '+(S.iniExpectedUrl?`<a href="${escapeHTML(S.iniExpectedUrl)}" target="_blank" rel="noopener noreferrer">Open official firmware-matched INI</a>`:'No official archive URL available')+'.');
  if(S.signatureMismatch)return notice(`ECU signature (${escapeHTML(S.ecuSignature)}) does not match the loaded .ini signature (${escapeHTML(S.definition.signature)}). Calibration writes are blocked.`,'error');
  return notice(`Connected to <strong>${escapeHTML(S.ecuSignature)}</strong>. Field addresses and scaling come from the imported definition.`);
}

// Dashboard instruments are user-configurable views of validated INI output channels.
// A display scale is never used for ECU calibration writes or safety limits.
function recommendedRange(f){const n=f.name.toLowerCase();if(/rpm/.test(n))return [0,8000];if(/lambda/.test(n))return [0.6,1.6];if(/afr/.test(n))return [8,22];if(/cool|clt|intake|iat|temp/.test(n))return [-40,150];if(/vbatt|battery/.test(n))return [8,18];if(/map|boost|press/.test(n))return [0,300];return [0,100]}
const GAUGE_COLORS=['#50e0c4','#f2b366','#83a9f7','#b49bff','#e67f9d','#88db9b'];
function defaultGauges(){return mainFields().slice(0,6).map((f,i)=>{const [min,max]=recommendedRange(f);return {name:f.name,kind:'dial',min,max,color:GAUGE_COLORS[i%GAUGE_COLORS.length]}})}
function activeGauges(){return S.gaugeSlots.length?S.gaugeSlots:defaultGauges()}
function ensureGaugeSlots(){if(!S.gaugeSlots.length)S.gaugeSlots=defaultGauges();}
function gaugeInfo(g){const f=visibleChannels().find(c=>c.name===g.name);if(!f)return null;const [dmin,dmax]=recommendedRange(f);const min=Number.isFinite(Number(g.min))?Number(g.min):dmin,max=Number.isFinite(Number(g.max))?Number(g.max):dmax;return {field:f,min,max:max>min?max:min+1}}
function ensureLiveCoverage(field){if(field&&field.offset+(TYPES[field.type]||4)>768&&S.liveMode==='fast'){S.liveMode='full';S.liveSeq=0;toast('Switched to All channels for '+channelTitle(field.name));}}
function gaugeValuePercent(value,min,max){return value==null||!Number.isFinite(value)?0:Math.max(0,Math.min(1,(value-min)/(max-min)))}
function instrument(g,index){const info=gaugeInfo(g);if(!info)return '';
 const f=info.field,value=getLatest(f),fraction=gaugeValuePercent(value,info.min,info.max);
 const kind=['dial','bar','digital'].includes(g.kind)?g.kind:'dial',color=/^#[0-9a-fA-F]{6}$/.test(g.color||'')?g.color:GAUGE_COLORS[index%6];
 const dial=`<svg class="dial-svg" viewBox="0 0 200 125" aria-hidden="true"><path d="M 18 99 A 82 82 0 0 1 182 99" fill="none" stroke="#2a4053" stroke-width="10" stroke-linecap="round"/><path class="dial-progress" d="M 18 99 A 82 82 0 0 1 182 99" fill="none" stroke="${color}" stroke-width="10" stroke-linecap="round" stroke-dasharray="${(fraction*257.61).toFixed(2)} 258"/><g class="dial-pointer" transform="rotate(${(-90+180*fraction).toFixed(2)} 100 99)"><path d="M 100 104 L 98 35 Q 100 26 102 35 L 102 104 Z" fill="#eaf3fb"/></g><circle cx="100" cy="99" r="7" fill="${color}"/></svg>`;
 return `<article class="instrument instrument-${kind}" data-instrument="${escapeHTML(f.name)}" data-gauge-index="${index}" style="--instrument-accent:${color}"><div class="instrument-title"><span class="instrument-indicator"></span><span>${escapeHTML(channelTitle(f.name))}</span><button data-gauge-edit="${index}" aria-label="Customize ${escapeHTML(channelTitle(f.name))}" title="Customize gauge">⚙</button></div>${kind==='dial'?dial:''}<div class="instrument-reading">${format(value)}<small>${escapeHTML(f.units)}</small></div>${kind==='bar'?`<div class="instrument-bar"><i style="width:${(fraction*100).toFixed(1)}%"></i></div>`:''}${kind==='digital'?'<div class="instrument-digital-label">LIVE ECU CHANNEL</div>':''}<div class="instrument-foot"><span>${format(info.min)}</span><span>${format(info.max)}</span></div></article>`}
function cockpitToolbar(){return `<div class="cockpit-control"><span>DISPLAY</span><select id="dashboard-layout" class="selector" aria-label="Dashboard layout"><option value="split" ${S.dashboardLayout==='split'?'selected':''}>Gauges + scope</option><option value="gauges" ${S.dashboardLayout==='gauges'?'selected':''}>Gauge focus</option><option value="scope" ${S.dashboardLayout==='scope'?'selected':''}>Scope focus</option></select>${button('add-gauge','+ Add gauge','ghost')}${button('open-gauge-customizer','Customize','ghost')}</div>`}
function instrumentGrid(){return `<div class="instrument-grid">${activeGauges().map(instrument).join('')||'<p class="muted">No channels found in your INI.</p>'}</div>`}
function gaugeEditor(){if(S.gaugeEdit===null)return '';
 const slot=activeGauges()[S.gaugeEdit];if(!slot)return '';
 const channels=visibleChannels(),[defaultMin,defaultMax]=recommendedRange(channels.find(c=>c.name===slot.name)||{name:''});
 return `<div class="modal gauge-modal" role="presentation"><section class="dialog gauge-dialog" role="dialog" aria-modal="true" aria-labelledby="gauge-editor-title"><div class="dialog-head"><div><span class="app-section">COCKPIT CUSTOMIZATION</span><h2 id="gauge-editor-title">Configure gauge</h2></div><button class="btn ghost" data-action="gauge-cancel">Close</button></div><div class="gauge-form"><label>Live ECU channel<select class="selector" id="gauge-field">${channels.map(c=>`<option value="${escapeHTML(c.name)}" ${c.name===slot.name?'selected':''}>${escapeHTML(channelTitle(c.name))} (${escapeHTML(c.units)})</option>`).join('')}</select></label><label>Gauge style<select class="selector" id="gauge-kind">${[['dial','Analog dial'],['bar','Progress bar'],['digital','Digital readout']].map(([v,l])=>`<option value="${v}" ${slot.kind===v?'selected':''}>${l}</option>`).join('')}</select></label><div class="gauge-form-row"><label>Display minimum<input type="number" step="any" class="field" id="gauge-min" value="${Number.isFinite(Number(slot.min))?Number(slot.min):defaultMin}"></label><label>Display maximum<input type="number" step="any" class="field" id="gauge-max" value="${Number.isFinite(Number(slot.max))?Number(slot.max):defaultMax}"></label></div><label>Gauge accent<input type="color" id="gauge-color" value="${/^#[0-9a-fA-F]{6}$/.test(slot.color||'')?slot.color:'#50e0c4'}"></label></div><div class="btnrow spaced">${button('gauge-save','Save gauge','primary')}${button('gauge-move-prev','Move earlier','ghost',S.gaugeEdit===0?'disabled':'')}${button('gauge-move-next','Move later','ghost',S.gaugeEdit>=activeGauges().length-1?'disabled':'')}${button('gauge-remove','Remove gauge','danger')}</div><p class="hint spaced">Display settings are saved on this device only. ECU calibration and warnings are not changed.</p></section></div>`;
}
function gauge(field,value){
  let clamped=value==null?0:Math.max(0,Math.min(100,Math.abs(value)/(field.name.toLowerCase().includes('rpm')?8000:100)*100));
  return `<div class="gauge"><div class="label">${escapeHTML(channelTitle(field.name))}</div><div class="reading" data-gauge="${escapeHTML(field.name)}">${format(value)} <small class="unit">${escapeHTML(field.units)}</small></div><div class="bar"><b style="width:${clamped.toFixed(1)}%"></b></div></div>`;
}
function visibleChannels(){return (S.definition?.channels||[]).filter(c=>c.kind==='scalar' && c.scale!==null && c.translate!==null)}
function metric(name){const c=visibleChannels().find(f=>f.name.toLowerCase()===name.toLowerCase());return c?getLatest(c):null}
function getLatest(field){if(!S.latest)return null;return getValue(S.latest,field,0,S.definition.endianness)}
function mainFields(){
 // Prefer the real generated rusEFI output field identifiers; aliases come
 // from the INI name list, never from invented byte offsets.
 const priority=[['RPMValue','rpm'],['MAPValue','map'],['TPSValue','tps'],
  ['coolant','clt'],['intake','iat'],['VBatt','batteryVoltage'],
  ['lambdaValue','lambda'],['AFRValue','afr'],['lowFuelPressure','fuelPressure'],
  ['oilPressure'],['vehicleSpeedKph','vss']];
 const fields=visibleChannels(),found=[];
 for(const names of priority){const f=names.map(name=>fields.find(x=>x.name.toLowerCase()===name.toLowerCase())).find(Boolean);
  if(f&&!found.includes(f))found.push(f)
 }
 if(found.length<8)for(const f of fields){if(found.length>=8)break;if(f.offset<768&&!found.includes(f))found.push(f)}
 return found.slice(0,8);
}
function livePerformance(){const z=S.status||{};
 const received=Number(z.live_actual_hz??S.liveDisplayHz);
 return `<div class="live-performance" role="status" aria-label="Measured telemetry throughput">
   <div><span>ECU sampler</span><strong data-live-actual>${received.toFixed(1)} <small>Hz actual</small></strong></div>
   <div><span>Requested</span><strong data-live-target>${S.liveHz} <small>Hz</small></strong></div>
   <div><span>USB round-trip</span><strong data-live-usb>${Number(z.live_sample_duration_us||S.liveDurationMs*1000)/1000||0} <small>ms</small></strong></div>
   <div><span>Sample age</span><strong data-live-age>${z.live_sample_age_ms??0} <small>ms</small></strong></div>
   <div><span>Payload</span><strong data-live-throughput>${(Number(z.live_sample_bytes||0)*received/1024).toFixed(1)} <small>KB/s</small></strong></div>
   </div>`;
}
function rateControl(){return `<div class="rate-control"><label for="live-mode">Channels</label><select id="live-mode" class="selector"><option value="fast" ${S.liveMode==='fast'?'selected':''}>Fast · 768 B</option><option value="full" ${S.liveMode==='full'?'selected':''}>All · ${Math.min(3072,S.definition?.outputSize||3072)} B</option></select><label for="live-rate">Target</label><select id="live-rate" class="selector">${[10,20,50,100].map(x=>`<option value="${x}" ${S.liveHz===x?'selected':''}>${x} Hz</option>`).join('')}</select><small>Measured rates may be lower, depending on USB and browser latency.</small></div>`}

function scopeFields(){const fields=visibleChannels();const picks=S.scopeNames.length?S.scopeNames:mainFields().slice(0,3).map(c=>c.name);return picks.map(n=>fields.find(f=>f.name===n)).filter(Boolean).slice(0,3)}
function scopePanel(){const fields=scopeFields();
 const choices=visibleChannels();return `<section class="workspace-pane scope-pane"><div class="pane-title"><span class="pane-dot"></span><strong>LIVE DATA SCOPE</strong><small>Real USB samples · 10-second window</small></div><canvas id="live-scope" class="live-scope" width="1050" height="300" aria-label="Scrolling telemetry time plot"></canvas><div class="scope-legend">${fields.map((f,i)=>`<span><i style="background:${['#4fe0c4','#ffbc5a','#7a9df8'][i]}"></i>${escapeHTML(channelTitle(f.name))}</span>`).join('')}</div><details class="scope-config"><summary>Configure graph channels</summary><div class="scope-pickers">${[0,1,2].map((i)=>`<label>Trace ${i+1}<select class="selector" data-scope-index="${i}"><option value="">Disabled</option>${choices.map(f=>`<option value="${escapeHTML(f.name)}" ${fields[i]?.name===f.name?'selected':''}>${escapeHTML(channelTitle(f.name))}</option>`).join('')}</select></label>`).join('')}</div></details></section>`;
}
function overview(){const f=activeGauges();return `<div class="workbench-header"><div><div class="workbench-kicker">BRICKOS / CUSTOM WORKSPACE</div><h1>Dashboard</h1><p>Real-time instruments and graphing from the connected ECU</p></div><div class="workbench-toolbar">${rateControl()}</div></div>`+
 statusMessage()+livePerformance()+cockpitToolbar()+`<div class="workbench-grid cockpit-layout-${S.dashboardLayout}"><section class="workspace-pane gauge-pane"><div class="pane-title"><span class="pane-dot"></span><strong>LIVE INSTRUMENTS</strong><small>${f.length} configured · INI-scaled</small></div>${instrumentGrid()}</section>`+scopePanel()+`</div>`+
 `<div class="workspace-lower"><section class="workspace-pane"><div class="pane-title"><span class="pane-dot"></span><strong>QUICK ACCESS</strong><small>Your ECU menus</small></div><div class="quick-menu"><button data-route="tuning">Open tuning <b>›</b></button><button data-route="gauges">Customize gauges <b>›</b></button>${(S.tsStructure?.categories||[]).slice(0,6).map(cat=>`<button data-tune-category="${escapeHTML(cat.name)}">${escapeHTML(cat.name)} <b>›</b></button>`).join('')}</div></section>`+
 `<section class="workspace-pane"><div class="pane-title"><span class="pane-dot"></span><strong>ECU HEALTH</strong><small>Connection diagnostics</small></div>${dataRow('Transport',connected()?'USB CDC / CONNECTED':'OFFLINE')}${dataRow('Definition',safeSignature()?'FIRMWARE MATCHED':'UNVERIFIED')}${dataRow('USB requests',String(S.status?.ok_requests??0))}${dataRow('USB errors',String(S.status?.failed_requests??0))}${dataRow('Live sample failures',String(S.status?.live_samples_failed??0))}<div class="btnrow spaced"><button class="btn ghost" data-route="system">Full diagnostics ›</button></div></section></div>`+gaugeEditor();
}
function gauges(){const filtered=visibleChannels().filter(c=>(channelTitle(c.name)+' '+c.name).toLowerCase().includes(S.search.toLowerCase()));
 return pageHeader('COCKPIT','Gauge studio','Build your own dashboard from live, firmware-defined output channels',button('add-gauge','Add instrument','primary')+button('reset-gauges','Reset gauges','ghost'))+statusMessage()+
 `<section class="workspace-pane"><div class="pane-title"><span class="pane-dot"></span><strong>YOUR INSTRUMENTS</strong><small>Tap the settings icon to change a gauge · saved on this device</small></div>${instrumentGrid()}</section>`+
 `<section class="workspace-pane spaced"><div class="pane-title"><strong>AVAILABLE ECU CHANNELS</strong><small>${visibleChannels().length} total</small></div><input class="search" id="channel-search" placeholder="Search any live ECU channel…" value="${escapeHTML(S.search)}" aria-label="Search live channels"><div class="channel-list">${filtered.slice(0,55).map(f=>`<button data-add-channel="${escapeHTML(f.name)}"><span><strong>${escapeHTML(channelTitle(f.name))}</strong><small>${escapeHTML(f.name)} · ${escapeHTML(f.units)}</small></span><b>+ Add</b></button>`).join('')||'<p class="muted">No matching channels.</p>'}</div><p class="hint">${filtered.length>55?'Showing 55 matches. Refine the search for more.':'Each channel uses the live ECU INI offset and scaling.'}</p></section>`+gaugeEditor();
}
// Presentation-only grouping: fields remain sourced exclusively from the live ECU INI.
// Match specific terms before generic terms so an ignition timing field is not filed as a sensor.
const WORKSPACE_GROUPS={
 'Fuel': /fuel|inject|veTable|lambda|afr|mixture|stoich|wallWetting|crank.*enrich|accel.*enrich|wue|airmass/i,
 'Ignition': /ignit|spark|dwell|timing|knock|coil|multispark/i,
 'Idle': /idle|stepper|iac|acIdle/i,
 'Boost': /boost|wastegate|antiLag|launch|turbo/i,
 'Throttle': /etb|dbw|pedal|tps|throttle/i,
 'Sensors': /sensor|adc|thermistor|coolant|clt|iat|map_|maf|baro|pressure|voltage|vbatt|oxygen|wideband|analog/i,
 'Outputs': /output|pin|solenoid|relay|fan|pump|pwm|highside|lowside|vvt|alternator/i,
 'CAN & Connectivity': /can|lin|serial|usb|baud|wifi|broadcast|obd|network/i,
 'Engine': /engine|trigger|crank|cam|firing|cylinder|displacement|rpm|vehicle|speed|gear|limiter/i,
 'Advanced': /lua|script|debug|diagnostic|error|verbose|logging|sdCard|sd_|memory/i
};
const WORKSPACE_CATEGORIES=['All',...Object.keys(WORKSPACE_GROUPS),'Other'];
function fieldCategory(f){const name=String(f.name||'');for(const [category,pattern] of Object.entries(WORKSPACE_GROUPS)){if(pattern.test(name))return category}return 'Other'}
function workspaceFilters(category,prop,items){return `<div class="workspace-filters" aria-label="Filter by tuning system">${WORKSPACE_CATEGORIES.map(cat=>{const count=cat==='All'?items.length:items.filter(f=>fieldCategory(f)===cat).length;return `<button type="button" class="workspace-filter ${category===cat?'active':''}" data-workspace-filter="${prop}" data-category="${escapeHTML(cat)}" aria-pressed="${category===cat?'true':'false'}">${escapeHTML(cat)} <span>${count}</span></button>`}).join('')}</div>`}
function fieldGroupLabel(f){return `<span class="system-tag">${escapeHTML(fieldCategory(f))}</span>`}
function arrays(){return (S.definition?.cal||[]).filter(c=>c.kind==='array'&&c.height>1&&c.byteLength<=4096)}
function tables(){const all=arrays();const sel=all.filter(x=>(S.tableCategory==='All'||fieldCategory(x)===S.tableCategory)&&x.name.toLowerCase().includes(S.tableSearch.toLowerCase()));
 if(S.editor)return editorView();
 return pageHeader('03','Calibration maps','Firmware-defined tuning maps · organized by engine system',button('load-ini','Load ECU .ini','ghost'))+
 statusMessage()+`<div class="panel"><div class="panel-header"><h2>Map library</h2><span class="pill">${all.length} maps · ${sel.length} shown</span></div>
 ${workspaceFilters(S.tableCategory,'tables',all)}
 <div class="workspace-tools"><input id="table-search" class="search" placeholder="Search VE, ignition, lambda, corrections…" value="${escapeHTML(S.tableSearch)}" aria-label="Search calibration maps"><span class="tiny">${sel.length} matching maps</span></div>
 <div class="field-list">${sel.slice(0,180).map(x=>`<button class="field-item" data-load-field="${escapeHTML(x.name)}" data-field-page="${x.page}"><span><strong>${escapeHTML(cleanLabel(x.name))}</strong><small>PAGE ${x.page} · ${x.width} × ${x.height} · ${x.type} · OFFSET ${x.offset}</small></span>${fieldGroupLabel(x)}</button>`).join('')||'<p class="muted">No matching maps. Try All systems or a different search.</p>'}</div>
 ${sel.length>180?'<p class="hint spaced">Showing the first 180 matches. Refine the search for more.</p>':''}</div>`;
}
function allSettings(){const fields=(S.definition?.cal||[]).filter(x=>x.kind==='scalar'||x.kind==='bits'||(x.kind==='array'&&x.height===1));
 const shown=fields.filter(x=>(S.settingsCategory==='All'||fieldCategory(x)===S.settingsCategory)&&x.name.toLowerCase().includes(S.search.toLowerCase()));
 if(S.editor)return editorView();
 return pageHeader('04','ECU settings','Firmware-defined settings · grouped by engine system',button('load-ini','Load definition','ghost'))+
 statusMessage()+`<div class="panel"><div class="panel-header"><h2>Parameter library</h2><span class="pill">${fields.length} fields · ${shown.length} shown</span></div>
 ${workspaceFilters(S.settingsCategory,'settings',fields)}
 <div class="workspace-tools"><input class="search" id="setting-search" placeholder="Search idle, boost, fuel, sensor, CAN, engine…" value="${escapeHTML(S.search)}" aria-label="Search ECU settings"><span class="tiny">${shown.length} matching parameters</span></div>
 <div class="field-list">${shown.slice(0,180).map(f=>`<button class="field-item" data-load-field="${escapeHTML(f.name)}" data-field-page="${f.page}"><span><strong>${escapeHTML(cleanLabel(f.name))}</strong><small>PAGE ${f.page} · ${f.type} · OFFSET ${f.offset} · ${f.editable?'DECODED':'READ ONLY'}</small></span>${fieldGroupLabel(f)}</button>`).join('')||'<p class="muted">No matching settings. Try All systems or a different search.</p>'}</div>
 ${shown.length>180?'<p class="hint spaced">Showing the first 180 matches. Refine the search for more.</p>':''}</div>`;
}
function changed(){return S.editor&&S.editor.current.some((b,i)=>b!==S.editor.original[i])}
function effectiveField(f){return f.kind==='array'&&(f.scale===null||f.translate===null)?{...f,scale:1,translate:0,editable:true,units:'raw'}:f}
function entryValue(e,i){return getValue(e.current,relative(effectiveField(e.field),e.start),i,S.definition.endianness)}
function activeEdit(){return S.axisSelection?S.editorAxes?.[S.axisSelection.axis]:S.editor}
function selectedIndexes(){return S.axisSelection?[S.axisSelection.index]:S.selection}
function stageSelected(fn){return stage(selectedIndexes(),fn,activeEdit())}
function editorValue(index){return entryValue(S.editor,index)}
function editorView(){
 const e=S.editor,f=e.field,table=f.height>1,curve=f.kind==='array'&&f.width>1&&f.height===1,grid=table||curve;
 const x=S.editorAxes?.x,y=S.editorAxes?.y,selected=S.axisSelection,dirty=staged();
 const pageDirty=dirty.filter(d=>d.field.page===f.page);
 const isDirtyField=v=>v&&v.current.some((b,i)=>b!==v.original[i]);
 let content='';
 if(grid){
   const vals=Array.from({length:f.width*f.height},(_,i)=>editorValue(i));
   const numeric=vals.filter(Number.isFinite),lo=Math.min(...numeric),hi=Math.max(...numeric);
   let html=`<div class="table-scroll"><table class="tune-grid axis-grid"><thead><tr><th class="axis-corner">${y?'LOAD / RPM':'ROW / COL'}</th>`+
    Array.from({length:f.width},(_,i)=>x?
      `<th class="axis-heading ${selected?.axis==='x'&&selected.index===i?'axis-selected':''}"><button data-axis-cell="x" data-axis-index="${i}" title="Edit X axis">${format(entryValue(x,i))}</button></th>`:
      `<th class="axis-heading">${i+1}</th>`).join('')+'</tr></thead><tbody>';
   for(let r=0;r<f.height;r++){
     html+=`<tr><th class="axis-heading ${selected?.axis==='y'&&selected.index===r?'axis-selected':''}">${y?`<button data-axis-cell="y" data-axis-index="${r}" title="Edit Y axis">${format(entryValue(y,r))}</button>`:r+1}</th>`;
     for(let c=0;c<f.width;c++){
       const index=r*f.width+c,v=vals[index],level=hi===lo?0.5:(v-lo)/(hi-lo),bg=level>.7?'hot':level>.3?'mid':'cool',sel=!selected&&S.selection.includes(index)?'sel':'';
       html+=`<td class="${bg} ${sel}"><button data-cell="${index}" title="Click to select, Shift-click for range, double-click to edit">${format(v)}</button></td>`;
     }html+='</tr>';
   }
   content=html+'</tbody></table></div>'+`<p class="hint">${x||y?'Grey headings are editable axis cells.':'No axis arrays resolved from this firmware definition.'} Click a cell then use Set selected, +/−, arrows or Enter. Double-click to enter a value.${[x,y].some(a=>a&&(a.field.scale===null||a.field.translate===null))?' Conditional axis conversion could not be resolved; affected axes display and edit raw ECU units.':''}</p>`+
     (table?'<div class="section-label">3D SURFACE PREVIEW</div><canvas id="map-surface" class="tuning-plot" width="900" height="260"></canvas>':'<div class="section-label">CURVE PREVIEW</div><canvas id="curve-chart" class="tuning-plot" width="900" height="235"></canvas>');
 }else{
   const val=editorValue(0);
   const selector=f.kind==='bits'&&f.options?.length>1&&f.options.length<257
     ?`<select id="scalar-value" class="field" style="width:100%" ${f.editable?'':'disabled'}>${f.options.map((name,i)=>`<option value="${i}" ${i===val?'selected':''}>${i} — ${escapeHTML(name)}</option>`).join('')}</select>`
     :`<input id="scalar-value" class="field" type="number" step="any" value="${Number.isFinite(val)?val:''}" style="width:100%" ${f.editable?'':'disabled'}>`;
   content=`<div class="panel"><div class="row"><div style="flex:1;min-width:140px"><h3>Current value</h3><div class="number">${format(val)} <span class="unit">${escapeHTML(f.units)}</span></div></div><div style="flex:2;min-width:180px"><label for="scalar-value">New value (staged, not sent)</label>${selector}</div></div></div>`;
 }
 return pageHeader('TUNE','Editing / '+cleanLabel(f.name),`ECU page ${f.page} · offset ${f.offset} · ${f.type} · ${f.width}×${f.height}`,
  button('close-editor','Back','ghost')+button('backup','Backup & download','ghost')+button('download-saved-backup','Download saved backup','ghost')+button('export-patch','Save patch','ghost')+button('import-patch','Load patch','ghost'))+
 (S.signatureMismatch?notice('Definition signature mismatch. ECU writes are blocked.','error'):'')+
 `<div class="grid g2"><div class="panel"><div class="panel-header"><h2>${escapeHTML(f.name)}</h2><div class="btnrow"><span class="pill">${f.type}</span><span class="pill ${pageDirty.length?'orange':'green'}">${pageDirty.length?`${pageDirty.length} STAGED FIELDS`:'ECU MATCHED'}</span></div></div>${content}<div class="toolbar spaced">${grid?
  `${button('inc01','+0.1','ghost')}${button('dec01','−0.1','ghost')}${button('inc1','+1','ghost')}${button('dec1','−1','ghost')}${button('inc5','+5','ghost')}${button('dec5','−5','ghost')}${button('smooth','Smooth','ghost')}${button('edit-selected','Set selected','ghost')}`:
  `<span class="scalar-staging-hint">Changes stage as you type · Burn to save</span>`}
 ${button('undo','Undo','ghost',S.undo.length?'':'disabled')}${button('redo','Redo','ghost',S.redo.length?'':'disabled')}</div></div>
 <div class="panel"><h2>Calibration session</h2><p class="note">Edit values here, then tap Burn changes once. BrickOS backs up, writes and verifies automatically.</p>
 <div class="rule"></div>${dataRow('Pending fields',String(dirty.length))}${dataRow('Pending bytes',String(modifiedBytes()))}${dataRow('Current page',`${pageDirty.length} edited fields`)}${dataRow('Page backup',savedBackup(f.page)?'Saved on ESP32 · downloadable':'Saved automatically on ESP32 before write',savedBackup(f.page)?'good-text':'warn-text')}${dataRow('Page persistence',S.definition.pageBurnable?.[f.page-1]?'FLASH BURN SUPPORTED':'RAM / NO BURN COMMAND')}${dataRow('Write access',S.status?.write_access_ready?'ENABLED — BURN TO SAVE':S.status?'ESP32 FIRMWARE REPORTS DISABLED':'WAITING FOR ESP32 STATUS',S.status?.write_access_ready?'good-text':'bad-text')}${dataRow('Authorization',S.status?.write_access_ready?'Automatic during Burn':'Not enabled')}${dataRow('BrickOS firmware',S.status?.brick_os_build||'Version unknown — check actual ESP32 flash')}
 ${S.lastWriteError?notice('Previous burn attempt stopped at '+escapeHTML(S.lastWritePhase)+': '+escapeHTML(S.lastWriteError),'error'):''}<div class="rule"></div><div class="btnrow">${button('discard-page','Discard page edits','danger',pageDirty.length?'':'disabled')}</div><p class="hint spaced">Burn automatically checks stopped RPM, saves a full ECU backup on the ESP32, verifies write bytes, performs one flash command per changed burnable page and confirms readback. No separate write or authorization buttons.</p></div></div><div class="burn-dock"><div class="burn-dock-copy"><strong>${dirty.length?`${dirty.length} staged field${dirty.length!==1?'s':''} · ${modifiedBytes()} byte${modifiedBytes()!==1?'s':''}`:'All changes saved to ECU'}</strong><span>${dirty.length?'NOT SAVED · Leaving without burning will not save these values.':'No pending edits'}</span></div>${button('burn',dirty.length?'Burn changes':'Saved','burn-button',dirty.length?'':'disabled')}</div>`;
}

function logs(){const files=S.logs||[];const read=hasDef()?Math.min(256,Math.max(32,S.definition.outputSize||256)):128;
 const info=S.status;
 return pageHeader('05','Logging & files','Capture directly on the ESP32; download logs over Wi-Fi',button('reload-logs','Refresh files','ghost'))+
 notice('The ESP32 logger polls rusEFI output channels over USB and writes them to ESP32-attached storage. This is independent of the onboard SD NAND. Internal SPIFFS space is small; configure ESP32-attached microSD in firmware for extended logging.')+
 `<div class="grid g3">${stat('Recording',info?.logging?'ACTIVE':'IDLE','',info?.log_name||'No active session')}${stat('Samples saved',info?.samples??0,'records',`Measured ${Number(info?.log_actual_hz||0).toFixed(1)} Hz · ${info?.log_failures||0} failed polls`)}${stat('Storage mount',info?.storage??'—','','Logging state: '+(info?.storage_error||'unknown'))}</div>
 <div class="section-label">RECORDING CONTROL</div><div class="panel"><div class="row"><div><label>Output bytes per sample</label><input id="log-size" class="field" type="number" min="1" max="512" value="${read}"></div><div><label>Sampling rate</label><select id="log-interval" class="field"><option value="500">2 Hz</option><option value="200">5 Hz</option><option selected value="100">10 Hz</option><option value="50">20 Hz</option></select></div><div class="btnrow" style="align-self:end">${button('start-log','Start onboard log','primary',info?.logging?'disabled':'')}${button('stop-log','Stop recording','danger',info?.logging?'':'disabled')}</div></div><p class="hint spaced">Raw <code>.blg</code> files contain timestamps and real rusEFI output bytes. CSV conversion uses the loaded .ini. Recording shares the USB session with live tuning; test access on the bench.</p></div>
 <div class="section-label">AUTONOMOUS STARTUP RECORDING</div>
 <div class="panel"><div class="row" style="align-items:center;justify-content:space-between"><div><h2>Record without a computer or browser</h2><p class="tiny">Keeps the recording configuration in ESP32 nonvolatile memory. A new file starts automatically after boot, including if nobody opens BrickOS.</p></div>
 <label><input id="auto-log" type="checkbox" ${info?.auto_log?'checked':''}> Start recording at boot</label></div>
 <div class="btnrow spaced">${button('save-auto-log','Save automatic recording settings','ghost')}</div>
 <p class="hint spaced">Use the offset, byte length and sample rate entered above. An ESP32-attached SD card is recommended for long recordings. Auto-recording is disabled unless explicitly saved.</p></div>
 <div class="section-label">ESP32 FILE ARCHIVE</div><div class="panel"><div class="panel-header"><h2>Stored logs</h2><span class="pill">${files.length} visible</span></div>${files.map(f=>`<div class="file-row"><span><strong>${escapeHTML(f.name)}</strong><small class="muted"> · ${(f.size/1024).toFixed(1)} KB</small></span><button class="btn ghost" data-file="${escapeHTML(f.name)}" data-file-action="view">Analyze</button><div class="btnrow"><button class="btn ghost" data-file="${escapeHTML(f.name)}" data-file-action="download">Download</button><button class="btn danger" data-file="${escapeHTML(f.name)}" data-file-action="delete">Delete</button></div></div>`).join('')||'<p class="muted">No logs available yet. Start an onboard capture while the ECU is connected.</p>'}</div>
 <div class="btnrow spaced">${button('import-log','Analyze local .blg file','ghost')}</div>${S.viewer?logViewer():''}`;
}
function logViewer(){const d=S.viewer,data=d.records||[];const channels=visibleChannels().filter(f=>f.offset>=d.offset&&f.offset+TYPES[f.type]<=d.offset+d.size);
 return `<div class="section-label">LOG ANALYZER</div><div class="panel"><div class="panel-header"><h2>Binary log viewer</h2>${button('export-csv','Export CSV','primary')}</div><p class="tiny">${data.length} records · offset ${d.offset} · ${d.size} bytes each ${d.truncated?'· Last record truncated':''}</p><div class="row spaced"><label>Plot channel</label><select id="log-channel" class="selector">${channels.map(x=>`<option value="${escapeHTML(x.name)}" ${x.name===S.viewChannel?'selected':''}>${escapeHTML(x.name)}</option>`).join('')}</select></div><canvas id="log-chart" class="logplot" width="1000" height="230"></canvas></div>`;
}
function system(){const z=S.status;return pageHeader('06','Connection & diagnostics','ESP32-S3 firmware configuration and rusEFI connectivity',button('ping','Run ECU query','primary')+button('ecu-version','Firmware version','ghost')+button('ecu-errors','Check ECU errors','ghost')+button('load-ini','Import generated .ini','ghost'))+statusMessage()+
 `<div class="grid g2"><div class="panel"><div class="panel-header"><h2>ESP32 native USB tuner</h2><span class="pill ${z?'green':'orange'}">${z?'READY':'NOT READY'}</span></div>${dataRow('Transport',z?.transport||'Unavailable')}${dataRow('USB host',z?.usb_device_ready?'ECU USB CONNECTED':'WAITING FOR ECU USB')}${dataRow('Protocol','Native rusEFI TunerStudio over USB')}${dataRow('USB gateway',z?.usb_gateway||'—')}${dataRow('ECU session',z?.usb_device_ready?'USB AVAILABLE':'WAITING FOR USB')}${dataRow('Last transport error',z?.last_error||'—')}${dataRow('Storage',z?.storage||'—')}${dataRow('Firmware version',S.firmwareVersion||'Not requested')}${dataRow('Configuration errors',S.ecuErrorMessage||'Not checked')}</div>
 <div class="panel"><div class="panel-header"><h2>ECU definition</h2><span class="pill ${hasDef()?'green':'orange'}">${hasDef()?'LOADED':'MISSING'}</span></div>${dataRow('Connected ECU signature',S.ecuSignature||'—')}${dataRow('INI status',S.iniResolveState)}${dataRow('ECU SD INI import',S.status?.ecu_sd_ini_state||'Waiting for USB storage')}${dataRow('SD access attempts',String(S.status?.ecu_sd_ini_attempts??0))}${dataRow('Verified SD imports',String(S.status?.ecu_sd_ini_imports??0))}${dataRow('.ini expected signature',S.definition?.signature||'—')}${dataRow('Signature match',safeSignature()?'YES':'NO',safeSignature()?'good-text':'bad-text')}${dataRow('Calibration definitions',S.definition?.cal.length??0)}${dataRow('Output channel definitions',S.definition?.channels.length??0)}${dataRow('Write commands',z?.write_compiled?'ENABLED':z?'DISABLED BY INSTALLED ESP32 FIRMWARE':'ESP32 STATUS UNKNOWN')}${dataRow('Write authorization',z?.write_mode||'UNKNOWN')}${dataRow('Write runtime arm',z?.write_armed?'ACTIVE DURING WRITE':'Automatic (only during Burn)')}${dataRow('BrickOS firmware',z?.brick_os_build||'Older/unknown ESP32 firmware')}${button('disarm','Disarm writes','danger')}</div></div>
 <div class="section-label">ECU DEFINITION MANAGER · INTERNAL SPIFFS</div>
 <div class="panel"><div class="panel-header"><h2>Saved rusEFI definition</h2><span class="pill ${z?.ini_cached?'green':'orange'}">${z?.ini_cached?((z.ini_size/1024).toFixed(0)+' KB CACHED'):'NOT CACHED'}</span></div>
 <p class="tiny">Automatically imported from the ECU SD card over USB MSC LUN1 when available, saved on ESP32 SPIFFS, then cached on this phone for faster repeat loads. SHA-256 detects updates; live ECU signature checking remains required. Manual upload is a fallback.</p>
 <div class="spaced"><label for="ini-upload">Select rusEFI INI from this phone or computer</label><input class="field ini-file-input" id="ini-upload" type="file" accept=".ini,.txt,text/plain,application/octet-stream" aria-label="Choose rusEFI INI or text file"><p class="tiny">Select Browse in iPhone Files (not Photos). Filenames do not need to be rusefi.ini; the contents and ECU signature are checked.</p></div>
 <details class="spaced"><summary>iPhone alternative: paste INI text instead</summary><p class="tiny">Open the INI in Files or a text editor, copy its complete contents, then paste below. This bypasses iOS file-type restrictions.</p><textarea class="field ini-paste" id="ini-paste" rows="5" spellcheck="false" autocapitalize="off" autocomplete="off" placeholder="Paste full generated rusEFI INI here"></textarea><div class="btnrow spaced">${button('ini-paste-import','Import pasted INI','primary')}</div></details>
 <div class="ini-import-status spaced" id="ini-import-progress" role="status" aria-live="polite">${escapeHTML(S.iniImportProgress)}</div><div class="btnrow spaced">${button('ini-read','Load saved INI','primary')}${button('ini-save','Save current INI to ESP32','ghost')}${button('ini-delete','Delete saved INI','danger')}</div>
 <div class="rule"></div><p class="tiny">AP-only build: no internet downloads. The phone loads the ESP32's locally stored ECU SD definition, not an internet copy. Browser storage is optional; if unavailable, the phone reloads from the ESP32.</p></div>
 <div class="section-label">USB CONNECTION</div><div class="panel"><h2>Connect Big Brick over USB</h2><div class="steps spaced"><div>Connect the STM32 USB device interface to the ESP32-S3 USB host interface using the same USB wiring as your proven working3wayhub firmware. USB host VBUS must be powered appropriately.</div><div>BrickOS speaks the rusEFI TunerStudio protocol directly to STM32 over USB; no TCP bridge or desktop TunerStudio connection is required.</div><div>Join the open <code>brick</code> Wi-Fi network and open <code>http://192.168.4.1/</code>.</div><div>BrickOS now attempts read-only ECU SD access over the STM32 USB Mass Storage interface (LUN1), if exposed by the installed firmware. Place a plain generated <code>rusefi.ini</code> on the card; it imports only when its signature matches the connected ECU. The ESP32 SPIFFS stores the verified copy. Existing USB CDC tuning remains active.</div></div></div>`;
}
function storyPacket(){
 const key=safeSignature()?S.definition.signature+':'+S.guideDefinitionHash:'';
 // Story is bound to the *current matching definition*, never to a stale INI.
 const packet=STORY.packet({engine:GUIDE,menuTree:S.tsStructure,menuLinks:S.guideMenuLinks,ecuKey:key});
 S.guidePacket=packet;return packet;
}
function guided(){
 if(S.guideScreen==='story')return renderSethStory(storyPacket(),{muted:SETH.muted});
 return `<div class="seth-return"><button type="button" class="btn ghost" data-seth-return>← Back to Seth's guide</button></div>`+
 renderGuide(GUIDE,{tab:S.guideTab,search:S.guideSearch,status:S.status,menuLinks:S.guideMenuLinks});
}
function storySpeak(playVoice){
 if(S.route==='guided'&&S.guideScreen==='story'&&S.guidePacket)SETH.speak(S.guidePacket.text,S.guidePacket.voice,{playVoice,tone:S.guidePacket.tone||'normal'});
}
async function fetchStoryCatalog(){
 try{const res=await fetch('/api/guide/v1/catalog',{cache:'no-store'});if(!res.ok)throw Error('HTTP '+res.status);
 STORY.load(await res.json());GUIDE.setRoleAliases(STORY.catalog.field_role_aliases);if(S.route==='guided'){render();storySpeak(false);}
 }catch(e){console.warn('Seth guide catalogue not available',e);}
}

function refreshGuideLinks(){
 S.guideMenuLinks={};const tree=S.tsStructure;if(!tree)return;
 for(const item of tree.itemById.values()){
  const resolved=tree.resolve(item.id);
  for(const f of resolved.fields||[]){if(!S.guideMenuLinks[f.name])S.guideMenuLinks[f.name]=item.id;}
  for(const t of resolved.tables||[]){for(const name of [t.z,t.x,t.y])if(name&&!S.guideMenuLinks[name])S.guideMenuLinks[name]=item.id;}
 }
}
function render(){const views={overview,gauges,tables,settings:allSettings,logs,system,tuning,tuner,guided};page.innerHTML=(views[S.route]||overview)()+(S.commandOpen?commandPalette():'');markHeader();if(S.route==='tuning')drawTsTree();if(S.viewer)drawLogChart();drawTuneChart();if(S.route==='overview')drawLiveScope();drawBurnStatus();}

function drawTuneChart(){
 const e=S.editor;if(!e||!e.field||e.field.kind!=='array')return;
 const f=e.field,field=relative(f,e.start),count=f.width*f.height;
 const vals=Array.from({length:count},(_,i)=>getValue(e.current,field,i,S.definition.endianness));
 const finite=vals.filter(Number.isFinite);if(!finite.length)return;
 let lo=Math.min(...finite),hi=Math.max(...finite);if(lo===hi){lo--;hi++}
 const curve=$('#curve-chart');
 if(curve){
  const ctx=curve.getContext('2d'),w=curve.width,h=curve.height;
  ctx.clearRect(0,0,w,h);ctx.fillStyle='#0c1414';ctx.fillRect(0,0,w,h);
  ctx.strokeStyle='#324d43';ctx.lineWidth=1;
  for(let i=0;i<=4;i++){const y=24+(h-58)*i/4;
   ctx.beginPath();ctx.moveTo(45,y);ctx.lineTo(w-20,y);ctx.stroke();
   ctx.fillStyle='#92aba2';ctx.font='13px monospace';ctx.fillText((hi-(hi-lo)*i/4).toFixed(1),3,y+4);
  }
  ctx.lineWidth=2.5;ctx.strokeStyle='#c9ee84';ctx.beginPath();
  vals.forEach((v,i)=>{const x=48+(w-72)*i/(vals.length-1||1),y=24+(hi-v)/(hi-lo)*(h-58);
   if(i)ctx.lineTo(x,y);else ctx.moveTo(x,y);
  });ctx.stroke();ctx.fillStyle='#c9ee84';
  vals.forEach((v,i)=>{const x=48+(w-72)*i/(vals.length-1||1),y=24+(hi-v)/(hi-lo)*(h-58);
   ctx.beginPath();ctx.arc(x,y,S.selection.includes(i)?6:3,0,Math.PI*2);ctx.fill();
  });
  return;
 }
 const canvas=$('#map-surface');if(!canvas)return;
 const ctx=canvas.getContext('2d'),w=canvas.width,h=canvas.height;
 ctx.clearRect(0,0,w,h);ctx.fillStyle='#0c1414';ctx.fillRect(0,0,w,h);
 const width=f.width,height=f.height;
 if(width<2||height<2)return;
 function pt(r,c){const normalized=(vals[r*width+c]-lo)/(hi-lo);
  const x=w/2+(c/(width-1)-r/(height-1))*w*.34;
  const y=h*.48+(c/(width-1)+r/(height-1))*h*.22-normalized*h*.35;
  return {x,y,z:normalized};}
 for(let r=height-2;r>=0;r--)for(let c=0;c<width-1;c++){
  const points=[pt(r,c),pt(r,c+1),pt(r+1,c+1),pt(r+1,c)];
  const level=points.reduce((n,x)=>n+x.z,0)/4;
  ctx.fillStyle=`hsl(${90-level*70}, 48%, ${18+level*23}%)`;
  ctx.strokeStyle='#a5c8b52b';ctx.lineWidth=1;
  ctx.beginPath();ctx.moveTo(points[0].x,points[0].y);
  for(const q of points.slice(1))ctx.lineTo(q.x,q.y);
  ctx.closePath();ctx.fill();ctx.stroke();
 }
 ctx.fillStyle='#b2c9bd';ctx.font='13px monospace';ctx.fillText('LIVE ECU VALUES · ISOMETRIC VIEW',16,22);
}
async function api(path,body=null,method='POST'){
 const opts=body===null?{}:{method,headers:{'Content-Type':'application/json'},body:JSON.stringify(body)};
 if(method==='DELETE')opts.method='DELETE';
 const res=await fetch(path,opts);let obj=null;try{obj=await res.json()}catch{}
 if(!res.ok||obj?.ok===false)throw Error(obj?.error||`HTTP ${res.status}`);
 return obj;
}
// Read requests are idempotent, so only reads get transient HTTP/USB retries.
// C and B are NOT retried blindly: an ambiguous reply may follow a successful flash operation.
async function ts(cmd,args='',timeout_ms=2500){
 const idempotent=cmd==='R'||cmd==='O';
 for(let attempt=0;;attempt++){
  try{
   const data=await api('/api/ts',{cmd,args,timeout_ms,...(['B','C'].includes(cmd)?{signature:S.ecuSignature}:{})});
   if(data.status!==0 && !(cmd==='B'&&data.status===4))throw Error('ECU rejected command '+cmd+' (status '+data.status+')');
   return unhex(data.hex||'');
  }catch(e){
   const transient=/HTTP 503|USB.*(?:TIMEOUT|NOT_FOUND)|network.*(?:busy|timeout)|request.*timeout|Failed to fetch|Load failed|network request to/i.test(e.message||'');
   if(!idempotent || attempt>=2 || !transient)throw e;
   await new Promise(resolve=>setTimeout(resolve,120*(attempt+1)));
  }
 }
}
const ascii=b=>new TextDecoder().decode(b).replace(/\0+$/,'').trim();
async function requestPing(){
 // Each connection query must re-read the live ECU signature. A previous cached
 // definition is never presumed compatible with a newly attached ECU.
 if(staged().length&&!confirm('Reconnect will discard all locally staged calibration changes. Continue?'))return;
 S.connected=false;S.ecuSignature='';S.latest=null;GUIDE.setMode('paused');clearCalibrationSession();S.iniResolveState='Checking ECU signature';render();
 try{
  const b=await ts('S','',4000);const signature=ascii(b);
  if(!signature.startsWith('rusEFI '))throw Error('Unexpected ECU signature');
  S.ecuSignature=signature;S.connected=true;
  await resolveLiveIni();
  toast('ECU connected: '+signature);
 }catch(e){S.connected=false;S.ecuSignature='';S.signatureMismatch=!!S.definition;S.iniResolveState='ECU handshake failed';toast('ECU handshake failed: '+e.message,true)}
 render();
}
async function resolveLiveIni(){
  if(!connected())return;
  const signature=S.ecuSignature;
  S.iniExpectedUrl=iniUrlForSignature(signature)||'';
  // ECU SD -> ESP SPIFFS -> optional phone local cache. The file hash tracks
  // change; a live ECU signature still gates access to calibration fields.
  if(await loadCachedIni(false,signature) && S.definition?.signature===signature){
    S.signatureMismatch=false;
    S.iniResolveState='Matching ECU INI loaded from saved definition';
    return;
  }
  S.definition=null;S.rawIni='';S.iniRevision='';S.tsStructure=null;GUIDE.reset(true);
  refreshGuideLinks();drawTsTree();S.latest=null;clearCalibrationSession();
  S.signatureMismatch=true;
  S.iniResolveState=S.iniExpectedUrl?
    'Waiting for ECU SD INI import or manual definition':
    'Custom ECU signature: matching INI must be provided';
}
async function getStatus(){if(S.working)return;try{
 const wasReady=S.status?.usb_device_ready===true;
 const wasIniCached=S.status?.ini_cached===true;
 S.status=await api('/api/status');S.statusTransportError='';
 if(!wasIniCached && S.status.ini_cached && S.ecuSignature && !S.definition && !S.working){
  resolveLiveIni().then(()=>render()).catch(()=>{});
 }
 // Hash is stable across boots, but changes after an SD import or upload.
 if(S.ecuSignature&&S.status.ini_cached&&S.status.ini_sha256&&
     S.iniRevision!==S.status.ini_sha256&&!S.working){
   resolveLiveIni().then(()=>render()).catch(()=>{});
 }
 const nowReady=S.status.usb_device_ready===true;
 const confirmedSignature=S.status.ecu_session_connected?S.status.ecu_signature:'';
 if(confirmedSignature && confirmedSignature!==S.ecuSignature && !S.working){
  clearCalibrationSession();S.ecuSignature=confirmedSignature;S.connected=true;S.latest=null;
  S.iniResolveState='Signature verified by ESP32 USB client';
  resolveLiveIni().then(()=>render()).catch(()=>{});
 }

 if(wasReady&&!nowReady){
  S.connected=false;S.ecuSignature='';S.latest=null;GUIDE.setMode('paused');clearCalibrationSession();S.signatureMismatch=true;
  S.iniResolveState='ECU USB disconnected; waiting for re-enumeration';
 }
 markHeader();
 if(!wasReady&&nowReady&&!S.connected&&!S.working&&!S.status.ecu_session_connected){
  // Re-enumerated ECU: always re-query signature and verify exact INI.
  requestPing().catch(()=>{});
 }
 if((S.route==='logs'||S.route==='system')&&!document.activeElement?.matches('input,select'))render();
 }catch(e){S.statusTransportError=e.message;S.status=null;S.connected=false;markHeader()}}
function liveCaptureLength(){
 if(S.route==='guided'&&safeSignature())return Math.max(4,Math.min(3072,GUIDE.def?guideRequiredBytes(GUIDE.def):S.definition.outputSize||3072));
 if(S.liveMode==='full')return Math.max(4,Math.min(3072,S.definition?.outputSize||3072));
 // Transfer only what visible instruments, scope and stationary-RPM checks need.
 // The 768-byte mode is a *maximum*, not a mandatory transfer length.
 const names=[...activeGauges().map(g=>g.name),...scopeFields().map(f=>f.name),'RPMValue','rpm'];
 const fields=visibleChannels().filter(f=>names.includes(f.name)&&f.offset>=0&&f.offset+TYPES[f.type]<=768);
 let length=Math.max(4,...fields.map(f=>f.offset+TYPES[f.type]));
 if(Number.isFinite(S.definition?.outputSize)&&S.definition.outputSize>0)length=Math.min(length,S.definition.outputSize);
 return Math.min(768,Math.max(4,length));
}
async function startLive(){
 if(!safeSignature()||!connected()||S.liveBusy||S.working)return;
 const len=liveCaptureLength();
 const effectiveHz=(S.route==='overview'||S.route==='gauges'||S.route==='guided')?S.liveHz:10;
 S.liveBusy=true;
 try{
  let incoming=null,sequence=0,duration=0;
  const since=performance.now();
  if(!S.liveFallback){
    const response=await fetch(`/api/live?length=${len}&hz=${effectiveHz}`,{cache:'no-store'});
    if(response.status===404){S.liveFallback=true;throw Error('Live endpoint missing')}
    if(!response.ok)throw Error('Live sampler pending (HTTP '+response.status+')');
    const frame=decodeLiveFrame(await response.arrayBuffer());
    if(frame.length!==len)throw Error('Wrong output block size');
    sequence=frame.sequence;duration=frame.usbDurationMs;
    if(sequence===S.liveSeq)return; // cached frame, no duplicate display or trace.
    incoming=frame.bytes;S.liveSeq=sequence;
  } else {
    incoming=new Uint8Array(len);for(let offset=0;offset<len;offset+=1024){let count=Math.min(1024,len-offset);let block=await ts('O',outputArgs(offset,count),2400);if(block.length!==count)throw Error('Legacy ECU output length mismatch');incoming.set(block,offset)}
    duration=performance.now()-since;
  }
  S.latest=incoming;S.latestMs=Date.now();
  if(safeSignature()&&GUIDE.def){GUIDE.observe(incoming,S.latestMs,sequence);if(S.route==='guided'&&S.guideScreen!=='story'&&performance.now()-S.guideLastRender>900&&!document.activeElement?.matches('input,textarea,select')){S.guideLastRender=performance.now();render();}}S.liveRttMs=performance.now()-since;
  S.liveDurationMs=duration;S.liveFailureCount=0;
  S.liveRxCount++;
  if(!S.liveWindowMs)S.liveWindowMs=S.latestMs;
  if(S.latestMs-S.liveWindowMs>=1000){S.liveDisplayHz=S.liveRxCount*1000/(S.latestMs-S.liveWindowMs);S.liveWindowMs=S.latestMs;S.liveRxCount=0}
  const fields=scopeFields();
  S.trace.push({ms:S.latestMs,values:fields.map(f=>getLatest(f))});
  if(S.trace.length>800)S.trace.shift();
  if(S.route==='overview'||S.route==='gauges'){renderLiveValues();if(!S.lastScopeDraw||performance.now()-S.lastScopeDraw>32){drawLiveScope();S.lastScopeDraw=performance.now()}}
 }catch(e){
  S.liveFailureCount++;
  // An HTTP sample timeout is not an ECU disconnect; /api/status is authoritative.
  if(GUIDE.mode==='learn'&&GUIDE.validMs&&Date.now()-GUIDE.validMs>3000){GUIDE.setMode('paused');if(S.route==='guided')render();}
  if(S.liveFailureCount===8 && !S.liveFallback)console.warn('Telemetry awaiting sample:',e.message);
  if(S.liveFallback && S.liveFailureCount>10)console.warn('Legacy USB polling:',e.message);
 }finally{S.liveBusy=false}
}

function renderLiveValues(){
 const fields=new Map(visibleChannels().map(f=>[f.name,f]));
 document.querySelectorAll('[data-gauge]').forEach(el=>{
  const field=fields.get(el.dataset.gauge);if(!field)return;const val=getLatest(field);el.textContent=format(val)+' '+field.units;
  const bar=el.parentElement.querySelector('.bar b');if(bar){let max=field.name.toLowerCase().includes('rpm')?8000:100;bar.style.width=Math.min(100,Math.max(0,Math.abs(val??0)/max*100))+'%';}
 });
 document.querySelectorAll('[data-instrument]').forEach(el=>{
  const f=fields.get(el.dataset.instrument);if(!f)return;
  const g=activeGauges()[Number(el.dataset.gaugeIndex)];if(!g)return;
  const info=gaugeInfo(g);if(!info)return;
  const val=getLatest(f),fraction=gaugeValuePercent(val,info.min,info.max);
  const reading=el.querySelector('.instrument-reading');if(reading){const number=reading.firstChild;if(number?.nodeType===3)number.textContent=format(val);else reading.textContent=format(val)+' '+f.units;}
  const progress=el.querySelector('.dial-progress');if(progress)progress.setAttribute('stroke-dasharray',(fraction*257.61).toFixed(2)+' 258');
  const pointer=el.querySelector('.dial-pointer');if(pointer)pointer.setAttribute('transform',`rotate(${(-90+180*fraction).toFixed(2)} 100 99)`);
  const bar=el.querySelector('.instrument-bar i');if(bar)bar.style.width=(fraction*100).toFixed(1)+'%';
 });
 const actual=Number(S.status?.live_actual_hz??S.liveDisplayHz);
 const info={'[data-live-actual]':actual.toFixed(1)+' Hz actual','[data-live-usb]':S.liveDurationMs.toFixed(1)+' ms','[data-live-age]':(Date.now()-S.latestMs)+' ms','[data-live-throughput]':(actual*liveCaptureLength()/1024).toFixed(1)+' KB/s'};
 for(const [selector,value] of Object.entries(info))document.querySelectorAll(selector).forEach(node=>node.textContent=value);
}
function drawLiveScope(){
 const cv=document.getElementById('live-scope');if(!cv)return;
 const ctx=cv.getContext('2d');if(!ctx)return;
 const w=cv.width,h=cv.height;ctx.clearRect(0,0,w,h);ctx.fillStyle='#0c1219';ctx.fillRect(0,0,w,h);
 ctx.strokeStyle='#243342';ctx.lineWidth=1;
 for(let x=0;x<=w;x+=w/10){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,h);ctx.stroke()}
 for(let y=0;y<=h;y+=h/5){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke()}
 const fields=scopeFields(),trace=S.trace;if(trace.length<2)return;
 const newest=trace.at(-1).ms,oldest=Math.max(trace[0].ms,newest-10000),duration=Math.max(1000,newest-oldest);
 fields.forEach((f,j)=>{
  const vals=trace.map(q=>q.values[j]).filter(Number.isFinite);
  if(!vals.length)return;
  let lo=Math.min(...vals),hi=Math.max(...vals);
  if(hi-lo<1){lo-=1;hi+=1}
  const pad=(hi-lo)*.12;lo-=pad;hi+=pad;
  ctx.strokeStyle=['#4fe0c4','#ffbc5a','#7a9df8'][j];ctx.lineWidth=2.2;
  ctx.beginPath();let first=true;
  for(const q of trace){if(q.ms<oldest||!Number.isFinite(q.values[j]))continue;
   const x=(q.ms-oldest)/duration*w,y=h-16-(q.values[j]-lo)/(hi-lo)*(h-32);
   if(first){ctx.moveTo(x,y);first=false}else ctx.lineTo(x,y)
  }ctx.stroke();
 });
 ctx.font='12px ui-monospace,monospace';ctx.fillStyle='#7f95a8';ctx.fillText('Up to 10 s · real USB samples',14,18);
}
async function readBytes(page,start,length){const out=new Uint8Array(length);for(let pos=0;pos<length;pos+=1024){const n=Math.min(1024,length-pos);
 const data=await ts('R',readArgs(S.definition,page,start+pos,n),4500);
 if(data.length!==n)throw Error(`ECU returned ${data.length} bytes, expected ${n}`);
 out.set(data,pos);
}return out}
async function loadEntry(f){
 const key=entryKey(f);if(S.edits.has(key))return S.edits.get(key);
 if(!Number.isInteger(f.byteLength)||f.byteLength<1||f.byteLength>8192)throw Error('Calibration field too large for editor');
 const size=S.definition.pageSizes[f.page-1];
 if(!Number.isInteger(size)||f.offset<0||f.offset+f.byteLength>size)throw Error('Field outside declared INI page size');
 const bytes=await readBytes(f.page,f.offset,f.byteLength);
 const entry={key,field:f,start:f.offset,original:bytes.slice(),current:bytes.slice()};
 S.edits.set(key,entry);return entry;
}
async function loadField(name,page,tableId=''){
 if(S.working)return toast('Finish current operation first',true);
 if(!safeSignature())return toast('Connect a firmware-matched ECU first',true);
 const f=S.definition.cal.find(f=>f.name===name&&f.page===page);if(!f)return toast('Calibration field not found',true);
 if(!confirmLeaveEditor(entryKey(f)))return;
 S.working=true;
 try{
  const entry=await loadEntry(f);S.editor=entry;S.editorAxes=null;S.editorTable=null;S.axisSelection=null;S.selection=[0];S.selectStart=0;
  const tables=S.tsStructure?.tables||[];
  const tb=tables.find(t=>(t.id===tableId||t.mapId===tableId)&&t.z===f.name)||tables.find(t=>t.z===f.name);
  if(tb&&f.kind==='array'&&f.height>1){
   const axes={};
   for(const [axis,size] of [['x',f.width],['y',f.height]]){
    const af=S.definition.cal.find(c=>c.name===tb[axis]&&c.kind==='array'&&c.width*c.height===size);
    if(af)axes[axis]=await loadEntry(af);
   }
   S.editorAxes=axes;S.editorTable=tb;
  }
  render();toast(`Opened ${f.name}${S.editorAxes? ' with ECU axis arrays':''}`);
 }catch(e){toast('Unable to read field: '+e.message,true)}finally{S.working=false}
}
// A scalar input stages edits immediately without rerendering the focused field.
function syncBurnDock(){
 const dock=$('.burn-dock');if(!dock)return;
 const changes=staged(),bytes=modifiedBytes();
 const title=dock.querySelector('.burn-dock-copy strong'),label=dock.querySelector('.burn-dock-copy span'),burn=dock.querySelector('[data-action="burn"]');
 if(title)title.textContent=changes.length?`${changes.length} staged fields · ${bytes} bytes`:'All changes saved to ECU';
 if(label)label.textContent=changes.length?'NOT SAVED · Leaving without burning will not save these values.':'No pending edits';
 if(burn){burn.disabled=changes.length===0;burn.textContent=changes.length?'Burn changes':'Saved';}
 const tuneButton=$('#nav [data-route="tuning"]');if(tuneButton){tuneButton.classList.toggle('has-staged',changes.length>0);tuneButton.dataset.pending=String(changes.length)}
}
function stage(index,fn,target=S.editor,redraw=true){
 const e=target;if(S.working||!e)return toast('Editor is busy',true);
 const f=effectiveField(e.field);if(!f.editable)return toast('Field uses unsupported conditional scaling; edit is unavailable',true);
 if(!index?.length)return;
 const before=e.current.slice(),after=before.slice(),rf=relative(f,e.start);
 try{for(const i of index){if(!Number.isInteger(i)||i<0||i>=f.width*f.height)throw Error('Invalid cell index');
   const old=getValue(after,rf,i,S.definition.endianness);
   putValue(after,rf,i,fn(old,i),S.definition.endianness);
  }}catch(ex){return toast(ex.message,true)}
 if(hex(after)===hex(before))return;
 S.undo.push({key:e.key,bytes:before});if(S.undo.length>60)S.undo.shift();S.redo=[];e.current=after;if(redraw)render();else syncBurnDock();
}
function shiftCell(i,shift){if(!S.editor)return;S.axisSelection=null;
 if(shift){const lo=Math.min(i,S.selectStart),hi=Math.max(i,S.selectStart);S.selection=Array.from({length:hi-lo+1},(_,j)=>lo+j)}
 else {S.selectStart=i;S.selection=[i]}render();}
function shiftAxis(axis,i){if(!S.editorAxes?.[axis])return;S.axisSelection={axis,index:i};render()}
function smoothCells(){const e=activeEdit();if(!e||e.field.kind!=='array'||S.axisSelection)return;
 const f=e.field,field=relative(effectiveField(f),e.start),old=e.current.slice();
 if(f.height===1){stage(S.selection,(v,i)=>i===0||i===f.width-1?v:(getValue(old,field,i-1,S.definition.endianness)+2*v+getValue(old,field,i+1,S.definition.endianness))/4,e);return;}
 stage(S.selection,(v,i)=>{const r=Math.floor(i/f.width),c=i%f.width;if(!r||!c||r===f.height-1||c===f.width-1)return v;
  const a=[i,i-1,i+1,i-f.width,i+f.width];return a.reduce((sum,j)=>sum+getValue(old,field,j,S.definition.endianness),0)/5;
 },e);
}
async function backupPage(){const f=S.editor?.field;if(!f)return;
 const size=S.definition.pageSizes[f.page-1];
 if(!size||!Number.isInteger(size)||size>65536||size<=0)return toast('Cannot determine complete page size from this .ini',true);
 S.working=true;toast(`Backing up full ECU page ${f.page} (${size} bytes)…`);
 try {await storeBackup(f.page);await downloadSavedBackup(f.page);render();
 }catch(e){toast('Page backup failed: '+e.message,true)}finally{S.working=false}}
// Store each entire prewrite ECU page on ESP32 SD/SPIFFS, never in Safari
// localStorage. The server verifies CRC and length and atomically publishes it.
// The browser only retains a receipt; no download popup during a transaction.
function savedBackup(page){return S.savedBackups.get(`${S.ecuSignature}:${page}`)||null;}
async function storeBackup(page){
 const size=S.definition?.pageSizes?.[page-1];
 if(!Number.isInteger(size)||size<1||size>65536)throw Error('Invalid ECU page size for backup');
 let receipt;
 // Backing up only reads ECU memory, so a disconnected GET/POST response can
 // be retried safely here (unlike C writes or B burns).
 for(let attempt=0;;attempt++){
   try{
     receipt=await api('/api/backup',{
       page,page_id:pageId(S.definition,page),size,signature:S.ecuSignature
     });
     break;
   }catch(e){
     if(attempt>=2||!/Load failed|Failed to fetch|NetworkError|network request|HTTP 503/i.test(e.message||''))throw e;
     await new Promise(resolve=>setTimeout(resolve,150*(attempt+1)));
   }
 }
 if(receipt.page!==page||receipt.length!==size||!Number.isInteger(receipt.crc32)||receipt.stored!=='ESP32_STORAGE')
   throw Error('ESP32 backup confirmation is incomplete');
 S.savedBackups.set(`${S.ecuSignature}:${page}`,receipt);
 S.backups.add(page);
 return receipt;
}
async function downloadSavedBackup(page){
 if(!Number.isInteger(page)||page<1||page>64)throw Error('Invalid backup page');
 const res=await fetch(`/api/backup/get?page=${page}`,{cache:'no-store'});
 if(!res.ok)throw Error(`ESP32 backup download failed (HTTP ${res.status})`);
 const blob=await res.blob();if(blob.size<1||blob.size>65536)throw Error('Backup file length is invalid');
 saveFile(`brick_ecu_page${page}_prewrite.bin`,blob);
 toast(`ECU page ${page} backup retrieved from ESP32 storage`);
}
function saveFile(name,blob){const a=document.createElement('a');const u=URL.createObjectURL(blob);a.href=u;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(u),1000)}
function authoritativeRpm(){
 const rpm=visibleChannels().find(f=>['RPMValue','rpm','engineRpm'].includes(f.name));
 if(!rpm||!S.latest||rpm.offset+TYPES[rpm.type]>S.latest.length)return null;
 return getLatest(rpm);
}
// Independent USB-side stopped-engine check, immediately before any actual page changes.
// The dashboard is frozen while applying, so its last sample alone is not sufficient.
async function freshStoppedRpm(){
 const rpm=visibleChannels().find(f=>['RPMValue','rpm','engineRpm'].includes(f.name));
 if(!rpm||!Number.isInteger(rpm.offset)||rpm.offset<0)throw Error('No reliable ECU RPM output channel');
 const width=TYPES[rpm.type],raw=await ts('O',outputArgs(rpm.offset,width),4500);
 if(raw.length!==width)throw Error('RPM output reply length mismatch');
 const value=getValue(raw,{...rpm,offset:0},0,S.definition.endianness);
 if(value!==0)throw Error(`Engine RPM is ${value}; writes require a stopped engine`);
}
// Read-only retry after B: do not blindly resend a flash command. No fixed delay
// if the ECU is already ready; allow a short settling period only if it is not.
async function verifyAfterBurn(page,chunk){
 let failure=null;
 for(let attempt=0;attempt<4;attempt++){
  try{
   const verify=await readBytes(page,chunk.offset,chunk.modified.length);
   if(hex(verify)===hex(chunk.modified))return;
   failure=Error(`Post-burn mismatch at page ${page}, offset ${chunk.offset}`);
  }catch(e){failure=e}
  if(attempt<3)await new Promise(resolve=>setTimeout(resolve,[160,350,700][attempt]));
 }
 throw failure||Error(`Unable to verify burned page ${page}`);
}
async function applyChanges(all=false){
 if(S.working)return toast('A calibration operation is already active',true);
 const focus=S.editor?.field.page;
 const entries=staged().filter(e=>all||e.field.page===focus);
 if(!entries.length)return toast('No staged changes');
 if(!safeSignature()||!connected())return toast('ECU signature must match the loaded .ini',true);
 // Query the live ESP32 firmware at the moment of applying edits. An old build
 // cannot be unlocked merely by changing the browser page or unpacking a ZIP.
 let runtime;
 try{runtime=await api('/api/status'); S.status=runtime}
 catch(e){return toast('Cannot verify ESP32 firmware write capability: '+e.message,true)}
 if(!runtime.write_compiled||!runtime.write_access_ready){
   const build=runtime.brick_os_build||'unknown';
   return toast(`ESP32 is running ${build} with writes DISABLED. Flash matching BrickOS firmware to the ESP32; updating the browser or project files alone will not unlock the installed firmware.`,true);
 }

 const pages=[...new Set(entries.map(e=>e.field.page))].sort((a,b)=>a-b);

 // Do not block calibration on an unrelated dashboard poll or its cached timestamp.
 // A direct ECU RPM query is made below and again immediately before each page write.
 let plan;
 try{plan=calibrationWritePlan(entries)}catch(e){return toast(e.message,true)}
 const bytes=plan.reduce((n,p)=>n+p.modified.length,0);
 // Single-tap Burn: the live ECU safety, backup, signature and readback checks run automatically.
 S.working=true;
 beginBurn(entries.length,bytes);
 S.lastWriteError='';S.lastWritePhase='';
 let activePage=null,writeMayHaveStarted=false,phase='initial stopped-RPM check';
 try{
  burnStep(1,'Preparing ECU backup','Checking engine RPM before any calibration changes.');
  await freshStoppedRpm();
  // ALWAYS capture each page fresh and persist it locally before transmitting.
  // Opening an iOS download sheet mid-transaction can disrupt network fetches.
  for(const pg of pages){
   phase=`saving verified ECU backup ${pg} on ESP32 storage`;burnStep(1,'Backing up ECU',`Page ${pg} · a complete verified copy is being saved on the ESP32.`);
   await storeBackup(pg);
  }
  // All originally staged bytes must still match live ECU, before the first change.
  burnStep(2,'Checking changes',`Comparing ${plan.length} changed regions against ECU memory.`);
  for(const chunk of plan){
    phase=`checking page ${chunk.page} at offset ${chunk.offset} before write`;
    const actual=await readBytes(chunk.page,chunk.offset,chunk.original.length);
    if(hex(actual)!==hex(chunk.original))throw Error(`ECU data conflict on page ${chunk.page} at offset ${chunk.offset}. No writes started; reload the editor.`);
  }
  // Automatically arm a short-lived session only AFTER backup and conflict checks.
  phase='authorizing write session';burnStep(3,'Writing calibration','Authorizing this stopped-engine session and preparing ECU writes.');
  await api('/api/arm',{confirm:'BENCH_ONLY_ENGINE_STOPPED_BACKUP_SAVED',signature:S.ecuSignature});
  for(const pg of pages){
    activePage=pg;
    phase=`checking stopped RPM before writing page ${pg}`;
    await freshStoppedRpm();
    const region=plan.filter(c=>c.page===pg);
    for(const chunk of region){
      burnStep(3,'Writing calibration',`Page ${pg} · offset ${chunk.offset} · ${chunk.modified.length} bytes`);
      phase=`writing page ${pg}, offset ${chunk.offset}`;
      writeMayHaveStarted=true; // If connection drops now, command outcome is UNKNOWN.
      await ts('C',writeArgs(S.definition,pg,chunk.offset,chunk.modified),6000);
      phase=`verifying written page ${pg}, offset ${chunk.offset}`;
      const verify=await readBytes(pg,chunk.offset,chunk.modified.length);
      if(hex(verify)!==hex(chunk.modified))throw Error(`Immediate readback mismatch on page ${pg}, offset ${chunk.offset}`);
    }
    if(region.length){
      if(S.definition.pageBurnable?.[pg-1]){phase=`burning page ${pg}`;burnStep(4,'Burning ECU flash',`Page ${pg} · waiting for the ECU to acknowledge the flash command.`);await ts('B',burnArgs(S.definition,pg),9000);}
      burnStep(5,'Verifying saved data',`Page ${pg} · comparing flash readback against your changes.`);
      for(const chunk of region){phase=`verifying page ${pg} after burn at offset ${chunk.offset}`;await verifyAfterBurn(pg,chunk);}
    }
    // This page has been confirmed; accept the current byte buffers as the new base.
    for(const e of S.edits.values())if(e.field.page===pg){
     const overlaps=region.some(c=>c.offset<e.start+e.current.length&&e.start<c.offset+c.modified.length);
     if(overlaps){const actual=await readBytes(pg,e.start,e.current.length);e.current=actual.slice();e.original=actual.slice();}
     else e.original=e.current.slice();
    }
    S.undo=[];S.redo=[];
  }
  const volatile=pages.filter(pg=>!S.definition.pageBurnable?.[pg-1]);
  S.lastWriteError='';S.lastWritePhase='';
  const summary=`Verified ${bytes} changed bytes across ${pages.length} page(s)${volatile.length?'; pages '+volatile.join(', ')+' are RAM only and NOT persistent':''}.`;
  endBurn(true,summary);toast(summary);
 }catch(ex){
   const interrupted=/Load failed|Failed to fetch|network request|NetworkError|HTTP 503/i.test(ex.message||'');
   const disposition=writeMayHaveStarted?'A write might have reached the ECU. Re-read its current values before trying again.':'No calibration write command was sent.';
   S.lastWritePhase=phase;
   S.lastWriteError=(interrupted?'ESP32 browser connection interrupted: ':'')+ex.message+'. '+disposition;
   endBurn(false,'Stopped at '+phase+': '+S.lastWriteError);toast('Burn stopped at '+phase+': '+S.lastWriteError,true);
 }
 finally{try{await api('/api/disarm',{})}catch{}S.working=false;await getStatus();render()}
}
function discardPage(){const pg=S.editor?.field.page;if(!pg)return;
 if(!confirm(`Discard locally staged edits on ECU page ${pg}? No ECU commands will be sent.`))return;
 for(const e of S.edits.values())if(e.field.page===pg)e.current=e.original.slice();
 S.undo=[];S.redo=[];render();toast('Staged page edits discarded (ECU unchanged)');
}
async function reloadLogs(){try{const r=await api('/api/logs');S.logs=r.files||[];if(S.route==='logs')render()}
 catch(e){toast(e.message,true)}}
async function fileAction(name,action){
 if(action==='download'){
  const a=document.createElement('a');a.href='/api/log/get?name='+encodeURIComponent(name);
  a.download=name;a.rel='noopener';document.body.appendChild(a);a.click();a.remove();return;
 }
 if(action==='view'){
  try{const file=S.logs.find(f=>f.name===name);if(file?.size>25000000)throw Error('Analyze logs larger than 25 MB by downloading them first; the built-in viewer has a memory limit');
   const res=await fetch('/api/log/get?name='+encodeURIComponent(name));if(!res.ok)throw Error('HTTP '+res.status);
   S.viewer=readBLog(await res.arrayBuffer());S.viewChannel=visibleChannels()[0]?.name||'';render();toast('Opened '+name)}
  catch(e){toast('Unable to open log: '+e.message,true)}
  return;
 }
 if(action==='delete'){
  if(!confirm('Delete log '+name+' from the ESP32?'))return;
  try{await api('/api/log/delete?name='+encodeURIComponent(name),undefined,'DELETE');toast('Log deleted');await reloadLogs()}
  catch(e){toast(e.message,true)}
 }
}
function drawLogChart(){const cv=$('#log-chart'),d=S.viewer;if(!cv||!d)return;
 const ctx=cv.getContext('2d'),field=visibleChannels().find(x=>x.name===S.viewChannel);
 ctx.clearRect(0,0,cv.width,cv.height);ctx.strokeStyle='#3c4d45';ctx.lineWidth=1;
 for(let i=1;i<5;i++){ctx.beginPath();ctx.moveTo(0,i*cv.height/5);ctx.lineTo(cv.width,i*cv.height/5);ctx.stroke()}
 if(!field||field.offset<d.offset||field.offset+TYPES[field.type]>d.offset+d.size)return;
 const points=d.records.map(o=>getValue(o.bytes,relative(field,d.offset),0,S.definition.endianness));
 const valid=points.filter(Number.isFinite);if(!valid.length)return;
 let lo=Math.min(...valid),hi=Math.max(...valid);if(hi===lo){lo-=1;hi+=1}
 ctx.strokeStyle='#c7e77e';ctx.lineWidth=2;ctx.beginPath();points.forEach((v,i)=>{
 const x=i/(points.length-1||1)*cv.width,y=cv.height-15-(v-lo)/(hi-lo)*(cv.height-30);
 if(i)ctx.lineTo(x,y);else ctx.moveTo(x,y);
});ctx.stroke();ctx.fillStyle='#aebcae';ctx.font='12px monospace';ctx.fillText(`${field.name}  min ${format(lo)}  max ${format(hi)} ${field.units}`,12,20);
}
function exportCsv(){const d=S.viewer;if(!d)return;
 const fields=visibleChannels().filter(f=>f.offset>=d.offset&&f.offset+TYPES[f.type]<=d.offset+d.size);
 const rows=[['uptime_ms',...fields.map(f=>f.name)].join(',')];
 for(const rec of d.records){rows.push([rec.ms,...fields.map(f=>{
 const val=getValue(rec.bytes,relative(f,d.offset),0,S.definition.endianness);return val??'';
 })].join(','))}
 saveFile('brickos_log.csv',new Blob([rows.join('\n')],{type:'text/csv'}));
}

async function runAction(type){try{
 switch(type){
 
case 'open-command':showCommand();break;
 case 'close-command':S.commandOpen=false;render();break;
 case 'tune-browse':if(!S.tuneBrowserOpen&&!confirmLeaveEditor())break;S.tuneBrowserOpen=!S.tuneBrowserOpen;render();break;
case 'open-gauge-customizer':navTo('gauges');break;
case 'add-gauge':{ensureGaugeSlots();const fields=visibleChannels();const f=fields.find(c=>!S.gaugeSlots.some(g=>g.name===c.name))||fields[0];if(!f){toast('Load an INI to add gauges',true);break}if(S.gaugeSlots.length>=16){toast('Maximum 16 gauges',true);break}const [min,max]=recommendedRange(f);S.gaugeSlots.push({name:f.name,kind:'dial',min,max,color:GAUGE_COLORS[S.gaugeSlots.length%6]});ensureLiveCoverage(f);savePrefs();S.gaugeEdit=S.gaugeSlots.length-1;render();break;}
case 'gauge-cancel':S.gaugeEdit=null;render();break;
 case 'gauge-move-prev':case 'gauge-move-next':{ensureGaugeSlots();const i=S.gaugeEdit,step=type==='gauge-move-prev'?-1:1,j=i+step;if(i!==null&&j>=0&&j<S.gaugeSlots.length){[S.gaugeSlots[i],S.gaugeSlots[j]]=[S.gaugeSlots[j],S.gaugeSlots[i]];S.gaugeEdit=j;savePrefs();render();}break;}
case 'gauge-save':{const index=S.gaugeEdit;if(index===null)break;const name=$('#gauge-field')?.value,kind=$('#gauge-kind')?.value,min=Number($('#gauge-min')?.value),max=Number($('#gauge-max')?.value),color=$('#gauge-color')?.value;
  if(!visibleChannels().some(f=>f.name===name))throw Error('Choose a valid ECU output channel');if(!['dial','bar','digital'].includes(kind))throw Error('Unknown gauge style');if(!Number.isFinite(min)||!Number.isFinite(max)||max<=min)throw Error('Maximum must exceed minimum');
  if(!/^#[0-9a-fA-F]{6}$/.test(color))throw Error('Invalid gauge color');ensureGaugeSlots();S.gaugeSlots[index]={name,kind,min,max,color};ensureLiveCoverage(visibleChannels().find(f=>f.name===name));S.gaugeEdit=null;savePrefs();render();toast('Gauge layout saved on this device');break;}
case 'gauge-remove':{const index=S.gaugeEdit;if(index!==null){ensureGaugeSlots();S.gaugeSlots.splice(index,1);S.gaugeEdit=null;savePrefs();render();toast('Gauge removed')}break;}
case 'reset-gauges':S.gaugeSlots=defaultGauges();S.gaugeEdit=null;savePrefs();render();toast('Default gauge layout restored');break;
 case 'export-patch':{
    if(!S.editor)return;
    const e=S.editor,patch=makePatch(S.definition,e.field,e.original,e.current);
    saveFile(`brick_${e.field.name}_page${e.field.page}.btune`,new Blob([JSON.stringify(patch,null,2)],{type:'application/json'}));
    toast('Saved portable staged calibration patch');break;
 }
 case 'import-patch':$('#patch-upload').click();break;
 case 'ecu-version':S.firmwareVersion=ascii(await ts('V','',4000));render();toast('Firmware version received');break;
 case 'ecu-errors':S.ecuErrorMessage=ascii(await ts('e','',4000))||'None reported';render();toast('ECU configuration status received');break;
 case 'import-log':$('#log-upload').click();break;
 case 'save-auto-log':{
    const length=Number($('#log-size')?.value),interval_ms=Number($('#log-interval')?.value);
    const enabled=!!$('#auto-log')?.checked;
    await api('/api/log/auto',{enabled,offset:0,length,interval_ms});
    await getStatus();toast(enabled?'Automatic recording saved for next boot':'Automatic recording disabled');render();break;
 }
 case 'load-ini':navTo('system');$('#ini-upload')?.focus();toast('Choose a file or use Paste INI below');break;
 case 'ini-paste-import':{const text=$('#ini-paste')?.value||'';if(!text.trim())throw Error('Paste the complete INI first');try{setIniImportProgress('Parsing pasted INI and checking signature…');await activateIni(text);await saveCurrentIni()}catch(e){setIniImportProgress('Import failed: '+e.message,true);throw e}break;}
 case 'ini-read':await loadCachedIni(true);break;
 case 'ini-save':try{await saveCurrentIni()}catch(e){setIniImportProgress('Save failed: '+e.message,true);throw e}break;
 case 'ini-delete':await deleteCachedIni();break;

 case 'ping':await requestPing();break;
 case 'close-editor':if(!confirmLeaveEditor())break;S.editor=null;render();break;
 case 'backup':await backupPage();break;
 case 'download-saved-backup':if(S.editor)downloadSavedBackup(S.editor.field.page).catch(e=>toast(e.message,true));break;
 case 'undo':{if(S.undo.length){const item=S.undo.pop(),target=S.edits.get(item.key);if(target){S.redo.push({key:item.key,bytes:target.current.slice()});target.current=item.bytes;render()}}break}
 case 'redo':{if(S.redo.length){const item=S.redo.pop(),target=S.edits.get(item.key);if(target){S.undo.push({key:item.key,bytes:target.current.slice()});target.current=item.bytes;render()}}break}
 case 'inc01':stageSelected(v=>v+0.1);break;case 'dec01':stageSelected(v=>v-0.1);break;
 case 'inc1':stageSelected(v=>v+1);break;case 'dec1':stageSelected(v=>v-1);break;
 case 'inc5':stageSelected(v=>v+5);break;case 'dec5':stageSelected(v=>v-5);break;
 case 'smooth':smoothCells();break;
 case 'edit-selected':{const v=prompt('New numerical value for selected cells:');if(v===null)return;const n=Number(v);if(!Number.isFinite(n))return toast('Invalid number',true);stageSelected(()=>n);break}
 case 'set-scalar':{const v=Number($('#scalar-value')?.value);if(!Number.isFinite(v))return toast('Invalid value',true);stage([0],()=>v);break}
 case 'burn':case 'apply':case 'apply-all':await applyChanges(true);break;
 case 'burn-dismiss':if(S.burnState.active)break;S.burnState.visible=false;drawBurnStatus();break;
 case 'discard-page':discardPage();break;
 case 'disarm':await api('/api/disarm',{});toast('Writes disarmed');await getStatus();render();break;
 case 'reload-logs':await reloadLogs();break;
 case 'start-log':{const length=Number($('#log-size')?.value),interval_ms=Number($('#log-interval')?.value);
    await api('/api/log/start',{offset:0,length,interval_ms});toast('ESP32 onboard logging started');await getStatus();render();break;}
 case 'stop-log':await api('/api/log/stop',{});toast('Onboard logging stopped');await getStatus();await reloadLogs();render();break;
 case 'export-csv':exportCsv();break;
 }}catch(e){toast(e.message,true)}}

document.addEventListener('submit',evt=>{
 const form=evt.target.closest('[data-seth-question-form]');
 if(!form)return;
 evt.preventDefault();
 const text=String(form.elements.question?.value||'').trim();
 if(!text)return;
 try{if(!STORY.ask(text))return;}catch(e){toast(e.message,true);return;}
 SETH.stop();S.guideScreen='story';render();storySpeak(true);
});
document.addEventListener('click',evt=>{
 const selected=evt.target.closest('[data-seth-choice]');
 const backward=evt.target.closest('[data-seth-back]');
 const home=evt.target.closest('[data-seth-home]');
 const ret=evt.target.closest('[data-seth-return]');
 const mute=evt.target.closest('[data-seth-mute]');
 const skip=evt.target.closest('[data-seth-skip]');
 if(mute){SETH.toggle();render();storySpeak(false);return;}
 if(skip){SETH.reveal();return;}
 if(ret){SETH.stop();S.guideScreen='story';render();storySpeak(false);return;}
 if(selected||backward||home){
  S.guideScreen='story';
  let action;
  try{action=STORY.go(home?'welcome':backward?'back':selected.dataset.sethChoice)}catch(e){toast(e.message,true);return;}
  if(action.type==='workspace'){SETH.stop();S.guideScreen='analysis';S.guideTab=['evidence','atlas','systems','next','history'].includes(action.tab)?action.tab:'next';render();return;}
  if(action.type==='manual_field'){
   const field=GUIDE.inventory[action.index];const item=field?S.guideMenuLinks[field.name]:null;
   if(!item){toast('This INI field has no verified original menu link',true);return;}SETH.stop();chooseTsItem(item);return;
  }
  if(action.type==='manual'){
   const target=findManualTarget(action.stage,S.tsStructure);
   if(!target){toast('No verified original INI menu for this system',true);return;}
   SETH.stop();chooseTsItem(target.id);return;
  }
  SETH.stop();render();storySpeak(true);
 }
});
document.addEventListener('click',evt=>{
 const tab=evt.target.closest('[data-guide-tab]');if(tab){SETH.stop();S.guideScreen='analysis';S.guideTab=tab.dataset.guideTab;S.guideSearch='';if(S.route!=='guided')navTo('guided');else render();return;}
 const stage=evt.target.closest('[data-guide-stage]');if(stage){S.guideTab='systems';render();document.querySelector('.guide-steps')?.scrollIntoView({block:'start'});return;}
 const mode=evt.target.closest('[data-guide-mode]');if(mode){
  try{if(mode.dataset.guideMode==='learn'&&(!connected()||!safeSignature()||!GUIDE.report().fresh))throw Error('Live verified telemetry required');
   GUIDE.setMode(mode.dataset.guideMode);render();}catch(e){toast(e.message,true)}return;}
 const open=evt.target.closest('[data-guide-manual]');if(open){const id=S.guideMenuLinks[open.dataset.guideManual];if(id)chooseTsItem(id);return;}
 const save=evt.target.closest('[data-guide-save-filters]');if(save){const config={};document.querySelectorAll('[data-guide-filter]').forEach(el=>config[el.dataset.guideFilter]=el.value);
  try{GUIDE.setFilters(config);render();toast('Analysis filters saved; observations reset');}catch(e){toast(e.message,true)}return;}
 const exp=evt.target.closest('[data-guide-export]');if(exp){const report=GUIDE.report();saveFile('brickos_guided_review.json',new Blob([JSON.stringify({report,inventory:GUIDE.inventory},null,2)],{type:'application/json'}));return;}
});
document.addEventListener('click',async evt=>{
 if(evt.target.id==='command-backdrop'){S.commandOpen=false;render();return}
 const cmdRoute=evt.target.closest('[data-command-route]');if(cmdRoute){navTo(cmdRoute.dataset.commandRoute);return}
 const cmdItem=evt.target.closest('[data-command-item]');if(cmdItem){chooseTsItem(cmdItem.dataset.commandItem);return}
 const quick=evt.target.closest('[data-ts-category]');if(quick){if(!confirmLeaveEditor())return;S.editor=null;S.tuneCategory=quick.dataset.tsCategory;S.tsSelected='';S.tuneBrowserOpen=true;S.route='tuning';render();return}
 const cat=evt.target.closest('[data-tune-category]');if(cat){if(!confirmLeaveEditor())return;S.editor=null;S.tuneCategory=cat.dataset.tuneCategory;S.tuneSearch='';S.tuneBrowserOpen=true;if(S.route!=='tuning')S.route='tuning';render();return}
 const star=evt.target.closest('[data-tune-star]');if(star){const id=star.dataset.tuneStar;S.tuneFavorites=S.tuneFavorites.includes(id)?S.tuneFavorites.filter(x=>x!==id):[...S.tuneFavorites,id].slice(0,24);savePrefs();drawTsTree();return}
 const editGauge=evt.target.closest('[data-gauge-edit]');if(editGauge){S.gaugeEdit=Number(editGauge.dataset.gaugeEdit);render();return}
 const addChannel=evt.target.closest('[data-add-channel]');if(addChannel){ensureGaugeSlots();const f=visibleChannels().find(c=>c.name===addChannel.dataset.addChannel);if(f&&S.gaugeSlots.length<16){const [min,max]=recommendedRange(f);S.gaugeSlots.push({name:f.name,kind:'dial',min,max,color:GAUGE_COLORS[S.gaugeSlots.length%6]});ensureLiveCoverage(f);savePrefs();render();toast('Added '+channelTitle(f.name))}else toast('Maximum 16 gauges per layout',true);return}
 const menuItem=evt.target.closest('[data-ts-item]');if(menuItem){chooseTsItem(menuItem.dataset.tsItem);return}
 const toggle=evt.target.closest('[data-ts-toggle]');if(toggle){const k=toggle.dataset.tsToggle;S.tsOpen[k]=S.tsOpen[k]===false?true:false;drawTsTree();return}
 const route=evt.target.closest('[data-route]');if(route){navTo(route.dataset.route);return}
 const filter=evt.target.closest('[data-workspace-filter]');if(filter){if(filter.dataset.workspaceFilter==='tables')S.tableCategory=filter.dataset.category;else S.settingsCategory=filter.dataset.category;render();return}
 const axis=evt.target.closest('[data-axis-cell]');if(axis){shiftAxis(axis.dataset.axisCell,+axis.dataset.axisIndex);return}
 const cell=evt.target.closest('[data-cell]');if(cell){shiftCell(+cell.dataset.cell,evt.shiftKey);return}
 const field=evt.target.closest('[data-load-field]');if(field){await loadField(field.dataset.loadField,+field.dataset.fieldPage,field.dataset.tableId||'');return}
 const file=evt.target.closest('[data-file-action]');if(file){await fileAction(file.dataset.file,file.dataset.fileAction);return}
 const a=evt.target.closest('[data-action]');if(a){await runAction(a.dataset.action)}
});
document.addEventListener('dblclick',e=>{
 const node=e.target.closest('[data-cell],[data-axis-cell]');if(!node||!S.editor)return;
 if(node.dataset.axisCell)S.axisSelection={axis:node.dataset.axisCell,index:Number(node.dataset.axisIndex)};
 else{S.axisSelection=null;S.selection=[Number(node.dataset.cell)];S.selectStart=S.selection[0]}
 const entry=activeEdit(),i=selectedIndexes()[0];if(!entry)return;
 const v=prompt(`Value (${effectiveField(entry.field).units||'raw'}):`,String(entryValue(entry,i)));
 if(v!==null&&v.trim()!==''&&Number.isFinite(Number(v)))stageSelected(()=>Number(v));
});
document.addEventListener('input',e=>{
 if(e.target.id==='scalar-value'&&S.editor&&!S.working){
  const raw=e.target.value,number=Number(raw);
  if(raw.trim()!==''&&Number.isFinite(number))stage([0],()=>number,S.editor,false);
 }
 if(e.target.id==='guide-search'){S.guideSearch=e.target.value;const cursor=e.target.selectionStart;render();const el=$('#guide-search');if(el){el.focus();el.setSelectionRange(cursor,cursor)}return;}
 if(e.target.id==='command-search'){S.commandSearch=e.target.value;const root=$('#command-results');if(root)root.innerHTML=commandResults();}
 if(e.target.id==='tune-search'){S.tuneSearch=e.target.value;drawTsTree();}
 if(e.target.id==='channel-search'||e.target.id==='setting-search'){S.search=e.target.value;const id=e.target.id,cursor=e.target.selectionStart;render();let el=$('#'+id);if(el){el.focus();el.setSelectionRange(cursor,cursor)}}
 if(e.target.id==='table-search'){S.tableSearch=e.target.value;const cursor=e.target.selectionStart;render();let el=$('#table-search');el?.focus();el?.setSelectionRange(cursor,cursor)}
});
document.addEventListener('change',e=>{
 if(e.target.id==='scalar-value'&&S.editor&&!S.working&&e.target.value.trim()!==''){
  const number=Number(e.target.value);if(Number.isFinite(number))stage([0],()=>number,S.editor,false);
 }
 if(e.target.matches('.ts-group')){const group=e.target;const cat=group.closest('.ts-cat')?.querySelector('[data-ts-toggle]')?.dataset.tsToggle||'';S.tsOpen['g:'+cat+':'+group.querySelector('summary').textContent]=group.open}
 if(e.target.id==='live-rate'||e.target.id==='live-mode'){if(e.target.id==='live-rate')S.liveHz=Number(e.target.value);else S.liveMode=e.target.value;S.liveSeq=0;S.liveWindowMs=0;S.liveRxCount=0;S.trace=[];render()}
 if(e.target.id==='dashboard-layout'){S.dashboardLayout=e.target.value;savePrefs();render();}
 if(e.target.matches('[data-scope-index]')){const i=Number(e.target.dataset.scopeIndex);const names=scopeFields().map(f=>f.name);names[i]=e.target.value;S.scopeNames=names.filter(Boolean).slice(0,3);ensureLiveCoverage(visibleChannels().find(f=>f.name===e.target.value));S.trace=[];savePrefs();render();}
 if(e.target.id==='log-channel'){S.viewChannel=e.target.value;drawLogChart()}
});
document.addEventListener('keydown',e=>{
 if(S.editor&&['INPUT','TEXTAREA','SELECT'].indexOf(document.activeElement?.tagName)<0&&!S.commandOpen){
  if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='z'){e.preventDefault();runAction(e.shiftKey?'redo':'undo');return}
  if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='y'){e.preventDefault();runAction('redo');return}
  if(['ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.key)){
   const map=S.editor.field;
   if(S.axisSelection){const axis=S.axisSelection.axis,lim=axis==='x'?map.width:map.height;
     const pos=Math.max(0,Math.min(lim-1,S.axisSelection.index+(['ArrowUp','ArrowLeft'].includes(e.key)?-1:1)));shiftAxis(axis,pos);
   }else{const i=S.selection.at(-1)||0,delta=e.key==='ArrowUp'?-map.width:e.key==='ArrowDown'?map.width:e.key==='ArrowLeft'?-1:1;
     shiftCell(Math.max(0,Math.min(map.width*map.height-1,i+delta)),e.shiftKey)}
   e.preventDefault();return;
  }
  if(e.key==='Enter'){e.preventDefault();runAction('edit-selected');return}
 }
 if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='k'){e.preventDefault();showCommand();}else if(e.key==='Escape'&&S.commandOpen){S.commandOpen=false;render();}});
$('#connect').onclick=requestPing;
$('#patch-upload').addEventListener('change',async evt=>{
 const file=evt.target.files[0];if(!file)return;
 try{
  if(file.size>50000)throw Error('Patch too large');
  if(!S.editor)throw Error('Open the calibration field before importing a patch');
  const patch=JSON.parse(await file.text());
  const e=S.editor,data=validatePatch(S.definition,e.field,e.original,patch);
  S.undo.push({key:e.key,bytes:e.current.slice()});if(S.undo.length>60)S.undo.shift();S.redo=[];e.current=data;render();toast('Patch staged locally. No ECU writes performed.');
 }catch(e){toast('Patch import failed: '+e.message,true)}finally{evt.target.value=''}
});
$('#log-upload').addEventListener('change',async evt=>{
 const file=evt.target.files[0];if(!file)return;
 try{if(file.size>25000000)throw Error('The analyzer currently limits files to 25 MB');
  S.viewer=readBLog(await file.arrayBuffer());S.viewChannel=visibleChannels()[0]?.name||'';
  S.route='logs';render();toast('Imported local log for analysis');
 }catch(e){toast('Log import failed: '+e.message,true)}finally{evt.target.value=''}
});
async function activateIni(text,readyDefinition=null,revision=''){
  if(new TextEncoder().encode(text).length>1200000)throw Error('Definition is too large for the ESP32 cache');
  const def=readyDefinition||(connected()?matchingIni(text,S.ecuSignature,parseIni):parseIni(text));
  if(connected()&&def.signature!==S.ecuSignature)throw Error('INI signature differs from connected ECU');
  if(!def.channels.length||!def.cal.length)throw Error('Not a generated rusEFI definition');
  if(S.rawIni!==text||S.iniRevision!==revision)clearCalibrationSession();
  S.rawIni=text;S.definition=def;S.iniRevision=revision;rebuildTs();GUIDE.attach(def,S.ecuSignature);
  S.signatureMismatch=connected()&&!safeSignature();S.latest=null;
  render();toast('Loaded '+def.cal.length+' calibration fields and '+def.channels.length+' channels');
}
let iniLoadTask=null;
async function loadCachedIni(showError=false,expectedSignature=''){
  // Single-flight: boot, USB handshake and status loop can request the INI at
  // the same time. Never download/parse three copies on the phone.
  if(iniLoadTask)return iniLoadTask;
  iniLoadTask=(async()=>{
    const meta=await api('/api/ini/status');
    if(!meta.cached)throw Error('No saved INI on ESP32 yet');
    const hash=/^[a-f0-9]{64}$/i.test(meta.sha256||'')?meta.sha256:'';
    const size=Number(meta.size)||0;
    if(hash&&S.iniRevision===hash&&S.definition&&
       (!expectedSignature||S.definition.signature===expectedSignature))return true;
    // Fast path: reuse the parsed definition previously received from the
    // ESP's exact SD import. IndexedDB failure means a normal network load.
    const prior=hash?await readLocalIni(hash,expectedSignature,size):null;
    if(prior){
      await activateIni(prior.rawIni,prior.definition,hash);
      S.iniResolveState='Reused locally parsed INI from ESP cache';
      return true;
    }
    const resp=await fetch('/api/ini/file',{cache:'no-store'});
    if(!resp.ok)throw Error('No readable INI on ESP32 (HTTP '+resp.status+')');
    const raw=await resp.text();
    const def=expectedSignature?matchingIni(raw,expectedSignature,parseIni):parseIni(raw);
    await activateIni(raw,def,hash);
    if(hash)writeLocalIni(hash,raw,def,size).catch(()=>{});
    S.iniResolveState='INI imported from ESP32 saved SD definition';
    return true;
  })();
  try{return await iniLoadTask}
  catch(e){if(showError)toast('Load saved INI failed: '+e.message,true);return false}
  finally{iniLoadTask=null}
}
function setIniImportProgress(message,failed=false){
 S.iniImportProgress=message;
 const indicator=$('#ini-import-progress');
 if(indicator){indicator.textContent=message;indicator.classList.toggle('error',failed)}
}
function uploadIniWithProgress(text){
 return new Promise((resolve,reject)=>{
  const request=new XMLHttpRequest();request.open('POST','/api/ini/upload',true);
  request.setRequestHeader('Content-Type','text/plain; charset=utf-8');
  request.timeout=60000;
  request.upload.onprogress=e=>{if(e.lengthComputable)setIniImportProgress(`Uploading INI to ESP32: ${Math.round(100*e.loaded/e.total)}% (${e.loaded}/${e.total} bytes)`);};
  request.onerror=()=>reject(Error('Network connection lost during upload'));
  request.ontimeout=()=>reject(Error('Upload timed out; check ESP32 Wi-Fi and storage'));
  request.onload=()=>{
   if(request.status>=200&&request.status<300){resolve();return}
   let detail='HTTP '+request.status;
   try{const data=JSON.parse(request.responseText);detail=data.error||data.message||detail}catch{if(request.responseText)detail=request.responseText.slice(0,150)}
   reject(Error('ESP32 rejected upload: '+detail));
  };
  request.send(text);
 });
}
async function saveCurrentIni(){
 if(!S.rawIni)throw Error('Import a generated INI first');
 setIniImportProgress('Uploading definition to ESP32 SPIFFS…');
 await uploadIniWithProgress(S.rawIni);
 setIniImportProgress('Upload complete. Verifying cached INI…');
 const resp=await fetch('/api/ini/file',{cache:'no-store'});
 if(!resp.ok)throw Error('ESP32 did not return a cached INI (HTTP '+resp.status+')');
 const saved=await resp.text();
 if(saved!==S.rawIni)throw Error('Cached INI differs from uploaded file');
 setIniImportProgress(`Complete — ${new TextEncoder().encode(saved).length} bytes saved and verified on ESP32`);
 const metadata=await api('/api/ini/status');
 const hash=/^[a-f0-9]{64}$/i.test(metadata.sha256||'')?metadata.sha256:'';
 S.iniRevision=hash;
 if(hash)writeLocalIni(hash,S.rawIni,S.definition,Number(metadata.size)||0).catch(()=>{});
 toast('Definition saved and verified on ESP32');await getStatus();
}
async function deleteCachedIni(){
 try{await api('/api/ini/file',{},'DELETE');await clearLocalIni();
    S.iniRevision='';S.rawIni='';S.definition=null;S.tsStructure=null;
    GUIDE.reset(true);clearCalibrationSession();toast('Saved INI deleted');await getStatus();}
 catch(e){toast('Delete failed: '+e.message,true)}
}
// INI picker belongs to the dynamically rendered Connection screen.
// Delegate the event so the handler works after navigation/re-rendering on Safari/iOS.
document.addEventListener('change',async evt=>{
 if(evt.target.id!=='ini-upload')return;
 const f=evt.target.files?.[0];if(!f)return;
 if(S.iniBusy){setIniImportProgress('Another INI import is already running',true);return}
 S.iniBusy=true;
 try{
  setIniImportProgress(`Reading ${f.name} (${f.size} bytes)…`);
  if(f.size>1200000)throw Error(`File is ${f.size} bytes; ESP32 currently supports up to 1200000 bytes`);
  if(f.size<100)throw Error('File too short to be a complete generated INI');
  const text=await f.text();
  setIniImportProgress('Parsing definitions and checking the live ECU signature…');
  await activateIni(text);
  await saveCurrentIni();
 }catch(e){setIniImportProgress('Import failed: '+e.message,true);toast('Definition import failed: '+e.message,true)}
 finally{S.iniBusy=false;evt.target.value=''}
});
async function initialize(){
 await loadCachedIni(false);
 render();
 await getStatus();
 if(S.status?.usb_device_ready && !S.connected)await requestPing();
 await reloadLogs().catch(()=>{});
 setInterval(()=>{getStatus().catch(()=>{})},3500);
 let last=0;
 setInterval(()=>{if(!connected())return;const now=Date.now();if(now-last<1000/S.liveHz)return;last=now;startLive()},10);
 // Show the source as ready, but never silently generate simulated ECU data.
}
initialize();

// Separate dialogue catalogue ships from ESP; no DS executable dialogue text.
fetchStoryCatalog();
