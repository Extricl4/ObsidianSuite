from amp_helpers import *
specs={}
def chevron_mask(n,wu,hu,sc=100):
    w,h=int(wu*sc),int(hu*sc); im=Image.new('RGBA',(w,h),(0,0,0,0)); d=ImageDraw.Draw(im)
    m=46; spine=130; cx=w/2; slw=40; pitch=86; t=math.tan(math.radians(28))
    rows=[(m,h/2-14),(h/2+14,h-m)]
    for (y0,y1) in rows:
        hh=y1-y0
        for side in (-1,1):
            x=cx+side*(spine/2+24)
            while (side>0 and x+hh*t+slw<w-m) or (side<0 and x-hh*t-slw>m):
                if side>0: pts=[(x,y1),(x+slw,y1),(x+slw+hh*t,y0),(x+hh*t,y0)]
                else: pts=[(x,y1),(x-slw,y1),(x-slw-hh*t,y0),(x-hh*t,y0)]
                d.polygon(pts,fill=(255,255,255,255)); x+=side*pitch
    im.save(R+n); return R+n
def diamond_mask(n,wu,hu,sc=100,s=70,m=46):
    w,h=int(wu*sc),int(hu*sc); im=Image.new('RGBA',(w,h),(0,0,0,0)); d=ImageDraw.Draw(im)
    row=0; y=m+s
    while y<h-m-s*0.6:
        x=m+s+(s if row%2 else 0)
        while x<w-m-s*0.6:
            d.polygon([(x,y-s*0.8),(x+s*0.8,y),(x,y+s*0.8),(x-s*0.8,y)],fill=(255,255,255,255)); x+=2*s
        y+=s; row+=1
    im.save(R+n); return R+n
def logo_img(n,txt,font_k,size,col,w=1300,h=340,sub=None,plate=None,spacing=0):
    lg=canvas(w,h); ld=ImageDraw.Draw(lg)
    if plate: ld.rounded_rectangle((0,0,w,h),30,fill=plate)
    text(ld,(w//2,h//2-(20 if sub else 0)),txt,font_k,size,col,spacing=spacing)
    if sub: text(ld,(w//2,h-55),sub,'cinzel',30,col,spacing=10)
    lg.save(R+n); return R+n
def std_panel(name,W,B,za,zb,xs,names,KZ,col,font_k='cinzel',fs=28,r=0.4,nums=False,extra=None,line=None):
    pn=Panel(W,B,za,zb)
    for x,nm in zip(xs,names):
        ticks(pn,x,KZ,int(r*PXU)+14,int(r*PXU)+32,11,col,3)
        if nums: numbers(pn,x,KZ,r,col,'cinzel',20,rr=int(r*PXU)+58)
        label(pn,x,KZ+r+0.55+(0.2 if nums else 0),nm,col,font_k,fs,5)
    if line:
        y=pn.P(0,line[0])[1]; pn.d.line([(pn.P(-pn.w/2+0.6,0)[0],y),(pn.P(pn.w/2-0.6,0)[0],y)],fill=line[1],width=4)
    if extra: extra(pn)
    return pn.save(name)
KN=lambda xs,KZ,r,vs,st: [kn(x,KZ,r,v,st) for x,v in zip(xs,vs)]
CAM={"rimcolor":[0.8,0.85,1.0]}

# ===== PLVTO v2 =====
W,B=24.0,0.7; IW=W-2*B; C=(200,196,190,255); Rd=(170,20,20,255)
xs=[-6.5,-3.9,-1.3,1.3,3.9,6.5]; KZ=2.35
def plx(pn): label(pn,9.4,1.45,'POWER',C,'cinzel',22,4); label(pn,8.4,1.45,'STBY',C,'cinzel',22,4)
art=std_panel('plvto_panel.png',W,B,0.7,4.25,xs,['GAIN','BASS','MID','TREBLE','PRESENCE','DEPTH'],KZ,C,'cinzel',32,0.5,True,plx,(0.95,Rd))
specs['plvto']={"out":R+"out/plvto.png","size":[24,10,10.5],"border":B,"recess":2.4,
 "zones":[{"type":"perf","z":[4.25,9.8],"mask":chevron_mask('plvto_mask.png',IW-0.04,9.8-4.25),"thick":0.3,"mesh":4,"color":[0.016,0.016,0.018],"metal":0.55,"rough":0.42,
   "tubes":[{"x":x,"h":2.6,"r":0.42,"color":[1,0.16,0.04],"strength":8,"light":60} for x in (-8.1,-5.4,-2.7,0,2.7,5.4,8.1)],
   "ambient":[{"x":0,"energy":60,"color":[1,0.15,0.05],"size":16}]},
  {"type":"panel","z":[0.7,4.25],"art":art,"color":[0.014,0.014,0.016],"rough":0.75,
   "knobs":KN(xs,KZ,0.5,[.8,.55,.45,.6,.5,.65],'black'),"toggles":[[8.4,2.35]],
   "jewels":[{"x":9.4,"z":2.35,"color":[1,0.08,0.04],"r":0.3,"strength":25,"button":True}]}],
 "rimcolor":[1,0.45,0.35]}

# ===== ACHERON (resin rhythm, Styx layout) =====
specs['acheron']={"out":R+"out/acheron.png","size":[22,9,8.6],"border":0.75,"recess":0.45,"corners":"L",
 "zones":[{"type":"wood","z":[4.15,7.85],"planks":2,"c1":[0.004,0.002,0.001],"c2":[0.36,0.15,0.045],"fade":[0.35,1.25],"coat":0.0,"rough":0.8},
  {"type":"panel","z":[0.75,4.12],"art":R+"amp_art.png","color":[0.016,0.016,0.018],"rough":0.62,"artemis":0.35,
   "knobs":KN([-4.37,-2.71,-1.54,-0.37,1.38,2.57,4.43],1.9,0.38,[.62,.45,.5,.55,.4,.6,.45],'black')[:0]+[kn(-4.37,1.9,0.62,.62,'black'),kn(4.43,1.9,0.62,.45,'black')]+KN([-2.71,-1.54,-0.37,1.38,2.57],1.9,0.38,[.45,.5,.55,.4,.6],'black'),
   "toggles":[[7.49,1.9],[8.37,1.9]],"jewels":[{"x":9.38,"z":1.9,"color":[1.0,0.55,0.2],"r":0.26}]}],
 "rimcolor":[1,0.75,0.5]}

# ===== ICARUS (lead: gold) =====
W,B=26,0.8; Kd=(25,20,12,255); Gd=(215,170,80,255)
xs=[-9.0,-7.2,-5.4,-3.6,-1.8,0,1.8,3.6,5.4,7.2]; KZ=2.4
def icx(pn):
    label(pn,-7.2,4.45,'CH 1  CLEAN',Kd,'mich',20,6); label(pn,-0.9,4.45,'CH 2  LEAD',Kd,'mich',20,6); label(pn,6.3,4.45,'MASTER',Kd,'mich',20,6)
    for a,b in ((-9.4,-5.0),(-4.0,2.2),(3.2,7.6)): y=pn.P(0,4.15)[1]; pn.d.line([(pn.P(a,0)[0],y),(pn.P(b,0)[0],y)],fill=Kd,width=3)
    label(pn,9.2,KZ-0.75,'STBY',Kd,'mich',15,2); label(pn,10.1,KZ-0.75,'POWER',Kd,'mich',15,2)
art=std_panel('icarus_panel.png',W,B,0.8,4.9,xs,['VOL','TONE','GAIN','BASS','MID','TREBLE','LEAD','PRES','DEPTH','VOL'],KZ,Kd,'mich',22,0.36,False,icx)
specs['icarus']={"out":R+"out/icarus.png","size":[26,11,11],"border":B,"recess":0.5,"corners":"L",
 "zones":[{"type":"cloth","z":[4.9,10.2],"piping":[0.85,0.65,0.3],"weave":70,
   "logos":[{"img":logo_img('icarus_logo.png','ICARUS','cdec',190,(230,185,90,255),spacing=18),"x":0,"z":7.6,"w":6.6,"h":1.72,"metal":0.95,"rough":0.25,"emis":0.1}]},
  {"type":"panel","z":[0.8,4.9],"art":art,"color":[0.75,0.55,0.25],"metal":1.0,"rough":0.3,"bump":["brush",60,0.04],"artemis":0,
   "knobs":KN(xs,KZ,0.36,[.4,.6,.9,.55,.45,.6,.8,.5,.55,.35],'chicken'),"toggles":[[9.2,KZ],[10.1,KZ]],
   "jewels":[{"x":11.1,"z":KZ,"color":[1.0,0.12,0.06],"r":0.3}]}],
 "rimcolor":[1,0.8,0.5]}

# ===== ERINYES (lead: oxblood + diamond metal grille) =====
W,B=25,0.8; IW=W-2*B; Cs=(220,220,225,255); Rr=(215,40,50,255)
xs=[-8.4,-6.4,-4.4,-2.4,-0.4,1.6,3.6,5.6]; KZ=2.2
def erx(pn): label(pn,8.3,KZ-0.75,'BOOST',Cs,'mich',15,2); label(pn,9.4,KZ-0.75,'POWER',Cs,'mich',15,2); label(pn,-10.4,KZ,'III',Rr,'cinzel',44,4)
art=std_panel('erinyes_panel.png',W,B,0.8,4.2,xs,['GAIN','BASS','MID','TREBLE','CONTOUR','PRESENCE','RESONANCE','MASTER'],KZ,Cs,'mich',17,0.4,False,erx,(1.05,Rr))
specs['erinyes']={"out":R+"out/erinyes.png","size":[25,10.5,10.5],"border":B,"recess":1.8,"corners":"chrome","body":{"color":[0.16,0.012,0.018],"rough":0.6,"bump":["leather",36,0.55]},
 "zones":[{"type":"perf","z":[4.2,9.7],"mask":diamond_mask('erinyes_mask.png',IW-0.04,9.7-4.2),"thick":0.12,"color":[0.6,0.6,0.62],"metal":1.0,"rough":0.25,
   "tubes":[{"x":x,"h":2.3,"r":0.38,"color":[1,0.35,0.1],"strength":5,"light":30} for x in (-6,-3,0,3,6)],
   "logos":[{"img":logo_img('erinyes_logo.png','Erinyes','frak',220,(230,40,50,255),plate=(12,10,10,255)),"x":0,"z":6.95,"w":5.4,"h":1.42,"metal":0.0,"rough":0.35,"emis":0.35}]},
  {"type":"panel","z":[0.8,4.2],"art":art,"color":[0.015,0.015,0.017],"rough":0.4,"metal":0.6,"artemis":0.3,
   "knobs":KN(xs,KZ,0.4,[.85,.5,.4,.6,.45,.55,.6,.4],'alu'),"toggles":[[8.3,KZ],[9.4,KZ]],
   "jewels":[{"x":10.5,"z":KZ,"color":[1,0.05,0.1],"r":0.3}]}],
 "rimcolor":[1,0.4,0.4]}

# ===== ELYSIUM (clean: cream vintage) =====
W,B=24,0.8; Kd=(20,20,20,255)
xs=[-8.0,-6.0,-4.0,-2.0,0,2.0,4.0,6.0]; KZ=2.0
def elx(pn): label(pn,8.1,KZ-0.75,'STBY',Kd,'cinzel',16,2); label(pn,9.1,KZ-0.75,'POWER',Kd,'cinzel',16,2); label(pn,-10.0,KZ+0.4,'VIBRATO',Kd,'cinzel',16,2)
art=std_panel('elysium_panel.png',W,B,0.8,3.85,xs,['VOLUME','TREBLE','MIDDLE','BASS','REVERB','SPEED','INTENSITY','MASTER'],KZ,Kd,'cinzel',20,0.42,True,elx)
specs['elysium']={"out":R+"out/elysium.png","size":[24,10,11],"border":B,"recess":0.5,"corners":"chrome","body":{"color":[0.72,0.64,0.48],"rough":0.7,"bump":["leather",36,0.45]},
 "zones":[{"type":"cloth","z":[3.85,10.2],"color":[0.32,0.22,0.12],"weave":90,"piping":[0.8,0.75,0.62],
   "logos":[{"img":logo_img('elysium_logo.png','Elysium','cdec',170,(235,225,205,255)),"x":-6.4,"z":8.9,"w":5.0,"h":1.3,"metal":0.9,"rough":0.25,"emis":0.15}]},
  {"type":"panel","z":[0.8,3.85],"art":art,"color":[0.62,0.62,0.64],"metal":1.0,"rough":0.28,"bump":["brush",60,0.03],"artemis":0,
   "knobs":KN(xs,KZ,0.42,[.5,.6,.5,.45,.35,.4,.3,.6],'black'),"toggles":[[8.1,KZ],[9.1,KZ],[-10.0,KZ-0.2]],
   "jewels":[{"x":10.1,"z":KZ,"color":[1,0.15,0.05],"r":0.3}]}],
 "rimcolor":[1,0.9,0.75]}

# ===== NEREID (clean: seafoam + diamond cloth) =====
dc=canvas(256,256); dd=ImageDraw.Draw(dc); dd.rectangle((0,0,256,256),fill=(50,30,18,255))
dd.polygon([(128,6),(250,128),(128,250),(6,128)],outline=(170,135,75,255),width=10); dd.polygon([(128,40),(216,128),(128,216),(40,128)],outline=(110,80,45,255),width=5)
dc.save(R+'nereid_cloth.png')
W,B=23,0.75; Cc=(235,225,200,255)
xs=[-7.5,-5.4,-3.3,-1.2,0.9,3.0,5.1]; KZ=2.0
def nex(pn): label(pn,7.3,KZ-0.75,'BRIGHT',Cc,'cinzel',16,2); label(pn,8.4,KZ-0.75,'POWER',Cc,'cinzel',16,2)
art=std_panel('nereid_panel.png',W,B,0.75,3.75,xs,['VOLUME','TREBLE','BASS','CUT','REVERB','TREM','MASTER'],KZ,Cc,'cinzel',20,0.42,False,nex)
specs['nereid']={"out":R+"out/nereid.png","size":[23,10,10],"border":B,"recess":0.5,"body":{"color":[0.22,0.48,0.44],"rough":0.65,"bump":["leather",36,0.45]},
 "zones":[{"type":"cloth","z":[3.75,9.25],"color":[0.2,0.13,0.08],"weave":110,"img":R+'nereid_cloth.png',"imgscale":0.9,"piping":[0.85,0.8,0.68],
   "logos":[{"img":logo_img('nereid_logo.png','NEREID','cinzel',170,(40,30,20,255),plate=(225,215,190,255),spacing=24),"x":0,"z":8.2,"w":4.2,"h":1.1,"metal":0.0,"rough":0.4,"emis":0.1}]},
  {"type":"panel","z":[0.75,3.75],"art":art,"color":[0.4,0.25,0.14],"metal":1.0,"rough":0.32,"bump":["brush",60,0.03],"artemis":0.1,
   "knobs":KN(xs,KZ,0.42,[.5,.6,.45,.3,.4,.35,.55],'cream'),"toggles":[[7.3,KZ],[8.4,KZ]],
   "jewels":[{"x":9.5,"z":KZ,"color":[0.4,1.0,0.8],"r":0.28}]}],
 "rimcolor":[0.7,1,0.9]}

# ===== SELENE (clean: ivory + walnut + brushed alu) =====
W,B=22,0.7; Kd=(30,30,32,255)
xs=[-6.0,-4.0,-2.0,0,2.0,4.0]; KZ=1.85
def sex(pn):
    cx,cy=pn.P(-9.0,KZ); pn.d.ellipse((cx-60,cy-60,cx+60,cy+60),fill=Kd); pn.d.ellipse((cx-35,cy-72,cx+85,cy+48),fill=(0,0,0,0))
    label(pn,6.9,KZ-0.72,'POWER',Kd,'mich',14,2)
art=std_panel('selene_panel.png',W,B,0.7,3.5,xs,['VOLUME','BASS','MID','TREBLE','BLOOM','MASTER'],KZ,Kd,'mich',16,0.38,False,sex)
specs['selene']={"out":R+"out/selene.png","size":[22,9.5,9],"border":B,"recess":0.45,"bevel":0.4,"body":{"color":[0.8,0.78,0.72],"rough":0.55,"bump":["leather",20,0.25]},
 "zones":[{"type":"wood","z":[3.5,8.3],"planks":3,"c1":[0.06,0.03,0.015],"c2":[0.38,0.2,0.09],"coat":0.6,"rough":0.4},
  {"type":"panel","z":[0.7,3.5],"art":art,"color":[0.78,0.78,0.8],"metal":1.0,"rough":0.25,"aniso":0.6,"bump":["brush",60,0.03],"artemis":0,
   "knobs":KN(xs,KZ,0.38,[.5,.45,.55,.6,.3,.5],'alu'),"toggles":[],
   "jewels":[{"x":8.2,"z":KZ,"color":[1.0,0.85,0.6],"r":0.26,"strength":15,"button":True}]}],
 "rimcolor":[1,0.9,0.8]}

for k,v in specs.items(): json.dump(v,open(R+k+'.json','w'))
print('ok',list(specs))
