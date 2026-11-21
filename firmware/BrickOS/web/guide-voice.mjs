/* Uses the user-supplied gibberish clip. Presentation only. Requires a click.
 * The jaw is driven by WebAudio amplitude where supported; speech remains
 * readable with audio off, unsupported, or blocked by browser policy. */
export class SethSpeech {
 constructor(){this.muted=false;this.audio=null;this.ctx=null;this.analyser=null;this.wave=null;this.anim=null;this.seq=0;this.lastText='';this.moodTimer=null;this.moodReset=null;}
 stop(){this.seq++;clearTimeout(this.moodTimer);clearTimeout(this.moodReset);if(this.audio){this.audio.pause();this.audio.currentTime=0;}if(this.anim)cancelAnimationFrame(this.anim);this.anim=null;
  const p=typeof document==='undefined'?null:document.querySelector('.seth-portrait');if(p){p.style.setProperty('--seth-jaw-gap','0px');delete p.dataset.mood;}}
 toggle(){this.muted=!this.muted;if(this.muted)this.stop();return this.muted;}
 setup(src){if(!this.audio){this.audio=new Audio(src);this.audio.preload='auto';this.audio.volume=.4;this.audio.crossOrigin='anonymous';}
  if(this.audio.src!==new URL(src,document.baseURI).href)this.audio.src=src;
  if(!this.ctx){const C=window.AudioContext||window.webkitAudioContext;if(C){try{this.ctx=new C();this.analyser=this.ctx.createAnalyser();this.analyser.fftSize=256;this.wave=new Uint8Array(256);this.ctx.createMediaElementSource(this.audio).connect(this.analyser);this.analyser.connect(this.ctx.destination);}catch{this.ctx=null;this.analyser=null;}}}
 }
 mood(tone='normal'){
  if(typeof document==='undefined'||window.matchMedia?.('(prefers-reduced-motion: reduce)')?.matches)return;
  const current=this.seq;
  const variants=['blink','curious','bored','skeptical','chuckle','surprised','look-away'];
  const special=['curious','surprised','skeptical','chuckle'].includes(tone)?tone:null;
  const pick=()=>{
   if(current!==this.seq)return;
   const portrait=document.querySelector('.seth-portrait');
   if(!portrait?.isConnected)return;
   const next=special&&Math.random()<.5?special:variants[Math.floor(Math.random()*variants.length)];
   portrait.dataset.mood=next;
   clearTimeout(this.moodReset);
   this.moodReset=setTimeout(()=>{if(current===this.seq&&portrait.isConnected)delete portrait.dataset.mood;},900+Math.random()*700);
   this.moodTimer=setTimeout(pick,2500+Math.random()*3900);
  };
  this.moodTimer=setTimeout(pick,600+Math.random()*1600);
 }
 speak(text,src,{playVoice=false,tone='normal'}={}){
  this.stop();const seq=this.seq;const p=document.querySelector('[data-seth-dialog-text]');if(!p)return;this.mood(tone);
  const full=String(text||'');this.lastText=full;p.textContent='';let start=performance.now();const step=now=>{
   if(seq!==this.seq||!p.isConnected)return;const n=Math.min(full.length,Math.floor((now-start)/19));p.textContent=full.slice(0,n);
   if(n<full.length)requestAnimationFrame(step);
  };requestAnimationFrame(step);
  if(!playVoice||this.muted||!src)return;
  try{this.setup(src);this.ctx?.resume?.();this.audio.currentTime=0;
   this.audio.play().catch(()=>{});const timeout=Math.min(6000,Math.max(1100,full.length*19));
   const began=performance.now();const mouth=()=>{if(seq!==this.seq)return;const portrait=document.querySelector('.seth-portrait');if(!portrait?.isConnected){this.audio.pause();return;}
    let a=0;if(this.analyser){this.analyser.getByteTimeDomainData(this.wave);for(const s of this.wave)a+=Math.abs(s-128);a/=this.wave.length;}
    const gap=this.audio.paused?0:Math.max(0,Math.min(13,a*1.6));portrait.style.setProperty('--seth-jaw-gap',gap.toFixed(1)+'px');
    if(performance.now()-began>timeout||this.audio.paused){this.audio.pause();portrait.style.setProperty('--seth-jaw-gap','0px');return;}
    this.anim=requestAnimationFrame(mouth);
   };this.anim=requestAnimationFrame(mouth);
  }catch{/* Audio is optional. Guide text must always remain usable. */}
 }
 reveal(){const el=document.querySelector('[data-seth-dialog-text]');if(el)el.textContent=this.lastText;}
}
