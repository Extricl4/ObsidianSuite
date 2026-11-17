from art_common import *
import math, json, random, colorsys
from PIL import ImageFilter, Image
random.seed(21)
R='/home/claude/r3d/'
specs={}
def save(im,n): im.save(R+n); return R+n
SCR=[[110,110],[1210,110],[110,2290],[1210,2290]]
def blank_glow(): return canvas()

# ---------- PRISM (phaser) holographic ----------
im=canvas(); px=im.load()
for y in range(0,H,2):
    for x in range(0,W,2):
        h=((x*0.6+y*0.35)/1500 + 0.08*math.sin(x/90+y/140))%1.0
        r,g,b=colorsys.hsv_to_rgb(h,0.55,0.95)
        c=(int(r*255),int(g*255),int(b*255),255)
        px[x,y]=c; px[x+1,y]=c; px[x,y+1]=c; px[x+1,y+1]=c
d=ImageDraw.Draw(im); K=(15,15,20,255)
frame(d,K)
cx,cy=660,1230
oct_=[(cx+260*math.cos(math.radians(22.5+i*45)),cy+260*math.sin(math.radians(22.5+i*45))) for i in range(8)]
inn=[(cx+140*math.cos(math.radians(22.5+i*45)),cy+140*math.sin(math.radians(22.5+i*45))) for i in range(8)]
d.polygon(oct_,outline=K,width=10); d.polygon(inn,outline=K,width=6)
for a,b in zip(oct_,inn): d.line([a,b],fill=K,width=4)
for i in range(8): d.line([inn[i],(cx,cy)],fill=K,width=2)
for x,n in ((330,'RATE'),(990,'DEPTH')): text(d,(x,760),n,'mich',30,K,spacing=8)
for x,n in ((330,'STAGES'),(990,'REGEN')): text(d,(x,1520),n,'mich',22,K,spacing=6)
text(d,(660,1760),'PRISM','mich',120,K,spacing=22); text(d,(660,1865),'SPECTRAL PHASER','mich',28,K,spacing=10)
text(d,(660,2290),'ON','mich',26,K,spacing=6)
save(im,'prism_art.png')
g=canvas(); gd=ImageDraw.Draw(g)
gd.polygon([(cx+140*math.cos(math.radians(22.5+i*45)),cy+140*math.sin(math.radians(22.5+i*45))) for i in range(8)],outline=(255,255,255,255),width=10)
g=g.filter(ImageFilter.GaussianBlur(4)); save(g,'prism_glow.png')
specs['prism']={"out":R+"out/prism.png","art":R+"prism_art.png","glow":R+"prism_glow.png","glow_strength":3.0,"artmat":{"rough":0.15,"metal":0.9},
 "enclosure":{"color":[0.75,0.75,0.8],"metal":1.0,"rough":0.15},
 "knobs":[{"at":[330,540],"r":0.72,"style":"black","v":0.4},{"at":[990,540],"r":0.72,"style":"black","v":0.7},{"at":[330,1380],"r":0.42,"style":"black","v":0.5},{"at":[990,1380],"r":0.42,"style":"black","v":0.6}],
 "leds":[{"at":[660,1990],"color":[1,1,1],"strength":30}],"footswitches":[[660,2140,1.05]],"screws":SCR,"rimcolor":[0.8,0.6,1]}

# ---------- PULSE (tremolo) hot magenta ----------
Wp=(255,235,245,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,Wp)
g=canvas(); gd=ImageDraw.Draw(g)
cx,cy=660,1220
for i in range(7):
    rr=60+i*52; d.ellipse((cx-rr,cy-rr,cx+rr,cy+rr),outline=(255,235,245,255-i*28),width=6 if i%2==0 else 2)
pts=[]
for x in range(100,1221,6):
    t=(x-100)/1120; pts.append((x,1700+ (80 if (int(t*10)%2==0) else -80)*min(1,abs(math.sin(t*31.4))*3)))
gd.ellipse((cx-60,cy-60,cx+60,cy+60),outline=(255,90,200,255),width=14)
for x,n in ((330,'SPEED'),(990,'DEPTH')): text(d,(x,760),n,'cinzel',36,Wp,spacing=8)
text(d,(660,560),'SINE   SQUARE','cinzel',22,Wp)
text(d,(660,1720),'Pulse','cdec',190,Wp)
text(d,(660,1840),'HARMONIC TREMOLO','cinzel',30,Wp,spacing=10)
text(d,(330,2280),'TAP','cinzel',28,Wp,spacing=6); text(d,(990,2280),'ON','cinzel',28,Wp,spacing=6)
save(im,'pulse_art.png'); g=g.filter(ImageFilter.GaussianBlur(5)); save(g,'pulse_glow.png')
specs['pulse']={"out":R+"out/pulse.png","art":R+"pulse_art.png","glow":R+"pulse_glow.png","glow_strength":3.0,"artmat":{"rough":0.3,"metal":0.2},
 "enclosure":{"color":[0.75,0.02,0.32],"metal":0.3,"rough":0.2,"coat":1.0},
 "knobs":[{"at":[330,540],"r":0.72,"style":"cream","v":0.6},{"at":[990,540],"r":0.72,"style":"cream","v":0.45}],
 "toggles":[[660,640,-18]],"leds":[{"at":[660,1220],"color":[1,0.3,0.8],"strength":45}],
 "footswitches":[[330,2130,1.0],[990,2130,1.0]],"screws":SCR,"rimcolor":[1,0.5,0.8]}

# ---------- AURORA (ambient swell / pad) ----------
Wa=(215,235,240,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,Wa)
g=canvas(); gd=ImageDraw.Draw(g)
for k in range(140):
    x=120+k*8; base=1450+60*math.sin(k/9)
    h=320+140*math.sin(k/13+1)+60*math.sin(k/5)
    hue=0.33+0.45*(k/140)
    r,gg,b=colorsys.hsv_to_rgb(hue%1,0.8,1)
    for j in range(0,int(h),6):
        a=int(255*(1-j/h)**1.6)
        gd.line([(x,base-j),(x+6,base-j)],fill=(int(r*255),int(gg*255),int(b*255),a),width=6)
for s in range(120):
    x,y=random.randint(80,1240),random.randint(80,1000); r_=random.choice((2,2,3,4)); d.ellipse((x-r_,y-r_,x+r_,y+r_),fill=Wa)
d.line([(70,1460),(1250,1460)],fill=Wa,width=3)
for i in range(12): d.polygon([(70+i*100,1460),(120+i*100,1400-(i%3)*30),(170+i*100,1460)],fill=Wa)
for x,n in ((330,'SWELL'),(990,'DECAY')): text(d,(x,740),n,'cinzel',34,Wa,spacing=8)
for x,n in ((220,'TONE'),(660,'SHIFT'),(1100,'MIX')): text(d,(x,1015),n,'cinzel',24,Wa,spacing=4)
text(d,(660,1690),'Aurora','cdec',170,Wa)
text(d,(660,1810),'AMBIENT SWELL','cinzel',32,(160,200,210,255),spacing=12)
text(d,(330,2280),'HOLD','cinzel',28,Wa,spacing=6); text(d,(990,2280),'ON','cinzel',28,Wa,spacing=6)
save(im,'aurora_art.png'); g=g.filter(ImageFilter.GaussianBlur(6)); save(g,'aurora_glow.png')
specs['aurora']={"out":R+"out/aurora.png","art":R+"aurora_art.png","glow":R+"aurora_glow.png","glow_strength":3.2,"artmat":{"rough":0.4,"metal":0.0},
 "enclosure":{"color":[0.008,0.012,0.03],"metal":0.5,"rough":0.35,"coat":0.8,"bump":["noise",1400,0.03]},
 "knobs":[{"at":[330,530],"r":0.72,"style":"alu","v":0.65},{"at":[990,530],"r":0.72,"style":"alu","v":0.8},{"at":[220,880],"r":0.4,"style":"alu","v":0.4},{"at":[660,880],"r":0.4,"style":"alu","v":0.3},{"at":[1100,880],"r":0.4,"style":"alu","v":0.6}],
 "leds":[{"at":[660,1940],"color":[0.3,1,0.6],"strength":40}],
 "footswitches":[[330,2130,1.0],[990,2130,1.0]],"screws":SCR,"rimcolor":[0.5,1,0.8]}

# ---------- BLOOM (envelope filter) sunset gradient ----------
im=canvas(); px=im.load()
for y in range(H):
    t=y/H; c=(int(255*(1-t)+235*t),int(150*(1-t)+40*t),int(40*(1-t)+120*t),255)
    for x in range(W): px[x,y]=c
d=ImageDraw.Draw(im); Kb=(40,10,25,255); frame(d,Kb)
cx,cy=660,1180
for i in range(10):
    a=math.radians(i*36)
    ex,ey=cx+190*math.cos(a),cy+190*math.sin(a)
    pts=[]
    for t in range(0,41):
        f=t/40; w=math.sin(f*math.pi)*80
        bx,by=cx+(ex-cx)*f*1.6,cy+(ey-cy)*f*1.6
        pts.append((bx-math.sin(a)*w,by+math.cos(a)*w))
    for t in range(40,-1,-1):
        f=t/40; w=math.sin(f*math.pi)*80
        bx,by=cx+(ex-cx)*f*1.6,cy+(ey-cy)*f*1.6
        pts.append((bx+math.sin(a)*w,by-math.cos(a)*w))
    d.polygon(pts,outline=Kb,width=5)
d.ellipse((cx-60,cy-60,cx+60,cy+60),fill=Kb)
for x,n in ((330,'SENS'),(990,'RANGE')): text(d,(x,720),n,'cinzel',34,Kb,spacing=8)
text(d,(660,400),'UP   DOWN','cinzel',22,Kb)
text(d,(660,1700),'Bloom','cdec',190,Kb)
text(d,(660,1820),'ENVELOPE FILTER','cinzel',30,Kb,spacing=12)
text(d,(660,2290),'ON','cinzel',28,Kb,spacing=6)
save(im,'bloom_art.png')
specs['bloom']={"out":R+"out/bloom.png","art":R+"bloom_art.png","artmat":{"rough":0.18,"metal":0.25},
 "enclosure":{"color":[0.9,0.4,0.15],"metal":0.2,"rough":0.2,"coat":1.0},
 "knobs":[{"at":[330,520],"r":0.72,"style":"black","v":0.55},{"at":[990,520],"r":0.72,"style":"black","v":0.7}],
 "toggles":[[660,500,18]],"leds":[{"at":[660,1180],"color":[1,0.6,0.3],"strength":40}],
 "footswitches":[[660,2140,1.05]],"screws":SCR,"rimcolor":[1,0.6,0.7]}

# ---------- EMBER (fuzz) ----------
O=(255,140,40,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,(200,90,30,255))
g=canvas(); gd=ImageDraw.Draw(g)
for s in range(260):
    x=random.gauss(660,260); y=1500-abs(random.gauss(0,420))
    if not (70<x<1250 and 1000<y<1560): continue
    r_=random.uniform(2,8); c=(255,int(random.uniform(60,190)),20,255)
    gd.ellipse((x-r_,y-r_,x+r_,y+r_),fill=c); d.ellipse((x-r_*0.6,y-r_*0.6,x+r_*0.6,y+r_*0.6),fill=(255,200,120,255))
cx,cy=660,620
for i in range(11):
    a=math.radians(135+i*27); text(d,(cx+300*math.cos(a),cy+300*math.sin(a)),str(i),'mich',30,O)
text(d,(cx,cy+380),'FUZZ','mich',34,O,spacing=12)
for x,n in ((230,'TONE'),(1090,'VOL')): text(d,(x,1080),n,'mich',22,O,spacing=6)
text(d,(660,1700),'EMBER','mich',128,O,spacing=22)
text(d,(660,1810),'SILICON FUZZ','mich',30,(200,110,40,255),spacing=12)
text(d,(660,2290),'ON','mich',26,O,spacing=6)
save(im,'ember_art.png'); g=g.filter(ImageFilter.GaussianBlur(3)); save(g,'ember_glow.png')
specs['ember']={"out":R+"out/ember.png","art":R+"ember_art.png","glow":R+"ember_glow.png","glow_strength":4.0,"artmat":{"rough":0.5,"metal":0.0},
 "enclosure":{"color":[0.02,0.016,0.014],"metal":0.0,"rough":0.88,"bump":["noise",90,0.3]},
 "knobs":[{"at":[660,620],"r":1.05,"style":"copper","v":0.85},{"at":[230,930],"r":0.42,"style":"copper","v":0.4},{"at":[1090,930],"r":0.42,"style":"copper","v":0.65}],
 "leds":[{"at":[660,1960],"color":[1,0.45,0.1],"strength":45}],
 "footswitches":[[660,2140,1.05]],"screws":SCR,"rimcolor":[1,0.55,0.3]}

# ---------- HEX (bitcrusher) acid green ----------
Kh=(10,20,8,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,Kh)
g=canvas(); gd=ImageDraw.Draw(g)
for gx in range(0,16):
    for gy in range(0,10):
        x=180+gx*60; y=1000+gy*60
        lvl=int(5+4*math.sin(gx*0.7))
        if 9-gy<lvl and random.random()>0.08:
            d.rectangle((x,y,x+50,y+50),fill=Kh)
            if random.random()<0.25: gd.rectangle((x+6,y+6,x+44,y+44),fill=(220,255,60,255))
for x,n in ((330,'BITS'),(990,'RATE')): text(d,(x,740),n,'mich',32,Kh,spacing=8)
text(d,(660,890),'MIX','mich',22,Kh,spacing=6)
text(d,(660,1740),'HEX','mich',150,Kh,spacing=40)
text(d,(660,1860),'BITCRUSHER  /  LO-FI','mich',26,Kh,spacing=8)
text(d,(330,2280),'CRUSH','mich',24,Kh,spacing=6); text(d,(990,2280),'ON','mich',24,Kh,spacing=6)
save(im,'hex_art.png'); g=g.filter(ImageFilter.GaussianBlur(3)); save(g,'hex_glow.png')
specs['hex']={"out":R+"out/hex.png","art":R+"hex_art.png","glow":R+"hex_glow.png","glow_strength":2.5,"artmat":{"rough":0.5,"metal":0.0},
 "enclosure":{"color":[0.45,0.8,0.05],"metal":0.2,"rough":0.35,"coat":0.6},
 "knobs":[{"at":[330,530],"r":0.72,"style":"black","v":0.25},{"at":[990,530],"r":0.72,"style":"black","v":0.6},{"at":[660,760],"r":0.38,"style":"black","v":0.7}],
 "leds":[{"at":[660,1960],"color":[0.7,1,0.2],"strength":40}],
 "footswitches":[[330,2130,1.0],[990,2130,1.0]],"screws":SCR,"rimcolor":[0.8,1,0.5]}

for k,v in specs.items(): json.dump(v,open(R+k+'.json','w'))
print('arts3 ok',list(specs))
