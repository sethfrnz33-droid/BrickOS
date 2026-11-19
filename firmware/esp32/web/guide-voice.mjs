/* Uses the user-supplied gibberish clip. Presentation only. Requires a click.
 * The jaw is driven by WebAudio amplitude where supported; speech remains
 * readable with audio off, unsupported, or blocked by browser policy. */
export class SethSpeech {
 constructor(){this.muted=false;this.audio=null;this.ctx=null;this.analyser=null;this.wave=null;this.anim=null;this.seq=0;this.lastText='';}
 stop(){this.seq++;if(this.audio){this.audio.pause();this.audio.currentTime=0;}if(this.anim)cancelAnimationFrame(this.anim);this.anim=null;
  const p=typeof document==='undefined'?null:document.querySelector('.seth-portrait');if(p)p.style.setProperty('--seth-jaw-gap','0px');}
 toggle(){this.muted=!this.muted;if(this.muted)this.stop();return this.muted;}
 setup(src){if(!this.audio){this.audio=new Audio(src);this.audio.preload='auto';this.audio.volume=.4;this.audio.crossOrigin='anonymous';}
  if(this.audio.src!==new URL(src,document.baseURI).href)this.audio.src=src;
  if(!this.ctx){const C=window.AudioContext||window.webkitAudioContext;if(C){try{this.ctx=new C();this.analyser=this.ctx.createAnalyser();this.analyser.fftSize=256;this.wave=new Uint8Array(256);this.ctx.createMediaElementSource(this.audio).connect(this.analyser);this.analyser.connect(this.ctx.destination);}catch{this.ctx=null;this.analyser=null;}}}
 }
 speak(text,src,{playVoice=false}={}){
  this.stop();const seq=this.seq;const p=document.querySelector('[data-seth-dialog-text]');if(!p)return;
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
