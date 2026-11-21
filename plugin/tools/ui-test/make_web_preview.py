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


def main():
    state = run.build_page(standalone=True)
    state['presets'] = []                       # the real app ships with no presets
    WEB.mkdir(exist_ok=True)
    html = (run.UI / 'obsidian-demo.html').read_text(encoding='utf-8')
    mock = (HERE / 'mock-native.js').read_text(encoding='utf-8')
    head = '<script>\n' + mock + '\nwindow.__mock.load(' + json.dumps(state) + ');\n</script>\n'
    i = html.lower().find('<head>') + len('<head>')
    html = html[:i] + '\n' + head + html[i:]
    css = (run.PLUGIN / 'WebUI' / 'obsidian-native.css').read_text(encoding='utf-8')
    js = (run.PLUGIN / 'WebUI' / 'obsidian-bridge.js').read_text(encoding='utf-8')
    tail = '<style>\n' + css + '\n</style>\n<script>\n' + js + '\n</script>\n'
    j = html.lower().rfind('</body>')
    html = html[:j] + tail + html[j:]
    (WEB / 'index.html').write_text(html, encoding='utf-8')
    shutil.copy(run.UI / 'grain.png', WEB / 'grain.png')
    print('wrote', WEB / 'index.html', round(len(html) / 1024), 'KB')


if __name__ == '__main__':
    main()
