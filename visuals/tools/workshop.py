"""Build a workshop canvas: one page per item, each holding its versions side by side.
usage: python3 workshop.py <out_root> <title> <items.json>
items.json: [{"key","name","versions":[{"label","blob","w","h"}]}]"""
import json, sys, os
root, title, items = sys.argv[1], sys.argv[2], json.load(open(sys.argv[3]))
os.makedirs(f'{root}/project', exist_ok=True)
pages, boards, order = [], {}, []
for it in items:
    pid = it['key'][:40]; pages.append({"id": pid, "name": it['name']})
    x = 0
    for i, v in enumerate(it['versions']):
        fn = 'Main.dc.html' if not order else f"{it['key'].capitalize()}_{v['label'].replace(' ','_')}.dc.html"
        w, h = v['w'], v['h']
        open(f'{root}/project/{fn}', 'w').write(f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{it['name']} · {v['label']}</title>
<script src="./support.js"></script>
</head>
<body>
<x-dc>
<helmet><style>body{{margin:0;background:#050505}}</style></helmet>
<div style="width: {w}px; height: {h}px; overflow: hidden; background: #050505">
<img src="/_blob/{v['blob']}" alt="{it['name']}, {v['label']}" style="width: {w}px; height: {h}px; display: block; object-fit: cover">
</div>
</x-dc>
<script type="text/x-dc" data-dc-script data-props='{{"$preview":{{"width":{w},"height":{h}}}}}'>
class Component extends DCLogic {{
renderVals() {{ return {{}}; }}
}}
</script>
</body>
</html>
''')
        boards[fn] = {"x": x, "y": 0, "w": w, "h": h, "title": f"{it['name']} · {v['label']}", "radius": 14, "page": pid}
        order.append(fn); x += w + 80
json.dump({"v": 3, "createdOnFiles": {"v": 1, "at": "2026-11-17T15:00:00Z"}, "title": title, "launch": {"view": "canvas", "page": pages[0]['id']},
           "pages": pages, "boards": boards, "order": order, "notes": {}, "designSystems": []}, open(f'{root}/project/canvas.json', 'w'))
json.dump([{"as": "project/" + f, "path": f"{root}/project/" + f} for f in ['canvas.json'] + order], open(f'{root}/files.json', 'w'))
print(len(pages), 'pages', len(order), 'boards')
