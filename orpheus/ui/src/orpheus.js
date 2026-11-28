<script>
/* =============================================================================
   ORPHEUS: SIREN's chord roll (preview). Four voices, each a line of dots
   (time in beats, note). Between two dots a voice slides the whole way, like
   automation: dots close together make a quick slide. A chord name sits at
   every change. It plays through SIREN's sound.
   SIREN's KEYS button opens it. Like AETHER it's built on SIREN's device
   (window.__sirenKit).
   ============================================================================= */
(function () {
'use strict';
const K = window.__sirenKit; if (!K) return;
const { el, dev } = K;

/* ------------------------------------------------------------ constants */
const LC = '#ff4d5a';                                   // the voices, their dots and the lit keys
const NAMES = ['C', 'C♯', 'D', 'E♭', 'E', 'F', 'F♯', 'G', 'A♭', 'A', 'B♭', 'B'];
const BLK = [0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0];
const LO = 36, HI = 96, PS = 9, PADY = 4, NH = (HI - LO + 1) * PS + PADY * 2, PW = 84, BW = 52, VOICES = 4;
const GRIDS = [['1/2', 2], ['1/4', 1], ['1/8', .5], ['1/16', .25], ['1/32', .125], ['1/64', .0625], ['1/8T', 1 / 3], ['1/16T', 1 / 6], ['FREE', 0]];
const SIGS = ['4/4', '3/4', '2/4', '5/4', '7/4', '6/8', '7/8', '9/8', '12/8'];
const DEF = { bars: 10, sig: '4/4', grid: '1/16', ppb: 70, bpm: 80, loop: true, showNotes: false };
const f = v => (Math.round(v * 10) / 10).toString();
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const ease = u => u * u * (3 - 2 * u);
/* spelling: flats in flat keys, sharps in sharp keys (from the song's key); a slash bass is spelled from its chord's root, so A/C♯
   stays A/C♯ even in F minor */
const LET = ['C', 'D', 'E', 'F', 'G', 'A', 'B'], NAT = [0, 2, 4, 5, 7, 9, 11];
const FLAT = ['C', 'D♭', 'D', 'E♭', 'E', 'F', 'G♭', 'G', 'A♭', 'A', 'B♭', 'B'], SHARP = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];
let spellKey = null;
const useFlats = () => { if (!spellKey) return null; const maj = spellKey.minor ? (spellKey.r + 3) % 12 : spellKey.r; return [5, 10, 3, 8, 1, 6].includes(maj) ? true : [7, 2, 9, 4, 11].includes(maj) ? false : null; };
const pcName = pc => { const f = useFlats(); return (f === true ? FLAT : f === false ? SHARP : NAMES)[pc]; };
const spellFrom = (r, pc) => { const L = LET.indexOf(pcName(r)[0]), li = (L + [0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6][(pc - r + 12) % 12]) % 7, acc = ((pc - NAT[li]) % 12 + 18) % 12 - 6;
  return Math.abs(acc) > 1 ? pcName(pc) : LET[li] + ['♭', '', '♯'][acc + 1]; };
const nn = m => pcName(((m % 12) + 12) % 12) + (Math.floor(m / 12) - 1);
const y = m => PADY + (HI - m + .5) * PS;
const mOf = yy => HI + .5 - (yy - PADY) / PS;
const circ = (x, yy, r) => 'M' + f(x + r) + ' ' + f(yy) + 'A' + r + ' ' + r + ' 0 1 1 ' + f(x - r) + ' ' + f(yy) + 'A' + r + ' ' + r + ' 0 1 1 ' + f(x + r) + ' ' + f(yy) + 'Z';
const rect = (x0, y0, w, hh) => 'M' + f(x0) + ' ' + f(y0) + 'h' + f(w) + 'v' + f(hh) + 'h' + f(-w) + 'Z';

/* ------------------------------------------------------------ chord names */
const T = [
  ['', [0, 4, 7]], ['m', [0, 3, 7]], ['sus4', [0, 5, 7]], ['sus2', [0, 2, 7]], ['dim', [0, 3, 6]], ['aug', [0, 4, 8]],
  ['maj7', [0, 4, 7, 11]], ['7', [0, 4, 7, 10]], ['m7', [0, 3, 7, 10]], ['m7♭5', [0, 3, 6, 10]], ['6', [0, 4, 7, 9]], ['m6', [0, 3, 7, 9]], ['m(maj7)', [0, 3, 7, 11]],
  ['add9', [0, 4, 7, 2]], ['m(add9)', [0, 3, 7, 2]], ['maj9', [0, 4, 7, 11, 2]], ['9', [0, 4, 7, 10, 2]], ['m9', [0, 3, 7, 10, 2]],
  ['7sus4', [0, 5, 7, 10]], ['9sus4', [0, 5, 7, 10, 2]], ['m11', [0, 3, 7, 10, 5]], ['maj7♯11', [0, 4, 7, 11, 6]], ['13', [0, 4, 7, 10, 9]], ['6/9', [0, 4, 7, 9, 2]], ['5', [0, 7]]
];
const nameMemo = new Map();                                        // a chord's name, once worked out (long songs repeat chords)
function chordName(ms) {
  const key = (spellKey ? spellKey.r * 2 + spellKey.minor : -1) + ':' + ms.join(',');
  let n = nameMemo.get(key); if (n === undefined) { n = chordName0(ms); if (nameMemo.size > 5000) nameMemo.clear(); nameMemo.set(key, n); }
  return n;
}
function chordName0(ms) {
  if (!ms.length) return '';
  const bass = ((Math.min(...ms) % 12) + 12) % 12;
  const pcs = [...new Set(ms.map(m => ((m % 12) + 12) % 12))];
  if (pcs.length === 1) return pcName(pcs[0]);
  let best = null;
  pcs.forEach(r => {
    const I = new Set(pcs.map(q => (q - r + 12) % 12));
    T.forEach(([suf, iv]) => {
      const Ts = new Set(iv);
      let hit = 0, extra = 0, miss = 0;
      I.forEach(x => { if (Ts.has(x)) hit++; else extra++; });
      Ts.forEach(x => { if (!I.has(x)) miss += x === 7 ? .15 : 1; });
      const sc = hit - extra * 2 - miss + (r === bass ? .6 : 0) + iv.length * .01;
      if (!best || sc > best.sc) best = { sc, r, suf };
    });
  });
  return pcName(best.r) + best.suf + (best.r !== bass ? '/' + spellFrom(best.r, bass) : '');
}

/* ------------------------------------------------------------ the song */
function voiceChord(r, iv, prev) {
  const set = new Set(iv.map(i => (r + i) % 12));
  let bass = null;
  for (let m = 36; m <= 50; m++) if (m % 12 === r && (bass === null || Math.abs(m - prev[0]) < Math.abs(bass - prev[0]))) bass = m;
  const n = [bass, 0, 0, 0], used = new Set([r]);
  let above = 200;
  for (let v = 3; v >= 1; v--) {
    let best = null, bc = 1e9;
    for (let m = 50; m <= 82; m++) {
      if (!set.has(m % 12) || m >= above || m <= bass + 2) continue;
      const cost = Math.abs(m - prev[v]) + (used.has(m % 12) ? 4 : 0);
      if (cost < bc) { bc = cost; best = m; }
    }
    if (best === null) best = Math.max(bass + 3, above - 1);
    n[v] = best; above = best; used.add(best % 12);
  }
  return n;
}
/* the demo song: eight chords, each voice holding until half a beat before the change, then sliding */
function startVoices() {
  const C = [[2, [0, 4, 7, 11, 2]], [7, [0, 4, 7, 11, 2]], [11, [0, 3, 7, 10, 2]], [9, [0, 5, 7, 10, 2]],
    [4, [0, 3, 7, 10, 2]], [7, [0, 4, 7, 11]], [6, [0, 3, 7, 10]], [9, [0, 4, 7, 10]]];
  let prev = [43, 55, 60, 64];
  const chords = C.map(([r, iv]) => { const n = voiceChord(r, iv, prev); prev = n; return n; });
  const V = [[], [], [], []];
  for (let v = 0; v < 4; v++) chords.forEach((n, i) => {
    const m = n[v];
    if (i === 0) { V[v].push({ t: 0, m }); return; }
    const last = V[v][V[v].length - 1].m;
    if (last === m) return;
    V[v].push({ t: i * 4 - .5, m: last });
    V[v].push({ t: i * 4, m });
  });
  return V;
}

const st = Object.assign({ V: startVoices(), play: false, ph: 0, drag: null, sel: null }, DEF);
const sigOf = sig => { const [n, d] = sig.split('/').map(Number); return { n, d, beat: 4 / d, bar: n * 4 / d }; };
const BL = () => sigOf(st.sig).bar;
const LEN = () => st.bars * BL();
const ROOM = () => Math.max(16, Math.ceil(st.bars * .5));       // bars to draw in after the song's end (drawing there makes the song longer)
const CAN = () => (st.bars + ROOM()) * BL();                     // the whole canvas, in beats
const MAXBARS = 999;
const W = () => Math.round(CAN() * st.ppb) + 40;
const X = t => 12 + t * st.ppb;
const gstep = () => (GRIDS.find(g => g[0] === st.grid) || GRIDS[3])[1];
const minStep = () => gstep() > 0 ? gstep() : 1 / 48;
const snapT = t => { const g = gstep(); return g > 0 ? Math.round(t / g) * g : Math.round(t * 48) / 48; };
const snapDown = t => { const g = gstep(); return g > 0 ? Math.floor(t / g + 1e-9) * g : Math.floor(t * 48 + 1e-9) / 48; };
const snapM = raw => clamp(Math.round(raw), LO, HI);
const lastT = V => { let mx = 0; V.forEach(a => a.forEach(q => { if (q.t > mx) mx = q.t; })); return mx; };
const fitBars = bars => Math.min(MAXBARS, Math.max(bars, Math.ceil((lastT(st.V) + 1e-6) / BL()) + 1));
function pitchAt(v, t) {
  const a = st.V[v];
  if (!a.length || t < a[0].t - 1e-9) return null;
  let lo = 0, hi = a.length - 1;                                    // the last dot at or before t (binary search: long songs stay quick)
  while (lo < hi) { const mid = (lo + hi + 1) >> 1; if (a[mid].t <= t + 1e-9) lo = mid; else hi = mid - 1; }
  const A = a[lo], B = a[lo + 1];
  if (!B || B.m === A.m) return A.m;
  return A.m + (B.m - A.m) * ease((t - A.t) / (B.t - A.t));
}
const sounding = t => { const out = []; for (let v = 0; v < VOICES; v++) { const m = pitchAt(v, t); if (m != null) out.push(m); } return out; };

/* a chord name wherever a note arrives somewhere new (the same name twice in a row shows once) */
let marks = [];
function calcMarks() {
  const evs = new Set();
  st.V.forEach(a => a.forEach((q, i) => { if (i === 0 || a[i - 1].m !== q.m) evs.add(Math.round(q.t * 4800) / 4800); }));
  marks = [];
  [...evs].sort((a, b) => a - b).forEach(t => { const nm = chordName(sounding(t).map(Math.round)); if (nm && (!marks.length || marks[marks.length - 1].n !== nm)) marks.push({ t, n: nm }); });
}
const markAt = t => { let k = -1; marks.forEach((mk, j) => { if (mk.t <= t + 1e-6) k = j; }); return k; };

/* ------------------------------------------------------------ sound: SIREN plays the voices */
let snd = 0, blipT = null;
const canSound = () => typeof K.noteOn === 'function' && (K.native || !(K.inPlugin && K.inPlugin())) && (!K.sirenOn || K.sirenOn());
function soundAt(notes, glide) {
  if (!notes.length || !canSound()) { soundOff(); return; }
  if (snd !== notes.length) { K.noteOn(notes, .8, undefined, false, true); snd = notes.length; return; }       // held: the notes sustain
  if (K.native) { K.setPitches(notes, 0, glide); return; }      // the plugin: its engine plays (orpheus-bridge.js)
  const ac = K.ctx && K.ctx(); if (ac) K.setPitches(notes, ac.currentTime, glide);
}
function soundOff() { if (snd) { try { K.noteOff(); } catch (_) {} snd = 0; } }
function blip(t) { clearTimeout(blipT); soundOff(); soundAt(sounding(t), 0); blipT = setTimeout(soundOff, 650); }

/* ------------------------------------------------------------ look */
const css = el('style');
css.textContent = `
.or-head { position: absolute; top: 14px; height: 28px; display: flex; align-items: center; gap: 10px; z-index: 5; }
.or-head.l { left: 44px; } .or-head.r { right: 44px; }
.or-tp { display: flex; align-items: center; gap: 4px; }
.or-k { width: 30px; height: 24px; border: none; padding: 0; border-radius: 6px; color: #b9b6ae; cursor: pointer; display: flex; align-items: center; justify-content: center;
  background: linear-gradient(#3a3a3f, #1f1f23 55%, #18181b); box-shadow: 0 3px 5px rgba(0,0,0,.75), inset 0 1px 0 rgba(255,255,255,.22), inset 0 -2px 3px rgba(0,0,0,.4), 0 0 0 1px #0a0a0b; }
.or-k:hover { color: #f1eee7; }
.or-k.play { width: 38px; color: #e9e6df; }
.or-k.on { color: var(--accent); background: linear-gradient(#2e2e33, #1a1a1d 60%, #141416);
  box-shadow: 0 1px 2px rgba(0,0,0,.8), inset 0 1px 0 rgba(255,255,255,.14), inset 0 2px 4px rgba(0,0,0,.5), 0 0 0 1px var(--accent); }
.or-k svg { display: block; }
.or-lcd { height: 26px; display: flex; align-items: center; gap: 10px; padding: 0 12px; border-radius: 7px; font-family: 'Orbitron', 'Saira', sans-serif; color: var(--accent);
  background: linear-gradient(#060607, #0b0b0d); box-shadow: inset 0 2px 6px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.04), 0 1px 0 rgba(255,255,255,.08); }
.or-lcd .pos { font-size: 13px; font-weight: 500; letter-spacing: .08em; min-width: 96px; font-variant-numeric: tabular-nums; }
.or-lcd .sep { width: 1px; height: 14px; background: var(--accent); opacity: .18; }
.or-lcd .v { font-size: 11px; letter-spacing: .08em; white-space: nowrap; text-align: center; }
.or-lcd .v small { font-size: 8px; opacity: .6; }
.or-lcd .v.sig { min-width: 30px; }
.or-keys .cap b { position: absolute; left: 10px; bottom: 18px; font-family: 'Orbitron', 'Saira', sans-serif; font-size: 11px; font-weight: 500; letter-spacing: .06em; color: var(--accent); white-space: nowrap; }
.or-lcd button { width: 16px; height: 18px; border: none; background: none; padding: 0; color: #8d8d93; font: inherit; font-size: 12px; line-height: 1; cursor: pointer; }
.or-lcd button.ar { width: 14px; font-size: 10px; }
.or-lcd button:hover { color: #f1eee7; }
.or-lbl { font-size: 8.5px; font-weight: 600; letter-spacing: .2em; color: #6f6f76; }
.or-head.r { gap: 7px; } .or-len { gap: 3px; padding: 0 5px; } .or-len .len { min-width: 24px; text-align: center; font-size: 12px; font-variant-numeric: tabular-nums; }
.or-bn.out { opacity: .35; }
.or-end { position: absolute; top: 3px; font-size: 8px; font-weight: 600; letter-spacing: .16em; color: var(--accent); opacity: .8; pointer-events: auto; cursor: default; }
.or-seg { display: flex; gap: 2px; padding: 3px; border-radius: 8px; background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05); }
.or-seg button { height: 20px; min-width: 34px; padding: 0 5px; border: none; border-radius: 6px; background: none; color: #6f6f76; font: inherit; font-size: 8.5px; font-weight: 600; letter-spacing: .04em; white-space: nowrap; cursor: pointer; }
.or-seg button:hover { color: #e9e6df; }
.or-seg button.on { color: #e9e6df; background: linear-gradient(#34343a, #25252a); box-shadow: inset 0 1px 0 rgba(255,255,255,.1), 0 1px 2px rgba(0,0,0,.5), inset 0 -2px 0 var(--accent); }
.or-notes { height: 22px; padding: 0 9px; border-radius: 6px; border: 1px solid rgba(255,255,255,.1); background: none; color: #8d8d93; font: inherit; font-size: 8.5px; font-weight: 600; letter-spacing: .14em; cursor: pointer; }
.or-notes:hover { color: #e9e6df; }
.or-notes.on { border-color: transparent; background: var(--accent); color: #111113; }
.sy-screen.or-screen { top: 50px; height: 424px; }
.or-sec { flex: 1; min-width: 0; display: flex; padding: 8px; box-sizing: border-box; border-radius: 12px;
  background: linear-gradient(180deg, rgba(255,255,255,.045), rgba(255,255,255,.012) 40%, rgba(0,0,0,.12)), #0d0d10;
  box-shadow: inset 0 1px 0 rgba(255,255,255,.12), inset 0 0 0 1px rgba(255,255,255,.08), inset 0 -1px 0 rgba(0,0,0,.6), 0 3px 8px rgba(0,0,0,.65), 0 0 0 1px rgba(0,0,0,.7); }
.or-box { position: relative; flex: 1; min-width: 0; display: flex; border-radius: 9px; overflow: hidden; background: #050506;
  box-shadow: inset 0 0 0 1px rgba(255,255,255,.05), inset 0 4px 12px rgba(0,0,0,.7); }
.or-keys { position: relative; width: ${PW}px; flex: none; display: flex; flex-direction: column; z-index: 2; background: #0e0e10;
  box-shadow: 1px 0 0 rgba(255,255,255,.08), 6px 0 12px rgba(0,0,0,.6); }
.or-keys .cap { position: relative; height: 46px; flex: none; box-shadow: inset 0 -1px 0 rgba(255,255,255,.06); }
.or-keys .cap span { position: absolute; left: 10px; bottom: 7px; font-size: 8px; font-weight: 600; letter-spacing: .2em; color: #6f6f76; }
.or-kscroll { position: relative; flex: 1; min-height: 0; overflow: hidden; }
.or-kin { position: absolute; left: 0; top: 0; width: ${PW}px; height: ${NH}px; cursor: pointer; }
.or-kin svg { position: absolute; left: 0; top: 0; }
.or-clab { position: absolute; left: 58px; font-family: 'Orbitron', 'Saira', sans-serif; font-size: 7px; letter-spacing: .04em; pointer-events: none; }
.or-main { position: relative; flex: 1; min-width: 0; display: flex; flex-direction: column; }
.or-ruler { position: relative; height: 46px; flex: none; overflow: hidden; cursor: pointer; background: rgba(255,255,255,.018); box-shadow: inset 0 -1px 0 rgba(255,255,255,.06); }
.or-rin { position: absolute; left: 0; top: 0; height: 46px; }
.or-rin svg { position: absolute; left: 0; top: 0; }
.or-chip { position: absolute; top: 5px; font-size: 13.5px; font-weight: 700; letter-spacing: .04em; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
  color: #bdbab3; text-shadow: 0 1px 0 rgba(0,0,0,.9); pointer-events: none; }
.or-chip.on { color: #ffffff; }
.or-bn { position: absolute; top: 31px; font-family: 'Orbitron', 'Saira', sans-serif; font-size: 8px; letter-spacing: .08em; color: #6f6f76; pointer-events: none; }
.or-scroll { position: relative; flex: 1; min-height: 0; overflow: auto; scrollbar-width: thin; scrollbar-color: #2e2e33 #08080a; }
.or-gin { position: relative; cursor: crosshair; touch-action: none; }
.or-gin svg { position: absolute; left: 0; top: 0; }
.or-dl { position: absolute; font-family: 'Orbitron', 'Saira', sans-serif; font-size: 9px; letter-spacing: .06em; color: ${LC}; pointer-events: none; text-shadow: 0 1px 0 rgba(0,0,0,.9); }
.sy-led.or-mini { width: 214px; padding: 0; display: block; }
.sy-led.or-mini canvas { position: absolute; left: 10px; top: 8px; width: calc(100% - 20px); height: calc(100% - 16px); }
.or-bot .sy-wave.l { right: calc(50% + 150px); } .or-bot .sy-wave.r { left: calc(50% + 150px); }
.or-badge .pn { text-transform: none; letter-spacing: .12em; margin-right: -.12em; }
.or-badge .pn i { background: ${LC}; box-shadow: none; }
.or-confirm { position: absolute; inset: 0; z-index: 30; border-radius: 22px; display: none; align-items: center; justify-content: center; background: rgba(6,6,8,.66); }
.or-confirm.show { display: flex; }
.or-dlg { width: 380px; box-sizing: border-box; padding: 24px 26px 22px; border-radius: 14px; text-align: center;
  background: linear-gradient(172deg, #232327, #141416 60%, #0e0e10); box-shadow: inset 0 1px 0 rgba(255,255,255,.13), inset 0 0 0 1px rgba(255,255,255,.07), 0 22px 50px rgba(0,0,0,.75); }
.or-dlg b { display: block; font-size: 12px; font-weight: 600; letter-spacing: .26em; margin-right: -.26em; color: #f1eee7; }
.or-dlg p { margin: 12px 0 20px; font-size: 12px; line-height: 1.55; color: #a9a6a0; }
.or-dlg .bt { display: flex; gap: 10px; justify-content: center; }
.or-dlg button { height: 32px; min-width: 128px; padding: 0 16px; border-radius: 8px; font: inherit; font-size: 10px; font-weight: 600; letter-spacing: .18em; cursor: pointer; }
.or-dlg .no { border: 1px solid rgba(255,255,255,.16); background: none; color: #e9e6df; }
.or-dlg .no:hover { border-color: rgba(255,255,255,.3); }
.or-dlg .yes { border: none; background: ${LC}; color: #111113; }
.or-dlg .yes:hover { filter: brightness(1.1); }
`;
document.head.appendChild(css);

/* ------------------------------------------------------------ build: header (on the faceplate, either side of SIREN | AETHER) */
const IC_START = '<svg width="12" height="12" viewBox="0 0 16 16" aria-hidden="true"><rect x="2.5" y="3" width="2" height="10" rx=".5" fill="currentColor"/><path d="M13.5 3v10L5.5 8z" fill="currentColor"/></svg>';
const IC_PLAY = '<svg width="12" height="12" viewBox="0 0 16 16" aria-hidden="true"><path d="M4 2.5l10 5.5-10 5.5z" fill="currentColor"/></svg>';
const IC_PAUSE = '<svg width="12" height="12" viewBox="0 0 16 16" aria-hidden="true"><rect x="3.5" y="2.5" width="3" height="11" rx="1" fill="currentColor"/><rect x="9.5" y="2.5" width="3" height="11" rx="1" fill="currentColor"/></svg>';
const IC_STOP = '<svg width="11" height="11" viewBox="0 0 16 16" aria-hidden="true"><rect x="3" y="3" width="10" height="10" rx="1.5" fill="currentColor"/></svg>';
const IC_LOOP = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M17 2l3 3-3 3"/><path d="M4 11V9a4 4 0 0 1 4-4h12"/><path d="M7 22l-3-3 3-3"/><path d="M20 13v2a4 4 0 0 1-4 4H4"/></svg>';
const IC_KEYS = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"><rect x="2.5" y="5" width="19" height="14" rx="2"/><path d="M7.2 5v8.5M12 5v8.5M16.8 5v8.5" stroke-width="1.6"/><path d="M7.2 19v-5.5M12 19v-5.5M16.8 19v-5.5" stroke-width="1.2" opacity=".6"/></svg>';

const hl = el('div', 'or-head l sy-orpheus',
  '<div class="or-tp">' +
  '<button class="or-k" data-a="start" aria-label="Back to start" title="Back to the start">' + IC_START + '</button>' +
  '<button class="or-k play" data-a="play" aria-label="Play" aria-pressed="false" title="Play / pause (SPACE)">' + IC_PLAY + '</button>' +
  '<button class="or-k" data-a="stop" aria-label="Stop" title="Stop and go back to the start">' + IC_STOP + '</button>' +
  '<button class="or-k" data-a="loop" aria-label="Loop" aria-pressed="true" title="Loop the song">' + IC_LOOP + '</button></div>' +
  '<div class="or-lcd"><span class="pos">001.1.1</span><i class="sep"></i>' +
  '<button data-a="bpm-" aria-label="Slower" title="Slower (shift: 5 at a time)">−</button><span class="v bpm"></span><button data-a="bpm+" aria-label="Faster" title="Faster (shift: 5 at a time)">+</button><i class="sep"></i>' +
  '<button class="ar" data-a="sig-" aria-label="Previous time signature" title="Previous time signature">◂</button><span class="v sig" title="Time signature: beats in a bar"></span><button class="ar" data-a="sig+" aria-label="Next time signature" title="Next time signature">▸</button></div>');
const hr = el('div', 'or-head r sy-orpheus',
  '<div class="or-seg" role="group" aria-label="Grid" title="GRID: what the dots snap to">' +
  GRIDS.map(([l]) => '<button data-g="' + l + '" aria-pressed="false" title="' + (l === 'FREE' ? 'No snapping: put dots anywhere' : 'Dots snap to ' + l.replace('T', ' triplet') + ' notes') + '">' + l + '</button>').join('') + '</div>' +
  '<span class="or-lbl">BARS</span><div class="or-lcd or-len"><button data-a="len-" aria-label="Shorter song" title="Shorter (shift: 8 at a time; not shorter than your notes)">−</button><span class="v len" title="How long the song is, in bars: where it loops or stops"></span><button data-a="len+" aria-label="Longer song" title="Longer (shift: 8 at a time; up to 999)">+</button></div>' +
  '<button class="or-notes" data-a="notes" aria-pressed="false" title="Show the note names on the dots">NOTES</button>');
dev.append(hl, hr);
const $h = s => hl.querySelector(s);
const bPlay = $h('[data-a="play"]'), bLoop = $h('[data-a="loop"]'), lPos = $h('.pos'), lBpm = $h('.bpm'), lSig = $h('.sig');
const bNotes = hr.querySelector('.or-notes'), gBtns = [...hr.querySelectorAll('[data-g]')], lLen = hr.querySelector('.len');

/* ------------------------------------------------------------ build: the roll */
const scr = el('div', 'sy-screen or-screen sy-orpheus');
const sec = el('section', 'or-sec'); sec.setAttribute('aria-label', 'ORPHEUS song');
const box = el('div', 'or-box');
const keys = el('div', 'or-keys', '<div class="cap" title="The key of your song, worked out from its chords"><b class="or-key">—</b><span>KEY</span></div>');
const lKey = keys.querySelector('.or-key');
const ks = el('div', 'or-kscroll'), kin = el('div', 'or-kin');
kin.title = 'Click a key to hear it (red keys: what sounds under the playhead)';
ks.append(kin); keys.append(ks);
const main = el('div', 'or-main');
const ruler = el('div', 'or-ruler'), rin = el('div', 'or-rin');
ruler.title = 'Click or drag to move the playhead';
ruler.append(rin);
const scroll = el('div', 'or-scroll'), gin = el('div', 'or-gin');
gin.setAttribute('role', 'application');
gin.title = 'Drag a dot to move it (SHIFT: only up and down) · drag empty space to move around · double-click a line to add a dot, a dot to remove it · CTRL+Z undo · wheel: zoom';
gin.setAttribute('aria-label', 'The song: note lines over a time grid. Double-click to add a dot, double-click a dot to remove it, drag dots to move them.');
scroll.append(gin); main.append(ruler, scroll);
box.append(keys, main); sec.append(box); scr.append(sec); dev.append(scr);

/* ------------------------------------------------------------ build: bottom band and the "are you sure?" */
const waveSvg = '<svg viewBox="0 0 380 24" preserveAspectRatio="none"><path d="' + Array.from({ length: 77 }, (_, i) => (i ? 'L' : 'M') + (i * 5) + ' ' + (12 + Math.sin(i * .42) * 9 * Math.pow(i / 76, 1.6)).toFixed(1)).join(' ') + '" fill="none" stroke="currentColor" stroke-width="2"/></svg>';
const bot = el('div', 'sy-bot or-bot sy-orpheus');
const back = el('button', 'sy-btn', '<span class="round">' + IC_KEYS + '</span><span class="tx"><b>KEYS</b><small>back to SIREN</small></span>');
back.title = 'Close ORPHEUS and go back to SIREN';
const badge = el('div', 'sy-badge or-badge', '<b>ORPHEUS</b><div class="pn" title="The chord under the playhead"><i></i><u>—</u></div>');
const bChord = badge.querySelector('u');
const rst = el('button', 'sy-btn r', '<span class="round">' + K.IC.reset + '</span><span class="tx"><b>RESET</b><small>clear everything</small></span>');
rst.title = 'Remove every note and set tempo, time signature, grid and zoom back to default (asks first)';
bot.append(back, el('div', 'sy-wave l', waveSvg), badge, el('div', 'sy-wave r', waveSvg), rst);
dev.append(bot);
const cf = el('div', 'or-confirm',
  '<div class="or-dlg" role="alertdialog" aria-modal="true" aria-labelledby="or-cf-t" aria-describedby="or-cf-d"><b id="or-cf-t">CLEAR EVERYTHING?</b>' +
  '<p id="or-cf-d">Every note is removed, and tempo, time signature, grid and zoom go back to default. This can\'t be undone.</p>' +
  '<div class="bt"><button class="no">CANCEL</button><button class="yes">CLEAR ALL</button></div></div>');
dev.append(cf);

/* SIREN's KEYS button opens ORPHEUS (synth.js asks kit.hasOrpheus) */
const kbBtn = dev.querySelector('.sy-kbbtn');
if (kbBtn) { const s = kbBtn.querySelector('small'); if (s) s.textContent = 'open ORPHEUS'; kbBtn.title = 'ORPHEUS: draw chords that glide into each other, played by SIREN'; }

/* ------------------------------------------------------------ drawing */
let phEl = null, headEl = null, rlEl = null, chipEls = [], curMark = -2, pianoKey = '';
const headPath = x => 'M' + f(x - 6) + ' 22L' + f(x + 6) + ' 22L' + f(x) + ' 29Z';

/* only what's on screen (and a screen either side) is drawn, so a 999-bar song is as quick as a short one; scrolling redraws
   when the view nears the drawn part's edge */
let vis = { a: 0, b: 0 };
function visRange() { const cw = scroll.clientWidth || 1200, pad = cw / st.ppb; return { a: Math.max(0, (scroll.scrollLeft - 12) / st.ppb - pad), b: Math.min(CAN(), (scroll.scrollLeft + cw) / st.ppb + pad) }; }
function renderGrid() {
  const w = W(), LENv = LEN(), CANv = CAN(), SG = sigOf(st.sig), BQ = SG.beat, g = gstep(), V = st.V, R = visRange(); vis = R;
  gin.style.width = w + 'px'; gin.style.height = NH + 'px';
  let lanes = '', cLines = '';
  for (let m = LO; m <= HI; m++) {
    if (BLK[m % 12]) lanes += rect(0, y(m) - PS / 2, w, PS);
    if (m % 12 === 0) cLines += 'M0 ' + f(y(m) + PS / 2) + 'H' + w;
  }
  let gFine = '', gBeat = '', gBar = '';
  const fine = g > 0 ? g : .25;
  if (fine * st.ppb >= 4) for (let t = Math.floor(R.a / fine) * fine; t <= Math.min(CANv, R.b) + 1e-6; t += fine) {
    if (Math.abs(t / BQ - Math.round(t / BQ)) > 1e-6) gFine += 'M' + f(X(t)) + ' 0V' + NH;
  }
  const beats = BQ * st.ppb >= 5;                                // zoomed far out: bar lines only
  for (let k = Math.floor(R.a / BQ); k * BQ <= Math.min(CANv, R.b) + 1e-6; k++) { const s = 'M' + f(X(k * BQ)) + ' 0V' + NH; if (k % SG.n === 0) gBar += s; else if (beats) gBeat += s; }
  let lines = '', dots = '', cores = '';
  for (let v = 0; v < VOICES; v++) {
    const a = V[v]; if (!a.length) continue;
    let i0 = 0; while (i0 < a.length - 1 && a[i0 + 1].t <= R.a) i0++;          // from the last dot before the window ...
    let i1 = i0; while (i1 < a.length - 1 && a[i1].t < R.b) i1++;              // ... to the first one after it
    let d = 'M' + f(X(a[i0].t)) + ' ' + f(y(a[i0].m));
    for (let i = i0 + 1; i <= i1; i++) {
      const A = a[i - 1], B = a[i];
      if (A.m === B.m) { d += 'L' + f(X(B.t)) + ' ' + f(y(B.m)); continue; }
      const N = clamp(Math.round((X(B.t) - X(A.t)) / 4), 8, 60);
      for (let k = 1; k <= N; k++) { const u = k / N; d += 'L' + f(X(A.t + (B.t - A.t) * u)) + ' ' + f(y(A.m + (B.m - A.m) * ease(u))); }
    }
    if (i1 === a.length - 1 && a[i1].t < LENv) d += 'L' + f(X(LENv)) + ' ' + f(y(a[i1].m));
    lines += '<path d="' + d + '"/>';
    for (let i = i0; i <= i1; i++) { const q = a[i]; if (q.t < R.a - 1 || q.t > R.b + 1) continue; dots += circ(X(q.t), y(q.m), 4); cores += circ(X(q.t), y(q.m), 1.6); }
  }
  const sp = st.sel && V[st.sel.v] && V[st.sel.v][st.sel.i];
  const dr = st.drag && V[st.drag.v] && V[st.drag.v][st.drag.i];
  const hv = !st.drag && st.hover && V[st.hover.v] && V[st.hover.v][st.hover.i];
  let labels = '';
  if (st.showNotes) V.forEach(a => a.forEach((q, i) => { if (q.t >= R.a - 1 && q.t <= R.b + 1 && (i === 0 || a[i - 1].m !== q.m)) labels += '<span class="or-dl" style="left: ' + Math.round(X(q.t) + 8) + 'px; top: ' + Math.round(y(q.m) - 17) + 'px">' + nn(q.m) + '</span>'; }));
  gin.innerHTML = '<svg width="' + w + '" height="' + NH + '" viewBox="0 0 ' + w + ' ' + NH + '" aria-hidden="true">' +
    '<path d="' + lanes + '" style="fill: rgba(0,0,0,.32)"/>' +
    '<path d="' + cLines + '" style="fill: none; stroke: rgba(233,230,223,.16); stroke-width: 1"/>' +
    '<path d="' + gFine + '" style="fill: none; stroke: rgba(233,230,223,.045); stroke-width: 1"/>' +
    '<path d="' + gBeat + '" style="fill: none; stroke: rgba(233,230,223,.1); stroke-width: 1"/>' +
    '<path d="' + gBar + '" style="fill: none; stroke: rgba(233,230,223,.24); stroke-width: 1"/>' +
    '<path d="' + rect(X(LENv), 0, Math.max(0, w - X(LENv)), NH) + '" style="fill: rgba(0,0,0,.38)"/>' +
    '<path d="M' + f(X(LENv)) + ' 0V' + NH + '" style="fill: none; stroke: var(--accent); stroke-width: 1.5; stroke-dasharray: 4 4; opacity: .55"/>' +
    '<g style="fill: none; stroke: ' + LC + '; stroke-width: 2.6; stroke-linecap: round; stroke-linejoin: round">' + lines + '</g>' +
    '<path class="or-ph" d="" style="fill: none; stroke: var(--accent); stroke-width: 1.5"/>' +
    '<path d="' + dots + '" style="fill: ' + LC + '; stroke: #0a0a0b; stroke-width: 1.5"/>' +
    (sp ? '<path d="' + circ(X(sp.t), y(sp.m), 7.5) + '" style="fill: none; stroke: ' + LC + '; stroke-width: 1.2"/>' : '') +
    (hv ? '<path d="' + circ(X(hv.t), y(hv.m), 6.5) + '" style="fill: none; stroke: rgba(255,255,255,.55); stroke-width: 1.2"/>' : '') +
    '<path d="' + cores + '" style="fill: #ffffff"/>' +
    (dr ? '<path d="' + circ(X(dr.t), y(dr.m), 14) + '" style="fill: none; stroke: var(--accent); stroke-width: 1.5; stroke-dasharray: 3 3"/>' : '') +
    '</svg>' + labels;
  phEl = gin.querySelector('.or-ph');
  gin.style.cursor = st.drag ? 'grabbing' : '';
}

function renderRuler() {
  const w = W(), LENv = LEN(), SG = sigOf(st.sig), BQ = SG.beat;
  rin.style.width = w + 'px';
  let ticks = '', html = '';
  const CANv = CAN(), tick = BQ * st.ppb >= 5, every = Math.max(1, Math.ceil(28 / (SG.bar * st.ppb))), R = visRange();   // zoomed out: fewer numbers
  for (let k = Math.floor(R.a / BQ); k * BQ <= Math.min(CANv, R.b) + 1e-6; k++) { if (k % SG.n && !tick) continue; const x = f(X(k * BQ)); ticks += 'M' + x + (k % SG.n === 0 ? ' 27V46' : ' 39V46'); }
  for (let b = Math.floor(R.a / SG.bar); b < Math.min(st.bars + ROOM(), R.b / SG.bar + 1); b++) if (b % every === 0) html += '<span class="or-bn' + (b >= st.bars ? ' out' : '') + '" style="left: ' + Math.round(X(b * SG.bar) + 5) + 'px">' + (b + 1) + '</span>';
  html += '<span class="or-end" style="left: ' + Math.round(X(LENv) + 4) + 'px" title="The end of the song (it loops or stops here). Draw after it to make the song longer, or set BARS">END</span>';
  marks.forEach((mk, j) => {
    const nt = j < marks.length - 1 ? marks[j + 1].t : LENv; if (nt < R.a || mk.t > R.b) { html += '<span class="or-chip" style="display: none"></span>'; return; }
    const x = X(mk.t), nx = X(nt);
    html += '<span class="or-chip" style="left: ' + Math.round(x + 2) + 'px; max-width: ' + Math.max(22, Math.round(nx - x - 6)) + 'px">' + mk.n + '</span>';
  });
  rin.innerHTML = '<svg width="' + w + '" height="46" viewBox="0 0 ' + w + ' 46" aria-hidden="true"><path d="' + ticks + '" style="fill: none; stroke: rgba(233,230,223,.3); stroke-width: 1"/>' +
    '<path class="or-rl" d="" style="fill: none; stroke: var(--accent); stroke-width: 1.5"/><path class="or-hd" d="" style="fill: var(--accent)"/></svg>' + html;
  headEl = rin.querySelector('.or-hd'); rlEl = rin.querySelector('.or-rl');
  chipEls = [...rin.querySelectorAll('.or-chip')]; curMark = -2;
}

function renderPiano(force) {
  const lit = {};
  sounding(st.drag && !st.play ? st.V[st.drag.v][st.drag.i].t : st.ph).forEach(m => { lit[Math.round(m)] = 1; });
  const key = Object.keys(lit).join(',');
  if (!force && key === pianoKey) return;
  pianoKey = key;
  let white = '', black = '', lines = '', lw = '', lb = '', mk = '', labels = '';
  for (let m = LO; m <= HI; m++) {
    const pc = m % 12, y0 = y(m) - PS / 2, on = lit[m];
    if (!BLK[pc]) {
      white += rect(0, y0, PW, PS);
      if (on) { lw += rect(0, y0, PW, PS); mk += rect(PW - 6, y0 + 1, 4, PS - 2); }
      if (m < HI) lines += BLK[(m + 1) % 12] ? 'M' + BW + ' ' + f(y(m + 1)) + 'H' + PW : 'M0 ' + f(y0) + 'H' + PW;
      if (pc === 0) labels += '<span class="or-clab" style="top: ' + Math.round(y(m) - 5) + 'px; color: ' + (on ? '#ffffff' : '#8d8d93') + '">' + nn(m) + '</span>';
    } else {
      white += rect(BW, y0, PW - BW, PS);
      black += rect(0, y0 + .5, BW, PS - 1);
      if (on) lb += rect(0, y0 + .5, BW, PS - 1);
    }
  }
  kin.innerHTML = '<svg width="' + PW + '" height="' + NH + '" viewBox="0 0 ' + PW + ' ' + NH + '" role="img" aria-label="Piano keys C2 to C7, lined up with the note rows">' +
    '<path d="' + white + '" style="fill: #e4e0d7"/><path d="' + lw + '" style="fill: ' + LC + '"/><path d="' + mk + '" style="fill: #ffd3d7; opacity: .9"/>' +
    '<path d="' + lines + '" style="fill: none; stroke: #8f8e89; stroke-width: 1"/>' +
    '<path d="' + black + '" style="fill: #1b1b1f; stroke: #0a0a0b; stroke-width: 1"/><path d="' + lb + '" style="fill: ' + LC + '"/>' +
    '<rect x="' + (PW - 1) + '" y="0" width="1" height="' + NH + '" style="fill: #0a0a0b"/></svg>' + labels;
}

const pad = (n, k) => String(n).padStart(k, '0');
function lcd(t) {
  const SG = sigOf(st.sig), bar = Math.floor(t / SG.bar + 1e-9), r = t - bar * SG.bar, beat = Math.floor(r / SG.beat + 1e-9), sub = Math.floor((r - beat * SG.beat) / .25 + 1e-6);
  return pad(bar + 1, 3) + '.' + (beat + 1) + '.' + (sub + 1);
}

function renderHead() {
  bPlay.innerHTML = st.play ? IC_PAUSE : IC_PLAY; bPlay.classList.toggle('on', st.play);
  bPlay.setAttribute('aria-label', st.play ? 'Pause' : 'Play'); bPlay.setAttribute('aria-pressed', String(st.play));
  if (K.playW) K.playW._draw();
  if (K.pageDots) K.pageDots();
  bLoop.classList.toggle('on', st.loop); bLoop.setAttribute('aria-pressed', String(st.loop));
  lBpm.innerHTML = st.bpm + ' <small>BPM</small>'; lSig.textContent = st.sig; lLen.textContent = st.bars;
  if (window.obsSynth && window.obsSynth.setBpm) window.obsSynth.setBpm(st.bpm);
  gBtns.forEach(b => { const on = b.dataset.g === st.grid; b.classList.toggle('on', on); b.setAttribute('aria-pressed', String(on)); });
  bNotes.classList.toggle('on', st.showNotes); bNotes.setAttribute('aria-pressed', String(st.showNotes));
}

function renderLive() {
  const x = X(st.ph);
  if (phEl) { phEl.setAttribute('d', 'M' + f(x) + ' 0V' + NH); phEl.style.opacity = st.play ? 1 : .55; }
  if (headEl) headEl.setAttribute('d', headPath(x));
  if (rlEl) rlEl.setAttribute('d', 'M' + f(x) + ' 29V46');
  const k = markAt(st.ph);
  if (k !== curMark) { curMark = k; chipEls.forEach((c, j) => c.classList.toggle('on', j === k)); bChord.textContent = k >= 0 ? marks[k].n : '—'; }
  const dr = st.drag && st.V[st.drag.v][st.drag.i];
  lPos.textContent = lcd(dr ? dr.t : st.ph);
  renderPiano(false);
}

function syncScroll() {
  rin.style.transform = 'translateX(' + (-scroll.scrollLeft) + 'px)';
  kin.style.transform = 'translateY(' + (-scroll.scrollTop) + 'px)';
  const cw = scroll.clientWidth || 1200, pad = cw / st.ppb, va = (scroll.scrollLeft - 12) / st.ppb, vb = (scroll.scrollLeft + cw) / st.ppb;
  if ((va < vis.a + pad / 4 && vis.a > 0) || (vb > vis.b - pad / 4 && vis.b < CAN() - 1e-6)) { renderGrid(); renderRuler(); renderLive(); }   // near the drawn part's edge: draw the new part
}
function renderAll() { miniDirty = true; keyLater(); calcMarks(); renderGrid(); renderRuler(); renderPiano(true); renderHead(); renderLive(); syncScroll(); }
function edited() { miniDirty = true; keyLater(); st.bars = fitBars(st.bars); calcMarks(); renderGrid(); renderRuler(); renderLive(); }

/* ------------------------------------------------------------ playing */
let raf = 0, lastTs = 0, kickMark = -2;
function follow() {
  const x = X(st.ph);
  if (x < scroll.scrollLeft + 20 || x > scroll.scrollLeft + scroll.clientWidth - 80) { scroll.scrollLeft = Math.max(0, x - 120); syncScroll(); }
}
function frame(ts) {
  raf = 0;
  if (!st.play) return;
  if (dev.offsetParent === null) { pause(); return; }      // the SYNTH page was left (it keeps playing on SIREN and AETHER)
  const dt = lastTs ? Math.min(.1, (ts - lastTs) / 1000) : 0; lastTs = ts;
  let ph = st.ph + dt * st.bpm / 60; const len = LEN();
  if (ph >= len) {
    if (!st.loop) { st.ph = len; pause(); return; }
    ph %= len; soundOff();                                  // back to the top: play it fresh
  }
  st.ph = ph; follow(); const now = sounding(ph); soundAt(now, .03); renderLive();
  const km = markAt(ph); if (km !== kickMark) { kickMark = km; if (km >= 0 && now.length && K.filterKick) K.filterKick(Math.min(...now)); }
  raf = requestAnimationFrame(frame);
}
function play() {
  if (st.ph >= LEN() - 1e-6) st.ph = 0;
  clearTimeout(blipT); soundOff();
  if (K.native) { st.play = true; K.native.play(st.ph); renderHead(); renderLive(); return; }   // the plugin plays it; it reports where it is
  st.play = true; lastTs = 0; kickMark = markAt(st.ph); renderHead(); renderLive();
  if (!raf) raf = requestAnimationFrame(frame);
}
function pause() { st.play = false; if (raf) cancelAnimationFrame(raf); raf = 0; soundOff(); if (K.native) K.native.stop(); renderHead(); renderLive(); }
function toStart() { st.ph = 0; scroll.scrollLeft = 0; syncScroll(); if (st.play) soundOff(); if (K.native) K.native.seek(0); renderLive(); }
/* the plugin's player: where it is (from the DAW too, when the DAW plays) */
K.orphNative = {
  pos(ph, playing, host) {
    if (!playing && !st.play) return;                             // stopped: the page owns the playhead
    const was = st.play; st.play = !!playing; st.host = !!host;
    if (playing) { st.ph = Math.max(0, Math.min(LEN(), ph)); follow(); }
    if (was !== st.play) renderHead();
    renderLive();
  },
};
K.orphMarks = () => marks.map(m => m.t);

/* ------------------------------------------------------------ header actions */
function stepSig(dir) {
  const i = SIGS.indexOf(st.sig), sig = SIGS[(i + SIGS.length + dir) % SIGS.length], oldBar = BL(), bar = sigOf(sig).bar;
  /* the length in beats you had is kept while you try signatures, so going round them all comes back to the same bars (it used to drift) */
  if (st.lenB == null || Math.abs(st.lenB - st.bars * oldBar) >= oldBar - 1e-6) st.lenB = st.bars * oldBar;
  const bars = Math.min(MAXBARS, Math.max(2, Math.ceil((lastT(st.V) + 1e-6) / bar) + 1, Math.ceil(st.lenB / bar - 1e-9)));
  st.sig = sig; st.bars = bars; st.ph = Math.min(st.ph, bars * bar);
  renderAll();
}
hl.addEventListener('click', e => {
  const b = e.target.closest('button'); if (!b) return;
  const a = b.dataset.a, step = e.shiftKey ? 5 : 1;
  if (a === 'start') toStart();
  else if (a === 'play') { if (st.play) pause(); else play(); }
  else if (a === 'stop') { pause(); toStart(); }
  else if (a === 'loop') { st.loop = !st.loop; renderHead(); }
  else if (a === 'bpm-' || a === 'bpm+') { st.bpm = clamp(st.bpm + (a === 'bpm+' ? step : -step), 40, 220); renderHead(); }
  else if (a === 'sig-' || a === 'sig+') stepSig(a === 'sig+' ? 1 : -1);
});
hr.addEventListener('click', e => {
  const b = e.target.closest('button'); if (!b) return;
  if (b.dataset.g) { st.grid = b.dataset.g; renderGrid(); renderHead(); renderLive(); }
  else if (b.dataset.a === 'notes') { st.showNotes = !st.showNotes; renderGrid(); renderHead(); renderLive(); }
  else if (b.dataset.a === 'len-' || b.dataset.a === 'len+') { const n = e.shiftKey ? 8 : 1, min = Math.max(2, Math.ceil((lastT(st.V) + 1e-6) / BL()) + 1);
    st.bars = clamp(st.bars + (b.dataset.a === 'len+' ? n : -n), min, MAXBARS); st.ph = Math.min(st.ph, LEN()); miniDirty = true; renderHead(); }
});

/* ------------------------------------------------------------ editing */
const pt = (e, node) => { const r = node.getBoundingClientRect(), s = r.width / node.offsetWidth || 1; return { x: (e.clientX - r.left) / s, y: (e.clientY - r.top) / s }; };
function hitDot(q) {
  let hit = null, bd = 12;
  for (let v = 0; v < VOICES; v++) st.V[v].forEach((d, i) => { const dist = Math.hypot(q.x - X(d.t), q.y - y(d.m)); if (dist < bd) { bd = dist; hit = { v, i }; } });
  return hit;
}
/* editing, made calm:
   - drag a dot to move it (it only moves once the mouse has really moved; SHIFT: only up and down)
   - drag empty space to move around the song
   - double-click a line: a new dot on it; double-click a dot: it goes; double-click off the lines: a new voice (while one is free)
   - the piano only plays the note you click
   - CTRL+Z / CTRL+SHIFT+Z: undo / redo */
const hist = [], redo = [];
const snap = () => ({ V: st.V.map(a => a.map(q => ({ t: q.t, m: q.m }))), bars: st.bars });
function remember() { hist.push(snap()); if (hist.length > 200) hist.shift(); redo.length = 0; }
function undoRedo(back) {
  const from = back ? hist : redo, to = back ? redo : hist; if (!from.length) return;
  to.push(snap()); const s = from.pop(); st.V = s.V; st.bars = s.bars; st.sel = null; st.drag = null; st.hover = null; soundOff(); edited();
}
let press = null, pan = null;
const DEAD = 4;
gin.addEventListener('pointerdown', e => {
  if (e.button !== 0) return;
  const q = pt(e, gin), hit = hitDot(q);
  try { gin.setPointerCapture(e.pointerId); } catch (_) {}
  if (hit) { press = { hit, x: e.clientX, y: e.clientY, moved: false }; st.sel = hit; renderGrid(); renderLive(); return; }
  pan = { x: e.clientX, y: e.clientY, l: scroll.scrollLeft, t: scroll.scrollTop, moved: false };
  gin.style.cursor = 'grabbing';
});
gin.addEventListener('pointermove', e => {
  if (pan) {
    const r = gin.getBoundingClientRect(), s = r.width / gin.offsetWidth || 1, dx = (e.clientX - pan.x) / s, dy = (e.clientY - pan.y) / s;
    if (!pan.moved && Math.hypot(dx, dy) < DEAD) return;
    pan.moved = true; scroll.scrollLeft = pan.l - dx; scroll.scrollTop = pan.t - dy; syncScroll(); return;
  }
  if (press && !st.drag) {
    if (Math.hypot(e.clientX - press.x, e.clientY - press.y) < DEAD) return;
    remember(); st.drag = press.hit; press.moved = true;
  }
  const d = st.drag;
  if (!d) { const hv = hitDot(pt(e, gin)), same = hv && st.hover && hv.v === st.hover.v && hv.i === st.hover.i;
    if (!same && (hv || st.hover)) { st.hover = hv; renderGrid(); } gin.style.cursor = hv ? 'grab' : ''; return; }
  const q = pt(e, gin), a = st.V[d.v], cur = a[d.i];
  const lo = d.i > 0 ? a[d.i - 1].t + minStep() : 0;
  const hi = d.i < a.length - 1 ? a[d.i + 1].t - minStep() : CAN();
  const t = e.shiftKey ? cur.t : clamp(snapT((q.x - 12) / st.ppb), lo, Math.max(lo, hi)), m = snapM(mOf(q.y));
  if (cur.t === t && cur.m === m) return;
  a[d.i] = { t, m };
  edited();
  if (!st.play) soundAt(sounding(t), .02);                       // hear the chord while you drag
});
const endPress = () => {
  if (pan) { if (!pan.moved && st.sel) { st.sel = null; renderGrid(); renderLive(); } pan = null; gin.style.cursor = ''; }
  if (press) { press = null; if (st.drag) { st.drag = null; renderGrid(); renderLive(); if (!st.play) soundOff(); } }
};
gin.addEventListener('pointerup', endPress); gin.addEventListener('pointercancel', endPress); gin.addEventListener('lostpointercapture', endPress);
gin.addEventListener('pointerleave', () => { if (st.hover && !press && !pan) { st.hover = null; renderGrid(); } });
gin.addEventListener('dblclick', e => {
  const q = pt(e, gin), hit = hitDot(q);
  if (hit) {                                                     // double-click a dot: it goes
    remember(); st.V[hit.v].splice(hit.i, 1); st.sel = null; st.drag = null; st.hover = null; soundOff();
    edited(); return;
  }
  const t = Math.max(0, snapT((q.x - 12) / st.ppb));
  let best = -1, bd = 1e9, onLine = null;
  for (let v = 0; v < VOICES; v++) {                             // the line under the mouse (a voice sounds there)
    const pm = pitchAt(v, t); if (pm == null) continue;
    const dist = Math.abs(y(pm) - q.y); if (dist < bd) { bd = dist; best = v; onLine = pm; }
  }
  let v, m;
  if (best >= 0 && bd <= 10) { v = best; m = Math.round(onLine); }
  else { v = st.V.findIndex(a => !a.length); m = snapM(mOf(q.y)); if (v < 0) return; }   // off the lines: only a free voice starts there
  const a = st.V[v];
  if (a.some(d => Math.abs(d.t - t) < minStep() / 2)) return;
  remember(); a.push({ t, m }); a.sort((d1, d2) => d1.t - d2.t);
  st.sel = { v, i: a.findIndex(d => d.t === t && d.m === m) };
  edited();
  if (!st.play) blip(t);
});
function removeSel() {
  const s = st.sel; if (!s || !st.V[s.v][s.i]) return;
  remember(); st.V[s.v].splice(s.i, 1); st.sel = null; st.hover = null; edited();
}

/* the upright piano: lit = what sounds under the playhead; a click plays that note (it never changes the song) */
let keyNote = null;
kin.addEventListener('pointerdown', e => {
  if (e.button !== 0 || st.play || !canSound()) return;
  const m = clamp(Math.round(mOf(pt(e, kin).y)), LO, HI);
  clearTimeout(blipT); soundOff(); soundAt([m], 0); keyNote = m;
  try { kin.setPointerCapture(e.pointerId); } catch (_) {}
});
const keyUp = () => { if (keyNote != null) { keyNote = null; soundOff(); } };
kin.addEventListener('pointerup', keyUp); kin.addEventListener('pointercancel', keyUp); kin.addEventListener('lostpointercapture', keyUp);

/* the ruler: click or drag to move the playhead */
let scrub = false;
const seek = e => { st.ph = clamp(snapT((pt(e, rin).x - 12) / st.ppb), 0, LEN()); if (st.play) soundOff(); if (K.native) K.native.seek(st.ph); renderLive(); };
ruler.addEventListener('pointerdown', e => { if (e.button !== 0) return; scrub = true; try { ruler.setPointerCapture(e.pointerId); } catch (_) {} seek(e); });
ruler.addEventListener('pointermove', e => { if (scrub) seek(e); });
const endScrub = () => { scrub = false; };
ruler.addEventListener('pointerup', endScrub); ruler.addEventListener('pointercancel', endScrub);

/* the wheel zooms around the mouse; shift+wheel or a sideways swipe scrolls; over the keys (or alt+wheel) it scrolls up and down */
function zoom(e) {
  const r = scroll.getBoundingClientRect(), s = r.width / scroll.offsetWidth || 1, mx = (e.clientX - r.left) / s;
  const old = st.ppb, t = (scroll.scrollLeft + mx - 12) / old;
  const ppb = clamp(old * Math.exp(-e.deltaY * .0016), 3, 320);
  if (Math.abs(ppb - old) < .01) return;
  st.ppb = ppb; renderGrid(); renderRuler(); renderLive();
  scroll.scrollLeft = Math.max(0, 12 + t * ppb - mx); syncScroll();
}
const onWheel = e => {
  if (Math.abs(e.deltaX) > Math.abs(e.deltaY)) return;
  if (e.altKey) { e.preventDefault(); scroll.scrollTop += e.deltaY; return; }
  e.preventDefault();
  if (e.shiftKey) { scroll.scrollLeft += e.deltaY; return; }
  zoom(e);
};
scroll.addEventListener('wheel', onWheel, { passive: false });
ruler.addEventListener('wheel', onWheel, { passive: false });
ks.addEventListener('wheel', e => { e.preventDefault(); scroll.scrollTop += e.deltaY; }, { passive: false });
scroll.addEventListener('scroll', syncScroll);

/* ------------------------------------------------------------ bottom band */
back.addEventListener('click', e => { e.stopPropagation(); if (K.showPage) K.showPage('siren'); });
let cfYes = null, cfFrom = null;
const cfT = cf.querySelector('#or-cf-t'), cfD = cf.querySelector('#or-cf-d'), cfB = cf.querySelector('.yes');
function ask(title, text, yes, onYes) {
  if (st.play) pause();
  cfT.textContent = title; cfD.textContent = text; cfB.textContent = yes; cfYes = onYes; cfFrom = document.activeElement;
  cf.classList.add('show'); setTimeout(() => cf.querySelector('.no').focus(), 0);
}
K.ask = ask;
const openCf = () => ask('CLEAR EVERYTHING?', 'Every note is removed, and tempo, time signature, grid and zoom go back to default. This can\'t be undone.', 'CLEAR ALL', clearAll);
const closeCf = () => { cf.classList.remove('show'); if (cfFrom && cfFrom.focus) cfFrom.focus(); cfFrom = null; };
rst.addEventListener('click', e => { e.stopPropagation(); openCf(); });
cf.addEventListener('click', e => {
  e.stopPropagation();
  if (e.target === cf || e.target.closest('.no')) { cfYes = null; closeCf(); return; }
  if (e.target.closest('.yes')) { const f = cfYes; cfYes = null; closeCf(); if (f) f(); }
});
function clearAll() {
  hist.length = 0; redo.length = 0;
  pause(); clearTimeout(blipT); soundOff();
  Object.assign(st, DEF, { V: [[], [], [], []], ph: 0, drag: null, sel: null });
  renderAll();
  scroll.scrollLeft = 0; centerOn(60); syncScroll();
}
function centerOn(m) { scroll.scrollTop = Math.max(0, y(m) - scroll.clientHeight / 2); syncScroll(); }
function centerNotes() {
  let lo = 1e9, hi = -1e9; st.V.forEach(a => a.forEach(q => { lo = Math.min(lo, q.m); hi = Math.max(hi, q.m); }));
  centerOn(hi >= lo ? (lo + hi) / 2 : 60);
}

/* ------------------------------------------------------------ keys: SPACE plays, DELETE removes the chosen dot, ESC closes the question */
addEventListener('keydown', e => {
  if (cf.classList.contains('show')) { if (e.key === 'Escape') { e.preventDefault(); cfYes = null; closeCf(); } return; }
  if (!dev.classList.contains('page-orpheus') || dev.offsetParent === null) return;
  if (/INPUT|TEXTAREA|SELECT/.test(e.target.tagName)) return;
  if ((e.ctrlKey || e.metaKey) && (e.key === 'z' || e.key === 'Z' || e.key === 'y')) { e.preventDefault(); undoRedo(e.key === 'z' && !e.shiftKey); return; }
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  if (e.key === ' ') {
    e.preventDefault();
    if (document.activeElement && dev.contains(document.activeElement)) document.activeElement.blur();
    if (!e.repeat) { if (st.play) pause(); else play(); }
    return;
  }
  if ((e.key === 'Delete' || e.key === 'Backspace') && st.sel) { e.preventDefault(); removeSel(); }
});

/* ------------------------------------------------------------ the page */
let firstShow = true;
const prevShown = K.pageShown;
K.pageShown = p => {
  if (prevShown) prevShown(p);
  if (p === 'orpheus') setTimeout(() => { renderAll(); if (firstShow) { firstShow = false; centerNotes(); } }, 0);
  else { clearTimeout(blipT); if (!st.play) soundOff(); miniDirty = true; }
};
/* ------------------------------------------------------------ the song's key (Krumhansl-Schmuckler: the notes as they sound, held time counts) */
const KS_MAJ = [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88], KS_MIN = [6.33, 2.68, 3.52, 5.38, 2.6, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17];
function songKey() {
  const h = new Array(12).fill(0), end = Math.max(LEN(), 1);
  for (let t = 0; t < end; t += .25) for (let v = 0; v < VOICES; v++) { const m = pitchAt(v, t); if (m != null) h[((Math.round(m) % 12) + 12) % 12] += 1; }
  if (!h.some(x => x)) return null;
  const corr = (prof, r) => { const xs = h.map((_, i) => prof[(i - r + 12) % 12]), mx = xs.reduce((a, b) => a + b) / 12, my = h.reduce((a, b) => a + b) / 12;
    let n = 0, dx = 0, dy = 0; for (let i = 0; i < 12; i++) { n += (xs[i] - mx) * (h[i] - my); dx += (xs[i] - mx) ** 2; dy += (h[i] - my) ** 2; } return n / Math.sqrt(dx * dy || 1); };
  let best = null;
  for (let r = 0; r < 12; r++) [[KS_MAJ, 0], [KS_MIN, 1]].forEach(([p, minor]) => { const c = corr(p, r); if (!best || c > best.c) best = { c, r, minor }; });
  return best;
}
let keyT = 0;
function keyNow() {
  const k = songKey();
  const was = spellKey ? spellKey.r * 2 + spellKey.minor : -1; spellKey = k;
  lKey.textContent = k ? pcName(k.r) + (k.minor ? ' MIN' : ' MAJ') : '—';
  if (k && k.r * 2 + k.minor !== was) { calcMarks(); renderGrid(); renderLive(); miniDirty = true; }   // the names follow the key's spelling
  if (k) { if (Math.round(K.S['syn.key']) !== k.r) K.set('syn.key', k.r, 'orpheus'); if (Math.round(K.S['syn.scale']) !== k.minor) K.set('syn.scale', k.minor, 'orpheus'); }
}
function keyLater() { clearTimeout(keyT); keyT = setTimeout(keyNow, 350); }

/* ------------------------------------------------------------ saving: the song here, the sound in sirenfx.js (all in this browser) */
K.orphSave = () => ({ V: st.V, bars: st.bars, sig: st.sig, grid: st.grid, ppb: st.ppb, bpm: st.bpm, loop: st.loop, showNotes: st.showNotes });
K.orphLoad = o => {
  if (!o || !Array.isArray(o.V) || o.V.length !== VOICES) return;
  { let hh = 0; for (const c of JSON.stringify(o.V)) hh = (hh * 31 + c.charCodeAt(0)) | 0; if (hh === -1651659876) return; }   // the removed NEON RAIN demo, unedited: keep the default song
  const V = o.V.map(a => (Array.isArray(a) ? a : []).filter(q => q && isFinite(q.t) && isFinite(q.m)).map(q => ({ t: Math.max(0, +q.t), m: clamp(Math.round(+q.m), LO, HI) })).sort((p, q) => p.t - q.t));
  st.V = V; if (SIGS.includes(o.sig)) st.sig = o.sig; if (GRIDS.some(g => g[0] === o.grid)) st.grid = o.grid;
  if (isFinite(o.ppb)) st.ppb = clamp(+o.ppb, 3, 320); if (isFinite(o.bpm)) st.bpm = clamp(Math.round(+o.bpm), 40, 220);
  st.loop = o.loop !== false; st.showNotes = !!o.showNotes; st.bars = clamp(isFinite(o.bars) ? Math.round(+o.bars) : DEF.bars, 2, MAXBARS); st.bars = fitBars(st.bars);
  st.ph = 0; st.sel = null; renderAll(); firstShow = true;
};

/* ------------------------------------------------------------ SIREN's top row: PLAY and the small window on the song */
let miniDirty = true;
if (K.orphLink) Object.assign(K.orphLink, { playing: () => st.play, setPlay: v => { if (v) play(); else pause(); }, open: () => { if (K.showPage) K.showPage('orpheus'); } });
function drawMini() {                       // just the lines and the chord: what's played is dim, what's coming is bright
  const cv = K.miniLed && K.miniLed.querySelector('canvas'); if (!cv) return;
  const [g, w, h] = K.hidpi(cv); if (!g) return;
  const font = K.FONT ? K.FONT() : 'Saira, sans-serif', k = markAt(st.ph);
  g.textBaseline = 'top'; g.fillStyle = K.tc('f1eee7'); g.font = '600 11px ' + font; g.fillText(k >= 0 ? marks[k].n : '—', 0, 0);
  const top = 19, bot = h - 2, span = 8, t0 = st.ph - 2;
  let lo = 1e9, hi = -1e9; st.V.forEach(a => a.forEach(q => { lo = Math.min(lo, q.m); hi = Math.max(hi, q.m); }));
  if (hi < lo) return;
  lo -= 1; hi += 1; if (hi - lo < 12) { const c = (hi + lo) / 2; lo = c - 6; hi = c + 6; }
  const yy = m => top + (hi - m) / (hi - lo) * (bot - top), Xm = t => (t - t0) / span * w, dt = span / w * 2;
  g.lineWidth = 1.5; g.lineJoin = 'round'; g.lineCap = 'round';
  [[t0, st.ph, .32], [st.ph, t0 + span + dt, 1]].forEach(([a, b, al]) => {
    g.strokeStyle = LC; g.globalAlpha = al;
    for (let v = 0; v < VOICES; v++) {
      g.beginPath(); let pen = false;
      for (let t = Math.max(0, a); t <= b; t += dt) { const tt = Math.min(t, b), m = pitchAt(v, tt); if (m == null) { pen = false; continue; } const x = Xm(tt), yv = yy(m); if (pen) g.lineTo(x, yv); else { g.moveTo(x, yv); pen = true; } }
      g.stroke();
    }
  });
  g.globalAlpha = 1;
}
(function miniLoop() {
  requestAnimationFrame(miniLoop);
  if (!K.miniLed || dev.offsetParent === null || dev.classList.contains('page-orpheus') || dev.classList.contains('page-aether')) return;
  if (!st.play && !miniDirty) return;
  miniDirty = false; drawMini();
})();
K.hasOrpheus = true;
renderAll();
window.obsSynth = window.obsSynth || {};
window.obsSynth.orpheus = () => st;        // the song (for checking)
window.obsSynth._orphEdited = () => edited();   // (for timing checks)
})();
</script>
