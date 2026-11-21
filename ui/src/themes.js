<script>
/* ===== OBSIDIAN · themes ===== */
(function(){
const VARS=__VARS__;   /* [name, 'r,g,b', role] for every grey in the UI */
const THEMES=[
  {id:'obsidian', name:'Obsidian', accent:'#f3ead8', sh:1,  fn:(r,g,b)=>[r,g,b]},
  {id:'matte', name:'Matte', accent:'#4d5b6b', sh:.28, light:true, fn:(r,g,b,role)=>{ const L=lum(r,g,b);
      /* soft off-white with no gloss; text dark, text on accent white */
      let n; if(role==='k') n=.97; else if(role==='f') n=.1+(1-L)*.44; else n=L<.45?.84+.26*L:.82*(1-L)+.05;
      return hsl(40,.06,Math.min(.95,n)); }},
  {id:'midnight', name:'Midnight', accent:'#7cc4ff', sh:1, aurora:true, fn:(r,g,b)=>tint(r,g,b,220,.32)}
];
function lum(r,g,b){ return (Math.max(r,g,b)+Math.min(r,g,b))/510; }
function hsl(h,s,l){ const a=s*Math.min(l,1-l); const f=n=>{ const k=(n+h/30)%12; return Math.round(255*(l-a*Math.max(-1,Math.min(k-3,9-k,1)))); }; return [f(0),f(8),f(4)]; }
function tint(r,g,b,h,s){ const L=lum(r,g,b); return hsl(h,L<.5?s:s*.35,L); }

/* slow ambient colour glow (Midnight): green, purple and blue fading in and out */
const ast=document.createElement('style'); ast.textContent=`
#aurora{position:absolute;inset:0;pointer-events:none;z-index:4;mix-blend-mode:screen;overflow:hidden;border-radius:inherit;display:none}
#aurora i{position:absolute;width:70%;height:80%;border-radius:50%;filter:blur(60px);opacity:0;animation:aur 26s ease-in-out infinite}
#aurora i:nth-child(1){left:-15%;top:-25%;background:radial-gradient(closest-side,rgba(60,220,160,.24),transparent);animation-delay:0s}
#aurora i:nth-child(2){right:-20%;top:-10%;background:radial-gradient(closest-side,rgba(150,90,255,.24),transparent);animation-delay:-9s}
#aurora i:nth-child(3){left:15%;bottom:-35%;background:radial-gradient(closest-side,rgba(70,130,255,.22),transparent);animation-delay:-17s}
#aurora i:nth-child(4){right:5%;bottom:-30%;background:radial-gradient(closest-side,rgba(60,200,230,.16),transparent);animation-delay:-4s;animation-duration:34s}
@keyframes aur{0%,100%{opacity:.25;transform:translate(0,0) scale(1)}50%{opacity:1;transform:translate(6%,4%) scale(1.15)}}
@media (prefers-reduced-motion:reduce){#aurora i{animation:none;opacity:.6}}`;
document.head.appendChild(ast);
const aur=document.createElement('div'); aur.id='aurora'; aur.setAttribute('aria-hidden','true'); aur.innerHTML='<i></i><i></i><i></i><i></i>'; app.appendChild(aur);

function applyTheme(id,keepAccent){ const t=THEMES.find(x=>x.id===id)||THEMES[0]; const R=document.documentElement;
  VARS.forEach(([n,rgb,role])=>{ const [r,g,b]=rgb.split(',').map(Number); R.style.setProperty(n,t.fn(r,g,b,role).join(',')); });
  R.style.setProperty('--sh',t.sh); R.style.colorScheme=t.light?'light':'dark'; R.dataset.theme=t.id; aur.style.display=t.aurora?'block':'none'; const gr=$('.grain'); if(gr) gr.style.opacity=t.light?.05:'';
  ACC0=t.accent; if(!keepAccent) applyAcc(t.accent);
  $$('.thm').forEach(b=>{ const on=b.dataset.t===t.id; b.setAttribute('aria-pressed',on); b.style.boxShadow=on?'0 0 0 2px var(--accent)':'inset 0 0 0 1px rgba(255,255,255,.1)'; b.lastChild.style.color=on?'rgb(var(--cf_e9e6df))':'rgb(var(--cf_8d8d93))'; });
  return t; }

/* ---- Interface settings: drop Knob control / Show level meters / Animations, add Theme ---- */
const panel=$('.spanel[data-panel="Interface"]');
['Knob control','Show level meters','Animations'].forEach(n=>{ const sp=$$('span',panel).find(x=>x.textContent.trim()===n); if(sp) sp.parentElement.parentElement.remove(); });
const accRow=$$('span',panel).find(x=>x.textContent.trim()==='Accent colour').parentElement.parentElement;
const row=accRow.cloneNode(false); const left=accRow.firstElementChild.cloneNode(true);
left.children[0].textContent='Theme'; left.children[1].textContent='Colours for the whole plugin';
const pick=document.createElement('div'); pick.style.cssText='display: flex; align-items: center; gap: 8px';
THEMES.forEach(t=>{ const sw=t.fn(12,12,13,'b'), pn=t.fn(43,43,48,'b');
  const b=document.createElement('button'); b.className='thm'; b.dataset.t=t.id; b.setAttribute('aria-label',t.name+' theme');
  b.style.cssText='width: 66px; padding: 6px 0 5px; border: none; border-radius: 10px; background: none; cursor: pointer; display: flex; flex-direction: column; align-items: center; gap: 5px; font: inherit';
  b.innerHTML='<span style="width: 50px; height: 26px; border-radius: 7px; position: relative; overflow: hidden; background: linear-gradient(135deg, rgb('+pn+') 0 50%, rgb('+sw+') 50% 100%); box-shadow: inset 0 0 0 1px rgba(0,0,0,.25)"><span style="position: absolute; right: 6px; bottom: 5px; width: 9px; height: 9px; border-radius: 50%; background: '+t.accent+'; box-shadow: 0 0 6px '+t.accent+'"></span></span><span style="font-size: 10px; letter-spacing: .08em; color: rgb(var(--cf_8d8d93))">'+t.name+'</span>';
  pick.appendChild(b); });
row.append(left,pick); accRow.before(row);
pick.addEventListener('click',e=>{ const b=e.target.closest('.thm'); if(!b) return; const t=applyTheme(b.dataset.t);
  try{ localStorage.setItem('obsidianTheme',t.id); localStorage.setItem('obsidianAccent',t.accent); }catch(_){} toast(t.name+' theme'); });

let saved='obsidian', savedAcc=null; try{ saved=localStorage.getItem('obsidianTheme')||'obsidian'; savedAcc=localStorage.getItem('obsidianAccent'); }catch(_){}
applyTheme(saved, !!savedAcc);
window.applyTheme=applyTheme;
})();
</script>
