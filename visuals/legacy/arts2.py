from art_common import *
import math, json, random
from PIL import ImageFilter
random.seed(11)
R='/home/claude/r3d/'
specs={}
def save(im,n): im.save(R+n); return R+n
SCR=[[110,110],[1210,110],[110,2290],[1210,2290]]
def two_fs(d,col,a,b,k='mich',s=26):
    text(d,(330,2280),a,k,s,col,spacing=6); text(d,(990,2280),b,k,s,col,spacing=6)
def feather(d,base,ang,L,col,w):
    a=math.radians(ang); pts=[]
    for t in range(0,21):
        f=t/20; bend=math.sin(f*math.pi)*L*0.06
        x=base[0]+math.cos(a)*L*f - math.sin(a)*bend; y=base[1]+math.sin(a)*L*f + math.cos(a)*bend
        pts.append((x,y))
    d.line(pts,fill=col,width=w,joint='curve')
    for t in range(4,20,2):
        x,y=pts[t]; s=(1-t/20)*28+8
        d.line([(x,y),(x+math.cos(a+0.9)*s,y+math.sin(a+0.9)*s)],fill=col,width=2)

# ---------- SERAPH (heavenly shimmer reverb) ----------
G=(140,104,38,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,G)
cx,cy=660,640
for rr,w in ((250,5),(266,2),(300,1)): d.ellipse((cx-rr,cy-rr,cx+rr,cy+rr),outline=G,width=w)
for i in range(72):
    a=math.radians(i*5); l=360 if i%2==0 else 320
    if 95<i*5<445 and (i*5)%180 not in range(60,121): pass
    d.line([(cx+280*math.cos(a),cy+280*math.sin(a)),(cx+l*math.cos(a),cy+l*math.sin(a))],fill=G,width=2 if i%2 else 3)
for s in (-1,1):
    for i in range(7):
        ang=(-12-i*11) if s>0 else (180+12+i*11)
        feather(d,(cx+s*170,cy+60+i*8),ang,520-i*42,G,5)
for x,n in ((300,'SHIMMER'),(660,'TONE'),(1020,'MIX')): text(d,(x,1250),n,'cinzel',32,G,spacing=6)
text(d,(cx,cy+330),'DECAY','cinzel',38,G,spacing=10)
text(d,(560,1420),'+12','cinzel',26,G); text(d,(760,1420),'+24','cinzel',26,G)
text(d,(660,1620),'Seraph','cdec',170,G)
text(d,(660,1745),'SHIMMER REVERB','cinzel',34,(120,92,40,255),spacing=12)
two_fs(d,G,'FREEZE','ON','cinzel',30)
save(im,'seraph_art.png')
g=canvas(); gd=ImageDraw.Draw(g)
gd.ellipse((cx-258,cy-258,cx+258,cy+258),outline=(255,226,160,255),width=12)
g=g.filter(ImageFilter.GaussianBlur(6))
save(g,'seraph_glow.png')
specs['seraph']={"out":R+"out/seraph.png","art":R+"seraph_art.png","glow":R+"seraph_glow.png","glow_strength":3.0,"artmat":{"rough":0.25,"metal":0.85},
 "enclosure":{"color":[0.78,0.75,0.68],"metal":0.25,"rough":0.3,"coat":1.0,"bump":["noise",1500,0.02]},
 "knobs":[{"at":[660,640],"r":0.9,"style":"alu","v":0.7},{"at":[300,1100],"r":0.5,"style":"alu","v":0.6},{"at":[660,1100],"r":0.5,"style":"alu","v":0.45},{"at":[1020,1100],"r":0.5,"style":"alu","v":0.4}],
 "toggles":[[660,1420,-18]],"leds":[{"at":[660,1890],"color":[1.0,0.85,0.55],"strength":40}],
 "footswitches":[[330,2130,1.0],[990,2130,1.0]],"screws":SCR,"rimcolor":[1,0.9,0.7]}

# ---------- RICHTER (earthquake) ----------
K=(22,22,22,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,K)
g=canvas(); gd=ImageDraw.Draw(g)
def crack(x,y,ang,n,w):
    pts=[(x,y)]
    for i in range(n):
        ang+=random.uniform(-0.6,0.6); x+=math.cos(ang)*random.uniform(18,34); y+=math.sin(ang)*random.uniform(18,34); pts.append((x,y))
        if random.random()<0.07 and w>3: crack(x,y,ang+random.choice((-1,1))*0.9,n//2,w-3)
    d.line(pts,fill=(8,6,5,255),width=w+6,joint='curve'); gd.line(pts,fill=(255,90,20,255),width=max(2,w-2),joint='curve')
for (x,y,a) in ((70,1900,-0.3),(1250,2000,3.4),(660,2360,-1.57),(70,780,0.3),(1250,820,2.9)): crack(x,y,a,12,9)
cx,cy=660,560
for i in range(10):
    a=math.radians(135+i*30); text(d,(cx+250*math.cos(a),cy+250*math.sin(a)),str(i+1),'mich',30,K)
d.arc((cx-210,cy-210,cx+210,cy+210),135,405,fill=K,width=4)
text(d,(cx,cy+320),'MAGNITUDE','mich',30,K,spacing=8)
for x,n in ((300,'DEPTH'),(1020,'RUMBLE')): text(d,(x,1110),n,'mich',26,K,spacing=6)
pts=[]
for x in range(90,1231,4):
    t=(x-660)/570; amp=8+170*math.exp(-(t*3.2)**2)
    pts.append((x,1260+amp*math.sin(x*0.21)*random.uniform(0.5,1)))
d.line(pts,fill=K,width=4)
text(d,(660,1520),'RICHTER','mich',118,K,spacing=16)
text(d,(660,1630),'SEISMIC SUB-OCTAVE','mich',28,(50,50,50,255),spacing=10)
two_fs(d,K,'QUAKE','ON')
save(im,'richter_art.png'); g=g.filter(ImageFilter.GaussianBlur(2)); save(g,'richter_glow.png')
specs['richter']={"out":R+"out/richter.png","art":R+"richter_art.png","glow":R+"richter_glow.png","glow_strength":5.0,"artmat":{"rough":0.9,"metal":0.0},
 "enclosure":{"color":[0.30,0.29,0.28],"metal":0.0,"rough":0.95,"bump":["noise",60,0.7]},
 "knobs":[{"at":[660,560],"r":0.9,"style":"black","v":0.8},{"at":[300,960],"r":0.5,"style":"black","v":0.6},{"at":[1020,960],"r":0.5,"style":"black","v":0.7}],
 "leds":[{"at":[660,1880],"color":[1.0,0.35,0.05],"strength":45}],
 "footswitches":[[330,2130,1.0],[990,2130,1.0]],"screws":SCR,"rimcolor":[1,0.55,0.3],"world":[0.012,0.008,0.006]}

# ---------- TIDE (chorus) ----------
Wt=(225,240,238,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,Wt)
g=canvas(); gd=ImageDraw.Draw(g)
for k in range(11):
    pts=[(x,1180+k*30+55*math.sin(x/150+k*0.55)) for x in range(70,1252,6)]
    d.line(pts,fill=(225,240,238,250-k*16),width=4 if k%3==0 else 2)
    if k==4: gd.line(pts,fill=(90,255,240,255),width=8)
for x,n in ((330,'RATE'),(990,'DEPTH')): text(d,(x,780),n,'cinzel',36,Wt,spacing=8)
text(d,(660,1010),'MIX','cinzel',28,Wt,spacing=6)
text(d,(560,470),'CHO','cinzel',24,Wt); text(d,(760,470),'VIB','cinzel',24,Wt)
text(d,(660,1660),'Tide','cinzel',210,Wt,spacing=20)
text(d,(660,1790),'CHORUS  ·  VIBRATO','cinzel',30,(170,215,210,255),spacing=10)
text(d,(660,2290),'ON','cinzel',28,Wt,spacing=6)
save(im,'tide_art.png'); g=g.filter(ImageFilter.GaussianBlur(5)); save(g,'tide_glow.png')
specs['tide']={"out":R+"out/tide.png","art":R+"tide_art.png","glow":R+"tide_glow.png","glow_strength":2.5,"artmat":{"rough":0.3,"metal":0.3},
 "enclosure":{"color":[0.02,0.2,0.21],"metal":0.85,"rough":0.28,"coat":1.0,"bump":["noise",1400,0.04]},
 "knobs":[{"at":[330,560],"r":0.75,"style":"alu","v":0.35},{"at":[990,560],"r":0.75,"style":"alu","v":0.6},{"at":[660,880],"r":0.45,"style":"alu","v":0.5}],
 "toggles":[[660,470,-18]],"leds":[{"at":[660,1930],"color":[0.3,1.0,0.95],"strength":40}],
 "footswitches":[[660,2140,1.05]],"screws":SCR,"rimcolor":[0.6,1,1]}

# ---------- VORTEX (flanger) ----------
Kb=(18,12,8,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,Kb)
cx,cy=660,1300
for arm in range(5):
    pts=[]
    for t in range(0,240):
        th=t*0.045+arm*2*math.pi/5; r=26*math.exp(0.17*th)
        if r>310: break
        pts.append((cx+r*math.cos(th),cy+r*math.sin(th)))
    d.line(pts,fill=Kb,width=6,joint='curve')
d.ellipse((cx-318,cy-318,cx+318,cy+318),outline=Kb,width=3)
for x,n in ((330,'RATE'),(990,'DEPTH')): text(d,(x,690),n,'mich',30,Kb,spacing=8)
for x,n in ((330,'REGEN'),(990,'MANUAL')): text(d,(x,980),n,'mich',22,Kb,spacing=6)
text(d,(660,1750),'VORTEX','mich',112,Kb,spacing=16)
text(d,(660,1855),'THROUGH-ZERO FLANGER','mich',26,(60,35,20,255),spacing=8)
text(d,(660,2290),'ON','mich',26,Kb,spacing=6)
save(im,'vortex_art.png')
g=canvas(); gd=ImageDraw.Draw(g); gd.ellipse((cx-60,cy-60,cx+60,cy+60),outline=(80,170,255,255),width=10); g=g.filter(ImageFilter.GaussianBlur(5)); save(g,'vortex_glow.png')
specs['vortex']={"out":R+"out/vortex.png","art":R+"vortex_art.png","glow":R+"vortex_glow.png","glow_strength":3.0,"artmat":{"rough":0.6,"metal":0.0},
 "enclosure":{"color":[0.92,0.52,0.3],"metal":1.0,"rough":0.34,"bump":["brush",400,0.08]},
 "knobs":[{"at":[330,520],"r":0.7,"style":"black","v":0.4},{"at":[990,520],"r":0.7,"style":"black","v":0.7},{"at":[330,860],"r":0.42,"style":"black","v":0.6},{"at":[990,860],"r":0.42,"style":"black","v":0.3}],
 "leds":[{"at":[660,1300],"color":[0.3,0.65,1.0],"strength":45}],
 "footswitches":[[660,2140,1.05]],"screws":SCR,"rimcolor":[1,0.7,0.5]}

# ---------- ABYSS (octave) ----------
Ab=(150,225,205,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,Ab)
g=canvas(); gd=ImageDraw.Draw(g)
xs=[210,490,830,1110]
for x,n in zip(xs,['-2 OCT','-1 OCT','DRY','+1 OCT']): text(d,(x,720),n,'cinzel',28,Ab,spacing=4)
cx,cy=660,1270
for i,rr in enumerate(range(70,400,36)): d.ellipse((cx-rr,cy-rr,cx+rr,cy+rr),outline=(150,225,205,max(40,230-i*20)),width=3 if i%2 else 5)
for i in range(70):
    a=random.uniform(0,6.283); rr=random.uniform(90,380); x,y=cx+rr*math.cos(a),cy+rr*math.sin(a); s=random.uniform(3,9)
    gd.ellipse((x-s,y-s,x+s,y+s),fill=(60,255,190,255)); d.ellipse((x-s,y-s,x+s,y+s),fill=(150,255,215,255))
text(d,(660,1790),'Abyss','cdec',180,Ab)
text(d,(660,1910),'POLYPHONIC OCTAVE','cinzel',32,(110,180,165,255),spacing=12)
text(d,(660,2290),'ON','cinzel',28,Ab,spacing=6)
save(im,'abyss_art.png'); g=g.filter(ImageFilter.GaussianBlur(3)); save(g,'abyss_glow.png')
specs['abyss']={"out":R+"out/abyss.png","art":R+"abyss_art.png","glow":R+"abyss_glow.png","glow_strength":4.0,"artmat":{"rough":0.3,"metal":0.5},
 "enclosure":{"color":[0.008,0.035,0.032],"metal":0.7,"rough":0.3,"coat":0.8,"bump":["noise",1400,0.03]},
 "knobs":[{"at":[x,540],"r":0.48,"style":"alu","v":v} for x,v in zip(xs,[.3,.7,.6,.2])],
 "leds":[{"at":[660,1270],"color":[0.2,1.0,0.7],"strength":45}],
 "footswitches":[[660,2140,1.05]],"screws":SCR,"rimcolor":[0.5,1,0.85]}

# ---------- TESLA (ring mod / arc) ----------
Cu=(205,125,65,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,Cu)
g=canvas(); gd=ImageDraw.Draw(g)
cx=660
for y in range(1060,1460,14): d.arc((cx-110,y-18,cx+110,y+18),0,180,fill=Cu,width=4)
d.line([(cx-110,1060),(cx-110,1460)],fill=Cu,width=3); d.line([(cx+110,1060),(cx+110,1460)],fill=Cu,width=3)
d.rectangle((cx-170,1470,cx+170,1500),outline=Cu,width=4)
d.ellipse((cx-170,930,cx+170,1030),outline=Cu,width=6); d.ellipse((cx-110,955,cx+110,1005),outline=Cu,width=3)
def bolt(x,y,ang,n):
    pts=[(x,y)]
    for i in range(n):
        ang+=random.uniform(-0.7,0.7); x+=math.cos(ang)*random.uniform(16,30); y+=math.sin(ang)*random.uniform(16,30); pts.append((x,y))
        if random.random()<0.12: bolt(x,y,ang+random.uniform(-1,1),n//2)
    gd.line(pts,fill=(200,170,255,255),width=5); d.line(pts,fill=(235,225,255,255),width=2)
for a in (-2.6,-2.2,-1.0,-0.5,3.4,0.1): bolt(cx+170*math.cos(a)*0.95,980+50*math.sin(a),a,12)
for x,n in ((330,'FREQ'),(990,'ARC')): text(d,(x,740),n,'mich',30,Cu,spacing=8)
for x,n in ((220,'CHARGE'),(1100,'MIX')): text(d,(x,1350),n,'mich',22,Cu,spacing=4)
text(d,(660,1650),'TESLA','mich',118,Cu,spacing=20)
text(d,(660,1760),'RING MODULATOR','mich',28,(160,100,55,255),spacing=10)
text(d,(330,2280),'RING','mich',26,Cu,spacing=6); text(d,(660,2280),'STRIKE','mich',22,Cu,spacing=4); text(d,(990,2280),'ON','mich',26,Cu,spacing=6)
save(im,'tesla_art.png'); g=g.filter(ImageFilter.GaussianBlur(4)); save(g,'tesla_glow.png')
specs['tesla']={"out":R+"out/tesla.png","art":R+"tesla_art.png","glow":R+"tesla_glow.png","glow_strength":4.0,"artmat":{"rough":0.4,"metal":0.15},
 "enclosure":{"color":[0.012,0.012,0.016],"metal":0.3,"rough":0.15,"coat":1.0},
 "knobs":[{"at":[330,530],"r":0.72,"style":"copper","v":0.55},{"at":[990,530],"r":0.72,"style":"copper","v":0.8},{"at":[220,1200],"r":0.42,"style":"copper","v":0.4},{"at":[1100,1200],"r":0.42,"style":"copper","v":0.6}],
 "buttons":[{"at":[660,2130],"w":0.8,"h":0.55,"color":[0.4,0.3,1.0],"rough":0.3,"emit":[0.55,0.4,1.0],"estr":6}],
 "leds":[{"at":[660,1900],"color":[0.6,0.45,1.0],"strength":40}],
 "footswitches":[[330,2130,1.0],[990,2130,1.0]],"screws":SCR,"rimcolor":[0.7,0.6,1]}

# ---------- GRAVITY (tape stop / dive) ----------
Gw=(235,200,150,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,Gw)
g=canvas(); gd=ImageDraw.Draw(g)
cx,cy=660,1170
for i,(rx,ry) in enumerate([(440,105),(400,94),(360,84),(320,74),(280,64),(240,56)]):
    col=(255,int(150+i*15),int(60+i*20),255)
    gd.ellipse((cx-rx,cy-ry,cx+rx,cy+ry),outline=col,width=14-i)
    d.ellipse((cx-rx,cy-ry,cx+rx,cy+ry),outline=(235,200,150,180),width=2)
for i,rr in enumerate((270,250)): gd.arc((cx-rr,cy-rr*0.95,cx+rr,cy+rr*0.95),190,350,fill=(255,170,80,255),width=10-i*4)
d.ellipse((cx-128,cy-128,cx+128,cy+128),outline=Gw,width=3)
d.ellipse((cx-120,cy-120,cx+120,cy+120),fill=(0,0,0,255)); gd.ellipse((cx-120,cy-120,cx+120,cy+120),fill=(0,0,0,255))
gd.rectangle((cx-470,cy+2,cx+470,cy+40),fill=(0,0,0,0))
for i,(rx,ry) in enumerate([(440,105),(400,94),(360,84)]): gd.arc((cx-rx,cy-ry,cx+rx,cy+ry),0,180,fill=(255,150+i*20,60,255),width=14-i*2)
for x,n in ((300,'FALL'),(1020,'RISE')): text(d,(x,740),n,'cinzel',36,Gw,spacing=8)
text(d,(660,790),'CURVE','cinzel',26,Gw,spacing=6)
text(d,(660,1620),'Gravity','cinzel',168,Gw,spacing=8)
text(d,(660,1745),'TAPE STOP  ·  DIVE','cinzel',30,(190,150,100,255),spacing=10)
two_fs(d,Gw,'DROP','ON','cinzel',30)
save(im,'gravity_art.png'); g=g.filter(ImageFilter.GaussianBlur(5)); save(g,'gravity_glow.png')
specs['gravity']={"out":R+"out/gravity.png","art":R+"gravity_art.png","glow":R+"gravity_glow.png","glow_strength":3.5,"artmat":{"rough":0.4,"metal":0.3},
 "enclosure":{"color":[0.018,0.017,0.017],"metal":0.0,"rough":0.8,"bump":["noise",120,0.25]},
 "knobs":[{"at":[300,530],"r":0.72,"style":"black","v":0.7},{"at":[1020,530],"r":0.72,"style":"black","v":0.3},{"at":[660,620],"r":0.42,"style":"black","v":0.5}],
 "leds":[{"at":[660,1890],"color":[1.0,0.55,0.15],"strength":40}],
 "footswitches":[[330,2130,1.0],[990,2130,1.0]],"screws":SCR,"rimcolor":[1,0.7,0.4]}

# ---------- LEGION (big harmonizer) ----------
LW,LH=2600,2000
A=(222,170,80,255)
im=canvas(LW,LH); d=ImageDraw.Draw(im); frame(d,(222,170,80,150),inset=46,WW=LW,HH=LH)
g=canvas(LW,LH); gd=ImageDraw.Draw(g)
kx=[220+i*309 for i in range(8)]
for x,n in zip(kx,['KEY','SCALE','VOICE 1','VOICE 2','VOICE 3','VOICE 4','HUMANIZE','MIX']): text(d,(x,500),n,'mich',24,A,spacing=4)
d.rectangle((780,620,1820,1080),outline=A,width=3)
gd.rectangle((800,640,1800,1060),fill=(10,7,4,255))
f=font('mich',30); gd.text((1300,690),'E MINOR    AUTO FOLLOW',font=f,fill=(255,200,110,255),anchor='mm')
for i,(lab,h) in enumerate(zip(['-OCT','+3RD','+5TH','+OCT'],[150,230,190,120])):
    x=930+i*250; gd.rectangle((x-55,1000-h,x+55,1000),fill=(255,170,60,200)); gd.text((x,1030),lab,font=font('mich',24),fill=(255,220,160,255),anchor='mm')
for x,n in ((420,'FOLLOW'),(420,'LEARN')): pass
text(d,(410,720),'FOLLOW','mich',22,A,spacing=4); text(d,(410,960),'LEARN','mich',22,A,spacing=4)
text(d,(2190,720),'CHORD','mich',22,A,spacing=4); text(d,(2190,960),'DRONE','mich',22,A,spacing=4)
text(d,(1300,1250),'LEGION','mich',120,A,spacing=34)
text(d,(1300,1360),'POLYPHONIC HARMONY ENGINE','mich',26,(180,140,70,255),spacing=12)
for s in (-1,1):
    d.line([(1300+s*560,1250),(1300+s*900,1250)],fill=A,width=3); d.line([(1300+s*600,1272),(1300+s*860,1272)],fill=A,width=1)
fx=[400+i*600 for i in range(4)]
for x,n in zip(fx,['VOICES','FOLLOW','HOLD','ON']): text(d,(x,1880),n,'mich',26,A,spacing=6)
save(im,'legion_art.png'); save(g,'legion_glow.png')
specs['legion']={"out":R+"out/legion.png","size":[13.0,10.0,3.0],"art":R+"legion_art.png","glow":R+"legion_glow.png","glow_strength":2.4,"artmat":{"rough":0.3,"metal":0.8},
 "enclosure":{"color":[0.06,0.058,0.062],"metal":0.85,"rough":0.35,"bump":["brush",500,0.05]},
 "knobs":[{"at":[x,310],"r":0.56,"style":"alu" if i in (0,1,7) else "black","v":[.3,.6,.7,.55,.4,.2,.35,.5][i]} for i,x in enumerate(kx)],
 "buttons":[{"at":[600,720],"w":0.85,"h":0.6,"color":[0.06,0.055,0.05]},{"at":[600,960],"w":0.85,"h":0.6,"color":[0.06,0.055,0.05]},{"at":[2000,720],"w":0.85,"h":0.6,"color":[0.06,0.055,0.05]},{"at":[2000,960],"w":0.85,"h":0.6,"color":[0.06,0.055,0.05]}],
 "leds":[{"at":[x,1500],"color":[1.0,0.65,0.2],"strength":40 if i in (0,1,3) else 0.3} for i,x in enumerate(fx)],
 "footswitches":[[x,1680,1.0] for x in fx],
 "screws":[[110,110],[2490,110],[110,1890],[2490,1890]],"res":[1170,780],"lens":34,"cam":[0,-10,22],"target":[0,-0.4,1.5],"rimcolor":[1,0.75,0.4]}

for k,v in specs.items(): json.dump(v,open(R+k+'.json','w'))
print('arts2 ok',list(specs))
