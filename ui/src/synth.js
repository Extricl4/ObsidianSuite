<script>
/* ===== OBSIDIAN · SYNTH controller: values, graphics and switches ===== */
(function(){
const SP=$('.page[data-page="SYNTH"]'); if(!SP) return;
const NS='http://www.w3.org/2000/svg';
const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
const MINUS='−'; const sgn=(n,d)=>(n>0?'+':n<0?MINUS:'')+Math.abs(n).toFixed(d);
const dB=(x,d=1)=>!isFinite(x)?MINUS+'∞ dB':sgn(+x.toFixed(d),d)+' dB';
const ms=x=>x>=1000?(x/1000).toFixed(2)+' s':Math.round(x)+' ms';
const hz=f=>f>=1000?(f/1000).toFixed(f>=10000?1:2)+' kHz':Math.round(f)+' Hz';
const pct=x=>Math.round(x)+'%';

/* ---- knob ids, in page order ---- */
const IDS=['gate','sens','glide','mix','out','decay','sustain','o1semi','o1det','o1lvl','uwidth','udet','o2semi','o2det','o2lvl','cutoff','res','attack','drive'];
const kn=$$('.kn',SP); const K={}; kn.forEach((k,i)=>{ if(IDS[i]){ k.dataset.sid=IDS[i]; K[IDS[i]]=k; } });
const val=id=>+K[id].dataset.v;

/* ---- value mapping (knob 0..1 -> real value) and formatting ---- */
const MAP={
  gate:   v=>-90+60*v,                      // threshold dB
  sens:   v=>v*100,
  glide:  v=>1000*v*v,                      // ms
  mix:    v=>v*100,
  out:    v=>-24+36*v,                      // dB
  decay:  v=>10+4990*Math.pow(v,2.17),      // ms
  sustain:v=>v*100,
  o1semi: v=>Math.round(-24+48*v), o2semi:v=>Math.round(-24+48*v),
  o1det:  v=>Math.round((v-.5)*200), o2det:v=>Math.round((v-.5)*200),
  o1lvl:  v=>v<=0?-Infinity:40*Math.log10(v), o2lvl:v=>v<=0?-Infinity:40*Math.log10(v),
  uwidth: v=>v*100, udet:v=>v*100,
  cutoff: v=>20*Math.pow(1000,v),           // 20 Hz .. 20 kHz
  res:    v=>v*100,
  attack: v=>1+2000*v*v*v,                  // ms
  drive:  v=>v*100
};
const FMT={
  gate:x=>dB(x), sens:pct, glide:x=>x<1?'Off':ms(x), mix:x=>pct(x)+' synth', out:x=>dB(x),
  decay:ms, sustain:pct, o1semi:x=>sgn(x,0), o2semi:x=>sgn(x,0), o1det:x=>sgn(x,0)+'%', o2det:x=>sgn(x,0)+'%',
  o1lvl:x=>dB(x), o2lvl:x=>dB(x), uwidth:pct, udet:pct, cutoff:hz, res:pct, attack:ms, drive:pct
};
/* knobs with no number under them show the value in place of the label while you turn them */
function flashLabel(k,txt){ const lab=[...k.parentElement.children].find(c=>c.tagName==='SPAN'&&!c.classList.contains('kn-val')); if(!lab) return;
  if(!lab.dataset.name) lab.dataset.name=lab.textContent; lab.textContent=txt; lab.style.color='var(--accent)';
  clearTimeout(lab._t); lab._t=setTimeout(()=>{ lab.textContent=lab.dataset.name; lab.style.color=''; },900); }

/* ---- header knobs (INPUT / GATE / DOUBLER / OUTPUT) ---- */
const HEAD={INPUT:v=>dB(-12+24*v), GATE:v=>dB(-80+80*Math.pow(v,.858)), DOUBLER:v=>(24*v).toFixed(1)+' ms', OUTPUT:v=>dB(-12+24*v)};
function headKnob(k,v){ const box=k.nextElementSibling; if(!box) return false;
  if(k.dataset.width){ box.lastElementChild.textContent=Math.round(v*200)+'%'; return true; }
  if(k.dataset.tr){ const n=Math.round(-12+24*v); tr=n; box.lastElementChild.textContent=(n>0?'+':n<0?MINUS:'')+Math.abs(n)+' st';
    box.lastElementChild.style.color=n?'var(--accent)':''; const sv=(n+12)/24, r=k.querySelector('.kn-rot'); if(r) r.style.transform='rotate('+(-135+270*sv)+'deg)';
    const arc=k.querySelector('.kn-arc'), d=270*sv, a=Math.min(135,d), b=Math.max(135,d);
    if(arc) arc.style.background='conic-gradient(from 225deg, #2b2b30 0deg '+a+'deg, var(--accent) '+a+'deg '+b+'deg, #2b2b30 '+b+'deg 270deg, transparent 270deg 360deg)'; return true; } const name=box.querySelector('span'); if(!name||!HEAD[name.textContent.trim()]) return false;
  const out=box.lastElementChild; if(out&&out!==name.parentElement) out.textContent=HEAD[name.textContent.trim()](v); return true; }

/* ---- per-knob update ---- */
let quiet=false; const BIPOLAR=['o1semi','o2semi','o1det','o2det'];
window.knobHook=function(k,v){
  if(!SP.contains(k)){ if(!k.closest('.page')) headKnob(k,v); return; }
  const id=k.dataset.sid; if(!id) return;
  const x=MAP[id](v), txt=FMT[id](x);
  if(/semi$/.test(id)){ const sv=(x+24)/48; const r=k.querySelector('.kn-rot'); if(r) r.style.transform='rotate('+(-135+270*sv)+'deg)'; }
  const vl=k.parentElement.querySelector(':scope > .kn-val');
  if(vl) vl.textContent=txt; else if(!quiet) flashLabel(k,txt);
  if(BIPOLAR.includes(id)){ const arc=k.querySelector('.kn-arc'); const d=270*v, a=Math.min(135,d), b=Math.max(135,d);
    if(arc) arc.style.background='conic-gradient(from 225deg, #2b2b30 0deg '+a+'deg, var(--accent) '+a+'deg '+b+'deg, #2b2b30 '+b+'deg 270deg, transparent 270deg 360deg)'; }
  if(id==='uwidth'||id==='udet') uni[uniSel][id==='uwidth'?'w':'d']=v;
  draw();
};

/* ---- helpers to read switches from the page ---- */
const btnOn=b=>!/background: none/.test(b.getAttribute('style')||'') && !/rgba\(255,255,255,\.04\)/.test(b.getAttribute('style')||'');
const togOn=lbl=>{ const t=$('.tog[aria-label="'+lbl+'"]',SP); return !t||t.dataset.on==='1'; };
const waveGroups=$$('button[aria-label="Sine wave"]',SP).map(b=>b.parentElement);
const WAVES=['Sine','Triangle','Saw','Square','Pulse','Noise'];
const waveOf=i=>{ const g=waveGroups[i]; const b=[...g.children].find(btnOn); return b?b.getAttribute('aria-label').replace(' wave',''):'Saw'; };
const slopeOf=()=>{ const s=$$('.seg',SP).find(x=>/dB/.test(x.textContent)); const b=[...s.children].find(btnOn); return b?parseInt(b.textContent):12; };

/* ---- graphics ---- */
const svgs={}; $$('svg',SP).forEach(s=>{ const w=s.getAttribute('width'); if(w==='134') svgs.amp=s; if(w==='108') svgs.uni=s; if(w==='200') svgs.filt=s; if(w==='170') (svgs.osc=svgs.osc||[]).push(s); });
function paint(svg,items){ svg.innerHTML=''; items.forEach(([tag,attrs,style])=>{ const e=document.createElementNS(NS,tag); for(const a in attrs) e.setAttribute(a,attrs[a]); e.setAttribute('style',style||''); svg.appendChild(e); }); }
const STROKE='stroke: var(--accent); fill: none; stroke-width: 2; stroke-linejoin: round; stroke-linecap: round';
const GHOST='stroke: var(--accent); fill: none; stroke-width: 1.4; opacity: .35; stroke-linejoin: round';
const AXIS='stroke: rgba(255,255,255,.07); stroke-width: 1; fill: none';

function rnd(seed){ let s=seed; return ()=>{ s=(s*16807)%2147483647; return s/2147483647; }; }
function waveFn(w){ return {
  Sine:p=>Math.sin(2*Math.PI*p),
  Triangle:p=>{ p=(p+.25)%1; return p<.5?4*p-1:3-4*p; },
  Saw:p=>1-2*p,
  Square:p=>p<.5?1:-1,
  Pulse:p=>p<.25?1:-1 }[w]; }
function oscPath(w,cycles,amp,seed){ const x0=10,x1=160,cy=40; const pts=[];
  if(w==='Noise'){ const r=rnd(seed||7); for(let x=x0;x<=x1;x+=2) pts.push(x+' '+(cy-(r()*2-1)*amp).toFixed(1)); return 'M'+pts.join(' L'); }
  const f=waveFn(w); const hard=(w==='Square'||w==='Pulse'||w==='Saw'); const N=hard?1200:300;
  for(let i=0;i<=N;i++){ const x=x0+(x1-x0)*i/N; const ph=(cycles*i/N)%1; pts.push(x.toFixed(2)+' '+(cy-f(ph)*amp).toFixed(1)); }
  return 'M'+pts.join(' L'); }
function drawOsc(i){ const svg=svgs.osc&&svgs.osc[i]; if(!svg) return; const p=i?'o2':'o1'; const on=togOn('Oscillator '+(i+1)+' on');
  const semi=MAP[p+'semi'](val(p+'semi')), det=MAP[p+'det'](val(p+'det')), lv=val(p+'lvl'), w=waveOf(i);
  const amp=3+21*Math.sqrt(lv);                       /* louder = taller wave */
  const items=[['path',{d:'M10 40 H160'},AXIS],['path',{d:oscPath(w,2,amp,i+3)},STROKE]];
  const pitch=(semi?sgn(semi,0)+' st':'')+(det?(semi?'  ':'')+sgn(det,0)+'%':'');   /* pitch is a number, not a shape */
  if(pitch) items.push(['text',{x:164,y:12,'text-anchor':'end'},'fill: #a3a3a8; font-size: 9px; letter-spacing: .08em; font-family: inherit']);
  paint(svg,items); if(pitch) svg.lastChild.textContent=pitch; svg.style.opacity=on?1:.3; }
function drawAmp(){ const svg=svgs.amp; if(!svg) return; const d=MAP.decay(val('decay')), s=val('sustain'); const T=3000, top=6, bot=66, pts=[];
  for(let x=2;x<=134;x+=2){ const t=(x-2)/132*T; const lvl=s+(1-s)*Math.exp(-t/(d/3)); pts.push(x+','+(bot-(bot-top)*lvl).toFixed(1)); }
  const dx=clamp(d/T*134,0,134);
  const sy=(bot-(bot-top)*s).toFixed(1);
  paint(svg,[['line',{x1:0,y1:sy,x2:134,y2:sy},'stroke: rgba(255,255,255,.12); stroke-dasharray: 2 3'],['line',{x1:dx,y1:4,x2:dx,y2:70},'stroke: rgba(255,255,255,.08); stroke-dasharray: 2 3'],['polyline',{points:'0,'+bot+' 2,'+top+' '+pts.join(' ')},STROKE]]); }
function drawUni(){ const svg=svgs.uni; if(!svg) return; const w=uni[uniSel].w, d=uni[uniSel].d; const cx=54, items=[];
  /* top: the 3 voices as pitch lines; DETUNE pulls the outer voices away from the middle one */
  const gap=2+d*30;
  items.push(['path',{d:'M8 50 H100'},AXIS]);
  [-1,0,1].forEach(o=>{ const x=cx+o*gap, top=o?16:8; items.push(['path',{d:'M'+x.toFixed(1)+' '+top+' V50'},'stroke: var(--accent); stroke-width: 2.4; stroke-linecap: round']); });
  /* bottom: stereo field; WIDTH pushes the outer voices to left and right */
  items.push(['path',{d:'M16 64 H92'},'stroke: #3a3a3f; stroke-width: 1.2']);
  [-1,0,1].forEach(o=>{ items.push(['circle',{cx:(cx+o*w*38).toFixed(1),cy:64,r:o?2.6:3},'fill: var(--accent)']); });
  items.push(['text',{x:8,y:67,'text-anchor':'middle'},'fill: #8d8d93; font-size: 8px; font-family: inherit']);
  items.push(['text',{x:100,y:67,'text-anchor':'middle'},'fill: #8d8d93; font-size: 8px; font-family: inherit']);
  paint(svg,items); const t=svg.querySelectorAll('text'); t[0].textContent='L'; t[1].textContent='R'; }
function drawFilt(){ const svg=svgs.filt; if(!svg) return; const on=togOn('Filter on'); const fc=MAP.cutoff(val('cutoff')), r=val('res'), n=slopeOf()/6, dr=val('drive');
  const fx=x=>20*Math.pow(1000,x/200), Y=db=>clamp(22-db*52/48,1,73);
  const curve=c=>{ const pts=[]; for(let x=0;x<=200;x+=2){ const f=fx(x); let db=on?-10*n*Math.log10(1+Math.pow(f/c,2)):0;
      if(on) db+=r*18*Math.exp(-Math.pow(Math.log2(f/c)/.3,2)); pts.push(x+','+Y(db).toFixed(1)); } return pts.join(' '); };
  const items=[['line',{x1:0,y1:22,x2:200,y2:22},AXIS]];
  if(on && envAmt!==0) items.push(['polyline',{points:curve(clamp(fc*Math.pow(2,envAmt/100*4),20,20000))},GHOST]);
  items.push(['polyline',{points:curve(fc)},STROKE.replace('stroke-width: 2','stroke-width: '+(2+dr*2).toFixed(2))+(dr>.02?'; filter: drop-shadow(0 0 '+(dr*5).toFixed(1)+'px var(--accent))':'')]); paint(svg,items); svg.style.opacity=on?1:.35; }
function draw(){ drawAmp(); drawOsc(0); drawOsc(1); drawUni(); drawFilt(); }

/* ---- unison: separate width / detune for OSC 1 and OSC 2 ---- */
const uniBtns=$$('button',SP).filter(b=>/^OSC [12]$/.test(b.textContent.trim()));
let uniSel=0; const uni=[{w:val('uwidth'),d:val('udet')},{w:.6,d:.18}];

/* ---- sections dim when switched off ---- */
function dimSections(){ $$('.tog',SP).forEach(t=>{ const col=t.parentElement.parentElement; const on=t.dataset.on==='1';
  [...col.children].forEach(c=>{ if(c!==t.parentElement){ c.style.opacity=on?'':'.35'; c.style.filter=on?'':'grayscale(1)'; c.style.pointerEvents=on?'':'none'; c.style.transition='opacity .15s, filter .15s'; } }); }); }

/* ---- levers (ENV RETRIGGER, PRE / POST): lever points at the lit LED ---- */
function setLever(b,right){ const lv=b.querySelector('span'); lv.style.transform='rotate('+(right?25:-25)+'deg)';
  const L=b.previousElementSibling, R=b.nextElementSibling; const lit='width: 6px; height: 6px; border-radius: 50%; background: var(--accent); box-shadow: 0 0 8px var(--accent)', off='width: 6px; height: 6px; border-radius: 50%; border: 1px solid #8d8d93';
  L.setAttribute('style',right?off:lit); R.setAttribute('style',right?lit:off); b.dataset.on=right?1:0;
  const al=b.getAttribute('aria-label'); b.title=al==='Env Retrigger'?(right?'Retrigger on: every note restarts the envelope':'Retrigger off: legato notes keep the envelope'):(right?'Post: synth after the amp':'Pre: synth before the amp'); }
const levers=$$('button[aria-label="Env Retrigger"],button[aria-label="Pre / Post"]',SP);
levers.forEach(b=>setLever(b,/background: var\(--accent\)/.test(b.nextElementSibling.getAttribute('style'))));

/* ---- arpeggiator: steps are multi-select (ROOT, 2..8 = scale degrees) ---- */
const stepGrid=$$('button',SP).find(b=>b.textContent.trim()==='ROOT').parentElement;
const stepOn='height: 26px; border-radius: 6px; border: none; background: var(--accent); color: #111113; font: inherit; font-size: SZ; font-weight: 600; letter-spacing: .06em; cursor: pointer';
const stepOff='height: 26px; border-radius: 6px; border: none; background: rgba(255,255,255,.04); color: #8d8d93; font: inherit; font-size: SZ; font-weight: 600; letter-spacing: .06em; cursor: pointer';
function setStep(b,on){ const sz=b.textContent.trim()==='ROOT'?'10px':'11px'; b.setAttribute('style',(on?stepOn:stepOff).replace('SZ',sz)); b.dataset.on=on?1:0; }
[...stepGrid.children].forEach(b=>setStep(b,btnOn(b)));

/* ---- rate slider + SYNC ---- */
const rateLbl=$$('span',SP).find(s=>s.textContent.trim()==='RATE'); const rateTrack=rateLbl.nextElementSibling.firstElementChild; const rateFill=rateTrack.firstElementChild, rateTxt=rateTrack.querySelector('span');
const syncBtn=rateLbl.nextElementSibling.querySelector('button');
const DIVS=['1/1','1/2','1/2T','1/4','1/4.','1/4T','1/8','1/8.','1/8T','1/16','1/16.','1/16T','1/32'];
let ratePos=Math.log(2/.1)/Math.log(200);
function drawRate(){ rateFill.style.width=(ratePos*100).toFixed(1)+'%'; const sync=syncBtn.dataset.on==='1';
  rateTxt.textContent=sync?DIVS[Math.round(ratePos*(DIVS.length-1))]:(.1*Math.pow(200,ratePos)).toFixed(2)+' Hz'; }
rateTrack.style.cursor='ew-resize'; rateTrack.style.touchAction='none'; rateTrack.title='Arpeggiator speed. Drag left or right.';

/* ---- ENV AMOUNT: bipolar slider ---- */
const envBar=$$('span',SP).find(s=>/^ENV AMOUNT/.test(s.textContent.trim())).parentElement; const envTxt=envBar.querySelector('span');
const envFill=document.createElement('div'); envFill.style.cssText='position:absolute;top:0;bottom:0;border-radius:6px;background:rgba(243,234,216,.14);pointer-events:none';
const envMid=document.createElement('div'); envMid.style.cssText='position:absolute;left:50%;top:4px;bottom:4px;width:1px;background:rgba(255,255,255,.18);pointer-events:none';
envBar.prepend(envMid); envBar.prepend(envFill); envTxt.style.zIndex=1; envBar.style.cursor='ew-resize'; envBar.style.touchAction='none'; envBar.title='How far the filter opens with each note. Drag left or right, double-click to reset.';
let envAmt=0;
function drawEnv(){ const a=envAmt/100; envFill.style.left=(a<0?50+a*50:50)+'%'; envFill.style.width=Math.abs(a*50)+'%'; envFill.style.boxShadow=a===0?'none':(a>0?'inset -2px 0 0 var(--accent)':'inset 2px 0 0 var(--accent)');
  envTxt.textContent='ENV AMOUNT '+(envAmt>0?'+':envAmt<0?MINUS:'')+Math.abs(envAmt)+'%'; envTxt.style.color=envAmt?'#e9e6df':'#a3a3a8'; }

/* ---- TUNING display: drag up/down, wheel, click the arrows, double-click resets ---- */
const tunLbl=$$('span',SP).find(s=>s.textContent.trim()==='TUNING'); const tunBox=tunLbl.nextElementSibling; const tunTxt=tunBox.querySelector('span'), tunArrows=tunBox.querySelector('svg');
let tuning=440; tunBox.style.cursor='ns-resize'; tunBox.style.touchAction='none'; tunBox.title='Synth tuning reference. Drag up or down, scroll, or click the arrows. Double-click resets to 440.';
const setTun=t=>{ tuning=Math.round(clamp(t,415,466)*10)/10; tunTxt.textContent=tuning.toFixed(1); };

/* ---- pointer handling for the sliders ---- */
let sd=null;
SP.addEventListener('pointerdown',e=>{
  if(rateTrack.contains(e.target)){ sd={t:'rate'}; } else if(envBar.contains(e.target)){ sd={t:'env'}; }
  else if(tunBox.contains(e.target)){ if(tunArrows.contains(e.target)){ const r=tunArrows.getBoundingClientRect(); setTun(tuning+((e.clientY<r.top+r.height/2)?.1:-.1)); return; } sd={t:'tun',y:e.clientY,v:tuning}; }
  else return; e.preventDefault(); sdMove(e); });
function sdMove(e){ if(!sd) return;
  if(sd.t==='rate'){ const r=rateTrack.getBoundingClientRect(); ratePos=clamp((e.clientX-r.left)/r.width,0,1); drawRate(); }
  if(sd.t==='env'){ const r=envBar.getBoundingClientRect(); let a=Math.round(clamp(((e.clientX-r.left)/r.width-.5)*2,-1,1)*100); if(Math.abs(a)<3) a=0; envAmt=a; drawEnv(); drawFilt(); }
  if(sd.t==='tun'){ const sc=app.getBoundingClientRect().width/1360; setTun(sd.v+(sd.y-e.clientY)/sc*.1); } }
addEventListener('pointermove',sdMove); addEventListener('pointerup',()=>sd=null);
tunBox.addEventListener('wheel',e=>{ e.preventDefault(); setTun(tuning+(e.deltaY<0?.1:-.1)); },{passive:false});
tunBox.addEventListener('dblclick',()=>setTun(440));
envBar.addEventListener('dblclick',()=>{ envAmt=0; drawEnv(); drawFilt(); });

/* ---- clicks (runs after the main handler, which has already flipped toggles / segment buttons) ---- */
document.addEventListener('click',e=>{ const b=e.target.closest('button'); if(!b||!SP.contains(b)) return;
  if(b.parentElement===stepGrid){ const on=b.dataset.on!=='1'; const count=[...stepGrid.children].filter(x=>x.dataset.on==='1').length;
    if(!on&&count<=1){ toast('Keep at least one step on'); return; } setStep(b,on); return; }
  if(uniBtns.includes(b)){ uniSel=uniBtns.indexOf(b); quiet=true; setKnob(K.uwidth,uni[uniSel].w); setKnob(K.udet,uni[uniSel].d); quiet=false; draw(); return; }
  if(b===syncBtn){ drawRate(); return; }
  if(b.classList.contains('tog')){ dimSections(); draw(); return; }
  draw();
});
/* the main handler swaps lever rotation itself; undo that so only setLever drives it */
document.addEventListener('click',e=>{ const b=e.target.closest('button'); if(b&&levers.includes(b)){ setLever(b,b.dataset.on!=='1'); e.stopImmediatePropagation(); } },true);

/* ---- lock each knob's column width so changing numbers never shifts the knobs ---- */
kn.forEach(k=>{ const c=k.parentElement; const w=Math.max(c.offsetWidth,84); c.style.width=w+'px'; c.style.flex='none'; c.style.boxSizing='border-box';
  [...c.children].forEach(x=>{ if(x.tagName==='SPAN'){ x.style.whiteSpace='nowrap'; x.style.textAlign='center'; } }); });
/* ---- starting values: line knob positions up with the numbers already printed ---- */
K.o1lvl.dataset.v=Math.pow(10,-7.5/40).toFixed(3); K.cutoff.dataset.v=Math.log(4480/20)/Math.log(1000); K.res.dataset.v=0;
quiet=true; kn.forEach(k=>setKnob(k,+k.dataset.v)); quiet=false;
$$('.kn').forEach(k=>{ if(k.dataset.def===undefined) k.dataset.def=k.dataset.v; });
dimSections(); drawRate(); drawEnv(); draw();

/* ---- plugin hooks: every synth setting as a 0..1 value, keyed by the plugin's parameter id.
        Used by plugin/WebUI/obsidian-bridge.js; the browser demo doesn't need them. ---- */
const SID={gate:'syn.gate',sens:'syn.sens',glide:'syn.glide',mix:'syn.mix',out:'syn.out',decay:'syn.decay',sustain:'syn.sustain',
  o1semi:'syn.o1.semi',o1det:'syn.o1.det',o1lvl:'syn.o1.level',o2semi:'syn.o2.semi',o2det:'syn.o2.det',o2lvl:'syn.o2.level',
  cutoff:'syn.flt.cutoff',res:'syn.flt.res',attack:'syn.flt.attack',drive:'syn.flt.drive'};
const TOGS={'syn.o1.on':'Oscillator 1 on','syn.o2.on':'Oscillator 2 on','syn.flt.on':'Filter on','syn.arp.on':'Arpeggiator on'};
const segOf=re=>$$('.seg',SP).find(s=>re.test(s.textContent));
const kids=g=>g?[...g.children].filter(x=>x.tagName==='BUTTON'):[];
const activeIn=g=>{ const b=kids(g); let i=b.findIndex(x=>typeof isAct2==='function'&&g.classList.contains('seg')?isAct2(x):false);
  if(i<0){ const o=typeof oddOne==='function'?oddOne(b):null; i=o?b.indexOf(o):-1; } if(i<0) i=b.findIndex(btnOn); return Math.max(0,i); };
const pickIn=(g,i)=>{ const b=kids(g)[i]; if(b&&activeIn(g)!==i) b.click(); };
const groups=()=>({ scale:segOf(/MAJ/), oct:segOf(/\+2/), slope:segOf(/dB/), pat:$('button[aria-label="Up"]',SP)&&$('button[aria-label="Up"]',SP).parentElement });
const keyBtn=()=>$$('button',SP).find(b=>b.querySelector('path[d="M6 9l6 6 6-6"]')&&/^[A-G]#?$/.test(ddText(b)));
const KEYS=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
const lever=l=>$('button[aria-label="'+l+'"]',SP);
window.obsSynth={
  read(){ const o={}; for(const id in SID){ const v=val(id); o[SID[id]]=/semi$/.test(id)?(MAP[id](v)+24)/48:v; }
    o['syn.u1.width']=uni[0].w; o['syn.u1.det']=uni[0].d; o['syn.u2.width']=uni[1].w; o['syn.u2.det']=uni[1].d;
    for(const id in TOGS) o[id]=togOn(TOGS[id])?1:0;
    o['syn.o1.wave']=WAVES.indexOf(waveOf(0))/5; o['syn.o2.wave']=WAVES.indexOf(waveOf(1))/5;
    const g=groups(); o['syn.flt.slope']=activeIn(g.slope)/3; o['syn.arp.scale']=activeIn(g.scale); o['syn.arp.oct']=activeIn(g.oct)/4; o['syn.arp.pattern']=activeIn(g.pat)/4;
    const kb=keyBtn(); o['syn.arp.key']=Math.max(0,KEYS.indexOf(kb?ddText(kb):'C'))/11;
    [...stepGrid.children].forEach((b,i)=>o['syn.arp.s'+(i+1)]=b.dataset.on==='1'?1:0);
    o['syn.arp.rate']=ratePos; o['syn.arp.sync']=syncBtn.dataset.on==='1'?1:0;
    o['syn.flt.env']=(envAmt+100)/200; o['syn.tune']=(tuning-415)/51;
    o['syn.retrig']=lever('Env Retrigger').dataset.on==='1'?1:0; o['syn.post']=lever('Pre / Post').dataset.on==='1'?1:0;
    return o; },
  write(vals){ quiet=true; const g=groups();
    for(const id in vals){ const v=+vals[id]; if(!isFinite(v)) continue;
      const sid=Object.keys(SID).find(k=>SID[k]===id);
      if(sid){ setKnob(K[sid], /semi$/.test(sid)?clamp(Math.round(v*48),0,48)/48:v); continue; }
      const m=id.match(/^syn\.u([12])\.(width|det)$/);
      if(m){ const i=+m[1]-1; uni[i][m[2]==='width'?'w':'d']=v; if(uniSel===i) setKnob(m[2]==='width'?K.uwidth:K.udet,v); continue; }
      if(TOGS[id]){ const t=$('.tog[aria-label="'+TOGS[id]+'"]',SP); if(t) setTog(t,v>=.5); continue; }
      const w=id.match(/^syn\.o([12])\.wave$/); if(w){ const b=kids(waveGroups[+w[1]-1])[clamp(Math.round(v*5),0,5)]; if(b&&!btnOn(b)) b.click(); continue; }
      if(id==='syn.flt.slope'){ pickIn(g.slope,clamp(Math.round(v*3),0,3)); continue; }
      if(id==='syn.arp.scale'){ pickIn(g.scale,v>=.5?1:0); continue; }
      if(id==='syn.arp.oct'){ pickIn(g.oct,clamp(Math.round(v*4),0,4)); continue; }
      if(id==='syn.arp.pattern'){ pickIn(g.pat,clamp(Math.round(v*4),0,4)); continue; }
      if(id==='syn.arp.key'){ const kb=keyBtn(); if(kb) setDDText(kb,KEYS[clamp(Math.round(v*11),0,11)]); continue; }
      const s=id.match(/^syn\.arp\.s([1-8])$/); if(s){ setStep(stepGrid.children[+s[1]-1],v>=.5); continue; }
      if(id==='syn.arp.rate'){ ratePos=clamp(v,0,1); continue; }
      if(id==='syn.arp.sync'){ if((syncBtn.dataset.on==='1')!==(v>=.5)) syncBtn.click(); continue; }
      if(id==='syn.flt.env'){ envAmt=clamp(Math.round(v*200-100),-100,100); continue; }
      if(id==='syn.tune'){ setTun(415+51*v); continue; }
      if(id==='syn.retrig'){ setLever(lever('Env Retrigger'),v>=.5); continue; }
      if(id==='syn.post'){ setLever(lever('Pre / Post'),v>=.5); continue; }
    }
    quiet=false; dimSections(); drawRate(); drawEnv(); draw(); }
};
})();

/* ===== all knobs: scroll to fine-tune, double-click to reset ===== */
document.addEventListener('wheel',e=>{ const k=e.target.closest('.kn'); if(!k) return; e.preventDefault();
  setKnob(k,Math.max(0,Math.min(1,+k.dataset.v+(e.deltaY<0?1:-1)*(k.dataset.tr?1/24:(e.shiftKey?.005:.02))))); },{passive:false});
document.addEventListener('dblclick',e=>{ const k=e.target.closest('.kn'); if(!k) return; setKnob(k,+(k.dataset.def!==undefined?k.dataset.def:.5)); });
$$('.kn').forEach(k=>{ if(k.dataset.def===undefined) k.dataset.def=k.dataset.v; });
/* header GATE / DOUBLER: grey out knob, name and value when switched off */
function headTog(t){ const col=t.parentElement.parentElement, grp=col.parentElement, on=t.dataset.on==='1';
  [grp.querySelector('.kn'), t.previousElementSibling, col.lastElementChild].forEach(e=>{ if(!e) return;
    e.style.transition='opacity .15s, filter .15s'; e.style.opacity=on?'':'.35'; e.style.filter=on?'':'grayscale(1)'; }); }
const headTogs=$$('.tog[aria-label="Gate on"],.tog[aria-label="Doubler on"]');
headTogs.forEach(headTog);
document.addEventListener('click',e=>{ const t=e.target.closest('.tog'); if(t&&headTogs.includes(t)) headTog(t); });
/* header numbers follow their knobs */
$$('.kn').filter(k=>!k.closest('.page')&&!k.closest('.ov')).forEach(k=>{ const n=k.nextElementSibling&&k.nextElementSibling.querySelector('span'); if(n&&n.textContent.trim()==='OUTPUT'){ k.dataset.v=k.dataset.def=((4.7+12)/24).toFixed(4); setKnob(k,+k.dataset.v); } else knobHook(k,+k.dataset.v); });
</script>
<script>
/* ===== TRANSPOSE knob in the header, between GATE and the page tabs ===== */
(function(){
const td=$('[aria-label="Transpose down"]'); if(!td) return;
let t0=parseInt(td.parentElement.children[1].children[0].textContent.replace('−','-'),10); if(isNaN(t0)) t0=0;
td.closest('div[style*="justify-content: flex-end"]').replaceChildren();          // old top-right control goes
const hk=$$('.kn').filter(k=>!k.closest('.page')&&!k.closest('.ov'));
const inG=hk[0].parentElement, gateG=hk[1].parentElement, left=inG.parentElement;
const g=inG.cloneNode(true); const k=g.querySelector('.kn'), col=g.children[1];
col.querySelector('span').textContent='TRANSPOSE'; col.style.minWidth='0';
gateG.after(g);
/* tighter spacing: GATE sits closer to INPUT, and both sides of the bar match */
const right=hk[2].parentElement.parentElement;
[left,right].forEach(c=>c.style.gap='9px'); right.style.gap='7px';
hk.concat([k]).forEach(x=>{ x.style.width=x.style.height='54px'; x.parentElement.style.gap='8px';
  const c=x.parentElement.children[1]; if(c&&c!==col) c.style.minWidth='60px'; });
/* tabs a little tighter so both sides fit three dials */
const tabs=$('#app button[aria-label],#app button') && hk[0].closest('div[style*="height: 100px"]').children[1];
[...tabs.children].forEach(b=>{ b.style.minWidth='52px'; b.style.padding='0 4px'; }); tabs.style.gap='0px';
/* right side: OUTPUT hugs the meter, DOUBLER follows, and an open slot waits for a 3rd dial */
right.style.justifyContent='flex-end';
/* WIDTH dial between DOUBLER and OUTPUT: 0% mono, 100% normal, 200% extra wide */
const outG=hk[3].parentElement, wg=outG.cloneNode(true), wk=wg.querySelector('.kn');
wg.children[1].querySelector('span').textContent='WIDTH'; wk.dataset.width='1'; outG.before(wg);
try{ KTIP.WIDTH='Stereo width of the whole plugin: 0% is mono, 100% is normal, 200% is extra wide. Double-click resets.'; }catch(_){}
wk.dataset.v=wk.dataset.def=.5; setKnob(wk,.5);
const slot=document.createElement('div'); slot.className='hdr-slot'; slot.setAttribute('aria-hidden','true');
slot.style.cssText='width: 0; height: 54px; flex: 1 1 0';
right.children[0].after(slot);
[...right.children].forEach(c=>{ if(c.tagName==='SPAN'&&c.getBoundingClientRect().width>8&&c.getBoundingClientRect().width<30&&!c.children.length) c.remove(); });
if(window.KTIP) KTIP.TRANSPOSE='Transpose: shift everything up or down in semitones. Double-click resets to 0.';
try{ KTIP.TRANSPOSE='Transpose: shift everything up or down in semitones. Double-click resets to 0.'; }catch(_){}
k.dataset.tr='1';
window.setTranspose=n=>setKnob(k,(Math.max(-12,Math.min(12,Math.round(n)))+12)/24);
k.dataset.v=k.dataset.def=(t0+12)/24; setKnob(k,+k.dataset.v); k.dataset.def=.5;
})();
</script>
