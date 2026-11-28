<script>
/* =============================================================================
   SIREN's EFFECTS (ORPHEUS standalone), in the place of the arpeggiator:
   ten effects after the voice, from beautiful (chorus, delay, reverb, shimmer)
   to evil (octave down, drive, crush, ring), plus the DOUBLER and WIDTH on the
   top row. Built on SIREN's device (window.__sirenKit). The settings are
   syn.fx.* in synth.js (so SIREN's RESET puts them back too).
   ============================================================================= */
(function () {
'use strict';
const K = window.__sirenKit; if (!K || !K.fxView) return;
const { el, S, set, on, onChange, knob, toggle, seg } = K;

/* ------------------------------------------------------------ the effects (in the order they sit in the sound) */
const hzf = v => v < 1 ? v.toFixed(2) + ' Hz' : v < 10 ? v.toFixed(1) + ' Hz' : Math.round(v) + ' Hz';
const msf = v => v >= 1000 ? (v / 1000).toFixed(2) + ' s' : Math.round(v) + ' ms';
const secf = v => { const t = .6 * Math.pow(20, v / 100); return (t < 10 ? t.toFixed(1) : Math.round(t)) + ' s'; };
const FX = [
  { id: 'cho', name: 'CHORUS', info: 'A choir of slightly detuned copies: wide, lush and soft.', knobs: [['rate', 'RATE', hzf, 'How fast the copies sway: slow is lush, fast is seasick'], ['depth', 'DEPTH', 0, 'How far they sway out of tune'], ['mix', 'MIX', 0, 'How much chorus against the plain sound']] },
  { id: 'pha', name: 'PHASER', info: 'Slow swirling sweeps: dreamy, or seasick when fast.', knobs: [['rate', 'RATE', hzf, 'How fast it sweeps'], ['depth', 'DEPTH', 0, 'How wide the sweep is'], ['fb', 'FEEDBACK', 0, 'Sharper, whistling sweeps'], ['mix', 'MIX', 0, 'How strong the effect is']] },
  { id: 'fla', name: 'FLANGER', info: 'A jet-plane whoosh. Turn up the feedback for metal.', knobs: [['rate', 'RATE', hzf, 'How fast it sweeps'], ['depth', 'DEPTH', 0, 'How far it sweeps'], ['fb', 'FEEDBACK', 0, 'More metallic and ringing'], ['mix', 'MIX', 0, 'How strong the effect is']] },
  { id: 'dly', name: 'DELAY', info: 'Echoes that get warmer and darker each time: bouncing left and right, or a wobbly old tape.', sync: true,
    seg: { id: 'mode', opts: ['PING-PONG', 'TAPE'], w: 168, tip: 'PING-PONG: clean echoes bouncing left and right. TAPE: warm, wobbly, saturated echoes in the middle' }, knobs: [['time', 'TIME', msf, 'Time between the echoes'], ['fb', 'FEEDBACK', 0, 'How many echoes: a few, or on and on'], ['tone', 'TONE', 0, 'Darker or brighter echoes'], ['mix', 'MIX', 0, 'How loud the echoes are']] },
  { id: 'rev', name: 'REVERB', info: 'A real-sounding space: highs fade first, the tail blooms and moves.',
    seg: { id: 'type', opts: ['ROOM', 'HALL', 'PLATE', 'CATHEDRAL'], w: 300, tip: 'The space: ROOM close and woody, HALL big and warm, PLATE bright and dense at once, CATHEDRAL huge and slow to bloom' },
    knobs: [['size', 'DECAY', secf, 'How long the reverb rings'], ['pre', 'PRE', msf, 'A gap before the reverb starts: keeps the lead clear in front of it'], ['damp', 'DARK', 0, 'How quickly the highs fade: bright stone to a dark, soft hall'], ['mix', 'MIX', 0, 'How much reverb']] },
  { id: 'shm', name: 'SHIMMER', info: 'A huge, slowly blooming space where octaves rise above your chords: angels, light, heaven.',
    seg: { id: 'mode', opts: ['OCTAVES', '+ FIFTHS'], w: 190, tip: 'What rises: octaves (one, two, then a faint third), or octaves with a fifth between them (brighter, like an organ)' },
    knobs: [['amt', 'RISE', 0, 'How strongly the octaves rise above your chords and ring on after them (0: just the space)'], ['decay', 'DECAY', secf, 'How long it rings, and how slowly the octaves rise'], ['mix', 'MIX', 0, 'How much shimmer']] },
  { id: 'oct', name: 'OCTAVE', info: 'A real extra layer an octave or a fifth away (no warble): huge and dark, or bright.',
    seg: { id: 'mode', opts: ['−1 OCT', '−2 OCT', '+1 OCT', '+5TH'], w: 240, tip: 'Which layer: an octave down, two octaves down, an octave up, or a fifth up' }, knobs: [['mix', 'MIX', 0, 'How loud the shifted copy is (100% = as loud as the sound)']] },
  { id: 'drv', name: 'DRIVE', info: 'From warm tube grit to a buzzing, angry wall.',
    seg: { id: 'mode', opts: ['TUBE', 'FUZZ'], w: 150, tip: 'TUBE: warm and round, it breaks up gently. FUZZ: hard, buzzing and angry' }, knobs: [['amt', 'DRIVE', 0, 'How hard it is pushed: warm grit to a buzzing wall'], ['tone', 'TONE', 0, 'Darker or brighter distortion'], ['mix', 'MIX', 0, 'Distorted against clean']] },
  { id: 'cru', name: 'CRUSH', info: 'Digital decay: fewer bits and a lower sample rate, from old sampler to broken machine.', knobs: [['bits', 'BITS', v => Math.round(v) + ' bits', 'Fewer bits = more broken and gritty'], ['rate', 'RATE', v => v >= 1000 ? (v / 1000).toFixed(v < 10000 ? 1 : 0) + ' kHz' : Math.round(v) + ' Hz', 'The sample rate: lower is more metallic, aliased and lo-fi'], ['mix', 'MIX', 0, 'Crushed against clean']] },
  { id: 'ring', name: 'RING', info: 'Ring modulation: bells, broken metal and horror.', knobs: [['freq', 'FREQ', hzf, 'The ring frequency: low is a wobble, high is clanging metal'], ['mix', 'MIX', 0, 'How much of the ring']] },
];
const ORDER = ['drv', 'cru', 'ring', 'cho', 'fla', 'pha', 'dbl', 'dly', 'rev', 'shm'];      // the signal path (dbl: the DOUBLER; OCTAVE is in the voice, before all of them)
const NAME = Object.fromEntries(FX.map(x => [x.id, x.name])); NAME.dbl = 'DOUBLER';
const pid = (x, k) => 'syn.fx.' + x + '.' + k;

/* ------------------------------------------------------------ look */
const css = el('style');
css.textContent = `
.sy-wseg button { height: 30px; flex-direction: column; justify-content: center; gap: 2px; }
.sy-wseg button small { font-size: 7px; font-weight: 600; letter-spacing: .12em; line-height: 1; }
.sy-obox { flex: 1; min-width: 0; display: flex; align-items: center; gap: 2px; padding: 6px 6px 5px 3px; border-radius: 10px; background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.04); }
.sy-obox > .sy-lbl { writing-mode: vertical-rl; transform: rotate(180deg); font-size: 7px; letter-spacing: .22em; color: #55555b; flex: none; }
.sy-orow { flex: 1; min-width: 0; display: flex; justify-content: space-around; align-items: flex-start; }
.sy-orow2 { display: flex; gap: 6px; flex: none; }
.sy-ucol { display: flex; flex-direction: column; align-items: center; gap: 4px; padding-top: 6px; }
.sy-fpre { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 3px; padding: 3px; flex: none; border-radius: 8px; background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05); }
.sy-fpre button { height: 19px; padding: 0; border: none; border-radius: 6px; background: none; color: #6f6f76; font: inherit; font-size: 7.5px; font-weight: 600; letter-spacing: .1em; cursor: pointer; }
.sy-fpre button:hover { color: #e9e6df; }
.sy-fpre button.on { color: #e9e6df; background: linear-gradient(#34343a, #25252a); box-shadow: inset 0 1px 0 rgba(255,255,255,.1), 0 1px 2px rgba(0,0,0,.5), inset 0 -2px 0 var(--sc); }
.fx-grid { display: grid; grid-template-columns: repeat(5, minmax(0, 1fr)); gap: 4px; flex: none; }
.fx-b { position: relative; height: 30px; border: none; border-radius: 7px; padding: 0; cursor: pointer; font: inherit; font-size: 7.5px; font-weight: 600; letter-spacing: .1em; color: #6f6f76;
  background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05); display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 4px; }
.fx-b i { width: 5px; height: 5px; border-radius: 50%; background: #2c2c31; }
.fx-b:hover { color: #e9e6df; }
.fx-b.on { color: #cfccc5; }
.fx-b.on i { background: var(--sc); box-shadow: 0 0 6px var(--sc); }
.fx-grid.alloff .fx-b.on i { background: #4a4a50; box-shadow: none; }
.fx-grid.alloff .fx-b { color: #55555b; }
.fx-b.sel { color: #f1eee7; background: linear-gradient(#303035, #1d1d21); box-shadow: inset 0 1px 0 rgba(255,255,255,.1), 0 1px 2px rgba(0,0,0,.6), inset 0 -2px 0 var(--sc); }
.fx-info { flex: none; margin: 12px 2px 0; min-height: 26px; font-size: 9px; line-height: 1.45; color: #8d8d93; letter-spacing: .03em; }
.fx-info b { font-size: 9.5px; font-weight: 600; letter-spacing: .2em; color: #e9e6df; margin-right: 6px; }
.fx-pane { flex: 1; min-height: 0; display: flex; flex-direction: column; justify-content: center; gap: 12px; padding-bottom: 6px; transition: opacity .15s; }
.fx-pane.off { opacity: .4; }
.fx-pane .sy-seg { align-self: center; }
.fx-chain { flex: none; display: flex; align-items: center; gap: 6px; padding: 7px 10px; border-radius: 8px; background: #0b0b0d; box-shadow: inset 0 0 0 1px rgba(255,255,255,.04);
  font-size: 8px; letter-spacing: .14em; color: #55555b; white-space: nowrap; overflow: hidden; }
.fx-chain > span:first-child { color: #6f6f76; font-weight: 600; margin-right: 4px; }
.fx-chain b { color: #cfccc5; font-weight: 600; }
.fx-chain u { text-decoration: none; opacity: .5; }
.fx-chain.short { gap: 3px; letter-spacing: .04em; font-size: 7.5px; }
.fx-chain.short > span:first-child { display: none; }
`;
document.head.appendChild(css);

/* ------------------------------------------------------------ the panel: pick an effect, turn it on, shape it */
const view = K.fxView, right = K.fxRight;
const master = toggle('syn.fx.on', 'All effects on / off'); master.title = 'All the effects on or off at once (each one: double-click it)';
right.replaceChildren(master);
const grid = el('div', 'fx-grid');
FX.forEach((x, i) => { const b = el('button', 'fx-b', '<i></i><span>' + x.name + '</span>'); b.dataset.i = i; b.title = x.name + ': ' + x.info + ' Click to open it, double-click to switch it on or off.'; grid.appendChild(b); });
const info = el('div', 'fx-info');
const paneBox = el('div'); paneBox.style.cssText = 'flex: 1; min-height: 0; display: flex; flex-direction: column';
const chain = el('div', 'fx-chain');
view.append(grid, info, paneBox);
/* each effect's knobs and ON switch are made once and swapped in */
const panes = FX.map(x => {
  const p = el('div', 'fx-pane'), ks = el('div', 'sy-knobs');
  x.knobs.forEach(([k, label, f, tip]) => ks.appendChild(knob(pid(x.id, k), { label, size: 40, tip, fmt: f ? (id => f(S[id])) : undefined })));
  let sg = null;
  if (x.seg) { sg = seg(pid(x.id, x.seg.id), x.seg.opts, { small: true, width: x.seg.w }); sg.title = x.seg.tip; if (!x.sync) p.appendChild(sg); }
  if (x.sync) {                                                     // DELAY: in time with the song, or free
    const tKnob = ks.firstChild, nKnob = knob(pid(x.id, 'div'), { label: 'NOTE', size: 40, tip: 'Time between the echoes, in note values of the song\'s tempo', fmt: id => ['1/16', '1/8T', '1/8', '1/8.', '1/4T', '1/4', '1/4.', '1/2'][Math.round(S[id])] });
    const syncChip = K.chip(pid(x.id, 'sync'), 'SYNC', 'Echoes in time with the ORPHEUS tempo (off: set the time in milliseconds)');
    const sw = () => ks.replaceChild(on(pid(x.id, 'sync')) ? nKnob : tKnob, ks.firstChild);
    sw(); onChange(i => { if (i === pid(x.id, 'sync')) sw(); });
    const r = el('div'); r.style.cssText = 'display: flex; justify-content: center; align-items: center; gap: 10px'; if (sg) { sg.style.alignSelf = 'auto'; r.appendChild(sg); } r.appendChild(syncChip); p.appendChild(r);
  }
  p.appendChild(ks);
  return { p };
});
let cur = 0;
try { const s = +sessionStorage.getItem('obs.fxSel'); if (s >= 0 && s < FX.length) cur = s; } catch (_) {}
function pick(i) {
  cur = i; try { sessionStorage.setItem('obs.fxSel', String(i)); } catch (_) {}
  info.innerHTML = '<b>' + FX[i].name + '</b>' + FX[i].info;
  paneBox.replaceChildren(panes[i].p);
  draw();
}
const SHORT = { cho: 'CHO', pha: 'PHA', fla: 'FLA', dly: 'DLY', rev: 'REV', shm: 'SHM', oct: 'OCT', drv: 'DRV', cru: 'CRU', ring: 'RING', dbl: 'DBL' };
function draw() {
  const all = on('syn.fx.on');
  [...grid.children].forEach((b, i) => { b.classList.toggle('on', on(pid(FX[i].id, 'on'))); b.classList.toggle('sel', i === cur); });
  grid.classList.toggle('alloff', !all);
  panes[cur].p.classList.toggle('off', !all || !on(pid(FX[cur].id, 'on')));
  const act = ORDER.filter(id => id === 'dbl' ? on('syn.fx.dbl') : all && on(pid(id, 'on')));
  const line = names => '<span>CHAIN</span>' + (act.length ? act.map(id => '<b>' + names[id] + '</b>').join('<u>›</u>') : (all ? 'no effects on' : 'all effects off'));
  chain.classList.remove('short'); chain.innerHTML = line(NAME);
  if (chain.scrollWidth > chain.clientWidth + 1) { chain.classList.add('short'); chain.innerHTML = line(SHORT); }          // too many for the full names
}
grid.addEventListener('click', e => { const b = e.target.closest('.fx-b'); if (b) pick(+b.dataset.i); });
grid.addEventListener('dblclick', e => { const b = e.target.closest('.fx-b'); if (!b) return; const id = pid(FX[+b.dataset.i].id, 'on'); set(id, on(id) ? 0 : 1, 'ui'); });
onChange(id => { if (/^syn\.fx\.\w+\.on$/.test(id) || id === 'syn.fx.dbl' || id === 'syn.fx.on') draw(); });
pick(cur);
if (K.motDots) K.motDots();

/* ============================================================ tooltips for every control that has none */
const PT = {
  'syn.level': 'SIREN\'s volume', 'syn.flt.res': 'Resonance: a whistling peak at the cutoff, smooth to squelchy', 'syn.flt.drive': 'Pushes the filter: warmer, grittier, heavier',
  'syn.flt.cutoff': 'Where the filter cuts: low is dark and muffled, high is bright', 'syn.flt.env': 'How far the FILTER ENVELOPE opens the filter on each chord (minus closes it)', 'syn.flt.key': 'Higher notes get a brighter filter',
  'syn.flt.type': 'The filter: LDR24 / LDR12 warm ladders, LP lowpass, BP bandpass, HP highpass, NOTCH, VOWEL (talking)',
  'syn.noise.color': 'Noise colour: dark rumble, pink, or bright hiss', 'syn.noise.level': 'How much noise: breath and grit', 'syn.sub.level': 'A clean sub one or two octaves below',
  'syn.sub.wave': 'The sub\'s wave: round sine or buzzy square', 'syn.sub.oct': 'One or two octaves below',
  'syn.lfo.rate': 'How fast the LFO moves', 'syn.lfo.div': 'How fast the LFO moves, in note values', 'syn.lfo.depth': 'How strongly the LFO moves its target (0 = off)', 'syn.lfo.fade': 'The LFO fades in after each new chord',
  'syn.lfo.shape': 'The LFO\'s wave: sine, triangle, saw, square, random steps or slow drift', 'syn.lfo.target': 'What the LFO moves: VIBRATO pitch, WAH filter, SHAPE, TREMOLO volume or PAN',
  'atm.bed.level': 'How loud the noise is', 'atm.drn.level': 'How loud the drone is', 'atm.tex.level': 'How loud the events are',
};
const LT = {
  AMOUNT: 'how much of it: a drizzle or a downpour', BAND: 'which band of the old radio it hisses on', BREATHE: 'how much it slowly swells and fades, like breathing', BREEZE: 'how strong the breeze is',
  BUBBLES: 'how many bubbles rise', CHIRP: 'how busy the crickets are', CHORUS: 'how many voices join in', CRACKLE: 'how much it crackles and pops', CURRENT: 'how strongly the water moves',
  DRIFT: 'how much it slowly wanders by itself', DROPS: 'how many single drops you hear', FLOW: 'how fast the water flows', GUSTS: 'how often gusts come and go', HEAVE: 'how much the deep sub heaves',
  HEIGHT: 'how high the air sits', HOWL: 'how much it howls when it gets wild', PACE: 'how fast it moves', PITCH: 'higher or lower', RASP: 'how ragged and rasping it gets', RUSTLE: 'how much the leaves rustle',
  SPLASH: 'how much it splashes', SWELL: 'how big the swells are', THROAT: 'the shape of the throat: darker or brighter breath', TONE: 'darker or brighter', WHISTLE: 'the tuning whistle between stations',
  BEAT: 'how fast the voices beat against each other', BELLOWS: 'how hard the bellows push', BREATH: 'how breathy it is', CELESTE: 'a second, slightly detuned rank that shimmers',
  DETUNE: 'how far the voices drift out of tune with each other', DRAWBARS: 'which drawbars are out: mellow to full', ENSEMBLE: 'how many players: one to a whole section', GROWL: 'how much it growls underneath',
  JAWARI: 'the buzzing bridge of the tanpura', LESLIE: 'the rotating speaker: slow swirl to fast tremble', MOTION: 'how much it moves by itself', MUSETTE: 'detuned reeds: the accordion wobble',
  RING: 'how long the bowls ring', SHINE: 'an octave of sheen on top', SPEED: 'how fast it falls or rises', STOPS: 'which stops are pulled: soft to full', STRIKES: 'how often the bowls are struck',
  SWEEP: 'how far the sound sweeps', TREMULANT: 'a gentle organ shake', VIBRATO: 'how much the pitch wavers', CHAOS: 'how unpredictable it is', CRACKS: 'how many cracks and splinters',
  DENSITY: 'how often things happen', RATE: 'how fast it goes', STUMBLE: 'how unevenly it stumbles', WANDER: 'how far it wanders around you', LEVEL: 'how loud it is',
};
const TAB_TIPS = { 'OSC A': 'Oscillator A: the main tone', 'OSC B': 'Oscillator B: a second tone to blend with A (SYNC for screaming leads)', 'SUB · NOISE': 'A clean sub an octave or two below, and noise for breath and grit',
  'EFFECTS': 'Ten effects after the sound: click one to see its knobs, double-click it to switch it on or off', 'LFO': 'A slow wave that moves the pitch, filter, shape, volume or pan' };
function tips() {
  const dev = K.dev;
  dev.querySelectorAll('[data-param]').forEach(e => {
    const id = e.dataset.param;
    if (/^atm\.(bed|drn|tex)\./.test(id) && e.classList.contains('sy-kn')) {          // AETHER's knobs: their label follows the type
      const l = e.parentElement && e.parentElement.querySelector('.sy-kl'), L = l ? l.textContent.trim() : '';
      if (LT[L]) { e.title = L.charAt(0) + L.slice(1).toLowerCase() + ': ' + LT[L]; return; }
    }
    if (!e.title && PT[id]) e.title = PT[id];
  });
  dev.querySelectorAll('.sy-tab').forEach(b => { const t = TAB_TIPS[b.textContent.trim()]; if (t && !b.title) b.title = t; });
  dev.querySelectorAll('canvas').forEach(c => { const v = c.closest('.sy-vis'); if (v && !v.title) v.title = 'What it sounds like, drawn'; });
}
let tipT = 0;
new MutationObserver(() => { if (!tipT) tipT = requestAnimationFrame(() => { tipT = 0; tips(); }); }).observe(K.dev, { childList: true, subtree: true, characterData: true });
tips();

/* ============================================================ the sound (browser preview, Web Audio)
   SIREN's voice (OCTAVE is played by the voice itself: real oscillators an octave or a fifth away)
   -> DRIVE -> CRUSH -> RING -> CHORUS -> FLANGER -> PHASER -> DOUBLER -> DELAY -> REVERB -> SHIMMER -> WIDTH
   -> SIREN's SPACE and output. Each effect blends with the dry sound; an effect that is off passes the sound untouched
   (the heavy ones are also unplugged, so they cost nothing).
   Built for sound, not for cheapness:
   - DELAY, REVERB and SHIMMER ring out when switched off (they stop listening; their tails fade on their own).
   - REVERB / SHIMMER: convolution with impulses grown like a real space (rendered offline, off the main thread): every
     band rings for its own time (highs die first), early reflections, a bloom, a pre-delay, and a slow shimmer of
     modulation on the tail so held chords never sound frozen.
   - SHIMMER: a long, blooming, moving reverb beside the REVERB; the octaves that climb in it are clean oscillators in SIREN's voice
     (swelling in after each chord and ringing on into the tail), not a pitch shifter.
   - DELAY: filtered, softly saturated repeats with wow and flutter; PING-PONG or TAPE.
   - CHORUS: three voices a side on their own LFOs, the depth in cents (musical at any rate), the bass kept clean.
   - FLANGER: stereo, an exponential sweep, feedback built from taps (a Web Audio loop can't go under 2.7 ms).
   - PHASER: stereo, four 2-pole allpasses swept in octaves, the feedback's resonance as a peak riding the sweep.
   - DRIVE: TUBE (asymmetric, warm) or FUZZ (hard, buzzing), pre-emphasis, DC blocking and a 24 dB tone filter.
   - CRUSH: bits AND sample rate (a true sample-and-hold, from a delay line on a ramp). */
let FXN = null;
const v01 = id => S[id] / 100;
const secOf = v => .6 * Math.pow(20, v / 100);
const pending = new Set();                                                     // impulses being rendered
K.fxReady = () => Promise.all([...pending]);
const SENDS = new WeakMap(); K.shmSendFor = ac => SENDS.get(ac);       // where SIREN's voice sends SHIMMER's octaves (only the fallback shimmer)
/* the real SHIMMER runs in an AudioWorklet (fx-worklet.js): from the page's own file, else from a blob of the same code. If a browser
   refuses both, the fallback shimmer below plays instead */
const WL = new WeakMap();
function loadWorklet(ac) {
  if (WL.has(ac)) return WL.get(ac);
  const p = (async () => {
    if (!ac.audioWorklet || typeof AudioWorkletNode === 'undefined') throw new Error('no AudioWorklet');
    try { await ac.audioWorklet.addModule('fx-worklet.js'); return 'file'; } catch (_) {}
    if (!window.OBS_FX_WORKLET) throw new Error('no worklet source');
    const url = URL.createObjectURL(new Blob([window.OBS_FX_WORKLET], { type: 'application/javascript' }));
    await ac.audioWorklet.addModule(url); return 'blob';
  })();
  WL.set(ac, p); return p;
}
K.makeIR = (...a) => makeIR(...a);                                          // for checking

/* the spaces: early reflections [count, first, last, last added at full size], their share, when the tail starts and how
   fast it blooms [small, big], how long each band rings (lows ... highs) and how loud it starts, and the tail's wobble */
const RTYPE = [
  /* ROOM */      { er: [16, .0025, .02, .016], erE: .45, start: [.002, .005], rise: [.003, .01], mult: [.9, 1, 1, .85, .68, .5], gain: [1, 1, .95, .85, .68, .48], mod: .00018 },
  /* HALL */      { er: [12, .007, .04, .045], erE: .2, start: [.008, .022], rise: [.012, .05], mult: [1.25, 1.1, 1, .84, .64, .44], gain: [1, 1, .95, .8, .6, .4], mod: .00042 },
  /* PLATE */     { er: null, erE: 0, start: [0, 0], rise: [.0012, .002], mult: [.72, .95, 1, .96, .86, .72], gain: [.6, .92, 1, 1, .95, .85], mod: .00032 },
  /* CATHEDRAL */ { er: [10, .016, .08, .08], erE: .15, start: [.02, .045], rise: [.04, .15], mult: [1.4, 1.2, 1, .76, .52, .32], gain: [1, 1, .9, .74, .52, .32], mod: .0006 },
  /* SHIMMER */   { er: null, erE: 0, start: [.02, .03], rise: [.18, .3], mult: [1, 1.05, 1, .92, .8, .62], gain: [.6, .9, 1, .95, .85, .66], mod: .0006 },
];
const DK_M = [0, 0, .1, .35, .55, .7], DK_G = [0, 0, .1, .35, .6, .85];       // DARK: how much shorter and quieter each band gets
const irCache = new Map();
function makeIR(sr, sec, dark, ti, s01) {
  const key = [sr, sec.toFixed(2), dark.toFixed(2), ti, s01.toFixed(2)].join(':');
  if (irCache.has(key)) return irCache.get(key);
  const ty = RTYPE[ti], lerp = ([a, b]) => a + (b - a) * s01, start = lerp(ty.start), tau = lerp(ty.rise);
  const len = Math.round(Math.min(9.5, start + sec * 1.15 + .06) * sr);         // (the end fades out: no cut-off tail)
  const off = new OfflineAudioContext(2, len, sr), mg = off.createChannelMerger(2), rise = off.createGain();
  rise.gain.setValueAtTime(0, 0); if (start > 0) rise.gain.setValueAtTime(0, start); rise.gain.setTargetAtTime(1, start, tau);
  mg.connect(rise); rise.connect(off.destination);
  const XO = [250, 800, 2500, 6000, 12000].filter(f => f < sr * .42);
  for (let c = 0; c < 2; c++) {
    const nb = off.createBuffer(1, len, sr), d = nb.getChannelData(0); for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1;
    const src = off.createBufferSource(); src.buffer = nb; src.start(0);
    /* the bands: a Linkwitz-Riley tree (24 dB slopes, they add up flat), so each band really rings for its own time */
    const lr = (type, f, from) => { const b1 = off.createBiquadFilter(), b2 = off.createBiquadFilter(); b1.type = b2.type = type; b1.frequency.value = b2.frequency.value = f; b1.Q.value = b2.Q.value = -3.0103;   /* Butterworth (Q .707, in dB) */ from.connect(b1); b1.connect(b2); return b2; };
    let rest = src; const bands = [];
    XO.forEach(f => { bands.push(lr('lowpass', f, rest)); rest = lr('highpass', f, rest); }); bands.push(rest);
    bands.forEach((bn, k) => {
      const mk = ty.mult[k] * (1 - dark * DK_M[k]), gk = Math.max(1e-4, ty.gain[k] * (1 - dark * DK_G[k])), rt = Math.max(.06, sec * mk);
      const env = off.createGain();
      env.gain.setValueAtTime(gk, 0); env.gain.setValueAtTime(gk, start);
      env.gain.exponentialRampToValueAtTime(gk * 1e-3, start + rt); env.gain.exponentialRampToValueAtTime(gk * 1e-5, start + rt * 5 / 3);
      bn.connect(env); env.connect(mg, 0, c);
    });
  }
  const p = off.startRendering().then(buf => {
    const en = ch => { let e = 0; for (let i = 0; i < ch.length; i++) e += ch[i] * ch[i]; return e; };
    const chs = [buf.getChannelData(0), buf.getChannelData(1)];
    if (ty.er) {                                                                // early reflections: sparse taps, darker with DARK, their own on each side
      const [n, t0, t1b, t1x] = ty.er, t1 = t1b + t1x * s01, a = 1 - Math.exp(-2 * Math.PI * (9000 - 6000 * dark) / sr);
      chs.forEach(ch => {
        const L = Math.min(ch.length, Math.ceil((t1 + .01) * sr)), e = new Float32Array(L);
        for (let j = 0; j < n; j++) { const u = (j + Math.random() * .8) / n, t = t0 + (t1 - t0) * u, i = Math.min(L - 1, Math.round(t * sr));
          e[i] += (1 - .65 * u) * (.55 + .45 * Math.random()) * (Math.random() < .5 ? -1 : 1); }
        let y1 = 0, y2 = 0; for (let i = 0; i < L; i++) { y1 += (e[i] - y1) * a; y2 += (y1 - y2) * a; e[i] = y2; }
        const s = Math.sqrt(ty.erE * en(ch) / (en(e) + 1e-12)); for (let i = 0; i < L; i++) ch[i] += e[i] * s;
      });
    }
    const fl = Math.round(len * .22); chs.forEach(ch => { for (let i = 0; i < fl; i++) ch[len - 1 - i] *= .5 - .5 * Math.cos(Math.PI * i / fl); });
    const s = 1 / Math.sqrt((en(chs[0]) + en(chs[1])) / 2 + 1e-12); chs.forEach(ch => { for (let i = 0; i < ch.length; i++) ch[i] *= s; });
    return buf;
  });
  irCache.set(key, p); if (irCache.size > 10) irCache.delete(irCache.keys().next().value);
  return p;
}

function fxChain(ac, input) {
  const T = (p, v, tc = .03) => { const now = ac.currentTime; if (p.cancelAndHoldAtTime) p.cancelAndHoldAtTime(now); else { p.cancelScheduledValues(now); p.setValueAtTime(p.value, now); } p.setTargetAtTime(v, now, tc); };
  const G = v => { const g = ac.createGain(); g.gain.value = v; return g; };
  /* q is a plain Q for every type (Web Audio reads a lowpass's or highpass's Q in dB: .707 there would be a resonant bump) */
  const QV = (type, q) => type === 'lowpass' || type === 'highpass' ? 20 * Math.log10(q) : q;
  const filt = (type, f, q = .707, gain) => { const b = ac.createBiquadFilter(); b.type = type; b.frequency.value = f; b.Q.value = QV(type, q); if (gain != null) b.gain.value = gain; return b; };
  const shaper = (fn, n = 8192, os = '4x') => { const ws = ac.createWaveShaper(), c = new Float32Array(n); for (let i = 0; i < n; i++) c[i] = fn(i / (n - 1) * 2 - 1); ws.curve = c; ws.oversample = os; return ws; };
  const split = () => ac.createChannelSplitter(2), merge = () => ac.createChannelMerger(2);
  /* an LFO with a starting phase (degrees), sine or triangle, so stereo pairs stay locked however the rate moves */
  const lfo = (f, deg = 0, tri = false) => { const H = tri ? 15 : 1, re = new Float32Array(H + 1), im = new Float32Array(H + 1), ph = deg * Math.PI / 180;
    for (let n = 1; n <= H; n++) { const c = tri ? (n % 2 ? 8 / (Math.PI * Math.PI) * ((n - 1) / 2 % 2 ? -1 : 1) / (n * n) : 0) : 1; re[n] = c * Math.sin(n * ph); im[n] = c * Math.cos(n * ph); }
    const o = ac.createOscillator(); o.setPeriodicWave(ac.createPeriodicWave(re, im, { disableNormalization: true })); o.frequency.value = f; o.start(); return o; };
  const mono = () => { const g = G(1); g.channelCount = 1; g.channelCountMode = 'explicit'; g.channelInterpretation = 'speakers'; return g; };

  /* a block: in -> dry -> out, in -> (pi ... wet) -> out. Off: dry 1, wet 0, and pi unplugged after the tail */
  /* trails: switched off, it stops listening but what's in it rings out (echoes, reverb tails); from: where it listens (SHIMMER
     listens beside the REVERB, to what goes into it) */
  function block(id, opts) {
    const b = { i: G(1), o: G(1), dry: G(1), wet: G(0), pi: G(1), plugged: false, t: null };
    const src = () => opts.from ? opts.from() : b.i, unplug = () => { if (b.plugged && !b.live()) { try { src().disconnect(b.pi); } catch (_) {} b.plugged = false; } };
    b.i.connect(b.dry); b.dry.connect(b.o); b.wet.connect(b.o);
    b.live = () => id === 'dbl' ? on('syn.fx.dbl') : on('syn.fx.on') && on('syn.fx.' + id + '.on');
    b.mix = () => { const m = id === 'dbl' ? 1 : v01('syn.fx.' + id + '.mix'), isOn = b.live();
      const [d, w] = isOn ? opts.blend(m) : [1, 0];
      T(b.dry.gain, d); clearTimeout(b.t);
      if (!isOn && opts.trails && on('syn.fx.on') && !K.fxNoTrails) {            /* (the master OFF still means dry, now: no tails) */ T(b.pi.gain, 0, .015); if (b.plugged) b.t = setTimeout(() => { if (!b.live()) { T(b.wet.gain, 0, .05); b.t = setTimeout(unplug, 300); } }, opts.tail); return; }
      if (isOn) T(b.pi.gain, 1, .01);
      if (isOn && !b.plugged && opts.lead) { const p = b.wet.gain, now = ac.currentTime; if (p.cancelAndHoldAtTime) p.cancelAndHoldAtTime(now); else { p.cancelScheduledValues(now); p.setValueAtTime(p.value, now); }
        p.setTargetAtTime(w, now + opts.lead, .03); }            // its delay lines fill first: the copies come in smoothly, not with a step
      else T(b.wet.gain, w);
      if (isOn && !b.plugged) { src().connect(b.pi); b.plugged = true; }
      if (!isOn && b.plugged) b.t = setTimeout(unplug, opts.tail || 300); };
    return b;
  }
  const xfade = m => [1 - m, m];

  /* a slow stereo wobble for reverb tails (a few cents at most): keeps long tails alive */
  function wobble(depth, rates = [.17, .23]) {
    const sp = split(), mg = merge(), i = G(1), o = G(1);
    const gs = []; i.connect(sp); [[.0061, rates[0], 0], [.0073, rates[1], 90]].forEach(([dt, f, ph], c) => { const d = ac.createDelay(.05), l = lfo(f, ph), g = G(depth); d.delayTime.value = dt; gs.push(g);
      l.connect(g); g.connect(d.delayTime); sp.connect(d, c); d.connect(mg, 0, c); });
    mg.connect(o); return { i, o, set: v => gs.forEach(g => T(g.gain, v, .1)) };
  }
  /* a convolver whose impulse can be swapped smoothly (the new one fades in while the old one fades out) */
  function verb(into, out) {
    const v = { cur: null, gen: 0, key: '' };
    v.set = buf => { const c = ac.createConvolver(); c.normalize = false; c.buffer = buf; const g = G(v.cur ? 0 : 1); into.connect(c); c.connect(g); g.connect(out);
      if (v.cur) { const o = v.cur; T(g.gain, 1, .06); T(o.g.gain, 0, .1); setTimeout(() => { try { into.disconnect(o.c); o.c.disconnect(); o.g.disconnect(); } catch (_) {} }, 1500); }
      v.cur = { c, g }; };
    v.want = (key, args, now) => { if (key === v.key) return; v.key = key; const gen = ++v.gen;
      const go = () => { if (gen !== v.gen) return; const p = makeIR(ac.sampleRate, ...args).then(buf => { if (gen === v.gen) v.set(buf); }); pending.add(p); p.finally(() => pending.delete(p)); };
      clearTimeout(v.t); if (now) go(); else v.t = setTimeout(go, 120); };
    return v;
  }

  /* ------------------------------------------------ DRIVE: TUBE or FUZZ, then a 24 dB tone filter */
  const drv = block('drv', { blend: xfade });
  const DRV_T = [.96, .355, .168, .128, .116], DRV_F = [.098, .09, .089, .089, .089];
  const lerpT = (tb, a) => { const x = a * (tb.length - 1), i = Math.min(tb.length - 2, Math.floor(x)), u = x - i; return tb[i] * Math.pow(tb[i + 1] / tb[i], u); };
  const R = 32, drvMix = G(1), dc = filt('highpass', 22, .6), lp1 = filt('lowpass', 6000, .62), lp2 = filt('lowpass', 6000, .54), pres = filt('peaking', 3200, .9, 1.5), drvOut = G(1);
  const tube = { hp: filt('highpass', 70, .6), emph: filt('peaking', 900, .7, 2), pre: G(1 / R), sh: shaper(u => Math.tanh(R * u + .25) - Math.tanh(.25), 16384), g: G(1) };
  const fuzz = { hp: filt('highpass', 150, .6), emph: filt('peaking', 1100, .8, 6), pre: G(1 / R), sh: shaper(u => { const x = R * u; return x >= 0 ? .82 * Math.tanh(2.5 * x) : Math.tanh(4 * x); }, 16384), g: G(0) };
  const fuzz2 = shaper(u => Math.tanh(2.2 * u) / Math.tanh(2.2), 4096);
  [tube, fuzz].forEach(c => { drv.pi.connect(c.hp); c.hp.connect(c.emph); c.emph.connect(c.pre); c.pre.connect(c.sh); });
  tube.sh.connect(tube.g); fuzz.sh.connect(fuzz2); fuzz2.connect(fuzz.g); tube.g.connect(drvMix); fuzz.g.connect(drvMix);
  drvMix.connect(dc); dc.connect(lp1); lp1.connect(lp2); lp2.connect(pres); pres.connect(drvOut); drvOut.connect(drv.wet);
  drv.update = () => {
    const a = v01('syn.fx.drv.amt'), fz = Math.round(S['syn.fx.drv.mode']) === 1, f = 1600 * Math.pow(2, v01('syn.fx.drv.tone') * 2.9);
    const gT = 1 + a * a * 30, gF = 4 + a * a * 140;
    T(tube.pre.gain, gT / R); T(fuzz.pre.gain, gF / R); T(tube.g.gain, fz ? 0 : 1, .02); T(fuzz.g.gain, fz ? 1 : 0, .02);
    T(lp1.frequency, f); T(lp2.frequency, f * (fz ? .85 : 1.1));
    T(drvOut.gain, lerpT(fz ? DRV_F : DRV_T, a));                               // about as loud as the clean sound (measured)
    drv.mix(); };

  /* ------------------------------------------------ CRUSH: sample-and-hold (lower rate) then fewer bits */
  const cru = block('cru', { blend: xfade });
  const cruPre = G(2.6), cruPost = G(1 / 2.6); cruPost.connect(cru.wet);
  let cruHold = null, cruK = -1, cruQ = null, cruBits = -1;
  cru.update = () => {
    const k = Math.max(1, Math.round(ac.sampleRate / S['syn.fx.cru.rate']));
    if (k !== cruK) { cruK = k;                                                // a delay line whose delay climbs one sample per sample, then jumps back: a held sample
      const d = ac.createDelay(.05), b = ac.createBuffer(1, k, ac.sampleRate), r = b.getChannelData(0); for (let i = 0; i < k; i++) r[i] = i / ac.sampleRate;
      const src = ac.createBufferSource(); src.buffer = b; src.loop = true; d.delayTime.value = 0; src.connect(d.delayTime); src.start();
      const g = G(cruHold ? 0 : 1); cru.pi.connect(d); d.connect(g); g.connect(cruPre);
      if (cruHold) { const o = cruHold; T(g.gain, 1, .01); T(o.g.gain, 0, .01); setTimeout(() => { try { cru.pi.disconnect(o.d); o.src.stop(); o.d.disconnect(); o.g.disconnect(); } catch (_) {} }, 150); }
      cruHold = { d, g, src }; }
    const bits = Math.round(S['syn.fx.cru.bits']);
    if (bits !== cruBits) { cruBits = bits; const q = Math.pow(2, bits - 1), sh = shaper(x => Math.round(x * q) / q, 16384, 'none'), g = G(cruQ ? 0 : 1);
      cruPre.connect(sh); sh.connect(g); g.connect(cruPost);
      if (cruQ) { const o = cruQ; T(g.gain, 1, .01); T(o.g.gain, 0, .01); setTimeout(() => { try { cruPre.disconnect(o.sh); o.sh.disconnect(); o.g.disconnect(); } catch (_) {} }, 150); }
      cruQ = { sh, g }; }
    cru.mix(); };

  /* ------------------------------------------------ RING: times a sine, a hair apart on each side (wide, alive) */
  const ring = block('ring', { blend: m => [Math.cos(m * Math.PI / 2), Math.sin(m * Math.PI / 2)] });
  const rSp = split(), rMg = merge(), rL = G(0), rR = G(0), oL = lfo(220), oR = lfo(220, 90), rPost = G(1.45);
  oL.connect(rL.gain); oR.connect(rR.gain); ring.pi.connect(rSp); rSp.connect(rL, 0); rSp.connect(rR, 1); rL.connect(rMg, 0, 0); rR.connect(rMg, 0, 1); rMg.connect(rPost); rPost.connect(ring.wet);
  ring.update = () => { const f = S['syn.fx.ring.freq']; T(oL.frequency, f * .997, .05); T(oR.frequency, f * 1.003, .05); ring.mix(); };

  /* ------------------------------------------------ CHORUS: three voices a side, each on its own LFO; the depth is in cents */
  const cho = block('cho', { blend: m => [1 - m * .4, m * .9], lead: .022 });
  const cSp = split(), cMg = merge(), cV = [];
  cho.pi.connect(cSp);
  [0, 1].forEach(c => { const sum = G(1 / Math.sqrt(3)), hp = filt('highpass', 160, .55), lp = filt('lowpass', 9500, .6);
    [.0072, .0114, .0157].forEach((dt, j) => { const d = ac.createDelay(.05), l = lfo(.8, j * 120 + c * 60), g = G(0), rm = [1, 1.09, .92][j]; d.delayTime.value = dt + c * .0013;
      l.connect(g); g.connect(d.delayTime); cSp.connect(d, c); d.connect(sum); cV.push({ l, g, rm }); });
    sum.connect(hp); hp.connect(lp); lp.connect(cMg, 0, c); });
  cMg.connect(cho.wet);
  cho.update = () => { const r = S['syn.fx.cho.rate'], cents = v01('syn.fx.cho.depth') * 24;
    cV.forEach(v => { const f = r * v.rm; T(v.l.frequency, f, .05); T(v.g.gain, Math.min(.0055, cents * .000578 / (2 * Math.PI * f))); }); cho.mix(); };

  /* ------------------------------------------------ FLANGER: stereo, swept in octaves, feedback as a train of taps */
  const fla = block('fla', { blend: xfade, lead: .08 });
  const fSp = split(), fMg = merge(), fTaps = [], NT = 6;
  const fL = lfo(.15, 0, true), fR = lfo(.15, 90, true), fDep = [G(0), G(0)];
  fla.pi.connect(fSp);
  const fComp = [];
  [fL, fR].forEach((l, c) => { const e = shaper(u => Math.pow(2, 4 * u), 4096, 'none'), base = G(.0024), lp = filt('lowpass', 9000, .6), sum = G(.5), dir = G(1);
    fSp.connect(dir, c); dir.connect(sum); lp.connect(sum); sum.connect(fMg, 0, c); fComp.push(sum);
    l.connect(fDep[c]); fDep[c].connect(e); e.connect(base);
    for (let k = 1; k <= NT; k++) { const d = ac.createDelay(.1), km = G(k), tg = G(0); d.delayTime.value = 0; base.connect(km); km.connect(d.delayTime);
      fSp.connect(d, c); d.connect(tg); tg.connect(lp); fTaps.push({ tg, k }); }
  });
  fMg.connect(fla.wet);
  fla.update = () => { const r = S['syn.fx.fla.rate'], dep = v01('syn.fx.fla.depth') * 2.4, g = S['syn.fx.fla.fb'] / 100 * .9;
    T(fL.frequency, r, .05); T(fR.frequency, r, .05); fDep.forEach(x => T(x.gain, dep / 4));
    fTaps.forEach(t => T(t.tg.gain, Math.pow(g, t.k - 1))); fComp.forEach(s => T(s.gain, .78 * (1 - g * .45))); fla.mix(); };

  /* ------------------------------------------------ PHASER: stereo, four 2-pole allpasses swept in octaves, resonance riding the sweep */
  const pha = block('pha', { blend: xfade });
  const pSp = split(), pMg = merge(), pL = lfo(.3, 0), pR = lfo(.3, 90), pDep = [G(0), G(0)], pRes = [], pAp = [];
  pha.pi.connect(pSp);
  [pL, pR].forEach((l, c) => { l.connect(pDep[c]); let last = null;
    for (let s = 0; s < 4; s++) { const a = filt('allpass', 620, .55); pDep[c].connect(a.detune); if (last) last.connect(a); else pSp.connect(a, c); last = a; pAp.push(a); }
    const sum = G(.66), dir = G(1); pSp.connect(dir, c); dir.connect(sum); last.connect(sum);
    const pk = filt('peaking', 620, 1, 0); pDep[c].connect(pk.detune); sum.connect(pk); pk.connect(pMg, 0, c); pRes.push(pk); });
  pMg.connect(pha.wet);
  pha.update = () => { const fb = S['syn.fx.pha.fb'] / 100; T(pL.frequency, S['syn.fx.pha.rate'], .05); T(pR.frequency, S['syn.fx.pha.rate'], .05);
    pDep.forEach(x => T(x.gain, v01('syn.fx.pha.depth') * 3000)); pRes.forEach(p => { T(p.gain, fb * 13); T(p.Q, .9 + fb * 2.2); }); pAp.forEach(a => T(a.Q, .5 + fb * .25)); pha.mix(); };

  /* ------------------------------------------------ DOUBLER: two late, wavering copies, hard left and hard right */
  const dbl = block('dbl', { blend: () => [.74, .76], lead: .035 });
  const dblMono = mono(), dblMg = merge();
  [[.0168, .21, .0019, .067, .0007, 165, 9800], [.0287, .33, .0024, .049, .0009, 150, 8800]].forEach(([dt, f1, a1, f2, a2, hpf, lpf], ch) => {
    const d = ac.createDelay(.1), hp = filt('highpass', hpf, .6), lp = filt('lowpass', lpf, .6), l1 = lfo(f1, ch * 70), l2 = lfo(f2, ch * 140), g1 = G(a1), g2 = G(a2);
    d.delayTime.value = dt; l1.connect(g1); l2.connect(g2); g1.connect(d.delayTime); g2.connect(d.delayTime); dblMono.connect(d); d.connect(hp); hp.connect(lp); lp.connect(dblMg, 0, ch); });
  dbl.pi.connect(dblMono); dblMg.connect(dbl.wet);
  dbl.update = () => dbl.mix();

  /* ------------------------------------------------ DELAY: PING-PONG (bouncing) or TAPE (warm, wobbly, in the middle) */
  const dly = block('dly', { blend: m => [1, m], tail: 12000, trails: true });
  const dIn = mono(), dl = [ac.createDelay(4.6), ac.createDelay(4.6)], inj = [G(1), G(0)], dMg = merge(), dS = [], dSelf = [], dCross = [];
  dly.pi.connect(dIn);
  dl.forEach((d, c) => { dIn.connect(inj[c]); inj[c].connect(d);
    const hp = filt('highpass', 120, .5), lp = filt('lowpass', 5000, .55), pre = G(.25), sh = shaper(u => Math.tanh(5.2 * u), 4096, '2x'), post = G(1), out = G(1), self = G(0), cross = G(0);
    d.connect(hp); hp.connect(lp); lp.connect(pre); pre.connect(sh); sh.connect(post); post.connect(out);
    out.connect(self); self.connect(d); out.connect(cross); cross.connect(dl[1 - c]); out.connect(dMg, 0, c);
    dS.push({ hp, lp, pre, post }); dSelf.push(self); dCross.push(cross); });
  dMg.connect(dly.wet);
  const wow = [lfo(.52, 0), lfo(.61, 110)], wowG = [G(0), G(0)], flut = lfo(6.3, 0), flutG = G(0);
  wow.forEach((l, c) => { l.connect(wowG[c]); wowG[c].connect(dl[c].delayTime); flutG.connect(dl[c].delayTime); }); flut.connect(flutG);
  dly.update = () => {
    const tape = Math.round(S['syn.fx.dly.mode']) === 1;
    const t = on('syn.fx.dly.sync') ? 60 / (K.getBpm ? K.getBpm() : 120) * [.25, 1 / 3, .5, .75, 2 / 3, 1, 1.5, 2][Math.round(S['syn.fx.dly.div'])] : S['syn.fx.dly.time'] / 1000;
    const fb = S['syn.fx.dly.fb'] / 100, f = 1000 * Math.pow(2, v01('syn.fx.dly.tone') * 3.9) * (tape ? .62 : 1), drive = tape ? 1.7 : .55;
    T(dl[0].delayTime, t, .08); T(dl[1].delayTime, t + (tape ? .009 : 0), .08);
    T(inj[1].gain, tape ? 1 : 0, .02); dSelf.forEach(g => T(g.gain, tape ? fb : 0, .02)); dCross.forEach(g => T(g.gain, tape ? 0 : fb, .02));
    dS.forEach(s => { T(s.lp.frequency, f); T(s.hp.frequency, tape ? 190 : 110); T(s.pre.gain, drive / 4); T(s.post.gain, 1 / (1.3 * drive)); });
    wowG.forEach(g => T(g.gain, tape ? .00075 : .00012, .1)); T(flutG.gain, tape ? .00006 : .000015, .1);
    dly.mix(); };

  /* ------------------------------------------------ REVERB */
  const rev = block('rev', { blend: m => [1 - m * .3, m * .82], tail: 12000, trails: true });
  const revHp = filt('highpass', 120, .6), revPre = ac.createDelay(.5), revSum = G(1), revWob = wobble(RTYPE[1].mod);
  const revMono = mono(); rev.pi.connect(revHp); revHp.connect(revMono); revMono.connect(revPre); revSum.connect(revWob.i); revWob.o.connect(rev.wet);
  const revV = verb(revPre, revSum);
  rev.update = () => { const ti = Math.round(S['syn.fx.rev.type']), sz = S['syn.fx.rev.size'], dk = Math.round(S['syn.fx.rev.damp']);
    T(revPre.delayTime, Math.max(0, S['syn.fx.rev.pre'] / 1000 - .0067), .05); revWob.set(RTYPE[ti].mod); T(revSum.gain, [1, 1, 1.22, .94][ti]);
    if (rev.live() || revV.cur) revV.want(ti + ':' + Math.round(sz) + ':' + dk, [secOf(sz), dk / 100, ti, sz / 100], !revV.cur);
    rev.mix(); };

  /* ------------------------------------------------ SHIMMER: a long reverb that blooms and moves, beside the REVERB (not after it).
     The octaves that climb in it come from SIREN's own voice (synth.js): clean oscillators an octave and two up that swell in after
     each chord, drift a little and ring on into the tail. They come in through K.shmSendFor(ac). Nothing is pitch-shifted, so
     nothing warbles, chops or sounds canned */
  const shm = block('shm', { blend: m => [1 - m * .2, m * .95], tail: 20000, trails: true, from: () => rev.i });
  const shmHp = filt('highpass', 220, .6), shmSum = G(1), shmOut = G(1), shmBase = G(.6), shmMono = mono(), shmWob = wobble(RTYPE[4].mod), shmWob2 = wobble(.00025, [.71, .93]);
  shm.pi.connect(shmHp); shmHp.connect(shmMono); shmMono.connect(shmBase); shmBase.connect(shmSum);
  const shmSend = G(1), sendHp = filt('highpass', 320, .6), sendLp = filt('lowpass', 7500, .6);
  shmSend.connect(sendHp); sendHp.connect(sendLp); sendLp.connect(shmSum);
  const shmFb = G(1); shmOut.connect(shmWob.i); shmWob.o.connect(shmWob2.i); shmWob2.o.connect(shmFb); shmFb.connect(shm.wet);
  const shmV = verb(shmSum, shmOut);
  SENDS.set(ac, shmSend);
  let shmW = null;                                                  // the real one, once loaded
  const shmWIn = filt('highpass', 160, .6), shmWOut = G(0); shmWOut.connect(shm.wet);
  const wl = loadWorklet(ac).then(how => {
    shmW = new AudioWorkletNode(ac, 'obs-shimmer', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] });
    shm.pi.connect(shmWIn); shmWIn.connect(shmW); shmW.connect(shmWOut);
    SENDS.delete(ac); T(shmSend.gain, 0, .05); T(shmFb.gain, 0, .08); T(shmWOut.gain, .83, .08);
    setTimeout(() => { try { shm.pi.disconnect(shmHp); } catch (_) {} }, 400);
    if (FXN) FXN.shimmer = how; shm.update();
  }).catch(e => { if (FXN) FXN.shimmer = 'fallback (' + e.message + ')'; });
  pending.add(wl); wl.finally(() => pending.delete(wl));
  shm.update = () => { const dk = Math.round(S['syn.fx.shm.decay']);
    if (shmW) { const P = n => shmW.parameters.get(n);
      T(P('decay'), secOf(dk), .05); T(P('rise'), v01('syn.fx.shm.amt'), .05); T(P('fifth'), Math.round(S['syn.fx.shm.mode']), .05); }
    else if (shm.live() || shmV.cur) shmV.want('s' + dk, [secOf(dk), .22, 4, .8], !shmV.cur);
    shm.mix(); };

  /* ------------------------------------------------ WIDTH: mid / side; widening only above 150 Hz (the bass stays mono and solid) */
  const wIn = G(1), wSp = split(), wMg = merge(), wMid = G(1), wSide = G(1), wSideHi = G(0), wHp = filt('highpass', 150, .6), wNeg = G(-1), wSum = G(1);
  const wLm = G(.5), wRm = G(.5), wLs = G(.5), wRs = G(-.5);
  wIn.connect(wSp); wSp.connect(wLm, 0); wSp.connect(wRm, 1); wSp.connect(wLs, 0); wSp.connect(wRs, 1);
  wLm.connect(wMid); wRm.connect(wMid); wLs.connect(wSum); wRs.connect(wSum); wSum.connect(wSide); wSum.connect(wHp); wHp.connect(wSideHi);
  wSide.connect(wNeg); wSideHi.connect(wNeg);
  wMid.connect(wMg, 0, 0); wMid.connect(wMg, 0, 1); wSide.connect(wMg, 0, 0); wSideHi.connect(wMg, 0, 0); wNeg.connect(wMg, 0, 1);
  const width = { update: () => { const w = S['syn.fx.width'] / 100; T(wSide.gain, Math.min(1, w)); T(wSideHi.gain, Math.max(0, w - 1)); } };

  const B = { drv, cru, ring, cho, fla, pha, dbl, dly, rev, shm };
  let last = input;
  ORDER.forEach(id => { if (!B[id]) return; last.connect(B[id].i); last = B[id].o; });
  last.connect(wIn);
  Object.values(B).forEach(b => b.update()); width.update();
  FXN = { B, width, out: wMg };
  return wMg;
}
K.fxChain = fxChain;
K.fxTempo = () => { if (FXN) FXN.B.dly.update(); };
/* a safety limiter on the very end, so stacked feedback never clips */
K.fxLimit = ac => {
  const l = ac.createDynamicsCompressor(); l.threshold.value = -2; l.knee.value = 0; l.ratio.value = 20; l.attack.value = .002; l.release.value = .12;
  const clip = ac.createWaveShaper(), n = 2048, c = new Float32Array(n); for (let i = 0; i < n; i++) { const x = i / (n - 1) * 2 - 1; c[i] = Math.tanh(x * 1.15) / Math.tanh(1.15) * .98; } clip.curve = c;
  l.connect(clip); clip.connect(ac.destination); return l; };
onChange(id => {
  if (!FXN) return;
  if (id === 'syn.fx.width') { FXN.width.update(); return; }
  if (id === 'syn.fx.dbl') { FXN.B.dbl.update(); return; }
  if (id === 'syn.fx.on') { Object.keys(FXN.B).forEach(k => { if (k !== 'dbl') FXN.B[k].update(); }); return; }
  const m = /^syn\.fx\.(\w+)\./.exec(id); if (m && FXN.B[m[1]]) FXN.B[m[1]].update();
});
window.obsSynth = window.obsSynth || {};
window.obsSynth.fx = () => FXN;            // for checking

/* ============================================================ saved in this browser: the ORPHEUS song and every SIREN setting
   (the RESETs clear what's saved too, as they change the settings) */
const SAVE_KEY = 'orpheus.standalone.v1';
const keep = id => /^syn\./.test(id) && id !== 'syn.power';
const native = !!(window.__JUCE__ && window.__JUCE__.backend);      // the plugin / app: its state is saved with the project instead
try {
  if (native) throw 0;
  const raw = localStorage.getItem(SAVE_KEY), d = raw && JSON.parse(raw);
  if (d && (d.v === 1 || d.v === 2)) {
    if (d.S && d.v === 1) { if (d.S['syn.space'] === 20) d.S['syn.space'] = 0; if (d.S['syn.char'] === 1) d.S['syn.char'] = 0; }     // the old defaults: now a straight synth
    if (d.S) for (const id in d.S) if (K.P[id] && keep(id) && isFinite(d.S[id])) set(id, d.S[id], 'preset');
    if (d.song && K.orphLoad) K.orphLoad(d.song);
    if (d.preset && K.setPresetName) K.setPresetName(d.preset[0], d.preset[1]);
  }
} catch (_) {}
let lastSaved = '';
const saveNow = () => { if (native) return; try {
  const s = {}; for (const id in K.P) if (keep(id)) s[id] = S[id];
  const txt = JSON.stringify({ v: 2, S: s, song: K.orphSave ? K.orphSave() : null, preset: K.presetName ? K.presetName() : null });
  if (txt !== lastSaved) { localStorage.setItem(SAVE_KEY, txt); lastSaved = txt; } } catch (_) {} };
setInterval(saveNow, 1500); addEventListener('pagehide', saveNow);
})();
</script>
