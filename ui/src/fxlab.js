<script>
/* =============================================================================
   GLITCH: the SFX tab's second page (the switch on top: RACK | GLITCH).
     KERAUNOS  a multi-effect expression pedal: whammy, dive bombs, lasers,
               ring mod, talk box... moved by the treadle, your picking
               (ENVELOPE), by itself (AUTO) or a sweep on each pick (TRIGGER).
     KHAOS     a tempo-synced glitch engine: 22 glitch types in five groups,
               fired by chance (RANDOM) or drawn into 16 steps (SEQUENCE).
   Built with SIREN's controls (window.__sirenKit), on a faceplate of its own.
   The engine is plugin/Source/DSP/FxLab.h (settings: FxLabDefs.h, mirrored
   below); in a browser the page animates by itself (no sound).
   ============================================================================= */
(function () {
'use strict';
const K = window.__sirenKit; if (!K) return;
const { el, P, S, set, onChange, on, toN, toV, clamp, sg, knob, seg, pill, mknob, sel, segBool, row, spacer, section, IC, hidpi, tc, accent, FONT } = K;
const inPlugin = K.inPlugin;
const SFX = document.querySelector('.page[data-page="SFX"]'); if (!SFX) return;

/* ------------------------------------------------------------ the settings (as FxLabDefs.h) */
const ALGOS = ['Whammy Up', 'Whammy Down', 'Dive Bomb', 'Fifth Up', 'Oct + 5th', 'Squeal', 'Detune Drop', 'Laser', 'Zap Comb', 'Freq Shift', 'Ring Mod', 'Ring Tracking', 'Wah', 'Talk Box', 'Filter', 'Bit Crush'];
const ALGO_SHORT = ['WHAMMY ↑', 'WHAMMY ↓', 'DIVE BOMB', 'FIFTH UP', 'OCT + 5TH', 'SQUEAL', 'DETUNE', 'LASER', 'ZAP COMB', 'FREQ SHIFT', 'RING MOD', 'RING TRACK', 'WAH', 'TALK BOX', 'FILTER', 'BIT CRUSH'];
const AGROUPS = [['PITCH', '#f2b14c', [0, 1, 2, 3, 4, 5, 6]], ['LASER', '#3fd0c0', [7, 8, 9]], ['RING', '#b48cff', [10, 11]], ['FILTER', '#ff7b8c', [12, 13, 14, 15]]];
const algoCol = a => AGROUPS.find(g => g[2].includes(a))[1];
const TYPES = ['Stutter', 'Roll', 'Reverse', 'Scatter', 'Skip', 'Freeze', 'Pitch', 'Tape Stop', 'Spin Up', 'Half Speed', 'Warble', 'Crush', 'Chop', 'Dropout', 'Data', 'GSM', 'Radio', 'Hum', 'Static', 'Grains', 'Echo', 'Vinyl'];
const TKEY = ['stutter', 'roll', 'reverse', 'scatter', 'skip', 'freeze', 'pitch', 'tapestop', 'spinup', 'halfspeed', 'warble', 'crush', 'chop', 'dropout', 'data', 'gsm', 'radio', 'hum', 'static', 'grains', 'echo', 'vinyl'];
const TCODE = ['STUT', 'ROLL', 'REV', 'SCAT', 'SKIP', 'FRZ', 'PTCH', 'TAPE', 'SPIN', 'HALF', 'WARB', 'CRSH', 'CHOP', 'DROP', 'DATA', 'GSM', 'RDIO', 'HUM', 'STAT', 'GRN', 'ECHO', 'VNYL'];
const TGROUPS = [['TIME', '#7cc6ff', [0, 1, 2, 3, 4, 5]], ['PITCH', '#f2b14c', [6, 7, 8, 9, 10]], ['DIGITAL', '#9dff8a', [11, 12, 13, 14]], ['INTERFERENCE', '#ff8a5c', [15, 16, 17, 18]], ['TEXTURE', '#c69bff', [19, 20, 21]]];
const typeCol = t => t < 0 ? '#e9e6df' : TGROUPS.find(g => g[2].includes(t))[1];
const TYPE_TIP = [
  'Stutter: one slice repeated (a step, a half, a quarter)', 'Roll: repeats that speed up, like a drum roll', 'Reverse: the last beat played backwards',
  'Scatter: little slices from all over the last bars, shuffled', 'Skip: jumps around like a scratched CD', 'Freeze: holds one moment and lets it ring',
  'Pitch: the slice jumps up or down (PITCH and DIRECTION)', 'Tape Stop: slows down to a stop', 'Spin Up: starts from nothing and spins up to speed',
  'Half Speed: an octave down, half as fast', 'Warble: a wobbly, worn-out tape', 'Crush: fewer bits and a lower sample rate', 'Chop: a hard on / off gate in time',
  'Dropout: the sound cuts out like a bad cable', 'Data: digital corruption, broken buffers', 'GSM: the buzz a phone makes near a speaker', 'Radio: a thin, detuned AM radio',
  'Hum: mains hum and a ground loop', 'Static: crackle and noise bursts', 'Grains: a cloud of tiny grains of the last bars', 'Echo: an echo throw that keeps ringing', 'Vinyl: a slowed, crackling record'];
const T_ON = [1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0];
const SEQ_DEF = [0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 0, 8, 0];
const GRIDS = ['1/4', '1/8', '1/8T', '1/16', '1/16T', '1/32'], GRID_BEATS = [1, .5, 1 / 3, .25, 1 / 6, .125];
const DEFS = [
  ['ker.on', 'bool', 0, 1, 0], ['ker.algo', 'choice', 0, 15, 0, ALGOS.join('|')], ['ker.pos', 'f', 0, 100, 0, '%'], ['ker.mode', 'choice', 0, 3, 0, 'Pedal|Envelope|Auto|Trigger'],
  ['ker.speed', 'f', 0, 100, 50, '%'], ['ker.sens', 'f', 0, 100, 50, '%'], ['ker.shape', 'choice', 0, 3, 0, 'Up|Down|Up and down|Random'], ['ker.color', 'f', 0, 100, 50, '%'],
  ['ker.mix', 'f', 0, 100, 100, '%'], ['ker.level', 'f', -12, 12, 0, 'dB'], ['ker.place', 'choice', 0, 1, 0, 'Before amp|After amp'], ['ker.trig', 'bool', 0, 1, 0],
  ['glt.on', 'bool', 0, 1, 0], ['glt.mode', 'choice', 0, 1, 0, 'Random|Sequence'], ['glt.grid', 'choice', 0, 5, 3, GRIDS.join('|')], ['glt.loop', 'choice', 0, 3, 1, '1/2 bar|1 bar|2 bars|4 bars'],
  ['glt.chance', 'f', 0, 100, 35, '%'], ['glt.len', 'choice', 0, 3, 1, '1 step|2 steps|4 steps|8 steps'], ['glt.chaos', 'f', 0, 100, 40, '%'], ['glt.pitch', 'f', 0, 24, 12, 'st'],
  ['glt.dir', 'choice', 0, 2, 2, 'Up|Down|Both'], ['glt.filter', 'f', -100, 100, 0, '%'], ['glt.mix', 'f', 0, 100, 100, '%'], ['glt.level', 'f', -12, 12, 0, 'dB'], ['glt.hold', 'bool', 0, 1, 0],
  ...TKEY.map((k, i) => ['glt.t.' + k, 'bool', 0, 1, T_ON[i]]),
  ...SEQ_DEF.map((v, i) => ['glt.s' + (i + 1), 'choice', 0, 22, v, '-|' + TYPES.join('|')]),
];
DEFS.forEach(([id, type, min, max, def, extra]) => {
  P[id] = { id, type, min, max, def, skew: 1, unit: type === 'choice' ? '' : (extra || ''), opts: type === 'choice' ? extra.split('|') : null };
  S[id] = def;
});
const IDS = DEFS.map(d => d[0]);
const isFx = id => id.startsWith('ker.') || id.startsWith('glt.');
const PERFORM = ['ker.pos', 'ker.trig', 'glt.hold'];                  // played, not part of a sound
const stepId = i => 'glt.s' + (i + 1), typeId = t => 'glt.t.' + TKEY[t];
const stepType = i => Math.round(S[stepId(i)]) - 1;
const mode = () => Math.round(S['ker.mode']), algo = () => Math.round(S['ker.algo']);
const seqMode = () => Math.round(S['glt.mode']) === 1;

/* value text */
const hzT = v => v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2) + ' kHz' : Math.round(v) + ' Hz';
const msT = s => s >= 1 ? s.toFixed(2) + ' s' : Math.round(s * 1000) + ' ms';
function fmt(id, v = S[id]) {
  const p = P[id];
  switch (id) {
    case 'ker.speed': { const s = v / 100;
      if (mode() === 2) { const hz = .05 * Math.pow(2, s * 7.3); return (hz < 1 ? hz.toFixed(2) : hz.toFixed(1)) + ' Hz'; }
      if (mode() === 3) return msT(1.5 * Math.pow(2, -s * 6.2));
      if (mode() === 1) return msT((40 + (1 - s) * 1200) / 1000);
      return Math.round(v) + '%'; }
    case 'ker.level': case 'glt.level': return sg(v, 1) + ' dB';
    case 'glt.filter': return Math.abs(v) < 2 ? 'Open' : (v < 0 ? 'Low ' : 'High ') + Math.round(Math.abs(v)) + '%';
    case 'glt.pitch': return Math.round(v) + ' st';
  }
  if (p.opts) return p.opts[Math.round(v)] || '';
  if (p.unit === '%') return Math.round(v) + '%';
  return String(Math.round(v));
}
/* what the pedal does at a position (0..1), as in FxLab.h */
function effectAt(a, x) {
  const st = s => (Math.abs(s) < .05 ? '0' : sg(s, Math.abs(s) < 10 ? 1 : 0)) + ' st';
  switch (a) {
    case 0: return { y: x, t: st(12 * x), heel: 'DRY', toe: '+1 OCT' };
    case 1: return { y: 1 - x, t: st(-12 * x), heel: 'DRY', toe: '−1 OCT' };
    case 2: { const s = x * x * (3 - 2 * x); return { y: 1 - s, t: st(-24 * s), heel: 'DRY', toe: '−2 OCT' }; }
    case 3: return { y: x, t: st(7 * x), heel: 'DRY', toe: '+5TH' };
    case 4: return { y: x, t: 'OCT + ' + Math.round(100 * x) + '% 5TH', heel: '+1 OCT', toe: '+OCT +5TH' };
    case 5: return { y: x, t: st(12 + 12 * x), heel: '+1 OCT', toe: '+2 OCT' };
    case 6: return { y: 1 - x, t: st(-1.6 * x), heel: 'DRY', toe: 'DETUNED' };
    case 7: { const f = 1 / (.010 * Math.pow(2, -x * 5.2)); return { y: Math.log2(f / 100) / 5.2, t: hzT(f), heel: '100 Hz', toe: '3.7 kHz', log: 1 }; }
    case 8: { const f = 1 / (.006 * Math.pow(2, -x * 4)); return { y: x, t: hzT(f), heel: '167 Hz', toe: '2.7 kHz', log: 1 }; }
    case 9: { const f = (x - .5) * 1800; return { y: x, t: (f >= 0 ? '+' : '−') + Math.round(Math.abs(f)) + ' Hz', heel: '−900 Hz', toe: '+900 Hz', mid: 1 }; }
    case 10: { const f = 30 * Math.pow(2, x * 6.1); return { y: x, t: hzT(f), heel: '30 Hz', toe: '2 kHz', log: 1 }; }
    case 11: { const r = .5 * Math.pow(2, x * 3); return { y: x, t: (r < 1 ? r.toFixed(2) : r.toFixed(1)) + ' × NOTE', heel: '½ × NOTE', toe: '4 × NOTE', log: 1 }; }
    case 12: { const f = 350 * Math.pow(2, x * 3); return { y: x, t: hzT(f), heel: 'HEEL', toe: 'TOE', log: 1 }; }
    case 13: { const V = ['U', 'O', 'A', 'E', 'I'], i = Math.min(4, Math.round(x * 4)); return { y: x, t: '"' + V[i] + '"', heel: '"U"', toe: '"I"', vowels: 1 }; }
    case 14: { const f = 80 * Math.pow(2, x * 8); return { y: x, t: hzT(f), heel: '80 Hz', toe: '20 kHz', log: 1 }; }
    default: { const h = 1 + Math.floor(x * x * 48); return { y: x * x, t: x < .02 ? 'CLEAN' : hzT(48000 / h), heel: 'CLEAN', toe: '1 kHz' }; }
  }
}
const ALGO_TIP = [
  'Whammy up: the heel is dry, the toe an octave up', 'Whammy down: the toe is an octave down', 'Dive bomb: two octaves down, like a floating trem',
  'Fifth up: rises a fifth', 'Octave + fifth: an octave, a fifth blends in as you press', 'Squeal: one to two octaves up, screaming', 'Detune drop: a slightly flat, wide double',
  'Laser: a resonant comb that zaps up (COLOR: more resonance)', 'Zap comb: a phaser and a comb, sci-fi sweeps', 'Freq shift: shifts every frequency by Hz (metallic, out of tune on purpose)',
  'Ring mod: a bell-like ring that sweeps (30 Hz to 2 kHz)', 'Ring tracking: a ring that follows the note you play (robot voice)',
  'Wah: a classic wah (COLOR: sharper)', 'Talk box: vowels U-O-A-E-I', 'Filter: a resonant low-pass sweep (COLOR: resonance)', 'Bit crush: down to 1 kHz and fewer bits (COLOR: fewer bits)'];
const MODE_HINT = () => {
  const s = fmt('ker.speed');
  return ['Drag the pedal (or move it with a MIDI expression pedal)', 'Your picking moves it: harder goes further (back in ' + s + ')',
    'It rocks by itself at ' + s, 'Each pick attack (or TRIGGER) sweeps it in ' + s][mode()];
};

/* ------------------------------------------------------------ icons */
const svgI = (d, w = 1.8, extra = '') => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="' + w + '" stroke-linecap="round" stroke-linejoin="round">' + (d ? '<path d="' + d + '"/>' : '') + extra + '</svg>';
const GLYPH = [
  'M5 7v10M9.5 7v10M14 7v10M18.5 7v10',                                         // stutter
  'M3 8v8M9 8v8M13 8v8M15.6 8v8M17.6 8v8M19.2 8v8M20.5 8v8',                     // roll
  'M20 12H5M10 6.5L4.5 12l5.5 5.5',                                               // reverse
  'M4 15h3M10 7h3M15 17h3M18 9h3M7 10.5h2M13 12.5h2',                             // scatter
  'M3 17h3M18 17h3M6 17c1-10 11-10 12 0',                                         // skip
  'M12 3v18M4.2 7.5l15.6 9M4.2 16.5l15.6-9',                                      // freeze
  'M8 19V5M4 9l4-4 4 4M16 5v14M12 15l4 4 4-4',                                    // pitch
  'M3 6c6 0 9 3 11 7.5S18 19 21 19',                                              // tape stop
  'M3 19c3 0 5-.5 7-5S15 5 21 5',                                                 // spin up
  'M5 19L19 5M5 5h3v6M14 14.5c0-1.4 1-2 2.2-2s2 .7 2 1.8c0 1.8-4.2 3-4.2 5.2h4.4',  // half speed
  'M2 12c2-5 3-5 5 0s3 5 5 0 3-5 5 0 3 5 5 0',                                   // warble
  'M3 19h3v-3h3v-4h3V9h3V6h3V3',                                                  // crush
  'M3 16h3V8h4v8h3V8h4v8h4',                                                      // chop
  'M2 12h6M16 12h6M10 9l4 6M14 9l-4 6',                                           // dropout
  'M4 6h4v4H4zM11 6h2v4h-2zM16 6h4v4h-4zM4 14h2v4H4zM9 14h4v4H9zM16 14h4v4h-4z',  // data
  'M3 12h2M6 7v10M8.5 7v10M11 12h3M15 7v10M17.5 7v10M20 12h1',                   // gsm
  'M12 21v-9M9 8.8a4.2 4.2 0 016 0M6 5.8a8.5 8.5 0 0112 0',                        // radio
  'M2 12c3-7 6-7 9 0s6 7 9 0',                                                    // hum
  'M2 12l2-5 2 9 2-7 2 6 2-8 2 9 2-5 2 4 2-6 2 3',                                // static
  'M5 8h.01M9 14h.01M12 7h.01M15 12h.01M18 16h.01M7 17h.01M17 6h.01M11 18h.01',   // grains
  'M5 12h.01M9 7.5a6.5 6.5 0 010 9M13 5a10 10 0 010 14M17 3a13 13 0 010 18',     // echo
  'M12 3a9 9 0 110 18 9 9 0 010-18zM12 9.5a2.5 2.5 0 110 5 2.5 2.5 0 010-5zM6.5 9A6 6 0 019 6.5',   // vinyl
];
const glyph = i => svgI(GLYPH[i], i === 19 ? 3.4 : 1.8);
const GLYPH_PATHS = GLYPH.map(d => { try { return new Path2D(d); } catch (_) { return null; } });
const BOLT = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linejoin="round"><path d="M13.5 2.5L5 13.5h6.2L10 21.5l9-11.5h-6.3z"/></svg>';
const STAR = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.1" stroke-linecap="round"><path d="M12 2.5v6M12 15.5v6M2.5 12h6M15.5 12h6M5.3 5.3l4.2 4.2M14.5 14.5l4.2 4.2M18.7 5.3l-4.2 4.2M9.5 14.5l-4.2 4.2"/></svg>';
const SNOW = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M12 3v18M4.2 7.5l15.6 9M4.2 16.5l15.6-9M9.5 4.5L12 6.5l2.5-2M9.5 19.5l2.5-2 2.5 2"/></svg>';
const TARGET = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><circle cx="12" cy="12" r="6.5"/><circle cx="12" cy="12" r="1.6" fill="currentColor"/><path d="M12 2.5v4M12 17.5v4M2.5 12h4M17.5 12h4"/></svg>';
const CLEAR = '<svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 7h16M9 7V4.5h6V7M6.5 7l1 13h9l1-13"/></svg>';

/* ------------------------------------------------------------ styles */
const css = el('style');
css.textContent = `
.fl-page { flex: 1; min-height: 0; position: relative; display: none; }
.page[data-page="SFX"].fl-glitch > .fl-rack { display: none !important; }
.page[data-page="SFX"].fl-glitch > .fl-page { display: block; }
.fl-pagesw { top: 25px !important; }
.fl-root { position: absolute; inset: 0; display: flex; align-items: flex-start; justify-content: center; padding-top: 8px; user-select: none; }
.fl-dev { --c-ker: #6fa8ff; --c-glt: #ff5fa2;
  width: 1320px; height: 572px; box-sizing: border-box; position: relative; border-radius: 22px;
  background: radial-gradient(90% 60% at 75% 0%, rgba(255,255,255,.07), transparent 60%), repeating-linear-gradient(90deg, rgba(255,255,255,.014) 0 1px, transparent 1px 3px), linear-gradient(172deg, #232327, #141416 55%, #0e0e10);
  box-shadow: inset 0 1px 0 rgba(255,255,255,.13), inset 0 0 0 1px rgba(255,255,255,.05), inset 0 -3px 8px rgba(0,0,0,.6), 0 26px 54px rgba(0,0,0,.55), 0 2px 0 rgba(0,0,0,.8); }
.fl-dev::before { content: ''; position: absolute; inset: 9px; border-radius: 16px; border: 1px solid var(--accent); opacity: .3; pointer-events: none; }
.fl-dev .sy-top { top: 20px; }
.fl-dev .sy-hw.dim { opacity: .35; }
.fl-dev .sy-hw.dim .sy-mk svg line.on { stroke: rgba(233,230,223,.4); filter: none; }
.fl-dev.sfx-off .sy-screen, .fl-dev.sfx-off .fl-led > * { opacity: .38; filter: grayscale(.8); }
.fl-banner { position: absolute; z-index: 7; left: 50%; top: 146px; transform: translateX(-50%); display: none; align-items: center; gap: 10px; height: 24px; padding: 0 12px; border-radius: 12px;
  background: #1a1a1d; box-shadow: inset 0 0 0 1px rgba(255,255,255,.1), 0 4px 10px rgba(0,0,0,.6); font-size: 9px; font-weight: 600; letter-spacing: .14em; color: #e9e6df; white-space: nowrap; }
.fl-banner button { height: 18px; border: none; border-radius: 9px; padding: 0 9px; background: var(--accent); color: #111113; font: inherit; font-size: 8px; letter-spacing: .14em; cursor: pointer; }
.fl-dev.sfx-off .fl-banner { display: flex; }
/* the display */
.fl-led { position: relative; width: 218px; height: 100px; margin-top: 24px; border-radius: 10px; box-sizing: border-box; padding: 7px 12px 6px; display: flex; flex-direction: column;
  background: linear-gradient(#060607, #0b0b0d); box-shadow: inset 0 2px 8px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.04), 0 1px 0 rgba(255,255,255,.08); }
.fl-led .ln { display: flex; align-items: baseline; justify-content: space-between; gap: 8px; height: 11px; }
.fl-led .u { font-size: 7px; letter-spacing: .16em; color: #6f6f76; white-space: nowrap; display: flex; align-items: center; gap: 5px; }
.fl-led .u svg { width: 8px; height: 8px; }
.fl-led .big { display: flex; align-items: baseline; justify-content: space-between; gap: 6px; height: 19px; font-family: 'Orbitron', 'Saira', sans-serif; }
.fl-led .nm { font-size: 13px; letter-spacing: .05em; color: var(--lc); text-shadow: 0 0 10px var(--lc); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; min-width: 0; }
.fl-led .vv { font-size: 9px; letter-spacing: .04em; color: var(--lc); opacity: .8; white-space: nowrap; flex: none; }
.fl-led .bar { height: 2px; border-radius: 1px; background: #19191c; overflow: hidden; margin-top: 2px; }
.fl-led .bar i { display: block; height: 100%; width: 0; background: var(--lc); box-shadow: 0 0 6px var(--lc); }
.fl-led .sep { height: 1px; margin: 5px 0 4px; background: rgba(255,255,255,.05); }
.fl-led .dots { display: flex; gap: 2px; margin-top: 3px; }
.fl-led .dots i { flex: 1; height: 3px; border-radius: 1px; background: #1c1c20; }
.fl-led .dots i.q { background: #2a2a30; }
.fl-led .dots i.g { background: var(--gc); opacity: .55; }
.fl-led .dots i.p { background: #f1eee7; box-shadow: 0 0 5px #f1eee7; opacity: 1; }
.fl-led .k, .fl-led .g { transition: opacity .2s; }
.fl-led.k-off .k, .fl-led.g-off .g { opacity: .22; }
.fl-led .g .nm.flash { animation: flFlash .16s steps(2) 2; }
@keyframes flFlash { 50% { opacity: .35; } }
/* KERAUNOS */
.fl-kbody { display: flex; gap: 14px; flex: 1; min-height: 0; }
.fl-kcol { display: flex; flex-direction: column; gap: 8px; min-width: 0; flex: 1; }
.fl-lcd { height: 78px; cursor: ew-resize; }
.fl-alg { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 3px; flex: none; }
.fl-alg button { height: 23px; border: none; border-radius: 6px; background: #121215; box-shadow: inset 0 0 0 1px rgba(255,255,255,.06); color: #8d8d93; font: inherit; font-size: 8px; font-weight: 600; letter-spacing: .07em;
  cursor: pointer; display: flex; align-items: center; gap: 6px; padding: 0 7px; white-space: nowrap; overflow: hidden; min-width: 0; }
.fl-alg button i { width: 5px; height: 5px; border-radius: 50%; background: var(--ac); opacity: .5; flex: none; }
.fl-alg button:hover { color: #e9e6df; box-shadow: inset 0 0 0 1px rgba(255,255,255,.14); }
.fl-alg button.on { color: #f1eee7; background: linear-gradient(color-mix(in srgb, var(--ac) 24%, #18181b), #121215); box-shadow: inset 0 0 0 1px var(--ac), 0 0 12px color-mix(in srgb, var(--ac) 28%, transparent); }
.fl-alg button.on i { opacity: 1; box-shadow: 0 0 6px var(--ac); }
.fl-krow { display: flex; align-items: flex-start; justify-content: space-between; flex: none; }
.fl-krow .sy-seg { width: 128px; margin-top: 2px; }
.fl-lbl { font-size: 8.5px; font-weight: 600; letter-spacing: .16em; color: #a3a3a8; white-space: nowrap; text-align: center; margin-top: 5px; }
.fl-stack { display: flex; flex-direction: column; align-items: center; }
/* the treadle */
.fl-tread { width: 104px; flex: none; display: flex; flex-direction: column; align-items: center; gap: 7px; }
.fl-tread .cap { font-size: 8.5px; font-weight: 600; letter-spacing: .18em; color: #a3a3a8; white-space: nowrap; }
.fl-tread .cap b { color: #e9e6df; font-variant-numeric: tabular-nums; margin-left: 4px; }
.fl-pw { display: flex; gap: 8px; height: 172px; }
.fl-ped { position: relative; width: 74px; height: 172px; perspective: 420px; cursor: ns-resize; touch-action: none; }
.fl-ped .base { position: absolute; left: -5px; right: -5px; top: 8px; bottom: -6px; border-radius: 12px; background: linear-gradient(#19191c, #09090a); box-shadow: 0 10px 16px rgba(0,0,0,.85), inset 0 1px 0 rgba(255,255,255,.06); }
.fl-ped .plate { position: absolute; inset: 0; border-radius: 10px; transform-origin: 50% 58%; overflow: hidden;
  background: linear-gradient(#3b3b41, #232327 10%, #19191c 90%, #2b2b30); box-shadow: inset 0 1px 0 rgba(255,255,255,.2), inset 0 0 0 1px rgba(255,255,255,.07), 0 7px 10px rgba(0,0,0,.75); }
.fl-ped .grip { position: absolute; left: 9px; right: 9px; top: 24px; bottom: 12px; border-radius: 6px; background: repeating-linear-gradient(180deg, #0e0e10 0 5px, #27272c 5px 7px); box-shadow: inset 0 1px 3px rgba(0,0,0,.85); }
.fl-ped .toe { position: absolute; left: 50%; top: 9px; width: 24px; height: 5px; margin-left: -12px; border-radius: 3px; background: var(--gc); }
.fl-ped .logo { position: absolute; left: 0; right: 0; bottom: 30px; display: flex; justify-content: center; color: #4a4a50; }
.fl-ped .logo svg { width: 18px; height: 18px; filter: drop-shadow(0 1px 0 rgba(0,0,0,.9)); }
.fl-ped .sh { position: absolute; left: 0; right: 0; height: 50%; pointer-events: none; }
.fl-ped .sh.t { top: 0; background: linear-gradient(#000, transparent); }
.fl-ped .sh.b { bottom: 0; background: linear-gradient(transparent, #000); }
.fl-ped:hover .plate { box-shadow: inset 0 1px 0 rgba(255,255,255,.2), inset 0 0 0 1px rgba(255,255,255,.16), 0 7px 10px rgba(0,0,0,.75); }
.fl-pbar { width: 6px; display: flex; flex-direction: column-reverse; gap: 2px; padding: 3px 0; }
.fl-pbar i { flex: 1; border-radius: 1px; background: #1b1b1f; }
.fl-pbar i.on { background: var(--gc); box-shadow: 0 0 4px var(--gc); }
.fl-tread .hint { font-size: 8.5px; line-height: 1.4; color: #6f6f76; text-align: center; height: 36px; overflow: hidden; }
/* KHAOS */
.fl-gbody { display: flex; flex-direction: column; gap: 8px; flex: 1; min-height: 0; }
.fl-lane { height: 78px; cursor: crosshair; }
.fl-pal { display: flex; flex-direction: column; gap: 6px; flex: none; }
.fl-prow { display: flex; gap: 12px; }
.fl-grp { display: flex; flex-direction: column; gap: 4px; min-width: 0; }
.fl-grp > span { height: 10px; font-size: 7.5px; font-weight: 600; letter-spacing: .2em; color: var(--tc); cursor: pointer; white-space: nowrap; display: flex; align-items: center; gap: 6px; }
.fl-grp > span::before { content: ''; width: 12px; height: 2px; border-radius: 1px; background: var(--tc); box-shadow: 0 0 5px var(--tc); }
.fl-grp > span:hover { filter: brightness(1.3); }
.fl-grp > div { display: flex; gap: 4px; }
.fl-type { flex: 1; min-width: 0; height: 32px; border: none; border-radius: 7px; padding: 0 2px; cursor: pointer; font: inherit; color: #6f6f76; background: #111114; box-shadow: inset 0 0 0 1px rgba(255,255,255,.06);
  display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 3px; transition: background .1s, box-shadow .1s, color .1s; }
.fl-type svg { width: 13px; height: 13px; flex: none; }
.fl-type b { font-size: 7.5px; font-weight: 600; letter-spacing: .07em; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; max-width: 100%; }
.fl-type:hover { color: #cfccc5; box-shadow: inset 0 0 0 1px rgba(255,255,255,.15); }
.fl-type.on { color: #f1eee7; background: linear-gradient(color-mix(in srgb, var(--tc) 28%, #17171a), color-mix(in srgb, var(--tc) 9%, #0f0f11)); box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--tc) 65%, transparent); }
.fl-type.on svg { color: var(--tc); filter: drop-shadow(0 0 3px var(--tc)); }
.fl-type.arm { color: #f1eee7; box-shadow: inset 0 0 0 1.5px var(--tc), 0 0 14px color-mix(in srgb, var(--tc) 50%, transparent); }
.fl-type.arm svg { color: var(--tc); }
.fl-type.play { background: var(--tc); color: #0b0b0c; box-shadow: 0 0 18px var(--tc); transition: none; }
.fl-type.play svg { color: #0b0b0c; filter: none; }
.fl-grow { display: flex; align-items: flex-start; gap: 22px; flex: none; }
.fl-grow .sy-seg { width: 112px; margin-top: 2px; }
.fl-hint { flex: 1; min-width: 0; align-self: center; font-size: 9.5px; line-height: 1.45; color: #8d8d93; padding: 6px 12px; border-radius: 9px; background: #0c0c0e; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05); }
.fl-hint b { color: #e9e6df; font-weight: 600; }
.fl-hint i { font-style: normal; color: var(--c-glt); }
.fl-ic { width: 24px; height: 22px; border-radius: 6px; border: 1px solid rgba(255,255,255,.1); background: none; color: #8d8d93; cursor: pointer; display: flex; align-items: center; justify-content: center; padding: 0; flex: none; }
.fl-ic:hover { color: #e9e6df; border-color: rgba(255,255,255,.22); }
.fl-ic:disabled { opacity: .25; cursor: default; }
.fl-dev .sy-sec .sy-sh { gap: 7px; }
.fl-dev .sy-sh .sy-seg { width: auto; }
.fl-dev .sy-sh .sy-seg button { padding: 0 9px; }
.fl-dev .sy-sh .sy-pill { height: 22px; }
.fl-sub { font-size: 8px; letter-spacing: .14em; color: #55555b; white-space: nowrap; }
.fl-dev .sy-badge b { font-size: 24px; letter-spacing: .26em; margin-right: -.26em; }
.fl-dev .sy-badge b em { font-style: normal; font-size: 14px; vertical-align: 4px; margin: 0 .2em 0 0; -webkit-text-fill-color: var(--accent); color: var(--accent); }
.fl-pb-mom.lit .sy-pb { box-shadow: 0 1px 0 rgba(255,255,255,.1), 0 3px 6px rgba(0,0,0,.7), inset 0 2px 3px rgba(0,0,0,.8), 0 0 22px color-mix(in srgb, var(--mc) 60%, transparent); }
.fl-pb-mom.lit .sy-pb .cap { color: var(--mc); transform: translateY(2px); }
.fl-pb-mom.lit .sy-pbv { color: var(--mc); text-shadow: 0 0 8px var(--mc); }
`;
document.head.appendChild(css);

/* ------------------------------------------------------------ the page and its switch */
const rack = SFX.firstElementChild; if (rack) rack.classList.add('fl-rack');
const pageEl = el('div', 'fl-page'), rootEl = el('div', 'fl-root'), dev = el('div', 'fl-dev');
rootEl.appendChild(dev); pageEl.appendChild(rootEl); SFX.appendChild(pageEl);
['tl', 'tr', 'bl', 'br'].forEach(c => dev.appendChild(el('i', 'sy-screw ' + c)));
const pageSw = el('div', 'sy-page fl-pagesw', '<button data-p="rack" title="RACK: your special effects pedals"><i></i>RACK</button><button data-p="glitch" title="GLITCH: KERAUNOS (lasers, whammy, ring mod) and KHAOS (glitches)"><i></i>GLITCH</button>');
SFX.appendChild(pageSw);
let page = 'rack';
try { if (sessionStorage.getItem('obs.sfxPage') === 'glitch') page = 'glitch'; } catch (_) {}
function showPage(p) {
  page = p === 'glitch' ? 'glitch' : 'rack';
  SFX.classList.toggle('fl-glitch', page === 'glitch');
  [...pageSw.children].forEach(b => b.classList.toggle('on', b.dataset.p === page));
  try { sessionStorage.setItem('obs.sfxPage', page); } catch (_) {}
  if (page === 'glitch') { prOpen(false); setTimeout(redraw, 20); }
}
pageSw.addEventListener('click', e => { const b = e.target.closest('button'); if (b) showPage(b.dataset.p); });
const pageDots = () => { pageSw.children[1].firstChild.classList.toggle('lit', on('ker.on') || on('glt.on')); };

/* send right away (momentary buttons): the bridge sets this */
const sendNow = id => { if (window.obsFxLab && window.obsFxLab.paramOut) window.obsFxLab.paramOut(id, toN(id)); };
let sfxOn = true;
const wakeSection = () => { if (!sfxOn && window.obsFxLab && window.obsFxLab.sectionOut) window.obsFxLab.sectionOut(true); };
onChange((id, v, from) => { if ((id === 'ker.on' || id === 'glt.on') && v >= .5 && from === 'ui') wakeSection(); });

/* ------------------------------------------------------------ hardware: a latching power button, a momentary one */
function powerBtn(id, label, icon, tip) {
  const w = K.pushBtn(() => on(id), v => set(id, v ? 1 : 0, 'ui'), label, icon, 'ON', 'OFF', tip);
  w._btn.dataset.param = id;
  onChange(i => { if (i === id) w._draw(); });
  return w;
}
function momBtn(label, icon, tip, color, down, up) {
  const w = el('div', 'sy-hw fl-pb-mom'); w.style.setProperty('--mc', color);
  w.appendChild(el('span', 'sy-eng', label));
  const b = el('button', 'sy-pb', '<span class="cap">' + icon + '</span>'); b.title = tip; b.setAttribute('aria-label', label);
  const v = el('span', 'sy-pbv', 'PRESS');
  let t0 = 0;
  b.addEventListener('pointerdown', e => { if (e.button !== 0) return; e.preventDefault(); b.setPointerCapture(e.pointerId); t0 = performance.now(); down(v); });
  const end = () => { if (!t0) return; const held = performance.now() - t0; t0 = 0; up(v, held); };
  b.addEventListener('pointerup', end); b.addEventListener('pointercancel', end);
  w.append(b, v); w._v = v; w._b = b; return w;
}

/* ------------------------------------------------------------ top strip */
const top = el('div', 'sy-top'); dev.appendChild(top);
const kerPow = powerBtn('ker.on', 'KERAUNOS', BOLT, 'KERAUNOS on / off: the expression pedal (lasers, whammy, ring mod...)');
const modeSel = sel('ker.mode', 'MODE', ['PEDAL', 'ENV', 'AUTO', 'TRIG'], { tip: 'What moves the pedal: PEDAL (you), ENVELOPE (your picking), AUTO (by itself), TRIGGER (a sweep on every pick)' });
const speedK = mknob('ker.speed', 'SPEED', { fmt, tip: 'ENVELOPE: how fast it falls back · AUTO: how fast it rocks · TRIGGER: how long a sweep takes' });
const sensK = mknob('ker.sens', 'SENS', { fmt, tip: 'ENVELOPE: how far your picking pushes it · TRIGGER: how soft a pick attack still fires it' });
let trigLit = 0;
const trigBtn = momBtn('TRIGGER', TARGET, 'Fires a sweep now (switches the pedal to TRIGGER)', '#6fa8ff',
  v => { if (mode() !== 3) set('ker.mode', 3, 'ui'); if (!on('ker.on')) set('ker.on', 1, 'ui'); set('ker.trig', 1, 'ui'); sendNow('ker.trig'); sim.fire(); trigLit = performance.now(); trigBtn.classList.add('lit'); v.textContent = 'FIRE'; },
  v => { set('ker.trig', 0, 'ui'); sendNow('ker.trig'); setTimeout(() => { trigBtn.classList.remove('lit'); v.textContent = 'PRESS'; }, 120); });
let latched = false;
const holdBtn = momBtn('HOLD', SNOW, 'KHAOS HOLD: hold it down to freeze the glitch, or tap it to latch (tap again to let go)', '#ff5fa2',
  v => { if (!on('glt.on')) set('glt.on', 1, 'ui'); set('glt.hold', 1, 'ui'); sendNow('glt.hold'); holdBtn.classList.add('lit'); v.textContent = 'HOLD'; },
  (v, held) => { if (held < 300 && !latched) { latched = true; v.textContent = 'LATCHED'; return; }
    latched = false; set('glt.hold', 0, 'ui'); sendNow('glt.hold'); holdBtn.classList.remove('lit'); v.textContent = 'PRESS'; });
onChange((id, v, from) => { if (id === 'glt.hold' && from !== 'ui') { latched = v >= .5; holdBtn.classList.toggle('lit', latched); holdBtn._v.textContent = latched ? 'LATCHED' : 'PRESS'; } });
const chanceK = mknob('glt.chance', 'CHANCE', { fmt, tip: 'RANDOM: how likely a glitch is on each step' });
const chaosK = mknob('glt.chaos', 'CHAOS', { fmt, tip: 'How wild each glitch gets: shorter slices, faster rolls, lengths that vary' });
const filtK = mknob('glt.filter', 'FILTER', { fmt, tip: 'A DJ filter on the glitches: left darker, right thinner (double-click: open)' });
const gltPow = powerBtn('glt.on', 'KHAOS', STAR, 'KHAOS on / off: the glitch engine');
/* the display */
const led = el('div', 'fl-led');
led.innerHTML = '<div class="k"><div class="ln"><span class="u">' + BOLT + 'KERAUNOS</span><span class="u km"></span></div><div class="big"><span class="nm kn"></span><span class="vv kv"></span></div><div class="bar"><i class="kb"></i></div></div>'
  + '<div class="sep"></div><div class="g"><div class="ln"><span class="u">' + STAR + 'KHAOS</span><span class="u gm"></span></div><div class="big"><span class="nm gn"></span><span class="vv gv"></span></div><div class="dots">' + '<i></i>'.repeat(16) + '</div></div>';
const ledW = el('div', 'sy-hw'); ledW.appendChild(led);
K.stripGroups(top, [kerPow, modeSel, speedK, sensK, trigBtn], ledW, [holdBtn, chanceK, chaosK, filtK, gltPow]);
const dimKnobs = () => { const m = mode(); speedK.classList.toggle('dim', m === 0); sensK.classList.toggle('dim', m === 0 || m === 2); chanceK.classList.toggle('dim', seqMode()); };
onChange(id => { if (id === 'ker.mode' || id === 'glt.mode') dimKnobs(); if (id === 'ker.mode') { speedK.querySelector('.sy-val').textContent = fmt('ker.speed'); } });
const banner = el('div', 'fl-banner', 'THE SFX TAB IS BYPASSED<button>TURN ON</button>');
banner.querySelector('button').onclick = () => { if (window.obsFxLab && window.obsFxLab.sectionOut) window.obsFxLab.sectionOut(true); };
dev.appendChild(banner);

/* ------------------------------------------------------------ the screen */
const screen = el('div', 'sy-screen'); dev.appendChild(screen);

/* KERAUNOS */
const kSec = section('KERAUNOS', 480, 'var(--c-ker)', screen);
kSec._h.querySelector('h3').title = 'Keraunos (the thunderbolt): one pedal, sixteen effects';
const kSub = el('span', 'fl-sub', 'EXPRESSION PEDAL');
const placeSeg = seg('ker.place', ['BEFORE AMP', 'AFTER AMP'], { small: true });
placeSeg.title = 'BEFORE AMP: like a pedal in front of the amp · AFTER AMP: on the amp\'s sound (cleaner lasers and ring mod)';
const kDice = el('button', 'fl-ic', IC.dice); kDice.title = 'Roll a new KERAUNOS effect';
kSec._h.append(kSub, spacer(), placeSeg, kDice);
const kBody = el('div', 'fl-kbody'); kSec._b.appendChild(kBody);
const kCol = el('div', 'fl-kcol'), tread = el('div', 'fl-tread'); kBody.append(kCol, tread);
const lcd = el('div', 'sy-vis fl-lcd'); const lcdCv = el('canvas'); lcd.appendChild(lcdCv); kCol.appendChild(lcd);
lcd.title = 'What the pedal does from heel to toe (drag here to move it)';
const algGrid = el('div', 'fl-alg'); algGrid.dataset.param = 'ker.algo';
algGrid.innerHTML = ALGO_SHORT.map((t, i) => '<button data-i="' + i + '" style="--ac:' + algoCol(i) + '" title="' + ALGO_TIP[i] + '"><i></i>' + t + '</button>').join('');
kCol.appendChild(algGrid);
algGrid.addEventListener('click', e => { const b = e.target.closest('button'); if (b) set('ker.algo', +b.dataset.i, 'ui'); });
const shapeSeg = seg('ker.shape', null, { small: true, icons: ['up', 'down', 'updn', 'shuffle'] });
shapeSeg.title = 'Which way it sweeps (AUTO and TRIGGER): up, down, up and down, or random';
const shapeW = el('div', 'fl-stack'); shapeW.append(shapeSeg, el('span', 'fl-lbl', 'SWEEP'));
const kRow = el('div', 'fl-krow');
kRow.append(shapeW, knob('ker.color', { label: 'COLOR', size: 32, fmt, tip: 'The tone of each effect: resonance, sharpness, bits' }),
  knob('ker.mix', { label: 'MIX', size: 32, fmt, tip: 'Dry and effect' }), knob('ker.level', { label: 'LEVEL', size: 32, fmt, tip: 'Output level' }));
kCol.appendChild(kRow);
/* the treadle */
const tCap = el('div', 'cap', 'PEDAL<b>0%</b>');
const pw = el('div', 'fl-pw');
const ped = el('div', 'fl-ped', '<div class="base"></div><div class="plate"><i class="grip"></i><i class="toe"></i><div class="logo">' + BOLT + '</div><i class="sh t"></i><i class="sh b"></i></div>');
ped.dataset.param = 'ker.pos'; ped.title = 'The treadle: drag up and down (heel to toe). Grabbing it switches to PEDAL.';
const pbar = el('div', 'fl-pbar', '<i></i>'.repeat(18));
pw.append(ped, pbar);
const tHint = el('div', 'hint');
tread.append(tCap, pw, tHint);
const plate = ped.querySelector('.plate'), shT = ped.querySelector('.sh.t'), shB = ped.querySelector('.sh.b'), toeLed = ped.querySelector('.toe');
let dragPed = null;
const grabPedal = (e, startN) => { if (mode() !== 0) { set('ker.mode', 0, 'ui'); set('ker.pos', 100 * clamp(shownPos, 0, 1), 'ui'); } dragPed = { y: e.clientY, x: e.clientX, n: startN != null ? startN : toN('ker.pos') }; };
ped.addEventListener('pointerdown', e => { if (e.button !== 0) return; e.preventDefault(); ped.setPointerCapture(e.pointerId); grabPedal(e); });
ped.addEventListener('pointermove', e => { if (!dragPed) return; const sc = (document.getElementById('app') && document.getElementById('app').getBoundingClientRect().width / 1376) || 1;
  set('ker.pos', toV('ker.pos', dragPed.n + (dragPed.y - e.clientY) / sc / 150), 'ui'); });
['pointerup', 'pointercancel'].forEach(t => ped.addEventListener(t, () => { dragPed = null; }));
ped.addEventListener('dblclick', () => set('ker.pos', 0, 'ui'));
ped.addEventListener('wheel', e => { e.preventDefault(); if (mode() !== 0) set('ker.mode', 0, 'ui'); set('ker.pos', clamp(S['ker.pos'] + (e.deltaY < 0 ? 4 : -4), 0, 100), 'ui'); }, { passive: false });
lcd.addEventListener('pointerdown', e => { if (e.button !== 0) return; e.preventDefault(); lcd.setPointerCapture(e.pointerId); const r = lcd.getBoundingClientRect();
  grabPedal(e); dragPed.lcd = r; set('ker.pos', clamp((e.clientX - r.left - 12) / (r.width - 24), 0, 1) * 100, 'ui'); });
lcd.addEventListener('pointermove', e => { if (!dragPed || !dragPed.lcd) return; const r = dragPed.lcd; set('ker.pos', clamp((e.clientX - r.left - 12) / (r.width - 24), 0, 1) * 100, 'ui'); });
['pointerup', 'pointercancel'].forEach(t => lcd.addEventListener(t, () => { dragPed = null; }));

/* KHAOS */
const gSec = section('KHAOS', 772, 'var(--c-glt)', screen);
gSec._h.querySelector('h3').title = 'Khaos (the void before everything): glitches made from what you just played';
const modeSeg = seg('glt.mode', ['RANDOM', 'SEQUENCE'], { small: true });
modeSeg.title = 'RANDOM: glitches by chance, from the types that are lit · SEQUENCE: you draw them into 16 steps';
const gridPill = pill('glt.grid', 'GRID', { tip: 'How long a step is (the clock follows your DAW, or the app\'s BPM)' });
const loopPill = pill('glt.loop', 'REACH', { tip: 'How far back the glitches reach for what they replay' });
const lenPill = pill('glt.len', 'LENGTH', { tip: 'RANDOM: how long a glitch lasts (CHAOS varies it)' });
const gUndo = el('button', 'fl-ic', IC.undo); gUndo.title = 'Undo the last change to the steps'; gUndo.disabled = true;
const gClear = el('button', 'fl-ic', CLEAR); gClear.title = 'Clear the steps';
const gDice = el('button', 'fl-ic', IC.dice); gDice.title = 'Roll a new KHAOS pattern';
gSec._h.append(modeSeg, gridPill, loopPill, lenPill, spacer(), gUndo, gClear, gDice);
const gBody = el('div', 'fl-gbody'); gSec._b.appendChild(gBody);
const lane = el('div', 'sy-vis fl-lane'); const laneCv = el('canvas'); lane.appendChild(laneCv); gBody.appendChild(lane);
const pal = el('div', 'fl-pal'); gBody.appendChild(pal);
const typeBtns = [];
[[0, 1], [2, 3, 4]].forEach(rowG => {
  const r = el('div', 'fl-prow'); pal.appendChild(r);
  rowG.forEach(gi => {
    const [name, col, list] = TGROUPS[gi];
    const g = el('div', 'fl-grp'); g.style.setProperty('--tc', col); g.style.flex = String(list.length);
    const lab = el('span', null, name); lab.title = 'RANDOM: all ' + name.toLowerCase() + ' types on or off';
    lab.onclick = () => { const all = list.every(t => on(typeId(t))); list.forEach(t => set(typeId(t), all ? 0 : 1, 'ui')); if (seqMode()) set('glt.mode', 0, 'ui'); };
    const bx = el('div');
    list.forEach(t => { const b = el('button', 'fl-type', glyph(t) + '<b>' + TYPES[t].toUpperCase() + '</b>'); b.dataset.t = t; b.dataset.param = typeId(t); b.title = TYPE_TIP[t];
      b.style.setProperty('--tc', col); bx.appendChild(b); typeBtns[t] = b; });
    g.append(lab, bx); r.appendChild(g);
  });
});
let armed = 0;
pal.addEventListener('click', e => { const b = e.target.closest('.fl-type'); if (!b) return; const t = +b.dataset.t;
  if (seqMode()) { armed = t; drawTypes(); drawLane(); } else set(typeId(t), on(typeId(t)) ? 0 : 1, 'ui'); });
const gRow = el('div', 'fl-grow');
const dirSeg = seg('glt.dir', null, { small: true, icons: ['up', 'down', 'updn'] }); dirSeg.title = 'PITCH glitches go up, down or both ways';
const dirW = el('div', 'fl-stack'); dirW.append(dirSeg, el('span', 'fl-lbl', 'DIRECTION'));
const gHint = el('div', 'fl-hint');
gRow.append(knob('glt.pitch', { label: 'PITCH', size: 32, fmt, tip: 'How far PITCH glitches jump' }), dirW,
  knob('glt.mix', { label: 'MIX', size: 32, fmt, tip: 'How much of the glitch replaces the dry sound' }), knob('glt.level', { label: 'LEVEL', size: 32, fmt, tip: 'Output level' }), gHint);
gBody.appendChild(gRow);

/* ------------------------------------------------------------ bottom band: presets, badge, undo / reset / dice */
const bot = el('div', 'sy-bot'); dev.appendChild(bot);
const prBtn = el('button', 'sy-btn sy-prbtn fl-prbtn', '<span class="round">' + IC.list + '</span><span class="tx"><b>PRESETS</b><small class="fl-prname">Init</small></span>');
prBtn.title = 'Browse the GLITCH presets (both units)';
const prNav = el('div', 'sy-prnav', '<button aria-label="Previous glitch preset">' + IC.prev + '</button><button aria-label="Next glitch preset">' + IC.next + '</button>');
const prWrap = el('div', 'sy-prwrap'); prWrap.append(prBtn, prNav);
const badge = el('div', 'sy-badge', '<b>KERAUNOS<em> ✦ </em>KHAOS</b><div class="pn" title="The loaded glitch preset"><i></i><u>INIT</u><em></em></div>');
const undoB = el('button', 'sy-undo', IC.undo); undoB.title = 'Undo (dice, reset or preset)'; undoB.disabled = true;
const resetB = el('button', 'sy-btn r fl-reset', '<span class="round">' + IC.reset + '</span><span class="tx"><b>RESET</b><small>back to default</small></span>');
resetB.title = 'Both units back to their defaults (on / off stays)';
const diceB = el('button', 'sy-btn r fl-dice', '<span class="round">' + IC.dice + '</span><span class="tx"><b>DICE</b><small>roll new chaos</small></span>');
diceB.title = 'Roll a new effect and glitch pattern (keeps mix, level and where KERAUNOS sits)';
const diceWrap = el('div'); diceWrap.style.cssText = 'display: flex; align-items: center; gap: 22px'; diceWrap.append(undoB, resetB, diceB);
bot.append(prWrap, badge, diceWrap);

/* ------------------------------------------------------------ presets */
/* k: KERAUNOS (algo by name), g: KHAOS, t: the types RANDOM picks, q: the 16 steps ('-' or a type code) */
const A = n => ALGOS.indexOf(n);
const Q = str => str.trim().split(/\s+/).map(c => c === '-' ? 0 : TCODE.indexOf(c.toUpperCase()) + 1);
const PRESETS = [
  { name: 'Laser Riff', cat: 'LASER', note: 'Every pick zaps a laser up', k: { algo: A('Laser'), mode: 3, shape: 0, speed: 70, sens: 55, color: 62 } },
  { name: 'Pew Pew', cat: 'LASER', note: 'Short laser shots falling down', k: { algo: A('Laser'), mode: 3, shape: 1, speed: 86, sens: 60, color: 82 } },
  { name: 'Zap Storm', cat: 'LASER', note: 'Random zaps with stutters and crushes', k: { algo: A('Zap Comb'), mode: 2, shape: 3, speed: 74, color: 70 },
    g: { grid: 3, chance: 24, len: 0, chaos: 50 }, t: ['stutter', 'crush', 'chop'] },
  { name: 'Shifted Space', cat: 'LASER', note: 'A slow frequency shifter, out of this world', k: { algo: A('Freq Shift'), mode: 2, shape: 2, speed: 22, color: 50, mix: 70 } },
  { name: 'Whammy Solo', cat: 'PITCH', note: 'The classic: rock the pedal for an octave', k: { algo: A('Whammy Up'), mode: 0, pos: 0, color: 60 } },
  { name: 'Dive Bomb', cat: 'PITCH', note: 'Each pick dives two octaves', k: { algo: A('Dive Bomb'), mode: 3, shape: 0, speed: 34, sens: 50, color: 55 } },
  { name: 'Octave Scream', cat: 'PITCH', note: 'Pick hard and it screams up', k: { algo: A('Squeal'), mode: 1, speed: 50, sens: 62, color: 58 } },
  { name: 'Harmony Rise', cat: 'PITCH', note: 'An octave with a fifth breathing in', k: { algo: A('Oct + 5th'), mode: 2, shape: 2, speed: 26, color: 50, mix: 62 } },
  { name: 'Detuned Doom', cat: 'PITCH', note: 'A heavy, slightly flat double', k: { algo: A('Detune Drop'), mode: 0, pos: 100, color: 30 } },
  { name: 'Robot Voice', cat: 'ROBOT', note: 'A ring that follows your notes', k: { algo: A('Ring Tracking'), mode: 0, pos: 34, color: 50 } },
  { name: 'Ring Bell', cat: 'ROBOT', note: 'Picking rings it like a bell', k: { algo: A('Ring Mod'), mode: 1, speed: 58, sens: 70, color: 50, mix: 80 } },
  { name: 'Talk Box Funk', cat: 'ROBOT', note: 'Your picking says "wow"', k: { algo: A('Talk Box'), mode: 1, speed: 55, sens: 66, color: 50 } },
  { name: 'Auto Wah', cat: 'FILTER', note: 'A wah that follows your picking', k: { algo: A('Wah'), mode: 1, speed: 60, sens: 60, color: 40 } },
  { name: 'Sweep Filter', cat: 'FILTER', note: 'A resonant filter rising and falling', k: { algo: A('Filter'), mode: 2, shape: 2, speed: 34, color: 70 } },
  { name: '8-Bit Grind', cat: 'FILTER', note: 'Crushed bits with digital hiccups', k: { algo: A('Bit Crush'), mode: 0, pos: 66, color: 70 },
    g: { grid: 3, chance: 20, len: 0, chaos: 40 }, t: ['crush', 'data', 'chop'] },
  { name: 'Breakbeat', cat: 'GLITCH', note: 'Stutters, a roll and a tape stop in the bar', g: { mode: 1, grid: 3, chaos: 35 }, q: Q('- - - - stut - - - rev - - - roll roll tape tape') },
  { name: 'IDM Brain', cat: 'GLITCH', note: 'Busy, clever and a little broken', g: { mode: 1, grid: 3, chaos: 70, pitch: 7 }, q: Q('scat - skip - - ptch chop - rev - grn grn - skip ptch -') },
  { name: 'Data Storm', cat: 'GLITCH', note: 'Hard digital chaos at 1/32', g: { grid: 5, chance: 55, len: 1, chaos: 85 }, t: ['stutter', 'scatter', 'data', 'crush', 'chop', 'skip'] },
  { name: 'Stutter Edit', cat: 'GLITCH', note: 'Rolls that build at the end of the bar', g: { mode: 1, grid: 3, chaos: 20 }, q: Q('- - - - - - - - - - - - roll roll roll roll') },
  { name: 'Tape Death', cat: 'BROKEN', note: 'A tape machine that keeps dying', g: { grid: 1, chance: 32, len: 1, chaos: 40, filter: -18 }, t: ['tapestop', 'spinup', 'halfspeed', 'warble'] },
  { name: 'Broken Radio', cat: 'BROKEN', note: 'A radio losing the signal', g: { grid: 3, chance: 45, len: 1, chaos: 60, filter: 30 }, t: ['radio', 'static', 'gsm', 'dropout', 'hum'] },
  { name: 'Phone In Pocket', cat: 'BROKEN', note: 'The buzz of a phone near the amp', g: { grid: 3, chance: 34, len: 1, chaos: 30 }, t: ['gsm', 'data', 'dropout'] },
  { name: 'Bad Cable', cat: 'BROKEN', note: 'Cutting out and humming', g: { grid: 4, chance: 30, len: 0, chaos: 55 }, t: ['dropout', 'hum', 'static'] },
  { name: 'Vinyl Ghost', cat: 'TEXTURE', note: 'An old record with echoes', g: { grid: 0, chance: 40, len: 2, chaos: 30, filter: -40 }, t: ['vinyl', 'echo', 'grains', 'freeze'] },
  { name: 'Freeze Frame', cat: 'TEXTURE', note: 'Moments that hang in the air', g: { grid: 1, chance: 25, len: 3, chaos: 20 }, t: ['freeze', 'grains', 'echo'] },
  { name: 'Echo Throws', cat: 'TEXTURE', note: 'Echoes thrown at the end of each half bar', g: { mode: 1, grid: 1, chaos: 10 }, q: Q('- - - echo - - - echo - - - echo - - - echo') },
  { name: 'Laser Stutter', cat: 'COMBO', note: 'Lasers on every pick, stutters in time', k: { algo: A('Laser'), mode: 3, shape: 0, speed: 76, sens: 55, color: 70 },
    g: { mode: 1, grid: 3, chaos: 30 }, q: Q('- - - - stut - - - - - - - stut stut roll roll') },
  { name: 'Glitch Shred', cat: 'COMBO', note: 'Random whammy jumps and chopped runs', k: { algo: A('Whammy Up'), mode: 2, shape: 3, speed: 78, color: 60 },
    g: { grid: 3, chance: 30, len: 0, chaos: 50 }, t: ['stutter', 'pitch', 'chop'] },
  { name: 'Total Khaos', cat: 'COMBO', note: 'Everything at once. Good luck.', k: { algo: A('Ring Mod'), mode: 2, shape: 3, speed: 82, color: 70, mix: 80 },
    g: { grid: 4, chance: 60, len: 1, chaos: 100 }, t: TKEY.slice() },
];
const CATS = [['ALL', 'var(--accent)'], ['LASER', '#3fd0c0'], ['PITCH', '#f2b14c'], ['ROBOT', '#b48cff'], ['FILTER', '#ff7b8c'], ['GLITCH', '#ff5fa2'], ['BROKEN', '#ff8a5c'], ['TEXTURE', '#c69bff'], ['COMBO', '#9dff8a']];
const catCol = c => (CATS.find(x => x[0] === c) || CATS[0])[1];
function presetValues(pr) {
  const want = {};
  IDS.forEach(id => { want[id] = P[id].def; });
  want['ker.on'] = pr.k ? 1 : 0; want['glt.on'] = pr.g || pr.q ? 1 : 0;
  if (pr.k) for (const k in pr.k) want['ker.' + k] = pr.k[k];
  if (pr.g) for (const k in pr.g) want['glt.' + k] = pr.g[k];
  if (pr.t) TKEY.forEach((k, i) => { want[typeId(i)] = pr.t.includes(k) ? 1 : 0; });
  if (pr.q) { pr.q.forEach((v, i) => { want[stepId(i)] = v; }); want['glt.mode'] = 1; }
  else if (pr.g || pr.t) for (let i = 0; i < 16; i++) want[stepId(i)] = 0;
  return want;
}
let prIdx = -1, prCat = 'ALL', custom = false, undoState = null, applyingPreset = 0;
const snapshot = () => { const o = {}; IDS.forEach(id => { o[id] = S[id]; }); return { S: o, idx: prIdx, custom }; };
function applyValues(want, from) { applyingPreset++; try { IDS.forEach(id => { if (!PERFORM.includes(id) || id === 'ker.pos') set(id, id in want ? want[id] : P[id].def, from); }); } finally { applyingPreset--; } }
function loadPreset(i) {
  const pr = PRESETS[i]; if (!pr) return;
  undoState = snapshot(); undoB.disabled = false;
  applyValues(presetValues(pr), 'preset');
  prIdx = i; custom = false; drawName(); markPrList(); wakeSection(); if (seqMode()) armed = firstUsed();
}
function drawName() {
  const pr = PRESETS[prIdx], name = pr ? pr.name : prIdx === -2 ? 'Rolled' : 'Init';
  prBtn.querySelector('small').textContent = name + (custom ? ' *' : '');
  const pn = badge.querySelector('.pn'); pn.style.setProperty('--pc', pr ? catCol(pr.cat) : 'var(--accent)');
  pn.querySelector('u').textContent = name.toUpperCase() + (custom ? ' *' : ''); pn.querySelector('em').textContent = pr ? pr.cat : '';
}
onChange((id, v, from) => { if (!isFx(id) || applyingPreset || PERFORM.includes(id) || from === 'plugin' || from === 'preset') return; if (!custom) { custom = true; drawName(); } });
const prs = el('div', 'sy-prs fl-prs');
prs.innerHTML = '<div class="sy-prs-h"><b>GLITCH PRESETS</b><span>' + PRESETS.length + ' · lasers, pitch, robots, filters, glitches · the window stays open</span><button class="x" aria-label="Close">✕</button></div><div class="sy-prs-b"><div class="sy-prs-cats"></div><div class="sy-prs-list"></div></div>';
dev.appendChild(prs);
const prCats = prs.querySelector('.sy-prs-cats'), prList = prs.querySelector('.sy-prs-list');
prCats.innerHTML = CATS.map(([c, col]) => '<button data-c="' + c + '" style="--cc:' + col + '"><i></i>' + c + '<em>' + (c === 'ALL' ? PRESETS.length : PRESETS.filter(p => p.cat === c).length) + '</em></button>').join('');
function drawPrList() {
  [...prCats.children].forEach(b => b.classList.toggle('on', b.dataset.c === prCat));
  prList.innerHTML = PRESETS.map((p, i) => [p, i]).filter(([p]) => prCat === 'ALL' || p.cat === prCat)
    .map(([p, i]) => '<button class="sy-pr' + (i === prIdx ? ' on' : '') + '" data-i="' + i + '" style="--cc:' + catCol(p.cat) + '" title="' + p.note + '"><b>' + p.name + '</b><small>' + (p.k ? '⚡ ' : '') + (p.g || p.q ? '✦ ' : '') + p.note + '</small></button>').join('');
}
const markPrList = () => [...prList.children].forEach(b => b.classList.toggle('on', +b.dataset.i === prIdx));   // (re-rendering would detach the clicked tile mid-click)
prCats.addEventListener('click', e => { const b = e.target.closest('button'); if (!b) return; prCat = b.dataset.c; drawPrList(); prList.scrollTop = 0; });
prList.addEventListener('click', e => { const b = e.target.closest('.sy-pr'); if (b) loadPreset(+b.dataset.i); });
function prOpen(open) { prs.classList.toggle('show', open); prBtn.classList.toggle('on', open); if (open) { drawPrList(); const o = prList.querySelector('.on'); if (o) o.scrollIntoView({ block: 'nearest' }); } }
prBtn.addEventListener('click', e => { e.stopPropagation(); prOpen(!prs.classList.contains('show')); });
prs.querySelector('.x').addEventListener('click', () => prOpen(false));
document.addEventListener('pointerdown', e => { if (prs.classList.contains('show') && !prs.contains(e.target) && !prWrap.contains(e.target)) prOpen(false); });
const stepPreset = d => { const list = PRESETS.map((p, i) => i).filter(i => prCat === 'ALL' || PRESETS[i].cat === prCat);
  const at = list.indexOf(prIdx); loadPreset(list[at < 0 ? (d > 0 ? 0 : list.length - 1) : (at + d + list.length) % list.length]); };
prNav.children[0].addEventListener('click', () => stepPreset(-1));
prNav.children[1].addEventListener('click', () => stepPreset(1));
undoB.addEventListener('click', () => { if (!undoState) return; const u = undoState; undoState = null; undoB.disabled = true; applyValues(u.S, 'dice'); prIdx = u.idx; custom = u.custom; drawName(); markPrList(); });
resetB.addEventListener('click', () => { undoState = snapshot(); undoB.disabled = false; const w = {}; IDS.forEach(id => { w[id] = P[id].def; }); w['ker.on'] = S['ker.on']; w['glt.on'] = S['glt.on'];
  applyValues(w, 'preset'); prIdx = -1; custom = false; drawName(); markPrList(); });

/* ------------------------------------------------------------ dice */
const R = Math.random, ri = (a, b) => a + Math.floor(R() * (b - a + 1)), pick = a => a[Math.floor(R() * a.length)];
const wpick = (items, w) => { let s = 0; const t = w.reduce((a, b) => a + b, 0) * R(); for (let i = 0; i < items.length; i++) { s += w[i]; if (t <= s) return items[i]; } return items[items.length - 1]; };
function rollKer() {
  const m = wpick([0, 1, 2, 3], [1.5, 3, 3, 2.5]);
  const w = { 'ker.algo': ri(0, 15), 'ker.mode': m, 'ker.speed': ri(20, 85), 'ker.sens': ri(35, 80), 'ker.shape': ri(0, 3), 'ker.color': ri(25, 85) };
  if (m === 0) w['ker.pos'] = ri(25, 90);
  return w;
}
function rollGlt() {
  const w = {}, seq = R() < .45;
  w['glt.mode'] = seq ? 1 : 0; w['glt.grid'] = wpick([0, 1, 2, 3, 4, 5], [.4, 2, .6, 3.5, .8, 1.2]); w['glt.loop'] = wpick([0, 1, 2, 3], [1, 3, 2, .8]);
  w['glt.chance'] = ri(18, 55); w['glt.len'] = wpick([0, 1, 2, 3], [2.5, 3, 1.5, .6]); w['glt.chaos'] = ri(10, 85);
  w['glt.pitch'] = pick([5, 7, 7, 12, 12, 12, 19, 24]); w['glt.dir'] = ri(0, 2); w['glt.filter'] = R() < .6 ? 0 : (R() < .5 ? -1 : 1) * ri(18, 60);
  // a theme: one or two groups, a few of their types
  const gs = [pick(TGROUPS)]; if (R() < .6) gs.push(pick(TGROUPS));
  const pool = [...new Set(gs.flatMap(g => g[2]))].sort(() => R() - .5).slice(0, ri(2, 5));
  TKEY.forEach((k, i) => { w[typeId(i)] = pool.includes(i) ? 1 : 0; });
  const steps = new Array(16).fill(0);
  if (seq) {
    const n = ri(3, 6), spots = [2, 3, 6, 7, 10, 11, 13, 14, 15, 4, 12, 8, 5, 9, 1];
    for (let k = 0; k < n; k++) { const at = spots[Math.min(spots.length - 1, Math.floor(Math.pow(R(), 1.3) * spots.length))], t = pick(pool), run = R() < .3 ? ri(2, 3) : 1;
      for (let j = 0; j < run && at + j < 16; j++) steps[at + j] = t + 1; }
  }
  steps.forEach((v, i) => { w[stepId(i)] = v; });
  return w;
}
function roll(which) {
  undoState = snapshot(); undoB.disabled = false;
  const w = {}; IDS.forEach(id => { w[id] = S[id]; });
  if (which !== 'g') Object.assign(w, rollKer());
  if (which !== 'k') Object.assign(w, rollGlt());
  if (!which && !on('ker.on') && !on('glt.on')) { w['ker.on'] = 1; w['glt.on'] = 1; }
  if (which === 'k') w['ker.on'] = 1; if (which === 'g') w['glt.on'] = 1;
  applyValues(w, 'dice'); prIdx = -2; custom = false; drawName(); markPrList(); wakeSection(); if (seqMode()) armed = firstUsed();
}
diceB.addEventListener('click', () => roll());
kDice.addEventListener('click', () => roll('k'));
gDice.addEventListener('click', () => roll('g'));

/* ------------------------------------------------------------ drawing the steps */
let laneUndo = null;
const firstUsed = () => { for (let i = 0; i < 16; i++) if (stepType(i) >= 0) return stepType(i); return armed; };
const keepLaneUndo = () => { laneUndo = []; for (let i = 0; i < 16; i++) laneUndo.push(S[stepId(i)]); gUndo.disabled = false; };
gUndo.addEventListener('click', () => { if (!laneUndo) return; const u = laneUndo; laneUndo = null; gUndo.disabled = true; u.forEach((v, i) => set(stepId(i), v, 'ui')); });
gClear.addEventListener('click', () => { keepLaneUndo(); for (let i = 0; i < 16; i++) set(stepId(i), 0, 'ui'); if (!seqMode()) set('glt.mode', 1, 'ui'); });
let paint = null, hoverStep = -1;
const laneGeom = () => { const w = lane.clientWidth, pad = 8, gapS = 3, gapB = 9, cw = (w - 2 * pad - 12 * gapS - 3 * gapB) / 16; return { pad, cw, x: i => pad + i * cw + i * gapS + Math.floor(i / 4) * (gapB - gapS) }; };
const stepAt = cx => { const r = lane.getBoundingClientRect(), g = laneGeom(), x = cx - r.left; for (let i = 0; i < 16; i++) if (x >= g.x(i) - 2 && x <= g.x(i) + g.cw + 2) return i; return -1; };
lane.addEventListener('pointerdown', e => {
  const i = stepAt(e.clientX); if (i < 0) return; e.preventDefault(); lane.setPointerCapture(e.pointerId);
  keepLaneUndo();
  if (!seqMode()) set('glt.mode', 1, 'ui');
  if (!on('glt.on')) set('glt.on', 1, 'ui');
  const v = e.button === 2 || (stepType(i) === armed) ? 0 : armed + 1;
  paint = v; set(stepId(i), v, 'ui');
});
lane.addEventListener('pointermove', e => { const i = stepAt(e.clientX); if (paint != null && i >= 0) set(stepId(i), paint, 'ui'); if (i !== hoverStep) { hoverStep = i; drawLane(); } });
lane.addEventListener('pointerleave', () => { hoverStep = -1; drawLane(); });
['pointerup', 'pointercancel'].forEach(t => lane.addEventListener(t, () => { paint = null; }));
lane.addEventListener('contextmenu', e => e.preventDefault());
lane.addEventListener('wheel', e => { const i = stepAt(e.clientX); if (i < 0) return; e.preventDefault(); keepLaneUndo(); set(stepId(i), (Math.round(S[stepId(i)]) + (e.deltaY < 0 ? 1 : 22)) % 23, 'ui'); if (!seqMode()) set('glt.mode', 1, 'ui'); }, { passive: false });

/* ------------------------------------------------------------ live state: the engine, or a little simulation in a browser */
let shownPos = 0, liveStep = -1, liveType = -1, liveEv = 0, bpm = 120, lastLive = 0;
const hist = new Array(16).fill(null);              // RANDOM: what fired on each step lately {t, at}
let lastHistType = -1, lastHistStep = -1;
function noteGlitch(step, type) { if (step < 0 || type < 0) return; hist[step] = { t: type, at: performance.now() }; }
const sim = {
  t0: performance.now(), last: -1, cur: null, pos: 0, trigT: -1, from: 0, to: 0, env: 0, nextPick: 0, lfo: 0, rFrom: 0, rTo: 0,
  fire() { this.trigT = 0; this.from = shownPos; this.to = R(); },
  tick(dt) {
    const now = performance.now();
    // KERAUNOS
    const m = mode(), sp = S['ker.speed'] / 100, shape = Math.round(S['ker.shape']);
    let target = this.pos;
    if (m === 0) target = S['ker.pos'] / 100;
    else if (m === 1) { if (now > this.nextPick) { this.env = .4 + .6 * R(); this.nextPick = now + 300 + R() * 500; } this.env *= Math.exp(-dt / (40 + (1 - sp) * 1200)); target = clamp(this.env * (.3 + 1.4 * S['ker.sens'] / 100), 0, 1); }
    else if (m === 2) { const hz = .05 * Math.pow(2, sp * 7.3); this.lfo += hz * dt / 1000; if (this.lfo >= 1) { this.lfo -= 1; this.rFrom = this.rTo; this.rTo = R(); } const ph = this.lfo;
      target = shape === 0 ? ph : shape === 1 ? 1 - ph : shape === 2 ? 1 - Math.abs(2 * ph - 1) : this.rFrom + (this.rTo - this.rFrom) * ph * ph * (3 - 2 * ph); }
    else { if (now > this.nextPick && on('ker.on')) { this.fire(); this.nextPick = now + 900 + R() * 900; }
      const T = 1500 * Math.pow(2, -sp * 6.2);
      if (this.trigT >= 0) { this.trigT += dt; const u = this.trigT / T;
        target = shape === 0 ? (u < 1 ? u : u < 3 ? 1 : u < 4 ? 4 - u : 0) : shape === 1 ? (u < 1 ? 1 - u : u < 3 ? 0 : u < 4 ? u - 3 : 1) : shape === 2 ? (u < 1 ? u : u < 2 ? 2 - u : 0) : (u < 1 ? this.from + (this.to - this.from) * u : this.to);
        if (u > 4) this.trigT = -1; }
      else target = shape === 1 ? 1 : shape === 3 ? this.to : 0; }
    this.pos += (clamp(target, 0, 1) - this.pos) * (1 - Math.exp(-dt / (m === 0 ? 30 : 12)));
    shownPos = on('ker.on') ? this.pos : (m === 0 ? S['ker.pos'] / 100 : 0);
    // KHAOS
    if (!on('glt.on')) { liveStep = -1; liveType = -1; this.cur = null; return; }
    const sb = GRID_BEATS[Math.round(S['glt.grid'])], beat = (now - this.t0) / 1000 * bpm / 60, step = Math.floor(beat / sb);
    if (step !== this.last) {
      this.last = step; const s = ((step % 16) + 16) % 16;
      const busy = this.cur && step < this.cur.end;
      if (on('glt.hold')) { if (!this.cur || step >= this.cur.end) this.cur = { t: 5, start: step, end: step + 4 }; this.cur.end = Math.max(this.cur.end, step + 2); }
      else if (!busy) {
        let t = -1, len = 1;
        if (seqMode()) { t = stepType(s); if (t >= 0) { let k = 1; while (k < 4 && stepType((s + k) % 16) === t) k++; len = k; } }
        else if (R() * 100 < S['glt.chance']) { const pool = TKEY.map((k, i) => i).filter(i => on(typeId(i))); if (pool.length) { t = pick(pool); len = [1, 2, 4, 8][Math.round(S['glt.len'])];
          if (S['glt.chaos'] > 30 && R() < S['glt.chaos'] / 100) len = Math.max(1, Math.round(len * (.25 + R()))); } }
        if (t >= 0) { this.cur = { t, start: step, end: step + len }; liveEv++; }
      }
      liveStep = s;
    }
    liveType = this.cur && step < this.cur.end ? this.cur.t : -1;
  },
};
let simLast = performance.now();
function frame() {
  const now = performance.now(), dt = Math.min(100, now - simLast); simLast = now;
  const live = inPlugin() && now - lastLive < 1500;
  if (!live) sim.tick(dt);
  if (liveType >= 0 && (liveType !== lastHistType || liveStep !== lastHistStep) && (liveType !== lastHistType || hist[liveStep] == null || now - hist[liveStep].at > 400)) noteGlitch(liveStep, liveType);
  if (liveType !== lastHistType) lastHistStep = liveStep;
  lastHistType = liveType;
  if (page === 'glitch' && SFX.style.display !== 'none' && SFX.offsetParent !== null) { drawLed(); drawPedal(); drawLcd(); drawLane(); drawPlaying(); }
  requestAnimationFrame(frame);
}

/* ------------------------------------------------------------ drawings */
const lk = led.querySelector('.k'), lg = led.querySelector('.g');
const ledK = { m: led.querySelector('.km'), n: led.querySelector('.kn'), v: led.querySelector('.kv'), b: led.querySelector('.kb') };
const ledG = { m: led.querySelector('.gm'), n: led.querySelector('.gn'), v: led.querySelector('.gv'), dots: [...led.querySelectorAll('.dots i')] };
let ledType = -2;
function drawLed() {
  const a = algo(), col = algoCol(a), ef = effectAt(a, shownPos);
  lk.style.setProperty('--lc', col);
  ledK.m.textContent = fmt('ker.mode').toUpperCase(); ledK.n.textContent = ALGOS[a].toUpperCase(); ledK.v.textContent = ef.t; ledK.b.style.width = (shownPos * 100).toFixed(1) + '%';
  led.classList.toggle('k-off', !on('ker.on')); led.classList.toggle('g-off', !on('glt.on'));
  const t = liveType;
  lg.style.setProperty('--lc', t >= 0 ? typeCol(t) : '#6f6f76');
  ledG.m.textContent = (seqMode() ? 'SEQUENCE' : 'RANDOM ' + Math.round(S['glt.chance']) + '%') + ' · ' + fmt('glt.grid');
  if (t !== ledType) { ledType = t; ledG.n.textContent = t >= 0 ? TYPES[t].toUpperCase() : on('glt.hold') ? 'HOLD' : '· · ·'; if (t >= 0) { ledG.n.classList.remove('flash'); void ledG.n.offsetWidth; ledG.n.classList.add('flash'); } }
  ledG.v.textContent = liveStep >= 0 ? 'STEP ' + String(liveStep + 1).padStart(2, '0') : 'STOPPED';
  ledG.dots.forEach((d, i) => { const st = stepType(i); d.className = i === liveStep ? 'p' : seqMode() && st >= 0 ? 'g' : i % 4 === 0 ? 'q' : ''; if (seqMode() && st >= 0) d.style.setProperty('--gc', typeCol(st)); });
}
function drawPedal() {
  const p = clamp(shownPos, 0, 1), col = algoCol(algo());
  plate.style.transform = 'rotateX(' + ((p * 2 - 1) * 15).toFixed(2) + 'deg)';
  shT.style.opacity = (.55 * p).toFixed(3); shB.style.opacity = (.55 * (1 - p)).toFixed(3);
  kSec.style.setProperty('--gc', col);
  toeLed.style.opacity = (.18 + .82 * p).toFixed(3); toeLed.style.boxShadow = '0 0 ' + (12 * p).toFixed(1) + 'px ' + col;
  const n = pbar.children.length; [...pbar.children].forEach((x, i) => x.classList.toggle('on', i < Math.round(p * n)));
  tCap.querySelector('b').textContent = Math.round(p * 100) + '%';
  const h = MODE_HINT(); if (tHint.textContent !== h) tHint.textContent = h;
}
function drawLcd() {
  const [g, w, h] = hidpi(lcdCv); if (!g) return;
  const a = algo(), col = algoCol(a), x0 = 12, x1 = w - 12, y0 = 26, y1 = h - 16;
  g.font = '600 12px Orbitron, ' + FONT(); g.fillStyle = col; g.shadowColor = col; g.shadowBlur = 8; g.textBaseline = 'alphabetic';
  g.fillText(ALGOS[a].toUpperCase(), x0, 17); g.shadowBlur = 0;
  const ef = effectAt(a, shownPos);
  g.font = '600 11px Orbitron, ' + FONT(); g.textAlign = 'right'; g.fillStyle = tc('e9e6df', .95); g.fillText(ef.t, x1, 17); g.textAlign = 'left';
  // grid
  g.strokeStyle = tc('ffffff', .05); g.lineWidth = 1;
  for (let i = 0; i <= 4; i++) { const x = Math.round(x0 + (x1 - x0) * i / 4) + .5; g.beginPath(); g.moveTo(x, y0); g.lineTo(x, y1); g.stroke(); }
  if (ef.mid) { const ym = Math.round((y0 + y1) / 2) + .5; g.strokeStyle = tc('ffffff', .12); g.beginPath(); g.moveTo(x0, ym); g.lineTo(x1, ym); g.stroke(); }
  // the curve
  const N = 80, pts = [];
  for (let i = 0; i <= N; i++) { const x = i / N, e = effectAt(a, x); pts.push([x0 + (x1 - x0) * x, y1 - (y1 - y0) * clamp(e.y, 0, 1)]); }
  const grd = g.createLinearGradient(0, y0, 0, y1); grd.addColorStop(0, col.length === 7 ? col + '40' : col); grd.addColorStop(1, 'transparent');
  g.beginPath(); g.moveTo(pts[0][0], y1); pts.forEach(([x, y]) => g.lineTo(x, y)); g.lineTo(pts[N][0], y1); g.closePath(); g.fillStyle = grd; g.fill();
  g.beginPath(); pts.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y)); g.strokeStyle = col; g.lineWidth = 1.6; g.stroke();
  if (ef.vowels) { g.font = '600 8px ' + FONT(); g.fillStyle = tc('ffffff', .35); g.textAlign = 'center'; ['U', 'O', 'A', 'E', 'I'].forEach((v, i) => g.fillText(v, x0 + (x1 - x0) * i / 4, y0 - 2)); g.textAlign = 'left'; }
  // where it is now
  const px = x0 + (x1 - x0) * shownPos, py = y1 - (y1 - y0) * clamp(ef.y, 0, 1);
  g.strokeStyle = tc('ffffff', .25); g.setLineDash([2, 3]); g.beginPath(); g.moveTo(px, y0); g.lineTo(px, y1); g.stroke(); g.setLineDash([]);
  g.beginPath(); g.arc(px, py, 4, 0, Math.PI * 2); g.fillStyle = col; g.shadowColor = col; g.shadowBlur = 10; g.fill(); g.shadowBlur = 0;
  g.beginPath(); g.arc(px, py, 1.6, 0, Math.PI * 2); g.fillStyle = tc('ffffff', .95); g.fill();
  // heel / toe
  g.font = '600 7.5px ' + FONT(); g.fillStyle = tc('8d8d93', 1); g.fillText('HEEL · ' + ef.heel, x0, h - 4); g.textAlign = 'right'; g.fillText(ef.toe + ' · TOE', x1, h - 4); g.textAlign = 'left';
}
function rr(g, x, y, w, h, r) { g.beginPath(); g.moveTo(x + r, y); g.arcTo(x + w, y, x + w, y + h, r); g.arcTo(x + w, y + h, x, y + h, r); g.arcTo(x, y + h, x, y, r); g.arcTo(x, y, x + w, y, r); g.closePath(); }
const rgba = (hex, a) => 'rgba(' + [1, 3, 5].map(i => parseInt(hex.substr(i, 2), 16)).join(',') + ',' + a + ')';
function drawGlyph(g, t, cx, cy, size, color) {
  const p2 = GLYPH_PATHS[t]; if (!p2) return;
  g.save(); g.translate(cx - size / 2, cy - size / 2); g.scale(size / 24, size / 24);
  g.strokeStyle = color; g.lineWidth = t === 19 ? 3.4 : 2; g.lineCap = 'round'; g.lineJoin = 'round'; g.stroke(p2); g.restore();
}
function drawLane() {
  const [g, w, h] = hidpi(laneCv); if (!g) return;
  const G = laneGeom(), top = 14, bot = h - 8, ch = bot - top, now = performance.now(), seq = seqMode();
  // beat numbers
  g.font = '600 7.5px ' + FONT(); g.textBaseline = 'alphabetic';
  for (let i = 0; i < 16; i++) { g.fillStyle = i === liveStep ? tc('f1eee7', 1) : tc('6f6f76', i % 4 === 0 ? 1 : .55); g.fillText(i % 4 === 0 ? String(i / 4 + 1) : '·', G.x(i) + 3, 10); }
  // cells
  for (let i = 0; i < 16; i++) {
    const x = G.x(i);
    rr(g, x, top, G.cw, ch, 6); g.fillStyle = tc('151518', 1); g.fill();
    g.strokeStyle = tc('ffffff', i === hoverStep ? .2 : .05); g.lineWidth = 1; g.stroke();
    if (!seq) {                                          // the chance, as a level in every step
      const lv = ch * S['glt.chance'] / 100; g.save(); rr(g, x, top, G.cw, ch, 6); g.clip();
      g.fillStyle = rgba('#ff5fa2', .07); g.fillRect(x, bot - lv, G.cw, lv); g.fillStyle = rgba('#ff5fa2', .3); g.fillRect(x, bot - lv, G.cw, 1); g.restore();
    }
  }
  // blocks: the sequence (runs of one type merge, four steps at most, like the engine), or what fired
  const block = (i, len, t, alpha, live) => {
    const x = G.x(i), x2 = G.x(i + len - 1) + G.cw, col = typeCol(t), bw = x2 - x;
    rr(g, x + 1, top + 1, bw - 2, ch - 2, 5); g.fillStyle = rgba(col, live ? .55 : .22 * alpha); g.fill();
    g.strokeStyle = rgba(col, live ? 1 : .8 * alpha); g.lineWidth = live ? 1.6 : 1; g.stroke();
    if (live) { g.save(); g.shadowColor = col; g.shadowBlur = 16; g.stroke(); g.restore(); }
    drawGlyph(g, t, x + bw / 2, top + ch / 2 - 7, 15, live ? tc('ffffff', 1) : rgba(col, alpha));
    g.font = '600 7.5px ' + FONT(); g.textAlign = 'center'; g.fillStyle = live ? tc('ffffff', 1) : rgba(col, alpha);
    g.fillText(bw > 70 ? TYPES[t].toUpperCase() : TCODE[t], x + bw / 2, top + ch / 2 + 15); g.textAlign = 'left';
  };
  if (seq) {
    for (let i = 0; i < 16;) { const t = stepType(i); if (t < 0) { i++; continue; } let k = 1; while (k < 4 && i + k < 16 && stepType(i + k) === t) k++;
      block(i, k, t, 1, liveType === t && liveStep >= i && liveStep < i + k); i += k; }
    if (hoverStep >= 0 && stepType(hoverStep) < 0 && paint == null) { const x = G.x(hoverStep); drawGlyph(g, armed, x + G.cw / 2, top + ch / 2 - 7, 15, rgba(typeCol(armed), .35)); }
  } else {
    hist.forEach((hh, i) => { if (!hh) return; const age = (now - hh.at) / 1000, alpha = Math.max(0, 1 - age / 4); if (alpha <= 0) { hist[i] = null; return; }
      block(i, 1, hh.t, alpha, liveType === hh.t && liveStep === i); });
  }
  // the playhead
  if (liveStep >= 0) { const x = G.x(liveStep); rr(g, x - 1, top - 1, G.cw + 2, ch + 2, 7); g.strokeStyle = tc('f1eee7', .75); g.lineWidth = 1.2; g.stroke();
    g.fillStyle = tc('f1eee7', 1); g.fillRect(x + G.cw / 2 - 6, top - 3, 12, 2); }
}
let playingWas = -2;
function drawPlaying() { if (liveType === playingWas) return; playingWas = liveType; typeBtns.forEach((b, t) => b.classList.toggle('play', t === liveType)); }
function drawTypes() {
  const seq = seqMode(), used = new Set(); for (let i = 0; i < 16; i++) if (stepType(i) >= 0) used.add(stepType(i));
  typeBtns.forEach((b, t) => { b.classList.toggle('on', seq ? used.has(t) : on(typeId(t))); b.classList.toggle('arm', seq && t === armed); });
  pal.title = seq ? 'SEQUENCE: pick a type, then click steps to draw it' : 'RANDOM: glitches are picked from the lit types';
}
function drawAlg() { const a = algo(); [...algGrid.children].forEach((b, i) => b.classList.toggle('on', i === a)); kSec.style.setProperty('--sc', algoCol(a)); }
function drawHint() {
  const grid = fmt('glt.grid'), n = TKEY.filter((k, i) => on(typeId(i))).length;
  let count = 0; for (let i = 0; i < 16;) { const t = stepType(i); if (t < 0) { i++; continue; } let k = 1; while (k < 4 && i + k < 16 && stepType(i + k) === t) k++; count++; i += k; }
  gHint.innerHTML = seqMode()
    ? '<b>SEQUENCE</b> · ' + (count ? count + ' glitch' + (count > 1 ? 'es' : '') + ' in 16 steps of <i>' + grid + '</i> at ' + Math.round(bpm) + ' BPM · click a step to draw, right-click clears' : 'pick a type below, then click steps above to draw it')
    : '<b>RANDOM</b> · every <i>' + grid + '</i> a <i>' + Math.round(S['glt.chance']) + '%</i> chance of a ' + fmt('glt.len').replace(' steps', '-step').replace(' step', '-step') + ' glitch from <i>' + n + ' type' + (n === 1 ? '' : 's') + '</i>'
      + (S['glt.chaos'] > 30 ? ' · CHAOS varies how long' : '') + (n === 0 ? ' · <b>light some types below</b>' : ' · click a step to draw your own');
  lenPill.style.opacity = seqMode() ? .35 : 1;
}
function drawOn() { kSec.classList.toggle('sy-off', !on('ker.on')); gSec.classList.toggle('sy-off', !on('glt.on')); }
function redraw() { drawOn(); drawAlg(); drawTypes(); drawHint(); dimKnobs(); pageDots(); drawLed(); drawPedal(); drawLcd(); drawLane(); }
onChange(id => {
  if (!isFx(id)) return;
  if (id === 'ker.algo') drawAlg();
  if (id.startsWith('glt.')) { drawTypes(); drawHint(); }
  if (id === 'ker.on' || id === 'glt.on') { pageDots(); drawOn(); }
  if (id === 'glt.mode' && seqMode()) armed = firstUsed();
});

/* ------------------------------------------------------------ plugin hooks */
window.obsFxLab = {
  ids: IDS, defs: DEFS,
  read() { const o = {}; IDS.forEach(id => { o[id] = toN(id); }); return o; },
  write(vals) { for (const id in vals) if (P[id] && isFx(id)) set(id, toV(id, vals[id]), 'plugin'); },
  /* the engine's meters: { pos 0..1, step 0..15, type -1.., ev count, bpm } */
  live(m) { if (!m) return; lastLive = performance.now(); if (m.pos != null) shownPos = +m.pos; if (m.step != null) liveStep = on('glt.on') ? +m.step : -1; if (m.type != null) liveType = on('glt.on') ? +m.type : -1; if (m.ev != null) liveEv = +m.ev; if (m.bpm > 20) bpm = +m.bpm; },
  setSection(isOn) { sfxOn = !!isOn; dev.classList.toggle('sfx-off', !sfxOn); },
  paramOut: null,       // set by the bridge: (id, 0..1) -> the engine at once (TRIGGER, HOLD)
  sectionOut: null,     // set by the bridge: turns the SFX tab on
  showPage, presets: PRESETS, loadPreset, dice: () => roll(), reset: () => resetB.click(),
  page: () => page,
};

/* ------------------------------------------------------------ start */
drawName();
setTimeout(() => { showPage(page); redraw(); requestAnimationFrame(frame); }, 0);
})();
</script>
