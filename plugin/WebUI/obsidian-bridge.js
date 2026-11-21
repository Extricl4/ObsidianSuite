/* =============================================================================
   OBSIDIAN SUITE · native bridge
   -----------------------------------------------------------------------------
   The plugin injects this file (and obsidian-native.css) at the end of
   ui/obsidian-demo.html. In a normal browser it does nothing, so the same page
   still works as the demo.

   It connects the page to the audio engine through ONE native function,
   "obsidianCall" (see plugin/Source/Editor/UiBridge.cpp):

       call('setParam', 'amp.gain', 0.62)        -> Promise
       on('obs:params', values => ...)           <- events from the plugin

   HOW IT STAYS IN SYNC
   * Every control is a "binding": read() returns its value(s) as 0..1, write()
     shows value(s) coming from the plugin. Bindings are listed in setupBindings().
   * After any mouse/keyboard input the bridge reads every binding and sends only
     what changed (with begin/end gestures while dragging, so DAW automation
     records cleanly). Values coming from the plugin are written without echoing.

   ADDING A KNOB / SWITCH
   1. Add the parameter in plugin/Source/Core/Params.cpp.
   2. Add one line in setupBindings() below, e.g.  knob('my.param', someKnobEl).
   That's all: presets, DAW automation, MIDI learn and undo-safe state follow.
   ============================================================================= */
(function () {
'use strict';
const J = window.__JUCE__;
if (!J || !J.backend) return;                       // plain browser: demo mode
window.__OBS_NATIVE__ = true;

/* --------------------------------------------------------------------- basics */
const $ = (s, r = document) => r.querySelector(s), $$ = (s, r = document) => [...r.querySelectorAll(s)];
const clamp = (x, a, b) => Math.max(a, Math.min(b, x));
const MINUS = '−';
const sgn = (v, d) => (v > 0 ? '+' : v < 0 ? MINUS : '') + Math.abs(v).toFixed(d);
const fmt = {
  dB: (v, d = 1) => sgn(+v.toFixed(d), d) + ' dB',
  hz: f => f >= 1000 ? (f / 1000).toFixed(f >= 10000 ? 1 : 2) + ' kHz' : Math.round(f) + ' Hz',
  ms: x => x >= 1000 ? (x / 1000).toFixed(2) + ' s' : (x < 10 ? x.toFixed(1) : Math.round(x)) + ' ms',
  pct: x => Math.round(x) + '%',
  pan: x => Math.abs(x) < 1 ? 'C' : (x < 0 ? 'L' : 'R') + Math.round(Math.abs(x)),
};
const txt = el => (el ? el.textContent.trim() : '');
const byText = (sel, t, root) => $$(sel, root).find(e => txt(e) === t);
function safe(name, fn) { try { return fn(); } catch (e) { console.error('[obsidian] ' + name + ':', e); } }
function esc(s) { return String(s == null ? '' : s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c])); }
function say(msg) { if (typeof toast === 'function') toast(msg); }

/* ----------------------------------------------------------------- transport */
let nextId = 1;
const waiting = new Map();
J.backend.addEventListener('__juce__complete', ({ promiseId, result }) => {
  const done = waiting.get(promiseId);
  if (done) { waiting.delete(promiseId); done(result); }
});
function call(cmd, ...args) {
  return new Promise(resolve => {
    const id = 1e6 + nextId++;                        // keep clear of JUCE's own counter
    waiting.set(id, resolve);
    try { J.backend.emitEvent('__juce__invoke', { name: 'obsidianCall', params: [cmd, ...args], resultId: id }); }
    catch (e) { waiting.delete(id); resolve(null); }
    setTimeout(() => { if (waiting.has(id)) { waiting.delete(id); resolve(null); } }, 20000);
  });
}
const on = (ev, fn) => J.backend.addEventListener(ev, p => safe(ev, () => fn(p)));
window.obsidian = { call, on };                      // handy from the dev console

/* ---------------------------------------------------- parameters (0..1 <-> real) */
const META = {};            // id -> { min, max, def, skew, log, type, choices, name }
const vals = {};            // id -> latest 0..1 value
function toReal(id, n) {
  const m = META[id]; if (!m) return n;
  n = clamp(+n || 0, 0, 1);
  if (m.type === 'bool') return n >= .5 ? 1 : 0;
  if (m.type === 'int' || m.type === 'choice') return Math.round(m.min + (m.max - m.min) * n);
  if (m.log) return m.min * Math.pow(m.max / m.min, n);
  if (m.skew !== 1 && n > 0) n = Math.exp(Math.log(n) / m.skew);
  return m.min + (m.max - m.min) * n;
}
function toNorm(id, v) {
  const m = META[id]; if (!m) return v;
  if (m.type === 'bool') return v >= .5 ? 1 : 0;
  if (m.log) return clamp(Math.log(v / m.min) / Math.log(m.max / m.min), 0, 1);
  let n = clamp((v - m.min) / (m.max - m.min), 0, 1);
  if (m.type !== 'int' && m.type !== 'choice' && m.skew !== 1 && n > 0) n = Math.pow(n, m.skew);
  return n;
}

/* ------------------------------------------------------------------- bindings */
/* A binding: { ids:[...], read() -> {id:norm}, write({id:norm}) }                */
const bindings = new Set();
const groupOf = new Map();    // id -> binding
const last = {};              // what the UI last showed / sent, per id
let applying = 0;             // > 0 while the plugin is writing into the UI

function addBinding(b) {
  bindings.add(b);
  b.ids.forEach(id => groupOf.set(id, b));
  safe('bind', () => { const r = b.read(); for (const id in r) last[id] = r[id]; });
  return b;
}
function dropBinding(b) { bindings.delete(b); b.ids.forEach(id => { if (groupOf.get(id) === b) groupOf.delete(id); }); }
function single(id, read, write) { return addBinding({ ids: [id], read: () => ({ [id]: read() }), write: v => write(v[id]) }); }

// knob formatting for knobs the page itself doesn't format
const _knobHook = window.knobHook;
window.knobHook = function (k, v) {
  if (_knobHook) _knobHook(k, v);
  if (k._fmt) { const t = k._fmt(v); const val = k._val || k.parentElement.querySelector(':scope > .kn-val'); if (val && t != null) val.textContent = t; }
};
function knob(id, k, format) {
  if (!k) return null;
  k.dataset.param = id;
  if (vals[id] !== undefined) setKnob(k, clamp(vals[id], 0, 1));
  if (format) { k._fmt = v => format(toReal(id, v)); window.knobHook(k, +k.dataset.v); }
  return single(id, () => +k.dataset.v, n => setKnob(k, clamp(n, 0, 1)));
}
function intKnob(id, k) {             // stepped knob (transpose, semitones): report the snapped value
  if (!k) return null;
  k.dataset.param = id;
  const m = META[id] || { min: 0, max: 1 }, steps = Math.max(1, m.max - m.min);
  return single(id, () => Math.round(+k.dataset.v * steps) / steps, n => setKnob(k, clamp(n, 0, 1)));
}
function tog(id, t, after) {
  if (!t) return null;
  t.dataset.param = id;
  return single(id, () => t.dataset.on === '1' ? 1 : 0, n => { setTog(t, n >= .5); if (after) after(t); });
}
function pushBtn(id, b) {               // SOLO / MUTE / PHASE style buttons (page toggles them on click)
  if (!b) return null;
  b.dataset.param = id;
  return single(id, () => b.dataset.on === '1' ? 1 : 0, n => { if ((b.dataset.on === '1') !== (n >= .5)) b.click(); });
}

/* ------------------------------------------- UI -> plugin: scan for changes */
let scanQueued = false, dragging = false;
const gesture = new Set();
function queueScan() { if (!scanQueued) { scanQueued = true; requestAnimationFrame(scan); } }
function scan() {
  scanQueued = false;
  if (applying) return;
  bindings.forEach(b => {
    let r; try { r = b.read(); } catch (e) { return; }
    for (const id in r) {
      const v = +r[id];
      if (!isFinite(v)) continue;
      if (last[id] === undefined || Math.abs(v - last[id]) > 1e-5) {
        last[id] = v; vals[id] = v;
        if (dragging && !gesture.has(id)) { gesture.add(id); call('beginGesture', id); }
        call('setParam', id, v);
        if (learnArmed && !learnTarget) armLearn(id);
      }
    }
  });
}
['click', 'dblclick', 'wheel', 'input', 'change', 'keyup'].forEach(ev => document.addEventListener(ev, queueScan, true));
document.addEventListener('pointermove', e => { if (e.buttons) queueScan(); }, true);
document.addEventListener('pointerdown', () => { dragging = true; }, true);
addEventListener('pointerup', () => {
  scan();                                                   // final value first, then close the gesture
  gesture.forEach(id => call('endGesture', id)); gesture.clear();
  dragging = false;
  setTimeout(queueScan, 0);                                 // page handlers that run after pointerup
}, true);
setInterval(() => { if (!dragging) scan(); }, 700);        // safety net

/* ---------------------------------------- plugin -> UI: write without echo */
function applyParams(values) {
  const per = new Map();
  for (const id in values) {
    const v = +values[id]; if (!isFinite(v)) continue;
    vals[id] = v;
    if (gesture.has(id)) continue;                          // the user is holding this control
    const b = groupOf.get(id);
    if (b) { if (!per.has(b)) per.set(b, {}); per.get(b)[id] = v; }
  }
  if (!per.size) return;
  applying++;
  try {
    per.forEach((v, b) => {
      safe('write', () => b.write(v));
      safe('reread', () => { const r = b.read(); for (const id in r) last[id] = r[id]; });
    });
  } finally { applying--; }
  afterApply();
}
let afterApplyQueued = false;
function afterApply() {
  if (afterApplyQueued) return; afterApplyQueued = true;
  requestAnimationFrame(() => {
    afterApplyQueued = false;
    safe('eq bubble', () => { const svg = $('.page[data-page="EQ"] svg[width="1000"]'); if (svg && !dragging) svg.parentElement.lastElementChild.style.display = 'none'; });
    safe('slots', refreshSlotStates);
  });
}

/* ============================================================== THE BINDINGS */
let INIT = null;            // initial state from the plugin
function setupBindings() {
  safe('header', () => {
  const hk = $$('.kn').filter(k => !k.closest('.page') && !k.closest('.ov') && !k.closest('#studio'));
  const head = name => hk.find(k => { const s = k.nextElementSibling && k.nextElementSibling.querySelector('span'); return s && txt(s) === name; });
  knob('in.gain', head('INPUT'));
  knob('gate.thresh', head('GATE'));
  intKnob('transpose', head('TRANSPOSE'));
  knob('doubler.time', head('DOUBLER'));
  knob('width', head('WIDTH'));
  knob('out.gain', head('OUTPUT'));
  const headAfter = t => { if (typeof headTog === 'function') headTog(t); };
  tog('gate.on', $('.tog[aria-label="Gate on"]'), headAfter);
  tog('doubler.on', $('.tog[aria-label="Doubler on"]'), headAfter);
  });

  /* ---- page tabs: double-click bypasses a whole section ---- */
  safe('tabs', () => {
  const SECTIONS = { 'SPECIAL FX': 'sfx.on', 'PEDALS': 'pre.on', 'SYNTH': 'synth.on', 'AMP': 'amp.on', 'CAB': 'cab.on', 'EQ': 'eq.on', 'FX': 'post.on' };
  for (const name in SECTIONS)
    single(SECTIONS[name], () => bypassed.has(name) ? 0 : 1, n => { if (n >= .5) bypassed.delete(name); else bypassed.add(name); styleTabs(); });
  });

  /* ---- synth page (synth.js exposes all of its state) ---- */
  if (window.obsSynth) safe('synth', () => {
    const S = window.obsSynth;
    const ids = Object.keys(S.read());
    addBinding({ ids, read: () => S.read(), write: v => S.write(v) });
    // arpeggiator steps travel with the rig (presets), not as parameters
    if (S.onSteps) S.onSteps(from => { if (applying) return; const list = S.getSteps(); call('setSynthSteps', list); if (rig) { rig.misc = rig.misc || {}; rig.misc.synSteps = list.join(','); } });
    // AUDITION / computer keys play the real engine; the synth tab must be on to hear it
    S.noteOut = (note, isOn) => {
      if (isOn && (vals['synth.on'] || 0) < .5) { vals['synth.on'] = 1; call('setParam', 'synth.on', 1); bypassed.delete('SYNTH'); if (typeof styleTabs === 'function') styleTabs(); say('Synth switched on'); }
      call('synthNote', note, !!isOn);
    };
  });

  /* ---- cab page ---- */
  safe('cab', () => {
  const CP = $('.page[data-page="CAB"]');
  if (CP) {
    const counts = {};
    const KEY = { 'POSITION': 'pos', 'DISTANCE': 'dist', 'LEVEL': 'level', 'PAN': 'pan' };
    $$('.kn', CP).forEach(k => {
      const l = knobLabel(k);
      if (KEY[l]) {
        const mic = (counts[l] = (counts[l] || 0) + 1) === 1 ? 'a' : 'b', id = 'cab.' + mic + '.' + KEY[l];
        const f = { pos: x => x.toFixed(2), dist: x => Math.round(x) + ' cm', level: x => fmt.dB(x), pan: fmt.pan }[KEY[l]];
        knob(id, k, f);
      } else if (l === 'LOW CUT') knob('cab.lc', k, fmt.hz);
      else if (l === 'HIGH CUT') knob('cab.hc', k, fmt.hz);
    });
    ['SOLO', 'MUTE', 'PHASE Ø'].forEach((t, i) => {
      const bs = $$('button', CP).filter(b => txt(b) === t);
      const key = ['solo', 'mute', 'phase'][i];
      pushBtn('cab.a.' + key, bs[0]); pushBtn('cab.b.' + key, bs[1]);
    });
    // mic model dropdowns: show the plugin's mic list
    const micBtns = $$('button', CP).filter(b => b.querySelector('path[d="M6 9l6 6 6-6"]') && typeof DD !== 'undefined' && DD.some(g => g.includes(ddText(b)) && g.includes('Dynamic 57')));
    const micGroup = typeof DD !== 'undefined' ? DD.find(g => g.includes('Dynamic 57')) : null;
    const choices = (META['cab.a.mic'] || {}).choices || [];
    if (micGroup && choices) micGroup.splice(0, micGroup.length, ...choices);
    micBtns.slice(0, 2).forEach((b, i) => {
      const id = 'cab.' + (i ? 'b' : 'a') + '.mic';
      b.dataset.param = id;
      single(id, () => Math.max(0, choices.indexOf(ddText(b))) / Math.max(1, choices.length - 1),
                 n => setDDText(b, choices[Math.round(n * (choices.length - 1))] || choices[0]));
    });
  }
  });

  /* ---- EQ page ---- */
  safe('eq', () => {
  $$('.fd').forEach((fd, i) => {
    const id = 'eq.b' + (i + 1);
    fd.dataset.param = id;
    single(id, () => (+fd.dataset.g + 12) / 24, n => setFader(fd, toReal(id, n)));
  });
  const EP = $('.page[data-page="EQ"]');
  if (EP) {
    [['LOW CUT', 'eq.lc', 'LC'], ['HIGH CUT', 'eq.hc', 'HC']].forEach(([label, id, which]) => {
      const k = $$('.kn', EP).find(x => knobLabel(x) === label); if (!k) return;
      k.dataset.param = id;
      single(id, () => +k.dataset.v, n => { const f = toReal(id, n); setCut(which, ex(f)); cutKnob(label, f); });
    });
  }
  });

  /* ---- settings > audio: transpose quality (added by the plugin) ---- */
  safe('hq row', addTransposeQualityRow);
}

/* =========================================================== initial state */
async function start() {
  const st = await call('ready');
  if (!st || !st.params) { say('Obsidian: could not reach the audio engine'); return; }
  INIT = st;
  st.params.forEach(p => { META[p.id] = p; vals[p.id] = p.norm; });
  safe('bindings', setupBindings);
  const all = {}; st.params.forEach(p => all[p.id] = p.norm);
  applyParams(all);
  safe('chrome', () => setupChrome(st));
  safe('presets', () => setupPresets(st));
  safe('racks', () => setupRacks(st));
  safe('amp', () => setupAmpCab(st));
  safe('settings', () => setupSettings(st));
  safe('tempo', () => setupTempo(st));
  safe('tuner', setupTuner);
  safe('midi', () => setupMidi(st));
  safe('learn', setupLearn);
  safe('rig', () => applyRig(st.rig));
  on('obs:params', applyParams);
  on('obs:rig', applyRig);
  on('obs:preset', showPreset);
  on('obs:presets', list => { presetList = list || []; renderPresetList(); });
  on('obs:meters', showMeters);
  on('obs:toast', t => say(t.text));
  on('obs:audio', renderAudio);
  on('obs:audioStats', renderAudio);
  on('obs:midi', renderMappings);
  on('obs:settings', s => { INIT.settings = s; safe('settings', () => renderSettings(s)); });
  showPreset(st.preset);
}

/* ================================================================== chrome */
function setupChrome(st) {
  // version + format in the footer and About
  $$('span').forEach(s => {
    const t = txt(s);
    if (t === 'v0.1') s.textContent = 'v' + st.version;
    if (t === '0.1 (preview)') s.textContent = st.version;
    if (t === 'VST3 · Standalone') s.textContent = st.standalone ? 'Standalone app' : 'VST3 plugin';
    if (t === 'Windows 11 · 64-bit') s.textContent = st.platform;
  });
  // STUDIO lives in the standalone app only (a DAW already records)
  if (!st.standalone) {
    const sb = byText('button', 'STUDIO', $('#f0'));
    if (sb) { const prev = sb.previousElementSibling; sb.classList.add('obs-hidden'); if (prev && !prev.children.length && !txt(prev)) prev.classList.add('obs-hidden'); }
  }
  // About links
  const URLS = { 'CHECK FOR UPDATES': 'https://github.com/Extricl4/ObsidianSuite/releases', 'MANUAL': 'https://github.com/Extricl4/ObsidianSuite/blob/main/plugin/README.md', 'LICENCES': 'https://github.com/Extricl4/ObsidianSuite/blob/main/plugin/LICENSES.md' };
  intercept(b => URLS[txt(b)] && b.closest('.spanel'), b => call('openUrl', URLS[txt(b)]));
  const cu = byText('button', 'CHECK FOR UPDATES'); if (cu) cu.textContent = 'RELEASES';
}

/** Run fn instead of the demo's own click handling for buttons matching test(). */
const intercepts = [];
function intercept(test, fn) { intercepts.push([test, fn]); }
document.addEventListener('click', e => {
  const b = e.target.closest('button'); if (!b) return;
  for (const [test, fn] of intercepts) {
    let hit = false; try { hit = test(b, e); } catch (_) {}
    if (hit) { e.stopImmediatePropagation(); e.preventDefault(); safe('click', () => fn(b, e)); return; }
  }
}, true);

/* ================================================================= meters */
let meterEls = null, cpuSpan = null;
const dbPct = lin => { const db = 20 * Math.log10(Math.max(lin, 1e-6)); return clamp((db + 60) / 60 * 100, 2, 100); };
function showMeters(m) {
  if (window.obsSynth && window.obsSynth.live) safe('synth live', () => {
    window.obsSynth.live({ note: m.synNote, step: m.synStep, level: Math.min(1, (m.synth || 0) * 1.4), src: ['guitar', 'midi', 'hold'][m.synSrc] || 'guitar' });
    if (m.bpm) window.obsSynth.setBpm(m.bpm);
  });
  if (!meterEls) {
    meterEls = (typeof meters !== 'undefined' ? meters : []).map(x => x.d);
    cpuSpan = $$('span').find(s => /^CPU \d+%$/.test(txt(s)));
  }
  if (meterEls.length) {
    meterEls[0].style.height = dbPct(m.in) + '%';
    if (meterEls.length >= 3) { meterEls[meterEls.length - 2].style.height = dbPct(m.outL) + '%'; meterEls[meterEls.length - 1].style.height = dbPct(m.outR) + '%'; }
    else if (meterEls.length === 2) meterEls[1].style.height = dbPct(Math.max(m.outL, m.outR)) + '%';
    if (m.clip) meterEls.forEach(e => { e.style.filter = 'drop-shadow(0 0 4px #ff5a5a)'; clearTimeout(e._c); e._c = setTimeout(() => e.style.filter = '', 600); });
  }
  if (cpuSpan) cpuSpan.textContent = 'CPU ' + Math.round(clamp(m.cpu * 100, 0, 999)) + '%';
  showTuner(m.tuner || {});
  if (m.beat !== undefined && m.beat >= 0) flashBeat(m.beat);
  const busy = $('.obs-busy-global'); if (busy) busy.style.display = m.busy ? '' : 'none';
}
let lastBeat = -1;
function flashBeat(b) {
  if (b === lastBeat) return; lastBeat = b;
  const btn = $('[aria-label="Stop metronome"]'); if (!btn) return;
  btn.style.boxShadow = b === 0 ? '0 0 0 2px var(--accent)' : '0 0 0 1px var(--accent)';
  setTimeout(() => btn.style.boxShadow = '', 90);
}

/* ================================================================== tuner */
const NOTES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
let tunerEls = null;
function setupTuner() {
  const ov = $('.ov[data-ov="tuner"]'); if (!ov) return;
  const big = $$('span', ov).find(s => s.style.fontSize === '110px');
  const bars = big ? [...big.parentElement.nextElementSibling.children] : [];
  tunerEls = { ov, big, bars, cents: $$('span', ov).find(s => / cents$/.test(txt(s))), hz: $$('span', ov).find(s => /^\d+\.\d+ Hz$/.test(txt(s))), mini: $('#livemini') };
  bars.forEach(b => b.dataset.h = b.style.height);
  // Keep everything STILL: fixed widths and heights, so the box never moves when the note changes.
  if (big) {
    Object.assign(big.style, { display: 'inline-block', width: '210px', height: '100px', textAlign: 'center', whiteSpace: 'nowrap', fontVariantNumeric: 'tabular-nums' });
    if (big.firstElementChild) Object.assign(big.firstElementChild.style, { display: 'inline-block', width: '0', overflow: 'visible', textAlign: 'left' });
  }
  [tunerEls.cents, tunerEls.hz].forEach((e, i) => { if (!e) return;
    Object.assign(e.style, { display: 'inline-block', width: i ? '96px' : '96px', textAlign: i ? 'left' : 'right', fontVariantNumeric: 'tabular-nums', whiteSpace: 'nowrap', minHeight: '1.3em' }); });
  if (tunerEls.cents && tunerEls.cents.parentElement) tunerEls.cents.parentElement.style.minHeight = '20px';
  const box = big && big.closest('div[style*="width: 700px"]');
  if (box) {
    const lock = () => { if (ov.style.display === 'block' && box.offsetHeight > 0 && !box.dataset.locked) { box.style.height = box.offsetHeight + 'px'; box.dataset.locked = '1'; } };
    new MutationObserver(() => requestAnimationFrame(lock)).observe(ov, { attributes: true, attributeFilter: ['style'] });
  }
  const mini = tunerEls.mini;
  if (mini) {
    const note = mini.querySelector('div > span'); if (note) Object.assign(note.style, { display: 'inline-block', width: '26px', textAlign: 'center' });
    const sp = mini.querySelectorAll('span'); const last = sp[sp.length - 1];
    if (last) Object.assign(last.style, { display: 'inline-block', width: '28px', textAlign: 'right', fontVariantNumeric: 'tabular-nums' });
  }
  // open / close tell the engine (it mutes the output while tuning if you want)
  const _open = window.openOv, _close = window.closeOv;
  window.openOv = function (n) { _open(n); call('tunerOpen', n === 'tuner'); };
  window.closeOv = function () { _close(); call('tunerOpen', false); };
}
function showTuner(t) {
  if (!tunerEls) return;
  const hz = +t.hz || 0, ok = hz > 20 && (t.conf || 0) > 0.5;
  const ref = typeof refHz !== 'undefined' ? refHz : 440;
  let name = '–', oct = '', cents = 0;
  if (ok) {
    const midi = 69 + 12 * Math.log2(hz / ref), n = Math.round(midi);
    name = NOTES[((n % 12) + 12) % 12]; oct = String(Math.floor(n / 12) - 1); cents = Math.round((midi - n) * 100);
  }
  const visible = tunerEls.ov.style.display === 'block';
  if (visible && tunerEls.big) {
    tunerEls.big.firstChild.textContent = name;
    if (tunerEls.big.firstElementChild) tunerEls.big.firstElementChild.textContent = oct;
    // placeholders instead of empty text, so the line keeps its height
    if (tunerEls.cents) { tunerEls.cents.textContent = ok ? sgn(cents, 0) + ' cents' : '– cents'; tunerEls.cents.style.opacity = ok ? '' : '.35'; }
    if (tunerEls.hz) { tunerEls.hz.textContent = ok ? hz.toFixed(2) + ' Hz' : '– Hz'; tunerEls.hz.style.opacity = ok ? '' : '.35'; }
    const N = tunerEls.bars.length, mid = (N - 1) / 2, pos = Math.round(mid + clamp(cents / 50, -1, 1) * mid);
    tunerEls.bars.forEach((b, i) => {
      const inTune = ok && Math.abs(cents) <= 2;
      const lit = ok && ((i >= Math.min(mid, pos) && i <= Math.max(mid, pos)));
      b.style.background = i === pos && ok ? (inTune ? 'var(--accent)' : 'rgb(var(--cb_f4f1ea))') : lit ? 'var(--accent)' : 'rgb(var(--cb_26262a))';
      b.style.opacity = lit && i !== pos ? '.55' : '1';
      b.style.height = i === pos && ok ? '46px' : b.dataset.h;
      b.style.boxShadow = i === pos && inTune ? '0 0 12px var(--accent)' : 'none';
    });
  }
  const mini = tunerEls.mini;
  if (mini && mini.offsetParent) {
    const sp = mini.querySelectorAll('span');
    const note = mini.querySelector('div > span'); if (note) note.textContent = name;
    const mb = mini.querySelector('div > div');
    if (mb) { const kids = [...mb.children], M = (kids.length - 1) / 2, p = Math.round(M + clamp(cents / 50, -1, 1) * M);
      kids.forEach((b, i) => { b.style.height = i === p && ok ? '10px' : '5px'; b.style.background = i === p && ok ? 'var(--accent)' : 'rgb(var(--cb_333338))'; }); }
    sp[sp.length - 1].textContent = ok ? (cents ? sgn(cents, 0) : '0') : '';
  }
}

/* ================================================================= presets */
let presetList = [], presetCur = {}, presetSel = null, presetFolder = 'All presets', presetQuery = '';
function setupPresets(st) {
  presetList = st.presets || [];
  presetCur = st.preset || {};
  const prev = $('[aria-label="Previous preset"]'), next = $('[aria-label="Next preset"]');
  intercept(b => b === prev, () => guardSwitch(() => call('presetNext', -1)));
  intercept(b => b === next, () => guardSwitch(() => call('presetNext', 1)));
  const top = prev ? prev.parentElement.parentElement : document;
  intercept(b => top.contains(b) && txt(b) === 'SAVE', () => call('presetSave'));
  intercept(b => top.contains(b) && txt(b) === 'SAVE AS', () => saveAsDialog());
  intercept(b => top.contains(b) && txt(b) === 'DELETE', async () => {
    if (presetCur.factory) { say('Factory presets can\'t be deleted'); return; }
    if (await confirmDialog('DELETE PRESET', 'Delete "' + presetCur.name + '"? It goes to the recycle bin, so you can get it back.', 'DELETE')) call('presetDelete', presetCur.id);
  });
  const ov = $('.ov[data-ov="presets"]'); if (!ov) return;
  const rows = $$('button', ov).filter(b => /grid-template-columns: 22px/.test(b.getAttribute('style') || ''));
  const rowTpl = rows[0] ? rows[0].cloneNode(true) : null;
  const rowBox = rows[0] ? rows[0].parentElement : null;
  rows.forEach(r => r.remove());
  ov._rowTpl = rowTpl; ov._rowBox = rowBox;
  const folders = $$('.folder', ov);
  ov._folderTpl = folders[0] ? folders[0].cloneNode(true) : null;
  ov._folderBox = folders[0] ? folders[0].parentElement : null;
  ov._folderAnchor = folders.length ? folders[folders.length - 1].nextSibling : null;
  folders.forEach(f => f.remove());
  if (rowBox) { rowBox.style.overflowY = 'auto'; rowBox.style.minHeight = '0'; }
  const hdr = $$('span', ov).find(s => txt(s) === 'AMP'); if (hdr) { hdr.textContent = 'TYPE'; const t = hdr.nextElementSibling; if (t && txt(t) === 'TAGS') t.textContent = 'SOURCE'; }
  const nf = byText('button', 'NEW FOLDER', ov); if (nf) nf.classList.add('obs-hidden');
  const search = $('#preset-search', ov);
  if (search) search.addEventListener('input', () => { presetQuery = search.value.trim().toLowerCase(); renderPresetList(); });
  intercept(b => b.classList.contains('obs-folder'), b => { presetFolder = b.dataset.folder; renderPresetList(); });
  intercept(b => b.classList.contains('obs-prow'), b => { presetSel = b.dataset.id; markPresetSel(); });
  ov.addEventListener('dblclick', e => { const r = e.target.closest('.obs-prow'); if (r) guardSwitch(() => { call('presetLoad', r.dataset.id); closeOv(); }); });
  const sel = () => presetList.find(p => p.id === presetSel);
  intercept(b => ov.contains(b) && txt(b) === 'LOAD', () => { const p = sel(); if (p) guardSwitch(() => { call('presetLoad', p.id); closeOv(); }); });
  intercept(b => ov.contains(b) && txt(b) === 'LOAD DEFAULT', () => guardSwitch(() => { call('presetInit'); closeOv(); }));
  intercept(b => ov.contains(b) && txt(b) === 'SHOW IN EXPLORER', () => call('presetReveal'));
  intercept(b => ov.contains(b) && txt(b) === 'RENAME', async () => {
    const p = sel(); if (!p) return;
    if (p.factory) { say('Factory presets can\'t be renamed. DUPLICATE it first.'); return; }
    const name = await textDialog('RENAME PRESET', p.name, 'RENAME'); if (name) call('presetRename', p.id, name);
  });
  intercept(b => ov.contains(b) && txt(b) === 'DUPLICATE', async () => {
    const p = sel(); if (!p) return;
    await call('presetLoad', p.id);
    const ok = await call('presetSaveAs', p.name + ' copy', p.category || '');
    if (ok) say('Duplicated as "' + p.name + ' copy"');
  });
  renderPresetList();
}
function guardSwitch(go) {
  if (presetCur.dirty && INIT && INIT.settings && INIT.settings.askBeforeSwitching)
    confirmDialog('UNSAVED CHANGES', '"' + presetCur.name + '" has changes that aren\'t saved. Switch anyway?', 'SWITCH').then(ok => { if (ok) go(); });
  else go();
}
function showPreset(p) {
  if (!p) return; presetCur = p;
  const mark = !INIT || !INIT.settings || INIT.settings.markUnsaved !== false;
  if (typeof setPreset === 'function') setPreset(esc(p.name), p.dirty && mark);
  if (!presetSel) presetSel = p.id;
  renderPresetList();
}
function renderPresetList() {
  const ov = $('.ov[data-ov="presets"]'); if (!ov || !ov._rowTpl) return;
  const cats = [...new Set(presetList.map(p => p.category).filter(Boolean))].sort();
  const folders = [['All presets', presetList.length], ['Factory', presetList.filter(p => p.factory).length], ['My presets', presetList.filter(p => !p.factory).length]]
    .concat(cats.map(c => [c, presetList.filter(p => p.category === c).length]));
  if (ov._folderTpl && ov._folderBox) {
    $$('.obs-folder', ov._folderBox).forEach(x => x.remove());
    const anchor = ov._folderAnchor && ov._folderAnchor.parentNode === ov._folderBox ? ov._folderAnchor : byText('button', 'NEW FOLDER', ov._folderBox);
    folders.forEach(([name, count]) => {
      const f = ov._folderTpl.cloneNode(true); f.classList.remove('folder'); f.classList.add('obs-folder'); f.dataset.folder = name;
      const sp = f.querySelectorAll('span'); sp[0].textContent = name; sp[sp.length - 1].textContent = count;
      const onF = name === presetFolder;
      f.style.background = onF ? 'rgba(255,255,255,.07)' : 'none';
      f.style.color = onF ? 'rgb(var(--cf_f4f1ea))' : 'rgb(var(--cf_a3a3a8))';
      ov._folderBox.insertBefore(f, anchor || null);
    });
  }
  const show = presetList.filter(p => (presetFolder === 'All presets' || (presetFolder === 'Factory' && p.factory) || (presetFolder === 'My presets' && !p.factory) || p.category === presetFolder)
    && (!presetQuery || (p.name + ' ' + p.category).toLowerCase().includes(presetQuery)));
  $$('.obs-prow', ov._rowBox).forEach(x => x.remove());
  $$('.obs-empty', ov._rowBox).forEach(x => x.remove());
  show.forEach(p => {
    const r = ov._rowTpl.cloneNode(true); r.classList.add('obs-prow'); r.dataset.id = p.id;
    const sp = r.querySelectorAll(':scope > span');
    if (sp[1]) sp[1].textContent = p.name;
    if (sp[2]) sp[2].textContent = p.category || '—';
    if (sp[3]) sp[3].textContent = p.factory ? 'Factory' : 'My preset';
    if (sp[0]) sp[0].style.visibility = p.id === presetCur.id ? 'visible' : 'hidden';
    ov._rowBox.appendChild(r);
  });
  if (!show.length) { const e = document.createElement('div'); e.className = 'obs-empty'; e.textContent = presetQuery ? 'No presets match "' + presetQuery + '".' : 'No presets here yet. Use SAVE AS to make one.'; ov._rowBox.appendChild(e); }
  markPresetSel();
}
function markPresetSel() {
  const ov = $('.ov[data-ov="presets"]'); if (!ov || !ov._rowBox) return;
  $$('.obs-prow', ov._rowBox).forEach(r => {
    const s = r.dataset.id === presetSel;
    r.style.background = s ? 'rgba(255,255,255,.07)' : 'none';
    r.style.boxShadow = s ? 'inset 2px 0 0 var(--accent)' : 'none';
  });
  // details column
  const selLbl = $$('span', ov).find(s => txt(s) === 'SELECTED');
  const p = presetList.find(x => x.id === presetSel) || presetCur;
  if (selLbl && p) {
    const nameEl = selLbl.nextElementSibling; if (nameEl) nameEl.textContent = p.name || '';
    const info = nameEl && nameEl.nextElementSibling;
    if (info) {
      const lines = ['Folder · ' + (p.factory ? 'Factory' : 'My presets'), 'Type · ' + (p.category || '—'),
                     p.id === presetCur.id ? 'Loaded now' + (presetCur.dirty ? ' · edited' : '') : 'Double-click to load', ''];
      [...info.children].forEach((c, i) => { c.textContent = lines[i] || ''; c.style.display = lines[i] ? '' : 'none'; });
    }
  }
}
async function saveAsDialog() {
  const cats = [...new Set(['Rhythm', 'Lead', 'Clean', 'Ambient', 'Synth'].concat(presetList.map(p => p.category).filter(Boolean)))];
  const r = await formDialog('SAVE AS', [
    { key: 'name', label: 'Name', value: presetCur.factory || !presetCur.name ? (presetCur.name ? presetCur.name + ' (mine)' : 'My Preset') : presetCur.name },
    { key: 'category', label: 'Type', value: presetCur.category || 'Rhythm', options: cats }], 'SAVE');
  if (r && r.name) call('presetSaveAs', r.name, r.category);
}

/* ------------------------------------------------------------------ dialogs */
function modal(build) {
  return new Promise(resolve => {
    const m = document.createElement('div'); m.className = 'obs-modal';
    const d = document.createElement('div'); d.className = 'obs-dialog'; m.appendChild(d);
    const close = v => { m.remove(); resolve(v); };
    build(d, close);
    m.addEventListener('pointerdown', e => { if (e.target === m) close(null); });
    m.addEventListener('click', e => e.stopPropagation());          // the page's own click handler must not see dialog buttons
    m.addEventListener('keydown', e => { if (e.key === 'Escape') close(null); e.stopPropagation(); });
    (document.getElementById('app') || document.body).appendChild(m);
    const f = d.querySelector('input'); if (f) { f.focus(); f.select(); }
  });
}
function formDialog(title, fields, okText) {
  return modal((d, close) => {
    d.innerHTML = '<h3>' + esc(title) + '</h3>' + fields.map(f => f.options
      ? '<select data-k="' + f.key + '">' + f.options.map(o => '<option' + (o === f.value ? ' selected' : '') + '>' + esc(o) + '</option>').join('') + '</select>'
      : '<input data-k="' + f.key + '" value="' + esc(f.value) + '" maxlength="60" spellcheck="false">').join('')
      + '<div class="obs-row"><button class="obs-btn" data-x>CANCEL</button><button class="obs-btn primary" data-ok>' + esc(okText) + '</button></div>';
    const ok = () => { const r = {}; d.querySelectorAll('[data-k]').forEach(e => r[e.dataset.k] = e.value.trim()); close(r); };
    d.querySelector('[data-ok]').onclick = ok; d.querySelector('[data-x]').onclick = () => close(null);
    d.addEventListener('keydown', e => { if (e.key === 'Enter') ok(); });
  });
}
async function textDialog(title, value, okText) { const r = await formDialog(title, [{ key: 'v', value }], okText); return r && r.v; }
function confirmDialog(title, text, okText) {
  return modal((d, close) => {
    d.innerHTML = '<h3>' + esc(title) + '</h3><p>' + esc(text) + '</p><div class="obs-row"><button class="obs-btn" data-x>CANCEL</button><button class="obs-btn primary" data-ok>' + esc(okText) + '</button></div>';
    d.querySelector('[data-ok]').onclick = () => close(true); d.querySelector('[data-x]').onclick = () => close(false);
  }).then(Boolean);
}

/* =================================================== racks (pedals / FX / special) */
const RACKS = [{ page: 'PEDALS', key: 'pre', add: 'ADD PEDAL', mask: 'pre' }, { page: 'FX', key: 'post', add: 'ADD EFFECT', mask: 'post' }, { page: 'SPECIAL FX', key: 'sfx', add: 'ADD SPECIAL FX', mask: 'sfx' }];
const MODS = {};            // key -> module type
let rig = null;
const rackUi = [];          // per rack: { page, slots:[button], pop, grid, editor, sel, addSlot }
function setupRacks(st) {
  (st.modules || []).forEach(m => MODS[m.key] = m);
  RACKS.forEach((R, ri) => {
    const page = $('.page[data-page="' + R.page + '"]'); if (!page) return;
    const slots = $$('button', page).filter(b => txt(b) === R.add || /^ADD (PEDAL|EFFECT|SPECIAL FX)$/.test(txt(b))).slice(0, 5);
    if (!slots.length) return;
    const ui = { page, slots, sel: -1, addSlot: 0, empty: slots.map(s => s.innerHTML) };
    slots.forEach((s, i) => { s.classList.add('obs-slot'); s.dataset.rack = ri; s.dataset.slot = i; });
    ui.pop = $('.pop', page);
    if (ui.pop) {
      const grid = $$('div', ui.pop).find(d => /grid-template-columns: repeat\(3/.test(d.getAttribute('style') || ''));
      if (grid) { grid.className = 'obs-grid'; grid.removeAttribute('style'); ui.grid = grid; }
      const note = $$('span', ui.pop).find(s => /will show up here/.test(txt(s))); if (note) note.textContent = 'Click to add. Drag cards on the board to reorder.';
      const search = $('input', ui.pop); if (search) search.addEventListener('input', () => renderLibrary(ri));
      ui.pop.addEventListener('click', () => setTimeout(() => renderLibrary(ri), 0));
    }
    // editor strip under the board
    const board = slots[0].parentElement;
    ui.editor = document.createElement('div'); ui.editor.className = 'obs-editor obs-hidden';
    ui.editor.addEventListener('click', e => e.stopPropagation());
    board.parentElement.appendChild(ui.editor);
    ui.column = board.parentElement;
    const hint = $$('span', page).find(s => /Drag to reorder/.test(txt(s))); if (hint) hint.textContent = 'Click a card to edit · drag to reorder · footswitch turns it on / off';
    rackUi[ri] = ui;
  });
  // clicks on slots
  intercept(b => b.classList.contains('obs-slot'), (b, e) => {
    const ri = +b.dataset.rack, si = +b.dataset.slot, ui = rackUi[ri], s = rig && rig.racks[ri][si];
    if (e.target.closest('.obs-x')) { call('setModule', ri, si, ''); if (ui.sel === si) ui.sel = -1; return; }
    if (e.target.closest('.obs-foot')) { const id = RACKS[ri].key + '.' + (si + 1) + '.on'; const n = ((vals[id] ?? 1) >= .5) ? 0 : 1; vals[id] = n; last[id] = n; call('setParam', id, n); refreshSlotStates(); return; }
    if (e.target.closest('.obs-loadfile')) { call('loadModuleFile', ri, si); return; }
    if (s && s.key) { ui.sel = si; renderRack(ri); renderEditor(ri); return; }
    openLibrary(ri, si);
  });
  intercept(b => b.classList.contains('obs-tile') && b.dataset.mod !== undefined, b => {
    const ri = +b.dataset.rack, ui = rackUi[ri];
    call('setModule', ri, ui.addSlot, b.dataset.mod).then(r => { if (r) applyRig(r); });
    ui.sel = ui.addSlot; if (ui.pop) ui.pop.style.display = 'none';
  });
  setupDrag();
}
function openLibrary(ri, si) {
  const ui = rackUi[ri]; if (!ui || !ui.pop) return;
  ui.addSlot = si;
  $$('.pop').forEach(p => p.style.display = 'none');
  ui.pop.style.display = 'block';
  // keep the pop-up next to the slot that asked for it
  const board = ui.slots[0].parentElement, slot = ui.slots[si];
  const left = clamp(slot.offsetLeft - 160, 0, board.offsetWidth - 470);
  ui.pop.style.left = left + 'px';
  renderLibrary(ri);
  const search = $('input', ui.pop); if (search) setTimeout(() => search.focus(), 30);
}
function renderLibrary(ri) {
  const ui = rackUi[ri]; if (!ui || !ui.grid) return;
  const radio = $('.radio', ui.pop);
  const active = radio ? [...radio.children].find(b => typeof isAct2 === 'function' ? isAct2(b) || /background: var\(--accent\)/.test(b.getAttribute('style') || '') : false) : null;
  const filter = active ? txt(active).toLowerCase() : 'all';
  const q = (($('input', ui.pop) || {}).value || '').trim().toLowerCase();
  const cur = rig && rig.racks[ri][ui.addSlot] ? rig.racks[ri][ui.addSlot].key : '';
  const list = Object.values(MODS).filter(m => m.racks.includes(RACKS[ri].mask)
    && (filter === 'all' || m.category === filter || (filter === 'compressor' && m.category === 'dynamics') || (filter === 'modulation' && m.category === 'modulation'))
    && (!q || (m.name + ' ' + m.category + ' ' + m.description).toLowerCase().includes(q)));
  ui.grid.innerHTML = list.map(m => '<button class="obs-tile' + (m.key === cur ? ' on' : '') + '" data-mod="' + esc(m.key) + '" data-rack="' + ri + '" title="' + esc(m.description) + '"><i>' + esc(m.category) + '</i><b>' + esc(m.name) + '</b><small>' + esc(m.description) + '</small></button>').join('')
    || '<div class="obs-empty">Nothing matches. Try ALL or clear the search.</div>';
}
function applyRig(r) {
  if (!r) return; rig = r;
  RACKS.forEach((_, ri) => { safe('rack', () => {
    renderRack(ri);
    const ui = rackUi[ri]; if (!ui) return;
    const s = ui.sel >= 0 ? rig.racks[ri][ui.sel] || {} : {};
    const sig = ui.sel + '|' + (s.key || '') + '|' + (s.fileName || '');
    if (sig !== ui.editorSig) renderEditor(ri);
  }); });
  safe('amp', renderAmp); safe('cab', renderCab);
  safe('misc', () => {
    const m = r.misc || {};
    if (m.bpm && typeof setBpm === 'function') { applying++; try { setBpm(Math.round(+m.bpm)); } finally { applying--; } }
    const n = $('.eqname'); if (n && m.eqPreset) n.textContent = m.eqPreset;
    if (m.beats) markTimeSig(+m.beats);
    const acc = $('.tog[aria-label="Accent first beat"]'); if (acc && m.accent !== undefined) setTog(acc, !!m.accent);
    if (window.obsSynth && window.obsSynth.setSteps) {
      const list = m.synSteps ? String(m.synSteps).split(',').map(Number) : null;
      applying++; try { window.obsSynth.setSteps(list && list.length === 16 ? list : window.obsSynth.defaultSteps()); } finally { applying--; }
    }
  });
}
function renderRack(ri) {
  const ui = rackUi[ri]; if (!ui || !rig) return;
  ui.slots.forEach((b, si) => {
    const s = rig.racks[ri][si] || {}, m = MODS[s.key];
    if (!s.key || !m) {
      if (b.classList.contains('obs-filled')) { b.innerHTML = ui.empty[si]; b.classList.remove('obs-filled', 'obs-sel'); }
      b.title = 'Add a ' + (ri ? 'effect' : 'pedal');
      if (ui.sel === si) { ui.sel = -1; ui.editor.classList.add('obs-hidden'); ui.column.style.gap = ''; }
      return;
    }
    const sig = s.key + '|' + (s.fileName || '');
    if (b.dataset.sig !== sig || !b.classList.contains('obs-filled')) {
      b.innerHTML = '<div class="obs-card"><span class="obs-x" title="Remove">×</span><span class="obs-led"></span>'
        + '<span class="obs-cat">' + esc(m.category) + '</span><span class="obs-name">' + esc(m.name) + '</span>'
        + (m.fileKind === 'pack' ? '<span class="obs-file" title="Playing from the Obsidian pedal pack">' + (s.fileName ? esc(s.fileName) : '&nbsp;') + '</span>'
           : m.fileKind ? '<span class="obs-file obs-loadfile" title="Load a .nam capture">' + esc(s.fileName || 'LOAD CAPTURE…') + '</span>' : '<span class="obs-file">&nbsp;</span>')
        + '<span class="obs-foot" title="Footswitch: on / off"></span></div>';
      b.classList.add('obs-filled'); b.dataset.sig = sig;
    }
    b.classList.toggle('obs-sel', ui.sel === si);
    b.title = m.name + ' — click to edit';
  });
  if (ui.sel >= 0 && !(rig.racks[ri][ui.sel] || {}).key) { ui.sel = -1; ui.editor.classList.add('obs-hidden'); ui.column.style.gap = ''; }
  refreshSlotStates();
}
function refreshSlotStates() {
  rackUi.forEach((ui, ri) => { if (!ui) return; ui.slots.forEach((b, si) => {
    const c = b.querySelector('.obs-card'); if (c) c.classList.toggle('on', (vals[RACKS[ri].key + '.' + (si + 1) + '.on'] ?? 1) >= .5); }); });
}

/* ---- module editor: one knob per parameter ---- */
let knobTpl = null;
function makeKnob(label) {
  if (!knobTpl) {
    const k = $('.page[data-page="CAB"] .kn') || $('.kn');
    knobTpl = k.parentElement.cloneNode(true);
    const kk = $('.kn', knobTpl); delete kk.dataset.param; kk.removeAttribute('data-def');
  }
  const col = knobTpl.cloneNode(true);
  const spans = col.querySelectorAll(':scope > span');
  if (spans[0]) spans[0].textContent = label;
  col.classList.add('obs-ctl');
  return col;
}
const modFmt = p => v => {
  if (p.choices && p.choices.length) return p.choices[Math.round(v)] || '';
  if (p.toggle || p.momentary) return v >= .5 ? 'On' : 'Off';
  switch (p.unit) {
    case 'dB': return fmt.dB(v); case 'ms': return fmt.ms(v); case 'Hz': return fmt.hz(v); case '%': return fmt.pct(v);
    case 'st': return sgn(Math.round(v), 0) + ' st'; case 's': return v.toFixed(v < 10 ? 1 : 0) + ' s'; case 'x': return v.toFixed(v < 10 ? 1 : 0) + (p.name === 'Ratio' ? ':1' : '×');
    default: return v.toFixed(1);
  }
};
const modReal = (p, t) => {
  t = clamp(t, 0, 1);
  if (p.choices && p.choices.length) return Math.round(t * (p.choices.length - 1));
  if (p.toggle || p.momentary) return t >= .5 ? 1 : 0;
  if (p.skew !== 1) t = Math.pow(t, p.skew);
  return p.min + (p.max - p.min) * t;
};
const editorBindings = [[], [], []];
function renderEditor(ri) {
  const ui = rackUi[ri]; if (!ui) return;
  editorBindings[ri].forEach(dropBinding); editorBindings[ri] = [];
  const s = rig && rig.racks[ri][ui.sel], m = s && MODS[s.key];
  ui.editorSig = ui.sel + '|' + (s && s.key || '') + '|' + (s && s.fileName || '');
  if (!m) { ui.editor.classList.add('obs-hidden'); ui.column.style.gap = ''; return; }
  ui.editor.classList.remove('obs-hidden');
  ui.column.style.gap = '26px';                     // make room for the editor under the board
  ui.editor.innerHTML = '<div class="obs-head"><span class="obs-title">' + esc(m.name) + '</span><span class="obs-desc">' + esc(m.description) + '</span>'
    + (m.fileKind === 'pack' ? (s.fileName ? '<span class="obs-desc" title="Playing from the Obsidian pedal pack">▶ ' + esc(s.fileName) + '</span>' : '')
       : m.fileKind ? '<button class="obs-btn obs-modfile">' + esc(s.fileName ? 'CAPTURE: ' + s.fileName : 'LOAD CAPTURE…') + '</button>' : '')
    + '<button class="obs-btn obs-moddefaults" title="Back to this pedal\'s starting settings">RESET</button></div><div class="obs-knobs"></div>';
  const row = $('.obs-knobs', ui.editor);
  m.params.forEach((p, k) => {
    const id = RACKS[ri].key + '.' + (ui.sel + 1) + '.p' + (k + 1);
    const f = modFmt(p);
    if (p.choices && p.choices.length && p.choices.length <= 6) {
      const box = document.createElement('div'); box.className = 'obs-ctl';
      box.innerHTML = '<span>' + esc(p.name) + '</span><div class="obs-segs">' + p.choices.map((c, i) => '<button data-i="' + i + '">' + esc(c) + '</button>').join('') + '</div>';
      row.appendChild(box);
      const bs = $$('button', box), n = p.choices.length - 1;
      const show = v => bs.forEach((b, i) => b.classList.toggle('on', i === Math.round(clamp(v, 0, 1) * n)));
      bs.forEach((b, i) => b.addEventListener('click', () => { box.dataset.v = n ? i / n : 0; show(+box.dataset.v); }));
      box.dataset.v = vals[id] ?? p.defNorm; show(+box.dataset.v);
      editorBindings[ri].push(single(id, () => +box.dataset.v, v => { box.dataset.v = v; show(v); }));
      return;
    }
    if (p.toggle || p.momentary) {
      const box = document.createElement('div'); box.className = 'obs-ctl';
      box.innerHTML = '<span>' + esc(p.name) + '</span><button class="obs-btn' + (p.momentary ? ' hold' : '') + '">' + (p.momentary ? 'HOLD' : 'OFF') + '</button>';
      row.appendChild(box);
      const b = $('button', box);
      const show = v => { b.classList.toggle('primary', !p.momentary && v >= .5); b.classList.toggle('down', p.momentary && v >= .5); if (!p.momentary) b.textContent = v >= .5 ? 'ON' : 'OFF'; };
      box.dataset.v = p.momentary ? 0 : (vals[id] ?? p.defNorm); show(+box.dataset.v);
      if (p.momentary) {
        const set = v => { box.dataset.v = v; show(v); vals[id] = v; last[id] = v; call('setParam', id, v); };
        b.addEventListener('pointerdown', e => { e.stopPropagation(); b.setPointerCapture(e.pointerId); set(1); });
        b.addEventListener('pointerup', () => set(0)); b.addEventListener('pointercancel', () => set(0));
        b.addEventListener('keydown', e => { if (e.key === ' ' || e.key === 'Enter') set(1); });
        b.addEventListener('keyup', e => { if (e.key === ' ' || e.key === 'Enter') set(0); });
      } else b.addEventListener('click', () => { box.dataset.v = +box.dataset.v >= .5 ? 0 : 1; show(+box.dataset.v); });
      editorBindings[ri].push(p.momentary ? single(id, () => last[id] ?? 0, v => show(v)) : single(id, () => +box.dataset.v, v => { box.dataset.v = v; show(v); }));
      return;
    }
    const col = makeKnob(p.name); row.appendChild(col);
    const kn = $('.kn', col); kn.dataset.param = id;
    kn._fmt = v => f(modReal(p, v));
    kn.dataset.def = p.defNorm;
    setKnob(kn, vals[id] ?? p.defNorm);
    editorBindings[ri].push(single(id, () => +kn.dataset.v, v => setKnob(kn, clamp(v, 0, 1))));
  });
  const lf = $('.obs-modfile', ui.editor); if (lf) lf.onclick = () => call('loadModuleFile', ri, ui.sel);
  $('.obs-moddefaults', ui.editor).onclick = () => {
    const vs = {}; m.params.forEach((p, k) => vs[RACKS[ri].key + '.' + (ui.sel + 1) + '.p' + (k + 1)] = p.defNorm);
    for (const id in vs) { vals[id] = vs[id]; call('setParam', id, vs[id]); }
    applyParams(vs);
  };
  renderRack(ri);
}

/* ---- drag a card onto another slot to reorder ---- */
function setupDrag() {
  let d = null;
  document.addEventListener('pointerdown', e => {
    const b = e.target.closest('.obs-slot.obs-filled');
    if (!b || e.target.closest('.obs-x,.obs-foot,.obs-loadfile') || e.button !== 0) return;
    d = { b, x: e.clientX, y: e.clientY, ri: +b.dataset.rack, from: +b.dataset.slot, moved: false };
  });
  addEventListener('pointermove', e => {
    if (!d) return;
    if (!d.moved && Math.hypot(e.clientX - d.x, e.clientY - d.y) < 8) return;
    if (!d.moved) {
      d.moved = true; d.b.classList.add('obs-dragging');
      d.ghost = d.b.cloneNode(true); d.ghost.classList.add('obs-ghost'); d.ghost.classList.remove('obs-dragging');
      const app = $('#app'); app.appendChild(d.ghost);
    }
    const app = $('#app'), ar = app.getBoundingClientRect(), sc = ar.width / 1360;
    d.ghost.style.left = ((e.clientX - ar.left) / sc - 75) + 'px'; d.ghost.style.top = ((e.clientY - ar.top) / sc - 100) + 'px';
    const ui = rackUi[d.ri];
    d.to = -1;
    ui.slots.forEach((s, i) => { const r = s.getBoundingClientRect(); const over = e.clientX >= r.left && e.clientX <= r.right && e.clientY >= r.top && e.clientY <= r.bottom; s.classList.toggle('obs-drop', over && i !== d.from); if (over) d.to = i; });
  });
  addEventListener('pointerup', () => {
    if (!d) return;
    const x = d; d = null;
    if (!x.moved) return;
    x.b.classList.remove('obs-dragging'); if (x.ghost) x.ghost.remove();
    rackUi[x.ri].slots.forEach(s => s.classList.remove('obs-drop'));
    const swallow = ev => { ev.stopImmediatePropagation(); ev.preventDefault(); document.removeEventListener('click', swallow, true); };
    document.addEventListener('click', swallow, true); setTimeout(() => document.removeEventListener('click', swallow, true), 50);
    if (x.to >= 0 && x.to !== x.from) {
      const ui = rackUi[x.ri]; if (ui.sel === x.from) ui.sel = x.to;
      call('moveModule', x.ri, x.from, x.to).then(r => { if (r) applyRig(r); });
    }
  });
}

/* =========================================================== amp and cab */
let AMPS = [], CABS = [];
const ampUi = {};
function setupAmpCab(st) {
  AMPS = st.amps || []; CABS = st.cabs || [];
  const AP = $('.page[data-page="AMP"]'), CP = $('.page[data-page="CAB"]');
  // library pop-ups (AMP page has amp + cab, CAB page has cab)
  $$('.pop').forEach(p => {
    const title = txt($('span', p));
    if (title !== 'CHOOSE AMP' && title !== 'CHOOSE CAB') return;
    const grid = $$('div', p).find(d => /grid-template-columns: repeat\(3/.test(d.getAttribute('style') || ''));
    if (grid) { grid.className = 'obs-grid'; grid.removeAttribute('style'); }
    const note = $$('span', p).find(s => /will show up here/.test(txt(s)));
    if (note) note.textContent = title === 'CHOOSE AMP' ? 'Your own amps: put .nam captures in Documents\\Obsidian Amp Suite\\Captures, or describe an amp in …\\Amps (see the README).' : 'Your own cabs: IR files (.wav) in Documents\\Obsidian Amp Suite\\IRs, or click LOAD IR on the CAB page.';
    p._grid = grid; p._kind = title === 'CHOOSE AMP' ? 'amp' : 'cab';
    const search = $('input', p); if (search) search.addEventListener('input', () => renderLib(p));
    p.addEventListener('click', () => setTimeout(() => renderLib(p), 0));
    new MutationObserver(() => {
      const open = p.style.display === 'block';
      if (open && !p._open) p._folder = '';          // every time the chooser opens, start at the top
      p._open = open;
      if (open) renderLib(p);
    }).observe(p, { attributes: true, attributeFilter: ['style'] });
  });
  intercept(b => b.dataset.ampFolder !== undefined, b => { const p = b.closest('.pop'); p._folder = b.dataset.ampFolder; renderLib(p); p._grid.scrollTop = 0; });
  intercept(b => b.classList.contains('obs-crumb-back'), b => { const p = b.closest('.pop'); p._folder = ''; renderLib(p); });
  intercept(b => b.classList.contains('obs-tile') && b.dataset.amp !== undefined, b => { call('setAmp', b.dataset.amp).then(r => r && applyRig(r)); $$('.pop').forEach(p => p.style.display = 'none'); });
  intercept(b => (b === ampUi.cabBtn || b === ampUi.cabBig) && rig && rig.cab && rig.cab.key && rig.cab.key !== 'none', b => {
    const page = b.closest('.page'); const pop = $$('.pop', page).find(p => txt($('span', p)) === 'CHOOSE CAB');
    $$('.pop').forEach(p => p.style.display = 'none'); if (pop) pop.style.display = 'block';
  });
  intercept(b => b.classList.contains('obs-tile') && b.dataset.cab !== undefined, b => { call('setCab', b.dataset.cab).then(r => r && applyRig(r)); $$('.pop').forEach(p => p.style.display = 'none'); });

  // amp panel lives where ADD AMP was
  if (AP) {
    const add = $$('button', AP).find(x => txt(x).startsWith('ADD AMP'));
    if (add) {
      const box = add.parentElement;
      ampUi.addBtn = add; ampUi.box = box;
      if (getComputedStyle(box).position === 'static') box.style.position = 'relative';
      ampUi.panel = document.createElement('div'); ampUi.panel.className = 'obs-amp obs-hidden';
      box.appendChild(ampUi.panel);
      buildAmpPanel();
    }
    const addCab = $$('button', AP).find(x => txt(x).startsWith('ADD CAB'));
    if (addCab) { ampUi.cabBtn = addCab; ampUi.cabHtml = addCab.innerHTML; }
  }
  if (CP) {
    const addCab = $$('button', CP).find(x => txt(x).startsWith('ADD CAB'));
    if (addCab) { ampUi.cabBig = addCab; ampUi.cabBigHtml = addCab.innerHTML; }
    ampUi.micNote = $$('span', CP).find(s => /MICS APPEAR/.test(txt(s)));
    const mics = $$('div', CP).find(d => d.style.opacity === '0.4' && /MIC A/.test(d.textContent));
    ampUi.mics = mics;
    intercept(b => CP.contains(b) && txt(b) === 'Load IR…', () => call('loadIr', 2));
    ampUi.irNote = $$('span', CP).find(s => /Or load your own impulse/.test(txt(s)));
  }
}
function renderLib(p) {
  if (!p._grid) return;
  const radio = $('.radio', p);
  const act = radio ? [...radio.children].find(b => /background: var\(--accent\)/.test(b.getAttribute('style') || '')) : null;
  const filter = act ? txt(act) : 'ALL';
  const q = (($('input', p) || {}).value || '').trim().toLowerCase();
  if (p._kind === 'amp') {
    const cur = rig && rig.amp ? rig.amp.key : '';
    const roles = { 'CLEAN': /clean/i, 'CRUNCH': /crunch/i, 'HIGH GAIN': /high|lead|metal/i };
    const match = a => (filter === 'ALL' || (roles[filter] && roles[filter].test(a.role + ' ' + a.description))) && (!q || (a.name + ' ' + a.role + ' ' + a.description + ' ' + (a.basedOn || '')).toLowerCase().includes(q));
    // Folders: amps with a "folder" are grouped (OBSIDIAN AMPS = the in-house amps). Searching or
    // filtering at the top level shows matching amps from every folder.
    const folder = p._folder || '';
    const flat = !folder && (q || filter !== 'ALL');
    let list = AMPS.filter(a => (folder ? a.folder === folder : (flat || !a.folder)) && match(a));
    let head = '';
    if (folder) {
      head = '<div class="obs-crumb"><button class="obs-btn obs-crumb-back">‹  ALL AMPS</button><span>' + esc(folder) + '</span><em>' + AMPS.filter(a => a.folder === folder).length + ' amps</em></div>';
    } else if (!flat) {
      const folders = [...new Set(AMPS.map(a => a.folder).filter(Boolean))];
      head = folders.map(f => {
        const inF = AMPS.filter(a => a.folder === f), here = inF.find(a => a.key === cur);
        const sub = f === 'OBSIDIAN AMPS' ? 'Our in-house amps, each modelled on a classic' : 'Amps you added';
        return '<button class="obs-tile obs-ampfolder' + (here ? ' has' : '') + '" data-amp-folder="' + esc(f) + '"><i>' + inF.length + ' AMPS</i><b>' + esc(f) + '</b><small>' + esc(here ? 'Playing now: ' + here.name : sub) + '</small></button>';
      }).join('');
    }
    p._grid.innerHTML = head + list.map(a => '<button class="obs-tile' + (a.key === cur ? ' on' : '') + '" data-amp="' + esc(a.key) + '" title="' + esc(a.description) + '"><i>' + esc(a.role || a.type) + '</i><b>' + esc(a.name) + '</b><small>' + esc(a.basedOn ? (/^Obsidian original/i.test(a.basedOn) ? a.basedOn : 'Based on ' + a.basedOn) : a.description) + '</small></button>').join('') + (list.length || (!folder && !flat && head) ? '' : '<div class="obs-empty">No amps match.</div>');
  } else {
    const cur = rig && rig.cab ? rig.cab.key : '';
    const list = CABS.filter(c => (filter === 'ALL' || (c.size || '').replace('x', '×') === filter) && (!q || (c.name + ' ' + (c.size || '')).toLowerCase().includes(q)));
    p._grid.innerHTML = list.map(c => '<button class="obs-tile' + (c.key === cur ? ' on' : '') + '" data-cab="' + esc(c.key) + '"><i>' + esc((c.size || '').replace('x', '×') || 'IR') + '</i><b>' + esc(c.name) + '</b><small>' + esc(c.builtIn ? 'Built in' : 'Your cab') + '</small></button>').join('') || '<div class="obs-empty">No cabs match.</div>';
  }
}
const AMP_KNOBS = [['amp.gain', 'GAIN'], ['amp.bass', 'BASS'], ['amp.mid', 'MIDDLE'], ['amp.treble', 'TREBLE'], ['amp.presence', 'PRESENCE'], ['amp.depth', 'DEPTH'], ['amp.master', 'MASTER']];
function buildAmpPanel() {
  const p = ampUi.panel;
  p.innerHTML = '<div class="obs-plate"><span class="obs-role"></span><span class="obs-ampname"></span><span class="obs-busy" style="display:none">LOADING…</span></div>'
    + '<div class="obs-based"></div>'
    + '<div class="obs-knobs" style="display:flex;gap:4px;justify-content:center"></div>'
    + '<div class="obs-chans"></div>'
    + '<div style="display:flex;gap:10px"><button class="obs-btn obs-ampchange">CHANGE AMP</button><button class="obs-btn obs-ampremove">REMOVE</button></div>';
  const row = $('.obs-knobs', p);
  AMP_KNOBS.forEach(([id, label]) => {
    const col = makeKnob(label); row.appendChild(col);
    const kn = $('.kn', col); kn.dataset.def = toNorm(id, 5);
    knob(id, kn, v => v.toFixed(1));
  });
  $('.obs-ampchange', p).onclick = e => { e.stopPropagation(); ampUi.addBtn.click(); };
  $('.obs-ampremove', p).onclick = e => { e.stopPropagation(); call('setAmp', 'none').then(r => r && applyRig(r)); };
  p.addEventListener('click', e => e.stopPropagation());
  // channel selector (NAM amps can hold up to 4 captures)
  single('amp.channel', () => +(p.dataset.ch || 0) / 3, n => { p.dataset.ch = Math.round(n * 3); renderAmp(); });
}
function renderAmp() {
  if (!ampUi.panel || !rig) return;
  const a = rig.amp || {}, has = a.key && a.key !== 'none';
  ampUi.panel.classList.toggle('obs-hidden', !has);
  [...ampUi.box.children].forEach(c => { if (c !== ampUi.panel) c.classList.toggle('obs-hidden', has); });
  if (!has) return;
  const info = AMPS.find(x => x.key === a.key) || {};
  $('.obs-ampname', ampUi.panel).textContent = a.name || info.name || 'Amp';
  $('.obs-role', ampUi.panel).textContent = info.role || (a.type === 'nam' ? 'Capture' : 'Built in');
  const based = $('.obs-based', ampUi.panel);
  const own = /^Obsidian original/i.test(info.basedOn || '');
  based.textContent = info.basedOn ? (own ? info.basedOn : 'BASED ON  ' + info.basedOn) : (a.type === 'nam' ? 'NAM CAPTURES · KNOBS ARE AN EQ AFTER THE CAPTURE' : '');
  based.title = info.description || '';
  ampUi.panel.style.setProperty('--amp-tint', info.tint ? '#' + info.tint : 'transparent');
  $('.obs-busy', ampUi.panel).style.display = rig.busy ? '' : 'none';
  const chans = $('.obs-chans', ampUi.panel);
  const isNam = a.type === 'nam';
  const cur = +(ampUi.panel.dataset.ch || 0);
  chans.innerHTML = isNam ? (a.channels || []).map((c, i) => '<div class="obs-chan' + (i === cur ? ' on' : '') + '" data-ch="' + i + '"><span>CH ' + (i + 1) + '</span><em>' + esc(c.name || 'empty') + '</em>'
      + '<button class="obs-btn" data-load="' + i + '" style="height:24px;padding:0 8px">' + (c.name ? 'CHANGE' : 'LOAD') + '</button>' + (c.name ? '<button class="obs-btn" data-clear="' + i + '" style="height:24px;padding:0 8px">✕</button>' : '') + '</div>').join('') : '';
  chans.onclick = e => {
    const l = e.target.closest('[data-load]'), c = e.target.closest('[data-clear]'), ch = e.target.closest('[data-ch]');
    if (l) return call('loadCapture', +l.dataset.load);
    if (c) return call('clearCapture', +c.dataset.clear);
    if (ch) { ampUi.panel.dataset.ch = ch.dataset.ch; renderAmp(); queueScan(); }
  };
}
function renderCab() {
  if (!rig) return;
  const c = rig.cab || {}, has = c.key && c.key !== 'none';
  const label = (c.irAName || c.irBName) ? [c.irAName, c.irBName].filter(Boolean).join(' + ') : (c.name || '');
  const card = (big) => '<span style="display:flex;flex-direction:column;align-items:center;gap:' + (big ? 10 : 6) + 'px"><span style="font-size:' + (big ? 10 : 9) + 'px;letter-spacing:.24em;color:var(--accent)">CAB</span><span style="font-size:' + (big ? 20 : 15) + 'px;font-weight:600;letter-spacing:.14em">' + esc(c.name || 'Cab') + '</span><span style="font-size:11px;color:rgb(var(--cf_8d8d93))">' + esc(label !== c.name ? label : 'Click to change') + '</span></span>';
  if (ampUi.cabBtn) ampUi.cabBtn.innerHTML = has ? card(false) : ampUi.cabHtml;
  if (ampUi.cabBig) ampUi.cabBig.innerHTML = has ? card(true) : ampUi.cabBigHtml;
  if (ampUi.micNote) ampUi.micNote.textContent = has ? 'TWO MICS, BLENDED · LOAD IR TO USE YOUR OWN' : 'MICS APPEAR ONCE A CAB IS CHOSEN';
  if (ampUi.mics) ampUi.mics.style.opacity = has ? '1' : '.4';
  if (ampUi.irNote) ampUi.irNote.textContent = (c.irAName || c.irBName) ? 'IR: ' + label : 'Or load your own impulse response';
}

/* ============================================================== settings */
let ddWatch = [];             // [button, onPick(text)]
function watchDD(btn, fn) { if (btn) ddWatch.push([btn, fn]); }
function setupSettings(st) {
  const _pick = window.ddPick;
  window.ddPick = function (o) {
    const b = typeof ddBtn !== 'undefined' ? ddBtn : null, t = txt(o);
    _pick(o);
    const w = ddWatch.find(x => x[0] === b);
    if (w) safe('dd', () => w[1](t));
  };
  const panel = n => $('.spanel[data-panel="' + n + '"]');
  const ddIn = (p, test) => $$('button', p).find(b => b.querySelector('path[d="M6 9l6 6 6-6"]') && test(ddText(b)));
  const S = st.settings || {};
  const setS = (k, v) => { INIT.settings[k] = v; call('setSetting', k, v); };
  window.obsSetSetting = setS;

  /* ---- Audio ---- */
  const A = panel('Audio');
  if (A) {
    const os = $$('.seg', A).find(s => /1×/.test(s.textContent));
    if (os) os.addEventListener('click', e => { const b = e.target.closest('button'); if (b) setS('oversampling', parseInt(txt(b))); });
    const rq = ddIn(A, t => ['Standard', 'High', 'Highest'].includes(t));
    watchDD(rq, t => setS('renderQuality', t));
    const cal = $('.kn', A);
    if (cal) { cal._fmt = v => fmt.dB(-12 + 24 * v); cal.dataset.def = .5; cal.addEventListener('pointerup', () => setS('inputCalibration', -12 + 24 * +cal.dataset.v)); cal.addEventListener('dblclick', () => setTimeout(() => setS('inputCalibration', 0), 0)); cal.addEventListener('wheel', () => setTimeout(() => setS('inputCalibration', -12 + 24 * +cal.dataset.v), 0)); }
  }
  const TOGS = { 'Mute output while tuning': 'muteWhileTuning', 'Mute while tuning': 'muteWhileTuning', 'Live tuner': 'liveTuner', 'Latency compensation': 'latencyCompensation',
                 'Program change loads presets': 'pcLoadsPresets', 'Show tooltips': 'tooltips', 'Open last preset on start': 'openLastPreset', 'Mark unsaved changes': 'markUnsaved', 'Ask before switching': 'askBeforeSwitching' };
  document.addEventListener('click', e => {
    const t = e.target.closest('.tog'); if (!t) return;
    const k = TOGS[t.getAttribute('aria-label')]; if (!k) return;
    setTimeout(() => { setS(k, t.dataset.on === '1'); if (k === 'markUnsaved') showPreset(presetCur); }, 0);
  });

  /* ---- Interface ---- */
  const I = panel('Interface');
  const sz = I && ddIn(I, t => /^\d+%$/.test(t));
  watchDD(sz, t => { if (typeof zoom !== 'undefined') { zoom = 1; fit(); } setS('uiScale', parseInt(t) / 100); });
  if (typeof applyTheme === 'function') {
    const _apply = window.applyTheme;
    window.applyTheme = function (n) { _apply(n); if (!applying) setS('theme', n); };
  }
  const _acc = window.setAccent; let accT = null;
  window.setAccent = function (c) { _acc(c); if (!applying) { clearTimeout(accT); accT = setTimeout(() => setS('accent', c), 400); } };

  /* ---- Presets ---- */
  const P = panel('Presets');
  if (P) {
    intercept(b => P.contains(b) && txt(b) === 'CHANGE', () => call('presetChooseFolder'));
    intercept(b => P.contains(b) && txt(b) === 'IMPORT', () => call('presetImport'));
    intercept(b => P.contains(b) && txt(b) === 'EXPORT', () => call('presetExport', presetCur.id));
    intercept(b => P.contains(b) && txt(b) === 'RESTORE', () => call('presetRescan').then(() => say('Factory presets are built in and always there. List refreshed.')));
  }

  /* ---- Device (standalone only) ---- */
  const D = panel('Device');
  if (D && !st.standalone) {
    D.innerHTML = '<div class="obs-note">Inside a DAW, your DAW owns the audio interface, sample rate and buffer size.<br>Set them in your DAW\'s audio settings. The standalone app has its own device settings here.</div>';
  } else if (D) {
    intercept(b => D.contains(b) && txt(b) === 'TEST', () => call('audioTest'));
    intercept(b => D.contains(b) && txt(b) === 'Control Panel', () => call('audioControlPanel').then(renderAudio));
    intercept(b => D.contains(b) && txt(b) === 'Reset Device', () => call('audioReset').then(a => { renderAudio(a); say('Audio device restarted'); }));
    intercept(b => b.classList.contains('obs-chk-in') || b.classList.contains('obs-chk-out'), b => {
      const box = b.parentElement, isIn = b.classList.contains('obs-chk-in');
      b.dataset.on = b.dataset.on === '1' ? '0' : '1'; styleChk(b);
      const bits = $$(isIn ? '.obs-chk-in' : '.obs-chk-out', box).map(x => x.dataset.on === '1');
      if (!bits.some(Boolean)) { b.dataset.on = '1'; styleChk(b); say('Keep at least one channel on'); return; }
      call(isIn ? 'audioSetInputs' : 'audioSetOutputs', bits).then(renderAudio);
    });
    intercept(b => b.classList.contains('obs-chk-midi'), b => { b.dataset.on = b.dataset.on === '1' ? '0' : '1'; styleChk(b); call('midiInputEnable', b.dataset.id, b.dataset.on === '1'); });
  }
  renderSettings(S);
  renderAudio(st.audio);
}
function styleChk(b) {
  const on = b.dataset.on === '1';
  b.style.color = on ? 'rgb(var(--cf_e9e6df))' : 'rgb(var(--cf_8d8d93))';
  const bx = b.firstElementChild; if (!bx) return;
  bx.style.background = on ? 'rgb(var(--cb_f4f1ea))' : 'none';
  bx.style.boxShadow = on ? 'none' : 'inset 0 0 0 1.5px rgb(var(--cb_4a4a50))';
  bx.innerHTML = on ? '<svg aria-hidden="true" width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="rgb(var(--cb_111113))" stroke-width="3.5" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.5l4.5 4.5L19 7"/></svg>' : '';
}
function renderSettings(S) {
  applying++;
  try {
    const TOGS = { 'Mute output while tuning': 'muteWhileTuning', 'Mute while tuning': 'muteWhileTuning', 'Live tuner': 'liveTuner', 'Latency compensation': 'latencyCompensation',
                   'Program change loads presets': 'pcLoadsPresets', 'Show tooltips': 'tooltips', 'Open last preset on start': 'openLastPreset', 'Mark unsaved changes': 'markUnsaved', 'Ask before switching': 'askBeforeSwitching' };
    $$('.tog').forEach(t => { const k = TOGS[t.getAttribute('aria-label')]; if (k && S[k] !== undefined) setTog(t, !!S[k]); });
    if (S.liveTuner !== undefined && typeof live !== 'undefined') { live = !!S.liveTuner; setFoot(); }
    if (S.theme && typeof applyTheme === 'function') applyTheme(S.theme);
    if (S.themeFx !== undefined && typeof setThemeFx === 'function') setThemeFx(!!S.themeFx);
    if (S.accent && typeof setAccent === 'function') { setAccent(S.accent); const h = $('#acchex'); if (h) h.value = S.accent.toUpperCase(); }
    const A = $('.spanel[data-panel="Audio"]');
    if (A) {
      const os = $$('.seg', A).find(s => /1×/.test(s.textContent));
      if (os) { const b = byText('button', (S.oversampling || 2) + '×', os); if (b && !isAct2(b)) b.click(); }
      const rq = $$('button', A).find(b => ['Standard', 'High', 'Highest'].includes(ddText(b))); if (rq && S.renderQuality) setDDText(rq, S.renderQuality);
      const cal = $('.kn', A); if (cal) setKnob(cal, clamp(((+S.inputCalibration || 0) + 12) / 24, 0, 1));
    }
    const I = $('.spanel[data-panel="Interface"]');
    const sz = I && $$('button', I).find(b => /^\d+%$/.test(ddText(b))); if (sz && S.uiScale) setDDText(sz, Math.round(S.uiScale * 100) + '%');
    const P = $('.spanel[data-panel="Presets"]');
    const path = P && $$('span', P).find(s => /\\|\//.test(txt(s)) && !s.children.length);
    if (path && S.presetFolder) { path.textContent = S.presetFolder; path.title = S.presetFolder; path.style.wordBreak = 'break-all'; }
    const M = $('.spanel[data-panel="MIDI"]');
    if (M) {
      const ch = $$('button', M).find(b => DD[8] && DD[8].includes(ddText(b)));
      if (ch) setDDText(ch, S.midiChannel ? String(S.midiChannel) : 'Omni');
    }
  } finally { applying--; }
}
function renderAudio(a) {
  if (!a || !a.standalone) return;
  const D = $('.spanel[data-panel="Device"]'); if (!D) return;
  if (!DD) return;
  const dd = test => $$('button', D).find(b => b.querySelector('path[d="M6 9l6 6 6-6"]') && test(b));
  // which dropdown is which: remember them the first time (their texts change)
  if (!D._dd) {
    D._dd = { type: dd(b => DD[0].includes(ddText(b))), dev: dd(b => DD[1].includes(ddText(b))), rate: dd(b => DD[2].includes(ddText(b))), buf: dd(b => DD[3].includes(ddText(b))), bits: dd(b => DD[4].includes(ddText(b))) };
    watchDD(D._dd.type, t => call('audioSetType', t).then(renderAudio));
    watchDD(D._dd.dev, t => call('audioSetDevice', t, t).then(renderAudio));
    watchDD(D._dd.rate, t => call('audioSetSampleRate', parseFloat(t)).then(renderAudio));
    watchDD(D._dd.buf, t => call('audioSetBufferSize', parseInt(t)).then(renderAudio));
  }
  const sr = a.sampleRate || 48000;
  const bufLabel = n => n + ' samples (' + (n / sr * 1000).toFixed(1) + ' ms)';
  const fill = (i, list, cur, btn) => { DD[i].splice(0, DD[i].length, ...(list.length ? list : [cur || '—'])); if (btn) setDDText(btn, cur || '—'); };
  const d = D._dd;
  fill(0, a.types || [], a.type, d.type);
  const devs = [...new Set([...(a.outputDevices || []), ...(a.inputDevices || [])])];
  fill(1, devs, a.deviceName || a.outputDevice || a.inputDevice || '', d.dev);
  fill(2, (a.sampleRates || []).map(r => Math.round(r) + ' Hz'), a.sampleRate ? Math.round(a.sampleRate) + ' Hz' : '', d.rate);
  fill(3, (a.bufferSizes || []).map(bufLabel), a.bufferSize ? bufLabel(a.bufferSize) : '', d.buf);
  fill(4, [a.bitDepth ? a.bitDepth + '-bit' : '—'], a.bitDepth ? a.bitDepth + '-bit' : '—', d.bits);
  if (d.bits) { d.bits.title = 'Set by your audio driver'; d.bits.style.opacity = '.6'; }
  // channel lists
  const wells = $$('div', D).filter(w => w.querySelector('.chk,.obs-chk-in,.obs-chk-out,.obs-chk-midi') && /overflow: auto/.test(w.getAttribute('style') || ''));
  const chkHtml = (cls, label, on, extra) => '<button class="' + cls + '" data-on="' + (on ? 1 : 0) + '"' + (extra || '') + ' style="height: 30px; border: none; background: none; padding: 0; cursor: pointer; display: flex; align-items: center; gap: 12px; font: inherit; font-size: 13px; white-space: nowrap"><span style="width: 18px; height: 18px; flex: none; border-radius: 4px; display: flex; align-items: center; justify-content: center"></span>' + esc(label) + '</button>';
  if (wells[0]) { wells[0].innerHTML = (a.inputChannels || []).map((n, i) => chkHtml('obs-chk-in', n, (a.inputsOn || [])[i])).join('') || '<span class="obs-note" style="padding:4px 0">No inputs</span>'; $$('button', wells[0]).forEach(styleChk); }
  if (wells[1]) { wells[1].innerHTML = (a.outputChannels || []).map((n, i) => chkHtml('obs-chk-out', n, (a.outputsOn || [])[i])).join('') || '<span class="obs-note" style="padding:4px 0">No outputs</span>'; $$('button', wells[1]).forEach(styleChk); }
  if (wells[2]) { wells[2].innerHTML = (a.midiInputs || []).map(m => chkHtml('obs-chk-midi', m.name, m.enabled, ' data-id="' + esc(m.id) + '"')).join('') || '<span class="obs-note" style="padding:4px 0">No MIDI devices found</span>'; $$('button', wells[2]).forEach(styleChk); }
  const lat = $$('span', D).find(s => / ms in · /.test(txt(s)) || s.classList.contains('obs-lat'));
  if (lat) { lat.classList.add('obs-lat'); lat.textContent = a.error ? a.error : (a.inputLatencyMs || 0).toFixed(1) + ' ms in · ' + (a.outputLatencyMs || 0).toFixed(1) + ' ms out'; }
  const cp = byText('button', 'Control Panel', D); if (cp) cp.style.opacity = a.hasControlPanel ? '1' : '.4';
  // MIDI panel: input device list
  const M = $('.spanel[data-panel="MIDI"]');
  if (M && DD[7]) {
    const names = ['All devices', ...(a.midiInputs || []).map(m => m.name)];
    DD[7].splice(0, DD[7].length, ...names);
    const b = $$('button', M).find(x => x.querySelector('path[d="M6 9l6 6 6-6"]') && (names.includes(ddText(x)) || x._midiIn));
    if (b) { b._midiIn = true; const cur = (INIT.settings || {}).midiInput || 'All devices'; setDDText(b, names.includes(cur) ? cur : 'All devices'); if (!b._watched) { b._watched = true; watchDD(b, t => call('setSetting', 'midiInput', t)); } }
  }
}
/* "Transpose quality" row in Settings > Audio (Live = instant, HQ = studio quality, adds latency) */
function addTransposeQualityRow() {
  const A = $('.spanel[data-panel="Audio"]'); if (!A) return;
  const os = $$('.seg', A).find(s => /1×/.test(s.textContent)); if (!os) return;
  const row = os.closest('div[style*="justify-content: space-between"]'); if (!row) return;
  const r2 = row.cloneNode(true);
  const spans = $$(':scope > div > span', r2);
  if (spans[0]) spans[0].textContent = 'Transpose quality';
  if (spans[1]) spans[1].textContent = 'Live: instant, for playing · HQ: smoothest, adds latency (for recording)';
  const seg = $('.seg', r2);
  const bs = $$('button', seg);
  bs[0].textContent = 'LIVE'; bs[1].textContent = 'HQ'; if (bs[2]) bs[2].remove();
  row.after(r2);
  seg.dataset.param = 'transpose.hq';
  single('transpose.hq', () => isAct2($$('button', seg)[1]) ? 1 : 0, n => { const b = $$('button', seg)[n >= .5 ? 1 : 0]; if (b && !isAct2(b)) b.click(); });
}

/* ============================================================ tempo / metronome */
function setupTempo(st) {
  if (typeof setBpm === 'function') {
    const _set = window.setBpm;
    window.setBpm = function (n) { _set(n); if (!applying) call('setBpm', bpm); };
  }
  const sig = $$('.seg').find(s => /4\/4/.test(s.textContent));
  if (sig) sig.addEventListener('click', e => { const b = e.target.closest('button'); if (b) call('setBeats', parseInt(txt(b))); });
  const acc = $('.tog[aria-label="Accent first beat"]');
  if (acc) acc.addEventListener('click', () => setTimeout(() => call('setAccent', acc.dataset.on === '1'), 0));
  document.addEventListener('click', e => {
    const b = e.target.closest('[aria-label="Start metronome"],[aria-label="Stop metronome"]'); if (!b) return;
    setTimeout(() => call('metronome', typeof metro !== 'undefined' ? !!metro : false), 0);
  });
}
function markTimeSig(n) {
  const sig = $$('.seg').find(s => /4\/4/.test(s.textContent)); if (!sig) return;
  const b = $$('button', sig).find(x => parseInt(txt(x)) === n && /\/4$|\/8$/.test(txt(x)));
  if (b && !isAct2(b)) { applying++; try { b.click(); } finally { applying--; } }
}

/* ======================================================================= MIDI */
let learnArmed = false, learnTarget = null;
function setupMidi(st) {
  const M = $('.spanel[data-panel="MIDI"]'); if (!M) return;
  const head = $$('span', M).find(s => txt(s) === 'MESSAGE');
  M._list = head ? head.parentElement.parentElement : null;
  if (M._list) M._headRow = head.parentElement;
  watchDD($$('button', M).find(b => DD[8] && DD[8].includes(ddText(b))), t => call('setSetting', 'midiChannel', t === 'Omni' ? 0 : parseInt(t)));
  intercept(b => M.contains(b) && txt(b) === 'MIDI LEARN', () => { learnArmed = true; learnTarget = null; document.body.classList.add('obs-learn'); closeOv(); say('MIDI learn: touch a knob or switch, then move a control on your MIDI controller'); });
  intercept(b => M.contains(b) && txt(b) === 'CLEAR ALL', async () => { const list = await call('midiMappings') || []; for (let i = list.length - 1; i >= 0; --i) await call('midiRemove', i); renderMappings(await call('midiMappings')); say('All MIDI mappings removed'); });
  intercept(b => b.classList.contains('obs-unmap'), b => call('midiRemove', +b.dataset.i).then(renderMappings));
  renderMappings(st.midi);
}
function armLearn(id) {
  learnTarget = id; learnArmed = false; document.body.classList.remove('obs-learn');
  call('midiLearn', id);
}
function setupLearn() {
  // right-click any knob / switch / fader for MIDI learn
  document.addEventListener('contextmenu', e => {
    const el = e.target.closest('[data-param]'); if (!el) return;
    e.preventDefault();
    learnTarget = null; armLearn(el.dataset.param);
  });
  document.addEventListener('keydown', e => { if (e.key === 'Escape' && (learnArmed || learnTarget)) { learnArmed = false; learnTarget = null; document.body.classList.remove('obs-learn'); call('midiCancelLearn'); } });
  on('obs:midi', () => { learnTarget = null; });
}
function renderMappings(list) {
  const M = $('.spanel[data-panel="MIDI"]'); if (!M || !M._list) return;
  [...M._list.children].forEach(c => { if (c !== M._headRow) c.remove(); });
  (list || []).forEach((m, i) => {
    const r = document.createElement('div'); r.className = 'obs-maprow';
    r.innerHTML = '<span style="color: var(--accent); font-variant-numeric: tabular-nums">CC ' + m.cc + (m.channel ? ' · ch ' + m.channel : '') + '</span><span>' + esc(m.paramName) + '</span><span style="color: rgb(var(--cf_8d8d93))">' + (m.toggle ? 'Footswitch' : 'Knob / pedal') + '</span><button class="obs-unmap" data-i="' + i + '" title="Remove">×</button>';
    M._list.appendChild(r);
  });
  if (!(list || []).length) { const e = document.createElement('div'); e.className = 'obs-note'; e.textContent = 'No mappings. Click MIDI LEARN, or right-click any knob.'; M._list.appendChild(e); }
}

/* ======================================================================= go */
start().catch(e => console.error('[obsidian] start failed', e));
})();
