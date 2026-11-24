<script>
/* =============================================================================
   OBSIDIAN SYNTH: the SYNTH page.
   Played by the guitar (or a MIDI keyboard). Signal flows left to right:
     PLAY -> SOUND (osc A, osc B, sub + noise) -> FILTER -> ENVELOPE
   and below: ARPEGGIATOR, LFO, OUTPUT.
   * In the plugin, plugin/WebUI/obsidian-bridge.js connects it to the engine
     through window.obsSynth (read / write / steps / live / noteOut).
   * In a browser it plays by itself (Web Audio): keys A-K.
   The parameter list mirrors plugin/Source/DSP/SynthDefs.h (and AtmosDefs.h for the
   AETHER page, which aether.js builds on the same device).
   ============================================================================= */
(function () {
'use strict';
const SP = document.querySelector('.page[data-page="SYNTH"]'); if (!SP) return;
const $ = (s, r = document) => r.querySelector(s), $$ = (s, r = document) => [...r.querySelectorAll(s)];
const clamp = (x, a, b) => Math.max(a, Math.min(b, x));
const MINUS = '−';
const sg = (v, d = 0) => { const r = +(+v).toFixed(d); return (r > 0 ? '+' : r < 0 ? MINUS : '') + Math.abs(r).toFixed(d); };
const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };

/* ------------------------------------------------------------ parameters */
/* [id, type, min, max, default, unit or choices, skew]   (same as SynthDefs.h) */
const DEFS = [
  ['syn.sens', 'f', 0, 100, 55, '%', 1], ['syn.gate', 'f', -90, -30, -66, 'dB', 1], ['syn.glide', 'f', 0, 1000, 40, 'ms', .5],
  ['syn.dyn', 'f', 0, 100, 60, '%', 1], ['syn.legato', 'bool', 0, 1, 0], ['syn.hold', 'bool', 0, 1, 0],
  ['syn.chord', 'choice', 0, 6, 0, 'Off|Octaves|Power|Triad|Sus|Seventh|Ninth'], ['syn.key', 'choice', 0, 11, 4, 'C|C#|D|D#|E|F|F#|G|G#|A|A#|B'],
  ['syn.scale', 'choice', 0, 6, 1, 'Major|Minor|Dorian|Phrygian|Harmonic minor|Pentatonic|Blues'], ['syn.tune', 'f', 415, 466, 440, 'Hz', 1],
  ['syn.a.on', 'bool', 0, 1, 1], ['syn.a.wave', 'choice', 0, 4, 0, 'Saw|Pulse|Triangle|Sine|Super'], ['syn.a.pitch', 'int', -24, 24, 0, 'st'],
  ['syn.a.fine', 'f', -100, 100, 0, 'ct', 1], ['syn.a.shape', 'f', 0, 100, 0, '%', 1], ['syn.a.level', 'f', 0, 100, 80, '%', 1],
  ['syn.a.voices', 'choice', 0, 3, 1, '1|3|5|7'], ['syn.a.detune', 'f', 0, 100, 22, '%', 1], ['syn.a.width', 'f', 0, 100, 70, '%', 1],
  ['syn.b.on', 'bool', 0, 1, 1], ['syn.b.wave', 'choice', 0, 4, 1, 'Saw|Pulse|Triangle|Sine|Super'], ['syn.b.pitch', 'int', -24, 24, -12, 'st'],
  ['syn.b.fine', 'f', -100, 100, 7, 'ct', 1], ['syn.b.shape', 'f', 0, 100, 35, '%', 1], ['syn.b.level', 'f', 0, 100, 45, '%', 1],
  ['syn.b.voices', 'choice', 0, 3, 0, '1|3|5|7'], ['syn.b.detune', 'f', 0, 100, 12, '%', 1], ['syn.b.width', 'f', 0, 100, 40, '%', 1], ['syn.b.sync', 'bool', 0, 1, 0],
  ['syn.sub.level', 'f', 0, 100, 20, '%', 1], ['syn.sub.wave', 'choice', 0, 1, 0, 'Sine|Square'], ['syn.sub.oct', 'choice', 0, 1, 0, '-1|-2'],
  ['syn.noise.level', 'f', 0, 100, 0, '%', 1], ['syn.noise.color', 'f', 0, 100, 50, '%', 1],
  ['syn.flt.on', 'bool', 0, 1, 1], ['syn.flt.type', 'choice', 0, 6, 0, 'Ladder 24|Ladder 12|Lowpass|Bandpass|Highpass|Notch|Vowel'],
  ['syn.flt.cutoff', 'log', 20, 20000, 1800, 'Hz'], ['syn.flt.res', 'f', 0, 100, 30, '%', 1], ['syn.flt.drive', 'f', 0, 100, 20, '%', 1],
  ['syn.flt.env', 'f', -100, 100, 40, '%', 1], ['syn.flt.key', 'f', 0, 100, 50, '%', 1],
  ['syn.env.a.att', 'f', 1, 5000, 3, 'ms', .25], ['syn.env.a.dec', 'f', 5, 5000, 700, 'ms', .3], ['syn.env.a.sus', 'f', 0, 100, 80, '%', 1], ['syn.env.a.rel', 'f', 5, 5000, 260, 'ms', .3],
  ['syn.env.f.att', 'f', 1, 5000, 2, 'ms', .25], ['syn.env.f.dec', 'f', 5, 5000, 420, 'ms', .3], ['syn.env.f.sus', 'f', 0, 100, 30, '%', 1], ['syn.env.f.rel', 'f', 5, 5000, 300, 'ms', .3],
  ['syn.lfo.shape', 'choice', 0, 5, 0, 'Sine|Triangle|Saw|Square|Random|Drift'], ['syn.lfo.rate', 'log', .05, 20, 5, 'Hz'], ['syn.lfo.sync', 'bool', 0, 1, 0],
  ['syn.lfo.div', 'choice', 0, 7, 3, '1/1|1/2|1/4|1/8|1/8T|1/16|1/16T|1/32'], ['syn.lfo.depth', 'f', 0, 100, 0, '%', 1], ['syn.lfo.fade', 'f', 0, 3000, 0, 'ms', .5],
  ['syn.lfo.target', 'choice', 0, 4, 0, 'Pitch|Cutoff|Shape|Volume|Pan'],
  ['syn.arp.on', 'bool', 0, 1, 0], ['syn.arp.pattern', 'choice', 0, 4, 0, 'Up|Down|Up and down|Random|Steps'], ['syn.arp.rate', 'choice', 0, 5, 3, '1/4|1/8|1/8T|1/16|1/16T|1/32'],
  ['syn.arp.len', 'int', 1, 16, 16, 'steps'], ['syn.arp.oct', 'choice', 0, 3, 1, '1|2|3|4'], ['syn.arp.gate', 'f', 5, 100, 60, '%', 1], ['syn.arp.swing', 'f', 0, 75, 0, '%', 1],
  ['syn.mix', 'f', 0, 100, 50, '%', 1], ['syn.level', 'f', -24, 12, 0, 'dB', 1], ['syn.space', 'f', 0, 100, 20, '%', 1],
  ['syn.char', 'choice', 0, 3, 1, 'Clean|Analog|8-bit|VHS'], ['syn.post', 'bool', 0, 1, 1],
  /* AETHER (noise, ambience, atmospheres): same as AtmosDefs.h */
  ['atm.on', 'bool', 0, 1, 0], ['atm.level', 'f', -36, 6, -6, 'dB', 1], ['atm.follow', 'f', -100, 100, 0, '%', 1], ['atm.evolve', 'f', 0, 100, 35, '%', 1],
  ['atm.grit', 'f', 0, 100, 0, '%', 1], ['atm.tone', 'f', -100, 100, 0, '%', 1], ['atm.width', 'f', 0, 100, 80, '%', 1], ['atm.space', 'f', 0, 100, 45, '%', 1],
  ['atm.size', 'f', 0, 100, 60, '%', 1], ['atm.freeze', 'bool', 0, 1, 0],
  ['atm.bed.on', 'bool', 0, 1, 1], ['atm.bed.type', 'choice', 0, 6, 1, 'Hiss|Wind|Rain|Surf|Rumble|Static|Breath'], ['atm.bed.tone', 'f', 0, 100, 50, '%', 1],
  ['atm.bed.motion', 'f', 0, 100, 35, '%', 1], ['atm.bed.depth', 'f', 0, 100, 50, '%', 1], ['atm.bed.level', 'f', 0, 100, 60, '%', 1],
  ['atm.drn.on', 'bool', 0, 1, 1], ['atm.drn.voice', 'choice', 0, 6, 1, 'Organ|Strings|Choir|Glass|Abyss|Descent|Ascent'],
  ['atm.drn.chord', 'choice', 0, 7, 1, 'Root|Fifth|Octaves|Minor|Sus|Tritone|Cluster|Dread'], ['atm.drn.oct', 'choice', 0, 3, 1, 'Sub|Low|Mid|High'],
  ['atm.drn.follow', 'bool', 0, 1, 0], ['atm.drn.detune', 'f', 0, 100, 25, '%', 1], ['atm.drn.tone', 'f', 0, 100, 45, '%', 1], ['atm.drn.motion', 'f', 0, 100, 30, '%', 1], ['atm.drn.level', 'f', 0, 100, 35, '%', 1],
  ['atm.tex.on', 'bool', 0, 1, 0], ['atm.tex.type', 'choice', 0, 9, 1, 'Vinyl|Fire|Drips|Metal|Shimmer|Whispers|Heartbeat|Knocks|Moans|Flesh'],
  ['atm.tex.density', 'f', 0, 100, 40, '%', 1], ['atm.tex.tone', 'f', 0, 100, 50, '%', 1], ['atm.tex.chaos', 'f', 0, 100, 40, '%', 1], ['atm.tex.level', 'f', 0, 100, 50, '%', 1],
];
const P = {}, S = {};
DEFS.forEach(([id, type, min, max, def, extra, skew]) => {
  P[id] = { id, type, min, max, def, skew: skew || 1, unit: type === 'choice' ? '' : (extra || ''), opts: type === 'choice' ? extra.split('|') : null };
  S[id] = def;
});
const toN = (id, v = S[id]) => { const p = P[id];          // real -> 0..1 (same maths as JUCE / the bridge)
  if (p.type === 'bool') return v >= .5 ? 1 : 0;
  if (p.type === 'log') return clamp(Math.log(v / p.min) / Math.log(p.max / p.min), 0, 1);
  let n = clamp((v - p.min) / (p.max - p.min), 0, 1);
  if (p.type === 'f' && p.skew !== 1 && n > 0) n = Math.pow(n, p.skew);
  return n; };
const toV = (id, n) => { const p = P[id]; n = clamp(+n, 0, 1);                // 0..1 -> real
  if (p.type === 'bool') return n >= .5 ? 1 : 0;
  if (p.type === 'log') return p.min * Math.pow(p.max / p.min, n);
  if (p.type === 'f' && p.skew !== 1) n = Math.pow(n, 1 / p.skew);
  const v = p.min + (p.max - p.min) * n;
  return p.type === 'int' || p.type === 'choice' ? Math.round(v) : v; };
const listeners = [];
let quiet = 0;
function set(id, v, from) {
  const p = P[id]; if (!p) return;
  v = +v; if (!isFinite(v)) return;
  v = clamp(v, p.min, p.max); if (p.type !== 'f' && p.type !== 'log') v = Math.round(v);
  if (S[id] === v) return;
  S[id] = v;
  listeners.forEach(fn => fn(id, v, from));
}
const onChange = fn => listeners.push(fn);
const on = id => S[id] >= .5;

/* value text */
const ms = v => v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2) + ' s' : Math.round(v) + ' ms';
const hz = v => v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2) + ' kHz' : Math.round(v) + ' Hz';
function fmt(id, v = S[id]) {
  const p = P[id];
  if (p.opts) return p.opts[Math.round(v)] || '';
  switch (id) {
    case 'syn.glide': return v < 1 ? 'Off' : ms(v);
    case 'syn.lfo.fade': return v < 1 ? 'Off' : ms(v);
    case 'syn.gate': return Math.round(v) + ' dB';
    case 'syn.tune': return v.toFixed(0) + ' Hz';
    case 'syn.level': return sg(v, 1) + ' dB';
    case 'syn.flt.env': return sg(Math.round(v)) + '%';
    case 'syn.noise.color': return v < 34 ? 'Dark' : v < 67 ? 'Pink' : 'Bright';
    case 'syn.lfo.rate': return v < 1 ? v.toFixed(2) + ' Hz' : v.toFixed(1) + ' Hz';
    case 'syn.arp.len': return Math.round(v) + ' steps';
    case 'atm.level': return sg(v, 1) + ' dB';
    case 'atm.follow': return Math.abs(v) < 2 ? 'Off' : (v < 0 ? 'Duck ' : 'Swell ') + Math.round(Math.abs(v)) + '%';
    case 'atm.tone': return Math.abs(v) < 2 ? 'Neutral' : (v < 0 ? 'Dark ' : 'Bright ') + Math.round(Math.abs(v));
    case 'atm.size': { const t = 1.2 * Math.pow(25, v / 100); return (t < 10 ? t.toFixed(1) : Math.round(t)) + ' s'; }   // the tail it leaves (as in Atmos.h)
    case 'atm.space': return v < 1 ? 'Dry' : Math.round(v) + '%';
  }
  if (p.unit === 'ms') return ms(v);
  if (p.unit === 'Hz') return hz(v);
  if (p.unit === 'st') return sg(Math.round(v)) + ' st';
  if (p.unit === 'ct') return sg(Math.round(v)) + ' ct';
  if (p.unit === '%') return Math.round(v) + '%';
  return String(Math.round(v));
}

/* arpeggiator steps: {on, acc, tie, oct -1..2, ratchet 1..4, chance 0..100, note 0..14} */
const STEP_DEF = i => ({ on: [1,0,1,1, 1,0,1,0, 1,0,1,1, 1,0,1,1][i], acc: i % 4 === 0 ? 1 : 0, tie: 0, oct: [0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0][i], ratchet: 1, chance: 100, note: [0,0,2,4, 7,0,4,0, 0,0,2,4, 7,0,9,4][i] });
let steps = Array.from({ length: 16 }, (_, i) => STEP_DEF(i));
const packStep = s => (s.on ? 1 : 0) | (s.acc ? 2 : 0) | (s.tie ? 4 : 0) | ((s.oct + 1) & 7) << 3 | ((s.ratchet - 1) & 3) << 6 | (s.chance & 127) << 8 | (s.note & 15) << 15;
const unpackStep = v => ({ on: v & 1, acc: (v >> 1) & 1, tie: (v >> 2) & 1, oct: Math.min(2, ((v >> 3) & 7) - 1), ratchet: ((v >> 6) & 3) + 1, chance: Math.min(100, (v >> 8) & 127), note: Math.min(14, (v >> 15) & 15) });
const stepListeners = [];
function stepsChanged(from) { stepListeners.forEach(fn => fn(from)); }

/* ------------------------------------------------------------ theme colours for canvases */
const root = document.documentElement;
function tc(hex, a = 1, role = 'f') {
  let v = getComputedStyle(root).getPropertyValue('--c' + role + '_' + hex).trim();
  if (!v) v = [0, 2, 4].map(i => parseInt(hex.substr(i, 2), 16)).join(',');
  return 'rgba(' + v + ',' + a + ')';
}
function accent(a = 1) {
  let c = getComputedStyle(root).getPropertyValue('--accent').trim() || '#f3ead8';
  if (c[0] === '#') { if (c.length === 4) c = '#' + c[1] + c[1] + c[2] + c[2] + c[3] + c[3]; return 'rgba(' + [1, 3, 5].map(i => parseInt(c.substr(i, 2), 16)).join(',') + ',' + a + ')'; }
  const m = c.match(/[\d.]+/g); return m ? 'rgba(' + m.slice(0, 3).join(',') + ',' + a + ')' : c;
}
function hidpi(cv) {
  const d = window.devicePixelRatio || 1, w = cv.clientWidth, h = cv.clientHeight;
  if (!w || !h) return [null, w, h];
  if (cv.width !== Math.round(w * d) || cv.height !== Math.round(h * d)) { cv.width = Math.round(w * d); cv.height = Math.round(h * d); }
  const g = cv.getContext('2d'); g.setTransform(d, 0, 0, d, 0, 0); g.clearRect(0, 0, w, h); return [g, w, h];
}
const FONT = () => getComputedStyle(SP).fontFamily || 'sans-serif';

/* ------------------------------------------------------------ styles */
const css = el('style');
css.textContent = `
.sy-root { position: absolute; inset: 0; display: flex; align-items: flex-start; justify-content: center; padding-top: 8px; user-select: none; }
/* ---- the device: a brushed obsidian faceplate with an inlaid frame ---- */
.sy-dev { --h-osc: #3fd0c0; --h-oscb: #6fa8ff; --h-sub: #b48cff; --h-flt: #f2b14c; --h-env: #ff7b8c; --h-mot: #7cc6ff;
  width: 1320px; height: 572px; box-sizing: border-box; position: relative; border-radius: 22px;
  background: radial-gradient(90% 60% at 25% 0%, rgba(255,255,255,.07), transparent 60%), repeating-linear-gradient(90deg, rgba(255,255,255,.014) 0 1px, transparent 1px 3px), linear-gradient(172deg, #232327, #141416 55%, #0e0e10);
  box-shadow: inset 0 1px 0 rgba(255,255,255,.13), inset 0 0 0 1px rgba(255,255,255,.05), inset 0 -3px 8px rgba(0,0,0,.6), 0 26px 54px rgba(0,0,0,.55), 0 2px 0 rgba(0,0,0,.8); }
.sy-dev::before { content: ''; position: absolute; inset: 9px; border-radius: 16px; border: 1px solid var(--accent); opacity: .3; pointer-events: none; }
.sy-screw { position: absolute; width: 9px; height: 9px; border-radius: 50%; background: radial-gradient(circle at 40% 35%, #8a8984, #2a2a2c 70%); box-shadow: inset 0 0 0 1px rgba(0,0,0,.6), 0 1px 0 rgba(255,255,255,.08); }
.sy-screw::after { content: ''; position: absolute; left: 1px; right: 1px; top: 4px; height: 1px; background: rgba(0,0,0,.7); transform: rotate(35deg); }
.sy-screw.tl { top: 15px; left: 15px; } .sy-screw.tr { top: 15px; right: 15px; } .sy-screw.bl { bottom: 15px; left: 15px; } .sy-screw.br { bottom: 15px; right: 15px; }
/* ---- top strip: hardware controls ---- */
.sy-top { position: absolute; left: 40px; right: 40px; top: 20px; height: 128px; display: flex; align-items: flex-start; justify-content: space-between; gap: 18px; }
.sy-tg-l, .sy-tg-r { flex: 1; min-width: 0; display: flex; align-items: flex-start; justify-content: space-between; }
/* page switch (SIREN | AETHER), above the display in the middle of the strip */
.sy-page { position: absolute; z-index: 6; left: 50%; top: 17px; transform: translateX(-50%); display: flex; gap: 2px; padding: 2px; border-radius: 8px; background: #08080a;
  box-shadow: inset 0 1px 3px rgba(0,0,0,.9), inset 0 0 0 1px rgba(255,255,255,.05), 0 1px 0 rgba(255,255,255,.07); }
.sy-page button { width: 84px; height: 19px; border: none; border-radius: 6px; background: none; color: #6f6f76; font: inherit; font-size: 8.5px; font-weight: 600; letter-spacing: .24em; cursor: pointer;
  display: flex; align-items: center; justify-content: center; gap: 7px; padding: 0 0 0 .24em; transition: color .15s, background .15s; }
.sy-page button i { width: 5px; height: 5px; border-radius: 50%; background: #2c2c31; transition: background .2s, box-shadow .2s; }
.sy-page button i.lit { background: var(--accent); box-shadow: 0 0 6px var(--accent); }
.sy-page button:hover { color: #e9e6df; }
.sy-page button.on { color: #f1eee7; background: linear-gradient(#303035, #1d1d21); box-shadow: inset 0 1px 0 rgba(255,255,255,.12), 0 1px 2px rgba(0,0,0,.6); }
.sy-dev.page-aether .sy-siren, .sy-dev:not(.page-aether) .sy-aether { display: none !important; }
.sy-hw { display: flex; flex-direction: column; align-items: center; gap: 7px; min-width: 70px; }
.sy-eng { font-size: 9.5px; font-weight: 600; letter-spacing: .22em; color: #cfccc5; text-shadow: 0 1px 0 rgba(0,0,0,.9); white-space: nowrap; }
.sy-val { font-size: 11px; color: #e9e6df; font-variant-numeric: tabular-nums; white-space: nowrap; min-height: 14px; }
.sy-mk { position: relative; width: 66px; height: 66px; cursor: ns-resize; touch-action: none; }
.sy-mk svg { position: absolute; inset: 0; overflow: visible; }
.sy-mk svg line { stroke: rgba(233,230,223,.26); stroke-width: 1.6; stroke-linecap: round; }
.sy-mk svg line.on { stroke: var(--accent); filter: drop-shadow(0 0 2px var(--accent)); }
.sy-mk .knurl { position: absolute; inset: 9px; border-radius: 50%; background: repeating-conic-gradient(#3c3c42 0deg 4deg, #121214 4deg 8deg); box-shadow: 0 6px 10px rgba(0,0,0,.85), 0 2px 3px rgba(0,0,0,.9); }
.sy-mk .cap { position: absolute; inset: 14px; border-radius: 50%; background: radial-gradient(circle at 36% 26%, #55555c 0%, #1d1d21 46%, #0b0b0c 100%);
  box-shadow: 0 0 0 2px #8f8e89, 0 0 0 3px #232326, inset 0 1px 1px rgba(255,255,255,.3), inset 0 -4px 8px rgba(0,0,0,.7); }
.sy-mk .ptr { position: absolute; inset: 0; }
.sy-mk .ptr::after { content: ''; position: absolute; left: 50%; top: 3px; width: 3px; height: 12px; margin-left: -1.5px; border-radius: 2px; background: var(--accent); box-shadow: 0 0 6px var(--accent); }
.sy-mk:hover .cap { box-shadow: 0 0 0 2px #b0afaa, 0 0 0 3px #232326, inset 0 1px 1px rgba(255,255,255,.3), inset 0 -4px 8px rgba(0,0,0,.7); }
/* selector (chicken-head) knob with its positions around it */
.sy-sel { position: relative; width: 150px; height: 104px; }
.sy-sel .dial { position: absolute; left: 50%; top: 40px; width: 48px; height: 48px; margin-left: -24px; border-radius: 50%; cursor: ns-resize; touch-action: none;
  background: radial-gradient(circle at 38% 30%, #4a4a50, #18181b 60%, #0c0c0d); box-shadow: 0 0 0 2px #8f8e89, 0 0 0 3px #232326, 0 6px 10px rgba(0,0,0,.8); }
.sy-sel .dial svg { position: absolute; inset: -10px; width: 68px; height: 68px; overflow: visible; transition: transform .12s; }
.sy-sel .opt { position: absolute; font-size: 8.5px; font-weight: 600; letter-spacing: .1em; color: #6f6f76; cursor: pointer; transform: translate(-50%, -50%); white-space: nowrap; padding: 2px 3px; }
.sy-sel .opt:hover { color: #e9e6df; }
.sy-sel .opt.on { color: var(--accent); text-shadow: 0 0 6px var(--accent); }
/* bat-lever toggle switch */
.sy-pb { position: relative; width: 54px; height: 54px; margin-top: 4px; border: none; padding: 0; border-radius: 50%; cursor: pointer; display: flex; align-items: center; justify-content: center;
  background: radial-gradient(circle, #050506 58%, #1d1d20 61%, #57565a 66%, #1b1b1e 71%, #2c2c30 100%);
  box-shadow: 0 1px 0 rgba(255,255,255,.1), 0 3px 6px rgba(0,0,0,.7), inset 0 2px 3px rgba(0,0,0,.8); transition: box-shadow .2s; }
.sy-pb .cap { width: 38px; height: 38px; border-radius: 50%; display: flex; align-items: center; justify-content: center; color: #b9b6ae; transition: transform .08s, box-shadow .15s, color .2s;
  background: radial-gradient(circle at 38% 26%, #6b6a6f 0%, #2c2c30 46%, #141416 100%);
  box-shadow: 0 5px 7px rgba(0,0,0,.85), 0 1px 1px rgba(0,0,0,.9), inset 0 1.5px 1px rgba(255,255,255,.4), inset 0 -4px 6px rgba(0,0,0,.55), inset 0 0 0 1px rgba(0,0,0,.5); }
.sy-pb .cap svg { width: 17px; height: 17px; filter: drop-shadow(0 1px 0 rgba(0,0,0,.8)); }
.sy-pb:hover .cap { color: #f1eee7; }
.sy-pb:active .cap { transform: translateY(2px) scale(.97); box-shadow: 0 2px 3px rgba(0,0,0,.85), inset 0 1.5px 1px rgba(255,255,255,.3), inset 0 -2px 4px rgba(0,0,0,.6); }
.sy-pb.on { box-shadow: 0 1px 0 rgba(255,255,255,.1), 0 3px 6px rgba(0,0,0,.7), inset 0 2px 3px rgba(0,0,0,.8), 0 0 18px color-mix(in srgb, var(--accent) 45%, transparent);
  background: radial-gradient(circle, #050506 58%, var(--accent) 61%, color-mix(in srgb, var(--accent) 55%, #000) 66%, #1b1b1e 71%, #2c2c30 100%); }
.sy-pb.on .cap { color: var(--accent); transform: translateY(1px); box-shadow: 0 3px 5px rgba(0,0,0,.85), inset 0 1.5px 1px rgba(255,255,255,.4), inset 0 -4px 6px rgba(0,0,0,.55), inset 0 0 12px color-mix(in srgb, var(--accent) 30%, transparent); }
.sy-pb.on .cap svg { filter: drop-shadow(0 0 4px var(--accent)); }
.sy-pbv { font-size: 9px; font-weight: 600; letter-spacing: .18em; color: #6f6f76; }
.sy-pbv.on { color: var(--accent); text-shadow: 0 0 8px color-mix(in srgb, var(--accent) 60%, transparent); }
/* two latching buttons (AMP: AFTER / BEFORE) */
.sy-pair { display: flex; flex-direction: column; gap: 6px; margin-top: 4px; }
.sy-key { position: relative; width: 78px; height: 24px; border: none; padding: 0 0 0 20px; border-radius: 6px; cursor: pointer; font: inherit; font-size: 8.5px; font-weight: 600; letter-spacing: .16em; color: #8d8d93; text-align: left;
  background: linear-gradient(#3a3a3f, #1f1f23 55%, #18181b); box-shadow: 0 3px 5px rgba(0,0,0,.75), 0 1px 0 rgba(0,0,0,.9), inset 0 1px 0 rgba(255,255,255,.22), inset 0 -2px 3px rgba(0,0,0,.4), 0 0 0 1px #0a0a0b; transition: transform .08s, box-shadow .12s, color .15s; }
.sy-key::before { content: ''; position: absolute; left: 8px; top: 9px; width: 6px; height: 6px; border-radius: 50%; background: #2a1e1e; box-shadow: inset 0 1px 1px rgba(0,0,0,.8); }
.sy-key:hover { color: #e9e6df; }
.sy-key:active { transform: translateY(1px); }
.sy-key.on { color: #f1eee7; transform: translateY(1px); background: linear-gradient(#2e2e33, #1a1a1d 60%, #141416); box-shadow: 0 1px 2px rgba(0,0,0,.8), inset 0 1px 0 rgba(255,255,255,.14), inset 0 2px 4px rgba(0,0,0,.5), 0 0 0 1px #0a0a0b; }
.sy-key.on::before { background: radial-gradient(circle at 40% 35%, #fff, var(--accent) 60%); box-shadow: 0 0 8px var(--accent), 0 0 2px var(--accent); }
/* LED display */
.sy-led { position: relative; width: 176px; height: 92px; margin-top: 26px; border-radius: 10px; cursor: pointer; box-sizing: border-box; padding: 8px 12px;
  background: linear-gradient(#060607, #0b0b0d); box-shadow: inset 0 2px 8px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.04), 0 1px 0 rgba(255,255,255,.08);
  display: flex; flex-direction: column; align-items: center; justify-content: center; }
.sy-led .n { font-family: 'Orbitron', 'Saira', sans-serif; font-size: 30px; font-weight: 500; color: var(--accent); text-shadow: 0 0 12px var(--accent); line-height: 1; letter-spacing: .04em; }
.sy-led .n small { font-size: 14px; margin-left: 2px; opacity: .8; }
.sy-led .s { margin-top: 6px; font-family: 'Orbitron', 'Saira', sans-serif; font-size: 8.5px; letter-spacing: .16em; color: var(--accent); opacity: .65; white-space: nowrap; }
.sy-led .k { position: absolute; left: 10px; top: 7px; font-size: 7.5px; letter-spacing: .14em; color: #6f6f76; }
.sy-led .tn { position: absolute; right: 10px; top: 7px; font-size: 7.5px; letter-spacing: .1em; color: #6f6f76; }
.sy-led .bar { position: absolute; left: 12px; right: 12px; bottom: 7px; height: 2px; border-radius: 1px; background: #19191c; overflow: hidden; }
.sy-led .bar i { position: absolute; left: 0; top: 0; bottom: 0; width: 0; background: var(--accent); box-shadow: 0 0 6px var(--accent); transition: width .08s; }
.sy-led:hover { box-shadow: inset 0 2px 8px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.12), 0 1px 0 rgba(255,255,255,.08); }
.sy-pop { position: absolute; z-index: 20; top: 128px; left: 50%; transform: translateX(-50%); display: none; flex-direction: column; gap: 12px; padding: 16px 18px; border-radius: 14px;
  background: linear-gradient(#1e1e22, #141416); box-shadow: inset 0 1px 0 rgba(255,255,255,.08), inset 0 0 0 1px rgba(255,255,255,.06), 0 20px 40px rgba(0,0,0,.7); }
.sy-pop.show { display: flex; }
.sy-pop h4 { margin: 0; font-size: 10px; font-weight: 600; letter-spacing: .22em; color: #e9e6df; }
/* ---- the screen: four sections ---- */
.sy-screen { position: absolute; left: 22px; right: 22px; top: 158px; height: 316px; border-radius: 16px; display: flex; gap: 8px; box-sizing: border-box; padding: 8px;
  background: radial-gradient(120% 90% at 50% 0%, #121215, #08080a 70%); box-shadow: inset 0 3px 12px rgba(0,0,0,.9), inset 0 0 0 1px rgba(255,255,255,.05), 0 1px 0 rgba(255,255,255,.1); transition: opacity .2s, filter .2s; }
.sy-sec { position: relative; display: flex; flex-direction: column; padding: 10px 14px 10px; box-sizing: border-box; min-width: 0; border-radius: 12px;
  background: linear-gradient(180deg, rgba(255,255,255,.045), rgba(255,255,255,.012) 40%, rgba(0,0,0,.12)), #0d0d10;
  box-shadow: inset 0 1px 0 rgba(255,255,255,.12), inset 0 0 0 1px rgba(255,255,255,.08), inset 0 -1px 0 rgba(0,0,0,.6), 0 3px 8px rgba(0,0,0,.65), 0 0 0 1px rgba(0,0,0,.7); }
.sy-sec::before { content: ''; position: absolute; left: 14px; right: 14px; top: 0; height: 2px; border-radius: 0 0 2px 2px; background: var(--sc); opacity: .85; box-shadow: 0 0 10px var(--sc), 0 0 2px var(--sc); }
.sy-sec::after { content: ''; position: absolute; inset: 0; border-radius: 12px; pointer-events: none; background: radial-gradient(70% 40% at 50% 0%, color-mix(in srgb, var(--sc) 9%, transparent), transparent 75%); }
.sy-sh { display: flex; align-items: center; gap: 8px; height: 24px; margin-bottom: 8px; flex: none; }
.sy-sh h3 { margin: 0; font-size: 10.5px; font-weight: 600; letter-spacing: .22em; color: #e9e6df; white-space: nowrap; display: flex; align-items: center; gap: 8px; }
.sy-sh h3::before { content: ''; width: 6px; height: 6px; border-radius: 50%; background: var(--sc); box-shadow: 0 0 8px var(--sc); }
.sy-sp { flex: 1; }
.sy-off .sy-body { opacity: .35; filter: grayscale(1); }
.sy-body { display: flex; flex-direction: column; flex: 1; min-height: 0; transition: opacity .15s; }
.sy-vis { position: relative; flex: none; border-radius: 9px; overflow: hidden; background: #050506; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05), inset 0 4px 12px rgba(0,0,0,.7); }
.sy-vis canvas { display: block; width: 100%; height: 100%; }
.sy-knobs { display: flex; justify-content: space-around; align-items: flex-start; flex: none; }
.sy-lbl { font-size: 8.5px; font-weight: 600; letter-spacing: .2em; color: #6f6f76; white-space: nowrap; }
.sy-dev.off .sy-screen.sy-siren { opacity: .4; filter: grayscale(.8); }
.sy-dev.off .sy-led .n, .sy-dev.off .sy-led .s { opacity: .15; text-shadow: none; }
/* screen knobs (section colour arcs) */
.sy-ctl { display: flex; flex-direction: column; align-items: center; min-width: 0; }
.sy-kn { position: relative; flex: none; cursor: ns-resize; touch-action: none; border-radius: 50%; }
.sy-kn > i { position: absolute; border-radius: 50%; }
.sy-kn .a { inset: 0; -webkit-mask: radial-gradient(closest-side, transparent calc(100% - 3.2px), #000 calc(100% - 2.6px), #000 calc(100% - .5px), transparent 100%); mask: radial-gradient(closest-side, transparent calc(100% - 3.2px), #000 calc(100% - 2.6px), #000 calc(100% - .5px), transparent 100%); }
.sy-kn .c { inset: 16%; background: repeating-conic-gradient(#2e2e33 0deg 3deg, #151517 3deg 6deg); box-shadow: 0 4px 8px rgba(0,0,0,.8), 0 1px 2px rgba(0,0,0,.9); }
.sy-kn .c > i { position: absolute; inset: 6%; border-radius: 50%; background: radial-gradient(circle at 38% 22%, rgba(255,255,255,.1), transparent 45%), radial-gradient(circle at 50% 15%, #47474d 0%, #212124 55%, #131315 100%); box-shadow: inset 0 1px 0 rgba(255,255,255,.2), inset 0 -3px 6px rgba(0,0,0,.6); }
.sy-kn .c > i > b { position: absolute; inset: 0; }
.sy-kn .c > i > b::after { content: ''; position: absolute; left: 50%; top: 7%; width: 2.4px; height: 30%; margin-left: -1.2px; border-radius: 2px; background: #f4f1ea; }
.sy-kn:hover .c { box-shadow: 0 4px 8px rgba(0,0,0,.8), 0 0 0 1px rgba(255,255,255,.07); }
.sy-kl { font-size: 8.5px; font-weight: 600; letter-spacing: .16em; color: #a3a3a8; margin-top: 5px; white-space: nowrap; }
.sy-kv { font-size: 10.5px; color: #e9e6df; font-variant-numeric: tabular-nums; white-space: nowrap; margin-top: 2px; }
/* small switches */
.sy-tg { position: relative; width: 44px; height: 22px; flex: none; border: none; padding: 0 0 0 14px; border-radius: 6px; cursor: pointer; font: inherit; font-size: 8px; font-weight: 600; letter-spacing: .16em; color: #7a7a80;
  background: linear-gradient(#36363b, #1d1d21 55%, #161619); box-shadow: 0 3px 4px rgba(0,0,0,.75), inset 0 1px 0 rgba(255,255,255,.2), inset 0 -2px 3px rgba(0,0,0,.4), 0 0 0 1px #08080a; transition: transform .08s, box-shadow .12s, color .15s; }
.sy-tg::after { content: 'ON'; }
.sy-tg i { position: absolute; left: 7px; top: 8px; width: 6px; height: 6px; border-radius: 50%; background: #26262a; box-shadow: inset 0 1px 1px rgba(0,0,0,.8); transition: background .15s, box-shadow .15s; }
.sy-tg:hover { color: #e9e6df; }
.sy-tg:active { transform: translateY(1px); }
.sy-tg.on { color: #f1eee7; transform: translateY(1px); background: linear-gradient(#2c2c31, #19191c 60%, #131315);
  box-shadow: 0 1px 2px rgba(0,0,0,.8), inset 0 1px 0 rgba(255,255,255,.12), inset 0 2px 4px rgba(0,0,0,.5), 0 0 0 1px color-mix(in srgb, var(--sc, var(--accent)) 50%, #08080a), 0 0 10px color-mix(in srgb, var(--sc, var(--accent)) 25%, transparent); }
.sy-tg.on i { background: radial-gradient(circle at 40% 35%, #fff, var(--sc, var(--accent)) 60%); box-shadow: 0 0 7px var(--sc, var(--accent)); }
.sy-seg { display: flex; flex: none; gap: 2px; padding: 3px; border-radius: 8px; background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05); }
.sy-seg button { flex: 1; min-width: 0; height: 22px; padding: 0 4px; border: none; border-radius: 6px; background: none; color: #6f6f76; font: inherit; font-size: 8.5px; font-weight: 600; letter-spacing: .06em;
  cursor: pointer; display: flex; align-items: center; justify-content: center; white-space: nowrap; transition: background .12s, color .12s; }
.sy-seg button:hover { color: #e9e6df; }
.sy-seg button.on { color: #e9e6df; background: linear-gradient(#34343a, #25252a); box-shadow: inset 0 1px 0 rgba(255,255,255,.1), 0 1px 2px rgba(0,0,0,.5), inset 0 -2px 0 var(--sc, var(--accent)); }
.sy-seg.small button { height: 19px; font-size: 8px; }
.sy-seg.tight button { padding: 0 1px; letter-spacing: 0; }
.sy-chip { height: 20px; padding: 0 8px; flex: none; border-radius: 6px; border: 1px solid rgba(255,255,255,.1); background: none; color: #8d8d93; font: inherit; font-size: 8px; font-weight: 600; letter-spacing: .14em; cursor: pointer; white-space: nowrap; display: flex; align-items: center; gap: 6px; }
.sy-chip:hover { color: #e9e6df; border-color: rgba(255,255,255,.18); }
.sy-chip.on { background: var(--sc, var(--accent)); border-color: transparent; color: #111113; }
.sy-pill { height: 22px; flex: none; display: flex; align-items: center; gap: 7px; padding: 0 9px; border-radius: 7px; background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.06); cursor: ns-resize; touch-action: none; white-space: nowrap; }
.sy-pill:hover { box-shadow: inset 0 0 0 1px rgba(255,255,255,.14); }
.sy-pill .l { font-size: 8px; font-weight: 600; letter-spacing: .16em; color: #6f6f76; }
.sy-pill .v { font-size: 10px; color: #e9e6df; font-variant-numeric: tabular-nums; }
.sy-pill.cyc { cursor: pointer; }
.sy-tabs { display: flex; gap: 3px; }
.sy-tab { height: 22px; padding: 0 9px; border: none; border-radius: 6px; background: none; color: #6f6f76; font: inherit; font-size: 9px; font-weight: 600; letter-spacing: .16em; cursor: pointer; display: flex; align-items: center; gap: 6px; white-space: nowrap; }
.sy-tab:hover { color: #e9e6df; }
.sy-tab.on { color: #e9e6df; background: #1b1b1f; box-shadow: inset 0 0 0 1px rgba(255,255,255,.07), inset 0 -2px 0 var(--tc); }
.sy-tab i { width: 6px; height: 6px; border-radius: 50%; background: #3a3a40; }
.sy-tab i.lit { background: var(--tc); box-shadow: 0 0 6px var(--tc); }
/* arp lane */
.sy-lane { display: grid; grid-template-columns: repeat(16, 1fr); gap: 3px; height: 62px; flex: none; touch-action: none; }
.sy-st { position: relative; display: flex; flex-direction: column; gap: 3px; min-width: 0; cursor: pointer; }
.sy-st .cell { position: relative; flex: 1; border-radius: 4px; background: #17171a; box-shadow: inset 0 0 0 1px rgba(255,255,255,.04); overflow: hidden; }
.sy-st .cell > i { position: absolute; left: 0; right: 0; bottom: 0; border-radius: 3px; background: linear-gradient(var(--h-mot), #3f7fb8); }
.sy-st .cell > i.dim { background: linear-gradient(#4a4a50, #333338); }
.sy-st .cell > b { position: absolute; left: 0; right: 0; top: 3px; text-align: center; font-size: 7.5px; font-weight: 600; color: #08080a; pointer-events: none; }
.sy-st .cell > b.light { color: #a3a3a8; }
.sy-st .cell.tie::before { content: ''; position: absolute; left: -5px; top: 50%; width: 8px; height: 3px; margin-top: -1.5px; background: var(--accent); border-radius: 2px; z-index: 1; }
.sy-st .num { height: 9px; font-size: 7px; color: #55555b; text-align: center; }
.sy-st.out { opacity: .25; }
.sy-st.play .cell { box-shadow: inset 0 0 0 1px #fff, 0 0 10px var(--h-mot); }
.sy-st.beat .num { color: #a3a3a8; }
/* bottom band: presets, name badge, reset, dice */
.sy-bot { position: absolute; left: 40px; right: 40px; bottom: 22px; height: 70px; display: flex; align-items: center; justify-content: space-between; }
.sy-badge { position: absolute; left: 50%; top: 4px; transform: translateX(-50%); display: flex; flex-direction: column; align-items: center; pointer-events: none; }
.sy-badge b { font-family: 'Cinzel', 'Saira', serif; font-size: 30px; font-weight: 700; letter-spacing: .32em; margin-right: -.32em; line-height: 1; color: transparent;
  background: linear-gradient(180deg, #f4f1ea 0%, #a7a49c 55%, #e9e6df 100%); -webkit-background-clip: text; background-clip: text; filter: drop-shadow(0 2px 0 rgba(0,0,0,.9)); }
.sy-badge span { margin-top: 6px; font-size: 8.5px; font-weight: 600; letter-spacing: .5em; margin-right: -.5em; color: #8d8d93; }
.sy-badge .pn { --pc: var(--accent); display: flex; align-items: center; gap: 8px; margin-top: 7px; max-width: 380px; font-size: 11px; font-weight: 600; letter-spacing: .26em; margin-right: -.26em;
  color: #f1eee7; text-transform: uppercase; white-space: nowrap; }
.sy-badge .pn i { flex: none; width: 7px; height: 7px; border-radius: 50%; background: var(--pc); box-shadow: 0 0 8px var(--pc); }
.sy-badge .pn u { overflow: hidden; text-overflow: ellipsis; text-decoration: none; }
.sy-badge .pn em { flex: none; font-style: normal; font-size: 8px; letter-spacing: .2em; color: var(--pc); opacity: .85; }
.sy-wave { position: absolute; top: 18px; width: 92px; height: 24px; opacity: .45; color: var(--accent); }
.sy-wave.l { right: calc(50% + 136px); } .sy-wave.r { left: calc(50% + 136px); transform: scaleX(-1); }
.sy-prwrap { display: flex; align-items: center; gap: 12px; } .sy-prwrap .sy-kbbtn { margin-left: 10px; }
.sy-prbtn .tx small { max-width: 150px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: var(--accent); opacity: .95; font-size: 11px; font-weight: 600; letter-spacing: .04em; }
.sy-prnav { display: flex; gap: 4px; }
.sy-prnav button { width: 26px; height: 26px; border-radius: 50%; border: 1px solid rgba(255,255,255,.12); background: none; color: #8d8d93; cursor: pointer; display: flex; align-items: center; justify-content: center; padding: 0; }
.sy-prnav button:hover { color: #e9e6df; border-color: rgba(255,255,255,.24); }
.sy-prnav svg { width: 12px; height: 12px; }
.sy-prbtn.on .round { color: var(--accent); box-shadow: 0 0 0 2px var(--accent), 0 0 0 3px #232326, 0 0 18px var(--accent), inset 0 1px 1px rgba(255,255,255,.25); }
/* preset browser: lies over the screen */
.sy-prs { position: absolute; z-index: 15; left: 22px; right: 22px; top: 158px; height: 316px; border-radius: 16px; display: none; flex-direction: column; box-sizing: border-box; padding: 12px 14px 12px;
  background: radial-gradient(120% 90% at 50% 0%, #141418, #09090b 70%); box-shadow: inset 0 0 0 1px rgba(255,255,255,.07), inset 0 1px 0 rgba(255,255,255,.1), 0 18px 40px rgba(0,0,0,.75); }
.sy-prs.show { display: flex; }
.sy-prs-h { display: flex; align-items: center; gap: 14px; height: 26px; flex: none; margin-bottom: 10px; }
.sy-prs-h b { font-size: 10.5px; font-weight: 600; letter-spacing: .24em; color: #e9e6df; }
.sy-prs-h span { font-size: 9.5px; color: #6f6f76; letter-spacing: .04em; }
.sy-prs-h .x { margin-left: auto; width: 26px; height: 26px; border-radius: 8px; border: 1px solid rgba(255,255,255,.1); background: none; color: #a3a3a8; cursor: pointer; font: inherit; font-size: 13px; padding: 0; }
.sy-prs-h .x:hover { color: #e9e6df; border-color: rgba(255,255,255,.22); }
.sy-prs-b { display: flex; gap: 12px; flex: 1; min-height: 0; }
.sy-prs-cats { width: 138px; flex: none; display: flex; flex-direction: column; gap: 3px; }
.sy-prs-cats button { height: 25px; border: none; border-radius: 7px; background: none; color: #8d8d93; font: inherit; font-size: 9px; font-weight: 600; letter-spacing: .16em; cursor: pointer; display: flex; align-items: center; gap: 8px; padding: 0 9px; text-align: left; white-space: nowrap; }
.sy-prs-cats button i { width: 6px; height: 6px; border-radius: 50%; flex: none; background: var(--cc); box-shadow: 0 0 6px var(--cc); }
.sy-prs-cats button em { margin-left: auto; font-style: normal; font-size: 8.5px; color: #55555b; letter-spacing: 0; }
.sy-prs-cats button:hover { color: #e9e6df; }
.sy-prs-cats button.on { color: #e9e6df; background: #1b1b1f; box-shadow: inset 0 0 0 1px rgba(255,255,255,.07), inset 2px 0 0 var(--cc); }
.sy-prs-list { flex: 1; min-width: 0; display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); grid-auto-rows: 50px; gap: 6px; overflow-y: auto; padding-right: 4px; align-content: start; }
.sy-prs-list::-webkit-scrollbar { width: 6px; } .sy-prs-list::-webkit-scrollbar-thumb { background: #2a2a2f; border-radius: 3px; }
.sy-pr { position: relative; border: none; border-radius: 9px; padding: 7px 10px 0 12px; text-align: left; cursor: pointer; font: inherit; color: #e9e6df; min-width: 0; overflow: hidden;
  background: linear-gradient(180deg, rgba(255,255,255,.05), rgba(255,255,255,.015)), #0f0f12; box-shadow: inset 0 0 0 1px rgba(255,255,255,.06), inset 0 1px 0 rgba(255,255,255,.07); display: flex; flex-direction: column; gap: 3px; }
.sy-pr::before { content: ''; position: absolute; left: 0; top: 8px; bottom: 8px; width: 2px; border-radius: 0 2px 2px 0; background: var(--cc); opacity: .75; }
.sy-pr b { font-size: 11px; font-weight: 600; letter-spacing: .04em; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.sy-pr small { font-size: 9px; color: #7d7d84; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.sy-pr:hover { box-shadow: inset 0 0 0 1px rgba(255,255,255,.16), inset 0 1px 0 rgba(255,255,255,.07); }
.sy-pr.on { box-shadow: inset 0 0 0 1px var(--cc), 0 0 14px color-mix(in srgb, var(--cc) 30%, transparent); }
.sy-pr.on b { color: var(--cc); }
/* HORROR: blood red into black */
.sy-prs-cats button.hor i { background: radial-gradient(circle at 35% 35%, #ff4a4f, #8a0b12 55%, #000 100%); box-shadow: 0 0 7px #a3121a; }
.sy-prs-cats button.hor.on { background: linear-gradient(90deg, #3d0609, #150405 70%, #0d0d0f); box-shadow: inset 0 0 0 1px rgba(170,22,30,.45), inset 2px 0 0 #c41a24; }
.sy-pr.hor { background: linear-gradient(135deg, rgba(150,12,22,.42), rgba(45,4,8,.6) 45%, #070708 88%); box-shadow: inset 0 0 0 1px rgba(150,20,30,.38), inset 0 1px 0 rgba(255,90,90,.08); }
.sy-pr.hor::before { background: linear-gradient(#ff3a43, #3a0206); opacity: 1; }
.sy-pr.hor b { color: #f4e2e2; } .sy-pr.hor small { color: #a8817f; }
.sy-pr.hor:hover { box-shadow: inset 0 0 0 1px rgba(225,40,50,.65); }
.sy-pr.hor.on { box-shadow: inset 0 0 0 1px #d01c26, 0 0 18px rgba(175,15,25,.6); } .sy-pr.hor.on b { color: #ff5a61; }
.sy-badge .pn.hor i { background: radial-gradient(circle at 35% 35%, #ff4a4f, #8a0b12 60%, #000); box-shadow: 0 0 9px #c41a24; }
.sy-badge .pn.hor em { color: #e0303a; }
/* KEYS: a keyboard over the screen */
.sy-keys { position: absolute; z-index: 15; left: 22px; right: 22px; top: 158px; height: 316px; border-radius: 16px; display: none; flex-direction: column; box-sizing: border-box; padding: 10px 14px 14px;
  background: radial-gradient(120% 90% at 50% 0%, #141418, #09090b 70%); box-shadow: inset 0 0 0 1px rgba(255,255,255,.07), inset 0 1px 0 rgba(255,255,255,.1), 0 18px 40px rgba(0,0,0,.75); }
.sy-keys.show { display: flex; }
.sy-keys-h { display: flex; align-items: center; gap: 12px; height: 28px; flex: none; margin-bottom: 10px; }
.sy-keys-h b { font-size: 10.5px; font-weight: 600; letter-spacing: .24em; color: #e9e6df; }
.sy-keys-h > span { font-size: 9.5px; color: #6f6f76; letter-spacing: .04em; white-space: nowrap; }
.sy-keys-h .sp { flex: 1; }
.sy-keys-h .sy-seg { width: auto; } .sy-keys-h .sy-seg button { padding: 0 9px; }
.sy-keys-h .x { width: 26px; height: 26px; border-radius: 8px; border: 1px solid rgba(255,255,255,.1); background: none; color: #a3a3a8; cursor: pointer; font: inherit; font-size: 13px; padding: 0; flex: none; }
.sy-keys-h .x:hover { color: #e9e6df; border-color: rgba(255,255,255,.22); }
.sy-keys-oct { display: flex; align-items: center; height: 25px; border-radius: 8px; background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05); flex: none; }
.sy-keys-oct button { width: 24px; height: 25px; border: none; background: none; color: #a3a3a8; font: inherit; font-size: 15px; cursor: pointer; padding: 0; }
.sy-keys-oct button:hover { color: var(--accent); }
.sy-keys-oct span { min-width: 64px; text-align: center; font-size: 9.5px; font-weight: 600; letter-spacing: .08em; color: #e9e6df; font-variant-numeric: tabular-nums; }
.sy-keys-b { position: relative; flex: 1; min-height: 0; touch-action: none; user-select: none; }
.sy-pk { position: absolute; top: 0; box-sizing: border-box; cursor: pointer; display: flex; flex-direction: column; align-items: center; justify-content: flex-end; gap: 4px; padding-bottom: 10px; }
.sy-pk.w { bottom: 0; border-radius: 0 0 8px 8px; z-index: 1; background: linear-gradient(#d9d6cf, #f1eee7 18%, #e6e3dc 92%, #bdbab3); box-shadow: inset -1px 0 0 rgba(0,0,0,.25), inset 0 -4px 0 rgba(0,0,0,.18), 0 2px 3px rgba(0,0,0,.6); }
.sy-pk.b { height: 60%; border-radius: 0 0 6px 6px; z-index: 2; background: linear-gradient(#2a2a2e, #141416 80%, #0b0b0c); box-shadow: inset 0 -5px 0 rgba(255,255,255,.06), inset 1px 0 0 rgba(255,255,255,.08), 0 4px 6px rgba(0,0,0,.8); padding-bottom: 8px; }
.sy-pk em { font-style: normal; font-size: 8.5px; font-weight: 600; letter-spacing: .06em; color: #8d8a83; }
.sy-pk.b em { color: #6f6f76; }
.sy-pk u { text-decoration: none; font-size: 9px; font-weight: 600; letter-spacing: .06em; color: #55534e; }
.sy-pk.root u { color: #111; } .sy-pk.w.root::after { content: ''; position: absolute; top: 8px; width: 7px; height: 7px; border-radius: 50%; background: var(--accent); box-shadow: 0 0 6px var(--accent); }
.sy-pk.ink.w::before { content: ''; position: absolute; top: 20px; width: 4px; height: 4px; border-radius: 50%; background: #8d8a83; }
.sy-pk.w.off { background: linear-gradient(#8f8c86, #a19e97 18%, #97948d 92%, #77746e); }
.sy-pk.b.off { background: linear-gradient(#1a1a1c, #0c0c0d); }
.sy-pk.w.lit { background: linear-gradient(#bff3ec, #6fe0d3 70%, #3fd0c0); box-shadow: inset 0 -3px 0 rgba(0,0,0,.2), 0 0 22px rgba(63,208,192,.55); transform: translateY(1px); }
.sy-pk.b.lit { background: linear-gradient(#1d6f66, #3fd0c0); box-shadow: 0 0 16px rgba(63,208,192,.8); }
.sy-pk.lit u, .sy-pk.lit em { color: #06302b; }
.sy-keys-b.pads { display: grid; grid-template-columns: repeat(8, minmax(0, 1fr)); grid-template-rows: repeat(2, minmax(0, 1fr)); gap: 8px; }
.sy-pad { border: none; border-radius: 12px; cursor: pointer; font: inherit; color: #e9e6df; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 6px;
  background: linear-gradient(180deg, rgba(255,255,255,.06), rgba(255,255,255,.015)), #121215; box-shadow: inset 0 0 0 1px rgba(255,255,255,.08), inset 0 1px 0 rgba(255,255,255,.1), 0 4px 10px rgba(0,0,0,.6); transition: transform .05s; }
.sy-pad b { font-size: 16px; font-weight: 600; letter-spacing: .04em; } .sy-pad small { font-size: 8px; letter-spacing: .18em; color: #6f6f76; }
.sy-pad.root { box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--accent) 55%, transparent), inset 0 1px 0 rgba(255,255,255,.1), 0 4px 10px rgba(0,0,0,.6); } .sy-pad.root small { color: var(--accent); }
.sy-pad:hover { box-shadow: inset 0 0 0 1px rgba(255,255,255,.2), 0 4px 10px rgba(0,0,0,.6); }
.sy-pad.lit { background: linear-gradient(rgba(63,208,192,.5), rgba(63,208,192,.18)); box-shadow: inset 0 0 0 1px #3fd0c0, 0 0 24px rgba(63,208,192,.45); transform: translateY(1px); }
.sy-pad em { font-style: normal; font-size: 8.5px; font-weight: 600; color: #55555b; letter-spacing: .1em; }
.sy-kbbtn.on .round { color: var(--accent); box-shadow: 0 0 0 2px var(--accent), 0 0 0 3px #232326, 0 0 18px var(--accent), inset 0 1px 1px rgba(255,255,255,.25); }
.sy-btn { display: flex; align-items: center; gap: 12px; cursor: pointer; border: none; background: none; padding: 0; font: inherit; color: inherit; }
.sy-btn .round { width: 46px; height: 46px; border-radius: 50%; display: flex; align-items: center; justify-content: center; color: #e9e6df;
  background: radial-gradient(circle at 38% 30%, #4a4a50, #1a1a1d 62%, #0c0c0d); box-shadow: 0 0 0 2px #8f8e89, 0 0 0 3px #232326, 0 6px 12px rgba(0,0,0,.8), inset 0 1px 1px rgba(255,255,255,.25); transition: box-shadow .15s, color .15s; }
.sy-btn .round svg { width: 15px; height: 15px; }
.sy-btn:hover .round { color: var(--accent); }
.sy-btn.on .round { color: var(--accent); box-shadow: 0 0 0 2px var(--accent), 0 0 0 3px #232326, 0 0 18px var(--accent), inset 0 1px 1px rgba(255,255,255,.25); }
.sy-btn .tx { display: flex; flex-direction: column; gap: 3px; text-align: left; }
.sy-btn .tx b { font-size: 10px; font-weight: 600; letter-spacing: .22em; color: #e9e6df; }
.sy-btn .tx small { font-size: 9px; color: #6f6f76; letter-spacing: .04em; }
.sy-btn.r { flex-direction: row-reverse; } .sy-btn.r .tx { text-align: right; }
.sy-undo { width: 26px; height: 26px; border-radius: 50%; border: 1px solid rgba(255,255,255,.12); background: none; color: #8d8d93; cursor: pointer; display: flex; align-items: center; justify-content: center; padding: 0; margin-right: -8px; }
.sy-undo:disabled { opacity: .25; cursor: default; }
.sy-undo svg { width: 12px; height: 12px; }
.sy-hint { font-size: 9px; color: #6f6f76; letter-spacing: .04em; }
.sy-flash { animation: syFlash .5s ease-out; }
@keyframes syFlash { 0% { box-shadow: inset 0 3px 12px rgba(0,0,0,.9), inset 0 0 0 1px var(--accent), 0 0 26px rgba(255,255,255,.1); } 100% { box-shadow: inset 0 3px 12px rgba(0,0,0,.9), inset 0 0 0 1px rgba(255,255,255,.05), 0 1px 0 rgba(255,255,255,.1); } }
`;
document.head.appendChild(css);

/* ------------------------------------------------------------ icons */
const IC = {
  saw: '<svg width="20" height="12" viewBox="0 0 22 14"><path d="M1 12 L9 2 V12 L17 2 V12 L21 7" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/></svg>',
  pulse: '<svg width="20" height="12" viewBox="0 0 22 14"><path d="M1 12 H4 V2 H9 V12 H13 V2 H18 V12 H21" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/></svg>',
  tri: '<svg width="20" height="12" viewBox="0 0 22 14"><path d="M1 12 L6 2 L11 12 L16 2 L21 12" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/></svg>',
  sine: '<svg width="20" height="12" viewBox="0 0 22 14"><path d="M1 7 C3.5 -1 6.5 -1 9 7 S14.5 15 17 7 S20 3 21 4" fill="none" stroke="currentColor" stroke-width="1.6"/></svg>',
  super: '<svg width="20" height="12" viewBox="0 0 22 14"><path d="M1 12 L9 2 V12 L17 2 V12" fill="none" stroke="currentColor" stroke-width="1.3" opacity=".5"/><path d="M4 12 L12 2 V12 L20 2 V12" fill="none" stroke="currentColor" stroke-width="1.6"/></svg>',
  sq: '<svg width="20" height="12" viewBox="0 0 22 14"><path d="M1 12 V2 H11 V12 H21 V2" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/></svg>',
  rnd: '<svg width="20" height="12" viewBox="0 0 22 14"><path d="M1 9 H5 V3 H9 V11 H13 V5 H17 V8 H21" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"/></svg>',
  drift: '<svg width="20" height="12" viewBox="0 0 22 14"><path d="M1 9 C4 9 5 3 8 4 S12 11 15 9 S19 4 21 5" fill="none" stroke="currentColor" stroke-width="1.6"/></svg>',
  up: '<svg width="14" height="14" viewBox="0 0 24 24"><path d="M12 19V5M6 11l6-6 6 6" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  down: '<svg width="14" height="14" viewBox="0 0 24 24"><path d="M12 5v14M6 13l6 6 6-6" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  updn: '<svg width="14" height="14" viewBox="0 0 24 24"><path d="M8 19V5M4 9l4-4 4 4M16 5v14M12 15l4 4 4-4" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  shuffle: '<svg width="14" height="14" viewBox="0 0 24 24"><path d="M3 7h3c4 0 6 10 10 10h4M17 14l3 3-3 3M3 17h3c1.6 0 2.8-1.6 3.8-3.4M13.2 9.4C14.2 7.6 15.4 7 17 7h3M17 4l3 3-3 3" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  steps: '<svg width="14" height="14" viewBox="0 0 24 24"><rect x="3" y="13" width="4" height="7" rx="1" fill="currentColor"/><rect x="10" y="7" width="4" height="13" rx="1" fill="currentColor"/><rect x="17" y="10" width="4" height="10" rx="1" fill="currentColor"/></svg>',
  dice: '<svg width="13" height="13" viewBox="0 0 24 24"><rect x="3.5" y="3.5" width="17" height="17" rx="4" fill="none" stroke="currentColor" stroke-width="2"/><circle cx="8.5" cy="8.5" r="1.6" fill="currentColor"/><circle cx="15.5" cy="15.5" r="1.6" fill="currentColor"/><circle cx="12" cy="12" r="1.6" fill="currentColor"/><circle cx="15.5" cy="8.5" r="1.6" fill="currentColor"/><circle cx="8.5" cy="15.5" r="1.6" fill="currentColor"/></svg>',
  undo: '<svg width="13" height="13" viewBox="0 0 24 24"><path d="M9 14L4 9l5-5M4 9h10a6 6 0 010 12h-3" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  list: '<svg viewBox="0 0 24 24"><path d="M4 6h11M4 12h11M4 18h7" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"/><circle cx="19" cy="17" r="2.6" fill="none" stroke="currentColor" stroke-width="2"/><path d="M21.6 17V6.5L18 5.5" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  reset: '<svg viewBox="0 0 24 24"><path d="M4.5 12a7.5 7.5 0 1 0 2.2-5.3M4.5 4.5v4.2h4.2" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  prev: '<svg viewBox="0 0 24 24"><path d="M15 6l-6 6 6 6" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  next: '<svg viewBox="0 0 24 24"><path d="M9 6l6 6-6 6" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/></svg>',
};
const WAVE_IC = ['saw', 'pulse', 'tri', 'sine', 'super'];

/* ------------------------------------------------------------ controls */
const ARC0 = 225;                     // knob arc: 270 degrees starting bottom-left
function knob(id, o = {}) {
  const size = o.size || 38, p = P[id];
  const c = el('div', 'sy-ctl');
  const k = el('div', 'sy-kn', '<i class="a"></i><i class="c"><i><b></b></i></i>');
  k.style.width = k.style.height = size + 'px'; k.dataset.param = id;
  c.appendChild(k);
  if (o.label !== false) { c.appendChild(el('span', 'sy-kl', o.label || '')); c.appendChild(el('span', 'sy-kv')); }
  const a = k.querySelector('.a'), r = k.querySelector('b'), kv = c.querySelector('.sy-kv');
  const bip = p.min < 0 && p.max > 0 && p.type !== 'int';
  const draw = () => {
    const t = toN(id), deg = 270 * t;
    if (bip) { const z = 270 * toN(id, 0), lo = Math.min(deg, z), hi = Math.max(deg, z);
      a.style.background = 'conic-gradient(from 225deg, #2b2b30 0deg ' + lo + 'deg, var(--sc, var(--accent)) ' + lo + 'deg ' + hi + 'deg, #2b2b30 ' + hi + 'deg 270deg, transparent 270deg 360deg)'; }
    else a.style.background = 'conic-gradient(from 225deg, var(--sc, var(--accent)) 0deg ' + deg + 'deg, #2b2b30 ' + deg + 'deg 270deg, transparent 270deg 360deg)';
    r.style.transform = 'rotate(' + (deg - 135) + 'deg)';
    if (kv) kv.textContent = (o.fmt || fmt)(id, S[id]);
  };
  draw(); onChange(i => { if (i === id) draw(); });
  let drag = null;
  k.addEventListener('pointerdown', e => { if (e.button !== 0) return; e.preventDefault(); k.setPointerCapture(e.pointerId); drag = { y: e.clientY, t: toN(id) }; k.classList.add('drag'); });
  k.addEventListener('pointermove', e => { if (!drag) return; const sc = (document.getElementById('app') && document.getElementById('app').getBoundingClientRect().width / 1376) || 1;
    set(id, toV(id, drag.t + (drag.y - e.clientY) / sc / (e.shiftKey ? 900 : 190)), 'ui'); });
  const end = () => { drag = null; k.classList.remove('drag'); };
  k.addEventListener('pointerup', end); k.addEventListener('pointercancel', end);
  k.addEventListener('dblclick', () => set(id, p.def, 'ui'));
  k.addEventListener('wheel', e => { e.preventDefault(); const st = p.type === 'int' || p.type === 'choice' ? 1 / Math.max(1, p.max - p.min) : (e.shiftKey ? .005 : .02); set(id, toV(id, toN(id) + (e.deltaY < 0 ? st : -st)), 'ui'); }, { passive: false });
  if (o.tip) k.title = o.tip;
  return c;
}
function toggle(id, tip) {
  const b = el('button', 'sy-tg', '<i></i>'); b.dataset.param = id; if (tip) b.title = tip; b.setAttribute('aria-label', tip || id);
  const draw = () => b.classList.toggle('on', on(id));
  b.addEventListener('click', () => set(id, on(id) ? 0 : 1, 'ui'));
  draw(); onChange(i => { if (i === id) draw(); }); return b;
}
function chip(id, label, tip) {
  const b = el('button', 'sy-chip', label); b.dataset.param = id; if (tip) b.title = tip;
  const draw = () => b.classList.toggle('on', on(id));
  b.addEventListener('click', () => set(id, on(id) ? 0 : 1, 'ui'));
  draw(); onChange(i => { if (i === id) draw(); }); return b;
}
function seg(id, labels, o = {}) {
  const s = el('div', 'sy-seg' + (o.small ? ' small' : '')); s.dataset.param = id;
  const opts = P[id].opts;
  s.innerHTML = (labels || opts).map((l, i) => '<button data-i="' + i + '" title="' + opts[i] + '">' + (o.icons ? IC[o.icons[i]] : l) + '</button>').join('');
  const draw = () => [...s.children].forEach((b, i) => b.classList.toggle('on', i === Math.round(S[id])));
  s.addEventListener('click', e => { const b = e.target.closest('button'); if (b) set(id, +b.dataset.i, 'ui'); });
  draw(); onChange(i => { if (i === id) draw(); }); if (o.width) s.style.width = o.width + 'px'; return s;
}
function pill(id, label, o = {}) {
  const p = P[id], b = el('div', 'sy-pill' + (p.opts ? ' cyc' : ''), '<span class="l">' + label + '</span><span class="v"></span>');
  b.dataset.param = id; if (o.tip) b.title = o.tip;
  const v = b.querySelector('.v'); const draw = () => { v.textContent = (o.fmt || fmt)(id); };
  draw(); onChange(i => { if (i === id) draw(); });
  let drag = null, moved = false;
  b.addEventListener('pointerdown', e => { if (e.button !== 0) return; e.preventDefault(); b.setPointerCapture(e.pointerId); drag = { y: e.clientY, t: toN(id) }; moved = false; });
  b.addEventListener('pointermove', e => { if (!drag) return; const dy = drag.y - e.clientY; if (Math.abs(dy) > 2) moved = true; if (moved) set(id, toV(id, drag.t + dy / (e.shiftKey ? 900 : 160)), 'ui'); });
  b.addEventListener('pointerup', e => { if (drag && !moved && p.opts) { const n = p.opts.length; set(id, (Math.round(S[id]) + (e.shiftKey ? n - 1 : 1)) % n, 'ui'); } drag = null; });
  b.addEventListener('pointercancel', () => { drag = null; });
  b.addEventListener('dblclick', () => { if (!p.opts) set(id, p.def, 'ui'); });
  b.addEventListener('wheel', e => { e.preventDefault(); const st = p.type === 'int' || p.type === 'choice' ? 1 / Math.max(1, p.max - p.min) : .02; set(id, toV(id, toN(id) + (e.deltaY < 0 ? st : -st)), 'ui'); }, { passive: false });
  return b;
}
function segBool(id, labels, tip, width) {      // two-way switch for an on/off setting, with words
  const s = el('div', 'sy-seg small'); s.dataset.param = id; if (tip) s.title = tip; if (width) s.style.width = width + 'px';
  s.innerHTML = labels.map((l, i) => '<button data-i="' + i + '">' + l + '</button>').join('');
  const draw = () => [...s.children].forEach((b, i) => b.classList.toggle('on', i === (on(id) ? 1 : 0)));
  s.addEventListener('click', e => { const b = e.target.closest('button'); if (b) set(id, +b.dataset.i, 'ui'); });
  draw(); onChange(i => { if (i === id) draw(); }); return s;
}
function stepper(id, label, tip) {
  const b = el('div', 'sy-stepper', '<button aria-label="Previous">‹</button><span><b>' + label + '</b><em></em></span><button aria-label="Next">›</button>');
  b.dataset.param = id; if (tip) b.title = tip;
  const n = P[id].opts.length, t = b.querySelector('em'); t.style.fontStyle = 'normal';
  const draw = () => { t.textContent = fmt(id).toUpperCase(); };
  b.children[0].onclick = () => set(id, (Math.round(S[id]) + n - 1) % n, 'ui');
  b.children[2].onclick = () => set(id, (Math.round(S[id]) + 1) % n, 'ui');
  b.addEventListener('wheel', e => { e.preventDefault(); set(id, clamp(Math.round(S[id]) + (e.deltaY < 0 ? 1 : -1), 0, n - 1), 'ui'); }, { passive: false });
  draw(); onChange(i => { if (i === id) draw(); }); return b;
}
/* big machined knob for the faceplate: tick ring lights up to the value */
function mknob(id, label, o = {}) {
  const p = P[id], w = el('div', 'sy-hw');
  w.appendChild(el('span', 'sy-eng', label));
  const k = el('div', 'sy-mk', '<svg viewBox="0 0 66 66"></svg><i class="knurl"></i><i class="cap"><b class="ptr"></b></i>');
  k.dataset.param = id; if (o.tip) k.title = o.tip;
  const svg = k.querySelector('svg'), N = 25;
  for (let i = 0; i < N; i++) { const a = (135 + 270 * i / (N - 1)) * Math.PI / 180, big = i % 6 === 0;
    const r0 = big ? 27 : 28.5, r1 = 32; const ln = document.createElementNS('http://www.w3.org/2000/svg', 'line');
    ln.setAttribute('x1', 33 + r0 * Math.cos(a)); ln.setAttribute('y1', 33 + r0 * Math.sin(a)); ln.setAttribute('x2', 33 + r1 * Math.cos(a)); ln.setAttribute('y2', 33 + r1 * Math.sin(a)); svg.appendChild(ln); }
  const ticks = [...svg.children], ptr = k.querySelector('.ptr'), val = el('span', 'sy-val');
  const bip = p.min < 0 && p.max > 0;
  const draw = () => { const t = toN(id), z = bip ? toN(id, 0) : 0;
    ticks.forEach((ln, i) => { const f = i / (N - 1); ln.classList.toggle('on', bip ? (f >= Math.min(t, z) - .001 && f <= Math.max(t, z) + .001) : f <= t + .001); });
    ptr.style.transform = 'rotate(' + (270 * t - 135) + 'deg)'; val.textContent = (o.fmt || fmt)(id, S[id]); };
  draw(); onChange(i => { if (i === id) draw(); });
  let drag = null;
  k.addEventListener('pointerdown', e => { if (e.button !== 0) return; e.preventDefault(); k.setPointerCapture(e.pointerId); drag = { y: e.clientY, t: toN(id) }; });
  k.addEventListener('pointermove', e => { if (!drag) return; const sc = (document.getElementById('app') && document.getElementById('app').getBoundingClientRect().width / 1376) || 1;
    set(id, toV(id, drag.t + (drag.y - e.clientY) / sc / (e.shiftKey ? 900 : 200)), 'ui'); });
  k.addEventListener('pointerup', () => { drag = null; }); k.addEventListener('pointercancel', () => { drag = null; });
  k.addEventListener('dblclick', () => set(id, p.def, 'ui'));
  k.addEventListener('wheel', e => { e.preventDefault(); set(id, toV(id, toN(id) + (e.deltaY < 0 ? 1 : -1) * (e.shiftKey ? .005 : .02)), 'ui'); }, { passive: false });
  w.append(k, val); return w;
}
/* chunky selector knob with its positions printed around it */
function sel(id, label, names, o = {}) {
  const n = names.length, span = o.span || (n > 5 ? 240 : 150), w = el('div', 'sy-hw');
  w.appendChild(el('span', 'sy-eng', label));
  const s = el('div', 'sy-sel'); s.dataset.param = id; if (o.tip) s.title = o.tip;
  const dial = el('div', 'dial', '<svg viewBox="-34 -34 68 68"><path d="M0 -31 C5 -31 7 -20 8 -8 C9 4 6 12 0 12 C-6 12 -9 4 -8 -8 C-7 -20 -5 -31 0 -31 Z" fill="url(#sySelG)" stroke="#8f8e89" stroke-width="1"/><line x1="0" y1="-28" x2="0" y2="-16" stroke="var(--accent)" stroke-width="2.4" stroke-linecap="round"/><circle r="5" fill="#0c0c0d" stroke="#3a3a40"/></svg>');
  s.appendChild(dial);
  const angleOf = i => -span / 2 + span * i / Math.max(1, n - 1);
  const opts = names.map((t, i) => { const a = angleOf(i) * Math.PI / 180, b = el('span', 'opt', t);
    b.style.left = (75 + Math.sin(a) * 56) + 'px'; b.style.top = (64 - Math.cos(a) * 42) + 'px'; b.onclick = () => set(id, i, 'ui'); s.appendChild(b); return b; });
  const svg = dial.querySelector('svg');
  const draw = () => { const i = Math.round(S[id]); svg.style.transform = 'rotate(' + angleOf(i) + 'deg)'; opts.forEach((b, k) => b.classList.toggle('on', k === i)); };
  draw(); onChange(i => { if (i === id) draw(); });
  let drag = null;
  dial.addEventListener('pointerdown', e => { if (e.button !== 0) return; e.preventDefault(); dial.setPointerCapture(e.pointerId); drag = { y: e.clientY, v: Math.round(S[id]) }; });
  dial.addEventListener('pointermove', e => { if (!drag) return; set(id, clamp(drag.v + Math.round((drag.y - e.clientY) / 22), 0, n - 1), 'ui'); });
  dial.addEventListener('pointerup', e => { if (drag && Math.abs(drag.y - e.clientY) < 3) set(id, (Math.round(S[id]) + 1) % n, 'ui'); drag = null; });
  dial.addEventListener('wheel', e => { e.preventDefault(); set(id, clamp(Math.round(S[id]) + (e.deltaY < 0 ? 1 : -1), 0, n - 1), 'ui'); }, { passive: false });
  w.appendChild(s); return w;
}
/* round latching push button with a lit ring (POWER, HOLD) */
function pushBtn(get, setter, label, icon, onText, offText, tip) {
  const w = el('div', 'sy-hw'); w.appendChild(el('span', 'sy-eng', label));
  const b = el('button', 'sy-pb', '<span class="cap">' + icon + '</span>'); if (tip) b.title = tip; b.setAttribute('aria-label', label);
  const v = el('span', 'sy-pbv');
  const draw = () => { const o = !!get(); b.classList.toggle('on', o); v.classList.toggle('on', o); v.textContent = o ? onText : offText; };
  b.addEventListener('click', () => { setter(!get()); draw(); });
  draw(); w.append(b, v); w._draw = draw; w._btn = b; return w;
}
/* two latching buttons, one lit (AMP: AFTER / BEFORE) */
function pairBtn(id, label, opts, tip) {
  const w = el('div', 'sy-hw'); w.appendChild(el('span', 'sy-eng', label));
  const pr = el('div', 'sy-pair'); if (tip) pr.title = tip;
  const bs = opts.map(([t, val]) => { const b = el('button', 'sy-key', t); b.dataset.param = id; b.onclick = () => set(id, val, 'ui'); pr.appendChild(b); return b; });
  const draw = () => bs.forEach((b, i) => b.classList.toggle('on', Math.round(S[id]) === opts[i][1]));
  draw(); onChange(i => { if (i === id) draw(); });
  w.appendChild(pr); w._draw = draw; return w;
}
const spacer = () => el('span', 'sy-sp');
const gapV = h => { const d = el('div'); d.style.height = h + 'px'; d.style.flex = 'none'; return d; };
function row(children, style) { const r = el('div'); r.style.cssText = 'display: flex; align-items: center; gap: 8px; flex: none;' + (style || ''); children.forEach(c => c && r.appendChild(c)); return r; }

/* ------------------------------------------------------------ wave shapes (same maths as the engine) */
const frac = x => x - Math.floor(x);
function waveAt(w, ph, shape) {                 // ph 0..1, shape 0..100, naive (for drawing and wavetables)
  const s = shape / 100;
  switch (w) {
    case 0: { const a = 1 - 2 * ph, b = 1 - 2 * frac(ph + .5 * s); return (1 - .5 * s) * a + .5 * s * b; }      // saw (+ offset saw)
    case 1: { const pw = .5 - .45 * s; return ph < pw ? 1 : -1; }                                            // pulse width
    case 2: { const t = ph < .5 ? 4 * ph - 1 : 3 - 4 * ph; return Math.sin(Math.PI / 2 * t * (1 + 3 * s)); }  // triangle with folding
    case 3: { let y = 0; for (let i = 0; i < 3; i++) y = Math.sin(2 * Math.PI * ph + s * 2.2 * y); return y; } // sine with feedback FM
    default: { let v = 0; for (let i = -3; i <= 3; i++) v += 1 - 2 * frac(ph * (1 + i * .011 * (.3 + s)) + i * .137); return v / 3.4; } // super
  }
}

/* ------------------------------------------------------------ chords and scales (same as the engine) */
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const SCALES = [[0, 2, 4, 5, 7, 9, 11], [0, 2, 3, 5, 7, 8, 10], [0, 2, 3, 5, 7, 9, 10], [0, 1, 3, 5, 7, 8, 10], [0, 2, 3, 5, 7, 8, 11], [0, 3, 5, 7, 10], [0, 3, 5, 6, 7, 10]];
function scaleTone(rootNote, degree, sc) {      // the scale note 'degree' steps above rootNote (rootNote snapped into the key)
  sc = sc || SCALES[Math.round(S['syn.scale'])];
  const key = Math.round(S['syn.key']), rel = ((rootNote - key) % 12 + 12) % 12;
  let d0 = 0; for (let i = 0; i < sc.length; i++) if (sc[i] <= rel) d0 = i;
  const off = rel - sc[d0];                       // played note is off-scale by this much: keep it, shift everything
  const i = d0 + degree, o = Math.floor(i / sc.length);
  return rootNote - rel + sc[((i % sc.length) + sc.length) % sc.length] + 12 * o + off;
}
function chordNotes(rootNote) {
  const c = Math.round(S['syn.chord']), sc0 = SCALES[Math.round(S['syn.scale'])], sc = sc0.length < 7 ? SCALES[1] : sc0;
  const t = d => scaleTone(rootNote, d, sc);
  switch (c) {
    case 1: return [rootNote, rootNote + 12];
    case 2: return [rootNote, rootNote + 7, rootNote + 12];
    case 3: return [t(0), t(2), t(4)];
    case 4: return [rootNote, rootNote + 5, rootNote + 7];
    case 5: return [t(0), t(2), t(4), t(6)];
    case 6: return [t(0), t(2), t(4), t(8)];
    default: return [rootNote];
  }
}

/* ============================================================ the device */
SP.replaceChildren();
SP.style.fontFamily = 'inherit';
const rootEl = el('div', 'sy-root'), dev = el('div', 'sy-dev'), panel = dev;
rootEl.appendChild(dev); SP.appendChild(rootEl);
['tl', 'tr', 'bl', 'br'].forEach(c => dev.appendChild(el('i', 'sy-screw ' + c)));
dev.insertAdjacentHTML('beforeend', '<svg width="0" height="0" style="position:absolute"><defs><radialGradient id="sySelG" cx="35%" cy="30%" r="80%"><stop offset="0" stop-color="#5a5a62"/><stop offset=".6" stop-color="#1c1c20"/><stop offset="1" stop-color="#0c0c0d"/></radialGradient></defs></svg>');

/* ---- top strip: what you reach for while playing ---- */
const topStrip = el('div', 'sy-top sy-siren'); dev.appendChild(topStrip);
let powered = true;
const PWR_IC = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round"><path d="M12 3v8"/><path d="M6.3 6.8a8 8 0 1 0 11.4 0"/></svg>';
const HOLD_IC = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="M9 7c-3.3 0-5 2.2-5 5s1.7 5 5 5c4 0 6-10 10-10 2.6 0 4 2.2 4 5s-1.4 5-4 5c-4 0-6-10-10-10z" transform="translate(-1 0)"/></svg>';
const powerW = pushBtn(() => powered, v => { setPowerUi(v); if (window.obsSynth && window.obsSynth.powerOut) window.obsSynth.powerOut(v); }, 'POWER', PWR_IC, 'ON', 'OFF', 'Synth on / off (also: double-click the SYNTH tab)');
function setPowerUi(v) { powered = !!v; dev.classList.toggle('off', !powered); powerW._draw(); if (pageDots) pageDots(); }
let pageDots = null;
const led = el('div', 'sy-led', '<span class="k"></span><span class="tn"></span><div class="n">–</div><div class="s">PLAY A NOTE</div><div class="bar"><i></i></div>');
led.title = 'Click for key, scale, tuning and playing settings';
const noteScr = led;
const drawLedInfo = () => { led.querySelector('.k').textContent = fmt('syn.key').toUpperCase() + ' ' + fmt('syn.scale').toUpperCase(); led.querySelector('.tn').textContent = 'A' + Math.round(S['syn.tune']); };
drawLedInfo(); onChange(i => { if (/^syn\.(key|scale|tune)$/.test(i)) drawLedInfo(); });
const ledW = el('div', 'sy-hw'); ledW.appendChild(led);
/* the strip: a group left and right of the display, which sits in the middle (under the page switch) */
function stripGroups(strip, left, mid, right) {
  const gl = el('div', 'sy-tg-l'), gr = el('div', 'sy-tg-r'); gl.append(...left); gr.append(...right); strip.append(gl, mid, gr); }
const holdW = pushBtn(() => on('syn.hold'), v => set('syn.hold', v ? 1 : 0, 'ui'), 'HOLD', HOLD_IC, 'ON', 'OFF', 'The last note keeps playing after you stop (a drone to play over)');
const postW = pairBtn('syn.post', 'AMP', [['AFTER', 1], ['BEFORE', 0]], 'AFTER: the synth joins after the cab · BEFORE: it plays through the amp');
stripGroups(topStrip, [powerW,
  mknob('syn.sens', 'SENSITIVITY', { tip: 'How strongly the synth reacts to your playing' }),
  mknob('syn.glide', 'GLIDE', { tip: 'Slide time between notes' }),
  sel('syn.chord', 'CHORD', ['OFF', 'OCT', 'PWR', 'TRIAD', 'SUS', '7TH', '9TH'], { tip: 'Play one note, hear a chord. Triad, 7th and 9th follow the key and scale.' }),
  holdW], ledW, [postW,
  sel('syn.char', 'CHARACTER', ['CLEAN', 'ANALOG', '8-BIT', 'VHS'], { span: 180, tip: 'Clean, warm analog drift, chiptune, or wobbly tape' }),
  mknob('syn.space', 'SPACE', { tip: 'Stereo echoes and room, just for the synth' }),
  mknob('syn.mix', 'MIX', { tip: 'Guitar and synth blend: 0% guitar only, 100% synth only' }),
  mknob('syn.level', 'OUTPUT')]);
onChange(i => { if (i === 'syn.hold') holdW._draw(); if (i === 'syn.post') postW._draw(); });
/* playing settings, behind the display */
const pop = el('div', 'sy-pop sy-siren');
pop.innerHTML = '<h4>PLAYING</h4>';
const popRow1 = row([pill('syn.key', 'KEY'), pill('syn.scale', 'SCALE'), pill('syn.tune', 'TUNING', { fmt: () => 'A ' + Math.round(S['syn.tune']) + ' Hz' })]);
const popRow2 = el('div', 'sy-knobs'); popRow2.style.gap = '18px';
popRow2.append(knob('syn.gate', { label: 'THRESHOLD', size: 36, tip: 'How loud the guitar must be before the synth plays' }), knob('syn.dyn', { label: 'DYNAMICS', size: 36, tip: 'How much picking harder makes it louder and brighter' }));
const popRow3 = row([chip('syn.legato', 'LEGATO', 'New notes glide without restarting the envelopes'), el('span', 'sy-hint', 'Notes glide into each other')]);
pop.append(popRow1, popRow2, popRow3);
dev.appendChild(pop);
led.addEventListener('click', e => { e.stopPropagation(); pop.classList.toggle('show'); });
document.addEventListener('pointerdown', e => { if (pop.classList.contains('show') && !pop.contains(e.target) && !led.contains(e.target)) pop.classList.remove('show'); });

/* ---- the screen ---- */
const screen = el('div', 'sy-screen sy-siren'); dev.appendChild(screen);
function section(title, width, color, into = screen) {
  const s = el('section', 'sy-sec'); s.style.width = width + 'px'; s.style.setProperty('--sc', color);
  const h = el('div', 'sy-sh'); h.appendChild(el('h3', null, title)); s.appendChild(h);
  const b = el('div', 'sy-body'); s.appendChild(b); s._h = h; s._b = b; into.appendChild(s); return s;
}
function dimWhen(sec, id) { const d = () => sec.classList.toggle('sy-off', !on(id)); d(); onChange(i => { if (i === id) d(); }); }

/* OSCILLATORS: A / B / SUB in one section */
const cSnd = section('', 378, 'var(--h-osc)');
cSnd._h.replaceChildren();
let tab = 0;
const tabs = el('div', 'sy-tabs');
const TAB_NAMES = ['OSC A', 'OSC B', 'SUB · NOISE'], TAB_COL = ['var(--h-osc)', 'var(--h-oscb)', 'var(--h-sub)'];
TAB_NAMES.forEach((n, i) => { const b = el('button', 'sy-tab', '<i></i>' + n); b.style.setProperty('--tc', TAB_COL[i]); b.onclick = () => showTab(i); tabs.appendChild(b); });
const tabRight = el('div'); tabRight.style.cssText = 'display: flex; align-items: center; gap: 8px';
cSnd._h.append(tabs, el('span', 'sy-sp'), tabRight);
const sndBody = cSnd._b;
const oscViews = ['a', 'b'].map((x, oi) => {
  const v = el('div'); v.style.cssText = 'display: flex; flex-direction: column; flex: 1';
  const sc = el('div', 'sy-vis'); sc.style.height = '54px'; const cv = el('canvas'); sc.appendChild(cv); v._cv = cv;
  v.append(seg('syn.' + x + '.wave', null, { icons: WAVE_IC }), gapV(8), sc, gapV(10));
  const k = el('div', 'sy-knobs');
  k.append(knob('syn.' + x + '.pitch', { label: 'PITCH', tip: 'Pitch in semitones (12 = one octave)' }), knob('syn.' + x + '.fine', { label: 'FINE', tip: 'Fine tune in cents' }),
    knob('syn.' + x + '.shape', { label: 'SHAPE', tip: 'Saw: a second saw for a hollow, brighter tone · Pulse: width · Triangle: wavefolding · Sine: FM growl · Super: side voices' }),
    knob('syn.' + x + '.level', { label: 'LEVEL' }));
  v.append(k, gapV(8));
  const uni = el('div', 'sy-knobs'); uni.style.alignItems = 'center';
  const useg = seg('syn.' + x + '.voices', null, { small: true, width: 118 });
  const ucol = el('div'); ucol.style.cssText = 'display: flex; flex-direction: column; align-items: center; gap: 6px'; ucol.append(useg, el('span', 'sy-kl', 'UNISON VOICES'));
  uni.append(ucol, knob('syn.' + x + '.detune', { label: 'DETUNE', size: 32, tip: 'How far the unison voices spread apart' }), knob('syn.' + x + '.width', { label: 'WIDTH', size: 32, tip: 'Stereo spread of the unison voices' }));
  v.appendChild(uni);
  const supr = () => { const isSuper = Math.round(S['syn.' + x + '.wave']) === 4; useg.style.opacity = isSuper ? .35 : ''; useg.title = isSuper ? 'Super always uses 7 voices' : ''; };
  supr(); onChange(i => { if (i === 'syn.' + x + '.wave') supr(); });
  return v;
});
const subView = (() => {
  const v = el('div'); v.style.cssText = 'display: flex; flex-direction: column; flex: 1';
  const sc = el('div', 'sy-vis'); sc.style.height = '54px'; const cv = el('canvas'); sc.appendChild(cv); v._cv = cv;
  const cols = el('div'); cols.style.cssText = 'display: flex; gap: 12px; flex: 1; margin-top: 12px';
  const mk = (title, kids) => { const c = el('div'); c.style.cssText = 'flex: 1; display: flex; flex-direction: column; align-items: center; gap: 12px; padding: 12px 8px; border-radius: 10px; background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.04)';
    c.appendChild(el('span', 'sy-lbl', title)); kids.forEach(k => c.appendChild(k)); return c; };
  cols.append(
    mk('SUB OSCILLATOR', [knob('syn.sub.level', { label: 'LEVEL', tip: 'A clean sub one or two octaves below' }), row([seg('syn.sub.wave', ['SINE', 'SQUARE'], { small: true, width: 92 }), seg('syn.sub.oct', [MINUS + '1 OCT', MINUS + '2 OCT'], { small: true, width: 92 })])]),
    mk('NOISE', [row([knob('syn.noise.level', { label: 'LEVEL', tip: 'Breath and grit' }), knob('syn.noise.color', { label: 'COLOR' })], 'gap: 20px')]));
  v.append(sc, cols);
  return v;
})();
const bSync = chip('syn.b.sync', 'SYNC', 'Hard sync: osc B restarts with osc A. Sweep B\'s pitch for the classic screaming lead.');
const aOn = toggle('syn.a.on', 'Oscillator A on'), bOn = toggle('syn.b.on', 'Oscillator B on');
function showTab(i) {
  tab = i;
  [...tabs.children].forEach((b, k) => b.classList.toggle('on', k === i));
  cSnd.style.setProperty('--sc', TAB_COL[i]);
  sndBody.replaceChildren([oscViews[0], oscViews[1], subView][i]);
  tabRight.replaceChildren(...(i === 0 ? [aOn] : i === 1 ? [bSync, bOn] : []));
  cSnd.classList.toggle('sy-off', (i === 0 && !on('syn.a.on')) || (i === 1 && !on('syn.b.on')));
  drawScopes();
}
const tabDots = () => { const d = [...tabs.children].map(b => b.firstChild);
  d[0].classList.toggle('lit', on('syn.a.on') && S['syn.a.level'] > 0); d[1].classList.toggle('lit', on('syn.b.on') && S['syn.b.level'] > 0);
  d[2].classList.toggle('lit', S['syn.sub.level'] > 0 || S['syn.noise.level'] > 0);
  cSnd.classList.toggle('sy-off', (tab === 0 && !on('syn.a.on')) || (tab === 1 && !on('syn.b.on'))); };
onChange(i => { if (/^syn\.(a|b)\.(on|level)$|^syn\.(sub|noise)\.level$/.test(i)) tabDots(); });

/* FILTER */
const cFlt = section('FILTER', 282, 'var(--h-flt)');
cFlt._h.append(el('span', 'sy-sp'), toggle('syn.flt.on', 'Filter on'));
dimWhen(cFlt, 'syn.flt.on');
const fScr = el('div', 'sy-vis'); fScr.style.height = '52px'; fScr.style.cursor = 'crosshair'; const fCv = el('canvas'); fScr.appendChild(fCv);
fScr.title = 'Drag: left/right = cutoff, up/down = resonance'; fScr.dataset.param = 'syn.flt.cutoff';
const fType = seg('syn.flt.type', ['LDR24', 'LDR12', 'LP', 'BP', 'HP', 'NOTCH', 'VOWEL']); fType.classList.add('tight');
cFlt._b.append(fType, gapV(8), fScr, gapV(8),
  (() => { const k = el('div', 'sy-knobs'); k.style.alignItems = 'flex-end';
    k.append(knob('syn.flt.cutoff', { label: 'CUTOFF', size: 42, tip: 'Brightness. With VOWEL it moves through A-E-I-O-U' }), knob('syn.flt.res', { label: 'RESONANCE' })); return k; })(),
  gapV(6),
  (() => { const k = el('div', 'sy-knobs'); k.append(knob('syn.flt.drive', { label: 'DRIVE', size: 32 }), knob('syn.flt.env', { label: 'ENV AMOUNT', size: 32, tip: 'How far the filter envelope opens (or closes) the filter' }), knob('syn.flt.key', { label: 'KEY TRACK', size: 32, tip: 'Higher notes open the filter more' })); return k; })());
{ let d = null;
  const at = e => { const r = fScr.getBoundingClientRect(); return [clamp((e.clientX - r.left) / r.width, 0, 1), clamp(1 - (e.clientY - r.top) / r.height, 0, 1)]; };
  fScr.addEventListener('pointerdown', e => { if (e.button !== 0) return; e.preventDefault(); fScr.setPointerCapture(e.pointerId); d = 1; const [x, y] = at(e); set('syn.flt.cutoff', toV('syn.flt.cutoff', x), 'ui'); set('syn.flt.res', y * 100, 'ui'); });
  fScr.addEventListener('pointermove', e => { if (!d) return; const [x, y] = at(e); set('syn.flt.cutoff', toV('syn.flt.cutoff', x), 'ui'); set('syn.flt.res', clamp((y - .1) / .8, 0, 1) * 100, 'ui'); });
  fScr.addEventListener('pointerup', () => { d = null; }); }

/* ENVELOPE: one graph, AMP or FILTER */
const cEnv = section('ENVELOPE', 240, 'var(--h-env)');
let envSel = 0;
const envSeg = el('div', 'sy-seg small'); envSeg.style.width = '104px';
envSeg.innerHTML = '<button class="on">AMP</button><button>FILTER</button>';
cEnv._h.append(el('span', 'sy-sp'), envSeg);
const eScr = el('div', 'sy-vis'); eScr.style.height = '104px'; const eCv = el('canvas'); eScr.appendChild(eCv); eScr.title = 'Drag the points';
const envKnobs = el('div', 'sy-knobs'), envHint = el('div', 'sy-hint'); envHint.style.textAlign = 'center';
cEnv._b.append(eScr, gapV(14), envKnobs, gapV(10), envHint);
const envIds = s => ['att', 'dec', 'sus', 'rel'].map(x => 'syn.env.' + s + '.' + x);
function buildEnvKnobs() {
  const ids = envIds(envSel ? 'f' : 'a');
  envKnobs.replaceChildren(...ids.map((id, i) => knob(id, { label: ['ATTACK', 'DECAY', 'SUSTAIN', 'RELEASE'][i], size: 34 })));
  envHint.textContent = envSel ? 'Shapes the filter over each note' : 'Shapes the volume over each note';
  [...envSeg.children].forEach((b, i) => b.classList.toggle('on', i === envSel));
  drawEnv();
}
envSeg.addEventListener('click', e => { const b = e.target.closest('button'); if (!b) return; envSel = [...envSeg.children].indexOf(b); buildEnvKnobs(); });

/* MOTION: ARP or LFO */
const cMot = section('', 336, 'var(--h-mot)');
cMot._h.replaceChildren();
let mtab = 0;
const mtabs = el('div', 'sy-tabs');
['ARPEGGIATOR', 'LFO'].forEach((n, i) => { const b = el('button', 'sy-tab', '<i></i>' + n); b.style.setProperty('--tc', 'var(--h-mot)'); b.onclick = () => showMot(i); mtabs.appendChild(b); });
const motRight = el('div'); motRight.style.cssText = 'display: flex; align-items: center; gap: 8px';
cMot._h.append(mtabs, el('span', 'sy-sp'), motRight);
/* arp view */
const arpView = el('div'); arpView.style.cssText = 'display: flex; flex-direction: column; flex: 1';
const PROPS = [['STEP', 'on'], ['ACC', 'acc'], ['TIE', 'tie'], ['OCT', 'oct'], ['RATCH', 'ratchet'], ['CHANCE', 'chance'], ['NOTE', 'note']];
let prop = 0;
const propSeg = el('div', 'sy-seg small');
propSeg.innerHTML = PROPS.map(([n], i) => '<button' + (i ? '' : ' class="on"') + '>' + n + '</button>').join('');
const PROP_TIPS = ['Click or drag across to turn steps on and off', 'Accented steps hit harder and brighter', 'A tied step slides on from the one before', 'Drag up or down: octave for each step', 'Drag up: repeat the note 2, 3 or 4 times in one step', 'Drag: how likely each step is to play', 'Drag: which scale note each step plays (STEPS pattern)'];
propSeg.addEventListener('click', e => { const b = e.target.closest('button'); if (!b) return; prop = [...propSeg.children].indexOf(b); [...propSeg.children].forEach((x, i) => x.classList.toggle('on', i === prop)); drawLane(); lane.title = PROP_TIPS[prop]; });
const lane = el('div', 'sy-lane');
lane.innerHTML = steps.map((_, i) => '<div class="sy-st' + (i % 4 === 0 ? ' beat' : '') + '" data-i="' + i + '"><div class="cell"><i></i><b></b></div><div class="num">' + (i + 1) + '</div></div>').join('');
lane.title = PROP_TIPS[0];
const arpKnobs = el('div', 'sy-knobs');
arpKnobs.append(knob('syn.arp.gate', { label: 'GATE', size: 30 }), knob('syn.arp.swing', { label: 'SWING', size: 30 }), knob('syn.arp.len', { label: 'STEPS', size: 30, fmt: id => String(Math.round(S[id])) }),
  (() => { const c = el('div', 'sy-ctl'); c.append(seg('syn.arp.oct', null, { small: true, width: 76 })); const l = el('span', 'sy-kl', 'OCTAVES'); l.style.marginTop = '8px'; c.append(l); return c; })());
arpView.append(row([seg('syn.arp.pattern', null, { icons: ['up', 'down', 'updn', 'shuffle', 'steps'], width: 112 }), el('span', 'sy-sp'), seg('syn.arp.rate', ['1/4', '1/8', '1/8T', '1/16', '1/16T', '1/32'], { small: true, width: 184 })], 'gap: 4px'),
  gapV(8), propSeg, gapV(6), lane, gapV(10), arpKnobs);
arpView.querySelector('.sy-seg[data-param="syn.arp.rate"]').title = 'Note value of each step';
arpView.querySelector('.sy-seg[data-param="syn.arp.rate"]').classList.add('tight');
/* lfo view */
const lfoView = el('div'); lfoView.style.cssText = 'display: flex; flex-direction: column; flex: 1';
const lScr = el('div', 'sy-vis'); lScr.style.height = '48px'; const lCv = el('canvas'); lScr.appendChild(lCv);
const lRate = knob('syn.lfo.rate', { label: 'RATE', size: 34 }), lDiv = knob('syn.lfo.div', { label: 'RATE', size: 34, tip: 'Rate in note values (synced to the tempo)' });
const lKnobs = el('div', 'sy-knobs');
lfoView.append(seg('syn.lfo.shape', null, { icons: ['sine', 'tri', 'saw', 'sq', 'rnd', 'drift'] }), gapV(8), lScr, gapV(12), lKnobs, gapV(12),
  seg('syn.lfo.target', ['VIBRATO', 'WAH', 'SHAPE', 'TREMOLO', 'PAN'], { small: true }));
const lK = () => lKnobs.replaceChildren(on('syn.lfo.sync') ? lDiv : lRate, knob('syn.lfo.depth', { label: 'DEPTH', size: 34 }), knob('syn.lfo.fade', { label: 'FADE IN', size: 34, tip: 'The LFO fades in after each new note (delayed vibrato)' }));
lK(); onChange(i => { if (i === 'syn.lfo.sync') lK(); });
const arpOnT = toggle('syn.arp.on', 'Arpeggiator on'), lfoSync = chip('syn.lfo.sync', 'SYNC', 'Lock the LFO to the tempo');
function showMot(i) {
  mtab = i; [...mtabs.children].forEach((b, k) => b.classList.toggle('on', k === i));
  cMot._b.replaceChildren(i ? lfoView : arpView); motRight.replaceChildren(i ? lfoSync : arpOnT);
  cMot.classList.toggle('sy-off', !i && !on('syn.arp.on'));
  if (i) drawLfo(); else drawLane();
}
const motDots = () => { const d = [...mtabs.children].map(b => b.firstChild); d[0].classList.toggle('lit', on('syn.arp.on')); d[1].classList.toggle('lit', S['syn.lfo.depth'] > 0);
  cMot.classList.toggle('sy-off', mtab === 0 && !on('syn.arp.on')); };
onChange(i => { if (i === 'syn.arp.on' || i === 'syn.lfo.depth') motDots(); });

function drawLane() {
  const n = Math.round(S['syn.arp.len']);
  [...lane.children].forEach((stEl, i) => {
    const s = steps[i], c = stEl.firstChild, bar = c.firstChild, lab = c.children[1];
    stEl.classList.toggle('out', i >= n);
    c.classList.toggle('tie', !!(s.tie && s.on));
    let h = 0, dim = !s.on, txt = '';
    switch (PROPS[prop][1]) {
      case 'on': h = s.on ? 100 : 0; dim = false; txt = s.acc && s.on ? '•' : ''; break;
      case 'acc': h = s.on ? (s.acc ? 100 : 45) : 0; txt = s.acc && s.on ? '•' : ''; break;
      case 'tie': h = s.on ? (s.tie ? 100 : 45) : 0; txt = s.tie && s.on ? '⌒' : ''; break;
      case 'oct': h = 25 + (s.oct + 1) * 25; txt = s.oct ? sg(s.oct) : '0'; break;
      case 'ratchet': h = s.ratchet * 25; txt = s.ratchet > 1 ? '×' + s.ratchet : '1'; break;
      case 'chance': h = s.chance; txt = s.chance < 100 ? s.chance + '' : ''; break;
      case 'note': h = 12 + s.note / 14 * 88; txt = String(s.note + 1); break;
    }
    bar.style.height = h + '%'; bar.classList.toggle('dim', dim);
    lab.textContent = txt; lab.classList.toggle('light', h < 30 || dim);
  });
}
{ let paint = null;
  const stepAt = e => { const r = lane.getBoundingClientRect(), x = (e.clientX - r.left) / r.width; return clamp(Math.floor(x * 16), 0, 15); };
  const yAt = e => { const r = lane.firstChild.firstChild.getBoundingClientRect(); return clamp(1 - (e.clientY - r.top) / r.height, 0, 1); };
  const apply = (i, e, first) => {
    const s = steps[i], key = PROPS[prop][1];
    if (key === 'on' || key === 'acc' || key === 'tie') {
      if (first) paint = { v: s[key] ? 0 : 1 };
      if (key !== 'on' && !s.on) return;
      s[key] = paint.v;
    } else {
      const y = yAt(e);
      if (key === 'oct') s.oct = clamp(Math.round(y * 3.6 - 1.3), -1, 2);
      if (key === 'ratchet') s.ratchet = clamp(Math.ceil(y * 4), 1, 4);
      if (key === 'chance') s.chance = clamp(Math.round(y * 20) * 5, 0, 100);
      if (key === 'note') s.note = clamp(Math.round((y - .12) / .88 * 14), 0, 14);
    }
    drawLane(); stepsChanged('ui');
  };
  lane.addEventListener('pointerdown', e => { if (e.button !== 0) return; e.preventDefault(); lane.setPointerCapture(e.pointerId); paint = {}; apply(stepAt(e), e, true); });
  lane.addEventListener('pointermove', e => { if (!paint) return; apply(stepAt(e), e, false); });
  lane.addEventListener('pointerup', () => { paint = null; }); lane.addEventListener('pointercancel', () => { paint = null; });
}
onChange(i => { if (i === 'syn.arp.len') drawLane(); });

/* ---- bottom band: presets, name badge, reset and dice ---- */
const bot = el('div', 'sy-bot sy-siren'); dev.appendChild(bot);
const prBtn = el('button', 'sy-btn sy-prbtn', '<span class="round">' + IC.list + '</span><span class="tx"><b>PRESETS</b><small class="sy-prname">Init</small></span>');
prBtn.title = 'Browse the synth sounds';
const prNav = el('div', 'sy-prnav', '<button aria-label="Previous synth preset">' + IC.prev + '</button><button aria-label="Next synth preset">' + IC.next + '</button>');
const kbBtn = el('button', 'sy-btn sy-kbbtn', '<span class="round"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"><rect x="2.5" y="5" width="19" height="14" rx="2"/><path d="M7.2 5v8.5M12 5v8.5M16.8 5v8.5" stroke-width="1.6"/><path d="M7.2 19v-5.5M12 19v-5.5M16.8 19v-5.5" stroke-width="1.2" opacity=".6"/></svg></span><span class="tx"><b>KEYS</b><small>no guitar needed</small></span>');
kbBtn.title = 'Play SIREN on the screen or the computer keys (when your guitar is not around)';
const prWrap = el('div', 'sy-prwrap'); prWrap.append(prBtn, prNav, kbBtn);
const waveSvg = '<svg viewBox="0 0 380 24" preserveAspectRatio="none"><path d="' + Array.from({ length: 77 }, (_, i) => (i ? 'L' : 'M') + (i * 5) + ' ' + (12 + Math.sin(i * .42) * 9 * Math.pow(i / 76, 1.6)).toFixed(1)).join(' ') + '" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>';
const badge = el('div', 'sy-badge', '<b>SIREN</b><div class="pn" title="The loaded synth preset"><i></i><u>INIT</u><em></em></div>');
const wl = el('div', 'sy-wave l', waveSvg), wr = el('div', 'sy-wave r', waveSvg);
const diceWrap = el('div'); diceWrap.style.cssText = 'display: flex; align-items: center; gap: 22px';
const undo = el('button', 'sy-undo', IC.undo); undo.title = 'Undo (dice, reset or preset)'; undo.disabled = true;
const reset = el('button', 'sy-btn r sy-reset', '<span class="round">' + IC.reset + '</span><span class="tx"><b>RESET</b><small>back to default</small></span>');
reset.title = 'Every synth setting back to its default (your key and tuning stay)';
const dice = el('button', 'sy-btn r', '<span class="round">' + IC.dice + '</span><span class="tx"><b>DICE</b><small>roll a new sound</small></span>');
dice.title = 'Roll a new sound. Keeps your mix, output, key and playing settings.';
diceWrap.append(undo, reset, dice);
bot.append(prWrap, wl, badge, wr, diceWrap);

/* ------------------------------------------------------------ drawings */
function hue(name, a = 1) {
  let c = getComputedStyle(dev).getPropertyValue('--h-' + name).trim() || '#ffffff';
  return 'rgba(' + [1, 3, 5].map(i => parseInt(c.substr(i, 2), 16)).join(',') + ',' + a + ')';
}
function drawScope(cv, which) {
  const [g, w, h] = hidpi(cv); if (!g) return;
  const H = which === 'b' ? 'oscb' : which === 'sub' ? 'sub' : 'osc';
  g.strokeStyle = tc('e9e6df', .06); g.lineWidth = 1; g.beginPath(); g.moveTo(0, h / 2 + .5); g.lineTo(w, h / 2 + .5); g.stroke();
  const line = (fn, a, lw, glow) => { g.beginPath(); for (let x = 0; x <= w; x++) { const y = h / 2 - fn(x / w) * h * .34; x ? g.lineTo(x, y) : g.moveTo(x, y); }
    g.strokeStyle = a; g.lineWidth = lw; g.lineJoin = 'round'; g.shadowColor = glow ? hue(H, .5) : 'transparent'; g.shadowBlur = glow ? 8 : 0; g.stroke(); g.shadowBlur = 0; };
  if (which === 'sub') {
    const sq = Math.round(S['syn.sub.wave']) === 1, lv = S['syn.sub.level'] / 100, nz = S['syn.noise.level'] / 100;
    let seed = 7; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647 - .5;
    line(t => { const ph = (t * 1.5) % 1; return (sq ? (ph < .5 ? .8 : -.8) : Math.sin(ph * 2 * Math.PI)) * Math.max(lv, .05) + rnd() * nz * 1.4; }, lv + nz > 0 ? hue(H) : tc('e9e6df', .25), 1.8, lv + nz > 0);
    return;
  }
  const x = which, onx = on('syn.' + x + '.on'), wv = Math.round(S['syn.' + x + '.wave']), sh = S['syn.' + x + '.shape'];
  const voices = wv === 4 ? 1 : [1, 3, 5, 7][Math.round(S['syn.' + x + '.voices'])], det = S['syn.' + x + '.detune'];
  for (let v = 0; v < voices; v++) { if (v === voices >> 1) continue; const off = (v - (voices - 1) / 2) * det / 1400;
    line(t => waveAt(wv, frac(t * 2 * (1 + off) + v * .07), sh), hue(H, .18), 1, false); }
  const sync = x === 'b' && on('syn.b.sync'), ratio = Math.pow(2, (S['syn.b.pitch'] - S['syn.a.pitch']) / 12);
  line(t => waveAt(wv, sync ? frac(frac(t * 2) * ratio) : frac(t * 2), sh), onx ? hue(H) : tc('e9e6df', .25), 2, onx);
}
function drawScopes() { if (tab === 2) drawScope(subView._cv, 'sub'); else drawScope(oscViews[tab]._cv, tab ? 'b' : 'a'); }
onChange(i => { if (/^syn\.(a|b|sub|noise)\./.test(i)) drawScopes(); });

/* filter response: analog prototypes, plus formants for VOWEL */
const VOWELS = [['A', [800, 1150, 2900]], ['E', [400, 1600, 2700]], ['I', [350, 2300, 3000]], ['O', [450, 800, 2830]], ['U', [325, 700, 2530]]];
function vowelAt(cut) { const x = clamp(Math.log(cut / 80) / Math.log(8000 / 80), 0, 1) * 4, i = Math.min(3, Math.floor(x)), t = x - i;
  return { f: VOWELS[i][1].map((f, k) => f + (VOWELS[i + 1][1][k] - f) * t), name: t < .5 ? VOWELS[i][0] : VOWELS[i + 1][0], x }; }
function filterMag(f) {
  const type = Math.round(S['syn.flt.type']), fc = S['syn.flt.cutoff'], r = S['syn.flt.res'] / 100;
  const cm = (a, b) => ({ re: a.re * b.re - a.im * b.im, im: a.re * b.im + a.im * b.re }), ca = (a, b) => ({ re: a.re + b.re, im: a.im + b.im }), cabs = a => Math.hypot(a.re, a.im);
  if (type === 6) { const v = vowelAt(fc), Q = 5 + r * 14; let m = 0; v.f.forEach((F, k) => { const s = f / F; const den = Math.hypot(1 - s * s, s / Q); m += [1, .7, .35][k] * (s / Q) / den; }); return m * .9; }
  const s = { re: 0, im: f / fc };
  if (type <= 1) { const n = type ? 2 : 4, k = (type ? 1.9 : 3.95) * r; let p = { re: 1, im: 0 }; for (let i = 0; i < n; i++) p = cm(p, { re: 1, im: s.im }); return (1 + k * .45) / cabs(ca(p, { re: k, im: 0 })); }
  const Q = .707 + r * 11, s2 = cm(s, s), den = ca(ca(s2, { re: 0, im: s.im / Q }), { re: 1, im: 0 });
  const num = type === 2 ? { re: 1, im: 0 } : type === 3 ? { re: 0, im: s.im / Q } : type === 4 ? s2 : ca(s2, { re: 1, im: 0 });
  return cabs(num) / cabs(den);
}
function drawFilter() {
  const [g, w, h] = hidpi(fCv); if (!g) return;
  const fx = x => 20 * Math.pow(1000, x / w), top = 14, db0 = h * .42, scale = (h - top) / 54;
  g.strokeStyle = tc('e9e6df', .05); g.lineWidth = 1;
  [100, 1000, 10000].forEach(f => { const x = Math.log(f / 20) / Math.log(1000) * w; g.beginPath(); g.moveTo(x + .5, 0); g.lineTo(x + .5, h); g.stroke(); });
  g.beginPath(); g.moveTo(0, db0 + .5); g.lineTo(w, db0 + .5); g.stroke();
  const pts = []; for (let x = 0; x <= w; x += 2) { const db = clamp(20 * Math.log10(Math.max(filterMag(fx(x)), 1e-5)), -36, 18); pts.push([x, db0 - db * scale]); }
  const live = on('syn.flt.on');
  const gr = g.createLinearGradient(0, 0, 0, h); gr.addColorStop(0, live ? hue('flt', .22) : tc('e9e6df', .04)); gr.addColorStop(1, hue('flt', 0));
  g.beginPath(); g.moveTo(0, h); pts.forEach(([x, y]) => g.lineTo(x, y)); g.lineTo(w, h); g.closePath(); g.fillStyle = gr; g.fill();
  g.beginPath(); pts.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y));
  g.strokeStyle = live ? hue('flt') : tc('e9e6df', .3); g.lineWidth = 1.9; g.shadowColor = hue('flt', .5); g.shadowBlur = live ? 8 : 0; g.stroke(); g.shadowBlur = 0;
  const nx = clamp(toN('syn.flt.cutoff') * w, 10, w - 10), ny = h - 8 - (S['syn.flt.res'] / 100 * .8 + .1) * (h - 16);
  g.fillStyle = '#ffffff'; g.beginPath(); g.arc(nx, clamp(ny, 6, h - 6), 4, 0, 7); g.fill();
  g.strokeStyle = hue('flt', .6); g.beginPath(); g.arc(nx, clamp(ny, 6, h - 6), 9, 0, 7); g.stroke();
  g.fillStyle = tc('e9e6df', .3); g.font = '9px ' + FONT();
  [['100', 100], ['1k', 1000], ['10k', 10000]].forEach(([t, f]) => g.fillText(t, Math.log(f / 20) / Math.log(1000) * w + 4, h - 5));
  if (Math.round(S['syn.flt.type']) === 6) {
    const v = vowelAt(S['syn.flt.cutoff']);
    g.font = '600 22px ' + FONT(); g.fillStyle = tc('e9e6df', .85); g.fillText(v.name, 12, 30);
    g.font = '600 8px ' + FONT(); g.fillStyle = tc('e9e6df', .35); g.fillText('VOWEL', 12, 44);
  }
}
onChange(i => { if (/^syn\.flt\.(cutoff|res|type|on)$/.test(i)) drawFilter(); });

/* envelope editor: drag the attack peak, decay/sustain corner and release end */
const ENVW = .3;
function envGeo() {
  const ids = envIds(envSel ? 'f' : 'a'), w = eCv.clientWidth, h = eCv.clientHeight, pad = 10, seg = (w - pad * 2) / 3.3;
  const ax = pad + toN(ids[0]) * seg, dx = ax + toN(ids[1]) * seg, sy = pad + (1 - S[ids[2]] / 100) * (h - pad * 2), sx = dx + seg * ENVW, rx = sx + toN(ids[3]) * seg;
  return { ids, w, h, pad, seg, ax, dx, sy, sx, rx, base: h - pad, top: pad };
}
let envHot = -1;
function drawEnv() {
  const [g, w, h] = hidpi(eCv); if (!g) return; const G = envGeo();
  g.strokeStyle = tc('e9e6df', .05); g.beginPath(); g.moveTo(0, G.base + .5); g.lineTo(w, G.base + .5); g.stroke();
  const path = () => { g.beginPath(); g.moveTo(G.pad, G.base); g.bezierCurveTo(G.pad + (G.ax - G.pad) * .4, G.top, G.ax, G.top, G.ax, G.top);
    g.bezierCurveTo(G.ax + (G.dx - G.ax) * .22, G.sy, G.dx, G.sy, G.dx, G.sy); g.lineTo(G.sx, G.sy);
    g.bezierCurveTo(G.sx + (G.rx - G.sx) * .22, G.base, G.rx, G.base, G.rx, G.base); };
  path(); g.lineTo(G.pad, G.base); const gr = g.createLinearGradient(0, 0, 0, h); gr.addColorStop(0, hue('env', .24)); gr.addColorStop(1, hue('env', 0)); g.fillStyle = gr; g.fill();
  path(); g.strokeStyle = hue('env'); g.lineWidth = 1.9; g.shadowColor = hue('env', .5); g.shadowBlur = 8; g.stroke(); g.shadowBlur = 0;
  g.setLineDash([2, 3]); g.strokeStyle = tc('e9e6df', .15); g.beginPath(); g.moveTo(G.sx + .5, G.top); g.lineTo(G.sx + .5, G.base); g.stroke(); g.setLineDash([]);
  [[G.ax, G.top], [G.dx, G.sy], [G.rx, G.base]].forEach(([x, y], i) => { g.fillStyle = i === envHot ? hue('env') : '#ffffff'; g.beginPath(); g.arc(x, y, i === envHot ? 4.5 : 3.6, 0, 7); g.fill(); });
  g.fillStyle = tc('e9e6df', .3); g.font = '600 8px ' + FONT(); g.fillText('NOTE OFF', G.sx + 5, 16);
}
{ let drag = null;
  eCv.addEventListener('pointerdown', e => { if (e.button !== 0) return; const r = eCv.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top, G = envGeo();
    const d = [[G.ax, G.top], [G.dx, G.sy], [G.rx, G.base]].map(([px, py]) => Math.hypot(px - x, py - y)); const i = d.indexOf(Math.min(...d));
    if (d[i] > 28) return; e.preventDefault(); drag = i; envHot = i; eCv.setPointerCapture(e.pointerId); drawEnv(); });
  eCv.addEventListener('pointermove', e => { if (drag === null) return; const r = eCv.getBoundingClientRect(), x = e.clientX - r.left, y = e.clientY - r.top, G = envGeo();
    if (drag === 0) set(G.ids[0], toV(G.ids[0], (x - G.pad) / G.seg), 'ui');
    if (drag === 1) { set(G.ids[1], toV(G.ids[1], (x - G.ax) / G.seg), 'ui'); set(G.ids[2], clamp((1 - (y - G.top) / (G.base - G.top)) * 100, 0, 100), 'ui'); }
    if (drag === 2) set(G.ids[3], toV(G.ids[3], (x - G.sx) / G.seg), 'ui'); });
  const end = () => { drag = null; envHot = -1; drawEnv(); };
  eCv.addEventListener('pointerup', end); eCv.addEventListener('pointercancel', end); }
onChange(i => { if (i.startsWith('syn.env.')) drawEnv(); });

/* LFO: animated */
const lfoVal = (sh, ph) => { switch (sh) { case 0: return Math.sin(ph * 2 * Math.PI); case 1: return ph < .25 ? ph * 4 : ph < .75 ? 2 - ph * 4 : ph * 4 - 4; case 2: return 1 - 2 * ph;
  case 3: return ph < .5 ? 1 : -1; case 4: return [.6, -.3, .9, -.8, .2, -.5, .7, -.1][Math.floor(ph * 8) % 8]; default: return Math.sin(ph * 2 * Math.PI) * .6 + Math.sin(ph * 6.28 * 3 + 1) * .3; } };
const DIV_BEATS = [4, 2, 1, .5, 1 / 3, .25, 1 / 6, .125];
let bpm = 120;
function lfoHz() { return on('syn.lfo.sync') ? bpm / 60 / DIV_BEATS[Math.round(S['syn.lfo.div'])] : S['syn.lfo.rate']; }
let lfoT0 = performance.now();
function drawLfo() {
  const [g, w, h] = hidpi(lCv); if (!g) return;
  const sh = Math.round(S['syn.lfo.shape']), amp = .14 + S['syn.lfo.depth'] / 100 * .26;
  g.strokeStyle = tc('e9e6df', .05); g.beginPath(); g.moveTo(0, h / 2 + .5); g.lineTo(w, h / 2 + .5); g.stroke();
  g.beginPath(); for (let x = 0; x <= w; x++) { const y = h / 2 - lfoVal(sh, (x / w * 2) % 1) * h * amp; x ? g.lineTo(x, y) : g.moveTo(x, y); }
  g.strokeStyle = S['syn.lfo.depth'] > 0 ? hue('mot') : tc('e9e6df', .3); g.lineWidth = 1.8; g.stroke();
  const ph = ((performance.now() - lfoT0) / 1000 * Math.min(lfoHz(), 8)) % 1, x = ph * w / 2;
  g.fillStyle = '#ffffff'; g.beginPath(); g.arc(x, h / 2 - lfoVal(sh, ph) * h * amp, 3.2, 0, 7); g.fill();
}

/* everything visible, redrawn when the page shows or the theme changes */
let lastShown = false;
function redrawAll() { drawScopes(); drawFilter(); drawEnv(); if (mtab) drawLfo(); drawLane(); }
(function loop() {
  const shown = SP.style.display !== 'none' && SP.offsetWidth > 0;
  if (shown && !lastShown) redrawAll();
  if (shown && mtab) drawLfo();
  lastShown = shown;
  requestAnimationFrame(loop);
})();
new MutationObserver(() => { if (lastShown) setTimeout(redrawAll, 30); }).observe(root, { attributes: true, attributeFilter: ['style', 'data-theme'] });

/* ------------------------------------------------------------ live note display */
const noteN = noteScr.querySelector('.n'), noteS = noteScr.querySelector('.s'), noteBar = noteScr.querySelector('.bar i');
let shownNote = null;
function showNote(m, src, level) {
  if (m == null || m < 0) { if (shownNote !== null) { noteN.textContent = '–'; noteS.textContent = on('syn.hold') ? 'HOLD IS ON' : 'PLAY A NOTE'; shownNote = null; } noteBar.style.width = '0'; return; }
  const r = Math.round(m);
  if (r !== shownNote) { noteN.innerHTML = NOTE_NAMES[((r % 12) + 12) % 12] + '<small>' + (Math.floor(r / 12) - 1) + '</small>'; shownNote = r; }
  noteS.textContent = (src || 'GUITAR').toUpperCase();
  noteBar.style.width = Math.round(clamp(level == null ? .6 : level, 0, 1) * 100) + '%';
}
let playStep = -1;
function showStep(i) { if (i === playStep) return; playStep = i; [...lane.children].forEach((s, k) => s.classList.toggle('play', k === i)); }

/* ------------------------------------------------------------ dice */
const kit = {};                 // shared with aether.js, the AETHER page (filled in at the end)
let undoState = null;
const rr = (a, b) => a + Math.random() * (b - a), ri = (a, b) => Math.floor(rr(a, b + 1)), pick = a => a[Math.floor(Math.random() * a.length)];
const wpick = ws => { let t = Math.random() * ws.reduce((x, y) => x + y, 0); for (let i = 0; i < ws.length; i++) { t -= ws[i]; if (t <= 0) return i; } return ws.length - 1; };
const KEEP = ['syn.sens', 'syn.gate', 'syn.dyn', 'syn.key', 'syn.scale', 'syn.tune', 'syn.mix', 'syn.level', 'syn.post'];
function keepUndo(withAether) { undoState = { S: { ...S }, steps: steps.map(s => ({ ...s })), name: prName, atm: !!withAether, aether: withAether && kit.aetherState ? kit.aetherState() : null }; undo.disabled = false; }
const isSyn = id => id.startsWith('syn.'), isAtm = id => id.startsWith('atm.');
function flash() { screen.classList.remove('sy-flash'); void screen.offsetWidth; screen.classList.add('sy-flash'); }
function roll() {
  keepUndo();
  const v = {};
  const type = wpick([4, 3, 2, 2, 1.5]);                                   // lead, bass, pad, pluck, weird
  v['syn.a.on'] = 1; v['syn.a.wave'] = wpick([4, 2, 1, 1, 2]); v['syn.a.pitch'] = 0; v['syn.a.fine'] = 0; v['syn.a.shape'] = pick([0, 0, rr(10, 70)]);
  v['syn.a.level'] = rr(65, 90); v['syn.a.voices'] = wpick([3, 3, 2, 1]); v['syn.a.detune'] = rr(8, 40); v['syn.a.width'] = rr(40, 100);
  v['syn.b.on'] = Math.random() < .8 ? 1 : 0; v['syn.b.wave'] = wpick([3, 3, 1, 2, 1]); v['syn.b.pitch'] = pick([-12, -12, 0, 7, 12, -24, 5]);
  v['syn.b.fine'] = rr(-12, 12); v['syn.b.shape'] = rr(0, 80); v['syn.b.level'] = rr(25, 70); v['syn.b.voices'] = wpick([4, 2, 1, 0]); v['syn.b.detune'] = rr(5, 30); v['syn.b.width'] = rr(20, 80);
  v['syn.b.sync'] = Math.random() < .15 ? 1 : 0; if (v['syn.b.sync']) v['syn.b.pitch'] = pick([7, 12, 19]);
  v['syn.sub.level'] = type === 1 ? rr(40, 75) : Math.random() < .5 ? 0 : rr(10, 40); v['syn.sub.wave'] = ri(0, 1); v['syn.sub.oct'] = Math.random() < .8 ? 0 : 1;
  v['syn.noise.level'] = Math.random() < .7 ? 0 : rr(5, 25); v['syn.noise.color'] = rr(20, 90);
  v['syn.flt.on'] = 1; v['syn.flt.type'] = wpick([4, 2, 1.5, 1, .5, .3, 1.5]);
  v['syn.flt.cutoff'] = type === 1 ? rr(250, 1200) : type === 2 ? rr(800, 4000) : rr(500, 5000);
  v['syn.flt.res'] = rr(0, 65); v['syn.flt.drive'] = rr(0, 60); v['syn.flt.env'] = pick([rr(15, 80), rr(15, 80), rr(-40, 0)]); v['syn.flt.key'] = rr(20, 80);
  if (type === 2) Object.assign(v, { 'syn.env.a.att': rr(200, 1500), 'syn.env.a.dec': rr(800, 3000), 'syn.env.a.sus': rr(70, 100), 'syn.env.a.rel': rr(600, 2500), 'syn.env.f.att': rr(300, 2000), 'syn.env.f.dec': rr(800, 3000), 'syn.env.f.sus': rr(40, 90), 'syn.env.f.rel': rr(500, 2000) });
  else if (type === 3) Object.assign(v, { 'syn.env.a.att': rr(1, 5), 'syn.env.a.dec': rr(120, 500), 'syn.env.a.sus': rr(0, 20), 'syn.env.a.rel': rr(80, 400), 'syn.env.f.att': 1, 'syn.env.f.dec': rr(80, 400), 'syn.env.f.sus': rr(0, 20), 'syn.env.f.rel': rr(80, 300) });
  else Object.assign(v, { 'syn.env.a.att': rr(1, 40), 'syn.env.a.dec': rr(300, 1500), 'syn.env.a.sus': rr(50, 95), 'syn.env.a.rel': rr(80, 600), 'syn.env.f.att': rr(1, 80), 'syn.env.f.dec': rr(150, 900), 'syn.env.f.sus': rr(10, 60), 'syn.env.f.rel': rr(100, 600) });
  v['syn.glide'] = type === 0 ? rr(20, 140) : rr(0, 60);
  v['syn.lfo.depth'] = Math.random() < .45 ? 0 : rr(10, 55); v['syn.lfo.shape'] = wpick([3, 2, 1, 1, 1, 1.5]); v['syn.lfo.target'] = wpick([3, 3, 1.5, 1, 1]);
  v['syn.lfo.rate'] = v['syn.lfo.target'] === 0 ? rr(4, 7) : rr(.2, 6); v['syn.lfo.fade'] = v['syn.lfo.target'] === 0 ? rr(200, 900) : rr(0, 300); v['syn.lfo.sync'] = Math.random() < .3 ? 1 : 0; v['syn.lfo.div'] = ri(2, 6);
  v['syn.chord'] = Math.random() < .3 ? pick([1, 2, 3, 3, 5, 6, 4]) : 0;
  v['syn.arp.on'] = type === 3 || Math.random() < .3 ? 1 : 0; v['syn.arp.pattern'] = wpick([3, 1, 2, 1, 2.5]); v['syn.arp.rate'] = wpick([.3, 1.5, 1, 4, 1, .8]);
  v['syn.arp.oct'] = wpick([2, 3, 1, .5]); v['syn.arp.gate'] = rr(30, 90); v['syn.arp.swing'] = Math.random() < .7 ? 0 : rr(10, 45); v['syn.arp.len'] = pick([16, 16, 8, 12, 16]);
  v['syn.char'] = wpick([2, 4, 1.2, 1.2]); v['syn.space'] = type === 2 ? rr(40, 80) : rr(10, 50);
  v['syn.legato'] = type === 0 && Math.random() < .5 ? 1 : 0;
  if (v['syn.arp.on']) steps = steps.map((_, i) => ({ on: i % 4 === 0 || Math.random() < .55 ? 1 : 0, acc: i % 4 === 0 ? 1 : (Math.random() < .15 ? 1 : 0), tie: Math.random() < .08 ? 1 : 0,
    oct: Math.random() < .18 ? pick([1, 1, -1, 2]) : 0, ratchet: Math.random() < .1 ? pick([2, 3]) : 1, chance: Math.random() < .15 ? pick([50, 75]) : 100, note: pick([0, 0, 2, 4, 4, 7, 9, 11]) }));
  for (const id in v) if (!KEEP.includes(id)) set(id, v[id], 'dice');
  drawLane(); stepsChanged('dice');
  showPreset('Dice roll', -1);
  flash();
}
dice.addEventListener('click', roll);
undo.addEventListener('click', () => { if (!undoState) return; const u = undoState; for (const id in u.S) if (isSyn(id) || (u.atm && isAtm(id))) set(id, u.S[id], 'dice'); steps = u.steps; drawLane(); stepsChanged('dice');
  if (u.aether && kit.aetherRestore) kit.aetherRestore(u.aether);
  showPreset(u.name, PRESETS.findIndex(p => p.name === u.name)); undoState = null; undo.disabled = true; });

/* ------------------------------------------------------------ presets and reset */
/* Presets (synth_presets.js) give only what differs from the defaults. They change the sound and
   the arpeggiator steps; your mix, output, key, tuning and playing settings (like DICE) stay. */
const PRESETS = window.OBS_SYNTH_PRESETS || [];
const PKEEP = KEEP.concat(['syn.hold']);
const RESET_KEEP = ['syn.key', 'syn.scale', 'syn.tune'];
let prName = 'Init', prIdx = -1;
const prNameEl = prBtn.querySelector('.sy-prname');
function drawPrName() {
  prNameEl.textContent = prName; prNameEl.title = prName;
  const pn = badge.querySelector('.pn'), pr = PRESETS[prIdx], col = prName === 'Custom' ? '#8d8d93' : pr ? catCol(pr.cat) : 'var(--accent)';
  pn.style.setProperty('--pc', col); pn.querySelector('u').textContent = prName; pn.classList.toggle('hor', !!pr && pr.cat === 'HORROR' && prName !== 'Custom');
  pn.querySelector('em').textContent = prName === 'Custom' ? 'FROM THE MAIN PRESET' : pr ? pr.cat : '';
}
function showPreset(name, i) { prName = name; prIdx = i; drawPrName(); markPr(); }
// mark the loaded one (no rebuild: the tile that was clicked stays in the page)
function markPr() { if (prList) [...prList.children].forEach(t => t.classList.toggle('on', +t.dataset.i === prIdx)); }
/** A preset value: a number, or one of the setting's choices by name ('Pulse', '1/16', '8-bit'). */
function presetValue(id, v) {
  const p = P[id]; if (!p) return undefined;
  if (typeof v === 'string') { const i = (p.opts || []).findIndex(o => o.toLowerCase() === v.toLowerCase()); return i < 0 ? undefined : i; }
  return v;
}
/** Steps in short form: on / acc / tie as 'x.x.', note / oct / rat / ch as lists; the rest default. */
function presetSteps(st) {
  if (!st) return Array.from({ length: 16 }, (_, i) => STEP_DEF(i));
  const f = (s, i) => s && s[i] === 'x' ? 1 : 0, a = (l, i, d) => l && l[i] != null ? l[i] : d;
  return Array.from({ length: 16 }, (_, i) => ({ on: st.on ? f(st.on, i) : 1, acc: f(st.acc, i), tie: f(st.tie, i), oct: a(st.oct, i, 0), ratchet: a(st.rat, i, 1), chance: a(st.ch, i, 100), note: a(st.note, i, 0) }));
}
function loadPreset(i) {
  const pr = PRESETS[i]; if (!pr) return;
  keepUndo(!!pr.a);
  const want = {};
  for (const k in pr.v) { const id = 'syn.' + k, v = presetValue(id, pr.v[k]); if (v !== undefined) want[id] = v; }
  for (const id in P) if (isSyn(id) && !PKEEP.includes(id)) set(id, id in want ? want[id] : P[id].def, 'preset');
  steps = presetSteps(pr.steps); drawLane(); stepsChanged('preset');
  // a preset with an AETHER part ("a": the HORROR ones) also sets the atmosphere around it, and turns it on
  if (pr.a && kit.loadAether) kit.loadAether(pr.a, pr.name, pr.cat);
  showPreset(pr.name, i); flash();
}
function resetAll() {
  keepUndo();
  for (const id in P) if (isSyn(id) && !RESET_KEEP.includes(id)) set(id, P[id].def, 'preset');
  steps = presetSteps(null); drawLane(); stepsChanged('preset');
  showPreset('Init', -1); flash();
}
reset.addEventListener('click', resetAll);
// a knob you turn marks the preset as edited (*); a sound that comes from the plugin (a rig preset, automation) is "Custom"
onChange((id, v, from) => { if (!isSyn(id) || PKEEP.includes(id)) return;
  if (from === 'ui' && !/\*$/.test(prName)) { prName += ' *'; drawPrName(); }
  else if (from === 'plugin' && prName !== 'Custom') { prName = 'Custom'; prIdx = -1; drawPrName(); markPr(); } });
stepListeners.push(from => { if (from === 'ui' && !/\*$/.test(prName)) { prName += ' *'; drawPrName(); } });
/* the browser */
const CATS = [['ALL', 'var(--accent)'], ['ARCADE', '#ffd23f'], ['CARTOON', '#ff9ec7'], ['SYN', '#ff5d5d'], ['LEADS', 'var(--h-osc)'], ['BASS', 'var(--h-sub)'], ['PADS & KEYS', 'var(--h-mot)'], ['ARPS', 'var(--h-flt)'], ['FX & WEIRD', '#9dff8a'], ['HORROR', '#c41a24']];
const horCls = c => c === 'HORROR' ? ' hor' : '';
const catCol = c => (CATS.find(x => x[0] === c) || CATS[0])[1];
let prCat = 'ALL';
const prs = el('div', 'sy-prs sy-siren');
prs.innerHTML = '<div class="sy-prs-h"><b>SYNTH PRESETS</b><span>' + PRESETS.length + ' sounds · click one and play, the window stays open</span><button class="x" aria-label="Close">✕</button></div><div class="sy-prs-b"><div class="sy-prs-cats"></div><div class="sy-prs-list"></div></div>';
dev.appendChild(prs);
const prCats = prs.querySelector('.sy-prs-cats'), prList = prs.querySelector('.sy-prs-list');
prCats.innerHTML = CATS.map(([c, col]) => '<button class="' + horCls(c).trim() + '" data-c="' + c + '" style="--cc:' + col + '"><i></i>' + c + '<em>' + (c === 'ALL' ? PRESETS.length : PRESETS.filter(p => p.cat === c).length) + '</em></button>').join('');
function drawPrList() {
  if (!prList) return;
  [...prCats.children].forEach(b => b.classList.toggle('on', b.dataset.c === prCat));
  prList.innerHTML = PRESETS.map((p, i) => [p, i]).filter(([p]) => prCat === 'ALL' || p.cat === prCat)
    .map(([p, i]) => '<button class="sy-pr' + horCls(p.cat) + (i === prIdx ? ' on' : '') + '" data-i="' + i + '" style="--cc:' + catCol(p.cat) + '" title="' + p.note.replace(/"/g, '&quot;') + '"><b>' + p.name + '</b><small>' + p.note + '</small></button>').join('');
}
prCats.addEventListener('click', e => { const b = e.target.closest('button'); if (!b) return; prCat = b.dataset.c; drawPrList(); prList.scrollTop = 0; });
prList.addEventListener('click', e => { const b = e.target.closest('.sy-pr'); if (b) loadPreset(+b.dataset.i); });
const prOpen = open => { prs.classList.toggle('show', open); prBtn.classList.toggle('on', open); if (open) { pop.classList.remove('show'); if (typeof kbOpen === 'function' && kbKeys.classList.contains('show')) kbOpen(false); drawPrList(); const o = prList.querySelector('.on'); if (o) o.scrollIntoView({ block: 'nearest' }); } };
prBtn.addEventListener('click', e => { e.stopPropagation(); prOpen(!prs.classList.contains('show')); });
prs.querySelector('.x').addEventListener('click', () => prOpen(false));
document.addEventListener('pointerdown', e => { if (prs.classList.contains('show') && !prs.contains(e.target) && !prWrap.contains(e.target)) prOpen(false); });
const stepPreset = d => { if (!PRESETS.length) return; const list = PRESETS.map((p, i) => i).filter(i => prCat === 'ALL' || PRESETS[i].cat === prCat);
  const at = list.indexOf(prIdx); loadPreset(list[at < 0 ? (d > 0 ? 0 : list.length - 1) : (at + d + list.length) % list.length]); };
prNav.children[0].addEventListener('click', () => stepPreset(-1));
prNav.children[1].addEventListener('click', () => stepPreset(1));
drawPrList();

/* ============================================================ KEYS: play SIREN without a guitar */
/* An on-screen keyboard over the screen (the sound knobs above stay in reach), or 16 pads that only
   play notes of the key. Mouse, touch, or the computer keys (A W S E D F T G Y H U J K O L P ;  Z X: octave). */
const KB = { oct: 36, view: 'piano', scale: 'all', vel: 'height', labels: true };
try { Object.assign(KB, JSON.parse(localStorage.getItem('obs.sirenKeys') || '{}')); } catch (_) {}
const kbSave = () => { try { localStorage.setItem('obs.sirenKeys', JSON.stringify(KB)); } catch (_) {} };
KB.oct = clamp(Math.round(KB.oct / 12) * 12, 12, 72);
const KEYMAP = { a: 0, w: 1, s: 2, e: 3, d: 4, f: 5, t: 6, g: 7, y: 8, h: 9, u: 10, j: 11, k: 12, o: 13, l: 14, p: 15, ';': 16 };
const KEYCHAR = Object.fromEntries(Object.entries(KEYMAP).map(([c, o]) => [o, c.toUpperCase()]));
const PADKEYS = ['a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i'];          // in PADS: bottom row, then top row
const padNote = i => { const sc = SCALES[Math.round(S['syn.scale'])]; return scaleTone(KB.oct + 12 + Math.round(S['syn.key']), i, sc); };
const kbHeld = new Map();                    // source ('ptr:<id>' / 'key:<char>') -> midi note sounding
const isBlackKey = m => [1, 3, 6, 8, 10].includes(((m % 12) + 12) % 12);
const inScale = m => { const sc = SCALES[Math.round(S['syn.scale'])], rel = ((m - Math.round(S['syn.key'])) % 12 + 12) % 12; return sc.includes(rel); };
const isRoot = m => ((m - Math.round(S['syn.key'])) % 12 + 12) % 12 === 0;
function snapNote(m) { if (KB.scale !== 'snap' || inScale(m)) return m; for (let d = 1; d < 7; d++) { if (inScale(m - d)) return m - d; if (inScale(m + d)) return m + d; } return m; }
const noteLbl = m => NOTE_NAMES[((m % 12) + 12) % 12] + (Math.floor(m / 12) - 1);
const kbKeys = el('div', 'sy-keys sy-siren');
kbKeys.innerHTML = '<div class="sy-keys-h"><b>KEYS</b><span>play SIREN without your guitar</span><i class="sp"></i></div><div class="sy-keys-b"></div>';
dev.appendChild(kbKeys);
const kbHead = kbKeys.querySelector('.sy-keys-h'), kbBody = kbKeys.querySelector('.sy-keys-b');
const kSeg = (opts, get, setv, tip) => { const s = el('div', 'sy-seg small'); if (tip) s.title = tip;
  s.innerHTML = opts.map(([v, l]) => '<button data-v="' + v + '">' + l + '</button>').join('');
  const draw = () => [...s.children].forEach(b => b.classList.toggle('on', b.dataset.v === String(get())));
  s.addEventListener('click', e => { const b = e.target.closest('button'); if (!b) return; setv(b.dataset.v); kbSave(); draw(); buildKb(); drawOct(); }); draw(); s._draw = draw; return s; };
const viewSeg = kSeg([['piano', 'PIANO'], ['pads', 'PADS']], () => KB.view, v => { KB.view = v; }, 'PIANO: every note · PADS: only notes of the key, so every pad fits');
const octBox = el('div', 'sy-keys-oct', '<button aria-label="Octave down">‹</button><span></span><button aria-label="Octave up">›</button>');
octBox.title = 'Octave (computer keys: Z and X)';
const scaleSeg = kSeg([['all', 'ALL NOTES'], ['key', 'IN KEY'], ['snap', 'SNAP']], () => KB.scale, v => { KB.scale = v; }, 'ALL NOTES: as they are · IN KEY: notes outside the key fade · SNAP: notes outside the key play the nearest one in it');
const velSeg = kSeg([['soft', 'SOFT'], ['hard', 'HARD'], ['height', 'BY HEIGHT']], () => KB.vel, v => { KB.vel = v; }, 'How hard the notes play. BY HEIGHT: lower on the key plays harder, like a real one');
const lblChip = el('button', 'sy-chip', 'LABELS'); lblChip.title = 'Note names and computer keys on the keys';
const holdChip = chip('syn.hold', 'HOLD', 'The last note keeps playing after you let go');
const kbClose = el('button', 'x', '✕'); kbClose.setAttribute('aria-label', 'Close the keys');
kbHead.append(viewSeg, octBox, scaleSeg, velSeg, lblChip, holdChip, kbClose);
const drawOct = () => { octBox.querySelector('span').textContent = KB.view === 'pads' ? noteLbl(padNote(0)) + ' – ' + noteLbl(padNote(15)) : noteLbl(KB.oct) + ' – ' + noteLbl(KB.oct + 36); lblChip.classList.toggle('on', KB.labels); };
octBox.children[0].onclick = () => shiftOct(-12); octBox.children[2].onclick = () => shiftOct(12);
lblChip.onclick = () => { KB.labels = !KB.labels; kbSave(); drawOct(); buildKb(); };
function shiftOct(d) { allOff(); KB.oct = clamp(KB.oct + d, 12, 72); kbSave(); drawOct(); buildKb(); }
const velFor = (ev, keyEl) => { if (KB.vel === 'soft') return .5; if (KB.vel === 'hard') return 1;
  const r = keyEl.getBoundingClientRect(); return clamp(.3 + .7 * (ev.clientY - r.top) / Math.max(1, r.height), .3, 1); };
function kbOn(src, m, vel) { m = snapNote(m); const old = kbHeld.get(src); if (old === m) return; if (old != null) releaseKey(old); kbHeld.set(src, m); press(m, 'KEYS', vel); drawKbLit(); }
function kbOff(src) { const m = kbHeld.get(src); if (m == null) return; kbHeld.delete(src); if (![...kbHeld.values()].includes(m)) releaseKey(m); drawKbLit(); }
function allOff() { [...kbHeld.keys()].forEach(kbOff); }
function drawKbLit() { const lit = new Set(kbHeld.values()); kbBody.querySelectorAll('[data-m]').forEach(k => k.classList.toggle('lit', lit.has(+k.dataset.m))); }
function buildKb() {
  allOff(); kbBody.replaceChildren(); kbBody.className = 'sy-keys-b ' + KB.view;
  if (KB.view === 'pads') {
    const root = KB.oct + 12 + Math.round(S['syn.key']), sc = SCALES[Math.round(S['syn.scale'])];
    for (let i = 0; i < 16; i++) {
      const row = i < 8 ? 1 : 0, deg = row ? i : i - 8, idx = row ? deg : deg + 8;   // bottom row low, top row high
      const m = scaleTone(root, idx, sc), n = idx % sc.length + 1;
      const p = el('button', 'sy-pad' + (isRoot(m) ? ' root' : ''), '<b>' + noteLbl(m) + '</b><small>' + (isRoot(m) ? 'ROOT' : 'DEGREE ' + n) + '</small>' + (KB.labels ? '<em>' + PADKEYS[idx].toUpperCase() + '</em>' : ''));
      p.dataset.m = m; p.dataset.i = idx; p.style.gridRow = String(row + 1); p.style.gridColumn = String(deg + 1); kbBody.appendChild(p);
    }
  } else {
    let wi = 0; const whites = [];
    for (let m = KB.oct; m <= KB.oct + 36; m++) if (!isBlackKey(m)) whites.push(m);
    const W = 100 / whites.length;
    for (let m = KB.oct; m <= KB.oct + 36; m++) {
      const black = isBlackKey(m), off = KB.scale !== 'all' && !inScale(m);
      const k = el('div', 'sy-pk ' + (black ? 'b' : 'w') + (off ? ' off' : '') + (isRoot(m) ? ' root' : '') + (!off && KB.scale !== 'all' ? ' ink' : ''));
      k.dataset.m = m;
      if (black) { k.style.left = 'calc(' + (wi * W) + '% - ' + (W * .32) + '%)'; k.style.width = (W * .64) + '%'; }
      else { k.style.left = (wi * W) + '%'; k.style.width = W + '%'; wi++; }
      const ch = KEYCHAR[m - KB.oct - 12];
      if (KB.labels) k.innerHTML = (ch ? '<em>' + ch + '</em>' : '') + (!black && (m % 12 === 0 || isRoot(m)) ? '<u>' + noteLbl(m) + '</u>' : '');
      kbBody.appendChild(k);
    }
  }
  drawKbLit();
}
kbBody.addEventListener('pointerdown', e => { const k = e.target.closest('[data-m]'); if (!k || e.button !== 0) return; e.preventDefault(); kbBody.setPointerCapture(e.pointerId); kbOn('ptr:' + e.pointerId, +k.dataset.m, velFor(e, k)); });
kbBody.addEventListener('pointermove', e => { if (!kbHeld.has('ptr:' + e.pointerId)) return; const t = document.elementFromPoint(e.clientX, e.clientY), k = t && t.closest && t.closest('[data-m]');
  if (k && kbBody.contains(k) && +k.dataset.m !== kbHeld.get('ptr:' + e.pointerId)) kbOn('ptr:' + e.pointerId, +k.dataset.m, velFor(e, k)); });
['pointerup', 'pointercancel'].forEach(t => kbBody.addEventListener(t, e => kbOff('ptr:' + e.pointerId)));
const kbOpen = open => { kbKeys.classList.toggle('show', open); kbBtn.classList.toggle('on', open); if (open) { prOpen(false); pop.classList.remove('show'); drawOct(); buildKb(); } else allOff(); };
kbClose.onclick = () => kbOpen(false);
kbBtn.addEventListener('click', e => { e.stopPropagation(); kbOpen(!kbKeys.classList.contains('show')); });
onChange(id => { if (/^syn\.(key|scale)$/.test(id) && kbKeys.classList.contains('show')) { buildKb(); drawOct(); } });


/* ============================================================ preview engine (browser only) */
/* In the plugin the C++ engine plays; here a small Web Audio version does, so the page can be tried anywhere. */
const inPlugin = () => !!(window.__JUCE__ && (!window.__mock || window.__mock.engineNotes));
let ac = null, outNode = null, comp = null, spaceIn = null, spaceWet = null;
const midiHz = m => 440 * Math.pow(2, (m - 69) / 12) * (S['syn.tune'] / 440);
const tables = new Map();
function table(w, shape) {
  const key = w + ':' + Math.round(shape / 5); if (tables.has(key)) return tables.get(key);
  const N = 1024, H = 64, x = new Float32Array(N); for (let i = 0; i < N; i++) x[i] = waveAt(w, i / N, shape);
  const re = new Float32Array(H), im = new Float32Array(H);
  for (let k = 1; k < H; k++) { let a = 0, b = 0; for (let i = 0; i < N; i++) { const t = 2 * Math.PI * k * i / N; a += x[i] * Math.cos(t); b += x[i] * Math.sin(t); } re[k] = a / N * 2; im[k] = b / N * 2; }
  const pw = ac.createPeriodicWave(re, im, { disableNormalization: false }); tables.set(key, pw); return pw;
}
function audio() {
  if (ac) return ac;
  ac = new (window.AudioContext || window.webkitAudioContext)();
  comp = ac.createDynamicsCompressor(); comp.threshold.value = -12; comp.ratio.value = 3;
  outNode = ac.createGain(); outNode.gain.value = .5;
  // space: ping-pong echoes + smear
  spaceIn = ac.createGain(); spaceWet = ac.createGain(); spaceWet.gain.value = 0;
  const dl = ac.createDelay(2), dr = ac.createDelay(2), fbl = ac.createGain(), fbr = ac.createGain(), lp = ac.createBiquadFilter(), m = ac.createChannelMerger(2);
  lp.type = 'lowpass'; lp.frequency.value = 3500; fbl.gain.value = fbr.gain.value = .42;
  spaceIn.connect(lp); lp.connect(dl); dl.connect(fbl); fbl.connect(dr); dr.connect(fbr); fbr.connect(dl);
  dl.connect(m, 0, 0); dr.connect(m, 0, 1); m.connect(spaceWet); spaceWet.connect(comp);
  audio.dl = dl; audio.dr = dr;
  outNode.connect(comp); outNode.connect(spaceIn); comp.connect(ac.destination);
  return ac;
}
function shaper(curveFn) { const ws = ac.createWaveShaper(), n = 2048, c = new Float32Array(n); for (let i = 0; i < n; i++) c[i] = curveFn(i / (n - 1) * 2 - 1); ws.curve = c; ws.oversample = '2x'; return ws; }
let voice = null, gateOpen = false, curNotes = null, velNow = .9;
function makeVoice(notes) {
  const v = { oscs: [], stop: [], t0: ac.currentTime };
  const ch = Math.round(S['syn.char']);
  v.mix = ac.createGain();
  const drv = 1 + S['syn.flt.drive'] / 100 * 5 + (ch === 1 ? .6 : 0);
  v.pre = shaper(x => Math.tanh(x * drv) / Math.tanh(drv));
  v.amp = ac.createGain(); v.amp.gain.value = 0;
  v.trem = ac.createGain(); v.pan = ac.createStereoPanner();
  v.mix.connect(v.pre);
  // filter
  const ft = Math.round(S['syn.flt.type']); v.filters = [];
  if (!on('syn.flt.on')) v.pre.connect(v.amp);
  else if (ft === 6) { v.vsum = ac.createGain(); [1, .7, .35].forEach(g => { const f = ac.createBiquadFilter(), gg = ac.createGain(); f.type = 'bandpass'; gg.gain.value = g * 2.2; v.pre.connect(f); f.connect(gg); gg.connect(v.vsum); v.filters.push(f); }); v.vsum.connect(v.amp); }
  else { const type = ['lowpass', 'lowpass', 'lowpass', 'bandpass', 'highpass', 'notch'][ft]; const f1 = ac.createBiquadFilter(); f1.type = type; v.filters.push(f1); v.pre.connect(f1);
    if (ft === 0) { const f2 = ac.createBiquadFilter(); f2.type = 'lowpass'; v.filters.push(f2); f1.connect(f2); f2.connect(v.amp); } else f1.connect(v.amp); }
  // character after the filter
  let last = v.amp;
  if (ch === 2) { const bits = 5, q = Math.pow(2, bits - 1); const cr = shaper(x => Math.round(x * q) / q); last.connect(cr); last = cr; }
  if (ch === 3) { const lp = ac.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 5200; last.connect(lp); last = lp; }
  last.connect(v.trem); v.trem.connect(v.pan); v.pan.connect(outNode);
  // oscillators: one set per chord note
  const nNotes = notes.length;
  const addOsc = (x, ni) => {
    if (!on('syn.' + x + '.on') || S['syn.' + x + '.level'] <= 0) return;
    const w = Math.round(S['syn.' + x + '.wave']), cnt = w === 4 ? 7 : [1, 3, 5, 7][Math.round(S['syn.' + x + '.voices'])];
    const lvl = S['syn.' + x + '.level'] / 100, det = w === 4 ? 8 + S['syn.' + x + '.detune'] * .55 : S['syn.' + x + '.detune'] * .5;
    const pw = table(w === 4 ? 0 : w, w === 4 ? 0 : S['syn.' + x + '.shape']);
    for (let i = 0; i < cnt; i++) {
      const o = ac.createOscillator(), g = ac.createGain(), p = ac.createStereoPanner();
      o.setPeriodicWave(pw);
      const sp = cnt > 1 ? (i - (cnt - 1) / 2) / ((cnt - 1) / 2) : 0;
      o.detune.value = S['syn.' + x + '.fine'] + sp * det;
      p.pan.value = sp * S['syn.' + x + '.width'] / 100;
      const side = w === 4 && i !== 3 ? (.3 + S['syn.' + x + '.shape'] / 100 * .7) : 1;
      g.gain.value = lvl * lvl * side / Math.sqrt(cnt) * .32 / Math.sqrt(nNotes);
      o.connect(g); g.connect(p); p.connect(v.mix); o.start();
      v.oscs.push({ o, ni, semi: S['syn.' + x + '.pitch'] }); v.stop.push(o);
    }
  };
  for (let ni = 0; ni < nNotes; ni++) { addOsc('a', ni); addOsc('b', ni); }
  if (S['syn.sub.level'] > 0) { const o = ac.createOscillator(), g = ac.createGain(); o.type = Math.round(S['syn.sub.wave']) ? 'square' : 'sine';
    g.gain.value = S['syn.sub.level'] / 100 * (Math.round(S['syn.sub.wave']) ? .18 : .4); o.connect(g); g.connect(v.mix); o.start(); v.oscs.push({ o, ni: 0, semi: Math.round(S['syn.sub.oct']) ? -24 : -12 }); v.stop.push(o); }
  if (S['syn.noise.level'] > 0) { const b = ac.createBuffer(1, ac.sampleRate, ac.sampleRate), d = b.getChannelData(0); let l = 0; const c = S['syn.noise.color'] / 100;
    for (let i = 0; i < d.length; i++) { const wn = Math.random() * 2 - 1; l += (wn - l) * (.04 + c * .96); d[i] = l * (1.5 - c * .6); }
    const src = ac.createBufferSource(), g = ac.createGain(); src.buffer = b; src.loop = true; g.gain.value = S['syn.noise.level'] / 100 * .3; src.connect(g); g.connect(v.mix); src.start(); v.stop.push(src); }
  // LFO (fades in) + VHS wobble
  const depth = S['syn.lfo.depth'] / 100;
  if (depth > 0) {
    const sh = Math.round(S['syn.lfo.shape']); let l;
    if (sh >= 4) { const N = 64, b = ac.createBuffer(1, N * 64, ac.sampleRate), d = b.getChannelData(0); let a = 0, tgt = 0;
      for (let i = 0; i < N; i++) { const r = Math.random() * 2 - 1; for (let j = 0; j < 64; j++) { if (sh === 5) { a += (r - a) * .05; d[i * 64 + j] = a; } else d[i * 64 + j] = r; } }
      l = ac.createBufferSource(); l.buffer = b; l.loop = true; l.playbackRate.value = lfoHz() * 64 * 64 / ac.sampleRate / 1; }
    else { l = ac.createOscillator(); l.type = ['sine', 'triangle', 'sawtooth', 'square'][sh]; l.frequency.value = lfoHz(); }
    const g = ac.createGain(), tg = Math.round(S['syn.lfo.target']), fade = S['syn.lfo.fade'] / 1000;
    let amt = 0;
    if (tg === 0) { amt = depth * 120; v.oscs.forEach(x => g.connect(x.o.detune)); }
    if (tg === 1) { amt = depth * S['syn.flt.cutoff'] * .9; v.filters.forEach(f => g.connect(f.frequency)); }
    if (tg === 2) { amt = depth * 18; v.oscs.forEach((x, i) => { if (i % 2) g.connect(x.o.detune); }); }
    if (tg === 3) { amt = depth * .5; v.trem.gain.value = 1 - depth * .5; g.connect(v.trem.gain); }
    if (tg === 4) { amt = depth; g.connect(v.pan.pan); }
    g.gain.setValueAtTime(fade > 0 ? 0 : amt, ac.currentTime); if (fade > 0) g.gain.linearRampToValueAtTime(amt, ac.currentTime + fade);
    l.connect(g); l.start(); v.stop.push(l); v.lfoGain = g; v.lfoAmt = amt;
  }
  if (ch === 3 || ch === 1) { const l = ac.createOscillator(), g = ac.createGain(); l.frequency.value = ch === 3 ? .55 : .23; g.gain.value = ch === 3 ? 14 : 3; l.connect(g); v.oscs.forEach(x => g.connect(x.o.detune)); l.start(); v.stop.push(l); }
  const beat = 60 / bpm; audio.dl.delayTime.setTargetAtTime(beat * .75, ac.currentTime, .05); audio.dr.delayTime.setTargetAtTime(beat * .75, ac.currentTime, .05);
  spaceWet.gain.setTargetAtTime(S['syn.space'] / 100 * .75, ac.currentTime, .05);
  outNode.gain.setTargetAtTime(.5 * Math.pow(10, S['syn.level'] / 20), ac.currentTime, .05);
  v.notes = notes;
  return v;
}
function killVoice(v, t = .015) { if (!v) return; const now = ac.currentTime; v.amp.gain.cancelScheduledValues(now); v.amp.gain.setTargetAtTime(0, now, t);
  setTimeout(() => { v.stop.forEach(o => { try { o.stop(); } catch (_) {} }); try { v.pan.disconnect(); } catch (_) {} }, 300); }
function setPitches(notes, t, glide) {
  if (!voice) return;
  voice.oscs.forEach(x => { const n = notes[Math.min(x.ni, notes.length - 1)], f = midiHz(n + x.semi);
    if (glide > 0) x.o.frequency.setTargetAtTime(f, t, glide / 3); else x.o.frequency.setValueAtTime(f, t); });
}
function filterTargets(rootNote, vel) {
  const ft = Math.round(S['syn.flt.type']), key = Math.pow(2, (rootNote - 52) / 12 * S['syn.flt.key'] / 100);
  const base = clamp(S['syn.flt.cutoff'] * key, 20, 18000), env = S['syn.flt.env'] / 100 * (.6 + .4 * vel);
  return { base, peak: clamp(base * Math.pow(2, env * 6), 20, 18000), sus: clamp(base * Math.pow(2, env * 6 * S['syn.env.f.sus'] / 100), 20, 18000), ft };
}
function applyFilter(t, rootNote, vel, steady) {
  if (!voice || !voice.filters.length) return;
  const F = filterTargets(rootNote, vel), r = S['syn.flt.res'] / 100;
  const set1 = (f, k, hzv, how) => { if (F.ft === 6) { const vw = vowelAt(hzv); hzv = vw.f[k]; }
    f.frequency.cancelScheduledValues(t);
    if (how === 'steady') f.frequency.setTargetAtTime(hzv, t, .02);
    else { f.frequency.setValueAtTime(Math.max(20, f.frequency.value), t); f.frequency.linearRampToValueAtTime(hzv, t + S['syn.env.f.att'] / 1000); } };
  voice.filters.forEach((f, k) => {
    f.Q.setValueAtTime(F.ft === 6 ? 5 + r * 14 : F.ft <= 1 ? (k ? .7 : .7 + r * (F.ft ? 16 : 12)) : .7 + r * 14, t);
    if (steady) { set1(f, k, gateOpen ? F.sus : F.base, 'steady'); return; }
    set1(f, k, F.peak, 'ramp');
    let sus = F.sus; if (F.ft === 6) sus = vowelAt(F.sus).f[k];
    f.frequency.setTargetAtTime(sus, t + S['syn.env.f.att'] / 1000, S['syn.env.f.dec'] / 3000);
  });
}
function noteOn(notes, vel = .9, t, legato) {
  audio(); if (ac.state === 'suspended') ac.resume();
  t = t ?? ac.currentTime;
  if (!voice || voice.notes.length !== notes.length) { const old = voice; voice = makeVoice(notes); if (old) killVoice(old); legato = false; }
  const glide = gateOpen ? S['syn.glide'] / 1000 : 0;
  setPitches(notes, t, glide);
  const retrig = !(legato && gateOpen);
  gateOpen = true; curNotes = notes; velNow = vel;
  const dyn = S['syn.dyn'] / 100, peak = .9 * (1 - dyn + dyn * vel);
  const g = voice.amp.gain;
  if (retrig) { g.cancelScheduledValues(t); g.setValueAtTime(g.value, t); g.linearRampToValueAtTime(peak, t + S['syn.env.a.att'] / 1000);
    g.setTargetAtTime(peak * S['syn.env.a.sus'] / 100, t + S['syn.env.a.att'] / 1000, S['syn.env.a.dec'] / 3000);
    applyFilter(t, notes[0], vel);
    if (voice.lfoGain && S['syn.lfo.fade'] > 0) { voice.lfoGain.gain.cancelScheduledValues(t); voice.lfoGain.gain.setValueAtTime(0, t); voice.lfoGain.gain.linearRampToValueAtTime(voice.lfoAmt, t + S['syn.lfo.fade'] / 1000); } }
  live.note = notes[0]; live.src = srcName;
}
function noteOff(t) {
  if (!voice) return; t = t ?? ac.currentTime; gateOpen = false;
  const g = voice.amp.gain; g.cancelScheduledValues(t); g.setValueAtTime(g.value, t); g.setTargetAtTime(0, t, S['syn.env.a.rel'] / 3000);
  voice.filters.forEach((f, k) => { f.frequency.cancelScheduledValues(t); let b = filterTargets(curNotes ? curNotes[0] : 40, velNow).base; if (Math.round(S['syn.flt.type']) === 6) b = vowelAt(b).f[k]; f.frequency.setTargetAtTime(b, t, S['syn.env.f.rel'] / 3000); });
}
let rebuildT = null;
onChange((id, v, from) => {
  if (!ac || !voice || !id.startsWith('syn.')) return;
  if (id === 'syn.hold' && !on('syn.hold') && held === null) { stopArp(); noteOff(); }
  clearTimeout(rebuildT);
  rebuildT = setTimeout(() => { if (!voice) return; const old = voice; voice = makeVoice(curNotes || old.notes); setPitches(curNotes || old.notes, ac.currentTime, 0);
    if (gateOpen) { voice.amp.gain.setValueAtTime(.9 * S['syn.env.a.sus'] / 100, ac.currentTime); applyFilter(ac.currentTime, (curNotes || [40])[0], velNow, true); }
    killVoice(old, .03); }, 50);
});

/* arpeggiator (browser) */
const RATE_BEATS = [1, .5, 1 / 3, .25, 1 / 6, .125];
let arp = null, held = null, srcName = 'KEYS';
const live = { note: null, src: '', step: -1 };
function arpList(rootNote) {
  const chord = Math.round(S['syn.chord']) ? chordNotes(rootNote) : [rootNote], oct = Math.round(S['syn.arp.oct']) + 1, list = [];
  for (let o = 0; o < oct; o++) chord.forEach(n => list.push(n + 12 * o));
  const pat = Math.round(S['syn.arp.pattern']);
  if (pat === 1) list.reverse();
  if (pat === 2 && list.length > 2) return list.concat(list.slice(1, -1).reverse());
  return list;
}
function startArp() {
  if (arp) return; audio();
  arp = { step: 0, idx: 0, next: ac.currentTime + .03 };
  const tick = () => {
    if (!arp) return;
    const len = RATE_BEATS[Math.round(S['syn.arp.rate'])] * 60 / bpm;
    while (arp.next < ac.currentTime + .12) {
      const n = Math.round(S['syn.arp.len']), si = arp.step % n, st = steps[si], t = arp.next + (arp.step % 2 ? S['syn.arp.swing'] / 100 * len * .5 : 0);
      const rootNote = held != null ? held : (on('syn.hold') ? arp.lastRoot : null);
      setTimeout(() => { live.step = si; }, Math.max(0, (t - ac.currentTime) * 1000));
      if (rootNote != null && st.on && Math.random() * 100 < st.chance) {
        arp.lastRoot = rootNote;
        const pat = Math.round(S['syn.arp.pattern']);
        let note;
        if (pat === 4) note = scaleTone(rootNote, st.note);
        else if (pat === 3) { const l = arpList(rootNote); note = l[Math.floor(Math.random() * l.length)]; }
        else { const l = arpList(rootNote); note = l[arp.idx % l.length]; arp.idx++; }
        note += 12 * st.oct;
        const vel = st.acc ? 1 : .7, r = st.ratchet, sub = len / r;
        for (let k = 0; k < r; k++) {
          const tk = t + k * sub;
          noteOn([note], vel, tk, st.tie && k === 0);
          const nx = steps[(arp.step + 1) % n];
          if (!(k === r - 1 && nx.tie && nx.on)) noteOff(tk + sub * S['syn.arp.gate'] / 100 * (r > 1 ? .9 : 1));
        }
      }
      arp.step++; arp.next += len;
    }
    arp.timer = setTimeout(tick, 25);
  };
  tick();
}
function stopArp() { if (!arp) return; clearTimeout(arp.timer); arp = null; live.step = -1; }
function press(m, src, vel = .9) {
  if (!powered && !inPlugin()) return;
  if (inPlugin()) { held = m; if (window.obsSynth.noteOut) window.obsSynth.noteOut(m, true, vel); return; }
  srcName = src || 'KEYS'; held = m;
  if (on('syn.arp.on')) { live.note = m; live.src = srcName; startArp(); }
  else noteOn(chordNotes(m), vel, undefined, on('syn.legato'));
}
function releaseKey(m) {
  if (inPlugin()) { if (held === m) held = null; if (window.obsSynth.noteOut) window.obsSynth.noteOut(m, false); return; }
  if (held !== m) return; held = null;
  if (on('syn.hold')) return;
  if (on('syn.arp.on')) { stopArp(); live.note = null; } else { noteOff(); live.note = null; }
}
onChange(id => { if (inPlugin() || !ac) return; if (id === 'syn.arp.on') { if (!on(id)) { stopArp(); if (held == null) noteOff(); } else if (held != null) { noteOff(); startArp(); } } });
addEventListener('keydown', e => { if (SP.style.display === 'none' || dev.classList.contains('page-aether') || e.repeat || e.ctrlKey || e.metaKey || e.altKey || /INPUT|TEXTAREA/.test(e.target.tagName)) return;
  const c = e.key.toLowerCase();
  if (c === 'z' || c === 'x') { e.preventDefault(); shiftOct(c === 'z' ? -12 : 12); return; }
  const vel = KB.vel === 'soft' ? .5 : KB.vel === 'hard' ? 1 : .85;
  if (KB.view === 'pads') { const i = PADKEYS.indexOf(c); if (i < 0) return; e.preventDefault(); kbOn('key:' + c, padNote(i), vel); return; }
  const o = KEYMAP[c]; if (o == null) return; e.preventDefault();
  kbOn('key:' + c, KB.oct + 12 + o, vel); });
addEventListener('keyup', e => { const c = e.key.toLowerCase(); if (KEYMAP[c] == null && !PADKEYS.includes(c)) return; kbOff('key:' + c); });
addEventListener('blur', () => allOff());
/* browser: show what the preview plays */
setInterval(() => { if (inPlugin() || window.__obsSynthLive) return; showNote(live.note, live.src, live.note != null ? .7 : 0); showStep(on('syn.arp.on') ? live.step : -1); }, 50);

/* ============================================================ plugin hooks */
window.obsSynth = {
  ids: DEFS.map(d => d[0]),
  defs: DEFS,
  read() { const o = {}; for (const id in P) if (isSyn(id) || isAtm(id)) o[id] = toN(id); return o; },
  write(vals) { quiet++; try { for (const id in vals) if (P[id]) set(id, toV(id, vals[id]), 'plugin'); } finally { quiet--; } },
  /* steps: 16 packed numbers (see SynthDefs.h) */
  getSteps() { return steps.map(packStep); },
  setSteps(list) { if (!Array.isArray(list) || list.length !== 16) return; steps = list.map(v => unpackStep(+v || 0)); drawLane(); },
  onSteps(fn) { stepListeners.push(fn); },
  defaultSteps() { return Array.from({ length: 16 }, (_, i) => packStep(STEP_DEF(i))); },
  /* live state from the engine: { note: midi or -1, step: -1..15, level 0..1, src: 'guitar'|'midi'|'hold' } */
  live(l) { window.__obsSynthLive = true; if (!l) return; showNote(l.note >= 0 ? l.note : null, l.src, l.level); showStep(on('syn.arp.on') ? l.step : -1); },
  setBpm(b) { if (b > 20 && b < 400) bpm = b; },
  noteOut: null,              // set by the bridge: (midi, isOn) -> engine
  powerOut: null,             // set by the bridge: turns the synth section on
  setPower(isOn) { if (!!isOn !== powered) setPowerUi(isOn); },
  /* AETHER's meters from the engine: { act: [bed, drone, texture] 0..1, ev: texture event count } */
  atmLive(m) { if (kit.aetherLive) kit.aetherLive(m); },
  dice: roll,
  reset: resetAll,
  presets: PRESETS,
  loadPreset,
};

/* ------------------------------------------------------------ AETHER: the second page (aether.js builds it on this device) */
Object.assign(kit, { el, P, S, set, onChange, on, fmt, toN, toV, clamp, sg, knob, toggle, chip, seg, pill, mknob, sel, pushBtn, row, spacer, section, IC, hidpi, tc, accent, FONT, dev, SP, root,
  PWR_IC, stripGroups, presetValue, NOTE_NAMES, catCol, horCls, isPowered: () => powered, audio: () => audio(), ctx: () => ac, out: () => comp, inPlugin });
const pageSw = el('div', 'sy-page', '<button data-p="siren" title="SIREN: the synth your guitar plays"><i></i>SIREN</button><button data-p="aether" title="AETHER: noise, ambience and atmospheres around your playing"><i></i>AETHER</button>');
dev.appendChild(pageSw);
let page = 'siren';
try { if (sessionStorage.getItem('obs.synthPage') === 'aether') page = 'aether'; } catch (_) {}
function showPage(p) {
  page = p === 'aether' && kit.hasAether ? 'aether' : 'siren';
  dev.classList.toggle('page-aether', page === 'aether');
  [...pageSw.children].forEach(b => b.classList.toggle('on', b.dataset.p === page));
  pop.classList.remove('show'); prOpen(false);
  try { sessionStorage.setItem('obs.synthPage', page); } catch (_) {}
  if (kit.pageShown) kit.pageShown(page);
  if (page === 'siren') setTimeout(redrawAll, 20);
}
pageSw.addEventListener('click', e => { const b = e.target.closest('button'); if (b) showPage(b.dataset.p); });
pageDots = () => { pageSw.children[0].firstChild.classList.toggle('lit', powered); pageSw.children[1].firstChild.classList.toggle('lit', on('atm.on')); };
onChange(id => { if (id === 'atm.on') pageDots(); });
kit.showPage = showPage; kit.page = () => page;
window.obsSynth.showPage = showPage;
window.__sirenKit = kit;

/* ------------------------------------------------------------ start */
buildEnvKnobs(); showTab(0); tabDots(); showMot(0); motDots(); drawLane();
setTimeout(redrawAll, 60);
setTimeout(() => { showPage(page); pageDots(); }, 0);       // once aether.js (the next script) has built its page
})();
</script>
