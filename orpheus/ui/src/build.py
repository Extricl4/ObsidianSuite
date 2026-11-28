#!/usr/bin/env python3
"""Builds ui/obsidian-demo.html (the Obsidian Suite UI) from the sources in this folder.

    python ui/src/build.py

  base.html  - the page (layout, knobs, pages, settings, base script)
  synth.js   - the SYNTH page (built in script) + plugin hooks (window.obsSynth)
  aether.js  - AETHER, the SYNTH page's second page (noise, ambience, atmospheres)
  fxlab.js   - GLITCH, the SFX page's second page (KERAUNOS and KHAOS)
  synth_presets.js - the synth's factory presets (PRESETS button)
  header.js  - header knobs (TRANSPOSE, WIDTH) and knob wheel / double-click
  studio.js  - STUDIO recorder
  scenes.js  - the picture behind the pages for each theme
  themes.js  - the themes (colours, fonts, details)
The same file runs in a browser (demo) and inside the plugin, where
plugin/WebUI/obsidian-bridge.js is injected and connects it to the audio engine.
"""
import os, re, json, colorsys
os.chdir(os.path.dirname(os.path.abspath(__file__)))
h=open('base.html').read()
def rep(a,b,cnt=1):
    global h
    assert h.count(a)==cnt,(a[:70],h.count(a)); h=h.replace(a,b)
rep("if(val) val.textContent=(v*10).toFixed(1); }","if(val) val.textContent=(v*10).toFixed(1); if(window.knobHook) knobHook(k,v); }")
rep("markR($$('button').filter(x=>x.textContent.trim()==='ROOT')); ","")
rep("const ACC0='#f3ead8';","var ACC0='#f3ead8';")
rep("""const td=$('[aria-label="Transpose down"]'); if(td) td.parentElement.children[1].children[0].textContent='0';""","""if(window.setTranspose) setTranspose(0);""")
rep("setInterval(()=>{ meters.forEach(","setInterval(()=>{ if(window.__JUCE__) return; meters.forEach(")
rep("setInterval(()=>{ const c=Math.round(Math.random()*8-5);","setInterval(()=>{ if(window.__JUCE__) return; const c=Math.round(Math.random()*8-5);")
# the Obsidian gem, as on the app icon: four facets (light top-left / top-right, dark below) and a soft glow.
# rgb() colours on purpose: the theme step below only rewrites #hex and rgba() greys, the logo keeps its shades.
def gem(w, glow):
    h = round(w * 32 / 30)
    return ('<svg class="obs-gem" aria-hidden="true" width="%d" height="%d" viewBox="0 0 30 32" style="flex: none; overflow: visible; '
            'filter: drop-shadow(0 0 %spx rgba(243,234,216,.32)) drop-shadow(0 1px 1px rgba(0,0,0,.6))">'
            '<path d="M15 0L0 16h15z" style="fill: rgb(190,184,170)"/><path d="M15 0l15 16H15z" style="fill: rgb(243,234,216)"/>'
            '<path d="M0 16l15 16V16z" style="fill: rgb(70,68,64)"/><path d="M30 16L15 32V16z" style="fill: rgb(120,116,108)"/>'
            '<path d="M15 .6L.8 16" style="stroke: rgb(255,250,240); stroke-width: .7; opacity: .55; fill: none"/>'
            '<path d="M15 .6l14.2 15.4" style="stroke: rgb(255,252,244); stroke-width: .7; opacity: .7; fill: none"/></svg>') % (w, h, glow)
rep('<div style="width: 13px; height: 13px; transform: rotate(45deg); background: linear-gradient(135deg, #f1ede4, #6d6a64); box-shadow: 0 0 12px rgba(255,255,255,.25)" aria-hidden="true"></div>', gem(18, 5), 3)
rep('<div style="width: 26px; height: 26px; transform: rotate(45deg); background: linear-gradient(135deg, #f1ede4, #6d6a64); box-shadow: 0 0 20px rgba(255,255,255,.25)" aria-hidden="true"></div>', gem(36, 9))
# SPECIAL FX is called SFX (shorter in the tab row)
rep('white-space: nowrap">SPECIAL FX</span>', 'white-space: nowrap">SFX</span>')
rep('<div class="page" data-page="SPECIAL FX"', '<div class="page" data-page="SFX"')
rep("const TABS=['SPECIAL FX','PEDALS','SYNTH','AMP','CAB','EQ','FX'];", "const TABS=['SFX','PEDALS','SYNTH','AMP','CAB','EQ','FX'];")
rep("const TABTIP={'SPECIAL FX':", "const TABTIP={'SFX':")
# HARMONY tab (harmony.js): HYDRA, the six-voice harmonizer (OBS_HARMONY=0 builds without it)
HARMONY = os.environ.get('OBS_HARMONY', '1') != '0'
if HARMONY:
    m = re.search(r'<button style="min-width: 62px;[^>]*>(?:(?!</button>).)*?>SYNTH</span><span style="width: 18px; height: 2px"></span></button>', h)
    assert m, 'SYNTH tab'
    hb = m.group(0).replace('>SYNTH</span>', '>HARMONY</span>')
    hb = re.sub(r'<svg aria-hidden="true" width="28".*?</svg>', '<svg aria-hidden="true" width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.35" stroke-linecap="round" stroke-linejoin="round"><path d="M8.5 18V5.5l11-2.5v12.5"/><path d="M8.5 9.5l11-2.5"/><circle cx="6" cy="18" r="2.5"/><circle cx="17" cy="15.5" r="2.5"/></svg>', hb, count=1, flags=re.S)
    h = h[:m.end()] + hb + h[m.end():]
    rep("const TABS=['SFX','PEDALS','SYNTH','AMP','CAB','EQ','FX'];", "const TABS=['SFX','PEDALS','SYNTH','HARMONY','AMP','CAB','EQ','FX'];")
    rep("const TABTIP={'SFX':", "const TABTIP={'HARMONY':'Smart harmonies that follow your key and mode','SFX':")
    a = h.find('<div class="page" data-page="AMP"')
    h = h[:a] + '<div class="page" data-page="HARMONY" style="flex: 1; min-height: 0; flex-direction: column; position: relative; display: none"></div>' + h[a:]
# the SYNTH page is built by synth.js: drop the old static markup of that page
a=h.find('<div class="page" data-page="SYNTH"'); assert a>0
b=h.find('>',a)+1; depth=1; i=b
while depth:
    o=h.find('<div',i); c=h.find('</div>',i)
    if o!=-1 and o<c: depth+=1; i=o+4
    else: depth-=1; i=c+6
h=h[:b]+h[i-6:]
i=h.rfind('</body>'); h=h[:i]+open('synth_presets.js').read()+'\n'+open('siren_trim.js').read()+'\n'+open('siren_presets.js').read()+'\n'+open('synth.js').read()+'\n'+open('orpheus.js').read()+'\n'+open('sirenfx.js').read()+'\n'+open('fxlab.js').read()+('\n'+open('harmony.js').read() if HARMONY else '')+'\n'+open('header.js').read()+'\n'+open('studio.js').read()+'\n'+h[i:]

# ---------- theme: greys -> CSS variables ----------
DATA=[]; 
def prot(m): DATA.append(m.group(0)); return '\x00%d\x00'%(len(DATA)-1)
h=re.sub(r'data:[a-z/+]+;base64,[A-Za-z0-9+/=]+',prot,h)
VARS={}
def norm(x):
    x=x.lstrip('#').lower()
    return ''.join(c*2 for c in x) if len(x)==3 else x
def isgrey(r,g,b): return max(r,g,b)-min(r,g,b)<=12
def L(r,g,b): return (max(r,g,b)+min(r,g,b))/510
def var(r,g,b,role):
    if role=='f' and L(r,g,b)>=.2: role='f'
    elif role=='f': role='k'   # dark ink on accent: kept dark
    n='--c%s_%02x%02x%02x'%(role,r,g,b); VARS[n]=('%d,%d,%d'%(r,g,b),role); return n
HEX=re.compile(r'#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b')
RGBA=re.compile(r'rgba\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*([\d.]+)\s*\)')
def themev(val,role):
    def hx(m):
        x=norm(m.group(0)); r,g,b=[int(x[k:k+2],16) for k in (0,2,4)]
        if not isgrey(r,g,b): return m.group(0)
        return 'rgb(var(%s))'%var(r,g,b,role)
    def ra(m):
        r,g,b,a=int(m.group(1)),int(m.group(2)),int(m.group(3)),m.group(4)
        if (r,g,b)==(0,0,0): return 'rgba(0,0,0,calc(%s*var(--sh)))'%a
        if (r,g,b)==(255,255,255) or not isgrey(r,g,b): return m.group(0)
        return 'rgba(var(%s),%s)'%(var(r,g,b,role),a)
    return RGBA.sub(ra,HEX.sub(hx,val))
FG={'color','caret-color','-webkit-text-fill-color'}
def themestyle(st):
    out=[]
    for d in st.split(';'):
        if ':' in d:
            p,v=d.split(':',1); role='f' if p.strip().lower() in FG else 'b'; out.append(p+':'+themev(v,role))
        else: out.append(d)
    return ';'.join(out)
k=h.find('<script'); head,js=h[:k],h[k:]
# <style> blocks
head=re.sub(r'(<style>)(.*?)(</style>)',lambda m:m.group(1)+re.sub(r'\{([^{}]*)\}',lambda n:'{'+themestyle(n.group(1))+'}',m.group(2))+m.group(3),head,flags=re.S)
# svg presentation attrs -> style
def tag(m):
    t=m.group(0); add=[]
    def at(a):
        nonlocal t
        mm=re.search(r'\s%s="(#[0-9a-fA-F]{3,6})"'%a,t)
        if mm:
            x=norm(mm.group(1)); r,g,b=[int(x[q:q+2],16) for q in (0,2,4)]
            if isgrey(r,g,b): t=t.replace(mm.group(0),''); add.append('%s: rgb(var(%s))'%(a,var(r,g,b,'b')))
    for a in ('fill','stroke','stop-color'): at(a)
    if not add: return t
    if ' style="' in t: return t.replace(' style="',' style="'+'; '.join(add)+'; ',1)
    return re.sub(r'(/?>)$',' style="'+'; '.join(add)+'"'+r'\1',t)
head=re.sub(r'<(?:path|circle|rect|line|polyline|polygon|ellipse|stop|svg|g|text|use)\b[^>]*>',tag,head)
head=re.sub(r'style="([^"]*)"',lambda m:'style="'+themestyle(m.group(1))+'"',head)
# JS string literals: role from nearest preceding css property
REGEX_OLD=r"/var\(--accent\)|#2a2a2e, #1b1b1e|\.14\)|\.13\)/"
assert js.count(REGEX_OLD)==1; js=js.replace(REGEX_OLD,'\x01RX\x01')
lines=js.split('\n'); outl=[]
for ln in lines:
    if 'hsl(' in ln or 'hsv2hex' in ln: outl.append(ln); continue
    def jhx(m):
        pre=ln[max(0,m.start()-90):m.start()]
        cands=[(mm.start(),mm.group(1)) for mm in re.finditer(r'(background|shadow|border|fill|stroke|color)',pre)]
        role='b'
        if cands:
            pos,w=cands[-1]
            if w=='color' and not pre[:pos].endswith(('-','stop-')) : role='f'
        x=m.group(0)
        return themev(x,role)
    ln=re.sub(r"#[0-9a-fA-F]{6}\b|#[0-9a-fA-F]{3}\b|rgba\(\s*\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*[\d.]+\s*\)",jhx,ln)
    outl.append(ln)
js='\n'.join(outl)
js=js.replace('\x01RX\x01',r"/var\(--accent\)|rgb\(var\(--cb_2a2a2e\)\), rgb\(var\(--cb_1b1b1e\)\)|\.14\)|\.13\)/")
assert '--cb_2a2a2e' in VARS or True
h=head+js
roots=':root{--sh:1;'+''.join('%s:%s;'%(n,v[0]) for n,v in VARS.items())+'}'
h=h.replace('<style>','<style>'+roots,1)
vl=json.dumps([[n,v[0],v[1]] for n,v in VARS.items()])
t=open('scenes.js').read()+'\n'+open('themes.js').read().replace('__VARS__',vl)
i=h.rfind('</body>'); h=h[:i]+t+'\n'+h[i:]
h=re.sub(r'\x00(\d+)\x00',lambda m:DATA[int(m.group(1))],h)
# the grain texture is a separate file next to the page (the plugin serves it too)
h=re.sub(r'url\(data:image/png;base64,[A-Za-z0-9+/=]+\)','url(grain.png)',h)
assert 'base64' not in h, 'unexpected inline image'
open(os.environ.get('OBS_OUT','../obsidian-demo.html'),'w').write(h)
print('vars',len(VARS),'size',len(h))
