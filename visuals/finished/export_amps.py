"""Write plugin/Content/Amps/<key>.json for every finished amp, from amps_data.py.

Each file works in today's plugin (the ADD AMP list reads key, name, role, description and up to 4
"captures", one per channel) and also carries "map": the full wiring the finished-amps page uses
(every button state, which file each one plays and how the knobs pick or blend files), for the
plugin to follow later. Capture paths are relative to Documents\\Obsidian Amp Suite\\Captures.

    python3 visuals/finished/export_amps.py
"""
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from amps_data import AMPS

ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
CAP = os.path.join(ROOT, 'plugin', 'Content', 'Captures')
OUT = os.path.join(ROOT, 'plugin', 'Content', 'Amps')
ROLE = dict(moirai='high gain', umbra='high gain', citadel='lead', ares='high gain', strix='high gain', nihil='high gain', talos='high gain',
            achlys='high gain', lyssa='high gain', belial='high gain', melinoe='clean', nyx='high gain', chrysaor='crunch · rhythm',
            eris='high gain', kratos='high gain', vulcan='high gain', moros='high gain', niflheim='high gain', persephone='clean',
            hyperion='clean · lead', hecate='high gain', typhon='high gain', tyche='crunch', thanatos='lead', orpheus='acoustic',
            lamia='high gain', astraea='clean · crunch', eos='clean · lead', erebus='high gain', dionysus='clean · lead')
os.makedirs(OUT, exist_ok=True)


def path(a, f):
    return f"Amps/{a['name']}/{f}.nam"


def default_file(a, c):
    fs = c['files']
    if c['by'] == 'grid':
        row = fs[len(fs) // 2]; return row[len(row) // 2]
    if c['by'] in ('gain', 'mid'):
        return fs[len(fs) // 2]
    if c['by'] == 'near':           # the capture closest to every knob at noon
        best = min(range(len(fs)), key=lambda i: sum(w * (v - 5) ** 2 for w, v in zip(c['w'], c['at'][i]) if v is not None))
        return fs[best]
    return fs[0]


def channel_defaults(a):
    if not a['dims']:
        return [default_file(a, a['caps'][''])]
    d0 = a['dims'][0]
    chans = sorted({int(k.split('|')[0]) for k in a['caps']})
    out = []
    for ch in chans[:4]:
        key = [str(ch)] + [str(d.get('start', 0)) for d in a['dims'][1:]]
        k = '|'.join(key)
        if k not in a['caps'] or a['caps'][k].get('hide'):
            k = next(kk for kk in a['caps'] if kk.split('|')[0] == str(ch) and not a['caps'][kk].get('hide'))
        out.append(default_file(a, a['caps'][k]))
    return out


def walk_files(a):
    for c in a['caps'].values():
        fs = c['files']
        for f in ([x for r in fs for x in r] if c['by'] == 'grid' else fs) + (c.get('alt') or []):
            yield f
    if a.get('sel'):
        for f in a['sel']['files']:
            if f: yield f


n = 0; missing = []
for a in AMPS:
    if a.get('loader'):
        continue                   # the plugin has its own NAM Loader amp
    for f in walk_files(a):
        if not os.path.exists(os.path.join(CAP, path(a, f))):
            missing.append(path(a, f))
    caps = {}
    for k, c in a['caps'].items():
        cc = {kk: v for kk, v in c.items() if kk not in ('files', 'alt')}
        cc['files'] = [[path(a, x) for x in r] for r in c['files']] if c['by'] == 'grid' else [path(a, x) for x in c['files']]
        if c.get('alt'): cc['spare'] = [path(a, x) for x in c['alt']]
        caps[k] = cc
    m = dict(knobs=a['knobs'], gainKnob=a['gain'], buttons=a['dims'], settings=caps)
    for k in ('mid', 'master', 'switches', 'steps'):
        if a.get(k) is not None: m[k] = a[k]
    if a.get('sel'):
        m['selector'] = dict(a['sel'], files=[path(a, f) if f else None for f in a['sel']['files']])
    entry = dict(key=a['key'], name=a['name'].upper(), type='nam', role=ROLE.get(a['key'], ''), folder='CAPTURE AMPS',
                 basedOn=a['based'], description=a['intro'], pack=a['pack'], madeBy=a['by'], files=a['nfiles'], kind=a['kind'],
                 captures=[path(a, f) for f in channel_defaults(a)], channelNames=[], map=m)
    if a['dims'] and a['dims'][0].get('names'):
        entry['channelNames'] = a['dims'][0]['names'][:4]
    json.dump(entry, open(os.path.join(OUT, a['key'] + '.json'), 'w'), indent=1, ensure_ascii=False)
    n += 1
print('wrote', n, 'amps;', 'missing files:', missing or 'none')
