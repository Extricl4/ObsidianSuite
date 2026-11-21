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
  const commands = {
    ready: () => { startMeters(); return S; },
    init: () => S,
    setParam: (id, v) => { params[id] = v; return true; },
    beginGesture: () => true, endGesture: () => true,
    getRig: () => S.rig,
    setModule: (r, s, k) => setModule(r, s, k),
    moveModule: (r, a, b) => { const slots = S.rig.racks[r]; const [x] = slots.splice(a, 1); slots.splice(b, 0, x); later('obs:rig', S.rig, 10); return S.rig; },
    setAmp: key => { const a = (S.amps || []).find(x => x.key === key); S.rig.amp = key === 'none' || !a ? { key: 'none', type: 'none', name: '', channels: [] } : { key, type: a.type, name: a.name, channels: [0, 1, 2, 3].map(() => ({ file: '', name: '', loaded: false })) }; later('obs:rig', S.rig, 10); return S.rig; },
    setCab: key => { const c = (S.cabs || []).find(x => x.key === key) || {}; S.rig.cab = { key, name: c.name || key, irA: '', irB: '', irAName: '', irBName: '' }; later('obs:rig', S.rig, 10); return S.rig; },
    presetList: () => S.presets,
    presetLoad: id => { const p = S.presets.find(x => x.id === id); if (!p) return false; S.preset = Object.assign({}, S.preset, { id: p.id, name: p.name, category: p.category, factory: p.factory, dirty: false }); later('obs:preset', S.preset, 10); return true; },
    presetNext: d => { const i = S.presets.findIndex(x => x.id === S.preset.id); const p = S.presets[(i + d + S.presets.length) % S.presets.length]; return commands.presetLoad(p.id); },
    presetInit: () => { S.preset = Object.assign({}, S.preset, { id: 'factory:Default', name: 'Default', factory: true, dirty: false }); later('obs:preset', S.preset, 10); return true; },
    presetSave: () => true,
    presetSaveAs: (name, cat) => { const p = { id: 'user:' + name, name, category: cat, factory: false }; S.presets.push(p); later('obs:presets', S.presets, 5); return commands.presetLoad(p.id); },
    presetDelete: () => true, presetRename: () => true, presetRescan: () => S.presets,
    getSettings: () => S.settings,
    setSetting: (k, v) => { S.settings[k] = v; return true; },
    midiMappings: () => S.midi, midiRemove: i => { S.midi.splice(i, 1); return S.midi; },
    audioSetup: () => S.audio,
  };
  let meterTimer = null;
  function startMeters() {
    if (meterTimer) return;
    let t = 0;
    meterTimer = setInterval(() => {
      t += 1;
      const lvl = 0.25 + 0.2 * Math.sin(t / 7);
      emit('obs:meters', { in: lvl, outL: lvl * 0.9, outR: lvl * 0.85, gate: 1, cpu: 0.06, clip: false,
        tuner: { hz: 82.41 * Math.pow(2, (Math.sin(t / 15) * 8) / 1200), conf: 0.9, level: lvl }, beat: -1, synth: 0, bpm: 120, busy: false, latencyMs: 0 });
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
    log, params, emit,
    load(state) { S = state; (S.params || []).forEach(p => params[p.id] = p.norm); },
    sent(id) { const l = log.filter(x => x[0] === 'setParam' && x[1] === id); return l.length ? l[l.length - 1][2] : undefined; },
  };
})();
