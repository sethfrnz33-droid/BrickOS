/* Presentation-only phone renderer. No decisions or user text hardcoded here. */
export const htmlSafe=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
export function renderSethStory(packet,{muted=false}={}){
 const a=packet.assets||{};
 const portrait=`<div class="seth-portrait" aria-label="Seth, the ECU guide" role="img"><img class="seth-layer seth-mouth" src="${htmlSafe(a.mouth||'')}" alt=""><img class="seth-layer seth-head" src="${htmlSafe(a.head||'')}" alt=""><img class="seth-layer seth-jaw" src="${htmlSafe(a.jaw||'')}" alt=""></div>`;
 return `<section class="seth-guide" aria-label="Seth's guided tuning conversation">
 <div class="seth-guide-top"><div class="seth-guide-identity"><span class="app-section">BRICKOS / GUIDED TUNING</span><h1>Ask Seth</h1><p>Full ECU tour or targeted help, based on the connected ECU definition.</p></div><span class="guide-pill">${htmlSafe(packet.read_only?'READ ONLY':'PREVIEW')}</span></div>
 <div class="seth-story-layout"><div class="seth-character-side">${portrait}<div class="seth-character-caption"><strong>SETH</strong><small>Resident ECU clone</small></div></div>
 <div class="seth-dialog-side"><div class="seth-dialog-frame"><div class="seth-dialog-name">SETH <span class="seth-dialog-tag">${htmlSafe(packet.title||'')}</span></div><p class="seth-dialog-text" data-seth-dialog-text>${htmlSafe(packet.text)}</p><div class="seth-dialog-actions"><button type="button" class="btn ghost" data-seth-skip>Show full text</button><button type="button" class="btn ghost" data-seth-mute aria-pressed="${muted}">${muted?'Unmute voice':'Mute voice'}</button></div></div>
 <div class="seth-answers">${(packet.choices||[]).map((c,i)=>`<button type="button" class="seth-answer" data-seth-choice="${htmlSafe(c.go)}" ${!c.enabled?'disabled':''}><span class="seth-answer-index">${String(i+1).padStart(2,'0')}</span><span>${htmlSafe(c.label)}${c.blocked_reason?` <small>${htmlSafe(c.blocked_reason)}</small>`:''}</span><span class="seth-answer-arrow">›</span></button>`).join('')}</div>
 <div class="seth-guide-links"><button type="button" class="btn ghost" data-seth-back ${!packet.can_back?'disabled':''}>Back</button><button type="button" class="btn ghost" data-seth-home>Guide home</button><button type="button" class="btn ghost" data-guide-tab="evidence">Evidence</button><button type="button" class="btn ghost" data-guide-tab="atlas">Full INI Atlas</button></div>
 </div></div><p class="seth-evidence-note">${packet.definition_key?'ECU definition: '+htmlSafe(packet.definition_key):'No ECU definition verified'} · ${htmlSafe(packet.freshness||'unknown')} data · Guide dialogue never writes calibration values.</p>
 </section>`;
}
