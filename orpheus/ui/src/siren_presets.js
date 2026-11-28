<script>
/* SIREN presets for ORPHEUS (the standalone): SIREN's sound only (never AETHER or the song), effects included.
   Made for chords that hold and glide: no arpeggiator, chord or hold, and the volume envelope stays as it is.
   Mostly leads, by mood: HEAVENLY, BEAUTIFUL, NEUTRAL, EERIE, SCARY; then PADS, KEYS & PLUCKS, BASS and RETRO.
   Each preset lists only what differs from the defaults (ids without "syn.", choices by name).
   shape() writes the FILTER ENVELOPE's points; the fx helpers switch an effect on with its settings.
   TRIM: each one's level in dB, measured (headless Chrome, the ORPHEUS demo song) so they all play about as loud.
   These are our own takes on the kind of sound in the name, not copies of anyone's patch. */
(function () {
const SH = {
  SNAP: [[2, 100], [90, 40]], PLUCK: [[3, 100], [180, 12]], BLOOM: [[650, 100], [1700, 45]], SWELL: [[2200, 100]], RISE: [[4000, 100]],
  OPEN: [[5, 100]], SLOW: [[1200, 100], [3000, 60]], DROP: [[5, 100], [1600, 0]],
  WAH: [[150, 100], [420, 15], [680, 85], [950, 15], [1250, 70], [1650, 25]],
  STEPS: [[5, 100], [500, 100], [510, 45], [1000, 45], [1010, 75], [1500, 75], [1510, 20]],
  PULSE: [[5, 95], [160, 15], [320, 95], [480, 15], [640, 95], [800, 15], [960, 60]],
};
const shape = (name, speed) => { const o = { 'fenv.n': SH[name].length, 'fenv.speed': speed || 100 }; SH[name].forEach(([t, l], i) => { o['fenv.t' + (i + 1)] = t; o['fenv.l' + (i + 1)] = l; }); return o; };
const vib = (depth, fade, rate) => ({ 'lfo.on': 1, 'lfo.shape': 'Sine', 'lfo.target': 'Pitch', 'lfo.depth': depth, 'lfo.fade': fade == null ? 400 : fade, 'lfo.rate': rate || 5.5 });
const opt = (o, k, v) => { if (v != null) o[k] = v; return o; };
const fx = (id, kv) => { const o = { ['fx.' + id + '.on']: 1 }; for (const k in kv) opt(o, 'fx.' + id + '.' + k, kv[k]); return o; };
const REV = (size, mix, damp) => fx('rev', { size, mix, damp }), DLY = (div, mix, fb, tone) => fx('dly', { div, mix, fb, tone });
const SHM = (amt, decay, mix) => fx('shm', { amt, decay, mix }), CHO = (mix, depth, rate) => fx('cho', { mix, depth, rate });
const OCT = (mode, mix) => fx('oct', { mode, mix }), DRV = (amt, tone, mix) => fx('drv', { amt, tone, mix }), RING = (freq, mix) => fx('ring', { freq, mix });
const PHA = (rate, depth, fb, mix) => fx('pha', { rate, depth, fb, mix }), FLA = (rate, depth, fb, mix) => fx('fla', { rate, depth, fb, mix }), CRU = (bits, mix) => fx('cru', { bits, mix });
const solo = { 'b.on': 0, 'sub.level': 0 };
const TRIM = window.OBS_SIREN_TRIM || {};
const P = (cat, name, note, v) => ({ cat, name, note, v: Object.assign({}, v, { trim: TRIM[name] || 0 }) });

window.OBS_SYNTH_PRESETS = [
  /* ---------------------------------------------------------------- HEAVENLY: light, octaves above, endless air */
  P('HEAVENLY', 'Seraph', 'Seven bright saws lifted by an endless shimmer', { 'a.wave': 'Super', 'a.shape': 60, 'a.detune': 30, 'b.wave': 'Sine', 'b.pitch': 12, 'b.level': 30, 'sub.level': 0,
    'flt.type': 'Lowpass', 'flt.cutoff': 4200, 'flt.env': 15, ...SHM(70, 85, 45), ...REV(85, 35, 30), 'fx.width': 150 }),
  P('HEAVENLY', 'Halo', 'A pure sine with a ghost an octave above and a rising tail', { ...solo, 'a.wave': 'Sine', 'a.voices': 3, 'a.detune': 12, 'flt.on': 0, ...vib(8, 700, 5),
    ...OCT('Octave up', 40), ...SHM(65, 80, 40), ...REV(80, 30) }),
  P('HEAVENLY', 'Ascension', 'Opens slowly on every chord and keeps climbing', { 'a.wave': 'Super', 'a.shape': 50, 'b.wave': 'Triangle', 'b.pitch': 12, 'b.level': 35, 'flt.cutoff': 400, 'flt.res': 20, 'flt.env': 75,
    ...shape('RISE'), ...OCT('Octave up', 30), ...SHM(75, 90, 45), ...REV(90, 30) }),
  P('HEAVENLY', 'Choir of Light', 'A vowel choir floating in shimmer', { 'a.voices': 5, 'b.wave': 'Saw', 'b.pitch': 12, 'b.voices': 5, 'b.level': 45, 'flt.type': 'Vowel', 'flt.cutoff': 1400, 'flt.res': 35,
    'flt.env': 25, ...shape('SLOW'), ...CHO(40), ...SHM(60, 85, 40), ...REV(85, 35) }),
  P('HEAVENLY', 'Glass Angel', 'Crystal triangles with quick sparkling echoes', { 'a.wave': 'Triangle', 'a.shape': 40, 'b.wave': 'Sine', 'b.pitch': 24, 'b.level': 25, 'sub.level': 0, 'flt.on': 0,
    ...OCT('Octave up', 30), ...DLY('1/16', 22, 45, 70), ...SHM(55, 75, 40), ...REV(70, 25) }),
  P('HEAVENLY', 'Celestial Flute', 'Breathy sine flute, vibrato, angels behind it', { ...solo, 'a.wave': 'Sine', 'a.voices': 1, 'noise.level': 8, 'noise.color': 85, 'flt.type': 'Lowpass', 'flt.cutoff': 5000, 'flt.env': 0,
    glide: 90, ...vib(14, 500, 5.6), ...SHM(50, 80, 35), ...REV(80, 35) }),
  P('HEAVENLY', 'Holy Saw', 'A soft, wide saw choir with a long shining tail', { 'a.wave': 'Super', 'a.shape': 35, 'a.detune': 25, 'flt.type': 'Lowpass', 'flt.cutoff': 2200, 'flt.env': 20, ...shape('BLOOM'),
    ...CHO(35), ...SHM(55, 90, 35), ...REV(95, 40, 25), 'fx.dbl': 1 }),
  P('HEAVENLY', 'Gabriel', 'A trumpet-like lead in a cathedral', { 'a.detune': 12, 'b.wave': 'Saw', 'b.pitch': 0, 'b.fine': -7, 'b.level': 60, 'flt.type': 'Ladder 12', 'flt.cutoff': 800, 'flt.env': 60,
    ...shape('BLOOM', 220), ...vib(10, 600), ...REV(100, 45, 30), ...SHM(35, 70, 25) }),
  P('HEAVENLY', 'Starlight', 'A narrow pulse that glitters, high and far away', { 'a.wave': 'Pulse', 'a.shape': 75, 'a.pitch': 12, 'b.on': 0, 'flt.type': 'Lowpass', 'flt.cutoff': 6000, 'flt.env': 0,
    ...DLY('1/8 dotted', 30, 55, 75), ...SHM(70, 85, 45), ...REV(85, 30), 'fx.width': 170 }),
  P('HEAVENLY', 'Heaven', 'Shimmering octaves that keep rising, very wide', { 'a.wave': 'Triangle', 'a.voices': 5, 'b.wave': 'Sine', 'b.pitch': 12, 'flt.type': 'Lowpass', 'flt.cutoff': 3000,
    ...SHM(75, 90, 55), ...REV(90, 40), ...CHO(40), 'fx.width': 150 }),
  /* ---------------------------------------------------------------- BEAUTIFUL: warm, soft, singing */
  P('BEAUTIFUL', 'Velvet', 'A soft saw lead wrapped in chorus', { ...solo, 'a.voices': 3, 'a.detune': 14, 'flt.type': 'Lowpass', 'flt.cutoff': 1600, 'flt.res': 10, 'flt.env': 25, ...shape('BLOOM'),
    glide: 70, ...vib(10, 600), ...CHO(45), ...REV(60, 25) }),
  P('BEAUTIFUL', 'Silk', 'Smooth triangle lead with a gentle echo', { 'a.wave': 'Triangle', 'a.shape': 25, 'a.voices': 3, 'a.detune': 10, 'b.wave': 'Sine', 'b.pitch': 12, 'b.level': 25, 'flt.on': 0,
    glide: 60, ...CHO(35), ...DLY('1/4', 22, 35, 55), ...REV(65, 25) }),
  P('BEAUTIFUL', 'Lullaby', 'A sine that sings softly, like a music box at night', { ...solo, 'a.wave': 'Sine', 'a.voices': 1, 'flt.on': 0, ...vib(12, 800, 5),
    ...DLY('1/8 dotted', 28, 40, 50), ...REV(75, 35) }),
  P('BEAUTIFUL', 'Golden Hour', 'Warm and glowing, the sun going down', { 'a.wave': 'Super', 'a.shape': 45, 'a.detune': 20, 'b.pitch': 12, 'b.level': 25, 'flt.type': 'Lowpass', 'flt.cutoff': 2600,
    ...CHO(40), ...DLY('1/4 dotted', 25, 40, 50), ...REV(70, 30) }),
  P('BEAUTIFUL', 'Nocturne', 'A dark, gentle lead with a slow vibrato', { 'a.wave': 'Triangle', 'a.voices': 3, 'b.wave': 'Sine', 'b.pitch': -12, 'b.level': 40, 'flt.type': 'Lowpass', 'flt.cutoff': 1100,
    ...vib(9, 900, 4.6), ...REV(85, 40, 70) }),
  P('BEAUTIFUL', 'Bloom Lead', 'Each chord flowers open, then settles', { 'a.voices': 5, 'a.detune': 18, 'flt.cutoff': 500, 'flt.res': 20, 'flt.env': 65, ...shape('BLOOM'), ...CHO(35), ...REV(70, 30) }),
  P('BEAUTIFUL', 'Rain Glass', 'Glassy drops with quick echoes', { 'a.wave': 'Sine', 'a.shape': 25, 'b.wave': 'Triangle', 'b.pitch': 12, 'b.level': 35, 'flt.type': 'Lowpass', 'flt.cutoff': 900, 'flt.env': 75,
    ...shape('PLUCK', 70), ...DLY('1/16', 30, 50, 60), ...REV(70, 30) }),
  P('BEAUTIFUL', 'Violin Glide', 'A bowed, gliding lead with a late vibrato', { 'a.detune': 10, 'b.on': 0, 'flt.type': 'Ladder 12', 'flt.cutoff': 1800, 'flt.env': 30, ...shape('SWELL', 250),
    glide: 180, legato: 1, ...vib(16, 700, 5.8), ...REV(70, 30) }),
  P('BEAUTIFUL', 'Dream', 'Hazy and soft, like a memory', { 'a.voices': 5, 'a.detune': 18, 'flt.type': 'Lowpass', 'flt.cutoff': 1500, 'flt.env': 30, ...shape('BLOOM'), char: 'VHS',
    ...CHO(50), ...DLY('1/4 dotted', 35, 55), ...REV(80, 40) }),
  P('BEAUTIFUL', 'Ocean', 'Slow waves washing in and out', { 'a.wave': 'Super', 'flt.type': 'Lowpass', 'flt.cutoff': 1200, 'lfo.on': 1, 'lfo.target': 'Cutoff', 'lfo.depth': 40, 'lfo.rate': .12,
    ...FLA(.05, 70, 40, 40), ...REV(75, 35) }),
  /* ---------------------------------------------------------------- NEUTRAL: classic synth leads */
  P('NEUTRAL', 'Supersaw Lead', 'The big trance lead: seven saws, wide and bright', { 'a.wave': 'Super', 'a.shape': 75, 'a.detune': 38, 'a.width': 90,
    'b.wave': 'Saw', 'b.pitch': 12, 'b.level': 30, 'b.voices': 3, 'b.detune': 20, 'sub.level': 10, 'flt.cutoff': 5200, 'flt.res': 15, 'flt.env': 25, ...shape('SNAP'),
    glide: 30, 'fx.dbl': 1, ...DLY('1/8 dotted', 22), ...REV(60, 25) }),
  P('NEUTRAL', 'Sync Screamer', 'Hard-sync scream that cuts through everything', { 'a.voices': 1, 'b.wave': 'Saw', 'b.sync': 1, 'b.pitch': 19, 'b.level': 70, 'b.voices': 1,
    'flt.type': 'Ladder 12', 'flt.cutoff': 3500, 'flt.res': 35, 'flt.drive': 50, 'flt.env': 45, ...shape('SNAP'), ...vib(18, 400), ...DRV(30), ...DLY(null, 18) }),
  P('NEUTRAL', 'Soft Saw Lead', 'Warm, round single saw with a gentle vibrato', { ...solo, 'a.voices': 1, 'flt.type': 'Lowpass', 'flt.cutoff': 1400, 'flt.res': 10, 'flt.env': 30, ...shape('BLOOM'),
    glide: 80, ...vib(12, 600), ...CHO(), ...REV(null, 30) }),
  P('NEUTRAL', 'Square Lead', 'Hollow square-wave lead, clean and classic', { 'a.wave': 'Pulse', 'a.shape': 0, 'a.voices': 1, 'b.shape': 0, 'b.pitch': 12, 'b.level': 25, 'sub.level': 0,
    'flt.type': 'Lowpass', 'flt.cutoff': 2600, 'flt.res': 20, glide: 50, ...DLY(null, 18) }),
  P('NEUTRAL', 'Fat Mono Lead', 'Two saws slightly apart through a warm ladder: thick and creamy', { 'a.voices': 1, 'b.wave': 'Saw', 'b.pitch': 0, 'b.fine': 9, 'b.level': 70, 'sub.level': 40,
    'flt.cutoff': 900, 'flt.res': 45, 'flt.drive': 45, 'flt.env': 55, ...shape('SNAP'), glide: 90 }),
  P('NEUTRAL', 'Glide Lead', 'Slides slowly from note to note: liquid', { 'a.voices': 1, 'b.level': 35, glide: 260, legato: 1, 'flt.cutoff': 2200, ...vib(14, 500), ...REV(null, 28) }),
  P('NEUTRAL', 'Talkbox Lead', 'A talking lead: the filter shapes vowels on every chord', { 'flt.type': 'Vowel', 'flt.cutoff': 900, 'flt.res': 55, 'flt.env': 70, ...shape('WAH', 80),
    'lfo.on': 1, 'lfo.target': 'Cutoff', 'lfo.depth': 30, 'lfo.rate': .3, ...CHO(35) }),
  P('NEUTRAL', 'Brass Lead', 'Brassy synth lead that swells open on every chord', { 'a.detune': 15, 'b.wave': 'Saw', 'b.pitch': 0, 'b.fine': -6, 'b.level': 60,
    'flt.type': 'Ladder 12', 'flt.cutoff': 700, 'flt.res': 15, 'flt.env': 65, 'flt.drive': 30, ...shape('BLOOM', 250), ...REV(null, 22) }),
  P('NEUTRAL', 'Whistle Lead', 'A pure sine whistle with breath and vibrato', { ...solo, 'a.wave': 'Sine', 'a.voices': 1, 'noise.level': 6, 'noise.color': 80, 'flt.type': 'Lowpass', 'flt.cutoff': 6000, 'flt.env': 0,
    ...vib(15, 500, 5.8), glide: 70, ...REV(null, 40), ...DLY(null, 20) }),
  P('NEUTRAL', 'FM Growl Lead', 'Sines with FM grit: a snarling, growling lead', { 'a.wave': 'Sine', 'a.shape': 65, 'a.voices': 1, 'b.wave': 'Sine', 'b.pitch': 12, 'b.shape': 40, 'b.level': 40,
    'flt.cutoff': 2400, 'flt.res': 25, 'flt.drive': 60, ...DRV(35) }),
  P('NEUTRAL', 'Acid Lead', 'Squelchy acid: high resonance and a bouncing filter', { ...solo, 'a.voices': 1, 'flt.cutoff': 380, 'flt.res': 82, 'flt.drive': 55, 'flt.env': 85, ...shape('PULSE'), ...DLY(null, 25) }),
  P('NEUTRAL', 'Synthwave Lead', 'Neon 80s lead with chorus and a long echo', { 'a.wave': 'Super', 'a.shape': 50, 'a.detune': 22, 'b.pitch': 12, 'b.level': 30, 'flt.type': 'Lowpass', 'flt.cutoff': 3200, 'flt.res': 20,
    ...CHO(), ...DLY('1/4 dotted', 30), ...REV(70, 30) }),
  P('NEUTRAL', 'Fuzz Lead', 'Like a fuzz guitar: hot, gritty and loud', { 'b.wave': 'Saw', 'b.pitch': -12, 'flt.type': 'Ladder 12', 'flt.cutoff': 2800, ...DRV(70, 55), ...DLY(null, 15), 'fx.dbl': 1 }),
  P('NEUTRAL', 'Octave Lead', 'A bright lead with a ghost an octave above', { 'a.wave': 'Pulse', 'a.shape': 15, ...OCT('Octave up', 45), 'flt.type': 'Lowpass', 'flt.cutoff': 4200, ...REV(null, 25) }),
  P('NEUTRAL', 'Pluck Lead', 'Every chord starts with a bright pluck, then softens', { 'flt.cutoff': 500, 'flt.res': 30, 'flt.env': 75, ...shape('PLUCK'), ...DLY(null, 25), ...REV(null, 20) }),
  P('NEUTRAL', 'Nasal Pulse Lead', 'Thin, nasal pulse with a slow phaser', { 'a.wave': 'Pulse', 'a.shape': 70, 'a.voices': 1, 'b.pitch': 7, 'b.level': 30, 'flt.type': 'Bandpass', 'flt.cutoff': 1400, 'flt.res': 30, ...PHA(.2) }),
  P('NEUTRAL', 'Rave Hoover', 'A huge, wide, slightly sour rave lead', { 'a.wave': 'Super', 'a.shape': 90, 'a.detune': 60, 'b.wave': 'Saw', 'b.pitch': -12, 'b.voices': 5, 'b.detune': 45,
    'flt.type': 'Ladder 12', 'flt.cutoff': 3000, 'flt.drive': 40, 'fx.dbl': 1, 'fx.width': 140, ...PHA(null, null, 22, 40) }),
  /* ---------------------------------------------------------------- EERIE: unsettling, cold, not quite right */
  P('EERIE', 'Theremin', 'The old sci-fi ghost: sliding sine, wide wobbling vibrato', { ...solo, 'a.wave': 'Sine', 'a.voices': 1, 'flt.on': 0, glide: 320, legato: 1, ...vib(32, 200, 6.2), ...REV(80, 35) }),
  P('EERIE', 'Ghost Signal', 'A transmission from nowhere: warbling tape and a low ring', { 'a.wave': 'Triangle', 'a.detune': 35, char: 'VHS', ...RING(38, 25), ...PHA(.07, 70, 40, 45), ...DLY('1/4', 30, 55, 40), ...REV(80, 30, 60) }),
  P('EERIE', 'Hollow Whisper', 'Breath through a narrow filter, rising in the dark', { 'a.wave': 'Sine', 'a.level': 45, 'b.on': 0, 'noise.level': 55, 'noise.color': 60, 'flt.type': 'Bandpass', 'flt.cutoff': 1200, 'flt.res': 55,
    ...vib(6, 0, 0.3), ...SHM(45, 80, 40), ...REV(85, 40, 60) }),
  P('EERIE', 'Séance', 'A vowel that slowly changes its mouth', { 'a.voices': 3, 'a.detune': 25, 'flt.type': 'Vowel', 'flt.cutoff': 500, 'flt.res': 50, 'lfo.on': 1, 'lfo.target': 'Cutoff', 'lfo.shape': 'Drift', 'lfo.depth': 60, 'lfo.rate': .15,
    ...REV(90, 45, 75) }),
  P('EERIE', 'Frozen', 'Cold metal that hangs in the air', { 'a.wave': 'Triangle', 'a.shape': 60, 'b.wave': 'Sine', 'b.pitch': 7, 'b.level': 30, 'flt.on': 0, ...FLA(.04, 80, 85, 55), ...REV(90, 40) }),
  P('EERIE', 'Music Box', 'A detuned music box in an empty room', { 'a.wave': 'Triangle', 'a.voices': 3, 'a.detune': 45, 'b.wave': 'Sine', 'b.pitch': 24, 'b.level': 30, 'flt.type': 'Lowpass', 'flt.cutoff': 700, 'flt.env': 80,
    ...shape('PLUCK', 60), char: 'VHS', ...DLY('1/8', 25, 40, 45), ...REV(80, 35, 50) }),
  P('EERIE', 'Wraith', 'A thin voice with a shadow an octave below', { 'a.wave': 'Sine', 'a.shape': 30, 'a.voices': 3, 'a.detune': 30, 'b.on': 0, ...OCT('Octave down', 45), ...PHA(.05, 80, 50, 50), ...REV(85, 40, 70) }),
  P('EERIE', 'Lost Radio', 'A melody leaking out of a broken radio', { 'a.wave': 'Pulse', 'a.shape': 40, 'flt.type': 'Bandpass', 'flt.cutoff': 1500, 'flt.res': 35, 'noise.level': 12, char: 'VHS',
    ...CRU(7, 45), ...DLY('1/8 dotted', 30, 45, 35) }),
  P('EERIE', 'Haunted', 'A broken music box in an empty house', { 'a.wave': 'Triangle', 'a.detune': 40, char: 'VHS', ...RING(47, 35), ...SHM(40, null, 30), ...REV(null, null, 70) }),
  P('EERIE', 'Fog', 'Something moving slowly in thick fog', { 'a.wave': 'Super', 'a.shape': 30, 'flt.cutoff': 300, 'flt.res': 30, 'flt.env': 40, ...shape('SLOW'), 'lfo.on': 1, 'lfo.target': 'Pan', 'lfo.depth': 60, 'lfo.rate': .2,
    ...REV(95, 50, 85) }),
  /* ---------------------------------------------------------------- SCARY: heavy, distorted, wrong */
  P('SCARY', 'Banshee', 'A screaming sync lead that rings like metal', { 'a.voices': 1, 'b.wave': 'Saw', 'b.sync': 1, 'b.pitch': 24, 'b.level': 75, 'flt.type': 'Ladder 12', 'flt.cutoff': 3000, 'flt.res': 45,
    ...vib(30, 150, 7), ...RING(420, 25), ...DRV(45), ...REV(80, 30, 60) }),
  P('SCARY', 'Demon Lead', 'A lead with a demon singing an octave under it', { 'b.wave': 'Saw', 'b.fine': -12, 'b.level': 60, 'flt.type': 'Vowel', 'flt.cutoff': 700, 'flt.res': 40, ...OCT('Octave down', 70), ...DRV(55, 40),
    ...REV(80, 30, 80) }),
  P('SCARY', 'Hellfire', 'Fuzz, a low octave and burning echoes', { 'b.wave': 'Saw', 'b.pitch': -12, 'flt.type': 'Ladder 12', 'flt.cutoff': 2200, ...OCT('Octave down', 50), ...DRV(80, 45), ...DLY('1/8 dotted', 30, 65, 35) }),
  P('SCARY', 'Doom', 'Crushing: two octaves down, distorted, in a dark hall', { 'b.wave': 'Saw', 'b.pitch': -12, 'flt.cutoff': 600, ...OCT('Two octaves down', 60), ...DRV(60, 30), ...REV(85, 40, 85) }),
  P('SCARY', 'Butcher', 'Crushed, distorted and clanging', { 'a.wave': 'Pulse', 'a.shape': 50, ...CRU(3, 60), ...DRV(50, 50), ...RING(130, 30), ...REV(60, 20, 70) }),
  P('SCARY', 'Possessed', 'A choir speaking in a voice two octaves too low', { 'a.voices': 5, 'a.detune': 35, 'flt.type': 'Vowel', 'flt.cutoff': 500, 'flt.res': 55, 'flt.env': 60, ...shape('WAH', 60),
    ...OCT('Two octaves down', 55), ...RING(90, 20), ...REV(90, 40, 80) }),
  P('SCARY', 'Nightmare', 'Rises slowly out of the dark, then screams', { 'flt.cutoff': 250, 'flt.res': 60, 'flt.env': 90, ...shape('RISE'), ...OCT('Two octaves down', 50), ...DRV(40), ...DLY(null, null, 70), ...REV() }),
  P('SCARY', 'Hell Bells', 'Bells from somewhere very deep', { ...solo, 'a.wave': 'Sine', 'a.shape': 70, 'a.voices': 1, 'flt.on': 0, ...RING(666, 45), ...OCT('Octave down', 50), ...REV(90, 45, 80) }),
  P('SCARY', 'Demon Choir', 'A choir of something that should not sing', { 'a.voices': 7, 'a.detune': 45, 'flt.type': 'Vowel', 'flt.cutoff': 400, 'flt.res': 50, ...OCT('Octave down', 70), ...REV(90, null, 75), ...PHA(.05) }),
  P('SCARY', 'Broken Machine', 'Glitched, rusty and mechanical', { 'a.wave': 'Pulse', 'a.shape': 60, ...CRU(4, 70), ...RING(310, 40), ...FLA(null, null, 90) }),
  /* ---------------------------------------------------------------- PADS */
  P('PADS', 'Warm Pad', 'Soft warm pad that blooms slowly', { 'a.voices': 5, 'a.detune': 25, 'b.wave': 'Saw', 'b.pitch': 12, 'b.level': 30, 'b.voices': 3, 'flt.type': 'Lowpass', 'flt.cutoff': 900, 'flt.res': 10,
    'flt.env': 40, ...shape('SWELL'), glide: 120, ...CHO(), ...REV(70, 40) }),
  P('PADS', 'Glass Pad', 'Glassy, bell-like pad with shimmer', { 'a.wave': 'Triangle', 'a.shape': 50, 'b.wave': 'Sine', 'b.pitch': 24, 'b.level': 30, 'sub.level': 0, 'flt.type': 'Lowpass', 'flt.cutoff': 6000, 'flt.env': 0,
    ...SHM(null, null, 35), ...REV(null, 30) }),
  P('PADS', 'Choir Pad', 'An "aah" choir from a vowel filter', { 'a.voices': 5, 'b.wave': 'Saw', 'b.pitch': 12, 'b.voices': 5, 'b.level': 40, 'flt.type': 'Vowel', 'flt.cutoff': 600, 'flt.res': 40, 'flt.env': 30,
    ...shape('SLOW'), ...CHO(), ...REV(80, 40) }),
  P('PADS', 'Dark Pad', 'Low, murky and huge', { 'b.wave': 'Pulse', 'b.pitch': -12, 'flt.cutoff': 350, 'flt.res': 25, 'flt.env': 20, ...shape('SLOW'), char: 'VHS', ...REV(85, 40, 80) }),
  P('PADS', 'Synth Strings', 'Lush strings with a slow ensemble shimmer', { 'a.voices': 7, 'a.detune': 30, 'b.on': 0, 'flt.type': 'Lowpass', 'flt.cutoff': 2600, 'flt.env': 20, ...shape('BLOOM'),
    ...vib(6, 900, 5), ...CHO(null, 60), ...REV(70, 35) }),
  P('PADS', 'Rising Pad', 'Opens over four seconds on every chord', { 'a.wave': 'Super', 'a.shape': 60, 'flt.cutoff': 300, 'flt.res': 25, 'flt.env': 70, ...shape('RISE'), ...REV(null, 35) }),
  P('PADS', 'Wah Pad', 'A pad that talks: opens and closes on each chord', { 'a.voices': 5, 'flt.type': 'Ladder 12', 'flt.cutoff': 500, 'flt.res': 50, 'flt.env': 70, ...shape('WAH'), ...CHO(), ...REV() }),
  P('PADS', 'Aurora', 'Slowly swirling light, wide and endless', { 'a.wave': 'Super', 'a.detune': 40, ...PHA(.08, 80, 50), ...SHM(null, null, 35), 'fx.width': 170 }),
  P('PADS', 'Cathedral', 'An organ-like wall in an endless stone hall', { 'b.pitch': 12, 'flt.type': 'Lowpass', 'flt.cutoff': 2000, ...REV(100, 55, 30), ...OCT(null, 25) }),
  /* ---------------------------------------------------------------- KEYS & PLUCKS */
  P('KEYS & PLUCKS', 'Glass Pluck', 'A short glassy pluck with echoes', { 'a.wave': 'Sine', 'a.shape': 30, 'b.wave': 'Triangle', 'b.pitch': 12, 'flt.type': 'Lowpass', 'flt.cutoff': 900, 'flt.env': 80, ...shape('PLUCK'),
    ...DLY('1/8 dotted', 30), ...REV(null, 25) }),
  P('KEYS & PLUCKS', 'Bells', 'Metallic bell tones that ring out', { ...solo, 'a.wave': 'Sine', 'a.shape': 45, 'a.voices': 1, 'b.on': 1, 'b.wave': 'Sine', 'b.pitch': 19, 'b.level': 25, 'flt.on': 0,
    ...RING(880, 20), ...REV(75, 35) }),
  P('KEYS & PLUCKS', 'Chord Stab', 'A punchy stab: bright, then shut', { 'a.voices': 5, 'b.wave': 'Saw', 'b.pitch': 12, 'b.voices': 3, 'flt.cutoff': 600, 'flt.env': 90, ...shape('SNAP', 70), ...REV(null, 20) }),
  P('KEYS & PLUCKS', 'E-Keys', 'Soft electric-piano-like keys', { 'a.wave': 'Triangle', 'a.shape': 20, 'a.voices': 1, 'b.wave': 'Sine', 'b.pitch': 12, 'b.level': 35, 'flt.type': 'Lowpass', 'flt.cutoff': 2200, 'flt.env': 40,
    ...shape('PLUCK', 60), ...CHO(40) }),
  P('KEYS & PLUCKS', 'Harp', 'A bright plucked harp', { 'a.wave': 'Pulse', 'a.shape': 50, 'b.on': 0, 'flt.cutoff': 700, 'flt.env': 90, ...shape('PLUCK', 150), ...SHM(null, null, 20), ...REV(null, 25) }),
  P('KEYS & PLUCKS', 'Step Keys', 'The filter steps in rhythm on every chord', { 'flt.cutoff': 500, 'flt.res': 35, 'flt.env': 70, ...shape('STEPS'), ...DLY(null, 25) }),
  /* ---------------------------------------------------------------- BASS */
  P('BASS', 'Sub Bass', 'Deep, clean, round sub', { ...solo, 'a.wave': 'Sine', 'a.voices': 1, 'sub.level': 60, 'flt.type': 'Lowpass', 'flt.cutoff': 400, 'flt.env': 0, space: 0 }),
  P('BASS', 'Reese', 'Dark, detuned, rolling bass', { 'a.detune': 30, 'b.wave': 'Saw', 'b.pitch': 0, 'b.fine': -15, 'b.voices': 3, 'b.level': 70, 'flt.cutoff': 500, 'flt.res': 20, 'flt.drive': 40,
    ...PHA(.1), ...OCT(null, 40), space: 5 }),
  P('BASS', 'Acid Bass', 'The squelchy 303-style bass', { ...solo, 'a.voices': 1, 'flt.cutoff': 250, 'flt.res': 85, 'flt.env': 80, 'flt.drive': 60, ...shape('PLUCK'), ...DLY(null, 15) }),
  P('BASS', 'Growl Bass', 'FM growl with grit, heavy and low', { 'a.wave': 'Sine', 'a.shape': 75, 'a.voices': 1, 'b.wave': 'Saw', 'b.pitch': -12, 'b.level': 40, 'flt.cutoff': 800, 'flt.res': 35,
    ...shape('WAH', 140), ...DRV(50) }),
  /* ---------------------------------------------------------------- RETRO */
  P('RETRO', '8-Bit Hero', 'Chiptune hero chords', { 'a.wave': 'Pulse', 'a.shape': 0, 'a.voices': 1, 'b.shape': 60, 'b.pitch': 12, 'b.level': 20, 'sub.level': 0, 'flt.on': 0, char: '8-bit', ...vib(25, 400, 6) }),
  P('RETRO', 'Chip Lead', 'Bright 8-bit square lead with a late vibrato', { ...solo, 'a.wave': 'Pulse', 'a.shape': 25, 'a.voices': 1, 'flt.on': 0, char: '8-bit', ...vib(30, 350, 6), space: 15 }),
  P('RETRO', 'VHS Lead', 'Warbly tape lead, nostalgic and a little seasick', { 'a.detune': 30, 'flt.type': 'Lowpass', 'flt.cutoff': 2000, char: 'VHS', ...CHO(), ...REV(null, 30) }),
  P('RETRO', 'Lo-Fi Tape', 'Worn cassette warmth', { 'flt.type': 'Lowpass', 'flt.cutoff': 1600, char: 'VHS', ...CRU(9, 35), ...CHO(), ...REV() }),
  P('RETRO', '80s Poly', 'The classic 80s polysynth', { 'a.detune': 18, 'b.pitch': 0, 'b.fine': 8, 'b.shape': 40, 'flt.type': 'Ladder 12', 'flt.cutoff': 1600, 'flt.env': 45, ...shape('BLOOM', 200),
    ...CHO(null, 60), ...REV(null, 30) }),
  P('RETRO', 'Arcade Organ', 'A cheap arcade organ with tremolo', { 'a.wave': 'Pulse', 'a.shape': 50, 'a.voices': 1, 'b.shape': 50, 'b.pitch': 12, 'b.level': 50, 'flt.on': 0, char: '8-bit',
    'lfo.on': 1, 'lfo.target': 'Volume', 'lfo.depth': 30, 'lfo.rate': 6 }),
  P('RETRO', 'Sci-Fi', 'Laser-gun science fiction', { 'a.wave': 'Pulse', 'a.shape': 30, 'a.voices': 1, char: '8-bit', 'lfo.shape': 'Saw', 'lfo.on': 1, 'lfo.target': 'Pitch', 'lfo.rate': 2, 'lfo.depth': 50, ...DLY(null, 30) }),
];
/* the effects' newer sides: each space (ROOM, HALL, PLATE, CATHEDRAL) and pre-delay, TAPE echoes, FUZZ, a lower CRUSH rate, a shimmer
   that climbs in fifths too. Kept apart so each preset above still reads simply */
const CATH = { 'fx.rev.type': 'Cathedral' }, PLATE = { 'fx.rev.type': 'Plate' }, ROOM = { 'fx.rev.type': 'Room' }, pre = ms => ({ 'fx.rev.pre': ms });
const TAPE = { 'fx.dly.mode': 'Tape' }, FUZZ = { 'fx.drv.mode': 'Fuzz' }, FIFTH = { 'fx.shm.mode': 'Octave and fifth' }, RATE = hz => ({ 'fx.cru.rate': hz });
const MORE = {
  'Seraph': pre(30), 'Halo': pre(30), 'Ascension': CATH, 'Choir of Light': { ...CATH, ...FIFTH }, 'Glass Angel': PLATE, 'Celestial Flute': pre(40),
  'Holy Saw': CATH, 'Gabriel': { ...CATH, ...FIFTH }, 'Starlight': pre(30), 'Heaven': { ...CATH, ...FIFTH },
  'Velvet': PLATE, 'Silk': pre(40), 'Golden Hour': { ...PLATE, ...TAPE }, 'Bloom Lead': { ...PLATE, ...pre(30) }, 'Rain Glass': PLATE, 'Violin Glide': pre(45), 'Dream': TAPE,
  'Supersaw Lead': { ...PLATE, ...pre(40) }, 'Soft Saw Lead': PLATE, 'Glide Lead': pre(50), 'Brass Lead': ROOM, 'Whistle Lead': pre(40), 'Synthwave Lead': PLATE,
  'Fuzz Lead': FUZZ, 'Octave Lead': PLATE, 'Pluck Lead': ROOM,
  'Theremin': pre(60), 'Ghost Signal': CATH, 'Hollow Whisper': CATH, 'Séance': CATH, 'Frozen': CATH, 'Music Box': ROOM, 'Wraith': CATH,
  'Lost Radio': { ...TAPE, ...RATE(6000) }, 'Haunted': CATH, 'Fog': CATH,
  'Banshee': CATH, 'Demon Lead': FUZZ, 'Hellfire': { ...FUZZ, ...TAPE }, 'Doom': { ...FUZZ, ...CATH }, 'Butcher': { ...FUZZ, ...ROOM, ...RATE(8000) },
  'Possessed': CATH, 'Nightmare': { ...CATH, ...TAPE }, 'Hell Bells': CATH, 'Demon Choir': CATH, 'Broken Machine': RATE(4000),
  'Glass Pad': { ...PLATE, ...FIFTH }, 'Choir Pad': CATH, 'Dark Pad': CATH, 'Aurora': FIFTH, 'Cathedral': CATH,
  'Glass Pluck': PLATE, 'Bells': PLATE, 'Chord Stab': ROOM, 'Harp': PLATE,
  'VHS Lead': { ...DLY('1/8 dotted', 16, 35, 40), ...TAPE }, 'Lo-Fi Tape': { ...RATE(16000), ...DLY('1/4', 18, 30, 35), ...TAPE, ...ROOM }, '80s Poly': PLATE,
  'Arcade Organ': { ...REV(25, 14), ...ROOM }, 'Sci-Fi': TAPE,
};
window.OBS_SYNTH_PRESETS.forEach(p => { if (MORE[p.name]) p.v = Object.assign({}, p.v, MORE[p.name], { trim: p.v.trim }); });
const unknown = Object.keys(MORE).filter(n => !window.OBS_SYNTH_PRESETS.some(p => p.name === n)); if (unknown.length) console.warn('MORE: no preset called', unknown);
})();
</script>
