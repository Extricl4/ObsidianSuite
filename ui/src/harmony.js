<script>
/* ===== HARMONY page: HYDRA, a smart harmonizer with up to six voices =====
   Pick a key and a mode; each voice is a step in that scale ("a 3rd up"), so the number of
   semitones changes with the note you play (on C in C major a 3rd is +4, on D it is +3).
   FIXED voices move by a set number of semitones instead (power fifths, octaves).
   TRACKING: LEAD follows single notes, as fast as possible (solos and melodies, what it's made for);
   CHORDS takes a little longer to listen and gives every note of a chord its own harmony.
   In a browser it plays by itself: click the keyboard or use keys A-K (in CHORDS, clicks build a chord). */
(function () {
'use strict';
const HP = document.querySelector('.page[data-page="HARMONY"]'); if (!HP) return;
const $ = (s, r = document) => r.querySelector(s), $$ = (s, r = document) => [...r.querySelectorAll(s)];
const clamp = (x, a, b) => Math.max(a, Math.min(b, x));
const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
const MINUS = '−';
const NOTE = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];
const KEYS = ['C', 'C♯ / D♭', 'D', 'D♯ / E♭', 'E', 'F', 'F♯ / G♭', 'G', 'G♯ / A♭', 'A', 'A♯ / B♭', 'B'];
const nn = m => NOTE[((m % 12) + 12) % 12] + (Math.floor(m / 12) - 1);

/* ------------------------------------------------------------ modes */
const MODES = [
  ['Common', [['Major (Ionian)', [0, 2, 4, 5, 7, 9, 11]], ['Natural minor (Aeolian)', [0, 2, 3, 5, 7, 8, 10]], ['Harmonic minor', [0, 2, 3, 5, 7, 8, 11]], ['Melodic minor', [0, 2, 3, 5, 7, 9, 11]]]],
  ['Church modes', [['Dorian', [0, 2, 3, 5, 7, 9, 10]], ['Phrygian', [0, 1, 3, 5, 7, 8, 10]], ['Lydian', [0, 2, 4, 6, 7, 9, 11]], ['Mixolydian', [0, 2, 4, 5, 7, 9, 10]], ['Locrian', [0, 1, 3, 5, 6, 8, 10]]]],
  ['Pentatonic and blues', [['Major pentatonic', [0, 2, 4, 7, 9]], ['Minor pentatonic', [0, 3, 5, 7, 10]], ['Blues', [0, 3, 5, 6, 7, 10]], ['Major blues', [0, 2, 3, 4, 7, 9]], ['Dominant pentatonic', [0, 2, 4, 7, 10]]]],
  ['Metal and dark', [['Phrygian dominant', [0, 1, 4, 5, 7, 8, 10]], ['Hungarian minor', [0, 2, 3, 6, 7, 8, 11]], ['Double harmonic (Byzantine)', [0, 1, 4, 5, 7, 8, 11]], ['Neapolitan minor', [0, 1, 3, 5, 7, 8, 11]], ['Neapolitan major', [0, 1, 3, 5, 7, 9, 11]], ['Locrian ♮6', [0, 1, 3, 5, 6, 9, 10]], ['Super Locrian (altered)', [0, 1, 3, 4, 6, 8, 10]], ['Enigmatic', [0, 1, 4, 6, 8, 10, 11]]]],
  ['Symmetric', [['Diminished (whole-half)', [0, 2, 3, 5, 6, 8, 9, 11]], ['Diminished (half-whole)', [0, 1, 3, 4, 6, 7, 9, 10]], ['Whole tone', [0, 2, 4, 6, 8, 10]], ['Augmented', [0, 3, 4, 7, 8, 11]], ['Chromatic', [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]]]],
  ['Melodic minor family', [['Dorian ♭2', [0, 1, 3, 5, 7, 9, 10]], ['Lydian augmented', [0, 2, 4, 6, 8, 9, 11]], ['Lydian dominant', [0, 2, 4, 6, 7, 9, 10]], ['Mixolydian ♭6', [0, 2, 4, 5, 7, 8, 10]], ['Locrian ♮2', [0, 2, 3, 5, 6, 8, 10]]]],
  ['Harmonic minor family', [['Ionian ♯5', [0, 2, 4, 5, 8, 9, 11]], ['Ukrainian Dorian', [0, 2, 3, 6, 7, 9, 10]], ['Lydian ♯2', [0, 3, 4, 6, 7, 9, 11]]]],
  ['World', [['Hirajoshi', [0, 2, 3, 7, 8]], ['In Sen', [0, 1, 5, 7, 10]], ['Iwato', [0, 1, 5, 6, 10]], ['Kumoi', [0, 2, 3, 7, 9]], ['Persian', [0, 1, 4, 5, 6, 8, 11]], ['Egyptian', [0, 2, 5, 7, 10]], ['Gypsy', [0, 2, 3, 6, 7, 8, 10]], ['Prometheus', [0, 2, 4, 6, 9, 10]]]],
  ['Jazz', [['Bebop dominant', [0, 2, 4, 5, 7, 9, 10, 11]], ['Bebop major', [0, 2, 4, 5, 7, 8, 9, 11]]]],
];
const MODE_LIST = MODES.flatMap(([g, l]) => l.map(([n, s]) => ({ group: g, name: n, steps: s })));
const DEG = ['1', '♭2', '2', '♭3', '3', '4', '♭5', '5', '♭6', '6', '♭7', '7'];

/* ------------------------------------------------------------ state */
const VC = ['#6fd3ff', '#ff7ba6', '#ffd36e', '#9cf07a', '#b48cff', '#ff9a5c'];
const S = { on: true, key: 4, mode: 1, mix: 50, out: 0, human: 15, spread: 50, place: 0, chroma: 0, track: 0,
  voices: [
    { on: 1, smart: 1, step: 2, semi: 4, level: 80, pan: -35, detune: 0, delay: 0 },
    { on: 1, smart: 1, step: 4, semi: 7, level: 65, pan: 35, detune: 0, delay: 0 },
    { on: 0, smart: 0, step: -7, semi: -12, level: 60, pan: 0, detune: 0, delay: 0 },
    { on: 0, smart: 1, step: 5, semi: 9, level: 60, pan: -60, detune: 6, delay: 12 },
    { on: 0, smart: 1, step: 7, semi: 12, level: 50, pan: 60, detune: -6, delay: 18 },
    { on: 0, smart: 1, step: -2, semi: -3, level: 60, pan: 0, detune: 0, delay: 0 } ] };
const DEF = JSON.parse(JSON.stringify(S));
let played = [];                // notes being played (from the guitar, the keyboard or keys A-K): one in LEAD, several in CHORDS
const lead = () => played.length ? played[played.length - 1] : null;
const scale = () => MODE_LIST[S.mode].steps;

/** The smart harmony: the note `steps` scale steps from `m` in the key and mode. Off-key notes snap to the
    nearest scale note for the count, then keep their own offset (or reuse the last shift with HOLD). */
const lastShift = [0, 0, 0, 0, 0, 0];
function harmony(m, steps, vi) {
  const sc = scale(), n = sc.length, rel = ((m - S.key) % 12 + 12) % 12;
  let i = sc.indexOf(rel);
  if (i < 0) {
    if (S.chroma === 1) return m + lastShift[vi];
    let bd = 99; sc.forEach((s, k) => { const d = Math.min(Math.abs(s - rel), 12 - Math.abs(s - rel)); if (d < bd) { bd = d; i = k; } });
  }
  const t = i + steps, o = Math.floor(t / n), idx = ((t % n) + n) % n;
  return m + sc[idx] + 12 * o - sc[i];
}
function voiceNote(m, vi) {
  const v = S.voices[vi];
  const out = v.smart ? harmony(m, v.step, vi) : m + v.semi;
  lastShift[vi] = out - m; return out;
}
function stepName(st) {
  const n = scale().length, a = Math.abs(st);
  if (st === 0) return 'UNISON';
  if (n === 7) return ['UNISON', '2ND', '3RD', '4TH', '5TH', '6TH', '7TH', 'OCTAVE', '9TH', '10TH', '11TH', '12TH', '13TH', '14TH', '2 OCT'][a] || a + ' STEPS';
  if (a % n === 0) return (a / n === 1 ? 'OCTAVE' : a / n + ' OCT');
  return a + (a === 1 ? ' STEP' : ' STEPS');
}
const INT = ['UNISON', 'MIN 2ND', 'MAJ 2ND', 'MIN 3RD', 'MAJ 3RD', '4TH', 'TRITONE', '5TH', 'MIN 6TH', 'MAJ 6TH', 'MIN 7TH', 'MAJ 7TH'];
const semiName = s => { const a = Math.abs(s); return a === 0 ? 'UNISON' : a % 12 === 0 ? (a === 12 ? 'OCTAVE' : a / 12 + ' OCT') : INT[a % 12] + (a > 12 ? ' +OCT' : ''); };
const sgn = n => (n > 0 ? '+' : n < 0 ? MINUS : '') + Math.abs(n);

/* ------------------------------------------------------------ styles */
const css = el('style');
css.textContent = `
.hm-root { position: absolute; inset: 0; display: flex; align-items: flex-start; justify-content: center; padding-top: 8px; user-select: none; }
.hm-dev { width: 1320px; height: 572px; box-sizing: border-box; position: relative; border-radius: 22px;
  background: radial-gradient(80% 55% at 70% 0%, rgba(255,255,255,.06), transparent 60%), linear-gradient(172deg, #1f1f23, #131315 55%, #0d0d0f);
  box-shadow: inset 0 1px 0 rgba(255,255,255,.12), inset 0 0 0 1px rgba(255,255,255,.05), 0 26px 54px rgba(0,0,0,.55), 0 2px 0 rgba(0,0,0,.8); }
.hm-dev::before { content: ''; position: absolute; inset: 9px; border-radius: 16px; border: 1px solid var(--accent); opacity: .22; pointer-events: none; }
.hm-dev.off .hm-voices, .hm-dev.off .hm-kb { opacity: .4; filter: grayscale(.8); }
/* top strip */
.hm-top { position: absolute; left: 34px; right: 34px; top: 22px; height: 118px; display: flex; align-items: flex-start; gap: 22px; }
.hm-col { display: flex; flex-direction: column; align-items: center; gap: 8px; flex: none; }
.hm-eng { font-size: 9.5px; font-weight: 600; letter-spacing: .22em; color: #cfccc5; white-space: nowrap; }
.hm-sub { font-size: 9px; color: #6f6f76; letter-spacing: .06em; white-space: nowrap; }
.hm-dd { height: 44px; border: none; border-radius: 10px; padding: 0 14px 0 16px; cursor: pointer; font: inherit; color: #f1eee7; display: flex; align-items: center; gap: 12px; text-align: left;
  background: linear-gradient(#0b0b0d, #111114); box-shadow: inset 0 2px 6px rgba(0,0,0,.9), inset 0 0 0 1px rgba(255,255,255,.06), 0 1px 0 rgba(255,255,255,.07); }
.hm-dd:hover { box-shadow: inset 0 2px 6px rgba(0,0,0,.9), inset 0 0 0 1px rgba(255,255,255,.16), 0 1px 0 rgba(255,255,255,.07); }
.hm-dd b { font-size: 17px; font-weight: 600; letter-spacing: .04em; white-space: nowrap; }
.hm-dd svg { width: 12px; height: 12px; margin-left: auto; color: #8d8d93; flex: none; }
.hm-degs { display: flex; gap: 3px; height: 16px; }
.hm-degs i { font-style: normal; min-width: 17px; height: 16px; border-radius: 4px; font-size: 8.5px; font-weight: 600; display: flex; align-items: center; justify-content: center; color: #8d8d93; background: #17171a; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05); }
.hm-degs i.root { color: #111113; background: var(--accent); }
/* live display */
.hm-led { width: 300px; height: 92px; border-radius: 12px; box-sizing: border-box; padding: 9px 14px; position: relative; display: flex; align-items: center; gap: 16px;
  background: linear-gradient(#060607, #0b0b0d); box-shadow: inset 0 2px 8px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.04), 0 1px 0 rgba(255,255,255,.08); }
.hm-led .k { position: absolute; left: 12px; top: 7px; font-size: 7.5px; letter-spacing: .16em; color: #6f6f76; }
.hm-led .n { font-family: 'Orbitron', 'Saira', sans-serif; font-size: 32px; color: var(--accent); text-shadow: 0 0 12px var(--accent); min-width: 64px; text-align: center; margin-top: 8px; line-height: 1; }
.hm-led .n small { font-size: 14px; opacity: .75; }
.hm-led .vs { display: grid; grid-template-columns: repeat(3, 1fr); gap: 5px 8px; flex: 1; margin-top: 10px; }
.hm-led .vs span { font-family: 'Orbitron', 'Saira', sans-serif; font-size: 11px; color: var(--c); text-shadow: 0 0 8px var(--c); white-space: nowrap; display: flex; gap: 5px; align-items: baseline; }
.hm-led .vs span em { font-style: normal; font-size: 8px; opacity: .6; }
.hm-led .vs span.off { color: #2c2c31; text-shadow: none; }
.hm-seg { display: flex; flex-direction: column; gap: 5px; }
.hm-chroma { display: flex; align-items: center; gap: 4px; margin-top: 1px; }
.hm-chroma span { font-size: 7.5px; font-weight: 600; letter-spacing: .16em; color: #6f6f76; margin-right: 4px; }
.hm-chroma button { height: 17px; padding: 0 7px; border: none; border-radius: 5px; background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.06); color: #6f6f76; font: inherit; font-size: 7.5px; font-weight: 600; letter-spacing: .12em; cursor: pointer; }
.hm-chroma button.on { color: #111113; background: var(--accent); }
.hm-led .chord { font-family: 'Orbitron', 'Saira', sans-serif; font-size: 15px; color: var(--accent); text-shadow: 0 0 10px var(--accent); margin-top: 8px; min-width: 64px; max-width: 110px; line-height: 1.25; text-align: center; }
.hm-seg button { width: 92px; height: 24px; border: none; border-radius: 6px; cursor: pointer; font: inherit; font-size: 8.5px; font-weight: 600; letter-spacing: .14em; color: #8d8d93; text-align: left; padding: 0 0 0 22px; position: relative;
  background: linear-gradient(#36363b, #1d1d21 55%, #161619); box-shadow: 0 3px 5px rgba(0,0,0,.75), inset 0 1px 0 rgba(255,255,255,.2), 0 0 0 1px #0a0a0b; }
.hm-seg button::before { content: ''; position: absolute; left: 9px; top: 9px; width: 6px; height: 6px; border-radius: 50%; background: #26262a; }
.hm-seg button.on { color: #f1eee7; background: linear-gradient(#2c2c31, #19191c 60%, #131315); box-shadow: 0 1px 2px rgba(0,0,0,.8), inset 0 2px 4px rgba(0,0,0,.5), 0 0 0 1px #0a0a0b; }
.hm-seg button.on::before { background: radial-gradient(circle at 40% 35%, #fff, var(--accent) 60%); box-shadow: 0 0 7px var(--accent); }
/* knobs */
.hm-kn { position: relative; flex: none; cursor: ns-resize; touch-action: none; border-radius: 50%; }
.hm-kn > i { position: absolute; border-radius: 50%; }
.hm-kn .a { inset: 0; -webkit-mask: radial-gradient(closest-side, transparent calc(100% - 3.2px), #000 calc(100% - 2.6px), #000 calc(100% - .5px), transparent 100%); mask: radial-gradient(closest-side, transparent calc(100% - 3.2px), #000 calc(100% - 2.6px), #000 calc(100% - .5px), transparent 100%); }
.hm-kn .c { inset: 15%; background: repeating-conic-gradient(#2e2e33 0deg 3deg, #151517 3deg 6deg); box-shadow: 0 4px 8px rgba(0,0,0,.8), 0 1px 2px rgba(0,0,0,.9); }
.hm-kn .c > i { position: absolute; inset: 6%; border-radius: 50%; background: radial-gradient(circle at 38% 22%, rgba(255,255,255,.1), transparent 45%), radial-gradient(circle at 50% 15%, #47474d 0%, #212124 55%, #131315 100%); box-shadow: inset 0 1px 0 rgba(255,255,255,.2), inset 0 -3px 6px rgba(0,0,0,.6); }
.hm-kn .c > i > b { position: absolute; inset: 0; }
.hm-kn .c > i > b::after { content: ''; position: absolute; left: 50%; top: 7%; width: 2.4px; height: 30%; margin-left: -1.2px; border-radius: 2px; background: #f4f1ea; }
.hm-ctl { display: flex; flex-direction: column; align-items: center; gap: 0; min-width: 0; }
.hm-kl { font-size: 8.5px; font-weight: 600; letter-spacing: .16em; color: #a3a3a8; margin-top: 6px; white-space: nowrap; }
.hm-kv { font-size: 10.5px; color: #e9e6df; font-variant-numeric: tabular-nums; white-space: nowrap; margin-top: 2px; }
.hm-pw { width: 50px; height: 50px; border: none; border-radius: 50%; cursor: pointer; display: flex; align-items: center; justify-content: center; color: #b9b6ae; margin-top: 2px;
  background: radial-gradient(circle, #050506 58%, #1d1d20 61%, #57565a 66%, #1b1b1e 71%, #2c2c30 100%); box-shadow: 0 3px 6px rgba(0,0,0,.7), inset 0 2px 3px rgba(0,0,0,.8); }
.hm-pw svg { width: 17px; height: 17px; }
.hm-pw.on { color: var(--accent); background: radial-gradient(circle, #050506 58%, var(--accent) 61%, color-mix(in srgb, var(--accent) 55%, #000) 66%, #1b1b1e 71%, #2c2c30 100%); box-shadow: 0 3px 6px rgba(0,0,0,.7), 0 0 18px color-mix(in srgb, var(--accent) 45%, transparent); }
/* voices */
.hm-voices { position: absolute; left: 22px; right: 22px; top: 150px; height: 262px; display: grid; grid-template-columns: repeat(6, 1fr); gap: 8px; padding: 8px; box-sizing: border-box; border-radius: 16px;
  background: radial-gradient(120% 90% at 50% 0%, #121215, #08080a 70%); box-shadow: inset 0 3px 12px rgba(0,0,0,.9), inset 0 0 0 1px rgba(255,255,255,.05), 0 1px 0 rgba(255,255,255,.1); transition: opacity .2s, filter .2s; }
.hm-v { position: relative; border-radius: 12px; padding: 10px 12px 10px; box-sizing: border-box; display: flex; flex-direction: column; align-items: stretch; min-width: 0;
  background: linear-gradient(180deg, rgba(255,255,255,.045), rgba(255,255,255,.012) 40%, rgba(0,0,0,.12)), #0d0d10;
  box-shadow: inset 0 1px 0 rgba(255,255,255,.12), inset 0 0 0 1px rgba(255,255,255,.08), 0 3px 8px rgba(0,0,0,.65), 0 0 0 1px rgba(0,0,0,.7); transition: opacity .15s; }
.hm-v::before { content: ''; position: absolute; left: 14px; right: 14px; top: 0; height: 2px; border-radius: 0 0 2px 2px; background: var(--c); box-shadow: 0 0 10px var(--c); opacity: .9; }
.hm-v.mute > :not(.hm-vh) { opacity: .3; filter: grayscale(1); }
.hm-v.mute::before { opacity: .2; box-shadow: none; }
.hm-vh { display: flex; align-items: center; gap: 8px; height: 24px; }
.hm-vh h3 { margin: 0; font-size: 10px; font-weight: 600; letter-spacing: .22em; color: #e9e6df; display: flex; align-items: center; gap: 8px; white-space: nowrap; }
.hm-vh h3::before { content: ''; width: 7px; height: 7px; border-radius: 50%; background: var(--c); box-shadow: 0 0 8px var(--c); }
.hm-tg { margin-left: auto; position: relative; width: 44px; height: 22px; border: none; padding: 0 0 0 14px; border-radius: 6px; cursor: pointer; font: inherit; font-size: 8px; font-weight: 600; letter-spacing: .16em; color: #7a7a80;
  background: linear-gradient(#36363b, #1d1d21 55%, #161619); box-shadow: 0 3px 4px rgba(0,0,0,.75), inset 0 1px 0 rgba(255,255,255,.2), 0 0 0 1px #08080a; }
.hm-tg::after { content: 'ON'; }
.hm-tg i { position: absolute; left: 7px; top: 8px; width: 6px; height: 6px; border-radius: 50%; background: #26262a; }
.hm-tg.on { color: #f1eee7; background: linear-gradient(#2c2c31, #19191c 60%, #131315); box-shadow: 0 1px 2px rgba(0,0,0,.8), inset 0 2px 4px rgba(0,0,0,.5), 0 0 0 1px color-mix(in srgb, var(--c) 50%, #08080a), 0 0 10px color-mix(in srgb, var(--c) 25%, transparent); }
.hm-tg.on i { background: radial-gradient(circle at 40% 35%, #fff, var(--c) 60%); box-shadow: 0 0 7px var(--c); }
.hm-ms { display: flex; gap: 2px; padding: 3px; border-radius: 8px; background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05); margin-top: 8px; }
.hm-ms button { flex: 1; height: 20px; border: none; border-radius: 6px; background: none; color: #6f6f76; font: inherit; font-size: 8.5px; font-weight: 600; letter-spacing: .12em; cursor: pointer; }
.hm-ms button.on { color: #e9e6df; background: linear-gradient(#34343a, #25252a); box-shadow: inset 0 1px 0 rgba(255,255,255,.1), inset 0 -2px 0 var(--c); }
.hm-int { margin-top: 8px; height: 66px; border-radius: 10px; display: flex; align-items: center; background: #050506; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05), inset 0 4px 12px rgba(0,0,0,.7); cursor: ns-resize; touch-action: none; }
.hm-int button { width: 26px; height: 100%; border: none; background: none; color: #6f6f76; cursor: pointer; font-size: 13px; padding: 0; flex: none; }
.hm-int button:hover { color: #e9e6df; }
.hm-int .mid { flex: 1; min-width: 0; display: flex; flex-direction: column; align-items: center; gap: 3px; }
.hm-int .dir { font-size: 8px; font-weight: 600; letter-spacing: .2em; color: var(--c); opacity: .9; }
.hm-int b { font-size: 17px; font-weight: 600; letter-spacing: .06em; color: #f4f1ea; white-space: nowrap; }
.hm-int .live { font-size: 9.5px; color: #8d8d93; font-variant-numeric: tabular-nums; white-space: nowrap; }
.hm-int .live em { font-style: normal; color: var(--c); }
.hm-vk { display: grid; grid-template-columns: repeat(4, 1fr); gap: 2px; margin-top: 12px; justify-items: center; }
.hm-vk .hm-kl { letter-spacing: .1em; font-size: 8px; }
.hm-vk .hm-kv { font-size: 10px; }
/* keyboard */
.hm-kb { position: absolute; left: 22px; right: 22px; top: 422px; height: 58px; border-radius: 12px; background: #08080a; box-shadow: inset 0 2px 8px rgba(0,0,0,.9), inset 0 0 0 1px rgba(255,255,255,.05); overflow: hidden; transition: opacity .2s; }
.hm-kb .keys { position: absolute; left: 10px; right: 10px; top: 8px; bottom: 6px; }
.hm-kb .w, .hm-kb .b { position: absolute; top: 0; box-sizing: border-box; cursor: pointer; }
.hm-kb .w { bottom: 0; border-radius: 0 0 4px 4px; background: linear-gradient(#2a2a2f, #1c1c20); box-shadow: inset 0 0 0 1px rgba(0,0,0,.7), inset 0 -2px 0 rgba(255,255,255,.04); }
.hm-kb .b { height: 58%; border-radius: 0 0 3px 3px; background: linear-gradient(#0c0c0e, #141417); box-shadow: inset 0 0 0 1px rgba(0,0,0,.9), 0 2px 3px rgba(0,0,0,.6); z-index: 2; }
.hm-kb .w.sc { background: linear-gradient(#3a3a40, #26262b); }
.hm-kb .b.sc { background: linear-gradient(#2a2a30, #1d1d21); }
.hm-kb .rt::after { content: ''; position: absolute; left: 50%; bottom: 5px; width: 5px; height: 5px; margin-left: -2.5px; border-radius: 50%; background: var(--accent); box-shadow: 0 0 6px var(--accent); }
.hm-kb .pl { background: linear-gradient(#f4f1ea, #b9b6ae) !important; box-shadow: 0 0 14px rgba(255,255,255,.5) !important; }
.hm-kb .dot { position: absolute; width: 9px; height: 9px; border-radius: 50%; margin-left: -4.5px; z-index: 3; pointer-events: none; box-shadow: 0 0 8px currentColor; background: currentColor; transition: left .08s; }
.hm-kb .lbl { position: absolute; bottom: 7px; font-size: 7px; color: #55555b; pointer-events: none; z-index: 1; margin-left: -6px; }
/* bottom band */
.hm-bot { position: absolute; left: 40px; right: 40px; bottom: 20px; height: 60px; display: flex; align-items: center; justify-content: space-between; }
.hm-badge { position: absolute; left: 50%; top: 6px; transform: translateX(-50%); display: flex; flex-direction: column; align-items: center; pointer-events: none; }
.hm-badge b { font-family: 'Cinzel', 'Saira', serif; font-size: 28px; font-weight: 700; letter-spacing: .32em; margin-right: -.32em; line-height: 1; color: transparent;
  background: linear-gradient(180deg, #f4f1ea 0%, #a7a49c 55%, #e9e6df 100%); -webkit-background-clip: text; background-clip: text; filter: drop-shadow(0 2px 0 rgba(0,0,0,.9)); }
.hm-badge span { margin-top: 6px; font-size: 8.5px; font-weight: 600; letter-spacing: .5em; margin-right: -.5em; color: #8d8d93; }
.hm-heads { position: absolute; top: 14px; display: flex; gap: 6px; }
.hm-heads i { width: 6px; height: 6px; border-radius: 50%; background: #26262a; transition: background .15s, box-shadow .15s; }
.hm-heads.l { right: calc(50% + 150px); } .hm-heads.r { left: calc(50% + 150px); }
.hm-btn { display: flex; align-items: center; gap: 12px; cursor: pointer; border: none; background: none; padding: 0; font: inherit; color: inherit; }
.hm-btn .round { width: 42px; height: 42px; border-radius: 50%; display: flex; align-items: center; justify-content: center; color: #e9e6df;
  background: radial-gradient(circle at 38% 30%, #4a4a50, #1a1a1d 62%, #0c0c0d); box-shadow: 0 0 0 2px #8f8e89, 0 0 0 3px #232326, 0 6px 12px rgba(0,0,0,.8), inset 0 1px 1px rgba(255,255,255,.25); }
.hm-btn .round svg { width: 15px; height: 15px; }
.hm-btn:hover .round { color: var(--accent); }
.hm-btn .tx { display: flex; flex-direction: column; gap: 3px; text-align: left; }
.hm-btn .tx b { font-size: 10px; font-weight: 600; letter-spacing: .22em; color: #e9e6df; }
.hm-btn .tx small { font-size: 9px; color: var(--accent); opacity: .9; max-width: 170px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.hm-btn.r { flex-direction: row-reverse; } .hm-btn.r .tx { text-align: right; } .hm-btn.r .tx small { color: #6f6f76; }
.hm-nav { display: flex; gap: 4px; margin-left: 12px; }
.hm-nav button { width: 26px; height: 26px; border-radius: 50%; border: 1px solid rgba(255,255,255,.12); background: none; color: #8d8d93; cursor: pointer; display: flex; align-items: center; justify-content: center; padding: 0; }
.hm-nav button:hover { color: #e9e6df; }
.hm-nav svg { width: 12px; height: 12px; }
/* menus */
.hm-menu { position: absolute; z-index: 30; display: none; border-radius: 14px; padding: 12px; box-sizing: border-box;
  background: linear-gradient(#1e1e22, #141416); box-shadow: inset 0 1px 0 rgba(255,255,255,.08), inset 0 0 0 1px rgba(255,255,255,.07), 0 24px 50px rgba(0,0,0,.8); }
.hm-menu.show { display: block; }
.hm-menu input { width: 100%; box-sizing: border-box; height: 30px; border: none; border-radius: 8px; padding: 0 10px; background: #0a0a0b; color: #e9e6df; font: inherit; font-size: 12px; outline: none; box-shadow: inset 0 2px 6px rgba(0,0,0,.9); margin-bottom: 8px; }
.hm-modes { max-height: 330px; overflow-y: auto; padding-right: 4px; columns: 3; column-gap: 12px; }
.hm-modes::-webkit-scrollbar { width: 6px; } .hm-modes::-webkit-scrollbar-thumb { background: #2a2a2f; border-radius: 3px; }
.hm-modes h5 { margin: 6px 0 4px; font-size: 8.5px; font-weight: 600; letter-spacing: .2em; color: var(--accent); break-after: avoid; }
.hm-modes button { display: flex; flex-direction: column; gap: 2px; width: 100%; border: none; border-radius: 7px; background: none; color: #d6d3cc; font: inherit; font-size: 11.5px; text-align: left; padding: 5px 8px; cursor: pointer; break-inside: avoid; }
.hm-modes button small { font-size: 8.5px; color: #6f6f76; letter-spacing: .06em; }
.hm-modes button:hover { background: rgba(255,255,255,.05); }
.hm-modes button.on { background: rgba(255,255,255,.08); box-shadow: inset 2px 0 0 var(--accent); color: #fff; }
.hm-keys { display: grid; grid-template-columns: repeat(4, 74px); gap: 5px; }
.hm-keys button { height: 34px; border: none; border-radius: 8px; background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.06); color: #d6d3cc; font: inherit; font-size: 12px; cursor: pointer; }
.hm-keys button:hover { box-shadow: inset 0 0 0 1px rgba(255,255,255,.18); }
.hm-keys button.on { background: var(--accent); color: #111113; font-weight: 600; }
.hm-prcols { display: grid; grid-template-columns: repeat(4, 208px); gap: 10px; align-items: start; }
.hm-prcol { --cc: #888; border-radius: 12px; padding: 8px; display: flex; flex-direction: column; gap: 5px; max-height: 440px; overflow-y: auto;
  background: linear-gradient(color-mix(in srgb, var(--cc) 9%, #141416), #111113 60%); box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--cc) 30%, transparent); }
.hm-prcol h4 { margin: 0; padding: 4px 6px 0; display: flex; justify-content: space-between; align-items: baseline; font-size: 10px; font-weight: 700; letter-spacing: .2em; color: var(--cc); text-shadow: 0 0 10px color-mix(in srgb, var(--cc) 45%, transparent); }
.hm-prcol h4 em { font-style: normal; font-size: 9px; color: #6f6f76; letter-spacing: .1em; text-shadow: none; }
.hm-prcol p { margin: 0 0 4px; padding: 0 6px 7px; font-size: 8.5px; color: #77777e; letter-spacing: .04em; border-bottom: 1px solid color-mix(in srgb, var(--cc) 25%, transparent); }
.hm-prcol button { border: none; border-radius: 9px; padding: 7px 9px 8px; text-align: left; cursor: pointer; font: inherit; color: #e9e6df; display: flex; flex-direction: column; gap: 3px; flex: none;
  background: #0f0f12; box-shadow: inset 2px 0 0 color-mix(in srgb, var(--cc) 55%, transparent), inset 0 0 0 1px rgba(255,255,255,.05); }
.hm-prcol button b { font-size: 11px; font-weight: 600; letter-spacing: .03em; }
.hm-prcol button small { font-size: 8.5px; color: #7d7d84; line-height: 1.3; }
.hm-prcol button:hover { background: #17171b; box-shadow: inset 2px 0 0 var(--cc), inset 0 0 0 1px rgba(255,255,255,.14); }
.hm-prcol button.on { background: color-mix(in srgb, var(--cc) 14%, #111114); box-shadow: inset 3px 0 0 var(--cc), inset 0 0 0 1px color-mix(in srgb, var(--cc) 60%, transparent); }
.hm-chips { display: flex; gap: 4px; flex-wrap: wrap; margin-top: 2px; }
.hm-chips i { font-style: normal; font-size: 7.5px; font-weight: 700; letter-spacing: .06em; padding: 1px 5px; border-radius: 5px; color: var(--c); background: color-mix(in srgb, var(--c) 13%, transparent); box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--c) 30%, transparent); }
`;
document.head.appendChild(css);

/* ------------------------------------------------------------ controls */
const IC = {
  power: '<svg viewBox="0 0 24 24"><path d="M12 3v8M6.5 6.8a8 8 0 1011 0" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"/></svg>',
  down: '<svg viewBox="0 0 24 24"><path d="M6 9l6 6 6-6" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  list: '<svg viewBox="0 0 24 24"><path d="M4 6h11M4 12h11M4 18h7" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"/><circle cx="19" cy="17" r="2.6" fill="none" stroke="currentColor" stroke-width="2"/><path d="M21.6 17V6.5L18 5.5" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg>',
  reset: '<svg viewBox="0 0 24 24"><path d="M4.5 12a7.5 7.5 0 1 0 2.2-5.3M4.5 4.5v4.2h4.2" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  prev: '<svg viewBox="0 0 24 24"><path d="M15 6l-6 6 6 6" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  next: '<svg viewBox="0 0 24 24"><path d="M9 6l6 6-6 6" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/></svg>',
};
const redraws = [];
const changed = () => redraws.forEach(f => f());
/** knob(get, set, {min, max, def, fmt, size, label, color, bip}) */
function knob(o) {
  const c = el('div', 'hm-ctl'), size = o.size || 34;
  const k = el('div', 'hm-kn', '<i class="a"></i><i class="c"><i><b></b></i></i>'); k.style.width = k.style.height = size + 'px';
  c.append(k, el('span', 'hm-kl', o.label), el('span', 'hm-kv'));
  const a = k.querySelector('.a'), r = k.querySelector('b'), kv = c.querySelector('.hm-kv');
  const t = () => (o.get() - o.min) / (o.max - o.min);
  const draw = () => { const d = 270 * t(), col = o.color || 'var(--accent)';
    if (o.min < 0) { const z = 270 * (-o.min / (o.max - o.min)), lo = Math.min(d, z), hi = Math.max(d, z);
      a.style.background = 'conic-gradient(from 225deg, #2b2b30 0deg ' + lo + 'deg, ' + col + ' ' + lo + 'deg ' + hi + 'deg, #2b2b30 ' + hi + 'deg 270deg, transparent 270deg 360deg)'; }
    else a.style.background = 'conic-gradient(from 225deg, ' + col + ' 0deg ' + d + 'deg, #2b2b30 ' + d + 'deg 270deg, transparent 270deg 360deg)';
    r.style.transform = 'rotate(' + (d - 135) + 'deg)'; kv.textContent = o.fmt(o.get()); };
  let drag = null;
  k.addEventListener('pointerdown', e => { if (e.button) return; e.preventDefault(); k.setPointerCapture(e.pointerId); drag = { y: e.clientY, v: t() }; });
  k.addEventListener('pointermove', e => { if (!drag) return; o.set(o.min + clamp(drag.v + (drag.y - e.clientY) / (e.shiftKey ? 900 : 180), 0, 1) * (o.max - o.min)); changed(); });
  k.addEventListener('pointerup', () => drag = null);
  k.addEventListener('dblclick', () => { o.set(o.def); changed(); });
  k.addEventListener('wheel', e => { e.preventDefault(); o.set(clamp(o.get() + (e.deltaY < 0 ? 1 : -1) * (o.max - o.min) * .02, o.min, o.max)); changed(); }, { passive: false });
  redraws.push(draw); draw(); return c;
}
const pct = v => Math.round(v) + '%';
const pan = v => Math.abs(v) < 1 ? 'C' : (v < 0 ? 'L ' : 'R ') + Math.round(Math.abs(v));

/* ============================================================ the device */
HP.replaceChildren();
const root = el('div', 'hm-root'), dev = el('div', 'hm-dev'); root.appendChild(dev); HP.appendChild(root);

/* ---- top strip ---- */
const top = el('div', 'hm-top'); dev.appendChild(top);
const pw = el('button', 'hm-pw on', IC.power); pw.title = 'Harmony on / off';
pw.onclick = () => { S.on = !S.on; changed(); if (window.obsHarmony && window.obsHarmony.powerOut) window.obsHarmony.powerOut(S.on); };
const pwV = el('span', 'hm-sub');
const colPw = el('div', 'hm-col'); colPw.append(el('span', 'hm-eng', 'POWER'), pw, pwV);

const keyBtn = el('button', 'hm-dd', '<b></b>' + IC.down); keyBtn.style.width = '118px'; keyBtn.title = 'The key of the song';
const colKey = el('div', 'hm-col'); colKey.append(el('span', 'hm-eng', 'KEY'), keyBtn, el('span', 'hm-sub', 'root note'));
const modeBtn = el('button', 'hm-dd', '<b></b>' + IC.down); modeBtn.style.width = '268px'; modeBtn.title = 'The scale the harmonies follow';
const degs = el('div', 'hm-degs');
const colMode = el('div', 'hm-col'); colMode.style.cssText = 'align-items: flex-start; gap: 7px'; colMode.append(el('span', 'hm-eng', 'MODE'), modeBtn, degs);

const led = el('div', 'hm-led', '<span class="k">PLAYING</span><div class="n">–</div><div class="vs"></div>');
led.title = 'The note you play and what each voice sings with it';
const ledVs = led.querySelector('.vs'), ledN = led.querySelector('.n');
const colLed = el('div', 'hm-col'); colLed.style.marginTop = '14px'; colLed.append(led);

const seg = (label, opts, get, set, tip) => { const c = el('div', 'hm-col'); c.title = tip || ''; const s = el('div', 'hm-seg');
  opts.forEach((t, i) => { const b = el('button', null, t); b.onclick = () => { set(i); changed(); }; s.appendChild(b); });
  redraws.push(() => [...s.children].forEach((b, i) => b.classList.toggle('on', get() === i)));
  c.append(el('span', 'hm-eng', label), s); return c; };
const colTrack = seg('TRACKING', ['LEAD', 'CHORDS'], () => S.track, i => { S.track = i; stop(); }, 'LEAD: the fastest, follows single notes (solos, melodies, riffs), only a few ms behind you · CHORDS: listens longer (the voices are about 64 ms behind) and gives every note of a chord its own harmony');
const colPlace = seg('PLACE', ['AFTER AMP', 'BEFORE AMP'], () => S.place, i => S.place = i, 'AFTER: harmonies of your amp sound, like a second guitarist (in stereo, with the pans) · BEFORE: the voices go through the amp with you (mono)');
const chroma = el('div', 'hm-chroma', '<span>OFF-KEY NOTES</span><button>FOLLOW</button><button>HOLD</button>');
chroma.title = 'Notes outside the mode: FOLLOW uses the interval of the nearest scale note · HOLD keeps the last harmony';
chroma.children[1].onclick = () => { S.chroma = 0; changed(); }; chroma.children[2].onclick = () => { S.chroma = 1; changed(); };
redraws.push(() => { chroma.children[1].classList.toggle('on', !S.chroma); chroma.children[2].classList.toggle('on', !!S.chroma); });
const knobs = el('div'); knobs.style.cssText = 'display: flex; gap: 14px; margin-left: auto; margin-top: 4px';
knobs.append(
  knob({ label: 'HUMANIZE', get: () => S.human, set: v => S.human = v, min: 0, max: 100, def: 15, fmt: pct, size: 40 }),
  knob({ label: 'SPREAD', get: () => S.spread, set: v => S.spread = v, min: 0, max: 100, def: 50, fmt: pct, size: 40 }),
  knob({ label: 'MIX', get: () => S.mix, set: v => S.mix = v, min: 0, max: 100, def: 50, fmt: pct, size: 40 }),
  knob({ label: 'OUTPUT', get: () => S.out, set: v => S.out = v, min: -24, max: 12, def: 0, fmt: v => sgn(+v.toFixed(1)) + ' dB', size: 40 }));
[...knobs.children].forEach(k => k.style.marginTop = '14px');
const knCol = el('div', 'hm-col'); knCol.style.cssText = 'margin-left: auto; align-items: stretch';
colMode.appendChild(chroma);
top.append(colPw, colKey, colMode, colLed, colTrack, colPlace, knobs);

/* ---- six voices ---- */
const voices = el('div', 'hm-voices'); dev.appendChild(voices);
const vEls = S.voices.map((v, i) => {
  const c = el('div', 'hm-v'); c.style.setProperty('--c', VC[i]);
  const h = el('div', 'hm-vh'); h.append(el('h3', null, 'VOICE ' + (i + 1)));
  const tg = el('button', 'hm-tg', '<i></i>'); tg.onclick = () => { v.on = v.on ? 0 : 1; changed(); }; h.appendChild(tg);
  const ms = el('div', 'hm-ms', '<button>SMART</button><button>FIXED</button>');
  ms.title = 'SMART: a step in the scale, the semitones follow the key and mode · FIXED: always the same number of semitones';
  ms.children[0].onclick = () => { v.smart = 1; changed(); }; ms.children[1].onclick = () => { v.smart = 0; changed(); };
  const iv = el('div', 'hm-int', '<button aria-label="Down">‹</button><div class="mid"><span class="dir"></span><b></b><span class="live"></span></div><button aria-label="Up">›</button>');
  iv.title = 'Drag up / down or use the arrows';
  const bump = d => { if (v.smart) v.step = clamp(v.step + d, -14, 14); else v.semi = clamp(v.semi + d, -24, 24); changed(); };
  iv.children[0].onclick = () => bump(-1); iv.children[2].onclick = () => bump(1);
  { let d = null; const mid = iv.children[1];
    mid.addEventListener('pointerdown', e => { e.preventDefault(); mid.setPointerCapture(e.pointerId); d = { y: e.clientY, v: v.smart ? v.step : v.semi }; });
    mid.addEventListener('pointermove', e => { if (!d) return; const n = d.v + Math.round((d.y - e.clientY) / 14); if (v.smart) v.step = clamp(n, -14, 14); else v.semi = clamp(n, -24, 24); changed(); });
    mid.addEventListener('pointerup', () => d = null);
    iv.addEventListener('wheel', e => { e.preventDefault(); bump(e.deltaY < 0 ? 1 : -1); }, { passive: false }); }
  const vk = el('div', 'hm-vk');
  vk.append(knob({ label: 'LEVEL', get: () => v.level, set: x => v.level = x, min: 0, max: 100, def: 70, fmt: pct, color: VC[i], size: 28 }),
            knob({ label: 'PAN', get: () => v.pan, set: x => v.pan = x, min: -100, max: 100, def: 0, fmt: pan, color: VC[i], size: 28 }),
            knob({ label: 'DETUNE', get: () => v.detune, set: x => v.detune = x, min: -50, max: 50, def: 0, fmt: x => sgn(Math.round(x)) + ' ct', color: VC[i], size: 28 }),
            knob({ label: 'DELAY', get: () => v.delay, set: x => v.delay = x, min: 0, max: 120, def: 0, fmt: x => Math.round(x) + ' ms', color: VC[i], size: 28 }));
  c.append(h, ms, iv, vk); voices.appendChild(c);
  return { c, tg, ms, iv };
});

/* ---- keyboard: the mode's notes, what you play, where each voice lands ---- */
const kb = el('div', 'hm-kb', '<div class="keys"></div>'); dev.appendChild(kb);
const keysEl = kb.querySelector('.keys');
const LO = 40, HI = 40 + 48;                       // E2 .. E6: four octaves, the guitar's range and the harmonies above it
const whites = []; for (let m = LO; m <= HI; m++) if (![1, 3, 6, 8, 10].includes(m % 12)) whites.push(m);
const keyEls = {}, keyX = {};
const wx = m => { const i = whites.indexOf(m); return i >= 0 ? (i + .5) / whites.length * 100 : null; };
for (let m = LO; m <= HI; m++) {
  const black = [1, 3, 6, 8, 10].includes(m % 12);
  const k = el('div', black ? 'b' : 'w'); keyEls[m] = k;
  const ww = 100 / whites.length;
  if (black) { const c = wx(m - 1) + ww / 2; k.style.left = (c - ww * .3) + '%'; k.style.width = (ww * .6) + '%'; keyX[m] = c; }
  else { const i = whites.indexOf(m); k.style.left = (i * ww) + '%'; k.style.width = ww + '%'; keyX[m] = (i + .5) * ww; }
  k.addEventListener('pointerdown', e => { e.preventDefault(); if (S.track) toggleChord(m); else play(m); });
  k.addEventListener('pointerup', () => { if (!S.track) stop(); });
  k.addEventListener('pointerleave', () => { if (!S.track && played[0] === m) stop(); });
  keysEl.appendChild(k);
  if (m % 12 === 4 && m < HI) { const l = el('span', 'lbl', 'E' + (Math.floor(m / 12) - 1)); l.style.left = (wx(m) + 1) + '%'; keysEl.appendChild(l); }
}
const dotPool = [];
const dot = i => { while (dotPool.length <= i) { const d = el('span', 'dot'); d.style.display = 'none'; keysEl.appendChild(d); dotPool.push(d); } return dotPool[i]; };
const xOf = m => keyX[clamp(m, LO, HI)];

/* ---- bottom band: presets, name, reset ---- */
const bot = el('div', 'hm-bot'); dev.appendChild(bot);
const prBtn = el('button', 'hm-btn', '<span class="round">' + IC.list + '</span><span class="tx"><b>PRESETS</b><small>Twin Thirds</small></span>');
const prNav = el('div', 'hm-nav', '<button aria-label="Previous">' + IC.prev + '</button><button aria-label="Next">' + IC.next + '</button>');
const prWrap = el('div'); prWrap.style.cssText = 'display: flex; align-items: center'; prWrap.append(prBtn, prNav);
const badge = el('div', 'hm-badge', '<b>HYDRA</b><span>OBSIDIAN HARMONIZER</span>');
const headsL = el('div', 'hm-heads l', '<i></i><i></i><i></i>'), headsR = el('div', 'hm-heads r', '<i></i><i></i><i></i>');
const resetB = el('button', 'hm-btn r', '<span class="round">' + IC.reset + '</span><span class="tx"><b>RESET</b><small>back to default</small></span>');
resetB.onclick = () => { const d = JSON.parse(JSON.stringify(DEF)); Object.assign(S, d); prName('Twin Thirds'); changed(); };
bot.append(prWrap, headsL, badge, headsR, resetB);
const heads = [...headsL.children, ...headsR.children];

/* ---- menus ---- */
function menu(anchor, build, x, y, w) {
  const m = el('div', 'hm-menu'); m.style.left = x + 'px'; m.style.top = y + 'px'; if (w) m.style.width = w + 'px'; build(m); dev.appendChild(m);
  anchor.addEventListener('click', e => { e.stopPropagation(); $$('.hm-menu.show', dev).forEach(o => o !== m && o.classList.remove('show')); m.classList.toggle('show'); if (m._open) m._open(); });
  document.addEventListener('pointerdown', e => { if (m.classList.contains('show') && !m.contains(e.target) && !anchor.contains(e.target)) m.classList.remove('show'); });
  return m;
}
const keyMenu = menu(keyBtn, m => {
  const g = el('div', 'hm-keys'); KEYS.forEach((k, i) => { const b = el('button', null, k); b.onclick = () => { S.key = i; m.classList.remove('show'); changed(); }; g.appendChild(b); });
  m.appendChild(g); redraws.push(() => [...g.children].forEach((b, i) => b.classList.toggle('on', i === S.key)));
}, 150, 84);
const modeMenu = menu(modeBtn, m => {
  const q = el('input'); q.placeholder = 'Search ' + MODE_LIST.length + ' modes'; const list = el('div', 'hm-modes');
  const draw = () => { const f = q.value.trim().toLowerCase(); list.innerHTML = '';
    MODES.forEach(([g, l]) => { const items = l.filter(([n]) => !f || n.toLowerCase().includes(f) || g.toLowerCase().includes(f)); if (!items.length) return;
      list.appendChild(el('h5', null, g.toUpperCase()));
      items.forEach(([n, st]) => { const i = MODE_LIST.findIndex(x => x.name === n), b = el('button', i === S.mode ? 'on' : '', n + '<small>' + st.map(s => DEG[s]).join('  ') + '</small>');
        b.onclick = () => { S.mode = i; m.classList.remove('show'); changed(); }; list.appendChild(b); }); }); };
  q.addEventListener('input', draw); m._open = () => { q.value = ''; draw(); setTimeout(() => q.focus(), 0); };
  m.append(q, list);
}, 290, 84, 720);
/* HYDRA presets, in four groups. A voice: [on, smart, step (smart), semitones (fixed), level, pan, detune]. */
const HCATS = [['TWIN LEADS', '#6fd3ff', 'Two guitars, one melody'], ['STACKS AND CHOIRS', '#ffd36e', 'Three parts and up'],
               ['POWER AND OCTAVES', '#9cf07a', 'Fixed intervals: work on chords too'], ['DARK AND STRANGE', '#b48cff', 'Unsettling colours']];
const HPRE = [
  ['Twin Thirds', 'A third above, like a second guitar', { voices: [[1, 1, 2]], mode: 1 }, 0],
  ['Maiden Gallop', 'Twin leads a third apart, panned wide', { voices: [[1, 1, 2, 0, 85, -70], [1, 1, 0, 0, 0, 0]], mode: 1 }, 0],
  ['Thirds Below', 'A third under the melody: warmer', { voices: [[1, 1, -2, 0, 75, 40]], mode: 1 }, 0],
  ['Sweet Sixths', 'A sixth under the lead, southern-rock style', { voices: [[1, 1, -5, 0, 75, -45]], mode: 0 }, 0],
  ['Open Fourths', 'A fourth above: open and modern', { voices: [[1, 1, 3, 0, 75, 45]], mode: 1 }, 0],
  ['Neo-Classical', 'Harmonic minor thirds and sixths', { voices: [[1, 1, 2], [1, 1, 5]], mode: 2 }, 0],
  ['Eastern Gold', 'Phrygian dominant thirds', { voices: [[1, 1, 2], [1, 1, -3]], mode: 14 }, 0],
  ['Thirds and Fifths', 'Classic three-part metal harmony', { voices: [[1, 1, 2], [1, 1, 4]] }, 1],
  ['Queen Choir', 'Stacked voices up to the octave', { voices: [[1, 1, 2], [1, 1, 4], [1, 1, 7], [1, 1, 9]], mode: 0 }, 1],
  ['Triad Below', 'A third and a fifth under you', { voices: [[1, 1, -2, 0, 70, -40], [1, 1, -4, 0, 60, 40]], mode: 1 }, 1],
  ['Seventh Heaven', 'Third, fifth and seventh: four-part, jazzy', { voices: [[1, 1, 2], [1, 1, 4], [1, 1, 6]], mode: 0 }, 1],
  ['Angel Wings', 'An octave and two octaves above', { voices: [[1, 1, 7, 0, 60, -50], [1, 1, 14, 0, 40, 50]], mode: 0 }, 1],
  ['Six Heads', 'All six voices: a wall of harmony', { voices: [[1, 1, 2], [1, 1, 4], [1, 1, 7], [1, 1, -3], [1, 1, -5], [1, 1, 9]] }, 1],
  ['Power Fifths', 'Fixed fifth and octave below: chords too', { voices: [[1, 0, 0, 7], [1, 0, 0, -12]] }, 2],
  ['Octave Stack', 'Octave down and up, organ-like', { voices: [[1, 0, 0, -12], [1, 0, 0, 12]] }, 2],
  ['Octave Up', 'Just the octave above: 12-string shimmer', { voices: [[1, 0, 0, 12, 65, 0]] }, 2],
  ['Fifth Below', 'A fixed fifth under you: huge and dark', { voices: [[1, 0, 0, -7, 80, 0]] }, 2],
  ['Doom Choir', 'Octaves below, slightly detuned', { voices: [[1, 0, 0, -12, 80, -40, 8], [1, 0, 0, -24, 60, 40, -8]] }, 2],
  ['Diminished Horror', 'Stacked minor thirds, unsettling', { voices: [[1, 1, 2], [1, 1, 4], [1, 1, 6]], mode: 22 }, 3],
  ['Tritone Ghost', 'A tritone above, slightly out of tune', { voices: [[1, 0, 0, 6, 65, 30, 12]] }, 3],
  ['Hungarian Night', 'Hungarian minor thirds and fifths', { voices: [[1, 1, 2], [1, 1, 4]], mode: 15 }, 3],
  ['Whole-Tone Dream', 'Whole-tone thirds: floating, unresolved', { voices: [[1, 1, 2, 0, 70, -50], [1, 1, 4, 0, 55, 50]], mode: 24 }, 3],
  ['Byzantine', 'Double harmonic thirds below and above', { voices: [[1, 1, 2], [1, 1, -2]], mode: 16 }, 3],
];
let prIdx = 0;
const prName = n => { prBtn.querySelector('small').textContent = n; };
function loadPre(i) {
  prIdx = (i + HPRE.length) % HPRE.length; const [n, , p] = HPRE[prIdx];
  S.voices.forEach((v, k) => { const d = p.voices[k]; v.on = d ? d[0] : 0; if (!d) return; v.smart = d[1]; if (d[1]) v.step = d[2]; else v.semi = d[3];
    v.level = d[4] != null ? d[4] : 75 - k * 5; v.pan = d[5] != null ? d[5] : [-35, 35, -60, 60, -15, 15][k]; v.detune = d[6] || 0; v.delay = 0; });
  if (p.mode != null) S.mode = p.mode;
  prName(n); changed();
}
// a voice as a short chip: "3RD ▲", "+7 ST"
const ORD = ['UNI', '2ND', '3RD', '4TH', '5TH', '6TH', '7TH', '8VA', '9TH', '10TH', '11TH', '12TH', '13TH', '14TH', '2 OCT'];
const chip = d => d[1] ? (ORD[Math.abs(d[2])] || Math.abs(d[2]) + ' ST') + (d[2] > 0 ? ' ▲' : d[2] < 0 ? ' ▼' : '') : sgn(d[3]) + ' ST';
const prMenu = menu(prBtn, m => {
  // one column per group, each in its own colour; every preset shows its voices as coloured chips
  const g = el('div', 'hm-prcols'), btns = [];
  HCATS.forEach(([cat, col, sub], ci) => {
    const c = el('div', 'hm-prcol'); c.style.setProperty('--cc', col);
    const mine = HPRE.map((p, i) => [p, i]).filter(([p]) => p[3] === ci);
    c.innerHTML = '<h4><span>' + cat + '</span><em>' + mine.length + '</em></h4><p>' + sub + '</p>';
    mine.forEach(([[n, d, p], i]) => {
      const b = el('button', null, '<b>' + n + '</b><small>' + d + '</small><span class="hm-chips">'
        + p.voices.map((v, k) => v[0] ? '<i style="--c:' + VC[k] + '">' + chip(v) + '</i>' : '').join('') + '</span>');
      b.onclick = () => loadPre(i); b.dataset.i = i; c.appendChild(b); btns.push(b);
    });
    g.appendChild(c);
  });
  m.appendChild(g); redraws.push(() => btns.forEach(b => b.classList.toggle('on', +b.dataset.i === prIdx)));
}, 40, 0);
prMenu.style.top = 'auto'; prMenu.style.bottom = '74px';
prNav.children[0].onclick = () => loadPre(prIdx - 1); prNav.children[1].onclick = () => loadPre(prIdx + 1);

/* ---- draw everything ---- */
redraws.push(() => {
  dev.classList.toggle('off', !S.on); pw.classList.toggle('on', S.on); pwV.textContent = S.on ? 'ON' : 'OFF'; pwV.style.color = S.on ? 'var(--accent)' : '';
  keyBtn.querySelector('b').textContent = NOTE[S.key];
  modeBtn.querySelector('b').textContent = MODE_LIST[S.mode].name;
  degs.innerHTML = scale().map(s => '<i' + (s ? '' : ' class="root"') + '>' + DEG[s] + '</i>').join('');
  const sc = scale();
  for (let m = LO; m <= HI; m++) { const k = keyEls[m], rel = ((m - S.key) % 12 + 12) % 12; k.classList.toggle('sc', sc.includes(rel)); k.classList.toggle('rt', rel === 0); k.classList.toggle('pl', played.includes(m)); }
  S.voices.forEach((v, i) => {
    const e = vEls[i]; e.c.classList.toggle('mute', !v.on); e.tg.classList.toggle('on', !!v.on);
    e.ms.children[0].classList.toggle('on', !!v.smart); e.ms.children[1].classList.toggle('on', !v.smart);
    const val = v.smart ? v.step : v.semi;
    e.iv.querySelector('.dir').textContent = val > 0 ? '▲ UP' : val < 0 ? '▼ DOWN' : '●';
    e.iv.querySelector('b').textContent = v.smart ? stepName(v.step) : sgn(v.semi) + ' ST';
    const ref = lead() != null ? lead() : 52 + S.key - 4;
    const out = voiceNote(ref, i);
    // smart voices: the semitones change with the key and mode, so show them (on the key's root when nothing is played)
    const root = 52 + ((S.key - 4) % 12 + 12) % 12, onRoot = voiceNote(root, i) - root;
    e.iv.querySelector('.live').innerHTML = v.smart ? (played.length ? (played.length > 1 ? 'now <em>' + played.map(p => sgn(voiceNote(p, i) - p)).join(' ') + ' st</em>' : 'now <em>' + sgn(out - ref) + ' st · ' + nn(out) + '</em>')
      : 'on ' + NOTE[S.key] + ' <em>' + sgn(onRoot) + ' st · ' + semiName(onRoot) + '</em>') : semiName(v.semi);
    heads[i].style.background = v.on && S.on ? VC[i] : ''; heads[i].style.boxShadow = v.on && S.on ? '0 0 8px ' + VC[i] : '';
  });
  let nd = 0;
  if (S.on) played.forEach(p => S.voices.forEach((v, i) => { if (!v.on) return; const o = voiceNote(p, i); if (o < LO || o > HI) return;
    const d = dot(nd++); d.style.display = ''; d.style.color = VC[i]; d.style.left = xOf(o) + '%'; d.style.top = [1, 3, 6, 8, 10].includes(((o % 12) + 12) % 12) ? '28%' : '70%'; }));
  for (let k = nd; k < dotPool.length; k++) dotPool[k].style.display = 'none';
  led.querySelector('.k').textContent = S.track ? 'PLAYING · CHORDS' : 'PLAYING · LEAD';
  if (played.length > 1) { ledN.className = 'chord'; ledN.innerHTML = played.slice().sort((a, b) => a - b).map(p => NOTE[p % 12]).join(' '); }
  else { ledN.className = 'n'; ledN.innerHTML = played.length ? NOTE[played[0] % 12] + '<small>' + (Math.floor(played[0] / 12) - 1) + '</small>' : '–'; }
  ledVs.innerHTML = S.voices.map((v, i) => { const ok = v.on && S.on && played.length > 0;
    if (!ok) return '<span style="--c:' + VC[i] + '" class="off">V' + (i + 1) + ' –</span>';
    if (played.length > 1) return '<span style="--c:' + VC[i] + '">' + played.slice().sort((a, b) => a - b).map(p => NOTE[voiceNote(p, i) % 12]).join(' ') + '</span>';
    const o = voiceNote(played[0], i); return '<span style="--c:' + VC[i] + '">' + nn(o) + '<em>' + sgn(o - played[0]) + '</em></span>'; }).join('');
});

/* ============================================================ preview sound (browser only) */
let ac = null, nodes = [];
let userHeld = false;          // a key held on the page (the plugin's live notes don't wipe it)
let liveWant = null, liveN = 0;   // CHORDS: the chord the engine reports, waiting to hold steady
function toggleChord(m) {
  userHeld = true;
  const had = played.includes(m);
  played = had ? played.filter(x => x !== m) : played.concat([m]);
  if (played.length > 6) { const gone = played.shift(); stopNote(gone); }
  if (!played.length) { stop(); return; }
  if (had) { stopNote(m); changed(); } else { changed(); startNote(m, Math.max(3, played.length)); }
}
function play(m) { userHeld = true; played = [m]; sound(played); }
function sound(notes) {
  changed();
  if (window.__JUCE__) return;
  try { ac = ac || new (window.AudioContext || window.webkitAudioContext)(); } catch (e) { return; }
  stopSound();
  notes.forEach(m => startNote(m, notes.length));
}
/** One played note and its harmonies (browser preview). n: how many notes share the level. */
function startNote(m, count) {
  if (window.__JUCE__) return;
  try { ac = ac || new (window.AudioContext || window.webkitAudioContext)(); } catch (e) { return; }
  const t = ac.currentTime, hz = n => 440 * Math.pow(2, (n - 69) / 12), mine = [];
  const voice = (n, lvl, p, det, dl) => {
    const o = ac.createOscillator(), g = ac.createGain(), f = ac.createBiquadFilter(), pn = ac.createStereoPanner ? ac.createStereoPanner() : null;
    o.type = 'sawtooth'; o.frequency.value = hz(n) * Math.pow(2, det / 1200); f.type = 'lowpass'; f.frequency.value = 2400;
    const st = t + dl / 1000; g.gain.setValueAtTime(0, st); g.gain.linearRampToValueAtTime(lvl * .09, st + .012); g.gain.setTargetAtTime(lvl * .05, st + .05, .4);
    o.connect(f); f.connect(g); if (pn) { pn.pan.value = clamp(p, -1, 1); g.connect(pn); pn.connect(ac.destination); } else g.connect(ac.destination);
    o.start(st); const nd = [o, g, m]; nodes.push(nd); mine.push(nd); };
  const mx = S.mix / 100, dryG = mx <= .5 ? 1 : 2 * (1 - mx), wetG = mx >= .5 ? 1 : 2 * mx;   // both at full in the middle (as in the plugin)
  const dry = dryG / Math.sqrt(count), lag = S.track ? 64 : 0;      // CHORDS listens longer (the plugin's chord voices are 64 ms behind)
  voice(m, dry, 0, 0, 0);
  if (S.on) S.voices.forEach((v, i) => { if (!v.on) return; const n = voiceNote(m, i); const sp = S.spread / 50;
    voice(n, v.level / 100 * wetG / Math.sqrt(count), v.pan / 100 * sp, v.detune + (Math.random() - .5) * S.human * .3, lag + v.delay + Math.random() * S.human * .25); });
}
function fadeOut(list) { const t = ac.currentTime; list.forEach(([o, g]) => { g.gain.cancelScheduledValues(t); g.gain.setTargetAtTime(0, t, .06); o.stop(t + .4); }); }
function stopNote(m) { if (!ac) return; fadeOut(nodes.filter(x => x[2] === m)); nodes = nodes.filter(x => x[2] !== m); }
function stopSound() { if (!ac) return; fadeOut(nodes); nodes = []; }
function stop() { userHeld = false; played = []; stopSound(); changed(); }
const KEYMAP = { a: 52, w: 53, s: 54, e: 55, d: 55, f: 57, t: 58, g: 59, h: 60, u: 61, j: 62, k: 64 };
const down = new Set();
addEventListener('keydown', e => { if (HP.style.display === 'none' || e.repeat || /INPUT/.test(e.target.tagName)) return; const m = KEYMAP[e.key.toLowerCase()]; if (m == null) return;
  down.add(m); if (S.track) { played = [...down]; sound(played); } else play(m); });
addEventListener('keyup', e => { const m = KEYMAP[e.key.toLowerCase()]; if (m == null || !down.has(m)) return; down.delete(m);
  if (S.track) { played = [...down]; if (played.length) sound(played); else stop(); } else if (played[0] === m) stop(); });

/* ============================================================ plugin hooks
   The plugin's parameters (plugin/Source/DSP/HarmonyDefs.h): harm.on, harm.key ... harm.v1.on ... harm.v6.delay.
   read() / write() use 0..1 like every other control; obsidian-bridge.js connects them. */
const PDEF = [['on', 'on', 0, 1], ['key', 'key', 0, 11], ['mode', 'mode', 0, MODE_LIST.length - 1], ['track', 'track', 0, 1], ['place', 'place', 0, 1],
  ['offkey', 'chroma', 0, 1], ['human', 'human', 0, 100], ['spread', 'spread', 0, 100], ['mix', 'mix', 0, 100], ['out', 'out', -24, 12]];
const VDEF = [['on', 0, 1], ['smart', 0, 1], ['step', -14, 14], ['semi', -24, 24], ['level', 0, 100], ['pan', -100, 100], ['detune', -50, 50], ['delay', 0, 120]];
const CONT = new Set(['human', 'spread', 'mix', 'out', 'level', 'pan', 'detune', 'delay']);      // the rest are whole numbers
const params = [];
PDEF.forEach(([id, key, lo, hi]) => params.push({ id: 'harm.' + id, lo, hi, cont: CONT.has(key), get: () => +S[key], set: v => { S[key] = key === 'on' ? !!v : v; } }));
S.voices.forEach((vc, i) => VDEF.forEach(([key, lo, hi]) => params.push({ id: 'harm.v' + (i + 1) + '.' + key, lo, hi, cont: CONT.has(key), get: () => +vc[key], set: v => { vc[key] = v; } })));
const byId = {}; params.forEach(q => byId[q.id] = q);
window.obsHarmony = { state: S, modes: MODE_LIST, play, stop, voiceNote, redraw: changed,
  ids: params.map(q => q.id).filter(id => id !== 'harm.on'),       // harm.on is the section switch: powerOut / setPower
  // harm.on is not reported here: the HARMONY tab owns it (two owners made double-click flicker on/off/on/off)
  read() { const o = {}; params.forEach(q => { if (q.id !== 'harm.on') o[q.id] = clamp((q.get() - q.lo) / (q.hi - q.lo), 0, 1); }); return o; },
  write(vals) { let any = false; for (const id in vals) { const q = byId[id]; if (!q) continue; let v = q.lo + clamp(+vals[id], 0, 1) * (q.hi - q.lo); if (!q.cont) v = Math.round(v); q.set(v); any = true; } if (any) changed(); },
  /* live from the engine: { note: midi or -1, chord: [midi...] } */
  live(l) {
    if (!l) return;
    const eng = S.track ? (l.chord || []).slice().sort((a, b) => a - b) : (l.note >= 0 ? [l.note] : []);
    const next = eng.length ? eng : (userHeld ? played : []);
    const key = next.join(',');
    if (key === played.join(',')) { liveWant = null; return; }
    // CHORDS: a new chord shows once it holds for 2 updates, an empty one after 5, so the keys don't flicker
    if (S.track && !userHeld) {
      if (liveWant !== key) { liveWant = key; liveN = 0; }
      if (++liveN < (next.length ? 2 : 5)) return;
    }
    liveWant = null; played = next; changed();
  },
  powerOut: null,             // set by the bridge: switches the HARMONY section
  setPower(isOn) { if (!!isOn !== !!S.on) { S.on = !!isOn; changed(); } },
  presets: HPRE, loadPreset: loadPre,
};
changed();
})();
</script>
