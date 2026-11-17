from art_common import *
import math, json, random
from PIL import ImageFilter
random.seed(3)
R='/home/claude/r3d/'
specs={}
def save(im,n): im.save(R+n); return R+n

# ---------- CRYPT (reverb) ----------
G=(236,196,112,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,G)
cx,cy=660,1180
d.line([(380,1560),(380,1050)],fill=G,width=6); d.line([(940,1560),(940,1050)],fill=G,width=6)
d.arc((380,700,1500,1820),180,240,fill=G,width=6); d.arc((-180,700,940,1820),300,360,fill=G,width=6)
for rr,w in ((230,6),(205,2),(80,4)): d.ellipse((cx-rr,cy-rr,cx+rr,cy+rr),outline=G,width=w)
for i in range(12):
    a=math.radians(i*30); d.line([(cx+80*math.cos(a),cy+80*math.sin(a)),(cx+205*math.cos(a),cy+205*math.sin(a))],fill=G,width=3)
    b=math.radians(i*30+15); px,py=cx+150*math.cos(b),cy+150*math.sin(b); d.ellipse((px-26,py-26,px+26,py+26),outline=G,width=2)
for x in (470,850): d.line([(x,1420),(x,1540)],fill=G,width=3); d.arc((x-40,1380,x+40,1460),180,360,fill=G,width=3)
text(d,(370,810),'DECAY','cinzel',40,G,spacing=8); text(d,(950,810),'SIZE','cinzel',40,G,spacing=8)
text(d,(250,1040),'TONE','cinzel',32,G,spacing=6); text(d,(1070,1040),'MIX','cinzel',32,G,spacing=6)
text(d,(560,960),'ROOM','cinzel',24,G,spacing=4); text(d,(760,960),'CRYPT','cinzel',24,G,spacing=4)
text(d,(660,1760),'Crypt','frak',230,G)
text(d,(660,1900),'CATHEDRAL REVERB','cinzel',36,(190,160,100,255),spacing=12)
save(im,'crypt_art.png')
g=canvas(); gd=ImageDraw.Draw(g)
cols=[(150,18,22,255),(170,95,20,255),(22,40,130,255),(100,12,40,255)]
for i in range(12):
    b=math.radians(i*30+15); px,py=cx+150*math.cos(b),cy+150*math.sin(b); gd.ellipse((px-22,py-22,px+22,py+22),fill=cols[i%4])
for i in range(12):
    a0=i*30; gd.pieslice((cx-200,cy-200,cx+200,cy+200),a0+3,a0+27,fill=(cols[(i+1)%4][0]//3,cols[(i+1)%4][1]//3,cols[(i+1)%4][2]//3,255))
gd.ellipse((cx-76,cy-76,cx+76,cy+76),fill=(0,0,0,0))
save(g,'crypt_glow.png')
specs['crypt']={"out":R+"out/crypt.png","art":R+"crypt_art.png","glow":R+"crypt_glow.png","glow_strength":1.6,"artmat":{"rough":0.28,"metal":0.55},
 "enclosure":{"color":[0.055,0.052,0.05],"metal":0.0,"rough":0.85,"bump":["noise",90,0.35]},
 "knobs":[{"at":[370,560],"r":0.85,"style":"black","v":0.66},{"at":[950,560],"r":0.85,"style":"black","v":0.48},{"at":[250,860],"r":0.5,"style":"black","v":0.5},{"at":[1070,860],"r":0.5,"style":"black","v":0.3}],
 "toggles":[[660,880,18]],"leds":[{"at":[660,1180],"color":[1.0,0.25,0.15],"strength":40}],"footswitches":[[660,2130,1.05]],
 "screws":[[110,110],[1210,110],[110,2290],[1210,2290]],"rimcolor":[1,0.75,0.5],"world":[0.01,0.009,0.008],"floor":[0.014,0.013,0.012]}

# ---------- SPECTRE (modulation) ----------
Wt=(240,236,255,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,(230,220,255,180))
wisp=canvas(); wd=ImageDraw.Draw(wisp)
for k in range(14):
    y0=1150+k*9; amp=60-k*2
    pts=[(x, y0+amp*math.sin(x/160+k*.4)*math.sin(x/700+1.3)) for x in range(70,1251,5)]
    wd.line(pts,fill=(235,228,255,int(60+12*k)),width=3)
wisp=wisp.filter(ImageFilter.GaussianBlur(1.2)); im=Image.alpha_composite(im,wisp); d=ImageDraw.Draw(im)
text(d,(300,780),'RATE','cinzel',36,Wt,spacing=8); text(d,(660,760),'DEPTH','cinzel',40,Wt,spacing=8); text(d,(1020,780),'MIX','cinzel',36,Wt,spacing=8)
text(d,(470,1010),'CHORUS','cinzel',24,Wt,spacing=4); text(d,(660,1010),'PHASE','cinzel',24,Wt,spacing=4); text(d,(850,1010),'FLANGE','cinzel',24,Wt,spacing=4)
text(d,(660,1620),'SPECTRE','cdec',170,Wt)
text(d,(660,1770),'MODULATION','cinzel',38,(210,200,240,255),spacing=16)
save(im,'spectre_art.png')
g=canvas(); gd=ImageDraw.Draw(g)
for r in range(70,20,-6): gd.ellipse((660-r,1330-r,660+r,1330+r),outline=(150,90,255,int(40+(70-r)*3)),width=3)
save(g.filter(ImageFilter.GaussianBlur(2)),'spectre_glow.png')
specs['spectre']={"out":R+"out/spectre.png","art":R+"spectre_art.png","glow":R+"spectre_glow.png","glow_strength":4,"artmat":{"rough":0.35,"metal":0.0},
 "enclosure":{"color":[0.11,0.02,0.2],"metal":0.85,"rough":0.22,"coat":0.9,"bump":["noise",1400,0.04]},
 "knobs":[{"at":[300,560],"r":0.62,"style":"alu","v":0.35},{"at":[660,480],"r":0.85,"style":"alu","v":0.6},{"at":[1020,560],"r":0.62,"style":"alu","v":0.45}],
 "toggles":[[660,930,-18]],"leds":[{"at":[660,1330],"color":[0.6,0.35,1.0],"strength":45}],"footswitches":[[660,2130,1.05]],
 "screws":[[110,110],[1210,110],[110,2290],[1210,2290]],"rimcolor":[0.8,0.6,1],"world":[0.008,0.006,0.012],"floor":[0.012,0.01,0.016]}

# ---------- STASIS (freeze) ----------
Sl=(30,48,62,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,Sl)
def branch(x,y,a,L,depth):
    if depth==0 or L<8: return
    x2,y2=x+L*math.cos(a),y+L*math.sin(a); d.line([(x,y),(x2,y2)],fill=(30,48,62,int(90+30*depth)),width=max(1,depth))
    for da in (-1.05,1.05): branch(x+(x2-x)*.55,y+(y2-y)*.55,a+da,L*.42,depth-1)
    branch(x2,y2,a,L*.6,depth-1)
for i in range(6): branch(660,1050,math.radians(i*60-90),230,5)
text(d,(300,560),'DECAY','cinzel',32,Sl,spacing=6); text(d,(660,560),'LAYERS','cinzel',32,Sl,spacing=6); text(d,(1020,560),'MIX','cinzel',32,Sl,spacing=6)
text(d,(660,1610),'STASIS','cdec',170,Sl)
text(d,(660,1760),'FREEZE · HOLD','cinzel',38,(50,72,90,255),spacing=14)
save(im,'stasis_art.png')
g=canvas(); gd=ImageDraw.Draw(g)
pts=[(660+330*math.cos(math.radians(a)),1050+330*math.sin(math.radians(a))) for a in range(30,390,60)]
gd.polygon(pts,fill=(90,190,255,140)); g=g.filter(ImageFilter.GaussianBlur(26))
gd=ImageDraw.Draw(g); gd.polygon(pts,outline=(150,220,255,255),width=5)
save(g,'stasis_glow.png')
specs['stasis']={"out":R+"out/stasis.png","art":R+"stasis_art.png","glow":R+"stasis_glow.png","glow_strength":3,"artmat":{"rough":0.4,"metal":0.0},
 "enclosure":{"color":[0.62,0.7,0.76],"metal":0.15,"rough":0.18,"coat":0.8,"bump":["noise",1200,0.03]},
 "knobs":[{"at":[300,380],"r":0.55,"style":"black","v":0.55},{"at":[660,380],"r":0.55,"style":"black","v":0.4},{"at":[1020,380],"r":0.55,"style":"black","v":0.6}],
 "buttons":[{"at":[660,1050],"w":2.3,"h":2.3,"color":[0.85,0.92,0.98],"rough":0.12,"emit":[0.5,0.8,1.0],"estr":0.6}],
 "leds":[{"at":[660,1400],"color":[0.4,0.8,1.0],"strength":40}],"footswitches":[[660,2130,1.05]],
 "screws":[[110,110],[1210,110],[110,2290],[1210,2290]],"rimcolor":[0.6,0.85,1],"key":1500,"world":[0.006,0.008,0.012],"floor":[0.012,0.014,0.018]}

# ---------- ASCEND (pitch) ----------
Lk=(20,20,22,255)
im=canvas(); d=ImageDraw.Draw(im)
labels=['−2','−1','−5','−4','DET','+4','+5','+1','+2']
cx,cy=560,2020
for i,l in enumerate(labels):
    a=math.radians(-135+i*270/8); x=cx+190*math.sin(a); y=cy-190*math.cos(a); text(d,(x,y),l,'saira',30,Lk)
    d.line([(cx+150*math.sin(a),cy-150*math.cos(a)),(cx+165*math.sin(a),cy-165*math.cos(a))],fill=Lk,width=3)
text(d,(200,2200),'MIX','saira',30,Lk,spacing=6); text(d,(1080,2230),'ON','saira',30,Lk,spacing=6)
save(im,'ascend_art.png')
t=canvas(1200,1500); td=ImageDraw.Draw(t)
text(td,(600,1200),'ASCEND','cdec',150,(225,228,235,255)); text(td,(600,1320),'PITCH · WHAMMY','cinzel',40,(170,175,185,255),spacing=14)
for k in range(5): td.line([(150,260+k*24),(1050,260+k*24)],fill=(80,220,255,200 - k*30),width=4)
save(t,'ascend_tread.png')
specs['ascend']={"out":R+"out/ascend.png","art":R+"ascend_art.png","artmat":{"rough":0.4,"metal":0.0},
 "enclosure":{"color":[0.55,0.56,0.59],"metal":1.0,"rough":0.32,"bump":["brush",200,0.08]},
 "treadle":{"at":[660,840],"w":6.0,"l":7.6,"art":R+"ascend_tread.png","glow":1.2},
 "knobs":[{"at":[560,2020],"r":0.62,"style":"black","v":0.875},{"at":[200,2020],"r":0.38,"style":"black","v":0.7}],
 "leds":[{"at":[900,2020],"color":[0.3,0.85,1.0],"strength":40}],"footswitches":[[1080,2020,0.95]],"jacks":True,"rimcolor":[0.6,0.9,1]}

# ---------- PHOTON (laser) ----------
Wh=(235,235,240,255)
im=canvas(); d=ImageDraw.Draw(im); frame(d,(235,235,240,120))
for k in range(18): d.line([(660,1180),(80+k*68,2300)],fill=(235,235,240,40),width=2)
for k in range(8): d.line([(80,1300+k*130),(1240,1300+k*130)],fill=(235,235,240,int(30+k*6)),width=2)
for (x,y,t) in [(330,320,'PITCH'),(990,320,'SWEEP'),(330,820,'RATE'),(990,820,'MIX')]: text(d,(x,y+200),t,'mich',30,Wh,spacing=6)
text(d,(660,1560),'PHOTON','mich',120,Wh,spacing=10)
text(d,(660,1680),'LASER · ZAP · SWEEP','mich',30,(190,190,200,255),spacing=8)
save(im,'photon_art.png')
g=canvas(); gd=ImageDraw.Draw(g)
gd.line([(70,1180),(1250,1180)],fill=(255,40,40,255),width=10)
g=g.filter(ImageFilter.GaussianBlur(10)); gd=ImageDraw.Draw(g); gd.line([(70,1180),(1250,1180)],fill=(255,170,170,255),width=4)
gd.ellipse((660-120,1180-120,660+120,1180+120),outline=(255,60,60,255),width=6)
save(g,'photon_glow.png')
specs['photon']={"out":R+"out/photon.png","art":R+"photon_art.png","glow":R+"photon_glow.png","glow_strength":7,"artmat":{"rough":0.3,"metal":0.0},
 "enclosure":{"color":[0.012,0.012,0.014],"metal":0.2,"rough":0.15,"coat":1.0,"bump":["noise",1500,0.02]},
 "knobs":[{"at":[330,320],"r":0.6,"style":"alu","v":0.7},{"at":[990,320],"r":0.6,"style":"alu","v":0.4},{"at":[330,820],"r":0.6,"style":"alu","v":0.55},{"at":[990,820],"r":0.6,"style":"alu","v":0.5}],
 "buttons":[{"at":[660,1180],"w":1.0,"h":1.0,"color":[0.6,0.02,0.02],"rough":0.2,"emit":[1.0,0.1,0.1],"estr":6}],
 "leds":[{"at":[660,560],"color":[1.0,0.15,0.15],"strength":40}],"footswitches":[[660,2130,1.05]],
 "screws":[[110,110],[1210,110],[110,2290],[1210,2290]],"rimcolor":[1,0.4,0.4],"world":[0.008,0.004,0.004],"floor":[0.014,0.01,0.01]}

# ---------- LAMENT (wah) ----------
WW,HH=1520,3000
im=canvas(WW,HH); d=ImageDraw.Draw(im)
text(d,(760,2860),'ON','saira',30,(200,200,205,255),spacing=6)
save(im,'lament_art.png')
t=canvas(1300,2400); td=ImageDraw.Draw(t)
for k in range(26): td.line([(110,180+k*62),(1190,180+k*62)],fill=(60,60,66,255),width=12)
for k in range(3): td.rounded_rectangle((80+k*14,80+k*14,1220-k*14,2320-k*14),60,outline=(150,40,48,200-k*50),width=4)
td.rounded_rectangle((300,1900,1000,2200),40,fill=(0,0,0,0))
text(td,(650,1980),'Lament','frak',200,(205,205,210,255)); text(td,(650,2130),'WAH','cinzel',44,(170,170,176,255),spacing=24)
save(t,'lament_tread.png')
specs['lament']={"out":R+"out/lament.png","size":[7.6,15.0,2.2],"art":R+"lament_art.png","artmat":{"rough":0.4,"metal":0.0},
 "enclosure":{"color":[0.09,0.008,0.014],"metal":0.75,"rough":0.25,"coat":1.0,"bump":["noise",1400,0.03]},
 "treadle":{"at":[760,1300],"w":6.6,"l":12.4,"art":R+"lament_tread.png","glow":0},
 "leds":[{"at":[1300,2860],"color":[1.0,0.2,0.15],"strength":40}],"jacks":True,"lens":36,"cam":[0,-12.5,29],"target":[0,-1.0,1.5]}

# ---------- SHATTER (big glitch) ----------
GW,GH=3000,2000
Rd=(225,40,45,255); Wg=(225,222,218,255)
im=canvas(GW,GH); d=ImageDraw.Draw(im); frame(d,(225,222,218,120),inset=46,WW=GW,HH=GH)
names=['RATE','LENGTH','CHAOS','PITCH','REVERSE','CRUSH','FILTER','MIX']
for i,n in enumerate(names): text(d,(300+i*343,560),n,'mich',26,Wg,spacing=4)
for k in range(40):
    x=random.randint(120,2880); y=random.randint(640,760); w=random.randint(20,180); d.rectangle((x,y,x+w,y+random.randint(4,14)),fill=(225,40,45,random.randint(80,220)))
d.rectangle((900,820,2100,1180),outline=Wg,width=3)
text(d,(1500,1300),'SHATTER','mich',110,Wg,spacing=18)
text(d,(1500,1400),'GLITCH WORKSTATION','mich',28,(170,165,160,255),spacing=12)
for i,n in enumerate(['STUTTER','REVERSE','FREEZE','ON']): text(d,(450+i*700,1890),n,'mich',28,Rd if n!='ON' else Wg,spacing=6)
for i,n in enumerate(['MODE','SYNC']): text(d,(470,900+i*180),n,'mich',22,Wg,spacing=4)
for i,n in enumerate(['HOLD','RAND']): text(d,(2530,900+i*180),n,'mich',22,Wg,spacing=4)
save(im,'shatter_art.png')
g=canvas(GW,GH); gd=ImageDraw.Draw(g)
gd.rectangle((920,840,2080,1160),fill=(8,10,14,255))
for k in range(7):
    y=868+k*28; gd.rectangle((960,y,960+random.randint(200,1050),y+16),fill=(255,60,60,random.randint(90,230)) if k%3==0 else (230,230,235,random.randint(60,160)))
f=font('mich',36); gd.text((1500,1122),'STUTTER 1/16   CHAOS 72%',font=f,fill=(255,255,255,255),anchor='mm')
save(g,'shatter_glow.png')
specs['shatter']={"out":R+"out/shatter.png","size":[15.0,10.0,3.0],"art":R+"shatter_art.png","glow":R+"shatter_glow.png","glow_strength":2.2,"artmat":{"rough":0.35,"metal":0.0},
 "enclosure":{"color":[0.014,0.014,0.016],"metal":0.0,"rough":0.62,"bump":["noise",700,0.07]},
 "knobs":[{"at":[300+i*343,330],"r":0.62,"style":"black","v":[.5,.35,.72,.5,.2,.4,.6,.55][i]} for i in range(8)],
 "buttons":[{"at":[700,900],"w":0.9,"h":0.6,"color":[0.06,0.06,0.07]},{"at":[700,1080],"w":0.9,"h":0.6,"color":[0.06,0.06,0.07]},{"at":[2300,900],"w":0.9,"h":0.6,"color":[0.06,0.06,0.07]},{"at":[2300,1080],"w":0.9,"h":0.6,"color":[0.06,0.06,0.07]}],
 "leds":[{"at":[450+i*700,1530],"color":[1.0,0.12,0.12] if i<3 else [1.0,1.0,1.0],"strength":35 if i in (0,3) else 0.3} for i in range(4)],
 "footswitches":[[450+i*700,1700,1.0] for i in range(4)],
 "screws":[[110,110],[2890,110],[110,1890],[2890,1890]],"res":[1170,780],"lens":34,"cam":[0,-10.5,23],"target":[0,-0.4,1.5],"rimcolor":[1,0.5,0.5]}

for k,v in specs.items():
    json.dump(v,open(R+k+'.json','w'))
print('arts ok', list(specs))
