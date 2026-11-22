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
  const SECTIONS = { 'SFX': 'sfx.on', 'PEDALS': 'pre.on', 'SYNTH': 'synth.on', 'HARMONY': 'harm.on', 'AMP': 'amp.on', 'CAB': 'cab.on', 'EQ': 'eq.on', 'FX': 'post.on' };
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
    const power = on => { vals['synth.on'] = on ? 1 : 0; last['synth.on'] = vals['synth.on']; call('setParam', 'synth.on', on ? 1 : 0);
      if (on) bypassed.delete('SYNTH'); else bypassed.add('SYNTH'); if (typeof styleTabs === 'function') styleTabs(); if (S.setPower) S.setPower(on); };
    S.powerOut = on => power(on);
    S.noteOut = (note, isOn) => {
      if (isOn && (vals['synth.on'] || 0) < .5) { power(true); say('Synth switched on'); }
      call('synthNote', note, !!isOn);
    };
    // show "the synth is off" on its page whenever the section is bypassed
    const showPower = () => { if (S.setPower) S.setPower((vals['synth.on'] || 0) >= .5); };
    showPower(); setInterval(showPower, 300);
  });

  /* ---- HARMONY page (HYDRA: harmony.js exposes all of its state) ---- */
  if (window.obsHarmony) safe('harmony', () => {
    const H = window.obsHarmony;
    addBinding({ ids: H.ids, read: () => H.read(), write: v => H.write(v) });
    // POWER is the section switch (like double-clicking the HARMONY tab)
    H.powerOut = on => { vals['harm.on'] = on ? 1 : 0; last['harm.on'] = vals['harm.on']; call('setParam', 'harm.on', on ? 1 : 0);
      if (on) bypassed.delete('HARMONY'); else bypassed.add('HARMONY'); if (typeof styleTabs === 'function') styleTabs(); };
    const showPower = () => { if (H.setPower) H.setPower((vals['harm.on'] || 0) >= .5); };
    showPower(); setInterval(showPower, 300);
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
    const cutOff = { LC: false, HC: false }, cutTogs = [];
    const redrawEq = () => { if (typeof eqRedraw === 'function') eqRedraw(); };
    [['LOW CUT', 'eq.lc', 'LC', 'eq.lc.on'], ['HIGH CUT', 'eq.hc', 'HC', 'eq.hc.on']].forEach(([label, id, which, onId]) => {
      const k = $$('.kn', EP).find(x => knobLabel(x) === label); if (!k) return;
      k.dataset.param = id;
      single(id, () => +k.dataset.v, n => { const f = toReal(id, n); setCut(which, ex(f)); cutKnob(label, f); });
      // the cut's own on / off switch, under its knob
      const tpl = $('.tog[aria-label="Gate on"]'); if (!tpl || !META[onId]) return;
      const col = k.parentElement, tg = tpl.cloneNode(true);
      tg.setAttribute('aria-label', label.toLowerCase() + ' on'); tg.title = label + ' on / off (off: the knob is ignored)';
      const row = document.createElement('div'); row.className = 'obs-cutsw'; row.appendChild(tg); col.appendChild(row);
      const show = () => {
        cutOff[which] = tg.dataset.on !== '1'; col.classList.toggle('obs-cutoff', cutOff[which]);
        const svg = typeof eqSvg === 'function' && eqSvg(), r = svg && svg.querySelectorAll('rect')[which === 'LC' ? 0 : 1];
        if (r) { r.style.opacity = cutOff[which] ? .3 : ''; if (r.nextElementSibling) r.nextElementSibling.style.opacity = cutOff[which] ? .3 : ''; }
        redrawEq();
      };
      tog(onId, tg, show);
      tg.addEventListener('click', () => setTimeout(show, 0));
      setTog(tg, (vals[onId] ?? 1) >= .5); show();
      cutTogs.push([tg, show]);
    });
    // the curve leaves out a cut that is switched off
    if (typeof eqRedraw === 'function') {
      const draw = eqRedraw;
      window.eqRedraw = function () { const lc = LCF, hc = HCF; if (cutOff.LC) LCF = 20; if (cutOff.HC) HCF = 20000; try { draw(); } finally { LCF = lc; HCF = hc; } };
    }
    // RESET: every band flat, both cuts off and their switches back on
    const rs = $('.eqreset', EP);
    if (rs) rs.addEventListener('click', () => setTimeout(() => { cutTogs.forEach(([t, show]) => { if (t.dataset.on !== '1') { setTog(t, true); show(); } }); queueScan(); }, 0));
    // double-click a fader, or its dot on the curve, to put that band back to 0 dB
    EP.addEventListener('dblclick', e => {
      const fd = e.target.closest('.fd');
      let band = fd ? $$('.fd').indexOf(fd) : -1;
      const svg = typeof eqSvg === 'function' && eqSvg();
      if (band < 0 && svg && svg.contains(e.target)) {
        const dots = [...svg.querySelectorAll('circle[r="9"]')]; let best = 1e9;
        dots.forEach((c, i) => { const r = c.getBoundingClientRect(), d = Math.hypot(e.clientX - (r.left + r.width / 2), e.clientY - (r.top + r.height / 2)); if (d < 18 && d < best) { best = d; band = i; } });
      }
      if (band < 0) return;
      const f = $$('.fd')[band]; if (!f) return;
      setFader(f, 0); redrawEq(); queueScan();
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
  safe('updates', () => setupUpdates(st));
  safe('tempo', () => setupTempo(st));
  safe('tuner', setupTuner);
  safe('midi', () => setupMidi(st));
  safe('learn', setupLearn);
  safe('rig', () => applyRig(st.rig));
  on('obs:params', applyParams);
  on('obs:rig', applyRig);
  on('obs:preset', showPreset);
  on('obs:presets', x => { if (Array.isArray(x)) presetList = x; else if (x) { presetList = x.list || []; presetUserFolders = x.folders || []; } renderPresetList(); });
  on('obs:meters', showMeters);
  on('obs:toast', t => say(t.text));
  on('obs:audio', renderAudio);
  on('obs:audioStats', renderAudio);
  on('obs:midi', renderMappings);
  on('obs:settings', s => { INIT.settings = s; safe('settings', () => renderSettings(s)); });
  on('obs:update', u => safe('update', () => renderUpdate(u)));
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

/* ===================================================== updates (Settings > About) */
let updEl = null, updToasted = 0, updAutoInstall = false;
const mb = b => (b / 1048576).toFixed(b >= 10485760 ? 0 : 1) + ' MB';
function setupUpdates(st) {
  const A = $('.spanel[data-panel="About"]'); if (!A || $('.obs-upd', A)) return;
  // the build number in the About table
  const grid = $$('div', A).find(d => d.children.length >= 8 && $$('span', d).some(s => txt(s) === 'Version'));
  if (grid) { const l = document.createElement('span'), v = document.createElement('span'); l.textContent = 'Build';
    l.setAttribute('style', grid.children[0].getAttribute('style') || ''); v.setAttribute('style', grid.children[1].getAttribute('style') || '');
    v.textContent = st.build ? String(st.build) : 'made by hand'; grid.appendChild(l); grid.appendChild(v); }
  const box = document.createElement('div'); box.className = 'obs-upd';
  box.innerHTML = '<div class="obs-upd-h"><b>UPDATES</b><span class="obs-upd-msg"></span></div>'
    + '<div class="obs-upd-bar"><i></i></div>'
    + '<div class="obs-upd-notes"></div>'
    + '<div class="obs-upd-row"><button class="obs-btn obs-upd-go">UPDATE</button><button class="obs-btn obs-upd-check">CHECK NOW</button>'
    + '<label class="obs-upd-auto"><span>Check when it opens</span></label></div>';
  const col = A.firstElementChild || A; col.appendChild(box);
  const tg = $('.tog') ? $('.tog').cloneNode(true) : document.createElement('button');
  tg.setAttribute('aria-label', 'Check for updates when it opens'); tg.classList.add('obs-upd-tog'); $('.obs-upd-auto', box).appendChild(tg);
  const drawTog = () => { if (typeof setTog === 'function') setTog(tg, INIT.settings.checkUpdates !== false); };
  drawTog();
  tg.addEventListener('click', e => { e.stopPropagation(); e.preventDefault(); const v = INIT.settings.checkUpdates === false; INIT.settings.checkUpdates = v; call('setSetting', 'checkUpdates', v); drawTog(); }, true);
  $('.obs-upd-check', box).addEventListener('click', e => { e.stopPropagation(); call('updateCheck').then(renderUpdate); });
  $('.obs-upd-go', box).addEventListener('click', e => {
    e.stopPropagation();
    const s = (updEl && updEl._u) || {};
    if (s.state === 'downloading') { updAutoInstall = false; call('updateCancel'); return; }
    if (s.state === 'ready') { installUpdate(); return; }
    if (s.state === 'available' || (s.state === 'failed' && s.latest)) { updAutoInstall = true; call('updateDownload').then(renderUpdate); return; }
    call('updateCheck').then(renderUpdate);
  });
  updEl = box;
  renderUpdate(st.update || {});
}
function installUpdate() {
  updAutoInstall = false;
  call('updateInstall').then(r => { if (r && r.message) say(r.message); if (updEl) $('.obs-upd-msg', updEl).textContent = r && r.message ? r.message : ''; });
}
function renderUpdate(u) {
  if (!u || !updEl) return;
  updEl._u = u;
  const msg = $('.obs-upd-msg', updEl), go = $('.obs-upd-go', updEl), bar = $('.obs-upd-bar', updEl), notes = $('.obs-upd-notes', updEl);
  const own = u.own ? 'build ' + u.own : 'a build made by hand';
  const size = u.size ? ' · ' + mb(u.size) + (u.full ? ' (with the pedal and amp pack)' : '') : '';
  const T = {
    idle: ['You have ' + own + '.', 'CHECK'],
    checking: ['Looking for a newer build…', 'CHECK'],
    uptodate: ['You have the newest build (' + (u.own || u.latest) + ').', 'CHECK'],
    available: ['Build ' + u.latest + ' is ready' + size + '. You have ' + own + '.', 'UPDATE'],
    downloading: ['Downloading build ' + u.latest + '… ' + Math.round((u.progress || 0) * 100) + '%', 'CANCEL'],
    ready: ['Build ' + u.latest + ' is downloaded and checked.', u.canInstall ? 'INSTALL' : 'SHOW FILE'],
    installing: ['Installing build ' + u.latest + '…', 'UPDATE'],
    failed: [(u.error || 'Something went wrong.'), u.latest ? 'TRY AGAIN' : 'CHECK'],
  }[u.state] || ['', 'CHECK'];
  if (u.state !== 'installing' || !msg.textContent) msg.textContent = T[0];     // installing keeps the installer's own message
  go.textContent = T[1];
  go.classList.toggle('hot', u.state === 'available' || u.state === 'ready');
  go.disabled = u.state === 'checking' || u.state === 'installing';
  bar.style.display = u.state === 'downloading' ? '' : 'none';
  bar.firstElementChild.style.width = Math.round((u.progress || 0) * 100) + '%';
  const showNotes = u.latest && (u.state === 'available' || u.state === 'downloading' || u.state === 'ready');
  notes.style.display = showNotes ? '' : 'none';
  if (showNotes) notes.textContent = (u.title ? u.title + '\n' : '') + (u.notes || '');
  if (u.state === 'available' && u.latest && u.latest !== updToasted && (!u.own || u.latest > u.own)) { updToasted = u.latest; say('Build ' + u.latest + ' is ready: Settings > About > UPDATE'); }
  if (u.state === 'ready' && updAutoInstall) installUpdate();
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
  if (window.obsHarmony && window.obsHarmony.live) safe('harmony live', () => window.obsHarmony.live({ note: m.hmNote, chord: m.hmChord || [] }));
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
  safe('face meters', () => faceMeters(m));
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
let presetList = [], presetCur = {}, presetSel = null, presetFolder = 'FACTORY', presetQuery = '', presetUserFolders = [];
/* Preset folders: FACTORY (by type), ARTISTS (one folder per artist), MY PRESETS (your saves, in folders you make).
   A factory preset whose category is "Artists/<name>" lives in that artist's folder.
   Factory and artist presets are built in and locked: they can't be deleted, renamed or moved. */
const presetOpen = new Set(['FACTORY']);
const presetGroup = p => !p.factory ? 'MY' : /^Artists\//.test(p.category || '') ? 'ARTISTS' : 'FACTORY';
const presetSub = p => { const g = presetGroup(p); return g === 'ARTISTS' ? p.category.slice(8) : g === 'MY' ? (p.dir || '') : (p.type || p.category || ''); };
const presetEntry = id => presetList.find(x => x.id === id);
const presetLocked = p => !p || p.factory !== false;               // built in (factory + artists)
const NO_FOLDER = '— No folder —';
const presetType = p => p.type || (/^Artists\//.test(p.category || '') ? '' : p.category) || '—';
const presetIn = (p, f) => { const [g, sub] = f.split('/'); return presetGroup(p) === g && (sub === undefined || presetSub(p) === f.slice(g.length + 1)); };
const GROUP_NAME = { FACTORY: 'Factory', ARTISTS: 'Artists', MY: 'My presets' };
/** The sub-folders of a top folder, in display order (Factory: by type; Artists: A-Z). */
function presetSubs(g) {
  if (g === 'MY') return presetUserFolders.slice();
  const subs = [...new Set(presetList.filter(p => presetGroup(p) === g).map(presetSub).filter(Boolean))];
  return subs.sort(g === 'ARTISTS' ? (a, b) => a.localeCompare(b) : (a, b) => ((TYPE_ORDER.indexOf(a) + 99) % 99) - ((TYPE_ORDER.indexOf(b) + 99) % 99) || a.localeCompare(b));
}
const FOLDER_IC = '<svg aria-hidden="true" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round"><path d="M3 6.5a1.5 1.5 0 0 1 1.5-1.5h4.6l2 2.2h8.4A1.5 1.5 0 0 1 21 8.7v9.8a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 18.5z"/></svg>';
const TYPE_ORDER = ['Clean', 'Crunch', 'Rhythm', 'Lead', 'Ambient', 'Synth', 'FX', 'Acoustic'];
function setupPresets(st) {
  presetList = st.presets || [];
  presetUserFolders = st.presetFolders || [];
  presetCur = st.preset || {};
  const prev = $('[aria-label="Previous preset"]'), next = $('[aria-label="Next preset"]');
  intercept(b => b === prev, () => guardSwitch(() => call('presetNext', -1)));
  intercept(b => b === next, () => guardSwitch(() => call('presetNext', 1)));
  const top = prev ? prev.parentElement.parentElement : document;
  intercept(b => top.contains(b) && txt(b) === 'SAVE', () => call('presetSave'));
  intercept(b => top.contains(b) && txt(b) === 'SAVE AS', () => saveAsDialog());
  intercept(b => top.contains(b) && txt(b) === 'DELETE', async () => {
    if (presetLocked(presetEntry(presetCur.id) || presetCur)) { say('Factory and artist presets are locked: they can\'t be deleted'); return; }
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
  const nf = byText('button', 'NEW FOLDER', ov);
  if (nf) { nf.classList.add('obs-newfolder'); nf.title = 'Make a folder in My presets'; }
  intercept(b => b.classList.contains('obs-newfolder'), () => newPresetFolder());
  // the details column: RENAME + DELETE side by side, MOVE TO FOLDER under them (your presets only)
  const ren = byText('button', 'RENAME', ov), dup = byText('button', 'DUPLICATE', ov);
  if (ren && dup) {
    const pair = document.createElement('div'); pair.className = 'obs-ppair'; ren.parentElement.insertBefore(pair, ren);
    const del = ren.cloneNode(true); del.textContent = 'DELETE'; del.classList.add('obs-pdel');
    pair.append(ren, del);
    const mv = dup.cloneNode(true); mv.textContent = 'MOVE TO FOLDER'; mv.classList.add('obs-pmove');
    pair.after(mv);
    ov._mine = [pair, mv];
  }
  intercept(b => b.classList.contains('obs-pdel'), async () => {
    const p = sel(); if (!p || presetLocked(p)) return;
    if (await confirmDialog('DELETE PRESET', 'Delete "' + p.name + '"? It goes to the recycle bin, so you can get it back.', 'DELETE')) call('presetDelete', p.id);
  });
  intercept(b => b.classList.contains('obs-pmove'), async () => {
    const p = sel(); if (!p || presetLocked(p)) return;
    if (!presetUserFolders.length) { say('Make a folder first: NEW FOLDER, under the folder list'); return; }
    const r = await formDialog('MOVE "' + p.name + '"', [{ key: 'dir', label: 'To folder', value: p.dir || NO_FOLDER, options: [NO_FOLDER].concat(presetUserFolders) }], 'MOVE');
    if (r) call('presetMove', p.id, r.dir === NO_FOLDER ? '' : r.dir);
  });
  // inside one of your folders: rename or delete it
  intercept(b => b.classList.contains('obs-pfren'), async () => {
    const old = presetFolder.slice(3), name = await textDialog('RENAME FOLDER', old, 'RENAME');
    if (!name || name === old) return;
    const before = presetUserFolders.slice();
    if (await call('presetRenameFolder', old, name)) goToNewFolder(before, name);
  });
  intercept(b => b.classList.contains('obs-pfdel'), async () => {
    const name = presetFolder.slice(3), n = presetList.filter(p => presetGroup(p) === 'MY' && p.dir === name).length;
    const what = n ? ' and the ' + (n === 1 ? 'preset' : n + ' presets') + ' in it' : '';
    if (!await confirmDialog('DELETE FOLDER', 'Delete the folder "' + name + '"' + what + '? It goes to the recycle bin, so you can get it back.', 'DELETE')) return;
    if (await call('presetDeleteFolder', name)) { presetFolder = 'MY'; renderPresetList(); }
  });
  // drag one of your presets onto a folder (or back onto My presets) to move it
  let pd = null;
  const dropAt = (x, y) => { const el = document.elementFromPoint(x, y), t = el && el.closest('[data-folder]'); return t && ov.contains(t) && /^MY(\/|$)/.test(t.dataset.folder) ? t : null; };
  ov.addEventListener('pointerdown', e => {
    const r = e.target.closest('.obs-prow.obs-pmine'); if (!r || e.button !== 0) return;
    pd = { r, id: r.dataset.id, x: e.clientX, y: e.clientY, moved: false };
  });
  addEventListener('pointermove', e => {
    if (!pd) return;
    if (!pd.moved && Math.hypot(e.clientX - pd.x, e.clientY - pd.y) < 6) return;
    const app = $('#app') || document.body, ar = app.getBoundingClientRect(), sc = ar.width / (app.offsetWidth || ar.width);
    if (!pd.moved) {
      pd.moved = true; ov.classList.add('obs-dragging'); pd.r.classList.add('obs-pdragged');
      pd.ghost = document.createElement('div'); pd.ghost.className = 'obs-pghost';
      pd.ghost.innerHTML = '<span></span>'; pd.ghost.firstChild.textContent = (presetEntry(pd.id) || {}).name || '';
      app.appendChild(pd.ghost);
    }
    pd.ghost.style.left = ((e.clientX - ar.left) / sc + 12) + 'px'; pd.ghost.style.top = ((e.clientY - ar.top) / sc + 10) + 'px';
    const t = dropAt(e.clientX, e.clientY);
    $$('.obs-drop', ov).forEach(x => x !== t && x.classList.remove('obs-drop'));
    if (t && t.dataset.folder.slice(3) !== ((presetEntry(pd.id) || {}).dir || '')) t.classList.add('obs-drop');
    pd.to = t && t.classList.contains('obs-drop') ? t.dataset.folder.slice(3) : null;
  });
  addEventListener('pointerup', () => {
    if (!pd) return;
    const x = pd; pd = null;
    if (!x.moved) return;
    ov.classList.remove('obs-dragging'); x.r.classList.remove('obs-pdragged'); if (x.ghost) x.ghost.remove();
    $$('.obs-drop', ov).forEach(el => el.classList.remove('obs-drop'));
    const swallow = ev => { ev.stopImmediatePropagation(); ev.preventDefault(); document.removeEventListener('click', swallow, true); };
    document.addEventListener('click', swallow, true); setTimeout(() => document.removeEventListener('click', swallow, true), 50);
    if (x.to != null) call('presetMove', x.id, x.to);
  });
  const search = $('#preset-search', ov);
  if (search) search.addEventListener('input', () => { presetQuery = search.value.trim().toLowerCase(); renderPresetList(); });
  intercept(b => b.classList.contains('obs-folder'), b => {
    const f = b.dataset.folder;
    if (!f.includes('/')) { if (presetFolder === f && presetOpen.has(f)) presetOpen.delete(f); else presetOpen.add(f); }
    presetFolder = f; presetQuery = ''; const se = $('#preset-search'); if (se) se.value = '';
    renderPresetList(); if (ov._rowBox) ov._rowBox.scrollTop = 0; });
  intercept(b => b.classList.contains('obs-prow'), b => { presetSel = b.dataset.id; markPresetSel(); });
  // a top folder lists its sub-folders: click one to go in, the back row goes up again
  intercept(b => b.classList.contains('obs-pdir') || b.classList.contains('obs-pup'), b => {
    const f = b.dataset.folder; presetFolder = f; presetOpen.add(f.split('/')[0]);
    renderPresetList(); if (ov._rowBox) ov._rowBox.scrollTop = 0; });
  ov.addEventListener('dblclick', e => { const r = e.target.closest('.obs-prow'); if (r) guardSwitch(() => { call('presetLoad', r.dataset.id); closeOv(); }); });
  const sel = () => presetList.find(p => p.id === presetSel);
  intercept(b => ov.contains(b) && txt(b) === 'LOAD', () => { const p = sel(); if (p) guardSwitch(() => { call('presetLoad', p.id); closeOv(); }); });
  intercept(b => ov.contains(b) && txt(b) === 'LOAD DEFAULT', () => guardSwitch(() => { call('presetInit'); closeOv(); }));
  intercept(b => ov.contains(b) && txt(b) === 'SHOW IN EXPLORER', () => call('presetReveal'));
  intercept(b => ov.contains(b) && txt(b) === 'RENAME', async () => {
    const p = sel(); if (!p) return;
    if (presetLocked(p)) { say('Factory and artist presets are locked. DUPLICATE one to make your own.'); return; }
    const name = await textDialog('RENAME PRESET', p.name, 'RENAME'); if (name) call('presetRename', p.id, name);
  });
  intercept(b => ov.contains(b) && txt(b) === 'DUPLICATE', async () => {
    const p = sel(); if (!p) return;
    // the copy goes in the folder of yours that's open, else next to the original (your presets) or loose in My presets
    const dir = presetFolder.startsWith('MY/') ? presetFolder.slice(3) : !presetLocked(p) ? (p.dir || '') : '';
    const type = presetType(p) === '—' ? '' : presetType(p);
    await call('presetLoad', p.id);
    const ok = await call('presetSaveAs', p.name + ' copy', type, dir);
    if (ok) say('Duplicated as "' + p.name + ' copy" in ' + (dir || 'My presets'));
  });
  renderPresetList();
}
/** After making or renaming a folder: open it (the name may have been tidied up). */
async function goToNewFolder(before, typed) {
  const now = (await call('presetFolders')) || presetUserFolders;
  presetUserFolders = now;
  const made = now.find(x => !before.includes(x)) || now.find(x => x.toLowerCase() === String(typed).trim().toLowerCase());
  if (made) { presetFolder = 'MY/' + made; presetOpen.add('MY'); presetQuery = ''; const se = $('#preset-search'); if (se) se.value = ''; }
  renderPresetList();
}
async function newPresetFolder() {
  const name = await textDialog('NEW FOLDER', 'New Folder', 'MAKE');
  if (!name) return;
  const before = presetUserFolders.slice();
  if (await call('presetNewFolder', name)) goToNewFolder(before, name);
}
function guardSwitch(go) {
  if (presetCur.dirty && INIT && INIT.settings && INIT.settings.askBeforeSwitching)
    confirmDialog('UNSAVED CHANGES', '"' + presetCur.name + '" has changes that aren\'t saved. Switch anyway?', 'SWITCH').then(ok => { if (ok) go(); });
  else go();
}
let presetShownId = null;
function showPreset(p) {
  if (!p) return; presetCur = p;
  if (p.id !== presetShownId) {             // the browser opens on the loaded preset's folder
    presetShownId = p.id;
    const e = presetList.find(x => x.id === p.id);
    if (e) { const g = presetGroup(e), sb = presetSub(e); presetFolder = sb ? g + '/' + sb : g; presetOpen.add(g); }
  }
  const topDel = $$('button').find(b => txt(b) === 'DELETE' && !b.closest('.ov'));
  if (topDel) { const lk = presetLocked(presetEntry(p.id) || p); topDel.classList.toggle('obs-locked', lk); topDel.title = lk ? 'Factory and artist presets are locked' : 'Delete this preset (to the recycle bin)'; }
  const mark = !INIT || !INIT.settings || INIT.settings.markUnsaved !== false;
  if (typeof setPreset === 'function') setPreset(esc(p.name), p.dirty && mark);
  if (!presetSel) presetSel = p.id;
  renderPresetList();
}
function renderPresetList() {
  const ov = $('.ov[data-ov="presets"]'); if (!ov || !ov._rowTpl) return;
  if (ov._folderTpl && ov._folderBox) {
    $$('.obs-folder', ov._folderBox).forEach(x => x.remove());
    ov._folderBox.style.overflowY = 'auto'; ov._folderBox.style.minHeight = '0';
    const anchor = ov._folderAnchor && ov._folderAnchor.parentNode === ov._folderBox ? ov._folderAnchor : byText('button', 'NEW FOLDER', ov._folderBox);
    const row = (key, label, count, sub) => {
      const f = ov._folderTpl.cloneNode(true); f.classList.remove('folder'); f.classList.add('obs-folder'); f.dataset.folder = key;
      if (sub) f.classList.add('obs-fsub'); else { f.classList.add('obs-fgroup'); f.classList.toggle('open', presetOpen.has(key)); }
      const sp = f.querySelectorAll('span'); sp[0].textContent = label; sp[sp.length - 1].textContent = count;
      const onF = key === presetFolder;
      f.classList.toggle('obs-fon', onF);
      f.style.background = onF ? 'rgba(255,255,255,.07)' : 'none';
      f.style.color = onF ? 'rgb(var(--cf_f4f1ea))' : sub ? 'rgb(var(--cf_8d8d93))' : 'rgb(var(--cf_c9c7c1))';
      f.title = sub ? '' : (presetOpen.has(key) ? 'Click again to fold' : 'Click to open');
      ov._folderBox.insertBefore(f, anchor || null);
    };
    for (const g of ['FACTORY', 'ARTISTS', 'MY']) {
      const inG = presetList.filter(p => presetGroup(p) === g);
      if (g === 'ARTISTS' && !inG.length) continue;
      row(g, GROUP_NAME[g], inG.length, false);
      if (!presetOpen.has(g)) continue;
      presetSubs(g).forEach(sb => row(g + '/' + sb, sb, inG.filter(p => presetSub(p) === sb).length, true));
    }
  }
  $$('.obs-prow, .obs-pdir, .obs-pbar', ov._rowBox).forEach(x => x.remove());
  $$('.obs-empty', ov._rowBox).forEach(x => x.remove());
  let [grp, sub] = presetFolder.split('/');
  if (presetFolder.includes('/')) sub = presetFolder.slice(grp.length + 1);
  if (sub !== undefined && !presetSubs(grp).includes(sub)) { presetFolder = grp; sub = undefined; }   // that folder is gone
  const loose = [];
  // the NAME / TYPE / SOURCE header only over preset rows
  if (ov._colHdr === undefined) { const n = $$('span', ov).find(x => txt(x) === 'NAME'); ov._colHdr = n ? n.parentElement : null; }
  if (ov._colHdr) ov._colHdr.style.visibility = !presetQuery && sub === undefined && grp !== 'MY' ? 'hidden' : '';
  // a top folder shows its folders, not every preset at once (My presets: your folders, then the loose ones)
  if (!presetQuery && sub === undefined)
  {
    const subs = presetSubs(grp);
    subs.forEach(sb => {
      const n = presetList.filter(p => presetGroup(p) === grp && presetSub(p) === sb).length;
      const d = document.createElement('button'); d.className = 'obs-pdir'; d.dataset.folder = grp + '/' + sb;
      d.innerHTML = '<span class="ic">' + FOLDER_IC + '</span><b></b><small></small><span class="go">›</span>';
      d.querySelector('b').textContent = sb; d.querySelector('small').textContent = n ? n + (n === 1 ? ' preset' : ' presets') : 'empty';
      if (presetList.some(p => p.id === presetCur.id && presetGroup(p) === grp && presetSub(p) === sb)) d.classList.add('cur');
      ov._rowBox.appendChild(d);
    });
    if (grp !== 'MY' || !subs.length && !presetList.some(p => presetGroup(p) === 'MY')) {
      if (!subs.length) { const e = document.createElement('div'); e.className = 'obs-empty'; e.textContent = grp === 'MY' ? 'No presets of yours yet. SAVE AS saves one; NEW FOLDER makes a folder for them.' : 'No presets here yet.'; ov._rowBox.appendChild(e); }
      markPresetSel();
      return;
    }
    presetList.forEach(p => { if (presetGroup(p) === 'MY' && !presetSub(p)) loose.push(p); });
    if (!loose.length) { markPresetSel(); return; }
  }
  // inside a folder: a way back up (and, in one of yours, rename / delete it)
  if (!presetQuery && sub !== undefined)
  {
    const bar = document.createElement('div'); bar.className = 'obs-pbar';
    const up = document.createElement('button'); up.className = 'obs-pup'; up.dataset.folder = grp;
    up.innerHTML = '<span class="go">‹</span><span>' + esc(GROUP_NAME[grp]) + '</span><span class="sep">/</span><b></b>';
    up.querySelector('b').textContent = sub; up.title = 'Back to ' + GROUP_NAME[grp] + (grp === 'MY' ? ' (drop a preset here to take it out of the folder)' : '');
    bar.appendChild(up);
    if (grp === 'MY') bar.insertAdjacentHTML('beforeend', '<button class="obs-pfact obs-pfren" title="Rename this folder">RENAME</button><button class="obs-pfact obs-pfdel" title="Delete this folder">DELETE</button>');
    ov._rowBox.appendChild(bar);
  }
  // searching looks everywhere; otherwise the open folder
  const show = loose.length ? loose : presetList.filter(p => presetQuery ? (p.name + ' ' + p.category + ' ' + (p.type || '') + ' ' + (p.dir || '') + ' ' + (p.note || '')).toLowerCase().includes(presetQuery) : presetIn(p, presetFolder));
  show.forEach(p => {
    const r = ov._rowTpl.cloneNode(true); r.classList.add('obs-prow'); r.dataset.id = p.id;
    const sp = r.querySelectorAll(':scope > span');
    if (sp[1]) sp[1].textContent = p.name;
    if (sp[2]) sp[2].textContent = presetType(p);
    if (sp[3]) sp[3].textContent = presetGroup(p) === 'ARTISTS' ? presetSub(p) : p.factory ? 'Factory' : p.dir || 'My preset';
    if (p.note) r.title = p.note;
    if (!presetLocked(p)) { r.classList.add('obs-pmine'); if (!p.note) r.title = 'Drag onto a folder to move it'; }
    if (sp[0]) sp[0].style.visibility = p.id === presetCur.id ? 'visible' : 'hidden';
    ov._rowBox.appendChild(r);
  });
  if (!show.length) { const e = document.createElement('div'); e.className = 'obs-empty'; e.textContent = presetQuery ? 'No presets match "' + presetQuery + '".' : grp === 'MY' ? 'This folder is empty. Drag presets onto it, or pick it in SAVE AS.' : 'No presets here yet. Use SAVE AS to make one.'; ov._rowBox.appendChild(e); }
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
      const e = presetEntry(p.id) || p, g = presetGroup(e), sb = presetSub(e), locked = presetLocked(e);
      const folder = GROUP_NAME[g] + (sb && g !== 'FACTORY' ? ' / ' + sb : '');
      const lines = ['Folder · ' + folder, 'Type · ' + presetType(e), e.note || '',
                     p.id === presetCur.id ? 'Loaded now' + (presetCur.dirty ? ' · edited' : '') : 'Double-click to load',
                     locked && e.id !== 'factory:Default' ? 'Built in · locked' : ''];
      while (info.children.length < lines.length) info.appendChild(document.createElement('span'));
      [...info.children].forEach((c, i) => { c.textContent = lines[i] || ''; c.style.display = lines[i] ? '' : 'none'; c.classList.toggle('obs-lockline', i === 4); });
      (ov._mine || []).forEach(x => x.classList.toggle('obs-hidden', locked));
    }
  }
}
async function saveAsDialog() {
  const cats = [...new Set(TYPE_ORDER.concat(presetList.filter(p => !p.factory).map(p => p.category).filter(c => c && !/^Artists\//.test(c))))];
  const cur = presetList.find(p => p.id === presetCur.id) || presetCur, curType = cur.type || (/^Artists\//.test(cur.category || '') ? '' : cur.category);
  const dir = presetFolder.startsWith('MY/') ? presetFolder.slice(3) : !presetLocked(cur) ? (cur.dir || '') : '';
  const fields = [
    { key: 'name', label: 'Name', value: presetCur.factory || !presetCur.name ? (presetCur.name ? presetCur.name + ' (mine)' : 'My Preset') : presetCur.name },
    { key: 'category', label: 'Type', value: curType || 'Rhythm', options: cats }];
  if (presetUserFolders.length) fields.push({ key: 'dir', label: 'Folder', value: presetUserFolders.includes(dir) ? dir : NO_FOLDER, options: [NO_FOLDER].concat(presetUserFolders) });
  const r = await formDialog('SAVE AS', fields, 'SAVE');
  if (r && r.name) call('presetSaveAs', r.name, r.category, !r.dir || r.dir === NO_FOLDER ? '' : r.dir);
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
    const many = fields.length > 1;
    d.innerHTML = '<h3>' + esc(title) + '</h3>' + fields.map(f => (many && f.label ? '<label class="obs-flabel">' + esc(f.label) + '</label>' : '') + (f.options
      ? '<select data-k="' + f.key + '">' + f.options.map(o => '<option' + (o === f.value ? ' selected' : '') + '>' + esc(o) + '</option>').join('') + '</select>'
      : '<input data-k="' + f.key + '" value="' + esc(f.value) + '" maxlength="60" spellcheck="false">')).join('')
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
const RACKS = [{ page: 'PEDALS', key: 'pre', add: 'ADD PEDAL', mask: 'pre' }, { page: 'FX', key: 'post', add: 'ADD EFFECT', mask: 'post' }, { page: 'SFX', key: 'sfx', add: 'ADD SPECIAL FX', mask: 'sfx' }];
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
  const PF = window.OBS_PEDALS || {};
  ui.grid.innerHTML = list.map(m => '<button class="obs-tile' + (m.key === cur ? ' on' : '') + (PF[m.key] ? ' pedpic' : '') + '" data-mod="' + esc(m.key) + '" data-rack="' + ri + '" title="' + esc(m.description) + '">'
    + (PF[m.key] ? '<img class="obs-pedpic" src="' + esc(m.key) + '_on.webp" alt="" draggable="false">' : '')
    + '<i>' + esc(m.category) + '</i><b>' + esc(m.name) + '</b><small>' + esc(m.description) + '</small></button>').join('')
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
      if (b.classList.contains('obs-filled')) { b.innerHTML = ui.empty[si]; b.classList.remove('obs-filled', 'obs-sel', 'pfslot'); }
      b.title = 'Add a ' + (ri ? 'effect' : 'pedal');
      if (ui.sel === si) { ui.sel = -1; ui.editor.classList.add('obs-hidden'); ui.column.style.gap = ''; }
      return;
    }
    const sig = s.key + '|' + (s.fileName || '');
    const pf = (window.OBS_PEDALS || {})[s.key];
    if (pf && (b.dataset.sig !== sig || !b.classList.contains('obs-filled'))) {
      // the pedal's own face (tools/faces/build_pedals.py): lit when on, the drawn footswitch switches it
      // size from the slot's own size: on a hidden page (a preset loaded from another tab) it measures 0
      const cs = getComputedStyle(b), bx = parseFloat(cs.borderLeftWidth) + parseFloat(cs.borderRightWidth) || 0, by = parseFloat(cs.borderTopWidth) + parseFloat(cs.borderBottomWidth) || 0;
      const bw = b.clientWidth || (parseFloat(cs.width) || 150) - bx, bh = b.clientHeight || (parseFloat(cs.height) || 214) - by;
      const sw = Math.max(40, bw - 10), sh = Math.max(60, bh - 10), k = Math.min(sw / pf.w, sh / pf.h);
      const W = Math.round(pf.w * k), H = Math.round(pf.h * k), f = (pf.fs || [])[pf.fs.length - 1] || [.5, .9, .1];
      const r = f[2] * W * 1.15;
      b.innerHTML = '<div class="obs-card face"><span class="obs-x" title="Remove">×</span><div class="obs-pface" style="width:' + W + 'px;height:' + H + 'px">'
        + '<img class="pf-off" src="' + s.key + '_off.webp" alt="" draggable="false"><img class="pf-on" src="' + s.key + '_on.webp" alt="" draggable="false">'
        + '<span class="obs-foot pf-foot" title="Footswitch: on / off" style="left:' + (f[0] * W - r) + 'px;top:' + (f[1] * H - r) + 'px;width:' + 2 * r + 'px;height:' + 2 * r + 'px"></span></div>'
        + (m.fileKind && m.fileKind !== 'pack' ? '<span class="obs-file obs-loadfile" title="Load a .nam capture">' + esc(s.fileName || 'LOAD CAPTURE…') + '</span>' : '') + '</div>';
      b.classList.add('obs-filled', 'pfslot'); b.dataset.sig = sig;
    } else if (!pf && (b.dataset.sig !== sig || !b.classList.contains('obs-filled'))) {
      b.classList.remove('pfslot');
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
    if (p._kind === 'amp') { const rd = $('.radio', p); if (rd) rd.classList.add('obs-hidden'); }
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
  intercept(b => b.classList.contains('obs-tile') && b.dataset.amp !== undefined, b => {
    const p = b.closest('.pop'), t = p && p._tiles && p._tiles[+b.dataset.tile];
    spawnAmp(b.dataset.amp, t ? t[1] : null); $$('.pop').forEach(x => x.style.display = 'none'); });
  intercept(b => b === ampUi.cabBig && rig && rig.cab && rig.cab.key && rig.cab.key !== 'none', b => {
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
    // The AMP page is just the amp: the cab is picked on the CAB page only. The amp sits in the middle.
    const addCab = $$('button', AP).find(x => txt(x).startsWith('ADD CAB'));
    if (addCab) addCab.parentElement.classList.add('obs-hidden');
    $$('.pop', AP).forEach(p => { if (txt($('span', p)) === 'CHOOSE CAB') p.classList.add('obs-hidden'); });
    if (ampUi.box) ampUi.box.classList.add('obs-ampbox');
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
/* ---- amp folders by sound ----------------------------------------------------------------
   Each amp sits in the folder(s) of the sounds it makes: an amp with a clean and a lead channel is in
   CLEAN and in LEAD, and picking it in a folder starts it on that channel (dims: face button -> position)
   and, for the in-house amps, at a gain that suits the sound (gain 0-10). lead: a touch more gain than
   the amp's own start, for a channel that does rhythm and lead. */
const SOUND_FOLDERS = [['CLEAN', 'Glassy, warm and sparkly cleans'], ['CRUNCH', 'Edge of breakup to classic rock'],
  ['RHYTHM', 'High gain for riffs and chugs'], ['LEAD', 'Solos: more gain, more sustain'], ['ACOUSTIC', 'Acoustic guitar sounds']];
const HG = [['RHYTHM', null, 6.5], ['LEAD', null, 8]];
const AMP_SOUNDS = {
  glass65: [['CLEAN', null, 4]], top30: [['CLEAN', null, 3], ['CRUNCH', null, 7]], plexi68: [['CRUNCH', null, 5.5], ['LEAD', null, 8.5]],
  brit800: [['CRUNCH', null, 4], ['RHYTHM', null, 7], ['LEAD', null, 8.5]], slo100: [['RHYTHM', null, 6], ['LEAD', null, 8]],
  styx51: HG, recto: HG, vh4: HG, core: HG,
  achlys: [['CLEAN', { 0: 0 }], ['RHYTHM', { 0: 1 }], ['LEAD', { 0: 2 }]],
  ares: [['CRUNCH', { 0: 0 }], ['RHYTHM', { 0: 1 }], ['LEAD', { 0: 1 }, 'lead']],
  astraea: [['CLEAN', { 0: 0 }], ['CRUNCH', { 0: 1 }]],
  belial: [['RHYTHM'], ['LEAD', null, 'lead']], erebus: [['RHYTHM'], ['LEAD', null, 'lead']], hecate: [['RHYTHM'], ['LEAD', null, 'lead']],
  nihil: [['RHYTHM'], ['LEAD', null, 'lead']], thanatos: [['RHYTHM'], ['LEAD', null, 'lead']],
  chrysaor: [['CRUNCH'], ['RHYTHM', null, 'lead']],
  citadel: [['CRUNCH', { 0: 0 }], ['RHYTHM', { 0: 1 }], ['LEAD', { 0: 1 }, 'lead']],
  dionysus: [['CLEAN', { 0: 0 }], ['CRUNCH', { 0: 1 }], ['LEAD', { 0: 2 }]],
  eos: [['CLEAN', { 0: 0 }], ['CRUNCH', { 0: 2, 1: 1 }], ['LEAD', { 0: 2, 1: 3 }]],
  eris: [['CLEAN', { 0: 0 }], ['CRUNCH', { 0: 1 }], ['RHYTHM', { 0: 2 }], ['LEAD', { 0: 2 }, 'lead']],
  hyperion: [['CLEAN', { 0: 0 }], ['CRUNCH', { 0: 1 }], ['LEAD', { 0: 1 }, 'lead']],
  kratos: [['RHYTHM', { 0: 0 }], ['LEAD', { 0: 1 }]],
  lamia: [['CLEAN', { 0: 0 }], ['CRUNCH', { 0: 1 }], ['RHYTHM', { 0: 2 }], ['LEAD', { 0: 2 }, 'lead']],
  lyssa: [['RHYTHM', { 0: 0 }], ['LEAD', { 0: 1 }]],
  melinoe: [['CLEAN']], persephone: [['CLEAN']], orpheus: [['ACOUSTIC']], tyche: [['CRUNCH']],
  moirai: [['CLEAN', { 0: 0 }], ['CRUNCH', { 0: 1 }], ['RHYTHM', { 0: 2 }], ['LEAD', { 0: 2 }, 'lead']],
  moros: [['RHYTHM', { 0: 0 }], ['LEAD', { 0: 2 }]],
  niflheim: [['CRUNCH', { 0: 0 }], ['RHYTHM', { 0: 1 }], ['LEAD', { 0: 1, 1: 1 }]],
  nyx: [['CLEAN', { 0: 0 }], ['RHYTHM', { 0: 1 }], ['LEAD', { 0: 1 }, 'lead']],
  strix: [['CLEAN', { 0: 0 }], ['RHYTHM', { 0: 1 }], ['LEAD', { 0: 1 }, 'lead']],
  talos: [['RHYTHM', { 0: 0 }], ['LEAD', { 0: 1 }]],
  typhon: [['CLEAN', { 0: 0 }], ['CRUNCH', { 0: 1 }], ['RHYTHM', { 0: 2 }], ['LEAD', { 0: 2 }, 'lead']],
  umbra: [['CLEAN', { 0: 0 }], ['CRUNCH', { 0: 1 }], ['RHYTHM', { 0: 2 }], ['LEAD', { 0: 2 }, 'lead']],
  vulcan: [['CLEAN', { 0: 0 }], ['CRUNCH', { 0: 1 }], ['RHYTHM', { 0: 2 }], ['LEAD', { 0: 3 }]],
};
/** The sounds an amp makes: [folder, dims, gain] from the table, or from its role for amps not in it. */
function ampSounds(a) {
  if (a.type === 'nam' || a.key === 'none') return [];
  if (AMP_SOUNDS[a.key]) return AMP_SOUNDS[a.key];
  const r = (a.role || '') + ' ' + (a.description || ''), out = [];
  if (/clean/i.test(r)) out.push(['CLEAN']); if (/crunch/i.test(r)) out.push(['CRUNCH']);
  if (/high gain|metal/i.test(r)) out.push(['RHYTHM']); if (/lead|high gain/i.test(r)) out.push(['LEAD', null, /high gain/i.test(r) ? 'lead' : null]);
  if (/acoustic/i.test(r)) out.push(['ACOUSTIC']);
  return out;
}
/** What the tile says about the sound: the channel's name on the amp, or the gain it starts at. */
function soundLabel(a, snd) {
  const dims = (a.face && a.face.dims) || [], parts = [];
  for (const i in (snd[1] || {})) { const d = dims[+i]; const n = d && d.names && d.names[snd[1][i]]; if (n) parts.push(n); }
  if (snd[2] === 'lead') parts.push('more gain');
  else if (typeof snd[2] === 'number') parts.push('gain ' + snd[2]);
  return parts.join(' · ');
}
/** Start an amp the way a folder means it: its channel / buttons and gain. */
async function spawnAmp(key, snd) {
  let r = await call('setAmp', key);
  // Eidolon starts with empty slots: start on slot 1
  if ((AMPS.find(x => x.key === key) || {}).type === 'nam' && ampUi.panel) { ampUi.panel.dataset.ch = 0; vals['amp.channel'] = 0; queueScan(); }
  if (snd) {
    for (const i in (snd[1] || {})) { const rr = await call('ampDim', +i, snd[1][i]); if (rr) r = rr; }
    const a = AMPS.find(x => x.key === key) || {};
    let g = null;
    if (typeof snd[2] === 'number') g = toNorm('amp.gain', snd[2]);
    else if (snd[2] === 'lead') { const k = (a.knobs || []).find(k => k.param === 'amp.gain'); g = Math.min(1, (k ? +k.def : vals['amp.gain'] || .5) + .15); }
    if (g != null) { await new Promise(res => setTimeout(res, 40)); applyParams({ 'amp.gain': g }); call('setParam', 'amp.gain', g); }
  }
  if (r) applyRig(r);
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
    // Folders by sound (CLEAN, CRUNCH, RHYTHM, LEAD...), ALL AMPS, and the folders of amps you added.
    // The NAM loader sits on its own at the top. Searching shows matching amps from everywhere.
    const folder = p._folder || '';
    const flat = !folder && !!q;
    const own = AMPS.filter(a => a.folder && a.folder !== 'OBSIDIAN AMPS' && a.folder !== 'CAPTURE AMPS');
    const tiles = [];                                        // [amp, sound or null]
    let head = '';
    if (folder) {
      const sf = SOUND_FOLDERS.find(f => f[0] === folder);
      if (sf) AMPS.forEach(a => ampSounds(a).forEach(snd => { if (snd[0] === folder) tiles.push([a, snd]); }));
      else if (folder === 'ALL AMPS') AMPS.filter(a => a.key !== 'none').slice().sort((x, y) => x.name.localeCompare(y.name)).forEach(a => tiles.push([a, null]));
      else AMPS.filter(a => a.folder === folder).forEach(a => tiles.push([a, null]));
      head = '<div class="obs-crumb"><button class="obs-btn obs-crumb-back">‹  FOLDERS</button><span>' + esc(folder) + '</span><em>' + tiles.length + (tiles.length === 1 ? ' amp' : ' amps') + '</em></div>';
    } else if (flat) {
      AMPS.filter(a => a.key !== 'none' && match(a)).forEach(a => tiles.push([a, null]));
    } else {
      const fold = (f, sub, list) => {
        const here = list.find(x => (x[0] || x).key === cur);
        return '<button class="obs-tile obs-ampfolder obs-af-' + esc(f.toLowerCase().replace(/[^a-z]+/g, '-')) + (here ? ' has' : '') + '" data-amp-folder="' + esc(f) + '"><i>' + list.length + ' AMPS</i><b>' + esc(f) + '</b><small>'
          + esc(here ? 'Playing now: ' + (here[0] || here).name : sub) + '</small></button>'; };
      head = SOUND_FOLDERS.map(([f, sub]) => { const list = []; AMPS.forEach(a => ampSounds(a).forEach(snd => { if (snd[0] === f) list.push([a, snd]); })); return list.length ? fold(f, sub, list) : ''; }).join('')
        + fold('ALL AMPS', 'Every amp, A to Z', AMPS.filter(a => a.key !== 'none'))
        + [...new Set(own.map(a => a.folder))].map(f => fold(f, 'Amps you added', own.filter(a => a.folder === f))).join('');
      AMPS.filter(a => a.type === 'nam').forEach(a => tiles.push([a, null]));          // the NAM loader
    }
    const pic = (a, snd) => {
      if (!a.face || !a.face.skins) return '';
      const d0 = a.face.dims && a.face.dims[0], pos = snd && snd[1] && snd[1][0] != null ? snd[1][0] : d0 && d0.start || 0;
      return '<span class="obs-tilepic" style="background-image:url(\'faces/img/' + esc(a.face.skins[d0 && d0.skin ? Math.min(a.face.skins.length - 1, pos) : 0].id) + '_on.webp\')"></span>'; };
    p._tiles = tiles;
    p._grid.innerHTML = head + tiles.map(([a, snd], i) => {
      const lbl = snd ? soundLabel(a, snd) : '';
      return '<button class="obs-tile' + (a.key === cur ? ' on' : '') + (a.face ? ' haspic' : '') + '" data-amp="' + esc(a.key) + '" data-tile="' + i + '" title="' + esc(a.description) + '">' + pic(a, snd)
        + '<i>' + esc(lbl || a.role || a.type) + '</i><b>' + esc(a.name) + '</b><small>' + esc(a.basedOn ? (/^Obsidian original/i.test(a.basedOn) ? a.basedOn : 'Based on ' + a.basedOn) : a.description) + '</small></button>'; }).join('')
      + (tiles.length || (!folder && !flat && head) ? '' : '<div class="obs-empty">No amps match.</div>');
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
  if (!has) { safe('face', () => renderFace({}, null)); return; }
  const info = AMPS.find(x => x.key === a.key) || {};
  safe('face', () => renderFace(a, info));
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
/* ============================================================== capture amp faces
   The amp pack's amps (Documents\Obsidian Amp Suite\Amps + \Faces) show their real face: the layer
   renders from visuals/finished (face on / off, one pointer sprite per knob, one sprite per lit LED).
   Knobs that are plugin parameters (gain, bass, middle...) move those parameters (automation, MIDI
   learn and presets follow); the other knobs go to the Rig (ampKnob), buttons pick channels and modes
   (ampDim), switches toggle (ampSwitch), and the amp's lamp is the AMP section's on / off. */
const faceUi = { key: '', info: null, stage: null, box: null, read: null, meters: {}, sendT: {}, readHtml: '' };
const FACE_BAR = 64;          // the name / readout bar under the amp, with its gap
/** The amp is as large as the AMP page allows (the page has nothing else on it now). */
function faceMax() {
  const pg = ampUi.box && ampUi.box.parentElement, w = (pg && pg.clientWidth) || 1376, h = (pg && pg.clientHeight) || 592;
  return [Math.min(1300, w - 70), h - 36 - FACE_BAR];
}
function stdKnob(id) { return ampUi.panel ? $$('.obs-knobs .kn', ampUi.panel).find(k => k.dataset.param === id) : null; }
function faceCh() { return Math.round(clamp(vals['amp.channel'] || 0, 0, 1) * 3); }
function faceOn() { return !(typeof bypassed !== 'undefined' && bypassed.has('AMP')); }
function faceState() { return (rig && rig.amp && rig.amp.face) || {}; }
function faceKnobVal(j) {
  const k = faceUi.info.knobs[j];
  if (k.role === 'browse') return faceCh() / 3;            // Eidolon: BROWSE sweeps the four capture slots
  if (k.param) { const n = vals[k.param]; return n === undefined ? k.def : clamp(toReal(k.param, n) / 10, 0, 1); }
  const f = faceState(), v = f.knobs ? f.knobs[j] : undefined;
  return v === undefined || v === null ? k.def : +v;
}
function faceDimVal(i) {
  const d = faceUi.info.face.dims[i];
  if (i === 0 && d.id === 'channel') return faceCh();
  const f = faceState(), per = f.dims && f.dims[i];
  return per ? (+per[faceCh()] || 0) : (d.start || 0);
}
function faceDimCount(i) {
  const F = faceUi.info.face, d = F.dims[i];
  if (d.names) return d.names.length;
  const ch = String(faceDimVal(0));
  return Math.max(1, new Set(Object.keys(F.caps).filter(k => k.split('|')[0] === ch).map(k => k.split('|')[i])).size);
}
function faceCap() { const F = faceUi.info.face, k = F.dims.map((d, i) => faceDimVal(i)).join('|'); return F.caps[k] || F.caps[k || '_'] || {}; }
function faceDimName(i) { const d = faceUi.info.face.dims[i]; return d.names ? (d.names[faceDimVal(i)] || '') : (faceCap().name || ''); }
function faceBtnLabel(id) {
  const F = faceUi.info.face;
  if (F.loader) return { load: 'LOAD: open a .nam capture into this slot (or the next empty one)', prev: 'PREV: the capture before', next: 'NEXT: the next capture' }[id] || id;
  const w = (F.switches || []).find(w => w.button === id);
  if (w) return w.label;
  const d = F.dims.find(d => d.button === id || (d.buttons || []).includes(id));
  return d ? (d.buttons ? d.names[d.buttons.indexOf(id)] : d.id[0].toUpperCase() + d.id.slice(1)) : id;
}
function faceThrottle(key, fn) {
  const t = faceUi.sendT[key] || (faceUi.sendT[key] = { last: 0, timer: 0 }), now = performance.now();
  clearTimeout(t.timer);
  if (now - t.last > 30) { t.last = now; fn(); } else t.timer = setTimeout(() => { t.last = performance.now(); fn(); }, 30);
}
function faceSetKnob(j, v) {
  const k = faceUi.info.knobs[j];
  v = clamp(v, 0, 1); if (k.steps > 1) v = Math.round(v * (k.steps - 1)) / (k.steps - 1);
  if (k.role === 'browse') { loaderSlot(Math.round(v * 3)); return; }
  if (k.param) {
    const el = stdKnob(k.param), n = clamp(toNorm(k.param, v * 10), 0, 1);
    if (el) { setKnob(el, n); if (window.knobHook) window.knobHook(el, n); }
    vals[k.param] = n;
    queueScan();
  } else {
    const f = faceState(); if (f.knobs) f.knobs[j] = v;
    faceThrottle('k' + j, () => call('ampKnob', j, v));
  }
  drawFace();
}
/* ---- Eidolon (the NAM loader): four capture slots ---- */
function loaderSlots() { return (rig && rig.amp && rig.amp.channels) || []; }
function loaderSlot(ch) {
  ch = clamp(ch, 0, 3); if (ch === faceCh()) return;
  ampUi.panel.dataset.ch = ch; vals['amp.channel'] = ch / 3; queueScan(); drawFace();
}
function loaderPress(id) {
  const sl = loaderSlots(), cur = faceCh(), full = sl.map((c, i) => c && c.file ? i : -1).filter(i => i >= 0);
  if (id === 'load') {                       // the slot you're on if it's empty, else the next empty one, else replace this one
    const empty = [0, 1, 2, 3].map(i => (cur + i) % 4).find(i => !(sl[i] && sl[i].file));
    const to = empty === undefined ? cur : empty;
    loaderSlot(to); call('loadCapture', to); return;
  }
  if (!full.length) { say('No captures yet: press LOAD to open a .nam file'); return; }
  const d = id === 'prev' ? -1 : 1;
  for (let i = 1; i <= 4; i++) { const c = (cur + d * i + 8) % 4; if (full.includes(c)) { loaderSlot(c); return; } }
}
function facePress(id) {
  const F = faceUi.info.face, f = faceState();
  if (F.loader) { loaderPress(id); return; }
  const si = (F.switches || []).findIndex(w => w.button === id);
  if (si >= 0) {
    const on = !(f.switches && f.switches[si]); if (f.switches) f.switches[si] = on;
    call('ampSwitch', si, on).then(r => r && applyRig(r)); drawFace(); return;
  }
  const i = F.dims.findIndex(d => d.button === id || (d.buttons || []).includes(id));
  if (i < 0) return;
  const d = F.dims[i], v = d.buttons ? d.buttons.indexOf(id) : (faceDimVal(i) + 1) % faceDimCount(i);
  if (i === 0 && d.id === 'channel') { vals['amp.channel'] = v / 3; last['amp.channel'] = v / 3; ampUi.panel.dataset.ch = v; }
  else if (f.dims && f.dims[i]) { if (d.per) f.dims[i][faceCh()] = v; else f.dims[i] = f.dims[i].map(() => v); }
  call('ampDim', i, v).then(r => r && applyRig(r));
  drawFace();
}
function facePower() {
  if (typeof bypassed === 'undefined') return;
  if (bypassed.has('AMP')) bypassed.delete('AMP'); else bypassed.add('AMP');
  if (typeof styleTabs === 'function') styleTabs();
  queueScan(); drawFace(true);
}
function faceTip(j) {
  const F = faceUi.info.face, k = F.knobs[j], tip = $('.ff-tip', faceUi.stage), v = faceKnobVal(j);
  tip.textContent = k.stepNames ? k.label + ' · ' + k.stepNames[Math.round(v * (k.steps - 1))] : k.label + '  ' + (v * 10).toFixed(1);
  tip.style.left = (k.c[0] / F.W * 100) + '%'; tip.style.top = ((k.c[1] - k.r) / F.H * 100) + '%';
  tip.classList.add('on');
}
function buildFace() {
  const I = faceUi.info, F = I.face, W = F.W, H = F.H, pc = (v, t) => (v / t * 100).toFixed(4) + '%';
  const st = faceUi.stage, nd = ' alt="" draggable="false"';
  let h = '';
  F.skins.forEach((sk, i) => { h += '<img class="ff-base off" data-s="' + i + '" src="faces/img/' + sk.id + '_off.webp"' + nd + '><img class="ff-base on" data-s="' + i + '" src="faces/img/' + sk.id + '_on.webp"' + nd + '>'; });
  F.skins.forEach((sk, i) => F.knobs.forEach((k, j) => {
    const [x, y, w, hh] = k.b, pos = 'left:' + pc(x, W) + ';top:' + pc(y, H) + ';width:' + pc(w, W) + ';height:' + pc(hh, H) + ';transform-origin:' + k.o[0] + '% ' + k.o[1] + '%';
    h += '<img class="ff-ptr" data-s="' + i + '" data-k="' + j + '" data-v="on" src="faces/img/' + sk.id + '_k' + j + '.png" style="' + pos + '"' + nd + '>';
    if (sk.ptrOff) h += '<img class="ff-ptr" data-s="' + i + '" data-k="' + j + '" data-v="off" src="faces/img/' + sk.id + '_k' + j + 'o.png" style="' + pos + '"' + nd + '>';
  }));
  for (const id in F.leds) { const [x, y, w, hh] = F.leds[id]; h += '<img class="ff-led" data-l="' + esc(id) + '" src="faces/img/' + F.key + '_led_' + id + '.png" style="left:' + pc(x, W) + ';top:' + pc(y, H) + ';width:' + pc(w, W) + ';height:' + pc(hh, H) + '"' + nd + '>'; }
  F.knobs.forEach((k, j) => { const r = k.r * 1.15; h += '<button class="ff-hot" data-k="' + j + '" aria-label="' + esc(k.label) + '" style="left:' + pc(k.c[0] - r, W) + ';top:' + pc(k.c[1] - r, H) + ';width:' + pc(2 * r, W) + ';height:' + pc(2 * r, H) + '"></button>'; });
  for (const id in F.buttons) { const [x, y, w, hh] = F.buttons[id]; h += '<button class="ff-hot ff-btn" data-b="' + esc(id) + '" title="' + esc(faceBtnLabel(id)) + '" style="left:' + pc(x - 6, W) + ';top:' + pc(y - 6, H) + ';width:' + pc(w + 12, W) + ';height:' + pc(hh + 12, H) + '"></button>'; }
  if (F.loader && F.screen) {               // Eidolon's screen: the slot and capture up top, each knob's value above its knob
    const [sx, sy, sw, sh] = F.screen;
    h += '<div class="ff-screen" style="left:' + pc(sx, W) + ';top:' + pc(sy, H) + ';width:' + pc(sw, W) + ';height:' + pc(sh, H) + '"><div class="sc-top"><span class="sc-slot"></span><b class="sc-name"></b><span class="sc-gear"></span><span class="sc-kind"></span></div><div class="sc-vals">'
      + F.knobs.map((k, j) => I.knobs[j] && I.knobs[j].role === 'browse' ? '' : '<span data-k="' + j + '" style="left:' + ((k.c[0] - sx) / sw * 100).toFixed(3) + '%"><em>' + esc(k.label.toUpperCase()) + '</em><b></b></span>').join('') + '</div></div>';
  }
  const [px, py, pr] = F.power, R = pr * 1.25;
  h += '<button class="ff-hot ff-pw" title="Power: the AMP section on / off" style="left:' + pc(px - R, W) + ';top:' + pc(py - R, H) + ';width:' + pc(2 * R, W) + ';height:' + pc(2 * R, H) + '"></button><div class="ff-tip"></div>';
  st.innerHTML = h;
  const [mw, mh] = faceMax(), sc = Math.min(mw / W, mh / H);
  st.style.width = Math.round(W * sc) + 'px'; st.style.height = Math.round(H * sc) + 'px';
  $$('.ff-hot[data-k]', st).forEach(b => {
    const j = +b.dataset.k; let y0 = 0, v0 = 0, pid = null;
    b.title = F.knobs[j].label + (I.knobs[j] && I.knobs[j].param ? '' : '') + ' · drag up or down, double-click to reset';
    b.addEventListener('pointerdown', e => { if (e.button !== 0) return; pid = e.pointerId; b.setPointerCapture(pid); y0 = e.clientY; v0 = faceKnobVal(j); st.classList.add('drag'); faceUi.dragK = j; faceTip(j); e.preventDefault(); });
    b.addEventListener('pointermove', e => { if (pid !== e.pointerId) return; faceSetKnob(j, v0 + (y0 - e.clientY) / (e.shiftKey ? 900 : 260)); faceTip(j); });
    const end = e => { if (pid !== e.pointerId) return; pid = null; st.classList.remove('drag'); faceUi.dragK = -1; $('.ff-tip', st).classList.remove('on'); drawFace(); };
    b.addEventListener('pointerup', end); b.addEventListener('pointercancel', end);
    b.addEventListener('wheel', e => { e.preventDefault(); const ns = I.knobs[j].steps; faceSetKnob(j, faceKnobVal(j) - Math.sign(e.deltaY) * (ns > 1 ? 1 / (ns - 1) : .02)); faceTip(j);
      clearTimeout(faceUi.tipT); faceUi.tipT = setTimeout(() => $('.ff-tip', st).classList.remove('on'), 700); }, { passive: false });
    b.addEventListener('dblclick', () => faceSetKnob(j, I.knobs[j].def));
  });
  $$('.ff-btn', st).forEach(b => b.addEventListener('click', e => { e.stopPropagation(); facePress(b.dataset.b); }));
  $('.ff-pw', st).addEventListener('click', e => { e.stopPropagation(); facePower(); });
}
function drawFace(pwr) {
  const st = faceUi.stage; if (!st || !faceUi.info || !rig) return;
  const F = faceUi.info.face, on = faceOn();
  const sk = F.dims[0] && F.dims[0].skin ? clamp(faceDimVal(0), 0, F.skins.length - 1) : 0;
  $$('.ff-base', st).forEach(im => {
    const mine = +im.dataset.s === sk, isOn = im.classList.contains('on');
    im.style.transition = isOn && pwr ? (on ? 'opacity 1.7s cubic-bezier(.6,0,.3,1)' : 'opacity .7s ease-out') : 'opacity .4s ease';
    im.style.opacity = mine ? (isOn ? (on ? 1 : 0) : 1) : 0;
  });
  const off = F.skins[sk].ptrOff;
  $$('.ff-ptr', st).forEach(im => {
    const mine = +im.dataset.s === sk;
    im.style.transform = 'rotate(' + ((faceKnobVal(+im.dataset.k) - .5) * 270).toFixed(2) + 'deg)';
    im.style.opacity = !mine ? 0 : (!off ? 1 : ((im.dataset.v === 'on') === on ? 1 : 0));
  });
  const lit = new Set(), f = faceState();
  F.dims.forEach((d, i) => ((d.leds || [])[faceDimVal(i)] || []).forEach(l => lit.add(l)));
  (F.switches || []).forEach((w, i) => { if (f.switches && f.switches[i]) lit.add(w.led); });
  $$('.ff-led', st).forEach(im => { im.style.opacity = on && lit.has(im.dataset.l) ? 1 : 0; });
  if (F.loader) loaderScreen(on);
  faceReadout();
}
function loaderScreen(on) {
  const sc = $('.ff-screen', faceUi.stage); if (!sc) return;
  const sl = loaderSlots(), ch = faceCh(), c = sl[ch] || {}, n = sl.filter(x => x && x.file).length;
  sc.classList.toggle('off', !on);
  const set = (q, t) => { const e = $(q, sc); if (e.textContent !== t) e.textContent = t; };
  set('.sc-slot', 'SLOT ' + (ch + 1) + ' / 4' + (n ? '  ·  ' + n + ' LOADED' : ''));
  set('.sc-name', c.file ? (c.title || c.name || 'Capture') : 'Empty slot');
  set('.sc-gear', c.file ? (c.gear ? c.gear + (c.by ? '  ·  by ' + c.by : '') : (c.title && c.name !== c.title ? c.name : '')) : 'Press LOAD to open a .nam capture');
  set('.sc-kind', c.file ? (c.kind || '').toUpperCase() : '');
  $$('.sc-vals span', sc).forEach(v => { const j = +v.dataset.k, b = v.querySelector('b'), t = (faceKnobVal(j) * 10).toFixed(1); if (b.textContent !== t) b.textContent = t; v.classList.toggle('hot', faceUi.dragK === j); });
}
function faceReadout() {
  const el = faceUi.read; if (!el || !faceUi.info) return;
  const I = faceUi.info, F = I.face, f = faceState(), m = faceUi.meters;
  if (F.loader) {                            // Eidolon: the slots, each one you can pick or empty
    const sl = loaderSlots(), ch = faceCh();
    const html = '<span class="obs-fslots">' + [0, 1, 2, 3].map(i => { const c = sl[i] || {};
      return '<span class="sl' + (i === ch ? ' on' : '') + (c.file ? ' full' : '') + '" data-slot="' + i + '" title="' + esc(c.file || 'Empty: press LOAD') + '"><i>' + (i + 1) + '</i>' + esc(c.file ? (c.title || c.name) : 'empty')
        + (c.file ? '<u data-clear="' + i + '" title="Empty this slot">✕</u>' : '') + '</span>'; }).join('') + '</span>';
    if (html !== faceUi.readHtml) {
      faceUi.readHtml = html; el.innerHTML = html;
      el.onclick = e => { const x = e.target.closest('[data-clear]'), s = e.target.closest('[data-slot]'); e.stopPropagation();
        if (x) { call('clearCapture', +x.dataset.clear); return; } if (s) loaderSlot(+s.dataset.slot); };
    }
    return;
  }
  // what you'd read on the amp itself: channel, mode / voice, switches. No file names.
  let ch = F.dims.length ? F.dims.map((d, i) => (d.id === 'boost' && d.names && faceDimVal(i) === 0) ? '' : faceDimName(i)).filter(Boolean).join(' · ') : 'Single channel';
  if (F.sel) { const si = Math.round(faceKnobVal(F.sel.knob) * (F.sel.names.length - 1)); if (si > 0) ch = F.sel.label + ' · ' + F.sel.names[si]; }
  (F.switches || []).forEach((w, i) => { if (f.switches && f.switches[i]) ch += ' · ' + w.label; });
  const note = (faceCap().note || '');
  let tail;
  if (!faceOn()) tail = 'Amp off';
  else if (f.ok === false) tail = 'Amp pack not installed';
  else if (!f.loaded) tail = 'Loading the amp…';
  else tail = note ? note.charAt(0).toUpperCase() + note.slice(1) : '';
  const html = '<b>' + esc(ch) + '</b>' + (I.role ? '<u>' + esc(I.role) + '</u>' : '') + '<span>' + esc(tail) + '</span>'
    + (m.cabIn && faceOn() && f.loaded ? '<em>CAB INCLUDED IN THIS CHANNEL</em>' : '');
  if (html !== faceUi.readHtml) { faceUi.readHtml = html; el.innerHTML = html; }
}
function faceMeters(m) {
  const fm = faceUi.meters;
  if (fm.cabIn === !!m.cabIn) return;
  fm.cabIn = !!m.cabIn;
  if (faceUi.info) faceReadout();
}
/** Called from renderAmp: shows the face for a capture amp, the plain panel for everything else. */
function renderFace(a, info) {
  const p = ampUi.panel, isFace = (a.type === 'capture' || a.type === 'nam') && info && info.face && info.knobs;
  p.classList.toggle('face', !!isFace);
  if (!faceUi.box) {
    faceUi.box = document.createElement('div'); faceUi.box.className = 'obs-facewrap';
    faceUi.box.innerHTML = '<div class="obs-face"></div><div class="obs-facebar"><div class="obs-faceinfo"><i></i><div class="obs-faceread"></div></div>'
      + '<button class="obs-btn obs-fchange">CHANGE AMP</button><button class="obs-btn obs-fremove">REMOVE</button></div>';
    p.appendChild(faceUi.box);
    faceUi.stage = $('.obs-face', faceUi.box); faceUi.read = $('.obs-faceread', faceUi.box);
    $('.obs-fchange', faceUi.box).onclick = e => { e.stopPropagation(); ampUi.addBtn.click(); };
    $('.obs-fremove', faceUi.box).onclick = e => { e.stopPropagation(); call('setAmp', 'none').then(r => r && applyRig(r)); };
    setInterval(() => { if (faceUi.info && p.offsetParent && !p.classList.contains('obs-hidden')) drawFace(); }, 120);
  }
  faceUi.box.classList.toggle('obs-hidden', !isFace);
  if (!isFace) { faceUi.info = null; faceUi.key = ''; return; }
  faceUi.info = info;
  if (faceUi.key !== a.key) { faceUi.key = a.key; faceUi.readHtml = ''; buildFace(); }
  $('.obs-faceinfo i', faceUi.box).textContent = (info.face.name || info.name || '').toUpperCase() + '  ·  ' + (info.face.loader ? 'NAM LOADER · YOUR OWN CAPTURES' : info.basedOn ? 'BASED ON ' + info.basedOn.toUpperCase() : 'CAPTURE AMP');
  $('.obs-faceinfo i', faceUi.box).title = info.description || '';
  drawFace();
}
function renderCab() {
  if (!rig) return;
  const c = rig.cab || {}, has = c.key && c.key !== 'none';
  const label = (c.irAName || c.irBName) ? [c.irAName, c.irBName].filter(Boolean).join(' + ') : (c.name || '');
  const card = (big) => '<span style="display:flex;flex-direction:column;align-items:center;gap:' + (big ? 10 : 6) + 'px"><span style="font-size:' + (big ? 10 : 9) + 'px;letter-spacing:.24em;color:var(--accent)">CAB</span><span style="font-size:' + (big ? 20 : 15) + 'px;font-weight:600;letter-spacing:.14em">' + esc(c.name || 'Cab') + '</span><span style="font-size:11px;color:rgb(var(--cf_8d8d93))">' + esc(label !== c.name ? label : 'Click to change') + '</span></span>';
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
  // Live UI: the look loads from Downloads\ObsidianUI (newer than this build), so UI updates need no reinstall
  if (I && !$('.obs-liveui', I)) {
    const row = document.createElement('div'); row.className = 'obs-liveui';
    row.innerHTML = '<div><b>Live UI</b><span>Loads the look (pages, themes, faces) from Downloads\\ObsidianUI when it is newer than this version. Updates then need no reinstall.</span></div>'
      + '<button class="obs-btn obs-livereload" title="Reload the interface now">RELOAD</button>';
    const tg = $('.tog', I) ? $('.tog', I).cloneNode(true) : document.createElement('button');
    tg.setAttribute('aria-label', 'Live UI'); tg.classList.add('obs-livetog'); row.appendChild(tg);
    I.appendChild(row);
    const drawLive = () => { if (typeof setTog === 'function') setTog(tg, !!INIT.settings.liveUi); };
    drawLive();
    tg.addEventListener('click', e => { e.stopPropagation(); e.preventDefault(); setS('liveUi', !INIT.settings.liveUi); drawLive();
      say(INIT.settings.liveUi ? 'Live UI on: press RELOAD (or reopen the window) after an update' : 'Live UI off'); }, true);
    $('.obs-livereload', row).addEventListener('click', e => { e.stopPropagation(); location.reload(); });
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
