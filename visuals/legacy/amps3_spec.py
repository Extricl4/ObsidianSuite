from amp_helpers import *
import importlib, sys
sys.argv=['x']
exec(open(R+'amps2_spec.py').read().split('# ===== PLVTO v2 =====')[0])   # helpers: chevron_mask, diamond_mask, logo_img, std_panel, KN
specs={}
def herring_mask(n,wu,hu,sc=100,rows=3,slw=34,pitch=72,ang=32,m=46):
    w,h=int(wu*sc),int(hu*sc); im=Image.new('RGBA',(w,h),(0,0,0,0)); d=ImageDraw.Draw(im); t=math.tan(math.radians(ang))
    rh=(h-2*m-(rows-1)*24)/rows
    for r in range(rows):
        y0=m+r*(rh+24); y1=y0+rh; s=1 if r%2==0 else -1; x=m+(rh*t if s<0 else 0)
        while x+slw+rh*t<w-m+ (rh*t if s<0 else 0) and x<w-m-slw:
            if s>0: pts=[(x,y1),(x+slw,y1),(x+slw+rh*t,y0),(x+rh*t,y0)]
            else: pts=[(x,y1),(x+slw,y1),(x+slw-rh*t,y0),(x-rh*t,y0)]
            if min(p[0] for p in pts)>m and max(p[0] for p in pts)<w-m: d.polygon(pts,fill=(255,255,255,255))
            x+=pitch
    im.save(R+n); return R+n
def bar_mask(n,wu,hu,sc=100,bh=46,gap=40,m=50,cols=2,cgap=120):
    w,h=int(wu*sc),int(hu*sc); im=Image.new('RGBA',(w,h),(0,0,0,0)); d=ImageDraw.Draw(im)
    cw=(w-2*m-(cols-1)*cgap)/cols; y=m
    while y+bh<h-m:
        for c in range(cols):
            x0=m+c*(cw+cgap); d.rounded_rectangle((x0,y,x0+cw,y+bh),bh//2,fill=(255,255,255,255))
        y+=bh+gap
    im.save(R+n); return R+n
def circle_mask(n,wu,hu,sc=100,r=26,step=70,m=50):
    w,h=int(wu*sc),int(hu*sc); im=Image.new('RGBA',(w,h),(0,0,0,0)); d=ImageDraw.Draw(im); row=0; y=m+r
    while y<h-m-r:
        x=m+r+(step/2 if row%2 else 0)
        while x<w-m-r: d.ellipse((x-r,y-r,x+r,y+r),fill=(255,255,255,255)); x+=step
        y+=step*0.87; row+=1
    im.save(R+n); return R+n

# ---------- Styx family (matte faded wood) ----------
def styx_like(key,c2,jewel,knob='black',label=None):
    specs[key]={"out":R+f"out/{key}.png","size":[22,9,8.6],"border":0.75,"recess":0.45,"corners":"L",
     "zones":[{"type":"wood","z":[4.15,7.85],"planks":2,"c1":[0.004,0.003,0.003],"c2":c2,"fade":[0.35,1.25],"coat":0.0,"rough":0.8},
      {"type":"panel","z":[0.75,4.12],"art":R+"amp_art.png","color":[0.016,0.016,0.018],"rough":0.62,"artemis":0.35,
       "knobs":[kn(-4.37,1.9,0.62,.7,knob),kn(4.43,1.9,0.62,.45,knob)]+KN([-2.71,-1.54,-0.37,1.38,2.57],1.9,0.38,[.45,.5,.55,.4,.6],knob),
       "toggles":[[7.49,1.9],[8.37,1.9]],"jewels":[{"x":9.38,"z":1.9,"color":jewel,"r":0.26}]}],
     "rimcolor":[min(1,c*2.2+0.3) for c in c2]}
styx_like('hades',[0.42,0.025,0.02],[1,0.1,0.06])
styx_like('hecate',[0.26,0.07,0.45],[0.7,0.35,1.0],'alu')
styx_like('nemesis',[0.03,0.32,0.16],[0.3,1.0,0.5])

# ---------- Plvto family (deep grille, tubes) ----------
def plvto_like(key,mask,tubecol,panel_names,knob,labcol,acc,grille=[0.016,0.016,0.018],gmetal=0.55,panelcol=[0.014,0.014,0.016],pmetal=0,body=None,rim=[1,0.5,0.3]):
    W,B=24.0,0.7; IW=W-2*B; n=len(panel_names); sp=min(2.6,15.5/(n-1)); xs=[-(n-1)*sp/2-1.0+i*sp for i in range(n)]; KZ=2.35
    def ex(pn): label(pn,8.4,1.45,'STBY',labcol,'cinzel',22,4); label(pn,9.4,1.45,'POWER',labcol,'cinzel',22,4)
    art=std_panel(f'{key}_panel.png',W,B,0.7,4.25,xs,panel_names,KZ,labcol,'cinzel',26,0.45,True,ex,(0.95,acc))
    specs[key]={"out":R+f"out/{key}.png","size":[24,10,10.5],"border":B,"recess":2.4,
     "zones":[{"type":"perf","z":[4.25,9.8],"mask":mask(f'{key}_mask.png',IW-0.04,9.8-4.25),"thick":0.3,"mesh":4,"color":grille,"metal":gmetal,"rough":0.42,
       "tubes":[{"x":x,"h":2.6,"r":0.42,"color":tubecol,"strength":8,"light":60} for x in (-8.1,-5.4,-2.7,0,2.7,5.4,8.1)],
       "ambient":[{"x":0,"energy":60,"color":tubecol,"size":16}]},
      {"type":"panel","z":[0.7,4.25],"art":art,"color":panelcol,"metal":pmetal,"rough":0.6 if not pmetal else 0.35,"bump":["brush",60,0.03] if pmetal else ["noise",300,0.04],
       "knobs":KN(xs,KZ,0.45,[.75,.5,.45,.6,.5,.6,.4,.5][:n],knob),"toggles":[[8.4,2.35]],
       "jewels":[{"x":9.4,"z":2.35,"color":tubecol,"r":0.3,"strength":25,"button":True}]}],"rimcolor":rim}
    if body: specs[key]["body"]=body
plvto_like('cerberus',herring_mask,[1,0.32,0.05],['GAIN','LOW','MID','HIGH','PRESENCE','DEPTH','MASTER'],'black',(205,200,195,255),(230,110,20,255))
plvto_like('ares',bar_mask,[1,0.12,0.06],['GAIN 1','GAIN 2','BASS','MID','TREBLE','PRES','MASTER'],'alu',(215,215,220,255),(200,30,30,255),
           grille=[0.13,0.13,0.14],gmetal=0.9,panelcol=[0.1,0.1,0.105],pmetal=0.9,body={"color":[0.03,0.03,0.032],"rough":0.75,"bump":["leather",42,0.55]},rim=[1,0.45,0.4])

# ---------- Nyx family (sheet + ambient tubes) ----------
def nyx_like(key,mask,cols,jewel,knob,labcol,panelcol,pmetal,body,logo,rim):
    W,B=24.0,0.5; IW=W-2*B; xs=[-8.1,-5.7,-3.3,-0.9,1.5,3.9,6.3]; KZ=1.75
    def ex(pn): label(pn,-10.4,KZ,logo,labcol,'mich',40,12); label(pn,8.6,KZ-0.72,'MODE',labcol,'mich',15,2)
    art=std_panel(f'{key}_panel.png',W,B,0.5,3.4,xs,['GAIN','BASS','MID','TREBLE','PRESENCE','DEPTH','VOLUME'],KZ,labcol,'mich',20,0.38,False,ex)
    specs[key]={"out":R+f"out/{key}.png","size":[24,10,10],"border":B,"recess":1.6,"body":body,"bevel":0.2,
     "zones":[{"type":"perf","z":[3.45,9.5],"mask":mask(f'{key}_mask.png',IW-0.04,9.5-3.45),"color":body["color"],"metal":0.75,"rough":0.38,
       "tubes":[{"x":x,"h":2.4,"r":0.38,"color":cols[i%len(cols)],"strength":10,"light":70} for i,x in enumerate((-8.4,-5.6,-2.8,0,2.8,5.6,8.4))],
       "ambient":[{"x":-6,"energy":60,"color":cols[0],"size":6},{"x":0,"energy":60,"color":cols[1%len(cols)],"size":6},{"x":6,"energy":60,"color":cols[-1],"size":6}]},
      {"type":"panel","z":[0.5,3.45],"art":art,"color":panelcol,"metal":pmetal,"rough":0.4,"bump":["brush",60,0.03],
       "knobs":KN(xs,KZ,0.38,[.8,.5,.6,.55,.45,.6,.4],knob),"toggles":[[8.6,KZ]],"jewels":[{"x":10.2,"z":KZ,"color":jewel,"r":0.28}]}],
     "rimcolor":rim,"world":[0.008,0.01,0.012]}
nyx_like('thanatos',hex_mask,[[0.1,0.85,0.95],[0.85,0.95,1.0],[0.1,0.6,1.0]],[0.2,0.9,1.0],'alu',(215,225,230,255),[0.02,0.022,0.026],0.7,
         {"color":[0.018,0.02,0.024],"metal":0.6,"rough":0.42,"bump":["noise",200,0.05]},'THANATOS',[0.5,0.9,1.0])
nyx_like('orpheus',circle_mask,[[1,0.6,0.15],[1,0.78,0.35],[1,0.45,0.1]],[1,0.7,0.25],'black',(40,32,22,255),[0.62,0.52,0.36],1.0,
         {"color":[0.06,0.045,0.03],"metal":0.7,"rough":0.4,"bump":["noise",200,0.05]},'ORPHEUS',[1,0.8,0.5])

# ---------- MNEMOSYNE (NAM loader, matte black) ----------
W,B=26,0.5; HH_=9.0; Wt=(225,225,228,255); Gy=(115,115,120,255)
pn=Panel(W,B,0.5,HH_-0.5)
xs=[-10.0+i*2.3 for i in range(8)]; KZ=1.75
for x,nm in zip(xs,['INPUT','GAIN','BASS','MID','TREBLE','PRESENCE','DEPTH','OUTPUT']):
    ticks(pn,x,KZ,104,116,21,Gy,3); label(pn,x,KZ-0.85,nm,Wt,'mich',21,5)
label(pn,-8.6,8.0,'MNEMOSYNE',Wt,'mich',40,16); label(pn,-8.6,7.62,'NEURAL CAPTURE ENGINE',Gy,'mich',15,8)
SCX,SCZ,SCW,SCH=-2.0,5.45,11.0,4.12
SKX=[SCX-SCW/2+(200+k*400)/2400*SCW for k in range(6)]
label(pn,7.0,4.3,'BROWSE  /  PUSH',Wt,'mich',18,4)
for i,s_ in enumerate('ABCD'): label(pn,8.55+i*1.05,7.62,s_,Wt,'mich',26,0)
label(pn,10.1,8.12,'CAPTURE SLOT',Gy,'mich',17,6)
for x,l in ((8.9,'IR'),(10.0,'CAB'),(11.1,'GATE')): label(pn,x,4.3,l,Wt,'mich',17,3)
label(pn,11.0,KZ-0.85,'POWER',Wt,'mich',18,3)
label(pn,-11.1,6.65,'INPUT',Wt,'mich',17,3); label(pn,-11.1,4.75,'USB-C',Wt,'mich',15,3)
y=pn.P(0,3.25)[1]; pn.d.line([(pn.P(-12.2,0)[0],y),(pn.P(12.2,0)[0],y)],fill=(42,42,45,255),width=2)
art=pn.save('mnemosyne_panel.png')
SW,SH=2400,900; sc=canvas(SW,SH); sd=ImageDraw.Draw(sc); sd.rectangle((0,0,SW,SH),fill=(5,6,8,255))
f=lambda s: font('mich',s); WH=(238,238,242,255); GR=(120,124,135,255); BL=(110,190,255,255); DK=(28,30,36,255)
sd.rectangle((0,0,SW,70),fill=(16,17,21,255)); sd.text((30,18),'SLOT A',font=f(30),fill=BL); sd.text((220,18),'RHYTHM  /  HIGH GAIN',font=f(30),fill=WH)
sd.text((SW-30,18),'48 kHz    CPU 23%    120 BPM',font=f(28),fill=GR,anchor='ra')
sd.rectangle((0,70,560,SH-110),fill=(10,11,14,255)); sd.text((30,95),'CAPTURES',font=f(24),fill=GR)
items=['PLVTO  RHYTHM','PLVTO  LEAD','STYX  CRUNCH','EREBUS  TIGHT','TARTARUS  CH2','NYX  AMBIENT','ACHERON  RESIN']
for k,it in enumerate(items):
    y=145+k*68
    if k==0: sd.rectangle((14,y-10,546,y+46),fill=(28,52,80,255))
    sd.text((40,y),it,font=f(26),fill=WH if k==0 else (185,188,196,255))
sd.text((610,100),'PLVTO RHYTHM',font=f(66),fill=WH)
sd.text((610,190),'NAM  /  FEATHER  /  STANDARD',font=f(28),fill=BL)
for k,(a_,b_) in enumerate((('ESR','0.0081'),('INPUT CAL','-18 dBu'),('LATENCY','2.6 ms'),('TAGS','DJENT  TIGHT  7-STRING'))):
    sd.text((610,265+k*58),a_,font=f(24),fill=GR); sd.text((900,265+k*58),b_,font=f(26),fill=WH)
sd.rounded_rectangle((1560,100,2370,460),16,outline=DK,width=4); sd.text((1590,120),'IR   4x12 V30',font=f(26),fill=WH); sd.text((1590,165),'SM57 + R121 BLEND',font=f(22),fill=GR)
pts=[(1590+k*7,350-60*math.exp(-((k-30)/18)**2)+70*math.exp(-((k-85)/14)**2)-20*math.sin(k/9)) for k in range(108)]
sd.line(pts,fill=BL,width=5); sd.line([(1590,350),(2340,350)],fill=DK,width=2)
for k,(lab,v) in enumerate((('IN',0.62),('OUT',0.55))):
    y=530+k*70; sd.text((610,y),lab,font=f(24),fill=GR); sd.rectangle((720,y+6,2370,y+30),fill=DK)
    for s_ in range(int(60*v)): sd.rectangle((722+s_*27,y+8,744+s_*27,y+28),fill=WH if s_<44 else (255,190,80,255))
sd.text((610,690),'GATE  -52 dB',font=f(24),fill=GR); sd.text((1000,690),'OPEN',font=f(24),fill=(120,230,140,255))
sd.rectangle((0,SH-110,SW,SH),fill=(14,15,19,255))
for k,l in enumerate(['MODEL','IR','EQ','GATE','TUNER','SAVE']):
    x=200+k*400; sd.text((x,SH-55),l,font=f(30),fill=WH if k==0 else GR,anchor='mm')
    if k==0: sd.rectangle((x-90,SH-16,x+90,SH-8),fill=BL)
sd.rectangle((2,2,SW-3,SH-3),outline=(55,58,66,255),width=5)
sc.save(R+'mnemosyne_screen.png')
line=canvas(2600,20); ImageDraw.Draw(line).rectangle((0,4,2600,16),fill=(235,240,255,255)); line.save(R+'mnemosyne_line.png')
specs['mnemosyne']={"out":R+"out/mnemosyne.png","size":[W,10,HH_],"border":B,"recess":0.45,"bevel":0.3,"body":{"color":[0.02,0.02,0.022],"metal":0.3,"rough":0.8,"bump":["noise",160,0.08]},
 "zones":[{"type":"plate","z":[0.5,HH_-0.5],"art":art,"color":[0.011,0.011,0.012],"rough":0.92,"bump":0,"artemis":0.25,
   "knobs":KN(xs,KZ,0.44,[.3,.7,.5,.45,.6,.5,.55,.4],'black')+[kn(7.0,5.65,0.8,.5,'black')],
   "jacks":[[-11.1,6.0,"jack"],[-11.1,4.25,"usb"]],"toggles":[[8.9,4.85],[10.0,4.85],[11.1,4.85]],
   "jewels":[{"x":8.55+i*1.05,"z":6.95,"color":[0.45,0.75,1.0] if i==0 else [0.9,0.9,0.95],"r":0.26,"strength":30 if i==0 else 3,"button":True} for i in range(4)]
     +[{"x":x,"z":2.95,"color":[0.45,0.75,1.0] if k==0 else [0.85,0.85,0.9],"r":0.2,"strength":20 if k==0 else 2,"button":True} for k,x in enumerate(SKX)]
     +[{"x":11.0,"z":KZ,"color":[0.9,0.92,1.0],"r":0.3,"strength":10,"button":True}],
   "logos":[{"img":R+'mnemosyne_screen.png',"x":SCX,"z":SCZ,"w":SCW,"h":SCH,"plate":[0.035,0.035,0.04],"pm":0.6,"pr":0.3,"metal":0,"rough":0.12,"emis":2.6},
            {"img":R+'mnemosyne_line.png',"x":0,"z":0.62,"w":24.6,"h":0.09,"metal":0,"rough":0.3,"emis":6.0}]}],
 "cam":[-7.8,-50,12.5],"target":[0.3,0,4.3],"rimcolor":[0.6,0.75,1.0],"world":[0.008,0.009,0.012]}

# ---------- CALLIOPE (acoustic-style clean, Petrucci-inspired) ----------
W,B=24,0.8; Br=(70,45,25,255)
xs=[-8.4,-6.3,-4.2,-2.1,0,2.1,4.2,6.3]; KZ=2.1
def cax(pn): label(pn,8.3,KZ-0.75,'BRIGHT',Br,'cinzel',16,2); label(pn,9.3,KZ-0.75,'POWER',Br,'cinzel',16,2)
art=std_panel('calliope_panel.png',W,B,0.8,4.1,xs,['VOLUME','BASS','MIDDLE','TREBLE','PRESENCE','CHORUS','REVERB','MASTER'],KZ,Br,'cinzel',19,0.4,True,cax)
ro=canvas(900,900); rd=ImageDraw.Draw(ro); C1=(235,220,190,255); C2=(150,95,45,255)
for i,(rr,w,c) in enumerate(((440,10,C1),(410,4,C2),(390,14,C1),(350,4,C2),(330,8,C1))): rd.ellipse((450-rr,450-rr,450+rr,450+rr),outline=c,width=w)
for k in range(72):
    a=math.radians(k*5); rd.line([(450+352*math.cos(a),450+352*math.sin(a)),(450+388*math.cos(a),450+388*math.sin(a))],fill=C2 if k%2 else C1,width=6)
text(rd,(450,430),'Calliope','cdec',130,C1); text(rd,(450,540),'ACOUSTIC CLEAN','cinzel',30,C1,spacing=10)
ro.save(R+'calliope_logo.png')
specs['calliope']={"out":R+"out/calliope.png","size":[24,10,10.5],"border":B,"recess":0.5,"body":{"color":[0.42,0.24,0.1],"rough":0.45,"bump":["noise",8,0.15]},
 "zones":[{"type":"cloth","z":[4.1,9.7],"color":[0.3,0.21,0.13],"weave":90,"piping":[0.85,0.78,0.62],
   "logos":[{"img":R+'calliope_logo.png',"x":0,"z":6.9,"w":4.8,"h":4.8,"metal":0.4,"rough":0.3,"emis":0.15}]},
  {"type":"panel","z":[0.8,4.1],"art":art,"color":[0.78,0.72,0.58],"rough":0.7,"artemis":0,
   "knobs":KN(xs,KZ,0.4,[.5,.45,.55,.6,.4,.3,.35,.5],'black'),"toggles":[[8.3,KZ]],
   "jewels":[{"x":9.3,"z":KZ,"color":[1,0.6,0.2],"r":0.3}]}],"rimcolor":[1,0.85,0.65]}

# ---------- HALCYON (open-core clean, Plini-inspired) ----------
W=24; Dg=(60,60,64,255)
xs=[-7.5,-5.0,-2.5,0,2.5,5.0]; KZ=2.15
def hax(pn): label(pn,7.4,KZ-0.7,'BRIGHT',Dg,'mich',14,2); label(pn,8.6,KZ-0.7,'POWER',Dg,'mich',14,2); label(pn,-9.2,KZ+0.35,'EOS',Dg,'mich',34,16)
art=std_panel('eos_panel.png',W,0.8,0.4,4.0,xs,['GAIN','BASS','MID','TREBLE','PRESENCE','MASTER'],KZ,Dg,'mich',16,0.4,False,hax)
warm=[1,0.55,0.22]
specs['eos']={"out":R+"out/eos.png","size":[24,11,9.5],"border":0.8,"recess":0.45,
 "open":{"cheek":0,"chassis_h":3.6,"c1":[0.05,0.025,0.012],"c2":[0.34,0.18,0.08],"chassis":[0.6,0.6,0.62],
   "tubes":[{"x":x,"y":-0.6,"h":2.8,"r":0.5,"color":warm,"strength":2.5,"light":25} for x in (-5.4,-1.8,1.8,5.4)]+[{"x":x,"y":-2.6,"h":1.5,"r":0.27,"color":warm,"strength":2,"light":10} for x in (-7.6,-6.2,6.2,7.6)],
   "trans":[{"x":-6.2,"y":2.6,"w":3.4,"d":3.0,"h":2.8},{"x":0,"y":2.8,"w":2.6,"d":2.4,"h":2.2},{"x":6.2,"y":2.6,"w":3.4,"d":3.0,"h":2.8}]},
 "zones":[{"type":"panel","z":[0.4,4.0],"art":art,"color":[0.86,0.86,0.84],"rough":0.55,"artemis":0,
   "knobs":KN(xs,KZ,0.4,[.4,.5,.55,.6,.45,.5],'alu'),"toggles":[[7.4,KZ]],
   "jewels":[{"x":8.6,"z":KZ,"color":[1,0.85,0.6],"r":0.26,"strength":15,"button":True}]}],
 "cam":[-8,-47,20],"target":[0.3,0,3.8],"rimcolor":[1,0.85,0.7]}

# ---------- AETHER (modern clean) ----------
W,B=23,0.6; Cu=(60,32,18,255)
xs=[-7.2,-5.0,-2.8,-0.6,1.6,3.8,6.0]; KZ=1.95
def aex(pn): label(pn,8.3,KZ-0.72,'POWER',Cu,'mich',14,2)
art=std_panel('aether_panel.png',W,B,0.6,3.6,xs,['VOLUME','BASS','MID','TREBLE','AIR','COMP','MASTER'],KZ,Cu,'mich',16,0.38,False,aex)
specs['aether']={"out":R+"out/aether.png","size":[23,9.5,9],"border":B,"recess":0.45,"bevel":0.45,"body":{"color":[0.82,0.82,0.83],"rough":0.6,"bump":["leather",20,0.2]},
 "zones":[{"type":"cloth","z":[3.6,8.4],"color":[0.42,0.42,0.44],"weave":110,
   "logos":[{"img":logo_img('aether_logo.png','AETHER','mich',150,(200,120,80,255),spacing=40),"x":0,"z":6.0,"w":5.2,"h":1.36,"metal":1.0,"rough":0.25,"emis":0.0}]},
  {"type":"panel","z":[0.6,3.6],"art":art,"color":[0.72,0.42,0.26],"metal":1.0,"rough":0.3,"bump":["brush",60,0.03],"artemis":0,
   "knobs":KN(xs,KZ,0.38,[.5,.45,.5,.6,.55,.3,.5],'black'),
   "jewels":[{"x":8.3,"z":KZ,"color":[0.5,0.8,1.0],"r":0.26}]}],"rimcolor":[0.8,0.9,1.0]}

for k,v in specs.items(): json.dump(v,open(R+k+'.json','w'))
print('ok',list(specs))
