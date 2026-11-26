<script>
/* =============================================================================
   AETHER: SIREN's second page. Noise, ambience and atmospheres that play by
   themselves around your playing (no notes needed).
     BED      noise you sit in: hiss, wind, rain, surf, rumble, static, breath,
              brook, night (crickets), forest, soft rain, underwater, air
     DRONE    tones that hang in the air (in SIREN's key, or following you):
              organ (drawbars, Leslie), strings, choir, glass, abyss, descent,
              ascent, pipe organ, pad, angels, tanpura, singing bowls,
              harmonium, flute; SHINE (an octave sheen) and AIR (breath)
     TEXTURE  things that happen now and then: crackle, fire, drips, metal,
              shimmer, whispers, a heartbeat, knocks, moans, flesh, wind
              chimes, birds, a music box, bells, a harp, stars, a kalimba,
              rain on leaves (the melodic ones in SIREN's key and scale)
     VOID     the space it all lives in (SIZE, SPACE, SHIMMER; FREEZE in the strip)
   MOOD decides what the dice, MUTATE and SUMMON reach for (calm .. horror).
   Built on SIREN's device with its controls (synth.js shares them as
   window.__sirenKit). The engine is plugin/Source/DSP/Atmos.h (settings:
   AtmosDefs.h); in a browser a small Web Audio version plays instead.
   ============================================================================= */
(function () {
'use strict';
const K = window.__sirenKit; if (!K) return;
const { el, P, S, set, onChange, on, fmt, toN, toV, clamp, knob, toggle, chip, seg, mknob, sel, pushBtn, section, IC, hidpi, tc, FONT, dev, SP } = K;
const inPlugin = K.inPlugin;

/* ------------------------------------------------------------ names */
const BEDS = ['HISS', 'WIND', 'RAIN', 'SURF', 'RUMBLE', 'STATIC', 'BREATH', 'BROOK', 'NIGHT', 'FOREST', 'SOFT RAIN', 'UNDERSEA', 'AIR'];
const VOICES = ['ORGAN', 'STRINGS', 'CHOIR', 'GLASS', 'ABYSS', 'DESCENT', 'ASCENT', 'PIPES', 'PAD', 'ANGELS', 'TANPURA', 'BOWLS', 'REEDS', 'FLUTE'];   // REEDS: the harmonium
const CHORDS = ['ROOT', '5TH', 'OCT', 'MIN', 'SUS', 'TRI', 'CLUS', 'DREAD', 'MAJ', 'MAJ7', 'ADD9', 'SUS2', 'MIN7', 'MIN9', 'HEAVEN', 'OPEN'];
const CHORD_INT = [[0], [0, 7], [0, 12], [0, 3, 7], [0, 5, 7], [0, 6], [0, 1, 2], [0, 1, 6, 11],
  [0, 4, 7], [0, 4, 7, 11], [0, 4, 7, 14], [0, 2, 7], [0, 3, 7, 10], [0, 3, 10, 14], [0, 7, 14, 16], [0, 7, 12, 19]];      // as in Atmos.h
const TEXS = ['VINYL', 'FIRE', 'DRIPS', 'METAL', 'SHIMMER', 'WHISPERS', 'HEART', 'KNOCKS', 'MOANS', 'FLESH', 'CHIMES', 'BIRDS', 'MUSICBOX', 'BELLS', 'HARP', 'STARS', 'KALIMBA', 'LEAVES'];
const isShep = v => v === 5 || v === 6;                     // Descent, Ascent: no notes, partials gliding forever
const MOODS = ['CALM', 'DREAM', 'EERIE', 'DREAD', 'HORROR'];
const MOOD_COL = ['#7fd6c2', '#9bb4ff', '#9fd18a', '#b07cff', '#e0262f'];

/* MOOD: a page setting (not a sound setting): it steers the dice, MUTATE and SUMMON */
P['ui.mood'] = { id: 'ui.mood', type: 'choice', min: 0, max: 4, def: 1, skew: 1, unit: '', opts: MOODS };
S['ui.mood'] = 1;
try { const x = localStorage.getItem('obs.aetherMood'); if (x != null && +x >= 0 && +x <= 4) S['ui.mood'] = Math.round(+x); } catch (_) {}
onChange((id, v) => { if (id === 'ui.mood') try { localStorage.setItem('obs.aetherMood', String(v)); } catch (_) {} });
const mood = () => Math.round(S['ui.mood']);

/* what the knobs do for each kind of bed, voice and texture (labels follow the type) */
const BED_INFO = [
  ['TONE', 'DRIFT', 'BREATHE', 'Hiss: brown (dark) to white (bright) noise, slowly breathing'],
  ['PITCH', 'GUSTS', 'HOWL', 'Wind: gusts that come and go, and a howl when it gets wild'],
  ['TONE', 'AMOUNT', 'DROPS', 'Rain: from a drizzle to a downpour, drops of every size'],
  ['TONE', 'PACE', 'SWELL', 'Surf: waves rolling in, left and right'],
  ['TONE', 'DRIFT', 'HEAVE', 'Rumble: deep earth and a sub that heaves'],
  ['BAND', 'CRACKLE', 'WHISTLE', 'Static: an old radio, crackle and a tuning whistle'],
  ['THROAT', 'PACE', 'RASP', 'Breath: slow breathing, ragged and rasping when DEEP'],
  ['PITCH', 'FLOW', 'SPLASH', 'Brook: a stream gurgling over stones, bubbles of every size'],
  ['PITCH', 'CHIRP', 'CHORUS', 'Night: crickets in the grass and the soft night air'],
  ['TONE', 'BREEZE', 'RUSTLE', 'Forest: leaves rustling in a breeze, the low wind under them'],
  ['TONE', 'AMOUNT', 'DROPS', 'Soft rain: gentle rain on a window, small taps on the glass'],
  ['TONE', 'CURRENT', 'BUBBLES', 'Undersea: deep and muffled, slow bubbles rising'],
  ['HEIGHT', 'DRIFT', 'SWELL', 'Air: a soft, high sheen of air, like a huge quiet hall'],
];
const VOICE_INFO = [
  ['CHORUS', 'DRAWBARS', 'LESLIE', 'Organ: tonewheels and drawbars through a Leslie (stopped, slow, then fast)'],
  ['DETUNE', 'TONE', 'MOTION', 'Strings: a section, each player with their own bow and vibrato'],
  ['DETUNE', 'TONE', 'MOTION', 'Choir: voices singing vowels that drift, with breath'],
  ['DETUNE', 'SHINE', 'MOTION', 'Glass: bell-like, struck glass'],
  ['DETUNE', 'GROWL', 'MOTION', 'Abyss: a sub with a growl underneath'],
  ['DETUNE', 'TONE', 'SPEED', 'Descent: a tone that falls forever and never lands'],
  ['DETUNE', 'TONE', 'SPEED', 'Ascent: a tone that rises forever'],
  ['CELESTE', 'STOPS', 'TREMULANT', 'Pipe organ: a soft flute, then principals, then the full organ as STOPS goes up'],
  ['DETUNE', 'TONE', 'SWEEP', 'Pad: warm, slow analog strings, the filter drifting'],
  ['DETUNE', 'TONE', 'MOTION', 'Angels: an airy "ooh", an octave floating above it'],
  ['JAWARI', 'TONE', 'PACE', 'Tanpura: four strings plucked round and round, buzzing as they ring'],
  ['BEAT', 'RING', 'STRIKES', 'Singing bowls: rubbed, and struck now and then, slowly beating'],
  ['MUSETTE', 'STOPS', 'BELLOWS', 'Harmonium: reeds breathing with the bellows'],
  ['ENSEMBLE', 'BREATH', 'VIBRATO', 'Flute: a soft choir of breathy flutes'],
];
const TEX_INFO = [
  ['DENSITY', 'TONE', 'CHAOS', 'Vinyl: crackle and pops of an old record'],
  ['DENSITY', 'TONE', 'CHAOS', 'Fire: a roaring fire, snapping'],
  ['DENSITY', 'PITCH', 'CHAOS', 'Drips: water drops in a cave'],
  ['DENSITY', 'PITCH', 'CHAOS', 'Metal: plates that are struck and scraped'],
  ['DENSITY', 'PITCH', 'CHAOS', 'Shimmer: grains of glass, in your key'],
  ['DENSITY', 'TONE', 'CHAOS', 'Whispers: someone whispering, close, then far'],
  ['RATE', 'TONE', 'STUMBLE', 'Heart: a heartbeat; STUMBLE makes it skip'],
  ['DENSITY', 'TONE', 'CHAOS', 'Knocks: knocking on a door somewhere in the house'],
  ['DENSITY', 'PITCH', 'CHAOS', 'Moans: voices in pain, low groans to high wails'],
  ['DENSITY', 'TONE', 'CRACKS', 'Flesh: wet, cracking, tearing'],
  ['DENSITY', 'PITCH', 'BREEZE', 'Wind chimes: a breeze through tubes tuned to your scale'],
  ['DENSITY', 'PITCH', 'CHAOS', 'Birds: songbirds calling, near and far'],
  ['DENSITY', 'PITCH', 'WANDER', 'Music box: little melodies in your scale'],
  ['DENSITY', 'PITCH', 'CHAOS', 'Bells: a church bell far away in your key; now and then it tolls'],
  ['DENSITY', 'PITCH', 'CHAOS', 'Harp: a few notes rolled up your scale'],
  ['DENSITY', 'PITCH', 'CHAOS', 'Stars: soft, high notes that fade in and twinkle away'],
  ['DENSITY', 'PITCH', 'CHAOS', 'Kalimba: thumb-plucked figures in your scale'],
  ['DENSITY', 'TONE', 'CHAOS', 'Leaves: rain pattering on leaves'],
];

/* ------------------------------------------------------------ styles */
const css = el('style');
css.textContent = `
.sy-dev { --a-bed: #6cc4d8; --a-drn: #b392ff; --a-tex: #ffa25e; --a-void: #ff6b86; }
.ae-disp { cursor: default; }
.ae-disp:hover { box-shadow: inset 0 2px 8px rgba(0,0,0,.95), inset 0 0 0 1px rgba(255,255,255,.04), 0 1px 0 rgba(255,255,255,.08); }
.ae-disp .rows { position: absolute; left: 12px; right: 12px; top: 23px; display: flex; flex-direction: column; gap: 6px; }
.ae-disp .r { display: grid; grid-template-columns: 34px minmax(0, 1fr) 26px; align-items: center; gap: 6px; font-family: 'Orbitron', 'Saira', sans-serif; font-size: 7.5px; letter-spacing: .06em; color: var(--accent); transition: opacity .2s; }
.ae-disp .r b { font-weight: 500; opacity: .5; }
.ae-disp .r u { text-decoration: none; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; text-shadow: 0 0 8px color-mix(in srgb, var(--accent) 50%, transparent); }
.ae-disp .r i { position: relative; height: 4px; border-radius: 2px; background: #19191c; overflow: hidden; }
.ae-disp .r em { position: absolute; left: 0; top: 0; bottom: 0; width: 0; border-radius: 2px; background: var(--lc); box-shadow: 0 0 6px var(--lc); }
.ae-disp .r.off { opacity: .22; }
.ae-disp .fz { color: #bfe9ff; text-shadow: 0 0 8px #7fd3ff; animation: aeBlink 1.6s ease-in-out infinite; }
@keyframes aeBlink { 50% { opacity: .35; } }
.sy-dev.aoff .ae-disp .rows, .sy-dev.aoff .ae-screen { opacity: .42; filter: grayscale(.75); }
.ae-screen { transition: opacity .2s, filter .2s; }
.ae-screen .sy-body { gap: 8px; }
.ae-grid { display: grid; gap: 3px; padding: 3px; border-radius: 8px; background: #0e0e10; box-shadow: inset 0 0 0 1px rgba(255,255,255,.05); flex: none; }
.ae-grid.tight button { height: 21px; font-size: 7.5px; letter-spacing: .03em; padding: 0 1px; }
.ae-chords.tight button { height: 18px; }
.ae-six .sy-kl { letter-spacing: .04em; }
.ae-grid button { height: 24px; min-width: 0; border: none; border-radius: 6px; background: none; color: #6f6f76; font: inherit; font-size: 8.5px; font-weight: 600; letter-spacing: .08em; cursor: pointer; white-space: nowrap; padding: 0 2px; transition: color .12s, background .12s; }
.ae-grid button:hover { color: #e9e6df; }
.ae-grid button.on { color: #f1eee7; background: linear-gradient(#34343a, #25252a); box-shadow: inset 0 1px 0 rgba(255,255,255,.1), 0 1px 2px rgba(0,0,0,.5), inset 0 -2px 0 var(--sc), 0 0 12px color-mix(in srgb, var(--sc) 16%, transparent); }
.ae-dice { width: 26px; height: 22px; flex: none; border-radius: 6px; border: 1px solid rgba(255,255,255,.1); background: none; color: #8d8d93; cursor: pointer; display: flex; align-items: center; justify-content: center; padding: 0; transition: color .12s, border-color .12s, transform .08s; }
.ae-dice:hover { color: var(--sc); border-color: color-mix(in srgb, var(--sc) 55%, transparent); }
.ae-dice:active { transform: rotate(-14deg) scale(.94); }
.ae-sub { font-size: 8.5px; color: #6f6f76; letter-spacing: .04em; white-space: nowrap; margin-left: 2px; }
.ae-hint { font-size: 9px; color: #6f6f76; letter-spacing: .03em; text-align: center; min-height: 12px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; flex: none; }
.ae-row { display: flex; align-items: center; gap: 8px; flex: none; }
.ae-notes { font-size: 9.5px; color: #a3a3a8; letter-spacing: .08em; font-variant-numeric: tabular-nums; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.ae-void { flex: 1; min-height: 0; }
.ae-bot .sy-wave { width: 96px; }
.ae-bot .sy-wave.l { right: calc(50% + 140px); } .ae-bot .sy-wave.r { left: calc(50% + 140px); }
.ae-sm { width: 26px; height: 26px; border-radius: 50%; border: 1px solid rgba(255,255,255,.12); background: none; color: #8d8d93; cursor: pointer; display: flex; align-items: center; justify-content: center; padding: 0; }
.ae-sm:hover { color: #e9e6df; border-color: rgba(255,255,255,.24); }
.ae-sm svg { width: 12px; height: 12px; }
.ae-summon .round { background: radial-gradient(circle at 38% 30%, #4a4a50, #1a1a1d 62%, #0c0c0d); }
.ae-summon:hover .round { color: var(--mc); box-shadow: 0 0 0 2px var(--mc), 0 0 0 3px #232326, 0 0 20px color-mix(in srgb, var(--mc) 70%, transparent), inset 0 1px 1px rgba(255,255,255,.25); }
.ae-summon.go .round { animation: aeSummon .7s ease-out; }
@keyframes aeSummon { 0% { box-shadow: 0 0 0 2px var(--mc), 0 0 0 3px #232326, 0 0 40px var(--mc); } 100% { box-shadow: 0 0 0 2px #8f8e89, 0 0 0 3px #232326, 0 6px 12px rgba(0,0,0,.8); } }
.ae-badge .pn { --pc: var(--a-void); }
`;
document.head.appendChild(css);

const ICA = {
  freeze: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2.5v19M3.8 7.25l16.4 9.5M20.2 7.25L3.8 16.75"/><path d="M9.6 4.3L12 6.3l2.4-2M9.6 19.7l2.4-2 2.4 2M5.1 10.1l2.9-.9-.6-3M18.9 13.9l-2.9.9.6 3M18.9 10.1L16 9.2l.6-3M5.1 13.9l2.9.9-.6 3"/></svg>',
  summon: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2.5l1.9 6.1L20 10.5l-6.1 1.9L12 18.5l-1.9-6.1L4 10.5l6.1-1.9z"/><path d="M19 17l.7 2.3L22 20l-2.3.7L19 23l-.7-2.3L16 20l2.3-.7z" stroke-width="1.6"/></svg>',
  mutate: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.1" stroke-linecap="round" stroke-linejoin="round"><path d="M3 9c2.5-3 5-3 7.5 0s5 3 7.5 0"/><path d="M3 15c2.5-3 5-3 7.5 0s5 3 7.5 0"/><path d="M18.5 5.5L21 9l-3.5 1.6M5.5 18.5L3 15l3.5-1.6"/></svg>',
  scenes: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.1" stroke-linecap="round" stroke-linejoin="round"><path d="M3 14c3-4 6 4 9 0s6 4 9 0"/><path d="M3 9c3-4 6 4 9 0s6 4 9 0" opacity=".55"/><path d="M3 19h18" opacity=".35"/></svg>',
};

/* ============================================================ the page */
/* ---- top strip ---- */
const strip = el('div', 'sy-top sy-aether'); dev.appendChild(strip);
const powerW = pushBtn(() => on('atm.on'), v => set('atm.on', v ? 1 : 0, 'ui'), 'POWER', K.PWR_IC, 'ON', 'OFF', 'AETHER on / off (it plays by itself, with or without SIREN)');
const freezeW = pushBtn(() => on('atm.freeze'), v => set('atm.freeze', v ? 1 : 0, 'ui'), 'FREEZE', ICA.freeze, 'HELD', 'OFF', 'Holds what is in the space, forever, and lets nothing new in');
const disp = el('div', 'sy-led ae-disp', '<span class="k"></span><span class="tn"></span><div class="rows">' +
  ['NOISE', 'DRONE', 'EVENT'].map((l, i) => '<div class="r" style="--lc:' + ['var(--a-bed)', 'var(--a-drn)', 'var(--a-tex)'][i] + '"><b>' + l + '</b><u></u><i><em></em></i></div>').join('') + '</div>');
const dispW = el('div', 'sy-hw'); dispW.appendChild(disp);
K.stripGroups(strip, [powerW,
  mknob('atm.evolve', 'EVOLVE', { tip: 'Every layer slowly drifts by itself: nothing stays the same for long' }),
  mknob('atm.follow', 'FOLLOW', { tip: 'Left: it ducks while you play and comes back when you stop. Right: your playing makes it swell.' }),
  sel('ui.mood', 'MOOD', MOODS, { span: 200, tip: 'The kind of world SUMMON and the dice make: calm nature .. dream .. eerie .. dread .. horror. The SCENES are made by hand.' }),
  freezeW], dispW, [
  mknob('atm.width', 'WIDTH', { tip: 'How wide it spreads around you' }),
  mknob('atm.grit', 'GRIT', { tip: 'Dirt: saturation, and above halfway it rots (lower rate, fewer bits)' }),
  mknob('atm.tone', 'TONE', { tip: 'Darker or brighter, all of it' }),
  mknob('atm.level', 'LEVEL', { tip: 'How loud the atmosphere is under your playing' })]);
onChange(id => { if (id === 'atm.on') powerW._draw(); if (id === 'atm.freeze') freezeW._draw(); });
const moodSel = strip.querySelector('[data-param="ui.mood"]');
const moodLbl = moodSel && moodSel.parentElement.querySelector('.sy-eng'); if (moodLbl) moodLbl.textContent = 'SUMMON MOOD';
const drawMoodOpts = () => { if (!moodSel) return; [...moodSel.querySelectorAll('.opt')].forEach((o, i) => { o.style.color = i === mood() ? MOOD_COL[i] : ''; o.style.textShadow = i === mood() ? '0 0 8px ' + MOOD_COL[i] : ''; }); };

/* ---- the screen ---- */
const ascreen = el('div', 'sy-screen ae-screen sy-aether'); dev.appendChild(ascreen);
function grid(id, labels, cols, tight) {
  const g = el('div', 'ae-grid' + (tight ? ' tight' : '')); g.style.gridTemplateColumns = 'repeat(' + cols + ', minmax(0, 1fr))'; g.dataset.param = id;
  labels.forEach((l, i) => { const b = el('button', null, l); b.dataset.i = i; b.title = P[id].opts[i]; g.appendChild(b); });
  const draw = () => [...g.children].forEach((b, i) => b.classList.toggle('on', i === Math.round(S[id])));
  g.addEventListener('click', e => { const b = e.target.closest('button'); if (b) set(id, +b.dataset.i, 'ui'); });
  draw(); onChange(i => { if (i === id) draw(); }); return g;
}
function diceBtn(tip, fn) { const b = el('button', 'ae-dice', IC.dice); b.title = tip; b.setAttribute('aria-label', tip); b.addEventListener('click', fn); return b; }
function canvasBox(h) { const v = el('div', 'sy-vis'); if (h) v.style.height = h + 'px'; const c = el('canvas'); v.appendChild(c); v._cv = c; return v; }
/* knobs whose label (and value text) follow the selected type */
function typedKnobs(ids, labelsFor, fmtFor) {
  const row = el('div', 'sy-knobs');
  const ks = ids.map((id, i) => knob(id, { label: 'X', size: 34, fmt: (pid, v) => (fmtFor && fmtFor(i, v)) || fmt(pid, v) }));
  row.append(...ks);
  row._relabel = () => { const L = labelsFor(); ks.forEach((k, i) => { const l = k.querySelector('.sy-kl'), v = k.querySelector('.sy-kv'); if (l) l.textContent = L[i];
    if (v) v.textContent = (fmtFor && fmtFor(i, S[ids[i]])) || fmt(ids[i], S[ids[i]]); }); };
  return row;
}

/* BED */
const sub = t => el('span', 'ae-sub', t);
const cBed = section('NOISE', 300, 'var(--a-bed)', ascreen);
const bedDice = diceBtn('Roll new noise (follows the MOOD)', () => { keepUndo(); rollBed(); markScene(); flash(); });
cBed._h.append(sub('the air around you'), el('span', 'sy-sp'), bedDice, toggle('atm.bed.on', 'Noise on'));
const bedGrid = grid('atm.bed.type', BEDS, 5, true);
const bedVis = canvasBox(66);
const bedKnobs = typedKnobs(['atm.bed.tone', 'atm.bed.motion', 'atm.bed.depth', 'atm.bed.level'], () => [...BED_INFO[Math.round(S['atm.bed.type'])].slice(0, 3), 'LEVEL']);
const bedHint = el('div', 'ae-hint');
cBed._b.append(bedGrid, bedVis, bedKnobs, bedHint);

/* DRONE */
const cDrn = section('DRONE', 340, 'var(--a-drn)', ascreen);
const drnDice = diceBtn('Roll a new drone (follows the MOOD)', () => { keepUndo(); rollDrone(); markScene(); flash(); });
cDrn._h.append(sub('a chord that hangs'), el('span', 'sy-sp'), chip('atm.drn.follow', 'FOLLOW', 'The drone moves to the note you play (otherwise it stays on SIREN\'s key)'), drnDice, toggle('atm.drn.on', 'Drone on'));
const drnGrid = grid('atm.drn.voice', VOICES, 7, true);
const chordSeg = grid('atm.drn.chord', CHORDS, 8, true); chordSeg.classList.add('ae-chords');
chordSeg.title = 'The chord the drone holds: one note, fifths, majors, sevenths, ninths, wide open, or a dread cluster';
const octSeg = seg('atm.drn.oct', ['SUB', 'LOW', 'MID', 'HIGH'], { small: true, width: 150 });
const drnNotes = el('span', 'ae-notes');
const octRow = el('div', 'ae-row'); octRow.append(octSeg, drnNotes);
const drnVis = canvasBox(22);
const drnKnobs = typedKnobs(['atm.drn.detune', 'atm.drn.tone', 'atm.drn.motion', 'atm.drn.shine', 'atm.drn.air', 'atm.drn.level'],
  () => [...VOICE_INFO[Math.round(S['atm.drn.voice'])].slice(0, 3), 'SHINE', 'AIR', 'LEVEL']);
drnKnobs.classList.add('ae-six');
drnKnobs.children[3].title = 'SHINE: the chord an octave and two octaves up, softly glinting';
drnKnobs.children[4].title = 'AIR: breath around the drone';
cDrn._b.append(drnGrid, chordSeg, octRow, drnVis, drnKnobs);

/* TEXTURE */
const cTex = section('EVENTS', 320, 'var(--a-tex)', ascreen);
const texDice = diceBtn('Roll new events (follows the MOOD)', () => { keepUndo(); rollTexture(); markScene(); flash(); });
cTex._h.append(sub('sounds now and then'), el('span', 'sy-sp'), texDice, toggle('atm.tex.on', 'Events on'));
const texGrid = grid('atm.tex.type', TEXS, 6, true);
const texVis = canvasBox(66);
const texKnobs = typedKnobs(['atm.tex.density', 'atm.tex.tone', 'atm.tex.chaos', 'atm.tex.level'], () => [...TEX_INFO[Math.round(S['atm.tex.type'])].slice(0, 3), 'LEVEL'],
  (i, v) => i === 0 && Math.round(S['atm.tex.type']) === 6 ? Math.round(42 + v * 1.18) + ' BPM' : null);
const texHint = el('div', 'ae-hint');
cTex._b.append(texGrid, texVis, texKnobs, texHint);

/* VOID */
const cVoid = section('SPACE', 276, 'var(--a-void)', ascreen);
const voidMood = el('span', 'sy-lbl'); cVoid._h.append(el('span', 'sy-sp'), voidMood);
const voidVis = canvasBox(0); voidVis.classList.add('ae-void');
const voidKnobs = el('div', 'sy-knobs');
voidKnobs.append(knob('atm.size', { label: 'SIZE', size: 34, tip: 'From a room to an endless void (the tail it leaves)' }),
  knob('atm.space', { label: 'SPACE', size: 34, tip: 'How much of it is in the space' }),
  knob('atm.shimmer', { label: 'SHIMMER', size: 34, tip: 'The space rises an octave as it rings: a halo over everything (needs SPACE)' }));
cVoid._b.append(voidVis, voidKnobs);

const flashAe = () => { ascreen.classList.remove('sy-flash'); void ascreen.offsetWidth; ascreen.classList.add('sy-flash'); };
const flash = flashAe;

/* ---- bottom band: scenes, name badge, undo, reset, mutate, summon ---- */
const abot = el('div', 'sy-bot ae-bot sy-aether'); dev.appendChild(abot);
const scBtn = el('button', 'sy-btn sy-prbtn', '<span class="round">' + ICA.scenes + '</span><span class="tx"><b>SCENES</b><small class="sy-prname">Default</small></span>');
scBtn.title = 'Ready-made atmospheres';
const scNav = el('div', 'sy-prnav', '<button aria-label="Previous scene">' + IC.prev + '</button><button aria-label="Next scene">' + IC.next + '</button>');
const scWrap = el('div', 'sy-prwrap'); scWrap.append(scBtn, scNav);
const mist = '<svg viewBox="0 0 380 24" preserveAspectRatio="none"><path d="' + Array.from({ length: 77 }, (_, i) => (i ? 'L' : 'M') + (i * 5) + ' ' + (12 + (Math.sin(i * .21) * 5 + Math.sin(i * .67 + 1) * 3) * Math.pow(i / 76, 1.3)).toFixed(1)).join(' ') + '" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>';
const abadge = el('div', 'sy-badge ae-badge', '<b>AETHER</b><div class="pn" title="The loaded scene"><i></i><u>DEFAULT</u><em></em></div>');
const awl = el('div', 'sy-wave l', mist), awr = el('div', 'sy-wave r', mist);
const aRight = el('div'); aRight.style.cssText = 'display: flex; align-items: center; gap: 18px';
const aUndo = el('button', 'sy-undo', IC.undo); aUndo.title = 'Undo (dice, mutate, summon, reset or scene)'; aUndo.disabled = true;
const aReset = el('button', 'ae-sm', IC.reset); aReset.title = 'Every AETHER setting back to its default'; aReset.setAttribute('aria-label', 'Reset AETHER');
const mutate = el('button', 'sy-btn r', '<span class="round">' + ICA.mutate + '</span><span class="tx"><b>MUTATE</b><small>nudge it a little</small></span>');
mutate.title = 'Nudge everything a little (and now and then swap one sound)';
const summon = el('button', 'sy-btn r ae-summon', '<span class="round">' + ICA.summon + '</span><span class="tx"><b>SUMMON</b><small>a new world</small></span>');
summon.title = 'A whole new atmosphere, in the MOOD you picked';
aRight.append(aUndo, aReset, mutate, summon);
abot.append(scWrap, awl, abadge, awr, aRight);

/* ============================================================ state shown on the page */
function drawDisplay() {
  const rows = disp.querySelectorAll('.r');
  const names = [BEDS[Math.round(S['atm.bed.type'])], VOICES[Math.round(S['atm.drn.voice'])] + ' · ' + CHORDS[Math.round(S['atm.drn.chord'])], TEXS[Math.round(S['atm.tex.type'])]];
  ['atm.bed.on', 'atm.drn.on', 'atm.tex.on'].forEach((id, i) => { rows[i].classList.toggle('off', !on(id)); rows[i].querySelector('u').textContent = names[i]; });
  disp.querySelector('.k').textContent = on('atm.drn.follow') ? 'FOLLOWS YOU' : 'KEY ' + fmt('syn.key').toUpperCase();
  const tn = disp.querySelector('.tn'); tn.classList.toggle('fz', on('atm.freeze')); tn.textContent = on('atm.freeze') ? 'FROZEN' : 'TAIL ' + fmt('atm.size');
}
const ROOT_OCT = [24, 36, 48, 60];
function droneNotes() {
  const key = liveRoot != null && on('atm.drn.follow') ? ((liveRoot % 12) + 12) % 12 : Math.round(S['syn.key']);
  const root = ROOT_OCT[Math.round(S['atm.drn.oct'])] + key;
  return CHORD_INT[Math.round(S['atm.drn.chord'])].map(i => root + i);
}
const noteName = m => K.NOTE_NAMES[((m % 12) + 12) % 12] + (Math.floor(m / 12) - 1);
function drawDroneInfo() {
  const v = Math.round(S['atm.drn.voice']), shep = isShep(v);
  drnNotes.textContent = shep ? (v === 5 ? 'FALLING' : 'RISING') + ' · ' + droneNotes().map(m => K.NOTE_NAMES[((m % 12) + 12) % 12]).join(' ') : droneNotes().map(noteName).join(' · ');
  drawDroneVis();
}
function drawHints() {
  bedHint.textContent = BED_INFO[Math.round(S['atm.bed.type'])][3];
  texHint.textContent = TEX_INFO[Math.round(S['atm.tex.type'])][3];
  drnGrid.title = VOICE_INFO[Math.round(S['atm.drn.voice'])][3];
}
const dimSec = (sec, id) => { const d = () => sec.classList.toggle('sy-off', !on(id)); d(); onChange(i => { if (i === id) d(); }); };
dimSec(cBed, 'atm.bed.on'); dimSec(cDrn, 'atm.drn.on'); dimSec(cTex, 'atm.tex.on');
/* AETHER plays only with its POWER on and the SYNTH tab on (double-clicking the tab bypasses both) */
const alive = () => on('atm.on') && K.isPowered();
const drawPower = () => dev.classList.toggle('aoff', !alive());
onChange(id => {
  if (id === 'atm.on') drawPower();
  if (/^atm\.(bed|drn|tex)\.(type|voice|chord|on)$/.test(id) || id === 'atm.freeze' || id === 'atm.size' || id === 'atm.drn.follow' || id === 'syn.key') drawDisplay();
  if (id === 'atm.bed.type') { bedKnobs._relabel(); drawHints(); }
  if (id === 'atm.drn.voice') { drnKnobs._relabel(); drawHints(); }
  if (id === 'atm.tex.type') { texKnobs._relabel(); drawHints(); }
  if (/^atm\.drn\.(voice|chord|oct|follow)$/.test(id) || id === 'syn.key') drawDroneInfo();
  if (id === 'ui.mood') { drawMoodOpts(); drawMood(); }
});

/* ============================================================ scenes */
const SCENES = window.OBS_AETHER_SCENES || [];
const SC_CATS = [['ALL', 'var(--accent)'], ['NATURE', '#7fd6a0'], ['DREAM', '#9bb4ff'], ['DARK', '#b07cff'], ['HORROR', '#c41a24']];
const scCol = c => (SC_CATS.find(x => x[0] === c) || SC_CATS[0])[1];
let scName = 'Default', scIdx = -1, scCat = '';
const isAtm = id => id.startsWith('atm.');
/** Sets the whole atmosphere: defaults, then the scene's own values ("a": ids without "atm.", choices by name). Turns AETHER on. */
function applyScene(a, keepLevel) {
  const want = {};
  for (const k in a) { const id = 'atm.' + k, v = K.presetValue(id, a[k]); if (v !== undefined) want[id] = v; }
  for (const id in P) if (isAtm(id) && id !== 'atm.on' && !(keepLevel && id === 'atm.level')) set(id, id in want ? want[id] : P[id].def, 'preset');
  set('atm.on', 'on' in a ? (a.on ? 1 : 0) : 1, 'preset');
}
const scNameEl = scBtn.querySelector('.sy-prname');
function drawScName() {
  scNameEl.textContent = scName; scNameEl.title = scName;
  const pn = abadge.querySelector('.pn'), col = scName === 'Custom' ? '#8d8d93' : scCat ? scCol(scCat) : 'var(--a-void)';
  pn.style.setProperty('--pc', col); pn.querySelector('u').textContent = scName;
  pn.querySelector('em').textContent = scName === 'Custom' ? 'FROM THE MAIN PRESET' : scCat;
  pn.classList.toggle('hor', scCat === 'HORROR' && scName !== 'Custom');
}
function showScene(name, i, cat) { scName = name; scIdx = i; scCat = cat || ''; drawScName(); markSc(); }
const markScene = () => showScene(lastRollName(), -1, '');
let undoA = null;
function keepUndo() { undoA = { S: Object.fromEntries(Object.keys(P).filter(isAtm).map(id => [id, S[id]])), name: scName, idx: scIdx, cat: scCat }; aUndo.disabled = false; }
aUndo.addEventListener('click', () => { if (!undoA) return; const u = undoA; for (const id in u.S) set(id, u.S[id], 'dice'); showScene(u.name, u.idx, u.cat); undoA = null; aUndo.disabled = true; flash(); });
const CAT_MOOD = { NATURE: 0, DREAM: 1, DARK: 2, HORROR: 4 };
function loadScene(i) {
  const sc = SCENES[i]; if (!sc) return;
  keepUndo(); applyScene(sc.a); showScene(sc.name, i, sc.cat); flash();
  if (sc.cat in CAT_MOOD) set('ui.mood', CAT_MOOD[sc.cat], 'preset');      // the dice carry on in the same spirit
}
function resetAether() {
  keepUndo();
  for (const id in P) if (isAtm(id) && id !== 'atm.on') set(id, P[id].def, 'preset');
  showScene('Default', -1, ''); flash();
}
aReset.addEventListener('click', resetAether);
// a knob you turn marks the scene as edited (*); a change from the plugin (rig preset, automation) makes it "Custom"
onChange((id, v, from) => { if (!isAtm(id) || id === 'atm.on') return;
  if (from === 'ui' && !/\*$/.test(scName)) { scName += ' *'; drawScName(); }
  else if (from === 'plugin' && scName !== 'Custom') { scName = 'Custom'; scIdx = -1; scCat = ''; drawScName(); markSc(); } });

/* the browser */
let scFilter = 'ALL';
const scs = el('div', 'sy-prs sy-aether');
scs.innerHTML = '<div class="sy-prs-h"><b>AETHER SCENES</b><span>' + SCENES.length + ' atmospheres · click one, the window stays open</span><button class="x" aria-label="Close">✕</button></div><div class="sy-prs-b"><div class="sy-prs-cats"></div><div class="sy-prs-list"></div></div>';
dev.appendChild(scs);
const scCats = scs.querySelector('.sy-prs-cats'), scList = scs.querySelector('.sy-prs-list');
scCats.innerHTML = SC_CATS.map(([c, col]) => '<button class="' + K.horCls(c).trim() + '" data-c="' + c + '" style="--cc:' + col + '"><i></i>' + c + '<em>' + (c === 'ALL' ? SCENES.length : SCENES.filter(s => s.cat === c).length) + '</em></button>').join('');
function markSc() { [...scList.children].forEach(t => t.classList.toggle('on', +t.dataset.i === scIdx)); }
function drawScList() {
  [...scCats.children].forEach(b => b.classList.toggle('on', b.dataset.c === scFilter));
  scList.innerHTML = SCENES.map((s, i) => [s, i]).filter(([s]) => scFilter === 'ALL' || s.cat === scFilter)
    .map(([s, i]) => '<button class="sy-pr' + K.horCls(s.cat) + (i === scIdx ? ' on' : '') + '" data-i="' + i + '" style="--cc:' + scCol(s.cat) + '" title="' + s.note.replace(/"/g, '&quot;') + '"><b>' + s.name + '</b><small>' + s.note + '</small></button>').join('');
}
scCats.addEventListener('click', e => { const b = e.target.closest('button'); if (!b) return; scFilter = b.dataset.c; drawScList(); scList.scrollTop = 0; });
scList.addEventListener('click', e => { const b = e.target.closest('.sy-pr'); if (b) loadScene(+b.dataset.i); });
const scOpen = open => { scs.classList.toggle('show', open); scBtn.classList.toggle('on', open); if (open) { drawScList(); const o = scList.querySelector('.on'); if (o) o.scrollIntoView({ block: 'nearest' }); } };
scBtn.addEventListener('click', e => { e.stopPropagation(); scOpen(!scs.classList.contains('show')); });
scs.querySelector('.x').addEventListener('click', () => scOpen(false));
document.addEventListener('pointerdown', e => { if (scs.classList.contains('show') && !scs.contains(e.target) && !scWrap.contains(e.target)) scOpen(false); });
const stepScene = d => { if (!SCENES.length) return; const list = SCENES.map((s, i) => i).filter(i => scFilter === 'ALL' || SCENES[i].cat === scFilter);
  const at = list.indexOf(scIdx); loadScene(list[at < 0 ? (d > 0 ? 0 : list.length - 1) : (at + d + list.length) % list.length]); };
scNav.children[0].addEventListener('click', () => stepScene(-1));
scNav.children[1].addEventListener('click', () => stepScene(1));

/* ============================================================ dice, mutate, summon (they follow the MOOD) */
const rr = (a, b) => a + Math.random() * (b - a);
const rng = r => rr(r[0], r[1]);
const wpick = ws => { let t = Math.random() * ws.reduce((x, y) => x + y, 0); for (let i = 0; i < ws.length; i++) { t -= ws[i]; if (t <= 0) return i; } return ws.length - 1; };
//                weights per MOOD:  CALM DREAM EERIE DREAD HORROR
const W = {
  //      Hiss          Wind        Rain            Surf            Rumble           Static          Breath
  bed:   [[1.5, 2, 1, .3, .2], [2, 2, 3, 2, 1], [3, 1, .6, .3, .2], [3, 2, .3, .2, 0], [.4, .3, 1, 3, 2.5], [.2, .4, 2, 1.2, 2], [0, .2, 1.5, 2, 3],
  //      Brook         Night             Forest          Soft rain         Undersea         Air
          [3, 1.2, .3, 0, 0], [2.5, 2, 1, .3, .1], [3, 1, .3, 0, 0], [2.5, 2.2, .5, .2, 0], [.8, 2.5, 1, .6, .2], [1.5, 3, .6, .2, 0]],
  //      Organ         Strings          Choir          Glass           Abyss            Descent          Ascent
  voice: [[3, 1, .5, .3, .3], [3, 2, 1.5, 1, 1], [1, 2.5, 3, 2, 3], [2, 3, 2, .5, .3], [.2, .2, 1, 3, 3], [0, .3, 2, 3, 2.5], [.3, 1.5, .5, .3, .2],
  //      Pipe organ      Pad             Angels           Tanpura          Bowls             Harmonium        Flute
          [1, 2, 1, 1.2, .5], [2, 3, .5, .2, 0], [1.5, 3.5, .4, .1, 0], [2.5, 2, .3, 0, 0], [2.5, 2.5, .6, .2, 0], [2, 1.5, .4, .2, 0], [2.5, 2, .3, 0, 0]],
  //      Root        5th          Oct             Min               Sus            Tri              Clus            Dread
  chord: [[1, 1, 1, .5, .3], [3, 2, 1, .6, .2], [2.5, 2, .8, .6, .3], [.5, .6, 3, 1.5, .6], [2, 3, .5, .2, 0], [0, .1, 2.5, 3, 2], [0, .2, 1, 2.5, 3], [0, 0, .4, 3, 4],
  //      Maj           Maj7          Add9           Sus2              Min7            Min9              Heaven          Open
          [2.5, 1.5, .2, 0, 0], [2, 3, .3, 0, 0], [2, 2.5, .3, 0, 0], [2, 2.5, .6, .2, 0], [1.5, 2, 1, .3, 0], [1.2, 2.5, 1, .3, 0], [1, 3, .2, 0, 0], [2, 2, .8, .5, .2]],
  oct:   [[.3, .2, .6, 2, 2], [2, 1.5, 2, 2, 2], [2, 2, 1.5, .6, .6], [.5, 1.5, .6, .2, .2]],
  //      Vinyl         Fire           Drips         Metal           Shimmer        Whispers          Heart           Knocks          Moans            Flesh
  tex:   [[2, .7, .6, .2, .1], [3, .5, .2, .3, .1], [2, 2, 2, .8, .4], [0, .2, 2, 2.5, 2], [2, 4, 1, 0, 0], [0, .6, 3, 1.5, 2.5], [0, 0, .6, 3, 2], [0, 0, 1.5, 2, 1], [0, 0, .5, 2, 3.5], [0, 0, 0, .6, 3],
  //      Chimes        Birds          Music box         Bells               Harp             Stars           Kalimba          Leaves
          [3, 2, .4, 0, 0], [3, .6, .2, 0, 0], [1, 2.5, 1.2, .4, .5], [1, 1.5, 1.5, 1.2, .5], [2, 2.5, .2, 0, 0], [1.5, 3.5, .5, 0, 0], [2.5, 1.5, .2, 0, 0], [3, .8, .3, 0, 0]],
};
//                ranges per MOOD:   CALM       DREAM      EERIE      DREAD       HORROR
const R = {
  grit:      [[0, 8], [0, 6], [0, 25], [8, 40], [20, 65]],
  tone:      [[-15, 25], [0, 40], [-45, 10], [-75, -15], [-85, -25]],
  evolve:    [[20, 50], [45, 85], [35, 75], [30, 70], [45, 90]],
  space:     [[25, 55], [55, 90], [40, 80], [35, 75], [30, 70]],
  size:      [[30, 65], [65, 95], [50, 90], [45, 85], [40, 85]],
  follow:    [[-60, 0], [-30, 25], [-40, 40], [-10, 60], [10, 80]],
  width:     [[60, 100], [80, 100], [70, 100], [60, 100], [70, 100]],
  bedTone:   [[30, 70], [35, 75], [20, 65], [10, 50], [10, 60]],
  bedMotion: [[15, 50], [10, 45], [20, 60], [20, 60], [30, 85]],
  bedDepth:  [[20, 55], [25, 60], [35, 75], [45, 85], [55, 100]],
  bedLevel:  [[50, 75], [35, 60], [45, 70], [45, 70], [45, 70]],
  detune:    [[5, 30], [10, 40], [20, 60], [30, 80], [45, 100]],
  drnTone:   [[25, 60], [35, 70], [20, 55], [10, 45], [10, 55]],
  drnMotion: [[15, 45], [30, 70], [25, 65], [20, 60], [30, 85]],
  drnLevel:  [[25, 50], [40, 65], [30, 55], [35, 60], [35, 60]],
  density:   [[20, 55], [25, 60], [20, 60], [25, 65], [30, 75]],
  texTone:   [[25, 70], [45, 90], [20, 70], [10, 55], [15, 70]],
  chaos:     [[10, 45], [20, 55], [35, 75], [40, 80], [55, 100]],
  texLevel:  [[40, 70], [35, 60], [45, 70], [50, 75], [50, 75]],
  shimmer:   [[0, 20], [15, 55], [0, 20], [0, 10], [0, 5]],
  shine:     [[0, 30], [15, 60], [0, 25], [0, 10], [0, 10]],
  air:       [[0, 30], [10, 45], [0, 35], [0, 25], [0, 30]],
};
const ON = { bed: [.95, .9, .9, .9, .9], drn: [.7, .9, .8, .9, .9], tex: [.7, .75, .85, .9, .95] };
const col = (tbl, m) => tbl.map(w => w[m]);
let rollName = 'Summoned';
const lastRollName = () => rollName;
function rollBed(m = mood(), sw = true) {
  set('atm.bed.type', wpick(col(W.bed, m)), 'dice'); set('atm.bed.tone', rng(R.bedTone[m]), 'dice'); set('atm.bed.motion', rng(R.bedMotion[m]), 'dice');
  set('atm.bed.depth', rng(R.bedDepth[m]), 'dice'); set('atm.bed.level', rng(R.bedLevel[m]), 'dice'); if (sw) set('atm.bed.on', 1, 'dice');
  rollName = BEDS[Math.round(S['atm.bed.type'])][0] + BEDS[Math.round(S['atm.bed.type'])].slice(1).toLowerCase() + ' (rolled)';
}
function rollDrone(m = mood(), sw = true) {
  set('atm.drn.voice', wpick(col(W.voice, m)), 'dice'); set('atm.drn.chord', wpick(col(W.chord, m)), 'dice'); set('atm.drn.oct', wpick(col(W.oct, m)), 'dice');
  set('atm.drn.detune', rng(R.detune[m]), 'dice'); set('atm.drn.tone', rng(R.drnTone[m]), 'dice'); set('atm.drn.motion', rng(R.drnMotion[m]), 'dice');
  set('atm.drn.level', rng(R.drnLevel[m]), 'dice'); if (sw) set('atm.drn.on', 1, 'dice');
  set('atm.drn.shine', Math.random() < (m <= 1 ? .6 : .25) ? rng(R.shine[m]) : 0, 'dice'); set('atm.drn.air', Math.random() < .5 ? rng(R.air[m]) : 0, 'dice');
  rollName = VOICES[Math.round(S['atm.drn.voice'])][0] + VOICES[Math.round(S['atm.drn.voice'])].slice(1).toLowerCase() + ' (rolled)';
}
function rollTexture(m = mood(), sw = true) {
  set('atm.tex.type', wpick(col(W.tex, m)), 'dice'); set('atm.tex.density', rng(R.density[m]), 'dice'); set('atm.tex.tone', rng(R.texTone[m]), 'dice');
  set('atm.tex.chaos', rng(R.chaos[m]), 'dice'); set('atm.tex.level', rng(R.texLevel[m]), 'dice'); if (sw) set('atm.tex.on', 1, 'dice');
  rollName = TEXS[Math.round(S['atm.tex.type'])][0] + TEXS[Math.round(S['atm.tex.type'])].slice(1).toLowerCase() + ' (rolled)';
}
/* scene names for a summoned world: two words, in the mood */
const NAME_A = [['Quiet', 'Soft', 'Golden', 'Still', 'Morning', 'Low'], ['Drifting', 'Silver', 'Endless', 'Floating', 'Distant', 'Velvet'],
  ['Hollow', 'Pale', 'Forgotten', 'Empty', 'Grey', 'Lost'], ['Sunken', 'Black', 'Rotting', 'Buried', 'Cold', 'Starless'], ['Flayed', 'Screaming', 'Bleeding', 'Severed', 'Choking', 'Weeping']];
const NAME_B = [['Meadow', 'Harbour', 'Hearth', 'Valley', 'Shore', 'Rain'], ['Nebula', 'Lagoon', 'Halo', 'Garden', 'Orbit', 'Mirage'],
  ['Hallway', 'Marsh', 'Chapel', 'Lighthouse', 'Ward', 'Attic'], ['Catacomb', 'Mineshaft', 'Abyss', 'Crypt', 'Well', 'Trench'], ['Butcher', 'Altar', 'Cellar', 'Saint', 'Wound', 'Choir']];
const pick = a => a[Math.floor(Math.random() * a.length)];
function summonWorld() {
  keepUndo();
  const m = mood();
  set('atm.on', 1, 'dice');
  const bedOn = Math.random() < ON.bed[m], drnOn = Math.random() < ON.drn[m];
  let texOn = Math.random() < ON.tex[m]; if (!bedOn && !drnOn) texOn = true;
  rollBed(m, false); rollDrone(m, false); rollTexture(m, false);
  set('atm.bed.on', bedOn ? 1 : 0, 'dice'); set('atm.drn.on', drnOn || (!bedOn && !texOn) ? 1 : 0, 'dice'); set('atm.tex.on', texOn ? 1 : 0, 'dice');
  set('atm.drn.follow', Math.random() < (m >= 3 ? .3 : .15) ? 1 : 0, 'dice');
  set('atm.evolve', rng(R.evolve[m]), 'dice'); set('atm.grit', rng(R.grit[m]), 'dice'); set('atm.tone', rng(R.tone[m]), 'dice');
  set('atm.space', rng(R.space[m]), 'dice'); set('atm.size', rng(R.size[m]), 'dice'); set('atm.follow', rng(R.follow[m]), 'dice'); set('atm.width', rng(R.width[m]), 'dice');
  set('atm.shimmer', Math.random() < (m <= 1 ? .55 : .15) ? rng(R.shimmer[m]) : 0, 'dice');
  set('atm.freeze', 0, 'dice');
  rollName = pick(NAME_A[m]) + ' ' + pick(NAME_B[m]);
  showScene(rollName, -1, ''); flash();
  summon.classList.remove('go'); void summon.offsetWidth; summon.classList.add('go');
}
summon.addEventListener('click', summonWorld);
const CONT = ['atm.evolve', 'atm.grit', 'atm.tone', 'atm.width', 'atm.space', 'atm.size', 'atm.follow', 'atm.bed.tone', 'atm.bed.motion', 'atm.bed.depth', 'atm.bed.level',
  'atm.drn.detune', 'atm.drn.tone', 'atm.drn.motion', 'atm.drn.level', 'atm.tex.density', 'atm.tex.tone', 'atm.tex.chaos', 'atm.tex.level',
  'atm.shimmer', 'atm.drn.shine', 'atm.drn.air'];
function mutateWorld() {
  keepUndo();
  const m = mood();
  CONT.forEach(id => { const p = P[id], span = p.max - p.min, lvl = /level$/.test(id); set(id, clamp(S[id] + span * rr(-1, 1) * (lvl ? .06 : .12), p.min, p.max), 'dice'); });
  if (Math.random() < .3) { const r = Math.random();
    if (r < .34) set('atm.bed.type', wpick(col(W.bed, m)), 'dice'); else if (r < .67) set('atm.drn.chord', wpick(col(W.chord, m)), 'dice'); else set('atm.tex.type', wpick(col(W.tex, m)), 'dice'); }
  if (!/ \(mutated\)$/.test(scName)) showScene(scName.replace(/ \*$/, '') + ' (mutated)', -1, scCat); else drawScName();
  flash();
}
mutate.addEventListener('click', mutateWorld);

/* ============================================================ drawings */
let act = [0, 0, 0], actSm = [0, 0, 0], evCount = 0, lastEv = 0, liveRoot = null, sparks = [], ticks = [];
const HUES = {}, hueOf = name => HUES[name] || (HUES[name] = getComputedStyle(dev).getPropertyValue('--a-' + name).trim() || '#ffffff');
const rgba = (hex, a) => { if (hex[0] !== '#') return hex; const h = hex.length === 4 ? '#' + hex[1] + hex[1] + hex[2] + hex[2] + hex[3] + hex[3] : hex; return 'rgba(' + [1, 3, 5].map(i => parseInt(h.substr(i, 2), 16)).join(',') + ',' + a + ')'; };
/* BED: where its energy sits (a spectrum shape per type), alive with the bed's level */
function bedShape(t, x) {           // x 0..1 = 20 Hz .. 20 kHz (log)
  const tone = S['atm.bed.tone'] / 100, d = S['atm.bed.depth'] / 100, bump = (c, w) => Math.exp(-Math.pow((x - c) / w, 2));
  switch (t) {
    case 0: return Math.pow(1 - x, 1.4 - tone * 1.3) * (.4 + .6 * bump(.3 + tone * .5, .5));
    case 1: return bump(.35 + tone * .35, .14 + .1 * (1 - d)) + .5 * d * d * bump(.52 + tone * .35, .04);
    case 2: return .5 * bump(.62 + tone * .2, .28) + .5 * bump(.8, .1);
    case 3: return bump(.3 + tone * .2, .25) * .9 + .2 * bump(.7, .2);
    case 4: return bump(.08 + tone * .12, .1) + .6 * bump(.04, .03);
    case 5: return bump(.55 + tone * .2, .2) * .8 + .5 * d * bump(.6 + .1 * Math.sin(performance.now() / 900), .015);
    case 6: return bump(.42 + tone * .1, .05) * .9 + .7 * bump(.58 + tone * .1, .05) + .2 * bump(.6, .3);
    case 7: return bump(.52 + tone * .15, .18) * .9 + .45 * bump(.68 + tone * .12, .1);
    case 8: return .3 * bump(.25, .12) + bump(.76 + tone * .08, .025) * (.6 + .4 * d);
    case 9: return .35 * bump(.3, .1) + bump(.72 + tone * .1, .17);
    case 10: return .6 * bump(.52 + tone * .15, .25) + .4 * bump(.78, .07);
    case 11: return bump(.12 + tone * .14, .1) + .3 * bump(.3, .07);
    default: return bump(.7 + tone * .14, .15);
  }
}
function drawBedVis(tNow) {
  const [g, w, h] = hidpi(bedVis._cv); if (!g) return;
  const t = Math.round(S['atm.bed.type']), live = alive() && on('atm.bed.on'), a = live ? .25 + .75 * Math.min(1, actSm[0] * 1.6) : .12, c = hueOf('bed');
  const n = 56, bw = w / n;
  for (let i = 0; i < n; i++) {
    const x = (i + .5) / n, base = bedShape(t, x), jit = live ? .65 + .35 * Math.sin(tNow / (90 + i * 7) + i * 1.7) * Math.sin(tNow / 333 + i) : 1;
    const bh = Math.max(1, base * jit * (h - 10) * a);
    const gr = g.createLinearGradient(0, h - bh, 0, h); gr.addColorStop(0, rgba(c, live ? .95 : .35)); gr.addColorStop(1, rgba(c, .08));
    g.fillStyle = gr; g.fillRect(i * bw + .5, h - bh, bw - 1.5, bh);
  }
  g.fillStyle = tc('e9e6df', .3); g.font = '600 8px ' + FONT(); g.fillText(BEDS[t], 8, 13);
}
/* DRONE: the notes it holds, on a keyboard of two octaves (lit keys), or falling / rising partials */
function drawDroneVis() {
  const [g, w, h] = hidpi(drnVis._cv); if (!g) return;
  const v = Math.round(S['atm.drn.voice']), c = hueOf('drn'), live = alive() && on('atm.drn.on');
  if (isShep(v)) {                    // Shepard: octave partials under a bell, gliding
    const t = performance.now() / 1000, sp = (.02 + S['atm.drn.motion'] / 100 * .2) * (v === 5 ? -1 : 1), pos = ((t * sp) % 1 + 1) % 1;
    for (let k = 0; k < 9; k++) { const y = (k + pos) / 9, x = y * w, a = Math.sin(Math.PI * y);
      g.strokeStyle = rgba(c, (live ? .85 : .3) * a); g.lineWidth = 2; g.beginPath(); g.moveTo(x, h * (1 - .8 * a)); g.lineTo(x, h); g.stroke(); }
    return;
  }
  const notes = droneNotes(), lo = Math.floor(Math.min(...notes) / 12) * 12;
  const keys = Math.max(24, Math.ceil((Math.max(...notes) - lo + 1) / 12) * 12), kw = w / (keys / 12 * 7);   // wide chords get a third octave
  const isBlack = n => [1, 3, 6, 8, 10].includes(((n % 12) + 12) % 12);
  let xi = 0; const pos = [];
  for (let i = 0; i < keys; i++) { const n = lo + i; if (!isBlack(n)) { pos[i] = xi * kw; xi++; } else pos[i] = xi * kw - kw * .32; }
  for (let i = 0; i < keys; i++) { const n = lo + i; if (isBlack(n)) continue; const lit = notes.includes(n);
    g.fillStyle = lit ? rgba(c, live ? .9 : .4) : tc('1b1b1f', 1, 'b'); g.fillRect(pos[i] + .5, 0, kw - 1.5, h);
    if (lit && live) { g.shadowColor = c; g.shadowBlur = 10; g.fillRect(pos[i] + .5, h - 3, kw - 1.5, 3); g.shadowBlur = 0; } }
  for (let i = 0; i < keys; i++) { const n = lo + i; if (!isBlack(n)) continue; const lit = notes.includes(n);
    g.fillStyle = lit ? rgba(c, live ? 1 : .45) : tc('0a0a0c', 1, 'b'); g.fillRect(pos[i], 0, kw * .64, h * .6); }
}
/* TEXTURE: a seismograph, each event a tick that scrolls away */
function drawTexVis(tNow) {
  const [g, w, h] = hidpi(texVis._cv); if (!g) return;
  const c = hueOf('tex'), live = alive() && on('atm.tex.on');
  g.strokeStyle = tc('e9e6df', .05); g.beginPath(); g.moveTo(0, h / 2 + .5); g.lineTo(w, h / 2 + .5); g.stroke();
  const span = 6000;
  ticks = ticks.filter(k => tNow - k.t < span);
  ticks.forEach(k => { const x = w - (tNow - k.t) / span * w, a = 1 - (tNow - k.t) / span, hh = k.a * (h / 2 - 6);
    g.strokeStyle = rgba(c, a * (live ? .95 : .35)); g.lineWidth = 2; g.beginPath(); g.moveTo(x, h / 2 - hh); g.lineTo(x, h / 2 + hh); g.stroke(); });
  if (live) { const e = Math.min(1, actSm[2] * 1.4); g.strokeStyle = rgba(c, .5); g.lineWidth = 1.2; g.beginPath();
    for (let x = 0; x <= w; x += 3) { const y = h / 2 + Math.sin(x * .3 + tNow / 60) * e * 4 * Math.sin(x / w * Math.PI); x ? g.lineTo(x, y) : g.moveTo(x, y); } g.stroke(); }
  g.fillStyle = tc('e9e6df', .3); g.font = '600 8px ' + FONT(); g.fillText(TEXS[Math.round(S['atm.tex.type'])], 8, 13);
}
/* VOID: fog for the bed, rings for the drone, sparks for the texture, in a space as big as SIZE */
const fog = Array.from({ length: 70 }, () => ({ x: Math.random(), y: Math.random(), r: rr(.04, .14), vx: rr(-1, 1), vy: rr(-1, 1), ph: Math.random() * 7 }));
function drawMood() { voidMood.textContent = MOODS[mood()]; voidMood.style.color = MOOD_COL[mood()]; summon.style.setProperty('--mc', MOOD_COL[mood()]); }
function drawVoid(tNow, dt) {
  const [g, w, h] = hidpi(voidVis._cv); if (!g) return;
  const live = alive(), frz = on('atm.freeze'), mc = MOOD_COL[mood()], size = S['atm.size'] / 100, wid = S['atm.width'] / 100;
  const cx = w / 2, cy = h / 2, R0 = Math.min(w, h) * (.28 + .2 * size);
  const bg = g.createRadialGradient(cx, cy, 4, cx, cy, Math.max(w, h) * .7);
  bg.addColorStop(0, rgba(mc, live ? .16 : .05)); bg.addColorStop(.5, rgba(mc, live ? .05 : .02)); bg.addColorStop(1, 'transparent');
  g.fillStyle = bg; g.fillRect(0, 0, w, h);
  // the edge of the space
  g.strokeStyle = rgba(frz ? '#bfe9ff' : mc, live ? .22 + .2 * S['atm.space'] / 100 : .08); g.setLineDash([2, 5]); g.lineWidth = 1;
  g.beginPath(); g.ellipse(cx, cy, R0 * (.7 + .3 * wid), R0 * .82, 0, 0, 7); g.stroke(); g.setLineDash([]);
  // fog (BED)
  const bedA = live && on('atm.bed.on') ? Math.min(1, actSm[0] * 1.6) : 0, bc = hueOf('bed');
  fog.forEach(f => {
    if (!frz && live) { const sp = .006 + S['atm.bed.motion'] / 100 * .03; f.x += f.vx * sp * dt; f.y += f.vy * sp * dt * .6; f.ph += dt * .8; }
    if (f.x < -.1) f.x = 1.1; if (f.x > 1.1) f.x = -.1; if (f.y < -.1) f.y = 1.1; if (f.y > 1.1) f.y = -.1;
    const x = cx + (f.x - .5) * w * (.5 + .5 * wid), y = f.y * h, r = f.r * Math.min(w, h) * (1 + .3 * Math.sin(f.ph));
    const a = bedA * .16 * (.6 + .4 * Math.sin(f.ph * .7)); if (a < .003) return;
    const gr = g.createRadialGradient(x, y, 0, x, y, r); gr.addColorStop(0, rgba(frz ? '#cfefff' : bc, a)); gr.addColorStop(1, rgba(bc, 0));
    g.fillStyle = gr; g.fillRect(x - r, y - r, r * 2, r * 2); });
  // rings (DRONE)
  const drnA = live && on('atm.drn.on') ? Math.min(1, actSm[1] * 1.8) : 0, dc = hueOf('drn'), notes = CHORD_INT[Math.round(S['atm.drn.chord'])], v = Math.round(S['atm.drn.voice']);
  const t = tNow / 1000, sh = isShep(v), shep = sh ? (((t * (.02 + S['atm.drn.motion'] / 100 * .2) * (v === 5 ? -1 : 1)) % 1) + 1) % 1 : 0;
  notes.forEach((iv, k) => {
    for (let o = 0; o < (sh ? 3 : 1); o++) {
      const rr0 = sh ? ((o + shep) / 3) : (.45 + .55 * ((iv % 12) / 12) * .9 + k * .06);
      const rad = R0 * rr0 * (1 + .04 * Math.sin(t * (1 + k * .3) + k) * (S['atm.drn.motion'] / 100)), a = drnA * (sh ? Math.sin(Math.PI * rr0) : 1) * .7;
      if (a < .01) continue;
      g.strokeStyle = rgba(dc, a); g.lineWidth = 1.4 + S['atm.drn.detune'] / 100 * 2; g.shadowColor = dc; g.shadowBlur = 8 * drnA;
      g.beginPath(); g.ellipse(cx, cy, rad * (.75 + .25 * wid), rad * .8, frz ? 0 : t * .05 * (k % 2 ? 1 : -1), 0, 7); g.stroke(); g.shadowBlur = 0; }
  });
  // sparks (TEXTURE)
  const tcol = hueOf('tex');
  sparks = sparks.filter(s => tNow - s.t < s.life);
  sparks.forEach(s => { const age = (tNow - s.t) / s.life, a = (1 - age) * (1 - age), r = 2 + s.big * 10 * age;
    const x = cx + s.x * w * .42 * (.4 + .6 * wid), y = cy + s.y * h * .4;
    g.fillStyle = rgba(tcol, a * .9); g.shadowColor = tcol; g.shadowBlur = 12 * a; g.beginPath(); g.arc(x, y, Math.max(1, 3 * (1 - age) + 1), 0, 7); g.fill(); g.shadowBlur = 0;
    g.strokeStyle = rgba(tcol, a * .45); g.lineWidth = 1; g.beginPath(); g.arc(x, y, r, 0, 7); g.stroke(); });
  // the heart of it
  const core = live ? .25 + .5 * Math.min(1, (actSm[0] + actSm[1] + actSm[2]) * .8) : .06;
  const cg = g.createRadialGradient(cx, cy, 0, cx, cy, 16 + 10 * core); cg.addColorStop(0, frz ? rgba('#e6f7ff', core) : tc('ffffff', core)); cg.addColorStop(.3, rgba(mc, core * .5)); cg.addColorStop(1, rgba(mc, 0));
  g.fillStyle = cg; g.beginPath(); g.arc(cx, cy, 30, 0, 7); g.fill();
  if (frz) { g.fillStyle = 'rgba(191,233,255,.85)'; g.font = '600 8.5px ' + FONT(); g.textAlign = 'center'; g.fillText('FROZEN', cx, h - 10); g.textAlign = 'left'; }
  if (!live) { g.fillStyle = tc('e9e6df', .35); g.font = '600 8.5px ' + FONT(); g.textAlign = 'center'; g.fillText(on('atm.on') ? 'THE SYNTH TAB IS OFF' : 'PRESS POWER', cx, h - 10); g.textAlign = 'left'; }
}
function newEvent(level) {
  const a = clamp(level == null ? rr(.3, 1) : level, .1, 1), now = performance.now();
  ticks.push({ t: now, a });
  if (sparks.length < 40) sparks.push({ t: now, life: rr(700, 1600), x: rr(-1, 1), y: rr(-.8, .8), big: a });
}
/* the display's meters */
function drawMeters() {
  const ems = disp.querySelectorAll('.r em');
  ['atm.bed.on', 'atm.drn.on', 'atm.tex.on'].forEach((id, i) => { ems[i].style.width = (alive() && on(id) ? Math.round(Math.min(1, actSm[i] * 1.6) * 100) : 0) + '%'; });
}
let shown = false, lastT = performance.now();
function loop() {
  const vis = SP.style.display !== 'none' && SP.offsetWidth > 0 && dev.classList.contains('page-aether');
  const now = performance.now(), dt = Math.min(.1, (now - lastT) / 1000); lastT = now;
  if (!inPlugin()) previewMeters();
  for (let i = 0; i < 3; i++) actSm[i] += (act[i] - actSm[i]) * Math.min(1, dt * (act[i] > actSm[i] ? 14 : 4));
  if (vis) {
    if (!shown) { drawDroneVis(); drawDisplay(); }
    drawBedVis(now); drawTexVis(now); drawVoid(now, dt); drawMeters();
    if (isShep(Math.round(S['atm.drn.voice']))) drawDroneVis();
  }
  shown = vis;
  requestAnimationFrame(loop);
}

/* ============================================================ hooks for synth.js and the bridge */
Object.assign(K, {
  hasAether: true,
  loadAether(a, name, cat) { applyScene(a); showScene(name, SCENES.findIndex(s => s.name === name), cat === 'HORROR' ? 'HORROR' : ''); flash(); if (cat in CAT_MOOD) set('ui.mood', CAT_MOOD[cat], 'preset'); },
  aetherState: () => ({ name: scName, idx: scIdx, cat: scCat }),
  aetherRestore: st => showScene(st.name, st.idx, st.cat),
  aetherLive(m) {
    if (!m || !inPlugin()) return;                  // in a browser the preview engine's own meters drive the page
    if (Array.isArray(m.act)) act = m.act.map(x => clamp(+x || 0, 0, 2));
    if (typeof m.ev === 'number') { const n = m.ev - lastEv; if (lastEv && n > 0 && n < 50) for (let i = 0; i < Math.min(n, 6); i++) newEvent(); lastEv = m.ev; }
    if (typeof m.root === 'number' && m.root >= 0) { const r = Math.round(m.root); if (r !== liveRoot) { liveRoot = r; if (on('atm.drn.follow')) { drawDroneInfo(); } } }
  },
  powerChanged() { drawPower(); if (inPlugin()) return; if (alive()) pvRebuild(['all']); else if (PV.on) pvGlobal(); },
  pageShown(p) { if (p === 'aether') { scOpen(false); setTimeout(() => { drawDroneVis(); drawDisplay(); }, 20); } else scOpen(false); },
});
Object.assign(window.obsSynth, { aetherSummon: summonWorld, aetherMutate: mutateWorld, aetherReset: resetAether, aetherScenes: SCENES, aetherLoadScene: loadScene,
  aetherPreview: () => PV,                 // the browser preview engine (for measuring it)
  aetherRollBed: () => { keepUndo(); rollBed(); markScene(); }, aetherRollDrone: () => { keepUndo(); rollDrone(); markScene(); }, aetherRollTexture: () => { keepUndo(); rollTexture(); markScene(); } });

/* ============================================================ preview engine (browser only) */
/* In the plugin the C++ engine plays (Atmos.h); here a simpler Web Audio version does, so the page can be heard anywhere. */
const PV = { on: false };
/* The browser's small engine has the first sounds of each list; the newer ones (2026-11-26) play as the nearest of those
   here. The plugin plays them all, with SHINE, AIR and SHIMMER. */
const PV_BED_OF = [0, 1, 2, 3, 4, 5, 6, 2, 0, 1, 2, 3, 0], PV_VOICE_OF = [0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 3, 0, 0];
const PV_TEX_OF = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 4, 4, 4, 3, 4, 4, 4, 1];
const pvBedT = () => PV_BED_OF[Math.round(S['atm.bed.type'])] ?? 0, pvVoice = () => PV_VOICE_OF[Math.round(S['atm.drn.voice'])] ?? 0,
  pvTexT = () => PV_TEX_OF[Math.round(S['atm.tex.type'])] ?? 0;
function noiseBuf(ac, kind, secs = 4) {
  const n = Math.floor(ac.sampleRate * secs), b = ac.createBuffer(2, n, ac.sampleRate);
  for (let c = 0; c < 2; c++) { const d = b.getChannelData(c); let b0 = 0, b1 = 0, b2 = 0, br = 0;
    for (let i = 0; i < n; i++) { const w = Math.random() * 2 - 1;
      if (kind === 'pink') { b0 = .99765 * b0 + w * .099046; b1 = .963 * b1 + w * .2965164; b2 = .57 * b2 + w * 1.0526913; d[i] = (b0 + b1 + b2 + w * .1848) * .22; }
      else if (kind === 'brown') { br = br * .996 + w * .06; d[i] = br * 2.2; }
      else if (kind === 'crackle') d[i] = Math.random() < .0009 ? w * (.3 + Math.random() * .7) : 0;
      else d[i] = w * .4; } }
  return b;
}
function ir(ac, secs, frz) {
  const n = Math.floor(ac.sampleRate * Math.min(12, secs)), b = ac.createBuffer(2, n, ac.sampleRate), k = Math.log(1000) / (secs * ac.sampleRate);
  for (let c = 0; c < 2; c++) { const d = b.getChannelData(c); for (let i = 0; i < n; i++) d[i] = (Math.random() * 2 - 1) * Math.exp(-k * i * (frz ? .15 : 1)) * (i < 200 ? i / 200 : 1); }
  return b;
}
function pvBuild() {
  const ac = K.audio(); if (ac.state === 'suspended') ac.resume();
  const out = ac.createGain(); out.gain.value = 0; out.connect(K.out());
  const lo = ac.createBiquadFilter(); lo.type = 'lowshelf'; lo.frequency.value = 260;
  const hi = ac.createBiquadFilter(); hi.type = 'highshelf'; hi.frequency.value = 2600;
  const grit = ac.createWaveShaper(); grit.oversample = '2x';
  const sum = ac.createGain(), dry = ac.createGain(), wet = ac.createGain(), conv = ac.createConvolver();
  sum.connect(grit); grit.connect(dry); grit.connect(conv); conv.connect(wet); dry.connect(lo); wet.connect(lo); lo.connect(hi); hi.connect(out);
  const mk = () => { const g = ac.createGain(), an = ac.createAnalyser(); an.fftSize = 512; g.connect(sum); g.connect(an); return { g, an, nodes: [] }; };
  Object.assign(PV, { ac, out, lo, hi, grit, sum, dry, wet, conv, bed: mk(), drn: mk(), tex: mk(), bufs: { white: noiseBuf(ac, 'white'), pink: noiseBuf(ac, 'pink'), brown: noiseBuf(ac, 'brown'), crackle: noiseBuf(ac, 'crackle') }, irKey: '' });
  PV.on = true;
}
const stopAll = L => { L.nodes.forEach(n => { try { n.stop ? n.stop() : 0; } catch (_) {} try { n.disconnect(); } catch (_) {} }); L.nodes = []; };
const lvlOf = pct => Math.pow(clamp(pct / 100, 0, 1), 1.7);
function pvSrc(buf) { const s = PV.ac.createBufferSource(); s.buffer = buf; s.loop = true; s.loopStart = Math.random(); s.start(0, Math.random() * 3); return s; }
function pvLfo(hz, amt, target, type = 'sine') { const o = PV.ac.createOscillator(), g = PV.ac.createGain(); o.type = type; o.frequency.value = hz; g.gain.value = amt; o.connect(g); g.connect(target); o.start(); return [o, g]; }
function pvBed() {
  const L = PV.bed, ac = PV.ac; stopAll(L); if (!on('atm.bed.on')) return;
  const t = pvBedT(), tone = S['atm.bed.tone'] / 100, mo = S['atm.bed.motion'] / 100, d = S['atm.bed.depth'] / 100, rate = .04 + mo * mo * 1.6;
  const f = ac.createBiquadFilter(), g = ac.createGain(); f.connect(g); g.connect(L.g); L.nodes.push(f, g);
  const src = pvSrc(t === 4 ? PV.bufs.brown : t === 5 ? PV.bufs.white : PV.bufs.pink); src.connect(f); L.nodes.push(src);
  if (t === 0) { f.type = 'lowpass'; f.frequency.value = 1800 * Math.pow(2, tone * 3.3); L.nodes.push(...pvLfo(rate, .3 * d, g.gain)); }
  if (t === 1) { f.type = 'bandpass'; f.Q.value = 1.4 + d * 4.5; const fc = 220 * Math.pow(2, tone * 3.4); f.frequency.value = fc; L.nodes.push(...pvLfo(rate, fc * .6 * d, f.frequency), ...pvLfo(rate * .7, .5, g.gain, 'triangle')); g.gain.value = .9; }
  if (t === 2) { f.type = 'highpass'; f.frequency.value = 420; const lp = ac.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 1800 + tone * 9000; f.disconnect(); f.connect(lp); lp.connect(g); L.nodes.push(lp);
    const cr = pvSrc(PV.bufs.crackle), bp = ac.createBiquadFilter(), cg = ac.createGain(); bp.type = 'bandpass'; bp.frequency.value = 1300 * Math.pow(2, tone * 1.5); bp.Q.value = 6; cg.gain.value = 3 + mo * 8;
    cr.playbackRate.value = .5 + mo * 3; cr.connect(bp); bp.connect(cg); cg.connect(L.g); L.nodes.push(cr, bp, cg); }
  if (t === 3) { f.type = 'lowpass'; f.frequency.value = 600 + tone * 3000; L.nodes.push(...pvLfo(1 / (11 - 7 * mo), 500 + tone * 2500, f.frequency), ...pvLfo(1 / (11 - 7 * mo), .45 * (.5 + d), g.gain)); g.gain.value = .9; }
  if (t === 4) { f.type = 'lowpass'; f.frequency.value = 32 + tone * tone * 260; f.Q.value = .9; g.gain.value = 1.6; const o = ac.createOscillator(), og = ac.createGain(); o.frequency.value = 27 + tone * 20; og.gain.value = .15 + .4 * d; o.connect(og); og.connect(L.g); o.start(); L.nodes.push(o, og, ...pvLfo(rate, .4 * d, og.gain)); }
  if (t === 5) { f.type = 'bandpass'; f.frequency.value = 1300 * Math.pow(2, tone * 2.2); f.Q.value = .9; g.gain.value = .7; const cr = pvSrc(PV.bufs.crackle), cg = ac.createGain(); cg.gain.value = 2.5; cr.playbackRate.value = .5 + mo * 3; cr.connect(cg); cg.connect(L.g);
    const w = ac.createOscillator(), wg = ac.createGain(); w.frequency.value = 2000 + tone * 1500; wg.gain.value = d * d * .05; w.connect(wg); wg.connect(L.g); w.start(); L.nodes.push(cr, cg, w, wg, ...pvLfo(.2 + mo, 600, w.frequency, 'triangle')); }
  if (t === 6) { f.type = 'bandpass'; f.frequency.value = 620 * Math.pow(2, (tone - .5) * 1.2); f.Q.value = 3; const f2 = ac.createBiquadFilter(); f2.type = 'bandpass'; f2.frequency.value = 1750 * Math.pow(2, (tone - .5) * 1.2); f2.Q.value = 4; src.connect(f2); f2.connect(g); L.nodes.push(f2);
    g.gain.value = 0; const per = 6.5 - 4 * mo, now = ac.currentTime; for (let i = 0; i < 40; i++) { const t0 = now + i * per; g.gain.setTargetAtTime(1.8, t0, per * .12); g.gain.setTargetAtTime(.9, t0 + per * .38, per * .05); g.gain.setTargetAtTime(0, t0 + per * .5, per * .1); } }
}
function pvDrone() {
  const L = PV.drn, ac = PV.ac; stopAll(L); if (!on('atm.drn.on')) return;
  const v = pvVoice(), tone = S['atm.drn.tone'] / 100, det = S['atm.drn.detune'] / 100, mo = S['atm.drn.motion'] / 100, tune = S['syn.tune'] || 440;
  const hz = m => tune * Math.pow(2, (m - 69) / 12), notes = droneNotes();
  const lp = ac.createBiquadFilter(); lp.type = 'lowpass'; lp.Q.value = .7; lp.connect(L.g); L.nodes.push(lp);
  const norm = 1 / Math.sqrt(notes.length * 2);
  if (v >= 5) {                    // Shepard: octave partials gliding under a bell window, rotated by a timer
    lp.frequency.value = 500 * Math.pow(2, tone * 4.5);
    const parts = notes.map(m => Array.from({ length: 8 }, (_, k) => { const o = ac.createOscillator(), g = ac.createGain(); g.gain.value = 0; o.connect(g); g.connect(lp); o.start(); L.nodes.push(o, g); return { o, g, k, pc: (m % 12) / 12 }; }));
    const t0 = ac.currentTime, sp = (.02 + mo * .2) * (v === 5 ? -1 : 1);
    const upd = () => { if (!PV.on || !L.nodes.length) return; const now = ac.currentTime, pos = (((now - t0) * sp) % 1 + 1) % 1;
      parts.flat().forEach(p => { const y = ((p.k + pos + p.pc) % 8 + 8) % 8, f = 16.35 * Math.pow(2, y), a = Math.sin(Math.PI * y / 8);
        p.o.frequency.setTargetAtTime(f, now, .03); p.g.gain.setTargetAtTime(a * a * .18 * norm, now, y < .1 || y > 7.9 ? .001 : .05); }); };
    upd(); const id = setInterval(upd, 60); L.nodes.push({ stop: () => clearInterval(id), disconnect() {} });
    return;
  }
  lp.frequency.value = [700 * Math.pow(2, tone * 4), 350 * Math.pow(2, tone * 4.6), 5000, 2500 * Math.pow(2, tone * 2), 90 * Math.pow(2, tone * 3.6)][v];
  let dest = lp;
  if (v === 2) { dest = ac.createGain(); [[800, 1], [1150, .7], [2900, .4]].forEach(([fq, a]) => { const b = ac.createBiquadFilter(), g = ac.createGain(); b.type = 'bandpass'; b.frequency.value = fq * Math.pow(2, (tone - .5) * .8); b.Q.value = 7; g.gain.value = a * 3.5; dest.connect(b); b.connect(g); g.connect(lp); L.nodes.push(b, g); L.nodes.push(...pvLfo(.04 + mo * .3, fq * .25, b.frequency)); }); L.nodes.push(dest); }
  if (v === 4) { const ws = ac.createWaveShaper(), c = new Float32Array(1024), dr = 1.5 + tone * 5; for (let i = 0; i < 1024; i++) c[i] = Math.tanh((i / 511.5 - 1) * dr); ws.curve = c; dest = ws; ws.connect(lp); L.nodes.push(ws); }
  notes.forEach(m => [-1, 1].forEach(sd => {
    const o = ac.createOscillator(), g = ac.createGain(), pn = ac.createStereoPanner(); pn.pan.value = sd * .6 * S['atm.width'] / 100;
    o.type = v === 1 || v === 2 ? 'sawtooth' : v === 4 ? 'sawtooth' : 'sine'; o.frequency.value = hz(m) * (v === 4 ? .5 : 1); o.detune.value = sd * det * 22;
    if (v === 0) { const re = new Float32Array([0, 1, .55, .3 * tone, .2 * tone * tone]), im = new Float32Array(5); o.setPeriodicWave(ac.createPeriodicWave(re, im)); }
    if (v === 3) { const mod = ac.createOscillator(), mg = ac.createGain(); mod.frequency.value = hz(m) * 3.51; mg.gain.value = hz(m) * (.04 + tone * tone * .45) * 3; mod.connect(mg); mg.connect(o.frequency); mod.start(); L.nodes.push(mod, mg); }
    if (v === 1 || v === 2) L.nodes.push(...pvLfo(4.6 + mo, 22 * mo, o.detune));
    g.gain.value = (v === 1 || v === 2 || v === 4 ? .22 : .32) * norm; o.connect(g); g.connect(pn); pn.connect(dest); o.start(); L.nodes.push(o, g, pn); }));
}
/* textures: one-shot sounds scheduled a little ahead */
function pvTexEvent(t) {
  const ac = PV.ac, L = PV.tex, ty = pvTexT(), tone = S['atm.tex.tone'] / 100, ch = S['atm.tex.chaos'] / 100, w = S['atm.width'] / 100;
  const pan = ac.createStereoPanner(); pan.pan.value = rr(-1, 1) * w * .9; pan.connect(L.g);
  const env = (g, a, at, dec) => { g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(a, t + at); g.gain.setTargetAtTime(0, t + at, dec); };
  const noise = dur => { const s = ac.createBufferSource(); s.buffer = PV.bufs.white; s.start(t, Math.random() * 3, dur); return s; };
  const osc = (type, f) => { const o = ac.createOscillator(); o.type = type; o.frequency.setValueAtTime(f, t); o.start(t); return o; };
  const end = (nodes, after) => setTimeout(() => nodes.forEach(n => { try { n.stop && n.stop(); } catch (_) {} try { n.disconnect(); } catch (_) {} }), (t - ac.currentTime + after) * 1000);
  const g = ac.createGain(); g.connect(pan);
  if (ty === 0 || ty === 1) { const s = noise(.03), b = ac.createBiquadFilter(); b.type = 'bandpass'; b.frequency.value = (ty ? 1400 : 3000) * Math.pow(2, tone * 1.5 + rr(-.5, .5)); b.Q.value = 2; s.connect(b); b.connect(g); env(g, rr(.3, .9), .001, .01); end([s, b, g, pan], .3); }
  if (ty === 2) { const o = osc('sine', 450 * Math.pow(2, tone * 2.2 + rr(-.6, .6))); o.frequency.exponentialRampToValueAtTime(o.frequency.value * 2.1, t + .03); o.connect(g); env(g, rr(.2, .5), .002, .04); end([o, g, pan], .6); }
  if (ty === 3) { const s = noise(Math.random() < .4 ? .015 : rr(.6, 2)), base = 55 * Math.pow(2, tone * 3 + rr(-.8, .8));
    [1, 2.32, 4.25, 6.63].forEach((r, k) => { const b = ac.createBiquadFilter(), bg = ac.createGain(); b.type = 'bandpass'; b.frequency.value = base * r; b.Q.value = 60; bg.gain.value = 260 / (k + 1); s.connect(b); b.connect(bg); bg.connect(g); });
    env(g, .5, .05, .6); end([s, g, pan], 4); }
  if (ty === 4) { const pent = [0, 2, 4, 7, 9], m = 72 + 12 * Math.floor(tone * 2) + Math.round(S['syn.key']) + pick(pent), o = osc('sine', 440 * Math.pow(2, (m - 69) / 12)); o.connect(g); const d = rr(.15, 1); env(g, rr(.08, .2), d * .3, d * .3); end([o, g, pan], d * 3); }
  if (ty === 5) { const s = noise(1.2), a = ac.createBiquadFilter(), b = ac.createBiquadFilter(); a.type = b.type = 'bandpass'; a.Q.value = 6; b.Q.value = 7;
    a.frequency.setValueAtTime(rr(300, 800), t); a.frequency.linearRampToValueAtTime(rr(300, 800), t + .9); b.frequency.setValueAtTime(rr(900, 2400), t); b.frequency.linearRampToValueAtTime(rr(900, 2400), t + .9);
    s.connect(a); s.connect(b); a.connect(g); b.connect(g); g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(1.6, t + .25); g.gain.linearRampToValueAtTime(0, t + rr(.5, 1.1)); end([s, a, b, g, pan], 1.5); }
  if (ty === 6) { const gap = 60 / (42 + S['atm.tex.density'] * 1.18); [0, .28 * gap].forEach((dt, k) => { const o = ac.createOscillator(), og = ac.createGain(); o.frequency.setValueAtTime(k ? 74 : 58, t + dt); o.frequency.exponentialRampToValueAtTime(k ? 46 : 36, t + dt + .1);
    o.connect(og); og.connect(pan); og.gain.setValueAtTime(0, t + dt); og.gain.linearRampToValueAtTime(k ? .5 : .75, t + dt + .005); og.gain.setTargetAtTime(0, t + dt + .005, .08); o.start(t + dt); end([o, og], dt + .5); }); pan.pan.value = 0; end([pan, g], 1); }
  if (ty === 7) { const n = 2 + Math.floor(Math.random() * (1.5 + ch * 1.5)), gap = rr(.13, .23); for (let k = 0; k < n; k++) { const tk = t + k * gap, s = ac.createBufferSource(), b = ac.createBiquadFilter(), kg = ac.createGain();
    s.buffer = PV.bufs.white; s.start(tk, Math.random() * 3, .004); b.type = 'bandpass'; b.frequency.value = 170 * Math.pow(2, tone * 1.3); b.Q.value = 5; s.connect(b); b.connect(kg); kg.connect(pan); kg.gain.value = 120; end([s, b, kg], k * gap + .4); } end([g, pan], n * gap + .5); }
  if (ty === 8) { const o = osc('sawtooth', 82 * Math.pow(2, tone * 2.4 + rr(-.45, .45))), dur = rr(1.3, 3.5), f0 = o.frequency.value, up = rr(1.5, 5.5);
    o.frequency.setValueAtTime(f0, t); o.frequency.linearRampToValueAtTime(f0 * Math.pow(2, up / 12), t + dur * .22); o.frequency.linearRampToValueAtTime(f0 * Math.pow(2, (up - 12 * rr(.3, 1)) / 12 - 1 / 12), t + dur);
    const fm = ac.createGain(); [[450, .5], [800, .3], [2830, .1]].forEach(([fq, a]) => { const b = ac.createBiquadFilter(), bg = ac.createGain(); b.type = 'bandpass'; b.frequency.value = fq; b.Q.value = 7; bg.gain.value = a * 5; fm.connect(b); b.connect(bg); bg.connect(g); });
    o.connect(fm); const vib = pvLfo(5.5, 6 + ch * 20, o.detune); g.gain.setValueAtTime(0, t); g.gain.linearRampToValueAtTime(.35, t + dur * .25); g.gain.linearRampToValueAtTime(0, t + dur); end([o, fm, g, pan, ...vib], dur + .3); }
  if (ty === 9) { const s = noise(rr(.12, .6)), b = ac.createBiquadFilter(); b.type = 'bandpass'; b.Q.value = 7; const f0 = 220 * Math.pow(2, tone * 1.5 + rr(-.5, .5)); b.frequency.setValueAtTime(f0, t); b.frequency.linearRampToValueAtTime(f0 * Math.pow(2, rr(-1.4, 1.4)), t + .5);
    s.connect(b); b.connect(g); g.gain.setValueAtTime(0, t); for (let k = 0; k < 8; k++) g.gain.setValueAtTime(Math.random() < .6 ? rr(.4, 1.6) : 0, t + k * .05); g.gain.setTargetAtTime(0, t + .4, .05); end([s, b, g, pan], 1); }
  if (ty === 0 || ty === 1 || ty === 2) {}           // continuous parts are in pvTexBed
  newEvent();
}
function pvTexBed() {
  const L = PV.tex, ac = PV.ac; stopAll(L); if (!on('atm.tex.on')) return;
  const ty = pvTexT(), tone = S['atm.tex.tone'] / 100, d = S['atm.tex.density'] / 100;
  if (ty === 0) { const s = pvSrc(PV.bufs.crackle), hp = ac.createBiquadFilter(), g = ac.createGain(); s.playbackRate.value = .5 + d * 3; hp.type = 'highpass'; hp.frequency.value = 900; g.gain.value = 2; s.connect(hp); hp.connect(g); g.connect(L.g); L.nodes.push(s, hp, g); }
  if (ty === 1) { const s = pvSrc(PV.bufs.brown), lp = ac.createBiquadFilter(), g = ac.createGain(); lp.type = 'lowpass'; lp.frequency.value = 250 + tone * 900; g.gain.value = .5 + .4 * d; s.connect(lp); lp.connect(g); g.connect(L.g); L.nodes.push(s, lp, g, ...pvLfo(.4 + d, .25, g.gain)); }
}
//                Hiss Wind Rain Surf Rumble Static Breath
const PV_BED_DB = [-7.5, 3.4, -7.2, -9.7, -13.6, 6.6, -1.9];
//                Organ Strings Choir Glass Abyss Descent Ascent
const PV_DRN_DB = [3.3, 7.5, 13.9, 2.4, -1.7, 1, 1.1];
//                Vinyl Fire Drips Metal Shimmer Whispers Heart Knocks Moans Flesh
const PV_TEX_DB = [5.6, -16.6, -0.5, -14.3, 0, 7, -3.8, -4.2, -3.8, -4.6];
const TEX_RATE = [[.15, 2], [.8, 14], [.25, 7], [.06, 1.4], [.6, 22], [.15, 2.8], [0, 0], [.06, 1.2], [.05, .9], [.12, 3.5]];
let pvNext = 0;
function pvSchedule() {
  if (!PV.on || !on('atm.tex.on')) return;
  const ac = PV.ac, ty = pvTexT(), d = S['atm.tex.density'] / 100, ch = S['atm.tex.chaos'] / 100;
  const rate = ty === 6 ? (42 + d * 118) / 60 : TEX_RATE[ty][0] + (TEX_RATE[ty][1] - TEX_RATE[ty][0]) * d * d;
  if (pvNext < ac.currentTime) pvNext = ac.currentTime + .05;
  while (pvNext < ac.currentTime + .2) {
    pvTexEvent(pvNext);
    const mean = 1 / Math.max(.01, rate);
    pvNext += ty === 6 ? mean * (1 + .18 * ch * rr(-1, 1)) : mean * ((1 - ch) + ch * -Math.log(Math.max(1e-4, Math.random())));
  }
}
function pvGlobal() {
  if (!PV.on) return;
  const ac = PV.ac, now = ac.currentTime, grit = S['atm.grit'] / 100, tilt = S['atm.tone'] / 100;
  PV.out.gain.setTargetAtTime(alive() ? Math.pow(10, S['atm.level'] / 20) * .9 : 0, now, .05);
  PV.lo.gain.setTargetAtTime(-tilt * 7, now, .05); PV.hi.gain.setTargetAtTime(tilt * 7, now, .05);
  const c = new Float32Array(1024), dr = 1 + grit * grit * 22, q = grit > .55 ? 2 + (1 - grit) * 60 : 0;
  for (let i = 0; i < 1024; i++) { const x = i / 511.5 - 1; let y = grit > .002 ? x + (Math.tanh(x * dr) / Math.tanh(dr * .5) * .5 - x) * Math.min(1, grit * 3) : x; if (q) y = Math.round(y * q) / q; c[i] = y; }
  PV.grit.curve = c;
  const mix = S['atm.space'] / 100; PV.wet.gain.setTargetAtTime(mix * 1.1, now, .05); PV.dry.gain.setTargetAtTime(1 - mix * .45, now, .05);
  const t60 = on('atm.freeze') ? 12 : 1.2 * Math.pow(25, S['atm.size'] / 100), key = t60.toFixed(1) + (on('atm.freeze') ? 'f' : '');
  if (key !== PV.irKey) { PV.irKey = key; PV.conv.buffer = ir(ac, Math.min(t60, 12), on('atm.freeze')); }
  // per sound, so the preview's balance follows the plugin's (measured in headless Chrome against the engine's renders)
  const lv = (L, id, idOn, trim) => L.g.gain.setTargetAtTime(on(idOn) ? lvlOf(S[id]) * Math.pow(10, trim / 20) : 0, now, .08);
  lv(PV.bed, 'atm.bed.level', 'atm.bed.on', PV_BED_DB[pvBedT()]);
  lv(PV.drn, 'atm.drn.level', 'atm.drn.on', PV_DRN_DB[pvVoice()]);
  lv(PV.tex, 'atm.tex.level', 'atm.tex.on', PV_TEX_DB[pvTexT()]);
}
const rms = an => { const a = new Float32Array(an.fftSize); an.getFloatTimeDomainData(a); let s = 0; for (const x of a) s += x * x; return Math.sqrt(s / a.length) * 4; };
function previewMeters() {
  if (!PV.on) { act = [0, 0, 0]; return; }
  act = [rms(PV.bed.an), rms(PV.drn.an), rms(PV.tex.an)];
}
/* rebuild what changed: every layer named in `what` (a scene changes all three at once) */
let pvT = null; const pvWant = new Set();
function pvRebuild(what) {
  if (inPlugin()) return;
  what.forEach(w => pvWant.add(w));
  if (!alive()) { if (PV.on) pvGlobal(); return; }
  if (!PV.on) { pvBuild(); pvWant.add('all'); }
  pvGlobal();
  clearTimeout(pvT);
  pvT = setTimeout(() => {
    const all = pvWant.has('all'); const w = new Set(pvWant); pvWant.clear();
    if (all || w.has('bed')) pvBed(); if (all || w.has('drn')) pvDrone(); if (all || w.has('tex')) pvTexBed();
  }, 30);
}
onChange((id, v, from) => {
  if (inPlugin() || !id.startsWith('atm.') && id !== 'syn.key' && id !== 'syn.tune') return;
  const w = [];
  if (id === 'atm.on' && on('atm.on')) w.push('all');
  if (/^atm\.bed\./.test(id) && !/level$/.test(id)) w.push('bed');
  if ((/^atm\.drn\./.test(id) && !/level$/.test(id)) || id === 'syn.key' || id === 'syn.tune' || id === 'atm.width') w.push('drn');
  if (/^atm\.tex\.(type|on|tone|density)$/.test(id)) w.push('tex');
  pvRebuild(w);
});
setInterval(() => { if (!inPlugin() && PV.on && alive()) pvSchedule(); }, 50);

/* ------------------------------------------------------------ start */
bedKnobs._relabel(); drnKnobs._relabel(); texKnobs._relabel(); drawHints(); drawDisplay(); drawDroneInfo(); drawPower(); drawMood(); drawMoodOpts(); drawScName();
loop();
})();
</script>
