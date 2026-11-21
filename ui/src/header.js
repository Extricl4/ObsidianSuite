<script>
/* ===== header knobs: value text (INPUT / GATE / TRANSPOSE / DOUBLER / WIDTH / OUTPUT) ===== */
(function(){ let tr=0;
const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
const MINUS='−'; const sgn=(n,d)=>(n>0?'+':n<0?MINUS:'')+Math.abs(n).toFixed(d);
const dB=(x,d=1)=>!isFinite(x)?MINUS+'∞ dB':sgn(+x.toFixed(d),d)+' dB';
const ms=x=>x>=1000?(x/1000).toFixed(2)+' s':Math.round(x)+' ms';
const hz=f=>f>=1000?(f/1000).toFixed(f>=10000?1:2)+' kHz':Math.round(f)+' Hz';
/* ---- header knobs (INPUT / GATE / DOUBLER / OUTPUT) ---- */
const HEAD={INPUT:v=>dB(-12+24*v), GATE:v=>dB(-80+80*Math.pow(v,.858)), DOUBLER:v=>(24*v).toFixed(1)+' ms', OUTPUT:v=>dB(-12+24*v)};
function headKnob(k,v){ const box=k.nextElementSibling; if(!box) return false;
  if(k.dataset.width){ box.lastElementChild.textContent=Math.round(v*200)+'%'; return true; }
  if(k.dataset.tr){ const n=Math.round(-12+24*v); tr=n; box.lastElementChild.textContent=(n>0?'+':n<0?MINUS:'')+Math.abs(n)+' st';
    box.lastElementChild.style.color=n?'var(--accent)':''; const sv=(n+12)/24, r=k.querySelector('.kn-rot'); if(r) r.style.transform='rotate('+(-135+270*sv)+'deg)';
    const arc=k.querySelector('.kn-arc'), d=270*sv, a=Math.min(135,d), b=Math.max(135,d);
    if(arc) arc.style.background='conic-gradient(from 225deg, #2b2b30 0deg '+a+'deg, var(--accent) '+a+'deg '+b+'deg, #2b2b30 '+b+'deg 270deg, transparent 270deg 360deg)'; return true; } const name=box.querySelector('span'); if(!name||!HEAD[name.textContent.trim()]) return false;
  const out=box.lastElementChild; if(out&&out!==name.parentElement) out.textContent=HEAD[name.textContent.trim()](v); return true; }
window.knobHook=function(k,v){ if(!k.closest('.page')&&!k.closest('.ov')) headKnob(k,v); };
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
(function(){ let tr=0;
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
