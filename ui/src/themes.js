<script>
/* =============================================================================
   OBSIDIAN · themes
   A theme changes the colours (every grey in the UI is a CSS variable), the
   accent, the fonts (logo, headings, text) and adds details: a decorative band
   under the header and above the footer, and a moving 2D layer (snow, embers,
   petals, fog, torch light, a neon grid). The moving layer can be switched off.
   Fonts are in ui/fonts (SIL Open Font License, see ui/fonts/OFL.txt).
   ============================================================================= */
(function(){
const VARS=__VARS__;   /* [name, 'r,g,b', role] for every grey in the UI */
const R=document.documentElement;
function lum(r,g,b){ return (Math.max(r,g,b)+Math.min(r,g,b))/510; }
function hsl(h,s,l){ const a=s*Math.min(l,1-l); const f=n=>{ const k=(n+h/30)%12; return Math.round(255*(l-a*Math.max(-1,Math.min(k-3,9-k,1)))); }; return [f(0),f(8),f(4)]; }
function tint(r,g,b,h,s){ const L=lum(r,g,b); return hsl(h,L<.5?s:s*.35,L); }
/* a dark theme from a hue: backgrounds tinted (darker greys more), text in its own hue */
function tone(o){ return (r,g,b,role)=>{ const L=lum(r,g,b);
  if(role==='k') return [r,g,b];
  if(role==='f') return hsl(o.fh??o.h, o.fs??.1, Math.min(.97, L*(o.fl||1)));
  const s=L<.22?o.s:L<.5?o.s*.6:o.s*.25;
  return hsl(o.h, s, Math.min(.98, L*(o.bl||1))); }; }

/* ---- fonts ---- */
const FONTS=[['Cinzel','cinzel-500',500],['Cinzel','cinzel-700',700],['Cormorant Garamond','cormorant-garamond-500',500],['Cormorant Garamond','cormorant-garamond-600',600],
  ['IM Fell English','im-fell-english-400',400],['Marcellus','marcellus-400',400],['MedievalSharp','medievalsharp-400',400],['Orbitron','orbitron-500',500],
  ['Pirata One','pirata-one-400',400],['Rajdhani','rajdhani-500',500],['Rajdhani','rajdhani-600',600],['Uncial Antiqua','uncial-antiqua-400',400],['UnifrakturMaguntia','unifrakturmaguntia-400',400]];
const BASE="'Saira', 'Segoe UI', system-ui, sans-serif";

/* ---- the themes ---- */
const THEMES=[
  {id:'obsidian', name:'Obsidian', note:'The original', accent:'#f3ead8', sh:1, fn:(r,g,b)=>[r,g,b]},
  {id:'matte', name:'Matte', note:'Light and calm', accent:'#4d5b6b', sh:.28, light:true, fn:(r,g,b,role)=>{ const L=lum(r,g,b);
      let n; if(role==='k') n=.97; else if(role==='f') n=.1+(1-L)*.44; else n=L<.45?.84+.26*L:.82*(1-L)+.05;
      return hsl(40,.06,Math.min(.95,n)); }},
  {id:'midnight', name:'Midnight', note:'Blue night, aurora', accent:'#7cc4ff', sh:1, aurora:true, fn:(r,g,b)=>tint(r,g,b,220,.32)},
  {id:'gothic', name:'Gothic', note:'Cathedral black, blood red', accent:'#c4233e', sh:1, fn:tone({h:332,s:.16,bl:.86,fh:12,fs:.1}),
    font:{logo:'UnifrakturMaguntia', head:'Cormorant Garamond', body:'Cormorant Garamond', w:600, adj:.45, headAdj:.4, logoCase:'Obsidian', logoLs:'.06em'}, fx:'fog', band:'arches'},
  {id:'roman', name:'Roman', note:'Imperial purple and gold', accent:'#d9b45e', sh:1, fn:tone({h:288,s:.2,bl:1.04,fh:44,fs:.3}),
    font:{logo:'Cinzel', head:'Cinzel', body:'Marcellus', w:500, adj:.5, headAdj:.46, logoLs:'.3em', numBase:true}, fx:'gold', band:'meander'},
  {id:'norse', name:'Norse', note:'Frost, runes and aurora', accent:'#7fe0c0', sh:1, aurora:true, fn:tone({h:208,s:.17,bl:.97,fh:195,fs:.12}),
    font:{logo:'Uncial Antiqua', head:'Marcellus', body:'Marcellus', w:400, adj:.5, logoCase:'Obsidian', logoLs:'.12em', numBase:true}, fx:'snow', band:'runes'},
  {id:'medieval', name:'Medieval', note:'Leather, parchment, torchlight', accent:'#e8a33c', sh:1, fn:tone({h:28,s:.26,bl:1,fh:40,fs:.34}),
    font:{logo:'MedievalSharp', head:'MedievalSharp', body:'IM Fell English', w:400, adj:.44, headAdj:'none', logoCase:'Obsidian', logoLs:'.1em'}, fx:'torch', band:'braid'},
  {id:'neon', name:'Neon', note:'Synthwave grid', accent:'#ff3fb4', sh:1, fn:tone({h:262,s:.42,bl:1.08,fh:288,fs:.22}),
    font:{logo:'Orbitron', head:'Orbitron', body:'Rajdhani', w:600, adj:.5, headAdj:.42, headLs:'.1em', logoLs:'.28em'}, fx:'grid', band:'neon'},
  {id:'sakura', name:'Sakura', note:'Plum night, falling petals', accent:'#f6a5c0', sh:1, fn:tone({h:334,s:.2,bl:1.04,fh:340,fs:.18}),
    font:{logo:'Cormorant Garamond', head:'Cormorant Garamond', body:'Cormorant Garamond', w:600, adj:.45, headAdj:.4, logoCase:'Obsidian', logoLs:'.18em'}, fx:'petals', band:'blossom'},
  {id:'inferno', name:'Inferno', note:'Embers and ash', accent:'#ff6a2a', sh:1, fn:tone({h:12,s:.3,bl:.95,fh:28,fs:.16}),
    font:{logo:'Pirata One', head:'Rajdhani', body:'Rajdhani', w:600, adj:.5, logoCase:'Obsidian', logoLs:'.1em'}, fx:'embers', band:'flame'},
];

/* ---- styles: fonts, aurora, bands, fx layer, theme cards ---- */
const ast=document.createElement('style');
ast.textContent=FONTS.map(([f,file,w])=>`@font-face{font-family:'${f}';src:url(fonts/${file}.woff2) format('woff2');font-weight:${w};font-style:normal;font-display:swap}`).join('\n')+`
#aurora{position:absolute;inset:0;pointer-events:none;z-index:4;mix-blend-mode:screen;overflow:hidden;border-radius:inherit;display:none}
#aurora i{position:absolute;width:70%;height:80%;border-radius:50%;filter:blur(60px);opacity:0;animation:aur 26s ease-in-out infinite}
#aurora i:nth-child(1){left:-15%;top:-25%;background:radial-gradient(closest-side,rgba(60,220,160,.24),transparent);animation-delay:0s}
#aurora i:nth-child(2){right:-20%;top:-10%;background:radial-gradient(closest-side,rgba(150,90,255,.24),transparent);animation-delay:-9s}
#aurora i:nth-child(3){left:15%;bottom:-35%;background:radial-gradient(closest-side,rgba(70,130,255,.22),transparent);animation-delay:-17s}
#aurora i:nth-child(4){right:5%;bottom:-30%;background:radial-gradient(closest-side,rgba(60,200,230,.16),transparent);animation-delay:-4s;animation-duration:34s}
html[data-theme="norse"] #aurora i:nth-child(2){background:radial-gradient(closest-side,rgba(80,230,190,.2),transparent)}
html[data-theme="norse"] #aurora i:nth-child(3){background:radial-gradient(closest-side,rgba(120,200,255,.16),transparent)}
@keyframes aur{0%,100%{opacity:.25;transform:translate(0,0) scale(1)}50%{opacity:1;transform:translate(6%,4%) scale(1.15)}}
#themefx{position:absolute;inset:0;width:100%;height:100%;pointer-events:none;z-index:4;mix-blend-mode:screen;border-radius:inherit;display:none}
.thband{position:absolute;left:0;right:0;height:12px;pointer-events:none;z-index:3;display:none;color:var(--accent);opacity:.55}
.thband svg{display:block;width:100%;height:100%}
html.thx .thband{display:block}
.thgrid{display:grid;grid-template-columns:repeat(5,1fr);gap:8px 10px;margin-top:10px}
.thc{border:none;padding:0;background:none;cursor:pointer;display:flex;flex-direction:column;gap:6px;align-items:stretch;font:inherit;text-align:left;border-radius:12px}
.thc .sw{position:relative;height:42px;border-radius:10px;overflow:hidden;box-shadow:inset 0 0 0 1px rgba(255,255,255,.08)}
.thc .sw b{position:absolute;left:10px;bottom:5px;font-size:20px;font-weight:400;line-height:1}
.thc .sw i{position:absolute;right:9px;top:9px;width:9px;height:9px;border-radius:50%}
.thc .sw em{position:absolute;left:0;right:0;top:0;height:14px;opacity:.9}
.thc span{font-size:11px;color:rgb(var(--cf_e9e6df));padding-left:2px}
.thc small{font-size:9.5px;color:rgb(var(--cf_8d8d93));padding-left:2px;margin-top:-4px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.thc[aria-pressed="true"] .sw{box-shadow:0 0 0 2px var(--accent)}
.thc:hover .sw{box-shadow:0 0 0 1px rgba(255,255,255,.3)}
.thc[aria-pressed="true"]:hover .sw{box-shadow:0 0 0 2px var(--accent)}
@media (prefers-reduced-motion:reduce){#aurora i{animation:none;opacity:.6}}`;
document.head.appendChild(ast);
const fontCss=document.createElement('style'); document.head.appendChild(fontCss);
const app=document.getElementById('app');
const aur=document.createElement('div'); aur.id='aurora'; aur.setAttribute('aria-hidden','true'); aur.innerHTML='<i></i><i></i><i></i><i></i>'; app.appendChild(aur);
const fxc=document.createElement('canvas'); fxc.id='themefx'; fxc.setAttribute('aria-hidden','true'); app.appendChild(fxc);

/* ---- font rules for a theme ---- */
const LS=['.14','.16','.18','.2','.22','.24','.26','.3'].flatMap(v=>['[style*="letter-spacing: '+v+'em"]','[style*="letter-spacing: 0'+v+'em"]']);
const HEAD_SEL=[...LS,'.sy-sh h3','.sy-kl','.sy-lbl','.sy-chip','.sy-seg button','.sy-tab','.sy-pill .l','.sy-btn .tx b','.sy-eng','.sy-sel .opt','.sy-bat span','.sy-pop h4',
  '.obs-tile i','.obs-role','.obs-based','.obs-btn','.obs-crumb span'].map(s=>'#app '+s).join(',');
const BIG_SEL=['.obs-ampname','[style*="font-size: 110px"]'].map(s=>'#app '+s).join(',');
function setFonts(t){
  const f=t.font;
  if(!f){ fontCss.textContent=''; return; }
  const q=n=>"'"+n+"', "+BASE;
  fontCss.textContent=`
#app,#app button,#app input,#app textarea{font-family:${q(f.body)}!important;font-size-adjust:${f.adj||'none'}!important;font-synthesis-weight:none}
${HEAD_SEL}{font-family:${q(f.head)}!important;font-size-adjust:${f.headAdj||f.adj||'none'}!important;${f.headLs?'letter-spacing:'+f.headLs+'!important;':''}}
${BIG_SEL}{font-family:${q(f.logo)}!important;font-size-adjust:none!important}
#app .sy-badge b{font-family:${q(f.logo)}!important;font-size-adjust:none!important;font-weight:400!important}
#app .thlogo{font-family:${q(f.logo)}!important;font-size-adjust:none!important;letter-spacing:${f.logoLs||'.3em'}!important;font-weight:400!important}
#app .thc .sw b{font-size-adjust:none!important}
${f.numBase?'#app [style*="tabular-nums"],#app .kn-val,#app .sy-kv,#app .sy-val,#app .sy-pill .v{font-family:'+BASE+'!important;font-size-adjust:none!important}':''}`;
}
/* logo text: some fonts read better as "Obsidian" than "OBSIDIAN" */
const logos=[...app.querySelectorAll('span')].filter(s=>s.textContent.trim()==='OBSIDIAN'&&!s.children.length);
logos.forEach(s=>{ s.classList.add('thlogo'); s.dataset.txt='OBSIDIAN'; });

/* ---- decorative bands under the header and above the footer ---- */
const bandTop=document.createElement('div'), bandBot=document.createElement('div');
bandTop.className=bandBot.className='thband'; app.append(bandTop,bandBot);
const RUNES=['M2 0V12M2 2L7 5M2 5L7 8','M2 0V12M7 0V12M2 0L7 4','M2 0V12M2 3L6 6L2 9','M2 0V12M2 0L7 6L2 12','M4 0V12M1 3L7 9','M1 0L7 12M7 0L1 12','M1 6L4 0L7 6L4 12Z','M4 0V12M4 0L1 3M4 0L7 3','M1 0V12M7 0V12M1 6H7','M4 0V12M1 9L4 6L7 9'];
function bandSvg(kind,flip){
  const W=1376, H=12; let p='';
  if(kind==='meander'){ for(let x=0;x<W;x+=16) p+=`<path d="M${x} 11V1H${x+12}V8H${x+4}V4H${x+8}" fill="none" stroke="currentColor" stroke-width="1.2"/>`; p+=`<path d="M0 11.5H${W}" stroke="currentColor" stroke-width=".6" opacity=".6"/>`; }
  else if(kind==='runes'){ let i=0; for(let x=24;x<W-20;x+=22,i++) p+=`<path transform="translate(${x} 0)" d="${RUNES[(i*7)%RUNES.length]}" fill="none" stroke="currentColor" stroke-width="1.1" stroke-linecap="round" opacity="${i%5?'.75':'1'}"/>`; }
  else if(kind==='arches'){ for(let x=0;x<W;x+=18) p+=`<path d="M${x} 12V7Q${x} 1 ${x+9} 0Q${x+18} 1 ${x+18} 7V12" fill="none" stroke="currentColor" stroke-width="1"/><circle cx="${x+9}" cy="6" r="1.3" fill="currentColor" opacity=".7"/>`; }
  else if(kind==='braid'){ for(let x=0;x<W;x+=14) p+=`<path d="M${x} 3C${x+4} 3 ${x+10} 9 ${x+14} 9M${x} 9C${x+4} 9 ${x+10} 3 ${x+14} 3" fill="none" stroke="currentColor" stroke-width="1.1"/>`; }
  else if(kind==='neon'){ p=`<defs><filter id="ng${flip?1:0}"><feGaussianBlur stdDeviation="1.6"/></filter></defs><path d="M0 6H${W}" stroke="currentColor" stroke-width="3" filter="url(#ng${flip?1:0})"/><path d="M0 6H${W}" stroke="currentColor" stroke-width="1"/>`; for(let x=40;x<W;x+=120) p+=`<path d="M${x} 2V10" stroke="currentColor" stroke-width="1" opacity=".6"/>`; }
  else if(kind==='blossom'){ p=`<path d="M0 6H${W}" stroke="currentColor" stroke-width=".6" opacity=".45"/>`; for(let x=30;x<W;x+=70+((x*7)%40)){ const c=(dx,dy)=>`<ellipse cx="${x+dx}" cy="${6+dy}" rx="2.2" ry="1.2" transform="rotate(${dx*20+dy*30} ${x+dx} ${6+dy})" fill="currentColor"/>`; p+=c(-2.4,0)+c(2.4,0)+c(0,-2.4)+c(-1.6,2)+c(1.6,2)+`<circle cx="${x}" cy="6" r="1" fill="#fff" opacity=".7"/>`; } }
  else if(kind==='flame'){ let d='M0 12'; for(let x=0;x<=W;x+=10) d+=` Q${x+5} ${(x/10)%3?2:5} ${x+10} 12`; p=`<path d="${d}" fill="currentColor" opacity=".55"/>`; }
  return `<svg viewBox="0 0 ${W} ${H}" preserveAspectRatio="none"${flip?' style="transform:scaleY(-1)"':''}>${p}</svg>`;
}
function placeBands(){
  const hdr=document.querySelector('#app > div > div[style*="padding: 0 14px"]')||[...app.querySelectorAll('div')].find(d=>/padding: 0 14px/.test(d.getAttribute('style')||'')&&d.offsetHeight>150);
  const foot=[...app.querySelectorAll('div')].reverse().find(d=>d.offsetHeight>30&&d.offsetHeight<70&&/TUNER/.test(d.textContent)&&/SETTINGS/.test(d.textContent));
  const ar=app.getBoundingClientRect(), sc=ar.width/app.offsetWidth||1;
  if(hdr){ const r=hdr.getBoundingClientRect(); bandTop.style.top=((r.bottom-ar.top)/sc-6)+'px'; }
  if(foot){ const r=foot.getBoundingClientRect(); bandBot.style.top=((r.top-ar.top)/sc-6)+'px'; }
}

/* ---- the moving layer ---- */
let fxOn=true, fxKind=null, fxRaf=0, parts=[], lastT=0, fxT=0;
try{ fxOn=localStorage.getItem('obsidianThemeFx')!=='0'; }catch(_){}
const rnd=(a,b)=>a+Math.random()*(b-a);
function seed(kind,w,h){
  const n={snow:70,embers:46,petals:22,dust:46,gold:26,fog:7,torch:40,grid:0}[kind]||0; parts=[];
  for(let i=0;i<n;i++) parts.push(newPart(kind,w,h,true));
}
function newPart(kind,w,h,anywhere){
  const p={x:rnd(0,w),y:anywhere?rnd(0,h):0,t:rnd(0,100)};
  if(kind==='snow'){ p.r=rnd(.6,1.9); p.vy=rnd(8,22); p.vx=rnd(-4,4); p.a=rnd(.25,.6); if(!anywhere) p.y=-4; }
  if(kind==='embers'){ p.r=rnd(.7,2); p.vy=-rnd(14,40); p.vx=rnd(-6,6); p.a=rnd(.4,.9); p.life=rnd(5,14); if(!anywhere) p.y=h+4; }
  if(kind==='petals'){ p.r=rnd(3,5.5); p.vy=rnd(10,24); p.vx=rnd(8,20); p.rot=rnd(0,6.28); p.vr=rnd(-1.2,1.2); p.a=rnd(.35,.7); if(!anywhere){ p.y=-8; p.x=rnd(-200,w); } }
  if(kind==='dust'||kind==='torch'||kind==='gold'){ p.r=rnd(.5,1.5); p.vy=rnd(-3,3); p.vx=rnd(-3,3); p.a=rnd(.15,.45); }
  if(kind==='fog'){ p.r=rnd(160,320); p.y=rnd(h*.45,h*1.05); p.vy=0; p.vx=rnd(4,10)*(Math.random()<.5?-1:1); p.a=rnd(.035,.07); }
  return p;
}
function fxFrame(now){
  fxRaf=requestAnimationFrame(fxFrame);
  if(now-lastT<33) return;                         /* ~30 fps is plenty */
  const dt=Math.min(.1,(now-lastT)/1000); lastT=now; fxT+=dt;
  const w=app.offsetWidth, h=app.offsetHeight;
  if(fxc.width!==w||fxc.height!==h){ fxc.width=w; fxc.height=h; seed(fxKind,w,h); }
  const g=fxc.getContext('2d'); g.clearRect(0,0,w,h);
  const acc=getComputedStyle(R).getPropertyValue('--accent').trim()||'#ffffff';
  if(fxKind==='grid'){
    const hz=h*.62, sp=(fxT*30)%40; g.strokeStyle=acc; g.lineWidth=1;
    for(let i=0;i<14;i++){ const y=hz+Math.pow((i*40+sp)/560,2)*(h-hz); g.globalAlpha=.05+.07*(y-hz)/(h-hz); g.beginPath(); g.moveTo(0,y); g.lineTo(w,y); g.stroke(); }
    for(let i=-16;i<=16;i++){ g.globalAlpha=.06; g.beginPath(); g.moveTo(w/2+i*28,hz); g.lineTo(w/2+i*170,h); g.stroke(); }
    const gr=g.createLinearGradient(0,hz-60,0,hz+6); gr.addColorStop(0,'rgba(0,0,0,0)'); gr.addColorStop(1,acc); g.globalAlpha=.06; g.fillStyle=gr; g.fillRect(0,hz-60,w,66);
    g.globalAlpha=1; return;
  }
  if(fxKind==='fog'||fxKind==='torch'){
    /* candle / torch flicker from the sides */
    const fl=.5+.5*Math.sin(fxT*7.3)*Math.sin(fxT*3.1+1)+.15*Math.sin(fxT*17);
    const c=fxKind==='fog'?'255,120,90':'255,160,70';
    [[0,h*.35],[w,h*.55]].forEach(([x,y],k)=>{ const r=g.createRadialGradient(x,y,0,x,y,w*.45); r.addColorStop(0,`rgba(${c},${(fxKind==='fog'?.05:.08)*(k?1.1-fl*.3:.7+fl*.3)})`); r.addColorStop(1,`rgba(${c},0)`); g.fillStyle=r; g.fillRect(0,0,w,h); });
  }
  for(let i=0;i<parts.length;i++){
    const p=parts[i]; p.t+=dt;
    p.x+=p.vx*dt; p.y+=p.vy*dt;
    if(fxKind==='snow'){ p.x+=Math.sin(p.t*.8)*6*dt; if(p.y>h+5) parts[i]=newPart('snow',w,h,false);
      g.globalAlpha=p.a; g.fillStyle='#e6f6ff'; g.beginPath(); g.arc(p.x,p.y,p.r,0,7); g.fill(); }
    else if(fxKind==='embers'){ p.x+=Math.sin(p.t*2+i)*10*dt; p.life-=dt; if(p.y<-5||p.life<0) parts[i]=newPart('embers',w,h,false);
      const f=.6+.4*Math.sin(p.t*9+i); g.globalAlpha=p.a*f*Math.min(1,p.life/2); g.fillStyle=i%3?'#ff8a3a':'#ffd27a'; g.beginPath(); g.arc(p.x,p.y,p.r,0,7); g.fill(); }
    else if(fxKind==='petals'){ p.rot+=p.vr*dt; p.x+=Math.sin(p.t*1.3)*14*dt; if(p.y>h+10||p.x>w+20) parts[i]=newPart('petals',w,h,false);
      g.save(); g.translate(p.x,p.y); g.rotate(p.rot); g.scale(1,.55+.45*Math.sin(p.t*2.4)); g.globalAlpha=p.a; g.fillStyle='#ffc2d6';
      g.beginPath(); g.moveTo(0,-p.r); g.quadraticCurveTo(p.r,0,0,p.r); g.quadraticCurveTo(-p.r,0,0,-p.r); g.fill(); g.restore(); }
    else if(fxKind==='fog'){ if(p.x<-p.r) p.x=w+p.r; if(p.x>w+p.r) p.x=-p.r;
      const r=g.createRadialGradient(p.x,p.y,0,p.x,p.y,p.r); r.addColorStop(0,`rgba(200,190,210,${p.a})`); r.addColorStop(1,'rgba(200,190,210,0)'); g.globalAlpha=1; g.fillStyle=r; g.fillRect(p.x-p.r,p.y-p.r,p.r*2,p.r*2); }
    else { if(p.x<0) p.x=w; if(p.x>w) p.x=0; if(p.y<0) p.y=h; if(p.y>h) p.y=0;
      g.globalAlpha=p.a*(.5+.5*Math.sin(p.t*1.7+i)); g.fillStyle=fxKind==='gold'?'#ffd98a':'#ffcf8a'; g.beginPath(); g.arc(p.x,p.y,p.r,0,7); g.fill(); }
  }
  g.globalAlpha=1;
}
function startFx(kind){
  cancelAnimationFrame(fxRaf); fxRaf=0; fxKind=kind;
  const run=!!kind&&fxOn&&!(window.matchMedia&&matchMedia('(prefers-reduced-motion: reduce)').matches);
  fxc.style.display=run?'block':'none';
  if(!run) return;
  fxc.width=app.offsetWidth; fxc.height=app.offsetHeight; seed(kind,fxc.width,fxc.height); lastT=performance.now(); fxRaf=requestAnimationFrame(fxFrame);
}
document.addEventListener('visibilitychange',()=>{ if(document.hidden){ cancelAnimationFrame(fxRaf); fxRaf=0; } else if(fxKind&&fxOn&&!fxRaf) startFx(fxKind); });

/* ---- apply ---- */
let cur=THEMES[0];
function applyTheme(id,keepAccent){ const t=THEMES.find(x=>x.id===id)||THEMES[0]; cur=t;
  VARS.forEach(([n,rgb,role])=>{ const [r,g,b]=rgb.split(',').map(Number); R.style.setProperty(n,t.fn(r,g,b,role).join(',')); });
  R.style.setProperty('--sh',t.sh); R.style.colorScheme=t.light?'light':'dark'; R.dataset.theme=t.id;
  aur.style.display=t.aurora?'block':'none'; const gr=$('.grain'); if(gr) gr.style.opacity=t.light?.05:'';
  setFonts(t);
  logos.forEach(s=>{ s.textContent=t.font&&t.font.logoCase?t.font.logoCase:s.dataset.txt; });
  R.classList.toggle('thx',!!t.band);
  if(t.band){ bandTop.innerHTML=bandSvg(t.band,false); bandBot.innerHTML=bandSvg(t.band,true); requestAnimationFrame(placeBands); }
  startFx(t.fx);
  ACC0=t.accent; if(!keepAccent) applyAcc(t.accent);
  $$('.thc').forEach(b=>b.setAttribute('aria-pressed',b.dataset.t===t.id));
  return t; }

/* ---- Interface settings: drop Knob control / Show level meters / Animations, add Theme ---- */
const panel=$('.spanel[data-panel="Interface"]');
if(panel){ panel.style.overflowY='auto'; panel.style.maxHeight='100%'; }
['Knob control','Show level meters','Animations'].forEach(n=>{ const sp=$$('span',panel).find(x=>x.textContent.trim()===n); if(sp) sp.parentElement.parentElement.remove(); });
const accRow=$$('span',panel).find(x=>x.textContent.trim()==='Accent colour').parentElement.parentElement;
const block=accRow.cloneNode(false); block.style.flexDirection='column'; block.style.alignItems='stretch'; block.style.justifyContent='flex-start';
const left=accRow.firstElementChild.cloneNode(true);
left.children[0].textContent='Theme'; left.children[1].textContent='Colours, fonts and moving details for the whole plugin';
const grid=document.createElement('div'); grid.className='thgrid';
THEMES.forEach(t=>{
  const bg=t.fn(12,12,13,'b'), pn=t.fn(32,32,36,'b'), tx=t.fn(233,230,223,'f'), hd=t.fn(26,26,29,'b');
  const b=document.createElement('button'); b.className='thc'; b.dataset.t=t.id; b.setAttribute('aria-label',t.name+' theme'); b.title=t.note;
  const f=t.font?"'"+t.font.logo+"', "+BASE:BASE;
  b.innerHTML='<span class="sw" style="background: linear-gradient(160deg, rgb('+pn+'), rgb('+bg+'))"><em style="background: rgb('+hd+')"></em><b style="font-family: '+f.replace(/"/g,'')+'; color: rgb('+tx+')">'+(t.font&&t.font.logoCase?'Ob':'Aa')+'</b><i style="background: '+t.accent+'; box-shadow: 0 0 8px '+t.accent+'"></i></span>'
    +'<span style="font-family: '+(t.font?"'"+t.font.head+"', ":'')+'inherit">'+t.name+'</span><small>'+t.note+'</small>';
  grid.appendChild(b); });
/* moving details on / off */
const fxRow=document.createElement('div'); fxRow.style.cssText='display: flex; align-items: center; justify-content: space-between; margin-top: 10px';
fxRow.innerHTML='<span style="font-size: 12px; color: rgb(var(--cf_a3a3a8))">Moving details (snow, embers, petals, fog…)</span>';
const fxTog=$('.tog',panel)?$('.tog',panel).cloneNode(true):document.createElement('button');
fxTog.setAttribute('aria-label','Moving theme details'); fxRow.appendChild(fxTog);
block.append(left,grid,fxRow); accRow.before(block);
const drawFxTog=()=>{ if(typeof setTog==='function') setTog(fxTog,fxOn); };
drawFxTog();
fxTog.addEventListener('click',e=>{ e.stopPropagation(); e.preventDefault(); setThemeFx(!fxOn,true); },true);
function setThemeFx(on,fromUser){ fxOn=!!on; drawFxTog(); try{ localStorage.setItem('obsidianThemeFx',fxOn?'1':'0'); }catch(_){}
  startFx(cur.fx); if(fromUser&&window.obsSetSetting) window.obsSetSetting('themeFx',fxOn); }
window.setThemeFx=on=>setThemeFx(on,false);
grid.addEventListener('click',e=>{ const b=e.target.closest('.thc'); if(!b) return; window.applyTheme(b.dataset.t); const t=cur;   /* window.applyTheme: the plugin saves it */
  try{ localStorage.setItem('obsidianTheme',t.id); localStorage.setItem('obsidianAccent',t.accent); }catch(_){} toast(t.name+' theme'); });

let saved='obsidian', savedAcc=null; try{ saved=localStorage.getItem('obsidianTheme')||'obsidian'; savedAcc=localStorage.getItem('obsidianAccent'); }catch(_){}
applyTheme(saved, !!savedAcc);
addEventListener('resize',()=>requestAnimationFrame(placeBands));
if(document.fonts&&document.fonts.ready) document.fonts.ready.then(()=>requestAnimationFrame(placeBands));
window.applyTheme=applyTheme;
window.obsThemes=THEMES.map(t=>t.id);
})();
</script>
