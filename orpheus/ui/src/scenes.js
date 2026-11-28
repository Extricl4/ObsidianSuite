<script>
/* =============================================================================
   OBSIDIAN · theme scenes
   The picture behind the pages for each theme: architecture, landscapes and
   ornaments in the margins (windows and bats, columns, mountains and runestones,
   castle and banners, a synthwave sun, a torii and blossoms, lava, constellations,
   tarot cards). Drawn as one SVG (1360 x 592, the page area) in the theme's own
   colours, kept dim so the controls stay in front. Small animations (bats, torch
   flames, lava, twinkling stars, a shooting star) stop when moving details are off.
   ============================================================================= */
(function(){
const W=1360, H=592;
const f1=v=>Math.round(v*10)/10;
/* deterministic random, so a scene looks the same every time */
function rng(seed){ let s=seed>>>0||1; return ()=>{ s=(s*1664525+1013904223)>>>0; return s/4294967296; }; }
const rgb=a=>'rgb('+a.join(',')+')';

function sky(c,top,bottom){ return `<defs><linearGradient id="scSky" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="${top||c.bg2}"/><stop offset="1" stop-color="${bottom||c.bg}"/></linearGradient>
  <radialGradient id="scVig" cx=".5" cy=".42" r=".75"><stop offset=".55" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity=".55"/></radialGradient></defs>
  <rect width="${W}" height="${H}" fill="url(#scSky)"/>`; }
const vignette=()=>`<rect width="${W}" height="${H}" fill="url(#scVig)"/>`;
function stars(r,n,x0,y0,x1,y1,col,twinkle){ let s=''; for(let i=0;i<n;i++){ const x=x0+r()*(x1-x0), y=y0+r()*(y1-y0), rr=.4+r()*r()*1.4, a=.25+r()*.6;
  s+=`<circle cx="${f1(x)}" cy="${f1(y)}" r="${f1(rr)}" fill="${col}" opacity="${f1(a)}"${twinkle&&i%5===0?` class="sc-tw" style="animation-delay:-${f1(r()*6)}s"`:''}/>`; } return s; }
function moon(cx,cy,r,col,glow,crescent,bgc){ let s=`<circle cx="${cx}" cy="${cy}" r="${r*3}" fill="url(#scHalo)"/><defs><radialGradient id="scHalo"><stop offset="0" stop-color="${glow}" stop-opacity=".22"/><stop offset=".35" stop-color="${glow}" stop-opacity=".07"/><stop offset="1" stop-color="${glow}" stop-opacity="0"/></radialGradient>
  <radialGradient id="scMoonG" cx=".38" cy=".35" r=".75"><stop offset="0" stop-color="${col}"/><stop offset="1" stop-color="${col}" stop-opacity=".55"/></radialGradient></defs>
  <circle cx="${cx}" cy="${cy}" r="${r}" fill="url(#scMoonG)" opacity=".55"/>`;
  s+=`<g opacity=".16" fill="#000"><circle cx="${cx-r*.3}" cy="${cy-r*.2}" r="${r*.18}"/><circle cx="${cx+r*.25}" cy="${cy+r*.25}" r="${r*.12}"/><circle cx="${cx+r*.1}" cy="${cy-r*.42}" r="${r*.08}"/></g>`;
  if(crescent) s+=`<circle cx="${cx+r*.42}" cy="${cy-r*.18}" r="${r*.92}" fill="${bgc}"/>`;
  return s; }
const shootingStar=(x,y,delay)=>`<g class="sc-shoot" style="animation-delay:${delay}s"><line x1="${x}" y1="${y}" x2="${x-120}" y2="${y-40}" stroke="url(#scShoot)" stroke-width="1.6" stroke-linecap="round"/></g>
  <defs><linearGradient id="scShoot" x1="1" y1="1" x2="0" y2="0"><stop offset="0" stop-color="#fff" stop-opacity=".9"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient></defs>`;
const bat=(i,y,dur,delay,sc)=>`<g class="sc-bat" style="--y:${y}px;--y2:${y-40-i*12}px;animation-duration:${dur}s;animation-delay:-${delay}s"><g transform="scale(${sc})"><g class="sc-flap">
  <path d="M0 0C-4 -6 -10 -8 -18 -6C-14 -4 -13 -1 -14 2C-10 0 -6 1 -4 3C-3 1 -1 0 0 0Z M0 0C4 -6 10 -8 18 -6C14 -4 13 -1 14 2C10 0 6 1 4 3C3 1 1 0 0 0Z"/>
  <ellipse cx="0" cy="1" rx="2.2" ry="3.2"/><path d="M-1.6 -1.6L-1 -4L0 -2L1 -4L1.6 -1.6Z"/></g></g></g>`;

/* ---------------------------------------------------------------- the scenes */
const S={};

S.gothic=c=>{
  const r=rng(7);
  const win=(x,y,w,h)=>{
    const sy=y+w*.62, ap=`M${x} ${y+h}L${x} ${sy}C${x} ${y+w*.18} ${x+w*.28} ${y} ${x+w/2} ${y}C${x+w*.72} ${y} ${x+w} ${y+w*.18} ${x+w} ${sy}L${x+w} ${y+h}Z`;
    let s=`<path d="${ap}" fill="url(#scGlass)"/>`;
    // stained glass lattice
    let lat=''; for(let yy=y+w*.7;yy<y+h;yy+=18) lat+=`M${x} ${yy}L${x+w} ${yy+18}M${x+w} ${yy}L${x} ${yy+18}`;
    s+=`<clipPath id="scW${x}"><path d="${ap}"/></clipPath><path d="${lat}" stroke="${c.bg}" stroke-width="1.2" opacity=".7" clip-path="url(#scW${x})"/>`;
    // tracery: two lancets, a rose, mullion and transom
    const cx=x+w/2, ry=y+w*.5;
    s+=`<g fill="none" stroke="${c.frame}" stroke-width="4"><path d="${ap}"/><path d="M${cx} ${y+w*.95}V${y+h}"/>
      <path d="M${x+5} ${y+h*.62}H${x+w-5}"/><circle cx="${cx}" cy="${ry}" r="${w*.26}"/></g>
      <g fill="none" stroke="${c.frame}" stroke-width="2.2"><path d="M${x+6} ${y+w*1.05}C${x+6} ${y+w*.78} ${cx-4} ${y+w*.78} ${cx-3} ${y+w*1.05}M${cx+3} ${y+w*1.05}C${cx+3} ${y+w*.78} ${x+w-6} ${y+w*.78} ${x+w-6} ${y+w*1.05}"/>
      ${[0,1,2,3].map(k=>{ const a=k*Math.PI/2+Math.PI/4; return `<circle cx="${f1(cx+Math.cos(a)*w*.12)}" cy="${f1(ry+Math.sin(a)*w*.12)}" r="${f1(w*.1)}"/>`; }).join('')}</g>
      <circle cx="${cx}" cy="${ry}" r="${w*.05}" fill="${c.acc}" opacity=".5"/>`;
    return s; };
  const spires=()=>{ let d=`M0 ${H}`; let x=0; const rr=rng(3);
    while(x<W){ const w=26+rr()*60, h=90+rr()*170*(x<300||x>1060?1:.55); const top=H-h;
      d+=`L${f1(x)} ${f1(H-h*.45)}L${f1(x+w*.18)} ${f1(H-h*.45)}L${f1(x+w*.5)} ${f1(top)}L${f1(x+w*.82)} ${f1(H-h*.45)}L${f1(x+w)} ${f1(H-h*.45)}`; x+=w+rr()*18; }
    return `<path d="${d}L${W} ${H}Z" fill="${c.sil}" opacity=".9"/>`; };
  return sky(c,c.bg3,c.bg)+`<defs><linearGradient id="scGlass" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="${c.acc}" stop-opacity=".42"/><stop offset=".5" stop-color="#6a2580" stop-opacity=".3"/><stop offset="1" stop-color="#1a0b22" stop-opacity=".15"/></linearGradient>
    <filter id="scBlur" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="16"/></filter></defs>`
    +moon(1205,92,46,'#f6e9e4','#ffb0a0',false)
    +stars(r,40,180,10,1180,180,'#ffe9e9',true)
    +`<g filter="url(#scBlur)" opacity=".5"><rect x="22" y="40" width="96" height="430" fill="${c.acc}" opacity=".35"/><rect x="1242" y="40" width="96" height="430" fill="${c.acc}" opacity=".35"/></g>`
    +win(22,36,96,436)+win(1242,36,96,436)
    +spires()
    +`<g fill="${c.batc}">${bat(0,120,22,3,1.1)}${bat(1,170,28,14,.8)}${bat(2,90,34,25,.65)}</g>`
    +vignette();
};

S.roman=c=>{
  const col=(x,y,w,h)=>{
    let s=`<defs><linearGradient id="scMarble${x}" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="${c.stone2}"/><stop offset=".35" stop-color="${c.stone}"/><stop offset=".7" stop-color="${c.stone2}"/><stop offset="1" stop-color="${c.stone3}"/></linearGradient></defs>`;
    const sx=x+w*.12, sw=w*.76, top=y+34, bot=y+h-26;
    s+=`<rect x="${x}" y="${y+h-26}" width="${w}" height="12" fill="url(#scMarble${x})"/><rect x="${x-6}" y="${y+h-14}" width="${w+12}" height="14" fill="url(#scMarble${x})"/>`;
    s+=`<path d="M${sx} ${top}L${sx+sw} ${top}L${sx+sw-2} ${bot}L${sx+2} ${bot}Z" fill="url(#scMarble${x})"/>`;
    for(let k=1;k<7;k++){ const fx=sx+sw*k/7; s+=`<path d="M${f1(fx)} ${top+4}V${bot-4}" stroke="${c.stone3}" stroke-width="2.2" opacity=".55"/><path d="M${f1(fx+1.6)} ${top+4}V${bot-4}" stroke="${c.stone}" stroke-width=".8" opacity=".6"/>`; }
    // ionic capital: abacus + volutes
    s+=`<rect x="${x-8}" y="${y}" width="${w+16}" height="10" fill="url(#scMarble${x})"/><path d="M${x-2} ${y+10}H${x+w+2}Q${x+w-6} ${y+24} ${x+w/2} ${y+24}Q${x+6} ${y+24} ${x-2} ${y+10}Z" fill="url(#scMarble${x})"/>`;
    for(const vx of [x+4,x+w-4]) s+=`<circle cx="${vx}" cy="${y+20}" r="10" fill="url(#scMarble${x})"/><path d="M${vx} ${y+20}m-6 0a6 6 0 1 0 6 -6a4 4 0 1 0 4 4a2 2 0 1 0 -2 2" fill="none" stroke="${c.stone3}" stroke-width="1.4"/>`;
    s+=`<rect x="${sx-4}" y="${top-8}" width="${sw+8}" height="8" fill="url(#scMarble${x})"/>`;
    return `<g opacity=".52">${s}</g>`; };
  const laurel=(x,y,flip)=>{ let s=''; const k=flip?-1:1;
    for(let i=0;i<11;i++){ const t=i/10, px=x+k*(t*150), py=y-Math.sin(t*2.4)*60;
      for(const sgn of [-1,1]) s+=`<ellipse cx="${f1(px+k*sgn*4)}" cy="${f1(py+sgn*7)}" rx="11" ry="4" transform="rotate(${f1(k*(-30*sgn-35+t*40))} ${f1(px+k*sgn*4)} ${f1(py+sgn*7)})" fill="${c.gold}"/>`; }
    return `<g opacity=".32"><path d="M${x} ${y}Q${x+k*80} ${y-70} ${x+k*155} ${y-20}" fill="none" stroke="${c.gold}" stroke-width="2"/>${s}</g>`; };
  let dent=''; for(let x=0;x<W;x+=14) dent+=`<rect x="${x}" y="12" width="8" height="7" fill="${c.stone2}"/>`;
  return sky(c,c.bg3,c.bg)+`<defs><radialGradient id="scSun" cx=".5" cy=".0" r=".8"><stop offset="0" stop-color="${c.gold}" stop-opacity=".16"/><stop offset="1" stop-color="${c.gold}" stop-opacity="0"/></radialGradient></defs>
    <rect width="${W}" height="${H}" fill="url(#scSun)"/>`
    +`<g opacity=".35"><rect x="0" y="0" width="${W}" height="10" fill="${c.stone2}"/>${dent}<rect x="0" y="20" width="${W}" height="2" fill="${c.gold}" opacity=".6"/></g>`
    +col(26,30,74,548)+col(1260,30,74,548)
    +`<g opacity=".18">${col(150,140,40,438)}${col(1170,140,40,438)}</g>`
    +laurel(130,575,false)+laurel(1230,575,true)
    +vignette();
};

S.norse=c=>{
  const r=rng(11);
  const ridge=(base,amp,step,seed2,fill,snow)=>{ const rr=rng(seed2); let d=`M0 ${H}L0 ${base}`, pts=[[0,base]]; for(let x=0;x<=W+step;x+=step){ const y=base-amp*(.3+rr()*.7)*(x<250||x>1100?1:.6); pts.push([x,y]); d+=`L${x} ${f1(y)}`; }
    d+=`L${W} ${H}Z`; let s=`<path d="${d}" fill="${fill}"/>`;
    if(snow) pts.forEach(([x,y],i)=>{ if(i&&i<pts.length-1&&y<pts[i-1][1]&&y<pts[i+1][1]) s+=`<path d="M${x-step*.32} ${f1(y+amp*.14)}L${x} ${f1(y)}L${x+step*.3} ${f1(y+amp*.16)}L${x+step*.12} ${f1(y+amp*.1)}L${x-step*.05} ${f1(y+amp*.18)}Z" fill="${snow}" opacity=".55"/>`; });
    return s; };
  const stone=(x,y,w,h)=>{ const d=`M${x} ${y+h}C${x-4} ${y+h*.4} ${x+w*.1} ${y} ${x+w*.5} ${y}C${x+w*.9} ${y} ${x+w+4} ${y+h*.4} ${x+w} ${y+h}Z`;
    let runes=''; const R2=['M0 0V14M0 3L5 7','M0 0V14M5 0V14M0 0L5 5','M0 0V14M0 4L4 7L0 10','M0 14L5 0L5 14','M2 0V14M-1 4L5 10','M0 0L5 14M5 0L0 14'];
    for(let i=0;i<6;i++) runes+=`<path transform="translate(${x+w*.5-3} ${y+30+i*22})" d="${R2[i]}" fill="none" stroke="${c.acc}" stroke-width="1.6" stroke-linecap="round"/>`;
    return `<path d="${d}" fill="${c.rock}"/><path d="${d}" fill="none" stroke="${c.rock2}" stroke-width="2"/>
      <path d="M${x+w*.22} ${y+h-8}C${x+w*.1} ${y+h*.5} ${x+w*.2} ${y+18} ${x+w*.5} ${y+14}C${x+w*.8} ${y+18} ${x+w*.9} ${y+h*.5} ${x+w*.78} ${y+h-8}" fill="none" stroke="${c.acc}" stroke-width="2.4" opacity=".55"/>
      <g class="sc-glow" opacity=".75">${runes}</g>`; };
  const aur=(y,col1,col2,d,delay)=>`<path class="sc-aur" style="animation-duration:${d}s;animation-delay:-${delay}s" d="M-40 ${y}C200 ${y-60} 420 ${y+50} 680 ${y-10}S1160 ${y-70} 1400 ${y}L1400 ${y-110}C1160 ${y-170} 900 ${y-90} 680 ${y-120}S200 ${y-160} -40 ${y-100}Z" fill="url(#scAur${y})"/>
    <defs><linearGradient id="scAur${y}" x1="0" y1="1" x2="0" y2="0"><stop offset="0" stop-color="${col1}" stop-opacity=".32"/><stop offset=".5" stop-color="${col2}" stop-opacity=".12"/><stop offset="1" stop-color="${col2}" stop-opacity="0"/></linearGradient>
    <filter id="scSoft" x="-20%" y="-50%" width="140%" height="200%"><feGaussianBlur stdDeviation="9"/></filter></defs>`;
  return sky(c,c.bg3,c.bg)+stars(r,70,0,0,W,260,'#e8f6ff',true)
    +`<g filter="url(#scSoft)">${aur(200,'#53f0b9','#5aa0ff',24,0)}${aur(150,'#7fe0ff','#a77bff',31,9)}</g>`
    +ridge(430,170,70,5,c.far,'#dff1ff')+ridge(500,120,55,9,c.mid,'#cfe6f7')
    +`<rect y="520" width="${W}" height="72" fill="${c.water}"/>`+Array.from({length:9},(_,i)=>`<path d="M${80+i*150} ${532+(i%3)*14}h${40+(i*37)%60}" stroke="#bfe8ff" stroke-width="1" opacity=".12"/>`).join('')
    +stone(18,330,92,262)+stone(1250,360,90,232)
    +vignette();
};

S.medieval=c=>{
  const castle=(x,flip)=>{ const k=flip?-1:1, X=v=>x+k*v; let s='';
    const tower=(tx,tw,th)=>{ let d=`M${X(tx)} ${H}V${H-th}`; for(let i=0;i<=4;i++){ const cx=tx+i*tw/4; d+=`H${X(cx)}V${H-th-(i%2?0:10)}`; } d+=`H${X(tx+tw)}V${H}Z`; return `<path d="${d}" fill="${c.sil}"/>`; };
    s+=tower(0,70,300)+tower(64,120,200)+tower(178,60,250)+tower(232,140,160);
    s+=`<path d="M${X(10)} ${H-300}L${X(35)} ${H-372}L${X(60)} ${H-300}Z" fill="${c.sil}"/><path d="M${X(35)} ${H-372}V${H-402}" stroke="${c.sil}" stroke-width="2"/><path d="M${X(35)} ${H-402}l${k*16} 5l${-k*16} 5Z" fill="${c.acc}" opacity=".7"/>`;
    for(const [wx,wy] of [[30,H-250],[110,H-150],[200,H-200],[290,H-110],[34,H-190]]) s+=`<rect x="${X(wx)-(flip?7:0)}" y="${wy}" width="7" height="12" rx="3.5" fill="#ffb347" opacity=".75" class="sc-flick" style="animation-delay:-${(wx%7)/3}s"/>`;
    return s; };
  const banner=(x)=>`<g class="sc-sway" style="transform-origin:${x}px 0px"><path d="M${x-34} 0H${x+34}V210L${x} 246L${x-34} 210Z" fill="${c.ban}"/><path d="M${x-34} 0H${x+34}V210L${x} 246L${x-34} 210Z" fill="none" stroke="${c.gold}" stroke-width="2" opacity=".7"/>
    <path d="M${x-26} 6H${x+26}V204L${x} 236L${x-26} 204Z" fill="none" stroke="${c.gold}" stroke-width="1" opacity=".45"/>
    <path d="M${x-18} 70H${x+18}V100C${x+18} 122 ${x} 132 ${x} 134C${x} 132 ${x-18} 122 ${x-18} 100Z" fill="${c.gold}" opacity=".75"/><path d="M${x-12} 108L${x} 88L${x+12} 108" fill="none" stroke="${c.ban}" stroke-width="5"/></g>`;
  const torch=(x,y)=>`<g><path d="M${x-5} ${y}h10l-3 34h-4Z" fill="#3a2414"/><path d="M${x-9} ${y}h18" stroke="#5a3a1e" stroke-width="4"/>
    <g class="sc-flame" style="transform-origin:${x}px ${y}px"><path d="M${x} ${y-38}C${x+12} ${y-20} ${x+10} ${y-4} ${x} ${y}C${x-10} ${y-4} ${x-12} ${y-20} ${x} ${y-38}Z" fill="#ff9a2a"/><path d="M${x} ${y-24}C${x+6} ${y-12} ${x+5} ${y-3} ${x} ${y}C${x-5} ${y-3} ${x-6} ${y-12} ${x} ${y-24}Z" fill="#ffe08a"/></g>
    <circle class="sc-flick" cx="${x}" cy="${y-16}" r="70" fill="url(#scTorch)"/></g>`;
  let stonework=''; for(let y=0;y<H;y+=26) for(let x=(y/26)%2?0:-30;x<W;x+=60) stonework+=`<rect x="${x+1}" y="${y+1}" width="58" height="24" rx="3" fill="none" stroke="${c.bg}" stroke-width="1.6"/>`;
  return sky(c,c.bg3,c.bg)+`<defs><radialGradient id="scTorch"><stop offset="0" stop-color="#ffb04a" stop-opacity=".28"/><stop offset="1" stop-color="#ffb04a" stop-opacity="0"/></radialGradient></defs>`
    +`<g opacity=".35">${stonework}</g>`
    +castle(0,false)+castle(W,true)
    +banner(70)+banner(1290)+torch(160,300)+torch(1200,300)
    +vignette();
};

S.neon=c=>{
  const sunX=1150, sunY=168, R=112, hor=540;
  let stripes=''; for(let i=0;i<8;i++){ const y=sunY+6+i*i*1.6+i*7; stripes+=`<rect x="${sunX-R}" y="${f1(y)}" width="${R*2}" height="${f1(1.6+i*.9)}" fill="${c.bg3}"/>`; }
  const palm=(x,flip)=>{ const k=flip?-1:1; let s=`<path d="M${x} ${H}C${x+k*6} ${H-120} ${x+k*30} ${H-230} ${x+k*58} ${H-300}" fill="none" stroke="${c.palm}" stroke-width="11" stroke-linecap="round"/>`;
    const tx=x+k*58, ty=H-300;
    for(const [a,l] of [[-160,110],[-130,130],[-95,90],[-60,125],[-25,115],[10,95],[-195,80]]){ const ar=a*Math.PI/180, ex=tx+k*Math.cos(ar)*l, ey=ty+Math.sin(ar)*l*.6+28;
      s+=`<path d="M${tx} ${ty}Q${f1(tx+k*Math.cos(ar)*l*.55)} ${f1(ty+Math.sin(ar)*l*.6-22)} ${f1(ex)} ${f1(ey)}Q${f1(tx+k*Math.cos(ar)*l*.5)} ${f1(ty+Math.sin(ar)*l*.6-6)} ${tx} ${ty}Z" fill="${c.palm}"/>`; }
    return s; };
  const wire=(x0,x1,base,seed2)=>{ const rr=rng(seed2); let pts=[]; for(let x=x0;x<=x1;x+=34) pts.push([x,base-(Math.sin((x-x0)/(x1-x0)*Math.PI))*(60+rr()*110)]);
    let s=`<path d="M${pts.map(p=>p.join(' ')).join('L')}" fill="none" stroke="${c.acc2}" stroke-width="1.4" opacity=".55"/>`;
    pts.forEach(([x,y])=>{ s+=`<path d="M${x} ${f1(y)}L${x+(x<680?60:-60)} ${base}" stroke="${c.acc2}" stroke-width=".8" opacity=".3"/>`; });
    return s; };
  return sky(c,c.bg3,c.bg)+`<defs><linearGradient id="scSunG" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffd36e"/><stop offset=".55" stop-color="#ff5fa2"/><stop offset="1" stop-color="#b54bff"/></linearGradient>
    <radialGradient id="scSunGlow"><stop offset="0" stop-color="#ff4fb0" stop-opacity=".35"/><stop offset="1" stop-color="#ff4fb0" stop-opacity="0"/></radialGradient></defs>`
    +stars(rng(5),50,0,0,W,300,'#ffd8ff',true)
    +`<circle cx="${sunX}" cy="${sunY}" r="${R*2.4}" fill="url(#scSunGlow)"/><g opacity=".72"><circle cx="${sunX}" cy="${sunY}" r="${R}" fill="url(#scSunG)"/>${stripes}</g>`
    +wire(0,520,hor,4)+wire(840,W,hor,8)
    +`<path d="M0 ${hor}H${W}" stroke="${c.acc}" stroke-width="2" opacity=".7"/><path d="M0 ${hor}H${W}" stroke="${c.acc}" stroke-width="9" opacity=".16"/>`
    +palm(40,false)+palm(118,false)+palm(W-40,true)
    +vignette();
};

S.sakura=c=>{
  const branch=(x,y,flip,seed2)=>{ const rr=rng(seed2), k=flip?-1:1; let s='', bl='';
    const seg=(x0,y0,a,len,w,depth)=>{ const x1=x0+k*Math.cos(a)*len, y1=y0+Math.sin(a)*len;
      s+=`<path d="M${f1(x0)} ${f1(y0)}Q${f1((x0+x1)/2+k*rr()*14)} ${f1((y0+y1)/2+rr()*14)} ${f1(x1)} ${f1(y1)}" stroke="${c.wood}" stroke-width="${f1(w)}" stroke-linecap="round" fill="none"/>`;
      if(depth>0){ seg(x1,y1,a-.5+rr()*.3,len*.7,w*.68,depth-1); if(rr()<.8) seg(x1,y1,a+.35+rr()*.4,len*.62,w*.6,depth-1); }
      else for(let i=0;i<3;i++) bl+=flower(x1+rr()*20-10,y1+rr()*20-10,5+rr()*3,rr()); };
    seg(x,y,.35,150,10,4);
    return s+bl; };
  const flower=(x,y,r,t)=>{ let s=''; for(let i=0;i<5;i++){ const a=i*1.2566+t; s+=`<ellipse cx="${f1(x+Math.cos(a)*r*.6)}" cy="${f1(y+Math.sin(a)*r*.6)}" rx="${f1(r*.62)}" ry="${f1(r*.42)}" transform="rotate(${f1(a*57.3)} ${f1(x+Math.cos(a)*r*.6)} ${f1(y+Math.sin(a)*r*.6)})" fill="${c.petal}" opacity=".8"/>`; }
    return s+`<circle cx="${f1(x)}" cy="${f1(y)}" r="${f1(r*.2)}" fill="#fff3c8" opacity=".85"/>`; };
  const torii=(x,y,s)=>`<g transform="translate(${x} ${y}) scale(${s})" fill="${c.torii}"><path d="M-110 0C-60 10 60 10 110 0L104 14C60 22 -60 22 -104 14Z"/><rect x="-92" y="30" width="184" height="10"/><rect x="-74" y="14" width="13" height="210"/><rect x="61" y="14" width="13" height="210"/><rect x="-6" y="20" width="12" height="14"/></g>`;
  return sky(c,c.bg3,c.bg)+moon(1150,120,70,'#fff0f3','#ffc2d4',false)
    +stars(rng(9),30,300,10,1050,200,'#ffe8f0',true)
    +`<path d="M440 ${H}L600 418Q680 372 760 418L920 ${H}Z" fill="${c.far}" opacity=".9"/><path d="M600 418Q680 372 760 418L728 436L706 424L680 440L656 426L634 438Z" fill="#fff4f7" opacity=".35"/>`
    +`<path d="M0 ${H}V540Q300 500 620 548T${W} 530V${H}Z" fill="${c.mid}"/>`
    +torii(150,370,.9)
    +`<g opacity=".9">${branch(0,30,false,3)}${branch(W,60,true,6)}</g>`
    +vignette();
};

S.inferno=c=>{
  const rocks=(seed2)=>{ const rr=rng(seed2); let d=`M0 ${H}`, cracks=''; let x=0;
    while(x<W){ const w=40+rr()*90, h=60+rr()*(x<260||x>1100?190:70); d+=`L${f1(x)} ${f1(H-h*.6)}L${f1(x+w*.3)} ${f1(H-h)}L${f1(x+w*.62)} ${f1(H-h*.85)}L${f1(x+w)} ${f1(H-h*.5)}`;
      if(rr()<.8){ const cx=x+w*.4; cracks+=`<path d="M${f1(cx)} ${f1(H-h*.8)}l${f1(rr()*14-7)} 22l${f1(rr()*16-8)} 18l${f1(rr()*12-6)} 30l${f1(rr()*10-5)} 40" fill="none" stroke="#ff7a2a" stroke-width="2" stroke-linecap="round"/>`; }
      x+=w; }
    return `<path d="${d}L${W} ${H}Z" fill="${c.rock}"/><g class="sc-lava" filter="url(#scGlowF)">${cracks}</g><g class="sc-lava">${cracks}</g>`; };
  const flames=()=>{ let s=''; const rr=rng(21); for(let i=0;i<26;i++){ const x=i*54+rr()*20, h=40+rr()*70, w=24+rr()*20;
    s+=`<path class="sc-flame" style="transform-origin:${f1(x)}px ${H}px;animation-delay:-${f1(rr()*2)}s;animation-duration:${f1(1.2+rr())}s" d="M${f1(x-w/2)} ${H}C${f1(x-w/2)} ${f1(H-h*.5)} ${f1(x-2)} ${f1(H-h*.6)} ${f1(x)} ${f1(H-h)}C${f1(x+4)} ${f1(H-h*.55)} ${f1(x+w/2)} ${f1(H-h*.45)} ${f1(x+w/2)} ${H}Z" fill="url(#scFlameG)"/>`; }
    return s; };
  return sky(c,c.bg,c.bg3)+`<defs><linearGradient id="scFlameG" x1="0" y1="1" x2="0" y2="0"><stop offset="0" stop-color="#ffd36a" stop-opacity=".55"/><stop offset=".5" stop-color="#ff5a1f" stop-opacity=".35"/><stop offset="1" stop-color="#ff2a10" stop-opacity="0"/></linearGradient>
    <linearGradient id="scHeat" x1="0" y1="1" x2="0" y2="0"><stop offset="0" stop-color="#ff4a12" stop-opacity=".28"/><stop offset=".5" stop-color="#ff4a12" stop-opacity=".05"/><stop offset="1" stop-color="#ff4a12" stop-opacity="0"/></linearGradient>
    <filter id="scGlowF" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="5"/></filter>
    <linearGradient id="scSmoke" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#000" stop-opacity=".45"/><stop offset="1" stop-color="#000" stop-opacity="0"/></linearGradient></defs>`
    +`<rect width="${W}" height="${H}" fill="url(#scHeat)"/><rect width="${W}" height="200" fill="url(#scSmoke)"/>`
    +rocks(13)+`<g opacity=".9">${flames()}</g>`
    +vignette();
};

S.midnight=c=>{
  const r=rng(17);
  const cons=(pts,lines)=>{ let s=''; lines.forEach(([a,b])=>s+=`<path d="M${pts[a][0]} ${pts[a][1]}L${pts[b][0]} ${pts[b][1]}" stroke="#bcd9ff" stroke-width=".8" opacity=".28"/>`);
    pts.forEach(([x,y])=>s+=`<circle cx="${x}" cy="${y}" r="1.9" fill="#eaf4ff" opacity=".85" class="sc-tw" style="animation-delay:-${f1(r()*5)}s"/>`); return s; };
  const orion=[[1180,140],[1240,150],[1196,210],[1212,214],[1228,218],[1176,290],[1250,286]];
  const dipper=[[70,110],[120,100],[165,112],[205,130],[220,180],[275,182],[282,135]];
  return sky(c,c.bg3,c.bg)+stars(r,140,0,0,W,H*.8,'#e6f2ff',true)
    +moon(1120,86,40,'#eef5ff','#9fc7ff',true,c.bg3)
    +cons(orion,[[0,2],[1,4],[2,3],[3,4],[2,5],[4,6],[0,1]])+cons(dipper,[[0,1],[1,2],[2,3],[3,4],[4,5],[5,6],[6,3]])
    +shootingStar(900,120,2)+shootingStar(400,80,9)
    +`<path d="M0 ${H}V520Q240 470 520 515T1000 500T${W} 470V${H}Z" fill="${c.mid}"/><path d="M0 ${H}V556Q380 520 760 552T${W} 540V${H}Z" fill="${c.sil}"/>`
    +vignette();
};

S.arcana=c=>{
  const r=rng(23);
  const card=(x,y,rot,kind)=>{
    const w=150,h=250; let art='';
    const cx=w/2, cy=h*.42;
    if(kind==='moon'){ art=`<circle cx="${cx}" cy="${cy}" r="34" fill="none" stroke="${c.gold}" stroke-width="1.6"/><path d="M${cx+8} ${cy-28}A30 30 0 1 0 ${cx+8} ${cy+28}A24 24 0 1 1 ${cx+8} ${cy-28}Z" fill="${c.gold}" opacity=".85"/>`
        +Array.from({length:16},(_,i)=>{ const a=i*Math.PI/8; return `<path d="M${f1(cx+Math.cos(a)*40)} ${f1(cy+Math.sin(a)*40)}L${f1(cx+Math.cos(a)*48)} ${f1(cy+Math.sin(a)*48)}" stroke="${c.gold}" stroke-width="1.2"/>`; }).join('')
        +`<path d="M22 ${h-60}V${h-110}h16v50M${w-38} ${h-60}V${h-110}h16v50" fill="none" stroke="${c.gold}" stroke-width="1.4"/><path d="M${cx} ${h-40}C${cx-20} ${h-70} ${cx+20} ${h-90} ${cx} ${h-118}" fill="none" stroke="${c.gold}" stroke-width="1.2" stroke-dasharray="3 4"/>`; }
    if(kind==='sun'){ art=`<circle cx="${cx}" cy="${cy}" r="26" fill="${c.gold}" opacity=".85"/>`+Array.from({length:12},(_,i)=>{ const a=i*Math.PI/6; return `<path d="M${f1(cx+Math.cos(a)*32)} ${f1(cy+Math.sin(a)*32)}Q${f1(cx+Math.cos(a+.15)*42)} ${f1(cy+Math.sin(a+.15)*42)} ${f1(cx+Math.cos(a)*54)} ${f1(cy+Math.sin(a)*54)}" fill="none" stroke="${c.gold}" stroke-width="1.6"/>`; }).join('')
        +`<path d="M20 ${h-58}H${w-20}M30 ${h-58}V${h-80}M${w-30} ${h-58}V${h-80}" stroke="${c.gold}" stroke-width="1.2"/>`+Array.from({length:5},(_,i)=>`<path d="M${36+i*20} ${h-66}l4 -10l4 10" fill="none" stroke="${c.gold}" stroke-width="1"/>`).join(''); }
    if(kind==='star'){ const st=(x0,y0,R1,R2)=>{ let d=''; for(let i=0;i<16;i++){ const a=i*Math.PI/8-Math.PI/2, rr=i%2?R2:R1; d+=(i?'L':'M')+f1(x0+Math.cos(a)*rr)+' '+f1(y0+Math.sin(a)*rr); } return d+'Z'; };
        art=`<path d="${st(cx,cy,40,14)}" fill="${c.gold}" opacity=".85"/>`+[[30,50],[120,46],[34,120],[118,124],[cx,30],[cx,140]].map(([a,b])=>`<path d="${st(a,b,8,3)}" fill="${c.gold}" opacity=".7"/>`).join('')
        +`<path d="M20 ${h-50}Q${cx} ${h-70} ${w-20} ${h-50}" fill="none" stroke="${c.gold}" stroke-width="1.2"/><path d="M${cx-10} ${h-62}l-14 -22M${cx+10} ${h-62}l14 -22" stroke="${c.gold}" stroke-width="1" opacity=".7"/>`; }
    const name={moon:'THE MOON',sun:'THE SUN',star:'THE STAR'}[kind], num={moon:'XVIII',sun:'XIX',star:'XVII'}[kind];
    return `<g transform="translate(${x} ${y}) rotate(${rot})" opacity=".42"><rect width="${w}" height="${h}" rx="10" fill="${c.card}"/><rect x="6" y="6" width="${w-12}" height="${h-12}" rx="6" fill="none" stroke="${c.gold}" stroke-width="1.6"/>
      <rect x="11" y="11" width="${w-22}" height="${h-22}" rx="4" fill="none" stroke="${c.gold}" stroke-width=".7" opacity=".7"/>
      <text x="${cx}" y="30" text-anchor="middle" font-family="Cinzel, serif" font-size="11" fill="${c.gold}" letter-spacing="2">${num}</text>${art}
      <text x="${cx}" y="${h-20}" text-anchor="middle" font-family="Cinzel, serif" font-size="11" fill="${c.gold}" letter-spacing="2.5">${name}</text></g>`; };
  let phases=''; for(let i=0;i<7;i++){ const x=520+i*54, y=30, f=i/6; phases+=`<circle cx="${x}" cy="${y}" r="10" fill="none" stroke="${c.gold}" stroke-width="1" opacity=".5"/>`;
    if(i!==3) phases+=`<path d="M${x} ${y-10}A10 10 0 0 ${i<3?1:0} ${x} ${y+10}A${f1(Math.abs(10-20*(i<3?f*2:(f-.5)*2)))} 10 0 0 ${(i<3?f*2:(f-.5)*2)>.5?(i<3?1:0):(i<3?0:1)} ${x} ${y-10}Z" fill="${c.gold}" opacity=".5"/>`; else phases+=`<circle cx="${x}" cy="${y}" r="10" fill="${c.gold}" opacity=".5"/>`; }
  const rings=`<g fill="none" stroke="${c.gold}" opacity=".1"><circle cx="680" cy="330" r="300" stroke-width="1"/><circle cx="680" cy="330" r="270" stroke-width=".6" stroke-dasharray="2 7"/><circle cx="680" cy="330" r="220" stroke-width="1"/>
    ${Array.from({length:12},(_,i)=>{ const a=i*Math.PI/6; return `<path d="M${f1(680+Math.cos(a)*220)} ${f1(330+Math.sin(a)*220)}L${f1(680+Math.cos(a)*300)} ${f1(330+Math.sin(a)*300)}" stroke-width=".8"/>`; }).join('')}</g>`;
  return sky(c,c.bg3,c.bg)+stars(r,110,0,0,W,H,'#f1e6ff',true)+rings
    +`<g>${phases}</g>`
    +card(28,120,-7,'moon')+card(1190,140,6,'sun')+card(70,330,4,'star')+card(1150,360,-5,'star').replace('THE STAR','THE STAR')
    +shootingStar(1000,90,4)+shootingStar(360,150,13)
    +vignette();
};

window.OBS_SCENES=S;
window.OBS_SCENE_CSS=`
#thscene{position:absolute;inset:0;pointer-events:none;z-index:0;overflow:hidden;display:none}
#thscene svg{display:block;width:100%;height:100%}
html.thscene #thscene{display:block}
html.thscene #app .pgbg{background:transparent!important}
#thscene .sc-tw{animation:scTw 5s ease-in-out infinite}
@keyframes scTw{0%,100%{opacity:.25}50%{opacity:1}}
#thscene .sc-bat{animation:scBat 24s linear infinite;transform:translate(-80px,var(--y))}
@keyframes scBat{0%{transform:translate(-80px,var(--y))}45%{transform:translate(640px,var(--y2))}100%{transform:translate(1460px,var(--y))}}
#thscene .sc-flap{animation:scFlap .32s ease-in-out infinite;transform-box:fill-box;transform-origin:center}
@keyframes scFlap{0%,100%{transform:scaleY(1)}50%{transform:scaleY(-.55)}}
#thscene .sc-flick{animation:scFlick 2.3s ease-in-out infinite}
@keyframes scFlick{0%,100%{opacity:.75}20%{opacity:.55}45%{opacity:.9}70%{opacity:.6}}
#thscene .sc-flame{animation:scFlame 1.6s ease-in-out infinite}
@keyframes scFlame{0%,100%{transform:scale(1,1)}30%{transform:scale(.92,1.12)}60%{transform:scale(1.06,.9)}}
#thscene .sc-sway{animation:scSway 9s ease-in-out infinite}
@keyframes scSway{0%,100%{transform:rotate(-.8deg)}50%{transform:rotate(.8deg)}}
#thscene .sc-lava{animation:scLava 4s ease-in-out infinite}
@keyframes scLava{0%,100%{opacity:.55}50%{opacity:1}}
#thscene .sc-glow{animation:scLava 6s ease-in-out infinite}
#thscene .sc-aur{animation:scAur 24s ease-in-out infinite;transform-box:fill-box;transform-origin:center}
@keyframes scAur{0%,100%{transform:translateX(-3%) scaleY(1);opacity:.7}50%{transform:translateX(3%) scaleY(1.25);opacity:1}}
#thscene .sc-shoot{opacity:0;animation:scShoot 14s linear infinite}
@keyframes scShoot{0%,90%{opacity:0;transform:translate(0,0)}92%{opacity:1}97%{opacity:0;transform:translate(-160px,-54px)}100%{opacity:0}}
html.thstill #thscene *{animation-play-state:paused!important}
@media (prefers-reduced-motion:reduce){#thscene *{animation:none!important}}`;
})();
</script>
