/* =============================================================================
   Fake audio engine for testing the plugin UI in a normal browser.
   Loaded before the page; it pretends to be JUCE's window.__JUCE__ so
   WebUI/obsidian-bridge.js runs exactly as inside the plugin.

   state.json comes from the real plugin:
       "Obsidian Suite" --dump-ui-state state.json
   Everything the UI sends is recorded in window.__mock.log.
   ============================================================================= */
(function () {
  const listeners = {};
  const log = [];
  let S = null;                  // initial state (set by the test before the page loads)
  const params = {};             // id -> norm
  const emit = (ev, payload) => (listeners[ev] || []).forEach(fn => { try { fn(payload); } catch (e) { console.error(e); } });
  const later = (ev, payload, ms = 0) => setTimeout(() => emit(ev, payload), ms);
  /* What a preset does (dumped from the real plugin by "--dump-presets", see make_web_preview.py):
     every knob back to Default except the ones the preset sets, and its rig (amp, cab, pedals, synth...). */
  function applyPreset(st) {
    if (!st || !S.presetDefaults) return;
    const vals = Object.assign({}, S.presetDefaults, st.p);
    Object.assign(params, vals); later('obs:params', vals, 15);
    if (st.rig) { S.rig = JSON.parse(JSON.stringify(st.rig)); later('obs:rig', S.rig, 10); }
  }
  let updTimer = null;
  const ST = { on: false, rec: false, ids: 0, arr: null, timer: 0 };
  const stNow = () => ({ pos: ST.from + (performance.now() - ST.t0) / 1000 * ((ST.arr && ST.arr.bpm) || 120) / 60 });
  function stStart(from, rec, track, recFrom) {
    clearInterval(ST.timer); Object.assign(ST, { on: true, rec, track, recFrom, from, t0: performance.now(), sent: 0 });
    ST.timer = setInterval(() => { const st = stNow(), m = { playing: true, recording: ST.rec, pos: st.pos, active: true };
      if (ST.rec && st.pos > ST.recFrom) { const need = Math.floor((st.pos - ST.recFrom) * 8); if (need > ST.sent) { m.peaks = Array.from({ length: need - ST.sent }, () => .3 + Math.random() * .6); ST.sent = need; } }
      emit('obs:studio', m); }, 33);
  }
  const setUpd = o => { Object.assign(S.update, o); later('obs:update', Object.assign({}, S.update), 0); };
  const listChanged = () => { S.presetFolders.sort((a, b) => a.localeCompare(b)); later('obs:presets', { list: S.presets, folders: S.presetFolders.slice() }, 5); };

  function cabMics(place) {     // like Rig::cabMics
    const cab = S.rig.cab, c = (S.cabs || []).find(x => x.key === cab.key) || {}, n = (c.speakers || []).length;
    cab.mics = ['A', 'B'].map((k, m) => { const ir = cab['ir' + k], sp = (c.spots || []).find(s => s.ir === ir);
      let spk = cab['spk' + k]; if (spk === undefined || spk >= n) spk = sp ? sp.spk[0] : Math.min(m, Math.max(0, n - 1));
      const real = !!ir && !/^builtin/.test(cab.key);
      return { spk, ang: cab['ang' + k] !== undefined ? cab['ang' + k] : (m ? -2.356 : -0.785), recPos: real ? (sp ? sp.pos : 0.3) : -1, recDist: real ? 2.5 : 0 }; });
    // like Rig::placeCabMics: an IR cab's mics start Flat, on their recorded spots
    if (place && cab.mics[0].recPos >= 0) {
      const v = {}; cab.mics.forEach((mm, m) => { const k = m ? 'b' : 'a'; v['cab.' + k + '.pos'] = mm.recPos; v['cab.' + k + '.dist'] = Math.sqrt(2.5 / 30); v['cab.' + k + '.mic'] = 1; }); v['cab.lc'] = 0; v['cab.hc'] = 1;
      Object.assign(params, v); later('obs:params', v, 15);
    }
  }
  function moduleType(key) { return (S.modules || []).find(m => m.key === key); }
  const RK = ['pre', 'post', 'sfx'];
  function setModule(r, s, key) {
    const slot = S.rig.racks[r][s];
    const m = moduleType(key);
    slot.key = m ? key : ''; slot.name = m ? m.name : ''; slot.file = ''; slot.fileName = '';
    if (m) {
      const vals = {};
      m.params.forEach((p, k) => { vals[RK[r] + '.' + (s + 1) + '.p' + (k + 1)] = p.defNorm; });
      vals[RK[r] + '.' + (s + 1) + '.on'] = 1;
      Object.assign(params, vals); later('obs:params', vals, 20);
    }
    later('obs:rig', S.rig, 10);
    return S.rig;
  }
  const echoDirty = new Set(); let echoTimer = 0;
  function startEcho() {
    if (echoTimer) return;
    echoTimer = setInterval(() => {
      if (echoDirty.size) { const o = {}; echoDirty.forEach(id => o[id] = Math.fround(params[id])); /* the plugin keeps 32-bit floats */ echoDirty.clear(); window.__mock.emit('obs:params', o); }
      if (window.__mock.rigDirty) { window.__mock.rigDirty = false; window.__mock.emit('obs:rig', JSON.parse(JSON.stringify(S.rig))); }
    }, 33);
  }
  const commands = {
    ready: () => { startMeters(); return S; },
    init: () => S,
    // with __mock.echo on, it answers like the plugin: a 30 Hz timer sends back every changed parameter (its value
    // then) and the rig after anything changed it (UiBridge::timerCallback)
    setParam: (id, v) => { params[id] = v; if (window.__mock.echo) { echoDirty.add(id); startEcho(); } return true; },
    beginGesture: () => true, endGesture: () => true,
    getRig: () => S.rig,
    setModule: (r, s, k) => setModule(r, s, k),
    moveModule: (r, a, b) => { const slots = S.rig.racks[r]; const [x] = slots.splice(a, 1); slots.splice(b, 0, x); later('obs:rig', S.rig, 10); return S.rig; },
    setAmp: key => {
      const a = (S.amps || []).find(x => x.key === key);
      S.rig.amp = key === 'none' || !a ? { key: 'none', type: 'none', name: '', channels: [] } : { key, type: a.type, name: a.name, channels: [0, 1, 2, 3].map(() => ({ file: '', name: '', loaded: false })) };
      if (a && a.type === 'capture') {            // like Rig::setAmp: the amp's defaults, buttons at their start
        const F = a.face, d0 = (F.dims[0] || {}).start || 0, vals = {};
        S.rig.amp.face = { dims: F.dims.map(d => [0, 1, 2, 3].map(c => !d.per || c === d0 ? (d.start || 0) : 0)), knobs: a.knobs.map(k => k.def), switches: (F.switches || []).map(() => false), loaded: true, ok: true };
        a.knobs.forEach(k => { if (k.param) vals[k.param] = k.def; });
        vals['amp.channel'] = (F.dims[0] && F.dims[0].id === 'channel' ? d0 : 0) / 3;
        Object.assign(params, vals); later('obs:params', vals, 15);
      }
      later('obs:rig', S.rig, 10); return S.rig;
    },
    ampDim: (i, v) => {
      const f = S.rig.amp.face; if (!f) return S.rig;
      const ch = Math.round((params['amp.channel'] || 0) * 3);
      if (i === 0 && (S.amps.find(x => x.key === S.rig.amp.key).face.dims[0] || {}).id === 'channel') { params['amp.channel'] = v / 3; later('obs:params', { 'amp.channel': v / 3 }, 10); }
      else f.dims[i][ch] = v;
      return S.rig;
    },
    ampKnob: (j, v) => { if (S.rig.amp.face) S.rig.amp.face.knobs[j] = v; return true; },
    ampSwitch: (i, on) => { if (S.rig.amp.face) S.rig.amp.face.switches[i] = !!on; return S.rig; },
    installIrPack: () => { (S.cabs || []).forEach(c => { if (c.pack) c.packReady = true; }); later('obs:cabs', JSON.parse(JSON.stringify(S.cabs)), 10); return true; },
    setCab: key => { const c = (S.cabs || []).find(x => x.key === key) || {}, v = (c.voices || [])[0] || c; S.rig.cab = { key, name: c.name || key, irA: v.irA || '', irB: v.irB || '', irAName: '', irBName: '', voice: 0 }; cabMics(true); later('obs:rig', S.rig, 10); return S.rig; },
    // like the plugin: your own IR goes on mic A, B or both (2), and the cab becomes "custom" (the IR loader shows it)
    loadIr: mic => { const c = S.rig.cab = S.rig.cab || {}, name = 'Test IR ' + (mic === 2 ? 'AB' : 'AB'[mic | 0]);
      if (!String(c.key || '').startsWith('custom')) Object.assign(c, { key: 'custom', name, voice: 0 });
      [0, 1].forEach(m => { if (mic === 2 || mic === m) { c[m ? 'irB' : 'irA'] = 'C:/IRs/' + name + '.wav'; c[m ? 'irBName' : 'irAName'] = name; } });
      later('obs:rig', S.rig, 10); return true; },
    setCabVoice: i => { const c = (S.cabs || []).find(x => x.key === S.rig.cab.key) || {}, vs = c.voices || []; if (!vs.length) return S.rig; i = Math.max(0, Math.min(vs.length - 1, i | 0));
      Object.assign(S.rig.cab, { irA: vs[i].irA, irB: vs[i].irB, voice: i }); delete S.rig.cab.spkA; delete S.rig.cab.spkB; cabMics(c.spots); later('obs:rig', S.rig, 10); return S.rig; },
    // like Rig::setCabMic: remember the speaker / angle; a cab recorded per spot switches the mic to the nearest real IR
    setCabMic: (mic, spk, ang, pos) => { const c = (S.cabs || []).find(x => x.key === S.rig.cab.key) || {}, k = mic ? 'B' : 'A';
      S.rig.cab['spk' + k] = spk | 0; S.rig.cab['ang' + k] = +ang;
      if (c.spots) { let on = c.spots.filter(s => s.spk.includes(spk | 0)); if (!on.length) on = c.spots;
        const b = on.reduce((a, s) => Math.abs(s.pos - pos) < Math.abs(a.pos - pos) ? s : a); S.rig.cab['ir' + k] = b.ir;
        S.rig.cab.voice = (c.voices || []).findIndex(v => v.irA === S.rig.cab.irA && v.irB === S.rig.cab.irB); }
      cabMics(); if (window.__mock.echo) { window.__mock.rigDirty = true; startEcho(); } return S.rig; },
    presetList: () => S.presets,
    presetLoad: id => { const p = S.presets.find(x => x.id === id); if (!p) return false; S.preset = Object.assign({}, S.preset, { id: p.id, name: p.name, category: p.category, factory: p.factory, dirty: false }); applyPreset(S.presetStates && S.presetStates[id]); later('obs:preset', S.preset, 10); return true; },
    presetNext: d => { const i = S.presets.findIndex(x => x.id === S.preset.id); const p = S.presets[(i + d + S.presets.length) % S.presets.length]; return commands.presetLoad(p.id); },
    presetInit: () => { S.preset = Object.assign({}, S.preset, { id: 'factory:Default', name: 'Default', factory: true, dirty: false }); if (S.presetDefaults) applyPreset({ p: {}, rig: S.presetDefaultRig }); later('obs:preset', S.preset, 10); return true; },
    presetSave: () => true,
    presetSaveAs: (name, cat, dir) => {
      const id = 'user:' + (dir ? dir + '/' : '') + name; S.presets = S.presets.filter(x => x.id !== id);
      const p = { id, name, category: cat, factory: false }; if (dir) { p.dir = dir; if (!S.presetFolders.includes(dir)) S.presetFolders.push(dir); }
      S.presets.push(p); if (S.presetStates) S.presetStates[id] = { p: Object.assign({}, params), rig: JSON.parse(JSON.stringify(S.rig)) };
      listChanged(); return commands.presetLoad(p.id); },
    presetDelete: id => { const p = S.presets.find(x => x.id === (id || S.preset.id)); if (!p || p.factory) return false; S.presets = S.presets.filter(x => x !== p); listChanged(); return true; },
    presetRename: () => true, presetRescan: () => S.presets,
    // your own folders, like PresetManager does on disk
    presetFolders: () => S.presetFolders.slice(),
    presetNewFolder: name => { name = String(name || '').replace(/[\\/:*?"<>|]/g, '').trim(); if (!name || S.presetFolders.includes(name)) return false; S.presetFolders.push(name); listChanged(); return true; },
    presetRenameFolder: (a, b) => { const i = S.presetFolders.indexOf(a); if (i < 0 || !b || S.presetFolders.includes(b)) return false; S.presetFolders[i] = b; S.presets.forEach(p => { if (!p.factory && p.dir === a) p.dir = b; }); listChanged(); return true; },
    presetDeleteFolder: a => { if (!S.presetFolders.includes(a)) return false; S.presetFolders = S.presetFolders.filter(x => x !== a); S.presets = S.presets.filter(p => p.factory || p.dir !== a); listChanged(); return true; },
    presetMove: (id, dir) => { const p = S.presets.find(x => x.id === id); if (!p || p.factory || (dir && !S.presetFolders.includes(dir))) return false; if (dir) p.dir = dir; else delete p.dir; listChanged(); return true; },
    // in-app updates (Settings > About), like obs::Updater: check -> available -> download -> ready -> install
    getUpdate: () => Object.assign({}, S.update),
    updateCheck: () => { setUpd({ state: 'checking', error: '' });
      setTimeout(() => setUpd({ state: 'available', latest: (S.build || 0) + 1, version: '0.2.0', title: 'Build ' + ((S.build || 0) + 1) + ' (0.2.0)',
        notes: 'Example notes: in the app this is what changed in the newest build.', size: 48 * 1048576, full: false }), 250);
      return Object.assign({}, S.update); },
    updateDownload: () => { if (!S.update.latest) return Object.assign({}, S.update); clearInterval(updTimer); setUpd({ state: 'downloading', progress: 0 });
      updTimer = setInterval(() => { const p = Math.min(1, (S.update.progress || 0) + 0.25); if (p < 1) setUpd({ progress: p }); else { clearInterval(updTimer); setUpd({ state: 'ready', progress: 1 }); } }, 120);
      return Object.assign({}, S.update); },
    updateCancel: () => { clearInterval(updTimer); setUpd({ state: 'available', progress: 0 }); return true; },
    updateInstall: () => { if (S.update.state !== 'ready') return { ok: false, message: 'Nothing downloaded yet' };
      setUpd({ state: 'installing' }); return { ok: true, message: 'This is the web preview: in the app the installer runs now and the app opens again on the new build.' }; },
    // the NAM loader (Eidolon): a .nam per slot; the file dialog is the plugin's, so this one just fills the slot
    loadCapture: ch => { const a = S.rig.amp; if (a.type !== 'nam') { const e = (S.amps || []).find(x => x.key === 'nam-loader') || {}; S.rig.amp = { key: 'nam-loader', type: 'nam', name: e.name || 'Eidolon', channels: [0, 1, 2, 3].map(() => ({ file: '', name: '', loaded: false })) }; }
      const n = ch + 1; S.rig.amp.channels[ch] = { file: 'C:/Captures/Example ' + n + '.nam', name: 'Example ' + n, title: ['5150 Red Lead', 'Plexi Crunch', 'Twin Clean', 'Uberschall Rhythm'][ch], gear: ['Peavey 5150', 'Marshall 1959', 'Fender Twin Reverb', 'Bogner Uberschall'][ch], kind: ch % 2 ? 'Full rig' : 'Amp only', by: 'Example', loaded: true };
      later('obs:rig', S.rig, 30); return true; },
    clearCapture: ch => { if (S.rig.amp.channels && S.rig.amp.channels[ch]) S.rig.amp.channels[ch] = { file: '', name: '', loaded: false }; later('obs:rig', S.rig, 10); return true; },
    // STUDIO: a pretend recorder (no audio here): the playhead runs, a take gets made-up peaks
    studioSync: a => { ST.arr = a; return true; },
    studioPlay: b => { stStart(b, false, 0, 0); return true; },
    studioRecord: (t, b, ci) => { stStart(b - ci, true, t, b); return true; },
    studioStop: () => { if (!ST.on) return true; const st = stNow(); clearInterval(ST.timer); ST.on = false;
      const msg = { playing: false, recording: false, pos: st.pos, active: false };
      if (ST.rec) { const len = Math.max(0, st.pos - ST.recFrom);
        msg.take = len < .2 ? { discarded: true, track: ST.track, start: ST.recFrom } : { id: ++ST.ids, track: ST.track, start: ST.recFrom, len, name: 'TAKE ' + ST.ids, peaks: Array.from({ length: Math.ceil(len * 8) }, () => .3 + Math.random() * .6), file: '' }; }
      ST.rec = false; later('obs:studio', msg, 30); return true; },
    studioExport: () => true, studioReveal: () => true,
    getSettings: () => S.settings,
    setSetting: (k, v) => { S.settings[k] = v; return true; },
    midiMappings: () => S.midi, midiRemove: i => { S.midi.splice(i, 1); return S.midi; },
    audioSetup: () => S.audio,
    eqSpectrum: on => { S.eqSpectrum = !!on; return true; },
  };
  let meterTimer = null;
  function startMeters() {
    if (meterTimer) return;
    let t = 0;
    meterTimer = setInterval(() => {
      t += 1;
      const lvl = 0.25 + 0.2 * Math.sin(t / 7);
      emit('obs:meters', { in: lvl, outL: lvl * 0.9, outR: lvl * 0.85, gate: 1, cpu: 0.06, clip: false,
        tuner: { hz: 82.41 * Math.pow(2, (Math.sin(t / 15) * 8) / 1200), conf: 0.9, level: lvl }, beat: -1, synth: 0, bpm: 120, busy: false, latencyMs: 0, synNote: -1, synStep: -1, synSrc: 0, ...(S.rig.amp && S.rig.amp.type === 'capture' ? { ampA: 0, ampB: -1, ampF: 0, cabIn: false } : {}), ...(window.__mock.meterExtra || {}) });
    }, 33);
  }
  window.__JUCE__ = {
    initialisationData: { __juce__functions: ['obsidianCall'] },
    backend: {
      addEventListener(ev, fn) { (listeners[ev] = listeners[ev] || []).push(fn); return [ev, fn]; },
      removeEventListener() {},
      emitEvent(ev, payload) {
        if (ev !== '__juce__invoke') return;
        const [cmd, ...args] = payload.params;
        log.push([cmd, ...args]);
        let result = true;
        try { result = commands[cmd] ? commands[cmd](...args) : true; } catch (e) { console.error('mock', cmd, e); result = null; }
        later('__juce__complete', { promiseId: payload.resultId, result: JSON.parse(JSON.stringify(result === undefined ? null : result)) });
      },
    },
  };
  window.__mock = {
    log, params, emit, get state() { return S; },
    load(state) { S = state; S.presetFolders = S.presetFolders || []; S.update = S.update || { state: 'idle', own: S.build || 0, canInstall: true }; (S.params || []).forEach(p => params[p.id] = p.norm); },
    sent(id) { const l = log.filter(x => x[0] === 'setParam' && x[1] === id); return l.length ? l[l.length - 1][2] : undefined; },
  };
})();
