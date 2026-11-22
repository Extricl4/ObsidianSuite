#!/usr/bin/env python3
"""Builds a clickable web preview of the real plugin UI (no sound).

    python plugin/tools/ui-test/make_web_preview.py     # -> plugin/tools/ui-test/web/

Same page the plugin shows (ui/obsidian-demo.html + obsidian-native.css +
obsidian-bridge.js), with the fake engine from mock-native.js standing in for
the C++ side, so every list, panel and setting behaves like the standalone app.
"""
import json, shutil, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import run  # noqa: E402  (reuses its page + standalone state builder)

WEB = HERE / 'web'


def preset_states():
    """Runs the standalone with --dump-presets (OBS_BIN, or the Linux dev build)."""
    import os, subprocess, tempfile
    exe = Path(os.environ.get('OBS_BIN', '/home/claude/build/ObsidianSuite_artefacts/Release/Standalone/Obsidian Amp Suite'))
    if not exe.exists(): return None
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / 'presets.json'
        subprocess.run([str(exe), '--dump-presets', str(out)], cwd=tmp, check=True, timeout=600, capture_output=True)
        d = json.loads(out.read_text(encoding='utf-8'))
    for r in [d['defaultRig']] + [e['rig'] for e in d['presets'].values()]:
        r['busy'] = False                                   # the preview has no loading to wait for
        a = r.get('amp') or {}
        if a.get('face'): a['face']['loaded'] = True
    return d


def main():
    state = run.build_page(standalone=True)
    state['presets'] = run.factory_presets()    # the 390 built-in presets (Factory + Artists), as the app lists them
    # what each one does to the knobs and the rig, from the real plugin, so LOAD changes the amp, pedals, synth...
    states = preset_states()
    if states:
        state['presetDefaults'], state['presetDefaultRig'] = states['defaults'], states['defaultRig']
        state['presetStates'] = states['presets']
    else:
        print('  (no plugin build found: presets will only change their name; set OBS_BIN to the standalone)')
    # every amp of the amp pack, with its face
    state['amps'] = [a for a in state['amps'] if a.get('type') != 'capture']
    state['amps'] += [run.capture_entry(f.stem) for f in sorted((run.CONTENT / 'Amps').glob('*.json'))]
    for a in state['amps']:            # the preview has a file limit: no separate "amp off" pointer images
        for sk in (a.get('face') or {}).get('skins', []): sk['ptrOff'] = False
    WEB.mkdir(exist_ok=True)
    html = (run.UI / 'obsidian-demo.html').read_text(encoding='utf-8')
    mock = (HERE / 'mock-native.js').read_text(encoding='utf-8')
    # the small face sprites (knob pointers, LEDs) go inline: the artifact may hold at most 512 files
    import base64, re
    sprites = {f.name: 'data:image/png;base64,' + base64.b64encode(f.read_bytes()).decode()
               for f in sorted((run.CONTENT / 'Faces' / 'img').glob('*.png')) if not re.search(r'_k\d+o\.png$', f.name)}
    head = ('<script>\n' + mock + '\nwindow.__mock.load(' + json.dumps(state) + ');\nwindow.__obsFaceImg = '
            + json.dumps(sprites, separators=(',', ':')) + ';\n</script>\n')
    i = html.lower().find('<head>') + len('<head>')
    html = html[:i] + '\n' + head + html[i:]
    css = (run.PLUGIN / 'WebUI' / 'obsidian-native.css').read_text(encoding='utf-8')
    js = (run.PLUGIN / 'WebUI' / 'obsidian-pedals.js').read_text(encoding='utf-8') + '\n' + (run.PLUGIN / 'WebUI' / 'obsidian-bridge.js').read_text(encoding='utf-8')
    tail = '<style>\n' + css + '\n</style>\n<script>\n' + js + '\n</script>\n'
    j = html.lower().rfind('</body>')
    html = html[:j] + tail + html[j:]
    (WEB / 'index.html').write_text(html, encoding='utf-8')
    shutil.copy(run.UI / 'grain.png', WEB / 'grain.png')
    for f in (run.PLUGIN / 'WebUI' / 'pedals').glob('*.webp'): shutil.copy(f, WEB / f.name)
    shutil.copytree(run.UI / 'fonts', WEB / 'fonts', dirs_exist_ok=True)
    if (WEB / 'faces').is_symlink(): (WEB / 'faces').unlink()
    if (WEB / 'faces').exists(): shutil.rmtree(WEB / 'faces')
    shutil.copytree(run.CONTENT / 'Faces' / 'img', WEB / 'faces' / 'img', ignore=shutil.ignore_patterns('*.png'))   # sprites are inline
    print('wrote', WEB / 'index.html', round(len(html) / 1024), 'KB')


if __name__ == '__main__':
    main()
