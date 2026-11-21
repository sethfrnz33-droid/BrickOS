/* Presentation-only phone renderer. No decisions or user text hardcoded here. */
export const htmlSafe=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
export function renderSethStory(packet,{muted=false}={}){
 const a=packet.assets||{};
 const check=packet.check||null;
 const chapterProgress=packet.chapter_progress?`<div class="seth-chapter-progress"><strong>Chapter checks:</strong> ${packet.chapter_progress.reported} user-reviewed · ${packet.chapter_progress.unknown} need clarification · ${packet.chapter_progress.unreviewed} untouched <small>These are notes, not proof of engine calibration.</small></div>`:'';
 const checkStatus=check?`<div class="seth-checkpoint" role="status"><strong>Chapter question ${check.index} of ${check.total}</strong><span>${htmlSafe(check.status_meaning)}</span>${check.requires_external?'<small>External physical verification required; reported completion is not ECU verification.</small>':''}</div>`:'';
 const reason=packet.reasoning;
 const reasoning=reason?`<section class="seth-reasoning" aria-label="Seth diagnostic reasoning">
   <div class="seth-reason-title"><strong>What Seth found</strong><span>${htmlSafe(reason.confidence)}</span></div>
   <p>${htmlSafe(reason.quality?.measurementTrust||'')}</p>
   ${reason.facts?.length?`<div class="seth-reason-sub"><strong>Observed facts</strong>${reason.facts.slice(0,4).map(f=>`<p>${htmlSafe(f.text)}</p>`).join('')}</div>`:''}
   ${reason.hypotheses?.slice(0,3).map(h=>`<div class="seth-reason-sub"><strong>${htmlSafe(h.title)}</strong><p>${htmlSafe(h.observation)}</p><small>Possible explanations: ${htmlSafe(h.causes.join(' · '))}</small><p>Missing: ${htmlSafe(h.missing.join('; '))}</p></div>`).join('')||''}
   ${reason.contradictions?.length?`<div class="seth-reason-sub"><strong>Conflicting evidence</strong>${reason.contradictions.map(x=>`<p>${htmlSafe(x.evidence)} ${htmlSafe(x.meaning)}</p>`).join('')}</div>`:''}
   ${reason.questions?.length?`<div class="seth-reason-sub"><strong>Best next question</strong><p>${htmlSafe(reason.questions[0].prompt)}</p><small>${htmlSafe(reason.questions[0].reason)}</small></div>`:''}
  </section>`:'';
 const portrait=`<div class="seth-portrait" aria-label="Seth, the ECU guide" role="img"><img class="seth-layer seth-mouth" src="${htmlSafe(a.mouth||'')}" alt=""><img class="seth-layer seth-head" src="${htmlSafe(a.head||'')}" alt=""><img class="seth-layer seth-jaw" src="${htmlSafe(a.jaw||'')}" alt=""></div>`;
 return `<section class="seth-guide" aria-label="Seth's guided tuning conversation">
 <div class="seth-guide-top"><div class="seth-guide-identity"><span class="app-section">BRICKOS / GUIDED TUNING</span><h1>Ask Seth</h1><p>Ask about a setting, explore a problem, or learn the whole system from the beginning.</p></div><span class="guide-pill">${packet.tour_step?`STEP ${packet.tour_step} / ${packet.tour_total}`:htmlSafe(packet.read_only?'READ ONLY':'PREVIEW')}</span></div>
 <div class="seth-story-layout"><div class="seth-character-side">${portrait}<div class="seth-character-caption"><strong>SETH</strong><small>Here to help</small></div></div>
 <div class="seth-dialog-side"><form class="seth-ask-form" data-seth-question-form><label for="seth-ask-input">Ask Seth about tuning or a setting</label><div class="seth-ask-row"><input id="seth-ask-input" name="question" type="search" maxlength="240" autocomplete="off" placeholder="What does injector dead time mean?" aria-label="Ask about a setting or tuning concept"><button type="submit" class="btn primary">Ask</button></div><small>Uses your ECU's settings when available. A name is a clue, not proof.</small></form><div class="seth-dialog-frame"><div class="seth-dialog-name">SETH <span class="seth-dialog-tag">${htmlSafe(packet.title||'')}</span></div><p class="seth-dialog-text" data-seth-dialog-text>${htmlSafe(packet.text)}</p><div class="seth-dialog-actions"><button type="button" class="btn ghost" data-seth-skip>Show full text</button><button type="button" class="btn ghost" data-seth-mute aria-pressed="${muted}">${muted?'Unmute voice':'Mute voice'}</button></div></div>
 ${checkStatus}${chapterProgress}
 ${reasoning}<div class="seth-answers">${(packet.choices||[]).map((c,i)=>`<button type="button" class="seth-answer" data-seth-choice="${htmlSafe(c.go)}" ${!c.enabled?'disabled':''}><span class="seth-answer-index">${String(i+1).padStart(2,'0')}</span><span>${htmlSafe(c.label)}${c.blocked_reason?` <small>${htmlSafe(c.blocked_reason)}</small>`:''}</span><span class="seth-answer-arrow">›</span></button>`).join('')}</div>
 <div class="seth-guide-links"><button type="button" class="btn ghost" data-seth-back ${!packet.can_back?'disabled':''}>Back</button><button type="button" class="btn ghost" data-seth-home>Guide home</button><button type="button" class="btn ghost" data-guide-tab="evidence">Evidence</button><button type="button" class="btn ghost" data-guide-tab="atlas">Settings browser</button></div>
 </div></div><details class="seth-evidence-note"><summary>Connection & technical details</summary><p>${packet.definition_key?'ECU definition: '+htmlSafe(packet.definition_key):'No matching ECU definition loaded'} · ${htmlSafe(packet.freshness||'unknown')} data · This guide doesn't edit the ECU.</p></details>
 </section>`;
}
