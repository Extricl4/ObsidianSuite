#!/usr/bin/env python3
"""Makes the factory presets (plugin/Presets/**.json, built into the plugin).

    python3 plugin/tools/presets/make_presets.py [path to the Obsidian Amp Suite standalone]

The sounds are written below in a short form (factory.py: the Factory folder, artists.py: the
Artists folder). This script turns them into a spec with every name resolved (amp buttons, pedal
choices, synth settings) and runs the plugin's preset writer on it:

    "Obsidian Amp Suite" --write-factory-presets plugin/Presets <spec.json> [target dB]

which builds each preset on the real plugin, loads its captures and pedal files, plays a test
riff through it and sets the output level so every preset plays about equally loud. Presets with
a capture amp from the amp pack also name an in-house amp to use when the pack isn't installed.

Needs the amp pack and pedal pack in Documents/Obsidian Amp Suite (for the level match).
"""
import json, os, re, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
PLUGIN = HERE.parents[1]
ROOT = PLUGIN.parent
CONTENT = PLUGIN / 'Content'
OUT = PLUGIN / 'Presets'
DEFAULT_BIN = Path('/home/claude/build/ObsidianSuite_artefacts/Release/Standalone/Obsidian Amp Suite')
TARGET_DB = -26.0

# ------------------------------------------------------------------ what the plugin has
def plugin_state(binary):
    if '--state' in sys.argv:                       # a state dumped before (--dump-ui-state), e.g. while the plugin is busy
        return json.loads(Path(sys.argv[sys.argv.index('--state') + 1]).read_text(encoding='utf-8'))
    f = Path(tempfile.gettempdir()) / 'obsidian-ui-state.json'
    subprocess.run([str(binary), '--dump-ui-state', str(f)], check=True, capture_output=True, timeout=180)
    return json.loads(f.read_text(encoding='utf-8'))

class Gear:
    def __init__(self, state):
        self.modules = {m['key']: m for m in state['modules']}
        self.params = {p['id']: p for p in state['params']}
        self.amps = {a['key']: a for a in state['amps']}
        self.cabs = {c['key'] for c in state['cabs']}
        self.cabfiles = {f.stem: json.loads(f.read_text(encoding='utf-8')) for f in (CONTENT / 'Cabs').glob('*.json')}
        self.capture = {}
        for f in (CONTENT / 'Amps').glob('*.json'):
            amp = json.loads(f.read_text(encoding='utf-8').replace('"":', '"_":'))
            face = json.loads((CONTENT / 'Faces' / f.name).read_text(encoding='utf-8'))
            self.capture[f.stem] = (amp, face)

G = None

def choice(options, v, what):
    if isinstance(v, str):
        for i, o in enumerate(options):
            if o.lower() == v.lower(): return i
        raise SystemExit(f'{what}: no choice "{v}" (has {options})')
    return v

# ------------------------------------------------------------------ amps
class Amp:
    """A('umbra', channel='Channel 3', mode='Mark IIC+', Gain=6, Treble=7...): a capture amp from the
    amp pack, buttons by name. M('recto', gain=7, bass=6...): an in-house amp."""
    def __init__(self, key, dims=None, knobs=None, switches=(), fallback=None):
        self.key, self.dims, self.knobs, self.switches, self.fallback = key, dims or {}, knobs or {}, list(switches), fallback

def A(key, fallback=None, switches=(), **kw):
    if key not in G.capture: raise SystemExit(f'no capture amp {key}')
    amp, face = G.capture[key]
    dim_ids = [d['id'] for d in face['dims']]
    dims = {k: v for k, v in kw.items() if k in dim_ids}
    knobs = {k.replace('_', ' '): v for k, v in kw.items() if k not in dim_ids}
    return Amp(key, dims, knobs, switches, fallback)

def M(key, **kw):
    if key not in G.amps or G.amps[key]['type'] == 'capture': raise SystemExit(f'no in-house amp {key}')
    return Amp(key, knobs=kw)

def amp_spec(a, name):
    if a.key not in G.capture:
        return {'key': a.key, 'knobs': {k: float(v) for k, v in a.knobs.items()}}, None
    amp, face = G.capture[a.key]
    fdims = face['dims']
    chosen = []
    for i, d in enumerate(fdims):
        if d['id'] not in a.dims: chosen.append(d.get('start') or 0); continue
        v = a.dims[d['id']]
        if isinstance(v, str):
            if d.get('names'):
                v = choice(d['names'], v, f'{name}: {a.key} {d["id"]}')
            else:                                   # unnamed button (voice / mode): look the setting up by its name
                hits = [k for k, c in face['caps'].items() if c and (c.get('name') or c.get('label') or '').lower() == v.lower()
                        and all(int(k.split('|')[j]) == chosen[j] for j in range(i))]
                if not hits: raise SystemExit(f'{name}: {a.key} has no {d["id"]} "{v}" here')
                v = int(hits[0].split('|')[i])
        chosen.append(v)
    labels = [k['label'] for k in face['knobs']]
    knobs = {}
    for k, v in a.knobs.items():
        lab = next((l for l in labels if l.lower() == k.lower()), None)
        if lab is None: raise SystemExit(f'{name}: {a.key} has no knob "{k}" ({labels})')
        if isinstance(v, str):                      # selector knob position by name (Eos: Solo = 'PULSE')
            sel = face.get('sel') or {}
            names = sel.get('names') or []
            v = names.index(v) / (len(names) - 1) * 10
        knobs[lab] = float(v)
    sw_ids = [w['id'] for w in face.get('switches', [])]
    switches = [sw_ids.index(s) for s in a.switches]
    return {'key': a.key, 'dims': [[i, v] for i, v in enumerate(chosen) if fdims[i]['id'] in a.dims or i == 0],
            'knobs': knobs, 'switches': switches}, chosen

# in-house stand-ins when the amp pack isn't installed
FALLBACK = {'achlys': 'recto', 'ares': 'vh4', 'astraea': 'top30', 'belial': 'styx51', 'chrysaor': 'brit800', 'citadel': 'slo100',
            'dionysus': 'slo100', 'eos': 'glass65', 'erebus': 'vh4', 'eris': 'slo100', 'hecate': 'styx51', 'hyperion': 'slo100',
            'kratos': 'styx51', 'lamia': 'brit800', 'lyssa': 'styx51', 'melinoe': 'glass65', 'moirai': 'styx51', 'moros': 'core',
            'niflheim': 'core', 'nihil': 'core', 'nyx': 'vh4', 'orpheus': 'glass65', 'persephone': 'glass65', 'strix': 'recto',
            'talos': 'recto', 'thanatos': 'slo100', 'tyche': 'plexi68', 'typhon': 'styx51', 'umbra': 'recto', 'vulcan': 'vh4',
            'nemean': 'styx51', 'phlegethon': 'vh4'}

# ------------------------------------------------------------------ pedals and effects
class Fx:
    def __init__(self, key, on=True, **values): self.key, self.on, self.values = key, on, values

def fx(key, **kw): return Fx(key, **kw)
def off(key, **kw): return Fx(key, on=False, **kw)

def fx_spec(f, rack, name):
    m = G.modules.get(f.key)
    if not m: raise SystemExit(f'{name}: no effect {f.key}')
    if rack not in m['racks']: raise SystemExit(f'{name}: {f.key} can\'t go in {rack} (only {m["racks"]})')
    params = {p['name']: p for p in m['params']}
    vals = {}
    for k, v in f.values.items():
        k2 = {'Oct_1': '-1 Oct', 'Oct_2': '-2 Oct', 'Oct_up': '+1 Oct'}.get(k, k.replace('_', ' '))
        k2 = {'Pre delay': 'Pre-delay', 'Rec Play': 'Rec / Play'}.get(k2, k2)
        p = params.get(k2)
        if not p: raise SystemExit(f'{name}: {f.key} has no {k2} ({list(params)})')
        if isinstance(v, str): v = choice(p.get('choices') or [], v, f'{name}: {f.key} {k2}')
        if not (p['min'] - 1e-6 <= v <= p['max'] + 1e-6): raise SystemExit(f'{name}: {f.key} {k2} = {v} outside {p["min"]}..{p["max"]}')
        vals[k2] = float(v)
    d = {'key': f.key, 'values': vals}
    if not f.on: d['on'] = False
    return d

# ------------------------------------------------------------------ synth (the SIREN presets in ui/src/synth_presets.js)
SYNTH = None
def synth_presets():
    js = (ROOT / 'ui/src/synth_presets.js').read_text(encoding='utf-8').replace('<script>', '').replace('</script>', '')
    out = subprocess.run(['node', '-e', 'global.window={};' + js + ';process.stdout.write(JSON.stringify(window.OBS_SYNTH_PRESETS))'],
                         capture_output=True, text=True, check=True)
    return {p['name']: p for p in json.loads(out.stdout)}

STEP_DEF = dict(on=[1,0,1,1, 1,0,1,0, 1,0,1,1, 1,0,1,1], oct=[0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0], note=[0,0,2,4, 7,0,4,0, 0,0,2,4, 7,0,9,4])
def pack_steps(st):
    out = []
    for i in range(16):
        if st is None:
            on, acc, tie, octv, rat, ch, note = STEP_DEF['on'][i], int(i % 4 == 0), 0, STEP_DEF['oct'][i], 1, 100, STEP_DEF['note'][i]
        else:
            f = lambda s: 1 if s and i < len(s) and s[i] == 'x' else 0
            a = lambda l, d: l[i] if l and i < len(l) and l[i] is not None else d
            on = f(st['on']) if st.get('on') else 1
            acc, tie, octv, rat, ch, note = f(st.get('acc')), f(st.get('tie')), a(st.get('oct'), 0), a(st.get('rat'), 1), a(st.get('ch'), 100), a(st.get('note'), 0)
        out.append(on | acc << 1 | tie << 2 | ((octv + 1) & 7) << 3 | ((rat - 1) & 3) << 6 | (ch & 127) << 8 | (note & 15) << 15)
    return out

class Syn:
    """S('Cosmic Arp', mix=45, **more): the synth on, with one of its presets (plus changes)."""
    def __init__(self, preset=None, mix=40, steps=None, **kw): self.preset, self.mix, self.steps, self.kw = preset, mix, steps, kw

def S(preset=None, mix=40, steps=None, **kw): return Syn(preset, mix, steps, **kw)

KEEP_SYN = {'syn.sens', 'syn.gate', 'syn.dyn', 'syn.key', 'syn.scale', 'syn.tune', 'syn.hold'}
def syn_spec(s, name):
    vals, steps = {}, None
    if s.preset:
        pr = SYNTH.get(s.preset)
        if not pr: raise SystemExit(f'{name}: no synth preset {s.preset}')
        vals.update({'syn.' + k: v for k, v in pr['v'].items()})
        steps = pr.get('steps')
    vals.update({'syn.' + k.replace('_', '.'): v for k, v in s.kw.items()})
    if s.steps is not None: steps = s.steps
    vals['syn.mix'] = s.mix
    out = {'synth.on': 1}
    for k, v in vals.items():
        p = G.params.get(k)
        if not p: raise SystemExit(f'{name}: no synth setting {k}')
        if isinstance(v, str): v = choice(p.get('choices') or [], v, f'{name}: {k}')
        out[k] = float(v)
    return out, (pack_steps(steps) if (out.get('syn.arp.on') or steps) else None)

# ------------------------------------------------------------------ cabs, EQ, the rest
MICS = ['Dynamic 57', 'Ribbon 121', 'Condenser 414', 'Dynamic 421', 'Dynamic 906', 'Flat']
def cab(size='4x12', mic='Dynamic 57', pos=None, dist=None, mic2='Ribbon 121', level2=None, lc=None, hc=None):
    """The built-in cab and its two mics (pos 0 = centre of the cone .. 1 = edge)."""
    p = {'cab.a.mic': MICS.index(mic), 'cab.b.mic': MICS.index(mic2)}
    if pos is not None: p['cab.a.pos'] = pos
    if dist is not None: p['cab.a.dist'] = dist
    if level2 is not None: p['cab.b.level'] = level2
    if lc is not None: p['cab.lc'] = lc
    if hc is not None: p['cab.hc'] = hc
    return 'builtin-' + size, p

class IrCab:
    def __init__(self, key, voice=None, a=None, b=None, params=None):
        self.key, self.voice, self.a, self.b, self.params = key, voice, a, b, params or {}

def ir(key, voice=None, a=None, b=None, **params):
    """A cab from the IR cab pack, its mics on the spots the IRs were recorded at (cabs.py says which cab each preset
    gets; this is for a preset that wants its own). voice: one of its voice buttons by name. a / b: mic A / B moved:
    (speaker, position 0..1, angle in radians). params: cab settings by name, b_level=-6 -> cab.b.level."""
    return IrCab(key, voice, a, b, {'cab.' + k.replace('_', '.'): v for k, v in params.items()})

def amp_is_rig(key, chosen, knobs=None):
    """A full-rig capture (its cab and mic are in the capture): the plugin's cab stays out of the way."""
    if key not in G.capture: return False
    amp = G.capture[key][0]
    sel = (amp.get('map') or {}).get('selector')               # a selector knob's own captures (Eos: the Comfortably Numb solos)
    if sel and sel.get('rig'):
        v = (knobs or {}).get(sel.get('label', ''))
        if v not in (None, 0, 0.0, 'OFF', 'Off', 'off'): return True
    kind = (amp.get('kind') or '').lower()
    st = (amp.get('map') or {}).get('settings') or {}
    s = st.get('|'.join(str(c) for c in chosen) if chosen else '_')
    if s is None and st: s = next(iter(st.values()))
    return bool(s.get('rig', kind.startswith('full rig'))) if isinstance(s, dict) else kind.startswith('full rig')

def cab_for(p, chosen):
    """-> (cab key or None, {settings}, {extra spec fields}). See cabs.py."""
    import cabs as C
    key, name = p['amp'].key, p['name']
    if key == 'orpheus': return None, {'cab.on': 0}, {}                 # the acoustic sim is made to be heard with no cab
    if amp_is_rig(key, chosen, p['amp'].knobs): return None, {}, {}
    c = p['cab']
    hint = ''
    if not isinstance(c, IrCab):
        old = (c[1] if c else {}) or {}                                  # what the built-in cab's mics said: darker, brighter
        if old.get('cab.a.mic') == 1 or old.get('cab.a.pos', 0) >= 0.5: hint = 'dark'
        artist = p['folder'][8:] if p['folder'].startswith('Artists/') else ''
        art = C.ARTIST.get(artist, {})
        pick = C.PRESET.get(name) or art.get(key) or art.get('*') or C.AMP.get(key)
        if pick is None: raise SystemExit(f'{name}: no cab for amp {key} (cabs.py)')
        explicit = name in C.PRESET
        c = IrCab(pick[0], pick[1])
        if hint == 'dark' and not explicit:
            names = [v['name'] for v in (G.cabfiles[c.key].get('voices') or [])]
            if c.voice == 'POS 1' and 'POS 2' in names: c.voice = 'POS 2'
            elif c.key == 'brontes_cab': c.voice = 'R121'
            elif c.key == 'asteria_cab': c.voice = 'ALNICO GOLD'
            else: c.params = {'cab.a.pos': 0.45, 'cab.b.pos': 0.45, **c.params}
    f = G.cabfiles.get(c.key)
    if f is None: raise SystemExit(f'{name}: no IR cab {c.key}')
    extra = {}
    if c.voice is not None:
        names = [v['name'] for v in (f.get('voices') or [])]
        if c.voice not in names: raise SystemExit(f'{name}: {c.key} has no voice "{c.voice}" ({names})')
        extra['cabVoice'] = names.index(c.voice)
    params = {'cab.on': 1, **c.params}
    mics = []
    for m, spot in ((0, c.a), (1, c.b)):
        if spot is None: mics.append(None); continue
        spk, pos = spot[0], spot[1]
        ang = spot[2] if len(spot) > 2 else (-0.785 if m == 0 else -2.356)
        params['cab.' + 'ab'[m] + '.pos'] = pos
        mics.append([spk, ang])
    if any(mics): extra['cabMics'] = [x or [0 if i == 0 else min(1, len(f.get('speakers') or [0]) - 1), -0.785 if i == 0 else -2.356] for i, x in enumerate(mics)]
    return c.key, params, extra

# EQ: 63, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz
EQ = {
    'flat':    [0, 0, 0, 0, 0, 0, 0, 0, 0],
    'tight':   [-2, 0.5, 0, -1.5, 0, 0.5, 1, 0, -1.5],
    'scoop':   [1, 1.5, 0, -3, -2, 0, 1.5, 0.5, -1],
    'thrash':  [0, 1.5, -0.5, -3.5, -1, 1, 2, 0, -2],
    'djent':   [-1.5, 1.5, -1, -2.5, -0.5, 1, 2, 0.5, -2.5],
    'lead':    [-1, 0, 0.5, 1, 1.5, 1, 0, -1, -2],
    'warm':    [0, 1, 1, 0.5, 0, -0.5, -1.5, -2, -3],
    'bright':  [-1, 0, -0.5, 0, 0.5, 1, 1.5, 2, 1],
    'glass':   [-2, -0.5, -1, 0, 0, 0.5, 1.5, 2, 1.5],
    'mid':     [-1, 0, 1, 2, 2, 1, 0, -1, -2],
    'vintage': [-2, 0, 1, 1, 0.5, 0, -1, -2.5, -4],
    'modern':  [-1, 1, -1, -1.5, 0, 1, 1.5, 1, -1.5],
    'fuzz':    [0, 1, 0.5, -1, -1, 0, 1, -1, -3],
    'acoustic':[-3, -1, -1, 0, 0, 1, 2, 2.5, 2],
    'lofi':    [-4, -1, 1, 2, 1, -1, -3, -6, -9],
}

PRESETS = []
# per-preset EQ corrections from the tone check (tonecheck.py --fix), keyed by preset name
TONEFIX = json.loads((HERE / 'tonefix.json').read_text()) if (HERE / 'tonefix.json').exists() else {}
def P(name, folder, type, note, amp, cab_=None, pre=(), post=(), sfx=(), eq='flat', gate=None, doubler=None, width=None,
      synth=None, params=None, level=0.0, transpose=0, in_gain=None, measure=True):
    PRESETS.append(dict(name=name, folder=folder, type=type, note=note, amp=amp, cab=cab_, pre=list(pre), post=list(post), sfx=list(sfx),
                        eq=eq, gate=gate, doubler=doubler, width=width, synth=synth, params=params or {}, level=level,
                        transpose=transpose, in_gain=in_gain, measure=measure))

def slug(s):
    s = s.lower().replace('&', 'and').replace("'", '')
    return re.sub(r'[^a-z0-9]+', '-', s).strip('-')

def build_spec():
    spec, names = [], set()
    for p in PRESETS:
        name = p['name']
        if name in names: raise SystemExit(f'two presets called {name}')
        names.add(name)
        amp, chosen = amp_spec(p['amp'], name)
        params = {}
        cabkey, cp, cabx = cab_for(p, chosen); params.update(cp)
        if p['gate'] is None: params['gate.on'] = 0
        else: params['gate.on'] = 1; params['gate.thresh'] = p['gate']
        if p['doubler'] is None: params['doubler.on'] = 0
        else: params['doubler.on'] = 1; params['doubler.time'] = p['doubler']
        if p['width'] is not None: params['width'] = p['width']
        if p['in_gain'] is not None: params['in.gain'] = p['in_gain']
        if p['transpose']: params['transpose'] = p['transpose']; params['transpose.hq'] = 1
        eq = EQ[p['eq']] if isinstance(p['eq'], str) else p['eq']
        album_ir = cabkey == 'metallon_cab' and G.cabfiles[cabkey]['voices'][cabx.get('cabVoice', 0)].get('album') not in (None, 'JC CLEAN')
        if album_ir: eq = EQ['flat']                 # an album IR carries the record's own EQ; the preset's curve would scoop twice
        fix = TONEFIX.get(name)                      # tonecheck.py --fix: measured on a real DI, laid on top
        if fix: eq = [max(-12.0, min(12.0, round(a + b, 1))) for a, b in zip(eq, fix)]
        for i, g in enumerate(eq): params[f'eq.b{i + 1}'] = g
        steps = None
        if p['synth']:
            sv, steps = syn_spec(p['synth'], name); params.update(sv)
        for k, v in p['params'].items():
            if k not in G.params: raise SystemExit(f'{name}: no setting {k}')
            params[k] = float(v)
        racks = [[fx_spec(f, r, name) for f in p[r]] for r in ('pre', 'post', 'sfx')]
        for r, lst in zip(('pre', 'post', 'sfx'), racks):
            if len(lst) > 5: raise SystemExit(f'{name}: more than 5 effects in {r}')
        artist = p['folder'].startswith('Artists/')
        cat = p['folder'] if artist else p['type']
        file = ('Artists/' + slug(p['folder'][8:]) + '/' + slug(p['folder'][8:]) + '-' + slug(name) if artist else 'Factory/factory-' + slug(name)) + '.json'
        e = {'name': name, 'category': cat, 'type': p['type'], 'note': p['note'], 'file': file,
             'amp': amp, 'cab': cabkey, 'racks': racks, 'params': params, 'levelOffset': p['level'], **cabx}
        if not p['measure']: e['level'] = False
        if isinstance(p['eq'], str) and p['eq'] != 'flat' and not TONEFIX.get(name) and not album_ir: e['eqName'] = p['eq'].capitalize()
        if steps: e['synSteps'] = steps
        if p['amp'].key in G.capture:
            fb = p['amp'].fallback
            if fb is None:
                fb = FALLBACK[p['amp'].key]
                if p['type'] in ('Clean', 'Ambient', 'Acoustic', 'Funk'): fb = 'glass65'
                elif p['type'] == 'Crunch' and fb not in ('top30', 'plexi68', 'brit800'): fb = 'brit800'
            e['fallbackAmp'] = fb
        spec.append(e)
    return spec

def main():
    global G, SYNTH
    args = [x for x in sys.argv[1:] if not x.startswith('--')]
    binary = Path(args[0]) if args else DEFAULT_BIN
    G = Gear(plugin_state(binary))
    SYNTH = synth_presets()
    sys.path.insert(0, str(HERE))
    import factory, artists                          # noqa: E402  (they call P(...) with the helpers above)
    factory.make(sys.modules[__name__]); artists.make(sys.modules[__name__])
    spec = build_spec()
    nf = sum(1 for e in spec if not e['category'].startswith('Artists/'))
    arts = sorted({e['category'][8:] for e in spec if e['category'].startswith('Artists/')})
    print(f'{len(spec)} presets: {nf} factory, {len(spec) - nf} artist presets in {len(arts)} artists')
    sf = HERE / 'spec.json'
    sf.write_text(json.dumps(spec, indent=1, ensure_ascii=False), encoding='utf-8')
    if '--spec-only' in sys.argv: return
    for f in OUT.rglob('*.json'): f.unlink()
    r = subprocess.run([str(binary), '--write-factory-presets', str(OUT), str(sf), str(TARGET_DB)], text=True, capture_output=True, timeout=7200)
    print(r.stdout[-200000:]); print(r.stderr[-4000:])
    (HERE / 'levels.txt').write_text(r.stdout, encoding='utf-8')
    sys.exit(r.returncode)

if __name__ == '__main__':
    main()
