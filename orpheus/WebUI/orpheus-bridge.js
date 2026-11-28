/* =============================================================================
   ORPHEUS · native bridge (the VST3 and the app)
   -----------------------------------------------------------------------------
   The plugin adds this file at the end of index.html. In a plain browser it
   does nothing, so the same page still plays on its own (Web Audio).

   In the plugin the page only shows and edits; the C++ engine plays:
   - every SIREN setting goes to the plugin (a host parameter); DAW automation
     and loaded projects come back and move the knobs
   - the ORPHEUS song goes to the plugin whenever it changes; PLAY / STOP / the
     playhead drive the plugin's player, which reports where it is (also when the
     DAW plays)
   - clicking dots and keys, and SIREN's KEYS, play the plugin's SIREN
   One native function, "orpheusCall" (Source/PluginEditor.cpp); events: "orph".
   ============================================================================= */
(function () {
'use strict';
const J = window.__JUCE__, K = window.__sirenKit;
if (!J || !J.backend || !K) return;

/* ------------------------------------------------------------ calls */
let nextId = 1;
const waiting = new Map();
J.backend.addEventListener('__juce__complete', ({ promiseId, result }) => {
  const done = waiting.get(promiseId); if (done) { waiting.delete(promiseId); done(result); }
});
function call(cmd, ...args) {
  return new Promise(resolve => {
    const id = 1e6 + nextId++;
    waiting.set(id, resolve);
    try { J.backend.emitEvent('__juce__invoke', { name: 'orpheusCall', params: [cmd, ...args], resultId: id }); }
    catch (e) { waiting.delete(id); resolve(null); }
    setTimeout(() => { if (waiting.has(id)) { waiting.delete(id); resolve(null); } }, 15000);
  });
}
const send = (cmd, ...args) => { try { J.backend.emitEvent('__juce__invoke', { name: 'orpheusCall', params: [cmd, ...args], resultId: 0 }); } catch (_) {} };
window.orpheus = { call };                                    // handy from the dev console

/* ------------------------------------------------------------ settings: page -> plugin, plugin -> page */
const known = new Set();
let applying = 0;
function applyValues(vals) {
  applying++;
  try { for (const id in vals) if (K.P[id]) K.set(id, vals[id], 'plugin'); }
  finally { applying--; }
}
K.onChange((id, v, from) => {
  if (applying || from === 'plugin' || !known.has(id)) return;
  send('set', id, v, K.fxNoTrails ? 1 : 0);                    // (a preset loading: what it switches off stops at once)
});

/* ------------------------------------------------------------ the song */
let lastSong = '', ready = false;                             // ready: the plugin's state has been loaded into the page
function sendSong(force) {
  if (!ready || !K.orphSave) return;
  const s = JSON.stringify(K.orphSave());
  if (!force && s === lastSong) return;
  lastSong = s; send('song', s, K.orphMarks ? K.orphMarks() : []);
}
setInterval(() => sendSong(false), 300);

/* ------------------------------------------------------------ the player and auditioning */
K.native = {
  play: ph => send('transport', 'play', +ph || 0),
  stop: () => send('transport', 'stop', 0),
  seek: ph => send('transport', 'seek', +ph || 0),
};
const notes4 = n => (Array.isArray(n) ? n : [n]).slice(0, 4).map(Number);
K.noteOn = (n, v, t, l, hold) => send('audition', notes4(n), +v || .8, hold ? 1 : 0);
K.noteOff = () => send('auditionOff');
K.setPitches = n => send('auditionPitches', notes4(n));
K.filterKick = () => {};

/* SIREN's KEYS: a key plays its CHORD (the plugin plays the notes it is given) */
const S = window.obsSynth = window.obsSynth || {};
const down = new Map();
S.noteOut = (m, isOn, vel) => {
  if (isOn) {
    const ns = (K.chordNotes ? K.chordNotes(m) : [m]).filter(x => x >= 0 && x <= 127);
    (down.get(m) || []).forEach(x => send('note', x, 0, 0));
    down.set(m, ns); ns.forEach(x => send('note', x, 1, +vel || .9));
  } else { (down.get(m) || [m]).forEach(x => send('note', x, 0, 0)); down.delete(m); }
};
S.bendOut = st => send('bend', +st || 0);
S.modOut = v => send('mod', +v || 0);
S.levelRoll = target => send('levelRoll', +target || -23.5);

/* ------------------------------------------------------------ the UI's own small things (the preset's name) */
let lastX = '';
setInterval(() => {
  if (!ready) return;
  const x = JSON.stringify({ preset: K.presetName ? K.presetName() : null });
  if (x !== lastX) { lastX = x; send('extras', x); }
}, 1000);

/* ------------------------------------------------------------ from the plugin */
function loadState(st) {
  if (!st) { ready = true; return; }
  (st.ids || []).forEach(id => known.add(id));
  applyValues(st.values || {});
  if (st.song && K.orphLoad) { try { K.orphLoad(JSON.parse(st.song)); } catch (_) {} }
  ready = true;
  lastSong = K.orphSave ? JSON.stringify(K.orphSave()) : '';
  if (!st.song) sendSong(true);                                 // a new ORPHEUS: the plugin plays what the page shows
  try { const x = st.extras ? JSON.parse(st.extras) : null; if (x && x.preset && K.setPresetName) K.setPresetName(x.preset[0], x.preset[1]); } catch (_) {}
  lastX = JSON.stringify({ preset: K.presetName ? K.presetName() : null });
}
J.backend.addEventListener('orph', p => {
  try {
    if (!p) return;
    if (p.state) loadState(p.state);
    if (p.params) applyValues(p.params);
    if (p.pos && K.orphNative) K.orphNative.pos(p.pos.ph, p.pos.play, p.pos.host);
    if (p.roll != null && isFinite(p.roll)) K.set('syn.trim', p.roll, 'dice');
  } catch (e) { console.error('[orpheus]', e); }
});
call('hello').then(loadState);
})();
