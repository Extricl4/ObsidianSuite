<script>
/* =============================================================================
   OBSIDIAN SYNTH: the SYNTH page.
   Played by the guitar (or a MIDI keyboard). Signal flows left to right:
     PLAY -> SOUND (osc A, osc B, sub + noise) -> FILTER -> ENVELOPE
   and below: ARPEGGIATOR, LFO, OUTPUT.
   * In the plugin, plugin/WebUI/obsidian-bridge.js connects it to the engine
     through window.obsSynth (read / write / steps / live / noteOut).
   * In a browser it plays by itself (Web Audio): keys A-K, or AUDITION.
   The parameter list mirrors plugin/Source/DSP/SynthDefs.h.
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
.sy-root { position: absolute; inset: 0; display: flex; align-items: flex-start; justify-content: center; padding-top: 10px; user-select: none; }
.sy-panel { width: 1316px; height: 570px; box-sizing: border-box; padding: 16px; border-radius: 20px; position: relative; display: flex; flex-direction: column; gap: 12px;
  background: linear-gradient(#1a1a1d, #131315); box-shadow: inset 0 1px 0 rgba(255,255,255,.07), inset 0 0 0 1px rgba(255,255,255,.04), 0 24px 48px rgba(0,0,0,.5); }
.sy-row { display: flex; gap: 14px; flex: none; }
.sy-row2 { gap: 10px; }
.sy-card { position: relative; box-sizing: border-box; border-radius: 14px; padding: 11px 13px 12px; display: flex; flex-direction: column; min-width: 0;
  background: #0e0e10; box-shadow: inset 0 1px 0 rgba(0,0,0,.6), inset 0 0 0 1px rgba(255,255,255,.04); }
.sy-card.flow::after { content: ''; position: absolute; right: -11px; top: 132px; width: 7px; height: 7px; border-right: 1.5px solid rgba(233,230,223,.28); border-top: 1.5px solid rgba(233,230,223,.28); transform: rotate(45deg); }
.sy-card > header { display: flex; align-items: center; gap: 8px; height: 22px; margin-bottom: 10px; flex: none; }
.sy-card h3 { margin: 0; font-size: 10.5px; font-weight: 600; letter-spacing: .24em; color: #e9e6df; white-space: nowrap; }
.sy-sp { flex: 1; }
.sy-off .sy-body { opacity: .38; filter: grayscale(1); transition: opacity .15s; }
.sy-body { display: flex; flex-direction: column; flex: 1; min-height: 0; transition: opacity .15s; }
.sy-scr { position: relative; flex: none; border-radius: 10px; overflow: hidden; background: #09090a; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05), inset 0 6px 18px rgba(0,0,0,.6); }
.sy-scr canvas { display: block; width: 100%; height: 100%; }
.sy-knobs { display: flex; justify-content: space-around; align-items: flex-start; flex: none; }
.sy-lbl { font-size: 8.5px; font-weight: 600; letter-spacing: .2em; color: #6f6f76; white-space: nowrap; }
/* knob */
.sy-ctl { display: flex; flex-direction: column; align-items: center; min-width: 0; }
.sy-kn { position: relative; flex: none; cursor: ns-resize; touch-action: none; border-radius: 50%; }
.sy-kn > i { position: absolute; border-radius: 50%; }
.sy-kn .a { inset: 0; -webkit-mask: radial-gradient(closest-side, transparent calc(100% - 3.4px), #000 calc(100% - 2.8px), #000 calc(100% - .5px), transparent 100%); mask: radial-gradient(closest-side, transparent calc(100% - 3.4px), #000 calc(100% - 2.8px), #000 calc(100% - .5px), transparent 100%); }
.sy-kn .c { inset: 16%; background: repeating-conic-gradient(#2e2e33 0deg 3deg, #151517 3deg 6deg); box-shadow: 0 4px 8px rgba(0,0,0,.8), 0 1px 2px rgba(0,0,0,.9); }
.sy-kn .c > i { position: absolute; inset: 6%; border-radius: 50%; background: radial-gradient(circle at 38% 22%, rgba(255,255,255,.1), transparent 45%), radial-gradient(circle at 50% 15%, #47474d 0%, #212124 55%, #131315 100%); box-shadow: inset 0 1px 0 rgba(255,255,255,.2), inset 0 -3px 6px rgba(0,0,0,.6); }
.sy-kn .c > i > b { position: absolute; inset: 0; }
.sy-kn .c > i > b::after { content: ''; position: absolute; left: 50%; top: 7%; width: 2.4px; height: 30%; margin-left: -1.2px; border-radius: 2px; background: #f4f1ea; }
.sy-kn:hover .c { box-shadow: 0 4px 8px rgba(0,0,0,.8), 0 0 0 1px rgba(255,255,255,.07); }
.sy-kn.drag .c > i { background: radial-gradient(circle at 38% 22%, rgba(255,255,255,.14), transparent 45%), radial-gradient(circle at 50% 15%, #4f4f55 0%, #252528 55%, #141416 100%); }
.sy-kl { font-size: 8.5px; font-weight: 600; letter-spacing: .18em; color: #a3a3a8; margin-top: 5px; white-space: nowrap; }
.sy-kv { font-size: 10.5px; color: #e9e6df; font-variant-numeric: tabular-nums; white-space: nowrap; margin-top: 2px; }
/* switches */
.sy-tg { width: 30px; height: 16px; flex: none; border-radius: 9px; border: none; padding: 0; position: relative; cursor: pointer; background: #2a2a2f; box-shadow: inset 0 1px 2px rgba(0,0,0,.6); transition: background .15s; }
.sy-tg i { position: absolute; top: 2px; left: 2px; width: 12px; height: 12px; border-radius: 50%; background: #7a7a80; transition: transform .15s, background .15s; }
.sy-tg.on { background: var(--accent); }
.sy-tg.on i { transform: translateX(14px); background: #111113; }
.sy-seg { display: flex; flex: none; gap: 2px; padding: 3px; border-radius: 9px; background: #0a0a0b; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05); }
.sy-seg button { flex: 1; min-width: 0; height: 24px; padding: 0 5px; border: none; border-radius: 7px; background: none; color: #6f6f76; font: inherit; font-size: 9px; font-weight: 600; letter-spacing: .08em;
  cursor: pointer; display: flex; align-items: center; justify-content: center; white-space: nowrap; transition: background .12s, color .12s; }
.sy-seg button:hover { color: #e9e6df; }
.sy-seg button.on { color: #e9e6df; background: linear-gradient(#36363c, #26262a); box-shadow: inset 0 1px 0 rgba(255,255,255,.1), 0 1px 2px rgba(0,0,0,.5); }
.sy-seg.small button { height: 20px; font-size: 8.5px; }
.sy-chip { height: 22px; padding: 0 9px; flex: none; border-radius: 7px; border: 1px solid rgba(255,255,255,.1); background: none; color: #8d8d93; font: inherit; font-size: 8.5px; font-weight: 600; letter-spacing: .16em; cursor: pointer; white-space: nowrap; display: flex; align-items: center; gap: 6px; transition: background .12s, color .12s; }
.sy-chip:hover { color: #e9e6df; border-color: rgba(255,255,255,.18); }
.sy-chip.on { background: var(--accent); border-color: transparent; color: #111113; }
.sy-chip svg { width: 12px; height: 12px; }
.sy-pill { height: 26px; flex: none; display: flex; align-items: center; gap: 8px; padding: 0 10px; border-radius: 8px; background: #0a0a0b; box-shadow: inset 0 0 0 1px rgba(255,255,255,.06); cursor: ns-resize; touch-action: none; white-space: nowrap; }
.sy-pill:hover { box-shadow: inset 0 0 0 1px rgba(255,255,255,.14); }
.sy-pill .l { font-size: 8.5px; font-weight: 600; letter-spacing: .18em; color: #6f6f76; }
.sy-pill .v { font-size: 10.5px; color: #e9e6df; font-variant-numeric: tabular-nums; }
.sy-pill.cyc { cursor: pointer; }
.sy-stepper { height: 26px; display: flex; align-items: center; border-radius: 8px; background: #0a0a0b; box-shadow: inset 0 0 0 1px rgba(255,255,255,.06); flex: none; }
.sy-stepper button { width: 26px; height: 26px; border: none; background: none; color: #8d8d93; font: inherit; font-size: 14px; cursor: pointer; padding: 0; }
.sy-stepper button:hover { color: #e9e6df; }
.sy-stepper span { flex: 1; text-align: center; font-size: 10.5px; color: #e9e6df; white-space: nowrap; }
.sy-stepper span b { font-size: 8.5px; font-weight: 600; letter-spacing: .18em; color: #6f6f76; margin-right: 8px; }
/* sound tabs */
.sy-tabs { display: flex; gap: 4px; }
.sy-tab { height: 24px; padding: 0 10px; border: none; border-radius: 7px; background: none; color: #6f6f76; font: inherit; font-size: 9.5px; font-weight: 600; letter-spacing: .16em; cursor: pointer; display: flex; align-items: center; gap: 7px; white-space: nowrap; }
.sy-tab:hover { color: #e9e6df; }
.sy-tab.on { color: #e9e6df; background: #1d1d21; box-shadow: inset 0 0 0 1px rgba(255,255,255,.07); }
.sy-tab i { width: 6px; height: 6px; border-radius: 50%; background: #3a3a40; }
.sy-tab i.lit { background: var(--accent); box-shadow: 0 0 6px var(--accent); }
.sy-tab svg { opacity: .7; }
/* note display */
.sy-note { height: 92px; display: flex; flex-direction: column; align-items: center; justify-content: center; }
.sy-note .n { font-size: 30px; font-weight: 600; letter-spacing: .02em; color: #e9e6df; line-height: 1; }
.sy-note .n small { font-size: 14px; font-weight: 500; color: #8d8d93; margin-left: 2px; }
.sy-note .s { margin-top: 7px; font-size: 8.5px; font-weight: 600; letter-spacing: .22em; color: #6f6f76; }
.sy-note .bar { position: absolute; left: 14px; right: 14px; bottom: 9px; height: 3px; border-radius: 2px; background: #1b1b1e; overflow: hidden; }
.sy-note .bar i { position: absolute; left: 0; top: 0; bottom: 0; width: 0; background: var(--accent); border-radius: 2px; transition: width .08s; }
.sy-note .tun { position: absolute; right: 8px; top: 7px; font-size: 8.5px; color: #6f6f76; letter-spacing: .08em; cursor: ns-resize; padding: 2px 4px; border-radius: 4px; }
.sy-note .tun:hover { color: #e9e6df; background: #161618; }
/* arp lane */
.sy-lane { display: grid; grid-template-columns: repeat(16, 1fr); gap: 4px; height: 86px; flex: none; touch-action: none; }
.sy-st { position: relative; display: flex; flex-direction: column; gap: 4px; min-width: 0; cursor: pointer; }
.sy-st .cell { position: relative; flex: 1; border-radius: 5px; background: #19191c; box-shadow: inset 0 0 0 1px rgba(255,255,255,.04); overflow: hidden; }
.sy-st .cell > i { position: absolute; left: 0; right: 0; bottom: 0; border-radius: 4px; background: linear-gradient(#e9e6df, #b9b5ad); }
.sy-st .cell > i.dim { background: linear-gradient(#5a5a60, #3e3e44); }
.sy-st .cell > b { position: absolute; left: 0; right: 0; top: 4px; text-align: center; font-size: 8.5px; font-weight: 600; color: #111113; pointer-events: none; }
.sy-st .cell > b.light { color: #a3a3a8; }
.sy-st .cell.tie::before { content: ''; position: absolute; left: -6px; top: 50%; width: 10px; height: 3px; margin-top: -1.5px; background: var(--accent); border-radius: 2px; z-index: 1; }
.sy-st .num { height: 10px; font-size: 8px; color: #6f6f76; text-align: center; }
.sy-st.out { opacity: .28; }
.sy-st.play .cell { box-shadow: inset 0 0 0 1px var(--accent), 0 0 10px rgba(255,255,255,.08); }
.sy-st.beat .num { color: #a3a3a8; }
/* small icon buttons */
.sy-ib { width: 26px; height: 22px; border-radius: 7px; border: 1px solid rgba(255,255,255,.1); background: none; color: #8d8d93; cursor: pointer; display: flex; align-items: center; justify-content: center; padding: 0; flex: none; }
.sy-ib:hover { color: #e9e6df; }
.sy-ib:disabled { opacity: .3; cursor: default; }
.sy-play { height: 30px; padding: 0 14px; border-radius: 9px; border: none; background: linear-gradient(#2c2c31, #1f1f23); color: #e9e6df; font: inherit; font-size: 9.5px; font-weight: 600; letter-spacing: .18em; cursor: pointer; display: flex; align-items: center; gap: 8px; box-shadow: inset 0 1px 0 rgba(255,255,255,.08), 0 2px 4px rgba(0,0,0,.4); }
.sy-play.on { background: var(--accent); color: #111113; }
.sy-play svg { width: 10px; height: 10px; }
.sy-hint { font-size: 9px; color: #6f6f76; letter-spacing: .04em; }
.sy-hint kbd { font: inherit; font-size: 8.5px; padding: 1px 4px; border-radius: 4px; background: #1d1d20; color: #a3a3a8; }
.sy-flash { animation: syFlash .5s ease-out; }
.sy-offmask { position: absolute; inset: 0; border-radius: 20px; z-index: 6; display: none; align-items: center; justify-content: center; background: rgba(10,10,11,.55); backdrop-filter: grayscale(.7) blur(1px); -webkit-backdrop-filter: grayscale(.7) blur(1px); }
.sy-offmask.show { display: flex; }
.sy-offcard { display: flex; flex-direction: column; align-items: center; gap: 12px; padding: 22px 30px; border-radius: 16px; background: linear-gradient(#1d1d21, #141416); box-shadow: inset 0 1px 0 rgba(255,255,255,.08), inset 0 0 0 1px rgba(255,255,255,.06), 0 20px 40px rgba(0,0,0,.6); }
.sy-offcard b { font-size: 13px; font-weight: 600; letter-spacing: .24em; color: #e9e6df; }
.sy-offcard span { font-size: 11px; color: #8d8d93; }
.sy-offcard button { height: 32px; padding: 0 18px; border: none; border-radius: 9px; background: var(--accent); color: #111113; font: inherit; font-size: 10px; font-weight: 600; letter-spacing: .18em; cursor: pointer; }
@keyframes syFlash { 0% { box-shadow: inset 0 0 0 1px var(--accent), 0 0 24px rgba(255,255,255,.12); } 100% { box-shadow: inset 0 1px 0 rgba(0,0,0,.6), inset 0 0 0 1px rgba(255,255,255,.04); } }
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
  play: '<svg viewBox="0 0 10 10"><path d="M2 1l7 4-7 4z" fill="currentColor"/></svg>',
  stop: '<svg viewBox="0 0 10 10"><rect x="2" y="2" width="6" height="6" rx="1" fill="currentColor"/></svg>',
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
      a.style.background = 'conic-gradient(from 225deg, #2b2b30 0deg ' + lo + 'deg, var(--accent) ' + lo + 'deg ' + hi + 'deg, #2b2b30 ' + hi + 'deg 270deg, transparent 270deg 360deg)'; }
    else a.style.background = 'conic-gradient(from 225deg, var(--accent) 0deg ' + deg + 'deg, #2b2b30 ' + deg + 'deg 270deg, transparent 270deg 360deg)';
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
function card(title, w, h, cls) {
  const c = el('section', 'sy-card' + (cls ? ' ' + cls : '')); c.style.width = w + 'px'; c.style.height = h + 'px';
  const hd = el('header'); hd.appendChild(el('h3', null, title)); c.appendChild(hd);
  const body = el('div', 'sy-body'); c.appendChild(body); c._h = hd; c._b = body; return c;
}
const spacer = () => el('span', 'sy-sp');
const gapV = h => { const d = el('div'); d.style.height = h + 'px'; d.style.flex = 'none'; return d; };
function row(children, style) { const r = el('div'); r.style.cssText = 'display: flex; align-items: center; gap: 8px; flex: none;' + (style || ''); children.forEach(c => c && r.appendChild(c)); return r; }
function dimWhen(cardEl, id) { const d = () => cardEl.classList.toggle('sy-off', !on(id)); d(); onChange(i => { if (i === id) d(); }); }

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

/* ============================================================ the page */
SP.replaceChildren();
SP.style.fontFamily = 'inherit';
const rootEl = el('div', 'sy-root'), panel = el('div', 'sy-panel');
rootEl.appendChild(panel); SP.appendChild(rootEl);
const row1 = el('div', 'sy-row'), row2 = el('div', 'sy-row sy-row2');
panel.append(row1, row2);

/* ---- PLAY ---- */
const cPlay = card('PLAY', 210, 302, 'flow');
cPlay._h.append(spacer(), chip('syn.legato', 'LEGATO', 'Legato: new notes glide without restarting the envelopes'), chip('syn.hold', 'HOLD', 'Hold: the last note keeps playing after you stop (a drone to play over)'));
const noteScr = el('div', 'sy-scr sy-note', '<div class="n">–</div><div class="s">PLAY A NOTE</div><div class="tun" title="Tuning reference. Drag to change, double-click for 440 Hz"></div><div class="bar"><i></i></div>');
noteScr.style.height = '92px';
const tunEl = noteScr.querySelector('.tun'); tunEl.dataset.param = 'syn.tune';
const drawTun = () => { tunEl.textContent = 'A ' + Math.round(S['syn.tune']) + ' Hz'; }; drawTun(); onChange(i => { if (i === 'syn.tune') drawTun(); });
{ let d = null; tunEl.addEventListener('pointerdown', e => { e.preventDefault(); tunEl.setPointerCapture(e.pointerId); d = { y: e.clientY, v: S['syn.tune'] }; });
  tunEl.addEventListener('pointermove', e => { if (d) set('syn.tune', Math.round(d.v + (d.y - e.clientY) / 6), 'ui'); });
  tunEl.addEventListener('pointerup', () => { d = null; }); tunEl.addEventListener('dblclick', () => set('syn.tune', 440, 'ui')); }
cPlay._b.append(noteScr, gapV(12),
  (() => { const k = el('div', 'sy-knobs'); k.append(knob('syn.gate', { label: 'THRESH', size: 34, tip: 'How loud the guitar must be before the synth plays' }), knob('syn.sens', { label: 'SENS', size: 34, tip: 'Input sensitivity: how strongly the synth reacts to your playing' }), knob('syn.glide', { label: 'GLIDE', size: 34, tip: 'Slide time between notes' }), knob('syn.dyn', { label: 'DYN', size: 34, tip: 'Dynamics: how much picking harder makes it louder and brighter' })); return k; })(),
  gapV(12), stepper('syn.chord', 'CHORD', 'Play one note, hear a chord. Triad, 7th and 9th follow the key and scale.'), gapV(8),
  row([pill('syn.key', 'KEY', { tip: 'Key for chords and the arpeggiator. Click to change, shift-click to go back.' }), pill('syn.scale', 'SCALE', { tip: 'Scale for chords and the arpeggiator' })], 'justify-content: space-between'));
cPlay._b.lastChild.children[0].style.flex = '0 0 66px'; cPlay._b.lastChild.children[1].style.flex = '1';
row1.appendChild(cPlay);

/* ---- SOUND: A / B / SUB tabs ---- */
const cSnd = card('', 446, 302, 'flow');
cSnd._h.replaceChildren();
let tab = 0;
const tabs = el('div', 'sy-tabs');
const TAB_NAMES = ['OSC A', 'OSC B', 'SUB · NOISE'];
TAB_NAMES.forEach((n, i) => { const b = el('button', 'sy-tab', '<i></i>' + n); b.onclick = () => showTab(i); tabs.appendChild(b); });
const tabRight = el('div'); tabRight.style.cssText = 'display: flex; align-items: center; gap: 8px';
cSnd._h.append(tabs, spacer(), tabRight);
const sndBody = cSnd._b;
const oscViews = ['a', 'b'].map(x => {
  const v = el('div'); v.style.cssText = 'display: flex; flex-direction: column; flex: 1';
  const sc = el('div', 'sy-scr'); sc.style.height = '72px'; const cv = el('canvas'); sc.appendChild(cv);
  v._cv = cv;
  v.append(seg('syn.' + x + '.wave', null, { icons: WAVE_IC }), gapV(8), sc, gapV(10));
  const k = el('div', 'sy-knobs');
  k.append(knob('syn.' + x + '.pitch', { label: 'PITCH', tip: 'Pitch in semitones (12 = one octave)' }), knob('syn.' + x + '.fine', { label: 'FINE', tip: 'Fine tune in cents' }),
    knob('syn.' + x + '.shape', { label: 'SHAPE', tip: 'Saw: adds a second saw for a hollow, brighter tone · Pulse: pulse width · Triangle: wavefolding · Sine: FM growl · Super: side voices' }),
    knob('syn.' + x + '.level', { label: 'LEVEL' }));
  v.appendChild(k); v.appendChild(gapV(10));
  const uni = row([el('span', 'sy-lbl', 'UNISON'), seg('syn.' + x + '.voices', null, { small: true, width: 112 }), pill('syn.' + x + '.detune', 'DETUNE', { tip: 'How far the unison voices spread apart' }), pill('syn.' + x + '.width', 'WIDTH', { tip: 'Stereo spread of the unison voices' })], 'justify-content: space-between');
  v.appendChild(uni);
  const supr = () => { const isSuper = Math.round(S['syn.' + x + '.wave']) === 4; uni.children[1].style.opacity = isSuper ? .35 : ''; uni.children[1].title = isSuper ? 'Super always uses 7 voices' : ''; };
  supr(); onChange(i => { if (i === 'syn.' + x + '.wave') supr(); });
  return v;
});
const subView = (() => {
  const v = el('div'); v.style.cssText = 'display: flex; flex-direction: column; flex: 1';
  const sc = el('div', 'sy-scr'); sc.style.height = '72px'; const cv = el('canvas'); sc.appendChild(cv); v._cv = cv;
  const cols = el('div'); cols.style.cssText = 'display: flex; gap: 12px; flex: 1; margin-top: 12px';
  const mk = (title, kids) => { const c = el('div'); c.style.cssText = 'flex: 1; display: flex; flex-direction: column; align-items: center; gap: 10px; padding: 10px 8px; border-radius: 10px; background: #121214; box-shadow: inset 0 0 0 1px rgba(255,255,255,.035)';
    c.appendChild(el('span', 'sy-lbl', title)); kids.forEach(k => c.appendChild(k)); return c; };
  cols.append(
    mk('SUB OSCILLATOR', [knob('syn.sub.level', { label: 'LEVEL', tip: 'A clean sub one or two octaves below' }), row([seg('syn.sub.wave', ['SINE', 'SQUARE'], { small: true, width: 104 }), seg('syn.sub.oct', [MINUS + '1 OCT', MINUS + '2 OCT'], { small: true, width: 104 })])]),
    mk('NOISE', [row([knob('syn.noise.level', { label: 'LEVEL', tip: 'Breath and grit' }), knob('syn.noise.color', { label: 'COLOR' })], 'gap: 18px')]));
  v.append(sc, cols);
  return v;
})();
const bSync = chip('syn.b.sync', 'SYNC', 'Hard sync: osc B restarts with osc A. Sweep B\'s pitch for the classic screaming lead.');
const aOn = toggle('syn.a.on', 'Oscillator A on'), bOn = toggle('syn.b.on', 'Oscillator B on');
function showTab(i) {
  tab = i;
  [...tabs.children].forEach((b, k) => b.classList.toggle('on', k === i));
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
row1.appendChild(cSnd);

/* ---- FILTER ---- */
const cFlt = card('FILTER', 350, 302, 'flow');
cFlt._h.append(spacer(), toggle('syn.flt.on', 'Filter on'));
dimWhen(cFlt, 'syn.flt.on');
const fScr = el('div', 'sy-scr'); fScr.style.height = '96px'; fScr.style.cursor = 'crosshair'; const fCv = el('canvas'); fScr.appendChild(fCv);
fScr.title = 'Drag: left/right = cutoff, up/down = resonance';
fScr.dataset.param = 'syn.flt.cutoff';
cFlt._b.append(seg('syn.flt.type', ['LDR 24', 'LDR 12', 'LP', 'BP', 'HP', 'NOTCH', 'VOWEL']), gapV(8), fScr, gapV(10),
  (() => { const k = el('div', 'sy-knobs'); k.style.alignItems = 'flex-end';
    k.append(knob('syn.flt.cutoff', { label: 'CUTOFF', size: 46, tip: 'Brightness. With VOWEL it moves through A-E-I-O-U' }), knob('syn.flt.res', { label: 'RESONANCE' }), knob('syn.flt.drive', { label: 'DRIVE' }), knob('syn.flt.env', { label: 'ENV', tip: 'How far the filter envelope opens (or closes) the filter' })); return k; })(),
  gapV(8), row([pill('syn.flt.key', 'KEY TRACK', { tip: 'Higher notes open the filter more' })], 'justify-content: center'));
{ let d = null;
  const at = e => { const r = fScr.getBoundingClientRect(); return [clamp((e.clientX - r.left) / r.width, 0, 1), clamp(1 - (e.clientY - r.top) / r.height, 0, 1)]; };
  fScr.addEventListener('pointerdown', e => { if (e.button !== 0) return; e.preventDefault(); fScr.setPointerCapture(e.pointerId); d = 1; const [x, y] = at(e); set('syn.flt.cutoff', toV('syn.flt.cutoff', x), 'ui'); set('syn.flt.res', y * 100, 'ui'); });
  fScr.addEventListener('pointermove', e => { if (!d) return; const [x, y] = at(e); set('syn.flt.cutoff', toV('syn.flt.cutoff', x), 'ui'); set('syn.flt.res', clamp((y - .1) / .8, 0, 1) * 100, 'ui'); });
  fScr.addEventListener('pointerup', () => { d = null; }); }
row1.appendChild(cFlt);

/* ---- ENVELOPE (one graph, AMP / FILTER) ---- */
const cEnv = card('ENVELOPE', 236, 302);
let envSel = 0;
const envSeg = el('div', 'sy-seg small'); envSeg.style.width = '112px';
envSeg.innerHTML = '<button class="on">AMP</button><button>FILTER</button>';
cEnv._h.append(spacer(), envSeg);
const eScr = el('div', 'sy-scr'); eScr.style.height = '132px'; const eCv = el('canvas'); eScr.appendChild(eCv);
eScr.title = 'Drag the points';
const envKnobs = el('div', 'sy-knobs');
cEnv._b.append(eScr, gapV(12), envKnobs, gapV(6), el('div', 'sy-hint', ''));
cEnv._b.lastChild.style.textAlign = 'center';
const envIds = s => ['att', 'dec', 'sus', 'rel'].map(x => 'syn.env.' + s + '.' + x);
function buildEnvKnobs() {
  const ids = envIds(envSel ? 'f' : 'a');
  envKnobs.replaceChildren(...ids.map((id, i) => knob(id, { label: ['ATK', 'DEC', 'SUS', 'REL'][i], size: 32 })));
  cEnv._b.lastChild.textContent = envSel ? 'SHAPES THE FILTER OVER TIME' : 'SHAPES THE VOLUME OVER TIME';
  [...envSeg.children].forEach((b, i) => b.classList.toggle('on', i === envSel));
  drawEnv();
}
envSeg.addEventListener('click', e => { const b = e.target.closest('button'); if (!b) return; envSel = [...envSeg.children].indexOf(b); buildEnvKnobs(); });
row1.appendChild(cEnv);

/* ---- ARPEGGIATOR ---- */
const cArp = card('ARPEGGIATOR', 652, 222);
cArp._h.append(toggle('syn.arp.on', 'Arpeggiator on'), spacer(), seg('syn.arp.pattern', null, { icons: ['up', 'down', 'updn', 'shuffle', 'steps'], width: 186 }));
dimWhen(cArp, 'syn.arp.on');
const PROPS = [['STEPS', 'on'], ['ACCENT', 'acc'], ['TIE', 'tie'], ['OCTAVE', 'oct'], ['RATCHET', 'ratchet'], ['CHANCE', 'chance'], ['NOTE', 'note']];
let prop = 0;
const propSeg = el('div', 'sy-seg small');
propSeg.innerHTML = PROPS.map(([n], i) => '<button' + (i ? '' : ' class="on"') + '>' + n + '</button>').join('');
propSeg.addEventListener('click', e => { const b = e.target.closest('button'); if (!b) return; prop = [...propSeg.children].indexOf(b); [...propSeg.children].forEach((x, i) => x.classList.toggle('on', i === prop)); drawLane(); laneHint(); });
const lane = el('div', 'sy-lane');
lane.innerHTML = steps.map((_, i) => '<div class="sy-st' + (i % 4 === 0 ? ' beat' : '') + '" data-i="' + i + '"><div class="cell"><i></i><b></b></div><div class="num">' + (i + 1) + '</div></div>').join('');
const hint = el('span', 'sy-hint');
const laneHint = () => { hint.textContent = ['Click or drag across to turn steps on and off', 'Accented steps hit harder and brighter', 'A tied step slides on from the one before', 'Drag up or down: octave for each step', 'Drag up: repeat the note 2, 3 or 4 times in one step', 'Drag: how likely each step is to play', 'Drag: which scale note each step plays (with the STEPS pattern)'][prop]; };
cArp._b.append(row([propSeg, spacer(), hint]), gapV(8), lane, gapV(10),
  row([seg('syn.arp.rate', null, { small: true, width: 196 }), el('span', 'sy-lbl', 'OCT'), seg('syn.arp.oct', null, { small: true, width: 84 }), spacer(),
    pill('syn.arp.len', 'LENGTH', { fmt: id => Math.round(S[id]) }), pill('syn.arp.gate', 'GATE'), pill('syn.arp.swing', 'SWING')]));
laneHint();
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
row2.appendChild(cArp);

/* ---- LFO ---- */
const cLfo = card('LFO', 300, 222);
const lRate = knob('syn.lfo.rate', { label: 'RATE', size: 34 }), lDiv = knob('syn.lfo.div', { label: 'RATE', size: 34, tip: 'Rate in note values (synced to the tempo)' });
cLfo._h.append(spacer(), chip('syn.lfo.sync', 'SYNC', 'Lock the LFO to the tempo'));
const lScr = el('div', 'sy-scr'); lScr.style.height = '38px'; const lCv = el('canvas'); lScr.appendChild(lCv);
const lKnobs = el('div', 'sy-knobs');
cLfo._b.append(seg('syn.lfo.shape', null, { icons: ['sine', 'tri', 'saw', 'sq', 'rnd', 'drift'] }), gapV(7), lScr, gapV(8), lKnobs, gapV(8),
  seg('syn.lfo.target', ['VIBRATO', 'WAH', 'SHAPE', 'TREMOLO', 'PAN'], { small: true }));
const lK = () => lKnobs.replaceChildren(on('syn.lfo.sync') ? lDiv : lRate, knob('syn.lfo.depth', { label: 'DEPTH', size: 34 }), knob('syn.lfo.fade', { label: 'FADE IN', size: 34, tip: 'The LFO fades in after each new note (delayed vibrato)' }));
lK(); onChange(i => { if (i === 'syn.lfo.sync') lK(); });
row2.appendChild(cLfo);

/* ---- OUTPUT ---- */
const cOut = card('OUTPUT', 318, 222);
const dice = el('button', 'sy-chip', IC.dice + 'DICE'); dice.title = 'Roll a new sound. Keeps your mix, level, key and playing settings.';
const undo = el('button', 'sy-ib', IC.undo); undo.title = 'Undo the last dice roll'; undo.disabled = true;
cOut._h.append(spacer(), dice, undo);
const audition = el('button', 'sy-play', IC.play + '<b>AUDITION</b>');
audition.title = 'Hear the synth without playing (or use keys A to K)';
cOut._b.append(
  (() => { const k = el('div', 'sy-knobs'); k.append(knob('syn.level', { label: 'LEVEL' }), knob('syn.mix', { label: 'MIX', tip: 'Guitar and synth blend: 0% guitar only, 100% synth only' }), knob('syn.space', { label: 'SPACE', tip: 'Stereo echoes and room, just for the synth' })); return k; })(),
  gapV(10), row([el('span', 'sy-lbl', 'CHARACTER'), seg('syn.char', ['CLEAN', 'ANALOG', '8-BIT', 'VHS'], { small: true })], ''), gapV(10),
  row([el('span', 'sy-lbl', 'AMP'), segBool('syn.post', ['BEFORE', 'AFTER'], 'Does the synth go through the amp (BEFORE) or join after the cab (AFTER)?', 120), spacer(), audition]));
cOut._b.children[2].children[1].style.flex = '1';
row2.appendChild(cOut);

/* ------------------------------------------------------------ drawings */
function drawScope(cv, which) {
  const [g, w, h] = hidpi(cv); if (!g) return;
  g.strokeStyle = tc('e9e6df', .06); g.lineWidth = 1; g.beginPath(); g.moveTo(0, h / 2 + .5); g.lineTo(w, h / 2 + .5); g.stroke();
  const line = (fn, a, lw, glow) => { g.beginPath(); for (let x = 0; x <= w; x++) { const y = h / 2 - fn(x / w) * h * .34; x ? g.lineTo(x, y) : g.moveTo(x, y); }
    g.strokeStyle = a; g.lineWidth = lw; g.lineJoin = 'round'; g.shadowColor = glow ? accent(.35) : 'transparent'; g.shadowBlur = glow ? 8 : 0; g.stroke(); g.shadowBlur = 0; };
  if (which === 'sub') {
    const sq = Math.round(S['syn.sub.wave']) === 1, lv = S['syn.sub.level'] / 100, nz = S['syn.noise.level'] / 100;
    let seed = 7; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647 - .5;
    line(t => { const ph = (t * 1.5) % 1; return (sq ? (ph < .5 ? .8 : -.8) : Math.sin(ph * 2 * Math.PI)) * Math.max(lv, .05) + rnd() * nz * 1.4; }, lv + nz > 0 ? tc('e9e6df', 1) : tc('e9e6df', .25), 1.6, lv + nz > 0);
    return;
  }
  const x = which, onx = on('syn.' + x + '.on'), wv = Math.round(S['syn.' + x + '.wave']), sh = S['syn.' + x + '.shape'];
  const voices = wv === 4 ? 1 : [1, 3, 5, 7][Math.round(S['syn.' + x + '.voices'])], det = S['syn.' + x + '.detune'];
  for (let v = 0; v < voices; v++) { if (v === voices >> 1) continue; const off = (v - (voices - 1) / 2) * det / 1400;
    line(t => waveAt(wv, frac(t * 2 * (1 + off) + v * .07), sh), tc('e9e6df', .13), 1, false); }
  const sync = x === 'b' && on('syn.b.sync'), ratio = Math.pow(2, (S['syn.b.pitch'] - S['syn.a.pitch']) / 12);
  line(t => waveAt(wv, sync ? frac(frac(t * 2) * ratio) : frac(t * 2), sh), onx ? tc('e9e6df', 1) : tc('e9e6df', .25), 1.8, onx);
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
  const gr = g.createLinearGradient(0, 0, 0, h); gr.addColorStop(0, live ? accent(.16) : tc('e9e6df', .04)); gr.addColorStop(1, accent(0));
  g.beginPath(); g.moveTo(0, h); pts.forEach(([x, y]) => g.lineTo(x, y)); g.lineTo(w, h); g.closePath(); g.fillStyle = gr; g.fill();
  g.beginPath(); pts.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y));
  g.strokeStyle = live ? tc('e9e6df', 1) : tc('e9e6df', .3); g.lineWidth = 1.7; g.shadowColor = accent(.3); g.shadowBlur = live ? 8 : 0; g.stroke(); g.shadowBlur = 0;
  const nx = toN('syn.flt.cutoff') * w, ny = h - 8 - (S['syn.flt.res'] / 100 * .8 + .1) * (h - 16);
  g.fillStyle = accent(1); g.beginPath(); g.arc(nx, clamp(ny, 6, h - 6), 4.5, 0, 7); g.fill();
  g.strokeStyle = accent(.35); g.beginPath(); g.arc(nx, clamp(ny, 6, h - 6), 9, 0, 7); g.stroke();
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
  path(); g.lineTo(G.pad, G.base); const gr = g.createLinearGradient(0, 0, 0, h); gr.addColorStop(0, accent(.16)); gr.addColorStop(1, accent(0)); g.fillStyle = gr; g.fill();
  path(); g.strokeStyle = tc('e9e6df', 1); g.lineWidth = 1.7; g.stroke();
  g.setLineDash([2, 3]); g.strokeStyle = tc('e9e6df', .15); g.beginPath(); g.moveTo(G.sx + .5, G.top); g.lineTo(G.sx + .5, G.base); g.stroke(); g.setLineDash([]);
  [[G.ax, G.top], [G.dx, G.sy], [G.rx, G.base]].forEach(([x, y], i) => { g.fillStyle = i === envHot ? accent(1) : tc('e9e6df', 1); g.beginPath(); g.arc(x, y, i === envHot ? 4.5 : 3.6, 0, 7); g.fill(); });
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
  g.strokeStyle = S['syn.lfo.depth'] > 0 ? tc('e9e6df', 1) : tc('e9e6df', .3); g.lineWidth = 1.6; g.stroke();
  const ph = ((performance.now() - lfoT0) / 1000 * Math.min(lfoHz(), 8)) % 1, x = ph * w / 2;
  g.fillStyle = accent(1); g.beginPath(); g.arc(x, h / 2 - lfoVal(sh, ph) * h * amp, 3.2, 0, 7); g.fill();
}

/* everything visible, redrawn when the page shows or the theme changes */
let lastShown = false;
function redrawAll() { drawScopes(); drawFilter(); drawEnv(); drawLfo(); drawLane(); }
(function loop() {
  const shown = SP.style.display !== 'none' && SP.offsetWidth > 0;
  if (shown && !lastShown) redrawAll();
  if (shown) drawLfo();
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
let undoState = null;
const rr = (a, b) => a + Math.random() * (b - a), ri = (a, b) => Math.floor(rr(a, b + 1)), pick = a => a[Math.floor(Math.random() * a.length)];
const wpick = ws => { let t = Math.random() * ws.reduce((x, y) => x + y, 0); for (let i = 0; i < ws.length; i++) { t -= ws[i]; if (t <= 0) return i; } return ws.length - 1; };
const KEEP = ['syn.sens', 'syn.gate', 'syn.dyn', 'syn.key', 'syn.scale', 'syn.tune', 'syn.mix', 'syn.level', 'syn.post'];
function roll() {
  undoState = { S: { ...S }, steps: steps.map(s => ({ ...s })) };
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
  undo.disabled = false;
  panel.classList.remove('sy-flash'); void panel.offsetWidth;
  [cSnd, cFlt, cEnv].forEach(c => { c.classList.remove('sy-flash'); void c.offsetWidth; c.classList.add('sy-flash'); });
}
dice.addEventListener('click', roll);
undo.addEventListener('click', () => { if (!undoState) return; for (const id in undoState.S) set(id, undoState.S[id], 'dice'); steps = undoState.steps; drawLane(); stepsChanged('dice'); undoState = null; undo.disabled = true; });

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
  if (!ac || !voice) return;
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
function press(m, src) {
  if (inPlugin()) { held = m; if (window.obsSynth.noteOut) window.obsSynth.noteOut(m, true); return; }
  srcName = src || 'KEYS'; held = m;
  if (on('syn.arp.on')) { live.note = m; live.src = srcName; startArp(); }
  else noteOn(chordNotes(m), .9, undefined, on('syn.legato'));
}
function releaseKey(m) {
  if (inPlugin()) { if (held === m) held = null; if (window.obsSynth.noteOut) window.obsSynth.noteOut(m, false); return; }
  if (held !== m) return; held = null;
  if (on('syn.hold')) return;
  if (on('syn.arp.on')) { stopArp(); live.note = null; } else { noteOff(); live.note = null; }
}
onChange(id => { if (inPlugin() || !ac) return; if (id === 'syn.arp.on') { if (!on(id)) { stopArp(); if (held == null) noteOff(); } else if (held != null) { noteOff(); startArp(); } } });
/* AUDITION: a short riff the way a guitarist would play it */
let demoT = null;
function demo(onOff) {
  audition.classList.toggle('on', onOff); audition.innerHTML = (onOff ? IC.stop : IC.play) + '<b>' + (onOff ? 'STOP' : 'AUDITION') + '</b>';
  if (!onOff) { clearTimeout(demoT); demoT = null; if (held != null) releaseKey(held); if (!inPlugin()) { stopArp(); noteOff(); live.note = null; } return; }
  const riff = [40, 43, 45, 47, 40, 50, 48, 45]; let i = 0;
  const next = () => { const beat = 60 / bpm; if (held != null) releaseKey(held);
    const m = riff[i % riff.length]; press(m, 'AUDITION'); i++;
    const dur = on('syn.arp.on') ? beat * 2 : beat * (i % 2 ? 1.5 : .5);
    demoT = setTimeout(() => { if (!on('syn.arp.on')) releaseKey(m); demoT = setTimeout(next, on('syn.arp.on') ? 0 : beat * 1000 * .02); }, dur * 1000 * .96); };
  next();
}
audition.addEventListener('click', () => demo(!demoT));
const KEYS = { a: 40, w: 41, s: 42, e: 43, d: 43, f: 45, t: 46, g: 47, h: 48, u: 49, j: 50, k: 52, o: 51, l: 53 };
const down = new Set();
addEventListener('keydown', e => { if (SP.style.display === 'none' || e.repeat || e.ctrlKey || e.metaKey || e.altKey || /INPUT|TEXTAREA/.test(e.target.tagName)) return;
  const m = KEYS[e.key.toLowerCase()]; if (m == null) return; e.preventDefault(); down.add(m); press(m, 'KEYS'); });
addEventListener('keyup', e => { const m = KEYS[e.key.toLowerCase()]; if (m == null || !down.has(m)) return; down.delete(m); releaseKey(m); });
addEventListener('blur', () => { down.forEach(m => releaseKey(m)); down.clear(); });
/* browser: show what the preview plays */
setInterval(() => { if (inPlugin() || window.__obsSynthLive) return; showNote(live.note, live.src, live.note != null ? .7 : 0); showStep(on('syn.arp.on') ? live.step : -1); }, 50);

/* ============================================================ plugin hooks */
window.obsSynth = {
  ids: DEFS.map(d => d[0]),
  defs: DEFS,
  read() { const o = {}; for (const id in P) o[id] = toN(id); return o; },
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
  setPower(isOn) { offMask.classList.toggle('show', !isOn); },
  dice: roll,
};

/* ------------------------------------------------------------ synth on / off (plugin) */
const offMask = el('div', 'sy-offmask', '<div class="sy-offcard"><b>THE SYNTH IS OFF</b><button>TURN IT ON</button><span>Double-click the SYNTH tab to switch it on or off any time</span></div>');
panel.appendChild(offMask);
offMask.querySelector('button').addEventListener('click', () => { if (window.obsSynth.powerOut) window.obsSynth.powerOut(true); });

/* ------------------------------------------------------------ start */
buildEnvKnobs(); showTab(0); tabDots(); drawLane();
setTimeout(redrawAll, 60);
})();
</script>
