<script>
/* ===== OBSIDIAN · STUDIO v2: tiny recorder with colour-coded tracks ===== */
(function(){
const BEATS=4, BARS=32, TOTAL=BARS*BEATS, LANE=74;
const TCOL=['var(--accent)','#5fd4c4','#a891ff','#ffb547','#ff7a9a','#6fb7ff','#9be36b','#ff8f5a'];
const css=document.createElement('style'); css.textContent=`
#studio{width:100%;height:100%;border-radius:14px;overflow:hidden;display:flex;flex-direction:column;position:relative;background:radial-gradient(120% 60% at 50% -10%, #222226 0%, #141416 45%, #0d0d0f 100%);box-shadow:inset 0 1px 0 rgba(255,255,255,.09),inset 0 0 0 1px rgba(255,255,255,.06),0 40px 90px rgba(0,0,0,.85);user-select:none;-webkit-user-select:none}
#studio button{font-family:inherit;cursor:pointer;line-height:1}
#studio button:focus{outline:none}#studio button:focus-visible{outline:1.5px solid var(--accent);outline-offset:2px}
/* ---- top ---- */
.st-top{height:78px;flex:none;display:flex;align-items:center;gap:14px;padding:0 16px 0 20px;background:linear-gradient(#1c1c1f, #121214);box-shadow:inset 0 1px 0 rgba(255,255,255,.07),0 -6px 18px rgba(0,0,0,.35);position:relative;z-index:5}
.st-brand{display:flex;flex-direction:column;gap:3px;width:112px;flex:none}
.st-brand b{font-size:14px;font-weight:600;letter-spacing:.34em;color:#f4f1ea}
.st-brand span{font-size:8px;letter-spacing:.18em;color:#77777d;white-space:nowrap}
.st-tr{display:flex;align-items:center;gap:8px;padding:6px 10px;border-radius:30px;background:#0a0a0b;box-shadow:inset 0 2px 8px rgba(0,0,0,.95),0 1px 0 rgba(255,255,255,.07)}
.st-tb{width:36px;height:36px;border:none;border-radius:50%;background:linear-gradient(#26262a, #18181b);box-shadow:inset 0 1px 0 rgba(255,255,255,.1),0 2px 5px rgba(0,0,0,.7);color:#c9c7c1;display:flex;align-items:center;justify-content:center;transition:transform .08s}
.st-tb:active{transform:scale(.94)}
.st-tb.big{width:46px;height:46px;background:radial-gradient(circle at 50% 30%, #3a3a3f, #1c1c1f 70%);color:#f4f1ea}
.st-tb.big.on{background:radial-gradient(circle at 50% 30%, color-mix(in srgb, var(--accent) 60%, #fff 10%), color-mix(in srgb, var(--accent) 45%, #111 55%) 75%);color:#111113;box-shadow:0 0 18px color-mix(in srgb, var(--accent) 55%, transparent),inset 0 1px 0 rgba(255,255,255,.3)}
.st-tb.rec{color:#ff4d4d}
.st-tb.rec.on{background:radial-gradient(circle at 50% 30%, #ff7070, #b81f2a 75%);color:#fff;box-shadow:0 0 20px rgba(255,60,60,.6),inset 0 1px 0 rgba(255,255,255,.3);animation:stpulse 1s ease-in-out infinite}
@keyframes stpulse{50%{box-shadow:0 0 30px rgba(255,60,60,.85),inset 0 1px 0 rgba(255,255,255,.3)}}
.st-tb.tg.on{color:var(--accent);box-shadow:inset 0 0 0 1px color-mix(in srgb, var(--accent) 60%, transparent),0 0 10px color-mix(in srgb, var(--accent) 30%, transparent)}
.st-tsep{width:1px;height:24px;background:rgba(255,255,255,.08);margin:0 2px}
.st-lcd{position:relative;display:flex;align-items:center;gap:16px;height:52px;padding:0 18px;border-radius:14px;background:linear-gradient(#070708, #0d0d0f);box-shadow:inset 0 2px 8px rgba(0,0,0,.95),inset 0 0 0 1px rgba(255,255,255,.04),0 1px 0 rgba(255,255,255,.07)}
.st-lcd>div{display:flex;flex-direction:column;align-items:flex-start;gap:3px}
.st-lcd b{font-size:22px;font-weight:500;font-variant-numeric:tabular-nums;color:var(--accent);letter-spacing:.06em;text-shadow:0 0 10px color-mix(in srgb, var(--accent) 55%, transparent)}
.st-lcd small{font-size:8px;letter-spacing:.22em;color:#5a5a60}
.st-lcd i{width:1px;height:28px;background:rgba(255,255,255,.07)}
.st-lcd .sec{font-size:15px;color:#c9c7c1;text-shadow:none}
.st-lcd .bpm{font-size:15px;color:#c9c7c1;text-shadow:none}
.st-rec-badge{display:none;flex:none;align-items:center;gap:6px;height:22px;padding:0 9px;border-radius:11px;background:rgba(255,60,60,.15);color:#ff6b6b;font-size:10px;font-weight:700;letter-spacing:.18em}
.st-rec-badge::before{content:'';width:7px;height:7px;border-radius:50%;background:#ff4d4d;box-shadow:0 0 8px #ff4d4d;animation:stblink 1s steps(2) infinite}
@keyframes stblink{50%{opacity:.2}}
#studio.recording .st-rec-badge{display:flex}
.st-pill{height:30px;flex:none;white-space:nowrap;padding:0 12px;border-radius:15px;border:1px solid rgba(255,255,255,.12);background:none;color:#a3a3a8;font-size:10px;font-weight:600;letter-spacing:.16em}
.st-pill.snap.on{background:none;color:var(--accent);border-color:color-mix(in srgb, var(--accent) 55%, transparent);box-shadow:none}
.st-pill.on{background:var(--accent);color:#111113;border-color:transparent;box-shadow:0 0 12px color-mix(in srgb, var(--accent) 45%, transparent)}
.st-master{margin-left:auto;display:flex;align-items:center;gap:10px}
.st-master>span{font-size:9px;letter-spacing:.2em;color:#77777d}
.st-mm{display:flex;flex-direction:column;gap:4px;width:120px}
.st-mm div{height:5px;border-radius:3px;background:#0a0a0b;box-shadow:inset 0 1px 2px rgba(0,0,0,.9);position:relative;overflow:hidden}
.st-mm div i{position:absolute;left:0;top:0;bottom:0;width:0;background:linear-gradient(90deg, var(--accent) 0 70%, #ffb547 85%, #ff5a36);background-size:120px 100%;transition:width .07s}
.st-close{width:34px;height:34px;flex:none;border-radius:12px;border:1px solid rgba(255,255,255,.08);background:linear-gradient(#1f1f22, #141416);color:#c9c7c1;display:flex;align-items:center;justify-content:center}
/* ---- body ---- */
.st-body{flex:1;display:flex;min-height:0}
.st-heads{width:300px;flex:none;display:flex;flex-direction:column;background:linear-gradient(90deg, #131315, #161618);box-shadow:inset -1px 0 0 rgba(255,255,255,.06),6px 0 14px rgba(0,0,0,.25);position:relative;z-index:2}
.st-hcorner{height:34px;flex:none;display:flex;align-items:center;justify-content:space-between;padding:0 12px 0 16px;font-size:9px;letter-spacing:.22em;color:#77777d;box-shadow:inset 0 -1px 0 rgba(255,255,255,.06)}
.st-hcorner button{height:22px;padding:0 9px;border-radius:11px;border:1px dashed rgba(255,255,255,.16);background:none;color:#a3a3a8;font-size:9px;font-weight:600;letter-spacing:.16em}
.st-hlist{flex:1;overflow:hidden}
.st-head{height:${LANE}px;display:grid;grid-template-columns:4px minmax(0,1fr) 98px 16px;align-items:stretch;box-shadow:inset 0 -1px 0 rgba(0,0,0,.6),inset 0 1px 0 rgba(255,255,255,.035);background:linear-gradient(#1b1b1e, #161618);position:relative}
.st-head.sel{background:linear-gradient(90deg, color-mix(in srgb, var(--tc) 16%, #1b1b1e), #18181b 75%)}
.st-strip{background:var(--tc);box-shadow:0 0 8px color-mix(in srgb, var(--tc) 50%, transparent)}
.st-hmain{display:flex;flex-direction:column;justify-content:center;gap:8px;padding:0 10px 0 10px;min-width:0}
.st-grp{display:flex;gap:3px}.st-grp+.st-grp{margin-left:8px}
.st-sliders{display:flex;flex-direction:column;justify-content:center;gap:8px;padding-right:6px}
.st-hrow{display:flex;align-items:center;gap:4px}
.st-b{height:22px;width:24px;padding:0;border-radius:4px;border:1px solid #2e2e33;background:linear-gradient(#2a2a2e, #1f1f22);box-shadow:inset 0 1px 0 rgba(255,255,255,.06);color:#b5b3ad;font-size:11px;font-weight:700;display:flex;align-items:center;justify-content:center;flex:none}
.st-b:hover{color:#f4f1ea;border-color:#3a3a3f}
.st-b.m.on{background:#e53935;border-color:#ff5a52;color:#fff;box-shadow:0 0 8px rgba(229,57,53,.45)}
.st-b.s.on{background:#e8c547;border-color:#e8c547;color:#111113}
.st-b.arm{width:24px}.st-b.arm i{width:9px;height:9px;border-radius:50%;background:#77777d}
.st-b.arm.on{background:#e53935;border-color:#ff5a52;box-shadow:0 0 8px rgba(229,57,53,.5)}.st-b.arm.on i{background:#111113}
.st-b.mon.on{background:#ff9f43;border-color:#ff9f43;color:#111113}
.st-b.r.on{background:#3fbf6a;border-color:#3fbf6a;color:#111113}
.st-b.w.on{background:#e5484d;border-color:#e5484d;color:#fff}
.st-b.st{width:24px}
.st-b.st.on{color:var(--tc);border-color:color-mix(in srgb, var(--tc) 55%, #2e2e33)}
.st-vm.mono span:last-child{display:none}.st-vm.mono span{width:4px}
.st-sw{width:12px;height:12px;flex:none;border-radius:3px;border:none;background:var(--tc);box-shadow:0 0 6px color-mix(in srgb, var(--tc) 50%, transparent);padding:0;margin-right:2px}
.st-name{flex:1;min-width:0;font-size:12px;font-weight:500;letter-spacing:.02em;color:#e9e6df;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;outline:none;border-radius:3px;padding:2px 4px}
.st-name:focus{background:#0a0a0b;box-shadow:0 0 0 1px var(--tc);text-overflow:clip}
.st-name.val{color:var(--accent)}
.st-panv{flex:none;min-width:26px;height:16px;padding:0 5px;box-sizing:border-box;border-radius:8px;display:flex;align-items:center;justify-content:center;font-size:9px;font-weight:700;letter-spacing:.06em;font-variant-numeric:tabular-nums;color:var(--tc);background:color-mix(in srgb, var(--tc) 14%, transparent);box-shadow:inset 0 0 0 1px color-mix(in srgb, var(--tc) 35%, transparent)}
.st-panv.c{color:#8d8d93;background:rgba(255,255,255,.04);box-shadow:inset 0 0 0 1px rgba(255,255,255,.08)}
.st-hgap{flex:1}
.st-vol,.st-pan{width:100%;box-sizing:border-box;height:22px;flex:none;display:flex;align-items:center;gap:5px;padding:0 6px;border-radius:4px;border:1px solid #2e2e33;background:#111113;box-shadow:inset 0 1px 3px rgba(0,0,0,.8);cursor:ew-resize;touch-action:none}
.st-vol svg,.st-pan svg{flex:none}
.st-vol .trk,.st-pan .trk{flex:1;height:3px;border-radius:2px;background:#2e2e33;position:relative}
.st-vol .f{position:absolute;left:0;top:0;bottom:0;border-radius:2px;background:var(--tc)}
.st-pan .f{position:absolute;top:0;bottom:0;border-radius:2px;background:var(--tc)}
.st-pan .trk::after{content:'';position:absolute;left:50%;top:-3px;bottom:-3px;width:1px;background:#4b4b51}
.st-vol .k,.st-pan .k{position:absolute;top:50%;width:11px;height:11px;margin:-5.5px 0 0 -5.5px;border-radius:50%;background:radial-gradient(circle at 50% 30%, #f4f1ea, #a3a3a8);box-shadow:0 1px 3px rgba(0,0,0,.8)}
.st-vm{display:flex;gap:2px;justify-content:center;padding:12px 0}
.st-vm span{width:3px;border-radius:2px;background:#0a0a0b;box-shadow:inset 0 1px 2px rgba(0,0,0,.9);position:relative;overflow:hidden}
.st-vm span i{position:absolute;left:0;right:0;bottom:0;height:0;background:linear-gradient(to top, var(--tc) 0 65%, #ffb547 85%, #ff5a36);background-size:100% 56px;background-position:bottom;transition:height .07s}
/* channel popover (e) and take menu */
.st-pop{position:absolute;z-index:30;width:236px;padding:12px;border-radius:12px;background:linear-gradient(#202024, #17171a);box-shadow:inset 0 1px 0 rgba(255,255,255,.08),inset 0 0 0 1px rgba(255,255,255,.06),0 18px 40px rgba(0,0,0,.75);display:none}
.st-pop h4{margin:0 0 10px;font-size:11px;font-weight:600;letter-spacing:.16em;color:#e9e6df;display:flex;align-items:center;gap:8px}
.st-pop h4 i{width:10px;height:10px;border-radius:3px;background:var(--tc)}
.st-pop .row{display:flex;align-items:center;justify-content:space-between;margin:8px 0;font-size:10px;letter-spacing:.14em;color:#8d8d93}
.st-pop .sws{display:flex;gap:6px;flex-wrap:wrap}
.st-pop .sws button{width:20px;height:20px;border-radius:5px;border:none;padding:0;box-shadow:inset 0 0 0 1px rgba(0,0,0,.4)}
.st-pop .sws button.on{box-shadow:0 0 0 2px #f4f1ea}
.st-pop input[type=range]{width:120px;accent-color:var(--tc)}
.st-pop .val{width:42px;text-align:right;color:#e9e6df;font-variant-numeric:tabular-nums;letter-spacing:0}
.st-pop .danger{width:100%;height:28px;margin-top:6px;border-radius:8px;border:1px solid rgba(229,72,77,.35);background:rgba(229,72,77,.08);color:#ff7a7f;font-size:10px;font-weight:600;letter-spacing:.14em}
.st-menu{position:absolute;z-index:31;min-width:190px;padding:5px;border-radius:10px;background:linear-gradient(#202024, #17171a);box-shadow:inset 0 1px 0 rgba(255,255,255,.08),inset 0 0 0 1px rgba(255,255,255,.06),0 18px 40px rgba(0,0,0,.75);display:none;flex-direction:column}
.st-menu button{height:30px;border:none;border-radius:6px;background:none;color:#e9e6df;font-size:12px;text-align:left;padding:0 10px;display:flex;align-items:center;justify-content:space-between;gap:16px}
.st-menu button:hover{background:rgba(255,255,255,.08)}
.st-menu button:disabled{opacity:.35;background:none;cursor:default}
.st-menu kbd{font:inherit;font-size:10px;color:#77777d}
.st-menu hr{border:none;height:1px;background:rgba(255,255,255,.07);margin:4px 2px}
.st-menu .del{color:#ff7a7f}
/* ---- timeline ---- */
.st-main{flex:1;min-width:0;position:relative;display:flex;flex-direction:column}
.st-scroll{flex:1;overflow:auto;position:relative}
.st-scroll::-webkit-scrollbar{height:0;width:0}
.st-ruler{position:sticky;top:0;z-index:3;height:34px;background:linear-gradient(#18181b, #121214);box-shadow:inset 0 -1px 0 rgba(255,255,255,.07),0 4px 10px rgba(0,0,0,.3);cursor:pointer}
.st-ruler .bar{position:absolute;top:0;bottom:0;font-size:10px;color:#5a5a60;padding:5px 0 0 6px;font-variant-numeric:tabular-nums;pointer-events:none}
.st-ruler .bar.m{color:#a3a3a8}
.st-ruler .ticks{position:absolute;left:0;right:0;bottom:0;height:8px;pointer-events:none}
.st-loop{position:absolute;top:19px;height:11px;border-radius:6px;background:rgba(255,255,255,.07);cursor:grab;touch-action:none}
.st-loop.on{background:linear-gradient(90deg, color-mix(in srgb, var(--accent) 45%, transparent), color-mix(in srgb, var(--accent) 25%, transparent));box-shadow:inset 0 0 0 1px var(--accent),0 0 10px color-mix(in srgb, var(--accent) 35%, transparent)}
.st-loop i{position:absolute;top:1px;bottom:1px;width:4px;border-radius:2px;background:rgba(255,255,255,.35);cursor:ew-resize}.st-loop i:first-child{left:2px}.st-loop i:last-child{right:2px}
.st-loopshade{position:absolute;top:34px;bottom:0;pointer-events:none;background:color-mix(in srgb, var(--accent) 5%, transparent);box-shadow:inset 1px 0 0 color-mix(in srgb, var(--accent) 35%, transparent),inset -1px 0 0 color-mix(in srgb, var(--accent) 35%, transparent);display:none}
.st-loopshade.on{display:block}
.st-lane{height:${LANE}px;position:relative;box-shadow:inset 0 -1px 0 rgba(255,255,255,.05)}
.st-lane:nth-child(even){background-color:rgba(255,255,255,.012)}
.st-lane.sel{background-color:color-mix(in srgb, var(--tc) 5%, transparent)}
.st-lane.rec{background-color:rgba(255,60,60,.05);box-shadow:inset 0 0 30px rgba(255,60,60,.08),inset 0 -1px 0 rgba(255,255,255,.05)}
.st-clip{position:absolute;top:5px;bottom:5px;border-radius:9px;overflow:hidden;cursor:grab;touch-action:none;background:linear-gradient(color-mix(in srgb, var(--tc) 30%, #111113), color-mix(in srgb, var(--tc) 12%, #0d0d0f));box-shadow:inset 0 0 0 1px color-mix(in srgb, var(--tc) 55%, transparent),0 4px 10px rgba(0,0,0,.45)}
.st-clip::before{content:'';position:absolute;left:0;right:0;top:0;height:17px;background:color-mix(in srgb, var(--tc) 38%, transparent)}
.st-clip.sel{box-shadow:inset 0 0 0 2px var(--tc),0 0 18px color-mix(in srgb, var(--tc) 45%, transparent),0 4px 10px rgba(0,0,0,.45)}
.st-clip.live{--tc:#ff4d4d}
.st-clip.play{filter:brightness(1.18)}
.st-clip.muted{filter:grayscale(1);opacity:.35}
.st-clip span{position:absolute;left:8px;top:3px;font-size:10px;font-weight:700;letter-spacing:.1em;color:#f4f1ea;white-space:nowrap;pointer-events:none;text-shadow:0 1px 2px rgba(0,0,0,.6)}
.st-clip svg{position:absolute;left:0;top:18px;pointer-events:none}
.st-clip .tr,.st-clip .tl{position:absolute;top:0;bottom:0;width:9px;cursor:ew-resize}.st-clip .tr{right:0}.st-clip .tl{left:0}
.st-clip .tr::after,.st-clip .tl::after{content:'';position:absolute;top:50%;width:3px;height:18px;margin-top:-9px;border-radius:2px;background:rgba(255,255,255,.4);opacity:0;transition:opacity .12s}
.st-clip .tr::after{right:3px}.st-clip .tl::after{left:3px}
.st-clip:hover .tr::after,.st-clip:hover .tl::after,.st-clip.sel .tr::after,.st-clip.sel .tl::after{opacity:1}
.st-ph{position:absolute;top:0;bottom:0;width:2px;margin-left:-1px;z-index:4;pointer-events:none;background:var(--accent);box-shadow:0 0 10px var(--accent),0 0 2px var(--accent)}
.st-ph::before{content:'';position:absolute;top:2px;left:-6px;width:14px;height:12px;background:var(--accent);clip-path:polygon(0 0,100% 0,100% 55%,50% 100%,0 55%);filter:drop-shadow(0 0 4px var(--accent))}
#studio.recording .st-ph{background:#ff4d4d;box-shadow:0 0 12px #ff4d4d}#studio.recording .st-ph::before{background:#ff4d4d}
/* overview strip */
.st-ovw{height:50px;flex:none;position:relative;margin:8px 12px 10px;border-radius:10px;background:#0a0a0b;box-shadow:inset 0 2px 6px rgba(0,0,0,.9),0 1px 0 rgba(255,255,255,.05);overflow:hidden;cursor:pointer}
.st-ovw b{position:absolute;border-radius:2px;opacity:.75}
.st-ovw .view{position:absolute;top:2px;bottom:2px;border-radius:7px;box-shadow:inset 0 0 0 1px rgba(255,255,255,.35);background:rgba(255,255,255,.05);cursor:grab}
.st-ovw .oph{position:absolute;top:0;bottom:0;width:1px;background:var(--accent)}
.st-ovw small{position:absolute;right:8px;top:4px;font-size:8px;letter-spacing:.2em;color:#5a5a60;pointer-events:none}
/* footer */
.st-foot{height:52px;flex:none;display:flex;align-items:center;gap:8px;padding:0 16px;background:linear-gradient(#141416, #101012);box-shadow:inset 0 1px 0 rgba(255,255,255,.06)}
.st-fb{height:32px;padding:0 13px;border-radius:10px;border:none;background:linear-gradient(#1f1f22, #18181b);box-shadow:inset 0 1px 0 rgba(255,255,255,.07),0 1px 3px rgba(0,0,0,.6);color:#c9c7c1;font-size:10px;font-weight:600;letter-spacing:.14em;display:flex;align-items:center;gap:7px}
.st-fb svg{opacity:.8}
.st-fb:disabled{opacity:.3;cursor:default}
.st-fb:not(:disabled):hover{color:#f4f1ea}
.st-fb.on{color:var(--accent)}
.st-sep{width:1px;height:20px;background:rgba(255,255,255,.08);margin:0 4px}
.st-zoom{display:flex;align-items:center;gap:8px;font-size:9px;letter-spacing:.18em;color:#77777d}
.st-zoom input{width:110px;accent-color:var(--accent)}

`; document.head.appendChild(css);

const sv=(d,w=16)=>'<svg width="'+w+'" height="'+w+'" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">'+d+'</svg>';
const MO_ICO='<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><circle cx="12" cy="12" r="6"/></svg>';
const ST_ICO='<svg width="17" height="13" viewBox="0 0 32 24" fill="none" stroke="currentColor" stroke-width="2.2"><circle cx="11" cy="12" r="6"/><circle cx="21" cy="12" r="6"/></svg>';
const I={ start:'<svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor"><rect x="4" y="5" width="2.6" height="14" rx="1"/><path d="M20 5.5v13L9 12z"/></svg>',
  play:'<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor"><path d="M8 4.8v14.4L19.5 12z"/></svg>',
  pause:'<svg width="17" height="17" viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="5" width="4" height="14" rx="1.2"/><rect x="14" y="5" width="4" height="14" rx="1.2"/></svg>',
  stop:'<svg width="13" height="13" viewBox="0 0 24 24" fill="currentColor"><rect x="5" y="5" width="14" height="14" rx="2.5"/></svg>',
  rec:'<svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor"><circle cx="12" cy="12" r="7"/></svg>',
  loop:sv('<path d="M17 2l3 3-3 3"/><path d="M4 11V9a4 4 0 0 1 4-4h12"/><path d="M7 22l-3-3 3-3"/><path d="M20 13v2a4 4 0 0 1-4 4H4"/>'),
  click:sv('<path d="M9 3h6l4 18H5z"/><path d="M12 15l5-9"/>'),
  close:sv('<path d="M6 6l12 12M18 6L6 18"/>',14),
  split:sv('<circle cx="6" cy="6" r="3"/><circle cx="6" cy="18" r="3"/><path d="M20 4L8.1 15.9M14.5 14.5L20 20M8.1 8.1L12 12"/>',14),
  dup:sv('<rect x="9" y="9" width="12" height="12" rx="2"/><path d="M5 15V5a2 2 0 0 1 2-2h8"/>',14),
  del:sv('<path d="M3 6h18M8 6V4h8v2M19 6l-1 14H6L5 6"/>',14),
  snap:sv('<path d="M6 3v8a6 6 0 0 0 12 0V3"/><path d="M6 7h4M14 7h4"/>',14),
  studio:'<svg aria-hidden="true" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M3 12h2M7 8v8M11 5v14M15 9v6M19 7v10"/></svg>' };
const ICO={ guitar:sv('<path d="M19 3l2 2-6.5 6.5"/><circle cx="9" cy="15" r="5"/><path d="M9 15l2-2"/>',14),
  lead:sv('<path d="M13 2L4 14h7l-1 8 9-12h-7z"/>',14), synth:sv('<path d="M2 12c2-6 4-6 6 0s4 6 6 0 4-6 6 0"/>',14), mic:sv('<rect x="9" y="2" width="6" height="12" rx="3"/><path d="M5 11a7 7 0 0 0 14 0M12 18v4"/>',14) };

/* ---------- markup ---------- */
const ov=document.createElement('div'); ov.className='ov'; ov.dataset.ov='studio'; ov.style.cssText='display: none; position: absolute; inset: 0; z-index: 20';
ov.innerHTML=`<div style="position: absolute; inset: 0; background: rgba(6,6,7,.82); backdrop-filter: blur(10px); -webkit-backdrop-filter: blur(10px); display: flex; align-items: center; justify-content: center">
<div id="studio" role="dialog" aria-label="Studio">
 <div class="st-body">
  <div class="st-heads"><div class="st-hcorner">TRACKS<button data-a="addtrack" title="Add a track">+ ADD</button></div><div class="st-hlist" id="st-hlist"></div></div>
  <div class="st-main">
   <div class="st-scroll" id="st-scroll"><div id="st-canvas" style="position: relative; min-height: 100%"><div class="st-ruler" id="st-ruler"></div><div class="st-loopshade" id="st-lshade"></div><div id="st-lanes"></div><div class="st-ph" id="st-ph"></div></div></div>
  </div>
 </div>
 <div class="st-top">
  <div class="st-brand"><b>STUDIO</b><span>OBSIDIAN RECORDER</span></div>
  <div class="st-tr">
   <button class="st-tb" data-a="start" aria-label="Back to start" title="Back to start (Home)">${I.start}</button>
   <button class="st-tb" data-a="stop" aria-label="Stop" title="Stop">${I.stop}</button>
   <button class="st-tb big" data-a="play" aria-label="Play" title="Play / pause (Space)">${I.play}</button>
   <button class="st-tb big rec" data-a="rec" aria-label="Record" title="Record onto the armed track (R)">${I.rec}</button>
   <span class="st-tsep"></span>
   <button class="st-tb tg" data-a="loop" aria-label="Loop" title="Loop the marked bars (L)">${I.loop}</button>
   <button class="st-tb tg" data-a="click" aria-label="Click" title="Metronome click while playing">${I.click}</button>
  </div>
  <div class="st-lcd">
   <div><small>BAR · BEAT</small><b id="st-bbt">1.1.00</b></div><i></i>
   <div><small>TIME</small><b class="sec" id="st-sec">0:00.0</b></div><i></i>
   <div><small>TEMPO</small><b class="bpm" id="st-bpm">120</b></div>
  </div>
  <button class="st-pill" data-a="count" title="One bar of clicks before recording starts">COUNT-IN</button>
  <button class="st-pill" data-a="export" title="Mix every track down to a WAV file">EXPORT</button>
  <button class="st-pill snap on" data-a="snap" title="Snap takes to the beat grid when you move or trim them. Ctrl+scroll zooms.">SNAP ON</button>
  <span class="st-rec-badge">REC</span>
  <div class="st-master"><span>MASTER</span><div class="st-mm"><div><i id="st-ml"></i></div><div><i id="st-mr"></i></div></div></div>
  <button class="st-close" aria-label="Close studio">${I.close}</button>
 </div>
</div></div>`;
app.appendChild(ov);
const S=ov.querySelector('#studio'), $s=q=>S.querySelector(q);
const hlist=$s('#st-hlist'), lanes=$s('#st-lanes'), ruler=$s('#st-ruler'), canvas=$s('#st-canvas'), scroller=$s('#st-scroll'), ph=$s('#st-ph'), ovw=$s('#st-ovw'), lshade=$s('#st-lshade');

/* ---------- footer button next to SETTINGS ---------- */
/* both footers get it: the normal one and the one with the live tuner (it used to vanish while the live tuner was on) */
let sb=$('#studio-btn');
if(!sb){ $$('button').filter(b=>b.textContent.trim()==='SETTINGS'&&!b.closest('.ov')).forEach((setBtn,i)=>{
  const sep=setBtn.previousElementSibling.cloneNode(true), btn=setBtn.cloneNode(true); btn.innerHTML=I.studio+'STUDIO';
  if(i===0){ btn.id='studio-btn'; sb=btn; } else btn.classList.add('studio-btn2');
  setBtn.after(sep,btn); btn.addEventListener('click',()=>{ openOv('studio'); render(); }); }); }
else sb.addEventListener('click',()=>{ openOv('studio'); render(); });
try{ TTIP.STUDIO='Tiny recorder: record takes, move them around, loop and play back'; }catch(_){}

/* ---------- state ---------- */
let ppb=16, snap=1;
const SNAPS=[[0,'OFF'],[1,'ON']];   /* ON snaps takes to the beat grid */
const mkT=(name,ico,ci)=>({name,ico,c:TCOL[ci%TCOL.length],vol:.8,pan:0,m:false,s:false,arm:false,mon:false,r:false,w:false,st:true});
let tracks=[mkT('Track 1','guitar',0)]; tracks[0].arm=true;   /* starts empty: one armed track */
let clips=[], selClip=null, selTrack=0, nid=1;
let pos=0, playing=false, recording=false, loopOn=false, clickOn=false, countIn=false, loopS=0, loopE=16;
let liveClip=null, t0=0, p0=0, raf=0, lastBeat=-1, counting=false;
const mkPeaks=(n,seed)=>{ let s=seed*9301+49297, v=.5; const out=[]; for(let i=0;i<n;i++){ s=(s*9301+49297)%233280; const r=s/233280; v=Math.max(.08,Math.min(1,v*.82+r*.36)); out.push(Math.min(1,v+((i%8===0)?.35:0))); } return out; };
function addClip(track,start,len,name,peaks){ const c={id:nid++,track,start,len,off:0,name,peaks:peaks||mkPeaks(Math.ceil(len*8)+8,nid*7)}; clips.push(c); return c; }

/* ---------- the plugin's recorder ----------
   In the app, window.obsStudioEngine (set by obsidian-bridge.js) records what you hear and plays the takes;
   this page owns the arrangement and sends it on every change. Without it (a browser) the page plays pretend. */
const E=()=>window.obsStudioEngine||null;
let engPos=null, pendingClip=null, syncT=0;
function sync(){ const e=E(); if(!e) return; clearTimeout(syncT); syncT=setTimeout(()=>e.sync({ bpm,
  tracks:tracks.map((t,i)=>({vol:t.vol,pan:t.pan,on:audible(i),st:!!t.st})),
  clips:clips.filter(c=>c.src).map(c=>({src:c.src,track:c.track,start:c.start,len:c.len,off:c.off})),
  loop:{on:loopOn,s:loopS,e:loopE}, click:clickOn }),25); }
window.obsStudio={ event(st){
  engPos=st.playing?st.pos:null;
  if(st.peaks&&liveClip) liveClip.peaks.push(...st.peaks);
  if(st.take){ const c=pendingClip;
    if(c){ if(st.take.discarded){ clips=clips.filter(x=>x!==c); if(selClip===c) selClip=null; }
      else { c.src=st.take.id; c.peaks=st.take.peaks.slice(); c.len=st.take.len; c.off=0; c.pending=false; toast(c.name+' recorded'+(st.take.file?' · saved in Recordings':'')); } }
    pendingClip=null; render(); }
}, sync };

/* ---------- helpers ---------- */
const audible=t=>{ const anyS=tracks.some(x=>x.s); return !tracks[t].m && (!anyS||tracks[t].s); };
const dbOf=v=>v<=.001?-Infinity:20*Math.log10(v/.8);
const fmtVol=v=>{ const d=dbOf(v); return !isFinite(d)?'−∞ dB':(d>0?'+':d<0?'−':'')+Math.abs(d).toFixed(1)+' dB'; };
const panFill=p=>'left:'+(p<0?50+p*50:50)+'%;width:'+Math.abs(p*50)+'%';
const fmtPan=p=>Math.abs(p)<.02?'C':(p<0?'L':'R')+Math.round(Math.abs(p)*100);
function ring(v,c,bip){ const d=270*v; let a=0,b=d; if(bip){ a=Math.min(135,d); b=Math.max(135,d); }
  return 'conic-gradient(from 225deg, #2b2b30 0deg '+a+'deg, '+c+' '+a+'deg '+b+'deg, #2b2b30 '+b+'deg 270deg, transparent 270deg 360deg)'; }
function waveSVG(c){ const w=Math.max(2,c.len*ppb), h=LANE-10-21, mid=h/2, n=c.peaks.length, step=8;
  let top='', bot='', top2='', bot2=''; const from=Math.floor(c.off*step), to=Math.min(n,from+Math.ceil(c.len*step)+1);
  for(let i=from;i<to;i++){ const x=((i-from)/step*ppb).toFixed(1), a=c.peaks[i]*mid*.86, a2=a*.45;
    top+=(i===from?'M':'L')+x+' '+(mid-a).toFixed(1); bot='L'+x+' '+(mid+a).toFixed(1)+bot;
    top2+=(i===from?'M':'L')+x+' '+(mid-a2).toFixed(1); bot2='L'+x+' '+(mid+a2).toFixed(1)+bot2; }
  if(!top) return '<svg width="'+w+'" height="'+h+'"></svg>';
  return '<svg width="'+w+'" height="'+h+'"><path d="'+top+bot+'Z" style="fill: var(--tc); opacity: .45"/><path d="'+top2+bot2+'Z" style="fill: var(--tc); opacity: .78"/><path d="M0 '+mid+' H'+w+'" style="stroke: var(--tc); stroke-width: .6; opacity: .5"/></svg>'; }

/* ---------- drawing ---------- */
function render(){
  const W=TOTAL*ppb; canvas.style.width=W+'px';
  let r=''; for(let b=0;b<BARS;b++) r+='<span class="bar'+(b%4===0?' m':'')+'" style="left:'+(b*BEATS*ppb)+'px;width:'+(BEATS*ppb)+'px">'+(b+1)+'</span>';
  r+='<div class="ticks" style="background:repeating-linear-gradient(90deg, rgba(255,255,255,.28) 0 1px, transparent 1px '+(BEATS*ppb)+'px) 0 0/100% 8px no-repeat, repeating-linear-gradient(90deg, rgba(255,255,255,.12) 0 1px, transparent 1px '+ppb+'px) 0 4px/100% 4px no-repeat"></div>';
  r+='<div class="st-loop'+(loopOn?' on':'')+'" id="st-loopb" style="left:'+(loopS*ppb)+'px;width:'+((loopE-loopS)*ppb)+'px" title="Loop range: drag to move, drag the ends to resize"><i></i><i></i></div>';
  ruler.innerHTML=r;
  lshade.className='st-loopshade'+(loopOn?' on':''); lshade.style.left=(loopS*ppb)+'px'; lshade.style.width=((loopE-loopS)*ppb)+'px';
  hlist.innerHTML=tracks.map((t,i)=>`<div class="st-head${i===selTrack?' sel':''}" data-t="${i}" style="--tc:${t.c}">
     <div class="st-strip"></div>
     <div class="st-hmain">
      <div class="st-hrow">
       <button class="st-sw" data-a="color" title="Track colour (click to change)" aria-label="Track colour"></button>
       <span class="st-name" spellcheck="false" title="Double-click to rename">${t.name}</span>
       <span class="st-panv${Math.abs(t.pan)<.02?' c':''}" title="Pan (double-click the pan slider to centre it)">${fmtPan(t.pan)}</span>
      </div>
      <div class="st-hrow">
       <div class="st-grp">
        <button class="st-b arm${t.arm?' on':''}" data-a="arm" aria-label="Arm ${t.name} for recording" title="Record enable"><i></i></button>
        <button class="st-b mon${t.mon?' on':''}" data-a="mon" aria-label="Monitor" title="Monitor: hear the input while armed"><svg width="12" height="12" viewBox="0 0 24 24" fill="currentColor"><path d="M4 9h4l5-4v14l-5-4H4z"/><path d="M16 9a4 4 0 0 1 0 6" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg></button>
        <button class="st-b st${t.st?' on':''}" data-a="stereo" aria-label="${t.st?'Stereo':'Mono'}" title="${t.st?'Stereo: click for mono':'Mono: click for stereo'}">${t.st?ST_ICO:MO_ICO}</button>
       </div>
       <div class="st-grp">
        <button class="st-b m${t.m?' on':''}" data-a="mute" title="Mute">M</button><button class="st-b s${t.s?' on':''}" data-a="solo" title="Solo">S</button>
       </div>
      </div>
     </div>
     <div class="st-sliders">
      <div class="st-vol" data-k="vol" title="Volume ${fmtVol(t.vol)}. Drag, double-click resets"><svg width="11" height="8" viewBox="0 0 12 9"><path d="M0 9 L12 0 L12 9 Z" style="fill: #77777d"/></svg><div class="trk"><div class="f" style="width:${t.vol*100}%"></div><div class="k" style="left:${t.vol*100}%"></div></div></div>
      <div class="st-pan" data-k="pan" title="Pan ${fmtPan(t.pan)}. Drag, double-click centres"><svg width="11" height="8" viewBox="0 0 12 9"><path d="M0 4.5 L4 1 V8 Z M12 4.5 L8 1 V8 Z" style="fill: #77777d"/></svg><div class="trk"><div class="f" style="${panFill(t.pan)}"></div><div class="k" style="left:${(t.pan+1)*50}%"></div></div></div>
     </div>
     <div class="st-vm${t.st?'':' mono'}"><span><i></i></span><span><i></i></span></div></div>`).join('');
  $s('[data-a="addtrack"]').style.display=tracks.length<8?'':'none';
  const grid='repeating-linear-gradient(90deg, rgba(255,255,255,.075) 0 1px, transparent 1px '+(BEATS*ppb)+'px), repeating-linear-gradient(90deg, rgba(255,255,255,.025) 0 1px, transparent 1px '+ppb+'px)';
  lanes.innerHTML=tracks.map((t,i)=>'<div class="st-lane'+(i===selTrack?' sel':'')+(recording&&t.arm?' rec':'')+'" data-t="'+i+'" style="--tc:'+t.c+';background-image:'+grid+'"></div>').join('');
  clips.forEach(c=>{ const lane=lanes.children[c.track]; if(!lane) return; const d=document.createElement('div');
    d.className='st-clip'+(c===selClip?' sel':'')+(c===liveClip?' live':'')+(audible(c.track)?'':' muted'); d.dataset.id=c.id;
    d.style.left=(c.start*ppb)+'px'; d.style.width=Math.max(2,c.len*ppb)+'px';
    d.innerHTML='<span>'+c.name+'</span>'+waveSVG(c)+'<div class="tl"></div><div class="tr"></div>'; lane.appendChild(d); });
  hlist.scrollTop=scroller.scrollTop;
  drawOvw(); drawPH(); tools(); sync(); }
function drawOvw(){ if(!ovw) return; const W=ovw.clientWidth||900, H=50, n=tracks.length, rowH=Math.min(10,(H-12)/n);
  let h='<small>OVERVIEW</small>'; clips.forEach(c=>{ h+='<b style="left:'+(c.start/TOTAL*W).toFixed(1)+'px;width:'+Math.max(2,c.len/TOTAL*W).toFixed(1)+'px;top:'+(6+c.track*rowH).toFixed(1)+'px;height:'+(rowH-2).toFixed(1)+'px;background:'+(c===liveClip?'#ff4d4d':tracks[c.track].c)+(audible(c.track)?'':';opacity:.2')+'"></b>'; });
  const vw=scroller.clientWidth/(TOTAL*ppb)*W, vx=scroller.scrollLeft/(TOTAL*ppb)*W;
  h+='<div class="view" style="left:'+vx.toFixed(1)+'px;width:'+Math.min(W,vw).toFixed(1)+'px"></div><div class="oph" style="left:'+(pos/TOTAL*W).toFixed(1)+'px"></div>';
  ovw.innerHTML=h; }
function drawPH(){ ph.style.left=(pos*ppb)+'px'; const beat=Math.floor(pos), bar=Math.floor(beat/BEATS)+1, bt=beat%BEATS+1, tk=Math.floor((pos-beat)*100);
  $s('#st-bbt').textContent=bar+'.'+bt+'.'+String(tk).padStart(2,'0');
  const sec=pos*60/bpm; $s('#st-sec').textContent=Math.floor(sec/60)+':'+(sec%60).toFixed(1).padStart(4,'0');
  $s('#st-bpm').textContent=bpm; const o=ovw&&ovw.querySelector('.oph'); if(o) o.style.left=(pos/TOTAL*(ovw.clientWidth||900)).toFixed(1)+'px'; }
function tools(){ const sn=$s('[data-a="snap"]'); sn.textContent='SNAP '+SNAPS.find(x=>x[0]===snap)[1]; sn.classList.toggle('on',snap>0);
  const pb=$s('[data-a="play"]'); pb.innerHTML=playing&&!recording&&!counting?I.pause:I.play; pb.classList.toggle('on',playing&&!recording&&!counting);
  $s('[data-a="rec"]').classList.toggle('on',recording||counting); $s('[data-a="loop"]').classList.toggle('on',loopOn); $s('[data-a="click"]').classList.toggle('on',clickOn); $s('[data-a="count"]').classList.toggle('on',countIn);
  S.classList.toggle('recording',recording||counting); sync(); }
const vms=()=>[...hlist.querySelectorAll('.st-vm')];
function clearMeters(){ vms().forEach(m=>m.querySelectorAll('i').forEach(i=>i.style.height=0)); $s('#st-ml').style.width=$s('#st-mr').style.width=0; lanes.querySelectorAll('.st-clip.play').forEach(c=>c.classList.remove('play')); }

/* ---------- transport ---------- */
let AC=null; function blip(acc){ try{ if(!AC) AC=new (window.AudioContext||window.webkitAudioContext)(); const o=AC.createOscillator(), g=AC.createGain(); o.frequency.value=acc?1600:1100; g.gain.setValueAtTime(.18,AC.currentTime); g.gain.exponentialRampToValueAtTime(.0001,AC.currentTime+.05); o.connect(g).connect(AC.destination); o.start(); o.stop(AC.currentTime+.06); }catch(_){} }
function tick(now){ const b=p0+(now-t0)/1000*bpm/60;
  if(counting){ const cb=Math.floor(b-p0); if(cb!==lastBeat){ lastBeat=cb; if(!E()) blip(cb%BEATS===0); } if(b-p0>=BEATS){ counting=false; beginRec(); return; } raf=requestAnimationFrame(tick); return; }
  const follow=!!E()&&engPos!=null;            /* the app: the engine's playhead (it loops and clicks itself) */
  pos=follow?engPos:b;
  if(!follow&&loopOn&&!recording&&pos>=loopE){ p0=loopS; t0=now; pos=loopS; lastBeat=-1; }
  if(pos>=TOTAL){ stop(false); return; }
  const fb=Math.floor(pos); if(fb!==lastBeat){ lastBeat=fb; if(clickOn&&!E()) blip(fb%BEATS===0); }
  if(recording&&liveClip){ liveClip.len=Math.max(.01,pos-liveClip.start); const need=Math.ceil(liveClip.len*8); while(!E()&&liveClip.peaks.length<need){ const prev=liveClip.peaks[liveClip.peaks.length-1]||.4; liveClip.peaks.push(Math.max(.06,Math.min(1,prev*.8+Math.random()*.4+(liveClip.peaks.length%8===0?.3:0)))); }
    const el=lanes.querySelector('.st-clip[data-id="'+liveClip.id+'"]'); if(el){ el.style.width=Math.max(2,liveClip.len*ppb)+'px'; el.querySelector('svg').outerHTML=waveSVG(liveClip); } }
  /* meters + clip highlight */
  const ms=vms(); let L=0,R=0;
  tracks.forEach((t,i)=>{ let lv=0; const on=clips.filter(c=>c.track===i&&pos>=c.start&&pos<c.start+c.len);
    if(audible(i)&&on.length){ const c=on[0]; const pk=c.peaks[Math.floor((pos-c.start+c.off)*8)]||.5; lv=t.vol*(.35+.65*pk); }
    if(recording&&t.arm) lv=.55+Math.random()*.45;
    const l=lv*Math.min(1,1-t.pan)*(.9+Math.random()*.1), r=lv*Math.min(1,1+t.pan)*(.9+Math.random()*.1), mo=lv*(.9+Math.random()*.1); L=Math.max(L,l); R=Math.max(R,r);
    if(ms[i]){ const s=ms[i].querySelectorAll('i'); s[0].style.height=((t.st?l:mo)*100)+'%'; s[1].style.height=(r*100)+'%'; } });
  $s('#st-ml').style.width=(L*100)+'%'; $s('#st-mr').style.width=(R*100)+'%';
  lanes.querySelectorAll('.st-clip').forEach(el=>{ const c=clips.find(x=>x.id===+el.dataset.id); el.classList.toggle('play',!!c&&pos>=c.start&&pos<c.start+c.len); });
  const sx=pos*ppb; if(sx>scroller.scrollLeft+scroller.clientWidth-60){ scroller.scrollLeft=sx-120; drawOvw(); }
  drawPH(); raf=requestAnimationFrame(tick); }
function play(){ if(playing) return; playing=true; t0=performance.now(); p0=pos; lastBeat=-1; engPos=null; if(E()){ sync(); E().play(pos); } raf=requestAnimationFrame(tick); tools(); }
function pause(){ if(recording) return stop(false); playing=false; if(E()) E().stop(); engPos=null; cancelAnimationFrame(raf); clearMeters(); tools(); }
function stop(rewind=true){ if(E()&&(playing||counting||recording)) E().stop(); engPos=null; if(recording) endRec(); playing=false; counting=false; cancelAnimationFrame(raf); clearMeters(); if(rewind) pos=loopOn?loopS:0; render(); }
/* a jump while playing: the engine plays on from there */
function rejump(){ if(playing&&!recording&&!counting){ t0=performance.now(); p0=pos; engPos=null; if(E()) E().play(pos); } }
function record(){ if(recording||counting) return stop(false); const t=tracks.findIndex(x=>x.arm); if(t<0){ toast('Arm a track first (the round button)'); return; }
  if(playing){ cancelAnimationFrame(raf); playing=false; if(E()) E().stop(); }
  if(E()){ sync(); E().record(t,pos,countIn?BEATS:0); engPos=null; }
  if(countIn){ counting=true; playing=true; t0=performance.now(); p0=pos; lastBeat=-1; raf=requestAnimationFrame(tick); tools(); return; }
  beginRec(); }
function beginRec(){ const t=tracks.findIndex(x=>x.arm); const n=clips.filter(c=>/^TAKE/.test(c.name)).length+1;
  liveClip=addClip(t,pos,.01,'TAKE '+n,[]); selClip=liveClip; selTrack=t; recording=true; playing=true; t0=performance.now(); p0=pos; lastBeat=-1; render(); raf=requestAnimationFrame(tick); }
function endRec(){ recording=false;
  if(E()){ if(liveClip){ liveClip.pending=true; pendingClip=liveClip; } liveClip=null; return; }     /* the engine sends the finished take */
  if(liveClip){ if(liveClip.len<.25){ clips=clips.filter(c=>c!==liveClip); selClip=null; } else { liveClip.peaks.push(...mkPeaks(8,3).map(x=>x*.3)); toast(liveClip.name+' recorded'); } } liveClip=null; }

/* ---------- editing ---------- */
const snapTo=b=>snap?Math.round(b/snap)*snap:b;
function split(){ const c=selClip; if(!c||!(pos>c.start&&pos<c.start+c.len)) return; const cut=pos-c.start;
  const n=addClip(c.track,pos,c.len-cut,c.name+' B',c.peaks); n.off=c.off+cut; c.len=cut; selClip=n; render(); }
function dup(){ const c=selClip; if(!c) return; const n=addClip(c.track,c.start+c.len,c.len,c.name,c.peaks); n.off=c.off; selClip=n; render(); }
function del(){ if(!selClip) return; clips=clips.filter(c=>c!==selClip); selClip=null; render(); }

S.addEventListener('click',e=>{ const b=e.target.closest('button'); if(!b) return; const a=b.dataset.a, head=b.closest('.st-head'), ti=head?+head.dataset.t:-1;
  if(b.classList.contains('st-close')) return closeStudio();
  if(a==='play') return playing&&!recording&&!counting?pause():play();
  if(a==='stop') return stop(true);
  if(a==='start'){ pos=loopOn?loopS:0; rejump(); scroller.scrollLeft=0; drawOvw(); return drawPH(); }
  if(a==='export'){ if(E()){ stop(false); E().export(); } else toast('EXPORT writes a WAV of the mix in the app'); return; }
  if(a==='rec') return record();
  if(a==='loop'){ loopOn=!loopOn; return render(); }
  if(a==='click'){ clickOn=!clickOn; return tools(); }
  if(a==='count'){ countIn=!countIn; return tools(); }
  if(a==='split') return split(); if(a==='dup') return dup(); if(a==='del') return del();
  if(a==='snap'){ const i=SNAPS.findIndex(x=>x[0]===snap); snap=SNAPS[(i+1)%SNAPS.length][0]; return tools(); }
  if(a==='arm'){ tracks.forEach((t,i)=>t.arm=i===ti?!t.arm:false); return render(); }
  if(a==='mute'){ tracks[ti].m=!tracks[ti].m; return render(); }
  if(a==='mon'){ tracks[ti].mon=!tracks[ti].mon; return render(); }
  if(a==='read'){ tracks[ti].r=!tracks[ti].r; return render(); }
  if(a==='write'){ tracks[ti].w=!tracks[ti].w; if(tracks[ti].w) toast('Write automation on: knob moves while playing get recorded (plugin build)'); return render(); }
  if(a==='stereo'){ const t=tracks[ti]; t.st=!t.st; toast(t.name+': '+(t.st?'stereo':'mono')); return render(); }
  if(a==='color'){ const t=tracks[ti]; const i=TCOL.indexOf(t.c); t.c=TCOL[(i+1)%TCOL.length]; return render(); }
  if(a==='hmenu'){ const t=+b.dataset.t, m=b.dataset.m; hideMenus();
    if(m==='rename'){ const n=hlist.children[t].querySelector('.st-name'); n.contentEditable='true'; n.focus(); document.getSelection().selectAllChildren(n); }
    if(m==='color'){ const i=TCOL.indexOf(tracks[t].c); tracks[t].c=TCOL[(i+1)%TCOL.length]; render(); }
    if(m==='stereo'){ tracks[t].st=!tracks[t].st; render(); } return; }
  if(a==='menu'){ const m=b.dataset.m; hideMenus(); if(m==='split') split(); if(m==='dup') dup(); if(m==='del') del(); if(m==='rename') renameClip(); if(m==='color'){ const t=tracks[selClip.track]; } return; }
  if(a==='deltrack'){ const t=+b.dataset.t; if(tracks.length<=1) return toast('Keep at least one track'); clips=clips.filter(c=>c.track!==t).map(c=>{ if(c.track>t) c.track--; return c; }); tracks.splice(t,1); selTrack=Math.min(selTrack,tracks.length-1); selClip=null; hideMenus(); return render(); }
  if(a==='solo'){ tracks[ti].s=!tracks[ti].s; return render(); }
  if(a==='addtrack'){ const n=tracks.length; tracks.push(mkT('Audio '+String(n+1).padStart(2,'0'),'mic',n)); selTrack=n; return render(); }
});
hlist.addEventListener('dblclick',e=>{ const n=e.target.closest('.st-name'); if(n){ n.contentEditable='true'; n.focus(); document.getSelection().selectAllChildren(n); return; }
  const v=e.target.closest('.st-vol'); if(v){ tracks[+v.closest('.st-head').dataset.t].vol=.8; render(); }
  const pn=e.target.closest('.st-pan'); if(pn){ tracks[+pn.closest('.st-head').dataset.t].pan=0; render(); } });
hlist.addEventListener('keydown',e=>{ const n=e.target.closest('.st-name'); if(n&&(e.key==='Enter'||e.key==='Escape')){ e.preventDefault(); n.blur(); } });
hlist.addEventListener('focusout',e=>{ const n=e.target.closest('.st-name'); if(!n) return; const t=+n.closest('.st-head').dataset.t; n.contentEditable='false'; tracks[t].name=(n.textContent.trim()||tracks[t].name).slice(0,20); render(); });
hlist.addEventListener('click',e=>{ const h=e.target.closest('.st-head'); if(h&&!e.target.closest('button')&&!e.target.closest('.st-vol')&&!e.target.closest('.st-pan')){ selTrack=+h.dataset.t; hlist.querySelectorAll('.st-head').forEach((x,i)=>x.classList.toggle('sel',i===selTrack)); lanes.querySelectorAll('.st-lane').forEach((x,i)=>x.classList.toggle('sel',i===selTrack)); } });
scroller.addEventListener('scroll',()=>{ hlist.scrollTop=scroller.scrollTop; drawOvw(); });
scroller.addEventListener('wheel',e=>{ if(e.ctrlKey){ e.preventDefault(); ppb=Math.max(7,Math.min(56,ppb*(e.deltaY<0?1.15:1/1.15))); render(); } else if(!e.shiftKey&&Math.abs(e.deltaY)>Math.abs(e.deltaX)){ e.preventDefault(); scroller.scrollLeft+=e.deltaY; } },{passive:false});

/* pointer: clips, ruler, loop, knobs, overview */
let drag=null; const sc=()=>app.getBoundingClientRect().width/1360;
let lastPress=null;
const twice=(k,t)=>{ const now=performance.now(), d=!!lastPress&&lastPress.k===k&&lastPress.t===t&&now-lastPress.at<400; lastPress=d?null:{k,t,at:now}; return d; };
S.addEventListener('pointerdown',e=>{ if(e.button!==0) return;
  if(!e.target.closest('.st-menu')&&!e.target.closest('.st-pop')) hideMenus();
  const vs=e.target.closest('.st-vol'); if(vs){ const ti=+vs.closest('.st-head').dataset.t; e.preventDefault(); if(twice('vol',ti)){ tracks[ti].vol=.8; render(); return; } drag={k:'vol',t:ti,el:vs}; volMove(e); return; }
  const ps=e.target.closest('.st-pan'); if(ps){ const ti=+ps.closest('.st-head').dataset.t; e.preventDefault(); if(twice('pan',ti)){ tracks[ti].pan=0; render(); return; } drag={k:'pan',t:ti,el:ps}; panMove(e); return; }
  if(ovw&&e.target.closest('#st-ovw')){ drag={k:'ovw'}; ovwMove(e); e.preventDefault(); return; }
  const lp=e.target.closest('.st-loop'); if(lp){ const i=[...lp.children].indexOf(e.target); drag={k:i===0?'ls':i===1?'le':'lm',x:e.clientX,s:loopS,e:loopE}; e.preventDefault(); return; }
  if(e.target.closest('#st-ruler')){ seek(e); drag={k:'seek'}; e.preventDefault(); return; }
  const cl=e.target.closest('.st-clip'); if(cl&&!recording){ const c=clips.find(x=>x.id===+cl.dataset.id); selClip=c; selTrack=c.track;
    const mode=e.target.classList.contains('tr')?'trimR':e.target.classList.contains('tl')?'trimL':'move';
    drag={k:mode,c,x:e.clientX,y:e.clientY,s:c.start,l:c.len,o:c.off}; render(); e.preventDefault(); return; }
  if(e.target.closest('.st-lane')&&!recording){ selClip=null; selTrack=+e.target.closest('.st-lane').dataset.t; seek(e); render(); }
});
function seek(e){ const r=canvas.getBoundingClientRect(); pos=Math.max(0,Math.min(TOTAL,(e.clientX-r.left)/sc()/ppb)); rejump(); drawPH(); tools(); }
function ovwMove(e){ const r=ovw.getBoundingClientRect(); const f=Math.max(0,Math.min(1,(e.clientX-r.left)/r.width)); scroller.scrollLeft=f*TOTAL*ppb-scroller.clientWidth/2; drawOvw(); }
function volMove(e){ const tr=drag.el.querySelector('.trk').getBoundingClientRect(); const t=tracks[drag.t]; t.vol=Math.max(0,Math.min(1,(e.clientX-tr.left)/tr.width));
  drag.el.querySelector('.f').style.width=(t.vol*100)+'%'; drag.el.querySelector('.k').style.left=(t.vol*100)+'%'; drag.el.title='Volume '+fmtVol(t.vol);
  const n=drag.el.closest('.st-head').querySelector('.st-name'); n.textContent=fmtVol(t.vol); n.classList.add('val'); sync(); }
function panMove(e){ const tr=drag.el.querySelector('.trk').getBoundingClientRect(); const t=tracks[drag.t]; let p=((e.clientX-tr.left)/tr.width)*2-1; p=Math.max(-1,Math.min(1,p)); if(Math.abs(p)<.05) p=0; t.pan=p;
  drag.el.querySelector('.f').setAttribute('style',panFill(p)); drag.el.querySelector('.k').style.left=((p+1)*50)+'%'; drag.el.title='Pan '+fmtPan(p);
  const v=drag.el.closest('.st-head').querySelector('.st-panv'); if(v){ v.textContent=fmtPan(p); v.classList.toggle('c',Math.abs(p)<.02); } sync(); }
function knobSet(){ const t=tracks[drag.t], k=drag.el, h=k.closest('.st-head');
  const v=drag.which==='vol'?t.vol:(t.pan+1)/2; k.querySelector('.ring').style.background=ring(v,t.c,drag.which==='pan'); k.querySelector('.cap').style.transform='rotate('+(-135+270*v)+'deg)';
  if(drag.which==='vol') h.querySelector('.st-db').textContent=fmtVol(t.vol); else k.nextElementSibling.textContent=fmtPan(t.pan); }
addEventListener('pointermove',e=>{ if(!drag) return;
  if(drag.k==='vol') return volMove(e);
  if(drag.k==='pan') return panMove(e);
  if(drag.k==='kn'){ const d=(drag.y-e.clientY)/sc()/140; const t=tracks[drag.t]; if(drag.which==='vol') t.vol=Math.max(0,Math.min(1,drag.v+d)); else { t.pan=Math.max(-1,Math.min(1,drag.v+d*2)); if(Math.abs(t.pan)<.04) t.pan=0; } return knobSet(); }
  if(drag.k==='ovw') return ovwMove(e);
  if(drag.k==='seek') return seek(e);
  const db=(e.clientX-drag.x)/sc()/ppb;
  if(drag.k==='lm'||drag.k==='ls'||drag.k==='le'){ const d=Math.round(db/BEATS)*BEATS;
    if(drag.k==='lm'){ const len=drag.e-drag.s; loopS=Math.max(0,Math.min(TOTAL-len,drag.s+d)); loopE=loopS+len; }
    if(drag.k==='ls') loopS=Math.max(0,Math.min(loopE-BEATS,drag.s+d)); if(drag.k==='le') loopE=Math.min(TOTAL,Math.max(loopS+BEATS,drag.e+d));
    const lb=$s('#st-loopb'); lb.style.left=(loopS*ppb)+'px'; lb.style.width=((loopE-loopS)*ppb)+'px'; lshade.style.left=(loopS*ppb)+'px'; lshade.style.width=((loopE-loopS)*ppb)+'px'; return; }
  const c=drag.c, el=lanes.querySelector('.st-clip[data-id="'+c.id+'"]');
  if(drag.k==='move'){ c.start=Math.max(0,Math.min(TOTAL-c.len,snapTo(drag.s+db)));
    const lr=lanes.getBoundingClientRect(); const t=Math.max(0,Math.min(tracks.length-1,Math.floor((e.clientY-lr.top)/sc()/LANE)));
    if(t!==c.track){ c.track=t; selTrack=t; render(); return; } if(el) el.style.left=(c.start*ppb)+'px'; }
  if(drag.k==='trimR'){ c.len=Math.max(.25,Math.min(TOTAL-c.start,snapTo(drag.s+drag.l+db)-c.start)); c.len=Math.min(c.len,c.peaks.length/8-c.off); if(el){ el.style.width=(c.len*ppb)+'px'; el.querySelector('svg').outerHTML=waveSVG(c); } }
  if(drag.k==='trimL'){ let ns=snapTo(drag.s+db); ns=Math.max(drag.s-drag.o,Math.min(drag.s+drag.l-.25,ns)); c.off=drag.o+(ns-drag.s); c.len=drag.l-(ns-drag.s); c.start=ns; if(el){ el.style.left=(c.start*ppb)+'px'; el.style.width=(c.len*ppb)+'px'; el.querySelector('svg').outerHTML=waveSVG(c); } }
});
addEventListener('pointerup',()=>{ if(!drag) return; const k=drag.k; drag=null; if(k!=='seek'&&k!=='ovw') render(); });
lanes.addEventListener('dblclick',e=>{ const cl=e.target.closest('.st-clip'); if(!cl) return; selClip=clips.find(x=>x.id===+cl.dataset.id); renameClip(); });
function renameClip(){ const c=selClip; if(!c) return; const cl=lanes.querySelector('.st-clip[data-id="'+c.id+'"]'); if(!cl) return; const sp=cl.querySelector('span');
  sp.style.pointerEvents='auto'; sp.contentEditable='true'; sp.style.outline='none'; sp.style.background='rgba(0,0,0,.6)'; sp.style.borderRadius='4px'; sp.focus(); document.getSelection().selectAllChildren(sp);
  const done=()=>{ c.name=(sp.textContent.trim()||c.name).toUpperCase().slice(0,24); render(); };
  sp.addEventListener('blur',done,{once:true}); sp.addEventListener('keydown',ev=>{ if(ev.key==='Enter'||ev.key==='Escape'){ ev.preventDefault(); sp.blur(); } ev.stopPropagation(); }); }

/* keyboard */
const open=()=>ov.style.display==='block';
function closeStudio(){ stop(false); closeOv(); }
document.addEventListener('keydown',e=>{ if(!open()){ if(e.key==='Escape'&&$$('.ov').some(o=>o.style.display==='block')) closeOv(); return; }
  if(e.target.isContentEditable||/INPUT|TEXTAREA/.test(e.target.tagName)&&e.target.type!=='range') return;
  const k=e.key.toLowerCase();
  if(k===' '){ e.preventDefault(); playing&&!recording&&!counting?pause():(recording?stop(false):play()); }
  else if(k==='r'){ e.preventDefault(); record(); }
  else if(k==='l'){ loopOn=!loopOn; render(); }
  else if(k==='s'&&!e.ctrlKey&&!e.metaKey){ split(); }
  else if(k==='d'&&(e.ctrlKey||e.metaKey)){ e.preventDefault(); dup(); }
  else if(k==='delete'||k==='backspace'){ e.preventDefault(); del(); }
  else if(k==='home'||k==='enter'){ pos=loopOn?loopS:0; rejump(); drawPH(); }
  else if(k==='escape'){ if(menu.style.display!=='none'&&menu.style.display||pop.style.display==='block') hideMenus(); else closeStudio(); }
});
ov.firstElementChild.addEventListener('click',e=>{ if(e.target===ov.firstElementChild) stop(false); });
/* right-click a take */
const menu=document.createElement('div'); menu.className='st-menu'; S.appendChild(menu);
const pop=document.createElement('div'); pop.className='st-pop'; S.appendChild(pop);
function place(el,x,y){ const r=S.getBoundingClientRect(), k=sc(); el.style.display=el===menu?'flex':'block';
  let L=(x-r.left)/k, T=(y-r.top)/k; const w=el.offsetWidth, h=el.offsetHeight; L=Math.min(L,S.offsetWidth-w-8); T=Math.min(T,S.offsetHeight-h-8); el.style.left=L+'px'; el.style.top=T+'px'; }
function hideMenus(){ menu.style.display='none'; pop.style.display='none'; }
lanes.addEventListener('contextmenu',e=>{ const cl=e.target.closest('.st-clip'); if(!cl||recording) return; e.preventDefault(); selClip=clips.find(x=>x.id===+cl.dataset.id); selTrack=selClip.track; render();
  const canSplit=pos>selClip.start+.05&&pos<selClip.start+selClip.len-.05;
  menu.innerHTML='<button data-a="menu" data-m="rename">Rename<kbd>Double-click</kbd></button><button data-a="menu" data-m="split"'+(canSplit?'':' disabled')+'>Split at playhead<kbd>S</kbd></button><button data-a="menu" data-m="dup">Duplicate<kbd>Ctrl+D</kbd></button><hr><button data-a="menu" data-m="del" class="del">Delete<kbd>Del</kbd></button>';
  place(menu,e.clientX,e.clientY); });
hlist.addEventListener('contextmenu',e=>{ const h=e.target.closest('.st-head'); if(!h) return; e.preventDefault(); const ti=+h.dataset.t, t=tracks[ti]; selTrack=ti;
  menu.innerHTML='<button data-a="hmenu" data-m="rename" data-t="'+ti+'">Rename track<kbd>Double-click</kbd></button><button data-a="hmenu" data-m="color" data-t="'+ti+'">Next colour</button><button data-a="hmenu" data-m="stereo" data-t="'+ti+'">Make '+(t.st?'mono':'stereo')+'</button><hr><button data-a="deltrack" data-t="'+ti+'" class="del">Delete track</button>';
  place(menu,e.clientX,e.clientY); });
S.addEventListener('contextmenu',e=>{ if(!e.target.closest('.st-clip')&&!e.target.closest('.st-head')) e.preventDefault(); });
function openChan(ti,btn){ if(pop.style.display==='block'&&pop.dataset.t==ti){ hideMenus(); return; } hideMenus(); const t=tracks[ti]; pop.dataset.t=ti; pop.style.setProperty('--tc',t.c); btn.classList.add('on');
  pop.innerHTML='<h4><i></i>'+t.name+'</h4><div class="row">COLOUR</div><div class="sws">'+TCOL.map(c=>'<button data-c="'+c+'" style="background:'+c+'" class="'+(c===t.c?'on':'')+'" aria-label="Colour"></button>').join('')+'</div>'
   +'<div class="row">VOLUME<input type="range" data-p="vol" min="0" max="1" step=".005" value="'+t.vol+'"><span class="val">'+fmtVol(t.vol).replace(' dB','')+'</span></div>'
   +'<div class="row">PAN<input type="range" data-p="pan" min="-1" max="1" step=".01" value="'+t.pan+'"><span class="val">'+fmtPan(t.pan)+'</span></div>'
   +'<button class="danger" data-a="deltrack" data-t="'+ti+'">DELETE TRACK</button>';
  const r=btn.getBoundingClientRect(); place(pop,r.right+6,r.top-20); }
pop.addEventListener('input',e=>{ const t=tracks[+pop.dataset.t]; const p=e.target.dataset.p; if(!p) return; t[p]=+e.target.value; if(p==='pan'&&Math.abs(t.pan)<.04) t.pan=0;
  e.target.nextElementSibling.textContent=p==='vol'?fmtVol(t.vol).replace(' dB',''):fmtPan(t.pan);
  sync(); const h=hlist.children[+pop.dataset.t]; if(h&&p==='vol'){ h.querySelector('.f').style.width=(t.vol*100)+'%'; h.querySelector('.k').style.left=(t.vol*100)+'%'; } });
pop.addEventListener('click',e=>{ const c=e.target.closest('[data-c]'); if(!c) return; const ti=+pop.dataset.t; tracks[ti].c=c.dataset.c; pop.style.setProperty('--tc',c.dataset.c); pop.querySelectorAll('[data-c]').forEach(x=>x.classList.toggle('on',x===c)); render(); hlist.children[ti].querySelector('.st-b.e').classList.add('on'); });
window.studioRender=render;
})();
</script>
