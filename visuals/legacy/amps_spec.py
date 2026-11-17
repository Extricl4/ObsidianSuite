from art_common import *
import math, json, random
random.seed(5)
R='/home/claude/r3d/'
specs={}
PXU=200
class Panel:
    def __init__(s,W,B,za,zb):
        s.IW=W-2*B; s.w=s.IW-0.1; s.za,s.zb=za,zb
        s.im=canvas(int(s.w*PXU),int((zb-za)*PXU)); s.d=ImageDraw.Draw(s.im)
    def P(s,x,z): return ((x+s.w/2)*PXU,(s.zb-z)*PXU)
    def save(s,n): s.im.save(R+n); return R+n
def numbers(pn,x,z,r,col,k='cinzel',sz=20,n=11,rr=None):
    cx,cy=pn.P(x,z); rr=rr or r*PXU+60
    for i in range(n):
        a=math.radians(135+270*i/(n-1)); text(pn.d,(cx+rr*math.cos(a),cy+rr*math.sin(a)),str(i),k,sz,col)
def ticks(pn,x,z,r0,r1,n,col,w=3):
    cx,cy=pn.P(x,z)
    for i in range(n):
        a=math.radians(135+270*i/(n-1)); pn.d.line([(cx+r0*math.cos(a),cy+r0*math.sin(a)),(cx+r1*math.cos(a),cy+r1*math.sin(a))],fill=col,width=w)
def label(pn,x,z,t,col,k='cinzel',sz=34,sp=5): text(pn.d,pn.P(x,z),t,k,sz,col,spacing=sp)
def kn(x,z,r,v,st='black'): return {"x":x,"z":z,"r":r,"v":v,"style":st}

# ===== PLVTO (Satan-inspired) =====
W,B=24.0,0.7; pn=Panel(W,B,0.7,4.25); C=(200,196,190,255); Rd=(170,20,20,255)
xs=[-6.5,-3.9,-1.3,1.3,3.9,6.5]; KZ=2.35
for x,n,v in zip(xs,['GAIN','BASS','MID','TREBLE','PRESENCE','DEPTH'],[.8,.55,.45,.6,.5,.65]):
    ticks(pn,x,KZ,118,140,11,C,3); numbers(pn,x,KZ,0.5,C,'cinzel',22,rr=172); label(pn,x,KZ+1.25,n,C,'cinzel',32,6)
cx,cy=pn.P(0,0.95); pn.d.line([(pn.P(-8,0)[0],cy),(pn.P(8,0)[0],cy)],fill=Rd,width=4)
cx,cy=pn.P(0,3.95); pn.d.line([(pn.P(-8,0)[0],cy),(pn.P(8,0)[0],cy)],fill=(90,90,90,255),width=2)
label(pn,9.4,1.45,'POWER',C,'cinzel',22,4); label(pn,8.4,1.45,'STBY',C,'cinzel',22,4)
specs['plvto']={"out":R+"out/plvto.png","size":[24,10,10.5],"border":B,"recess":1.6,
 "zones":[{"type":"louver","z":[4.25,9.8],"angle":52,"pitch":0.34,"mullions":[0.25,0.5,0.75],
   "tubes":[{"x":x,"h":2.6,"r":0.42,"color":[1,0.16,0.04],"strength":7,"light":45} for x in (-8.1,-5.4,-2.7,0,2.7,5.4,8.1)]},
  {"type":"panel","z":[0.7,4.25],"art":pn.save('plvto_panel.png'),"color":[0.014,0.014,0.016],"rough":0.75,
   "knobs":[kn(x,KZ,0.5,v) for x,v in zip(xs,[.8,.55,.45,.6,.5,.65])],"toggles":[[8.4,2.35]],
   "jewels":[{"x":9.4,"z":2.35,"color":[1,0.08,0.04],"r":0.3,"strength":25,"button":True}]}],
 "rimcolor":[1,0.45,0.35]}
pn2=Panel(24,0.8,0.8,23.2)
specs['plvto_cab']={"out":R+"out/plvto_cab.png","size":[24,13,24],"border":0.8,"recess":1.4,"res":[1000,1000],"samples":32,
 "zones":[{"type":"louver","z":[0.8,23.2],"angle":52,"pitch":0.38,"mullions":[0.5],"speakers":[[-5.6,6.3,4.8],[5.6,6.3,4.8],[-5.6,17.4,4.8],[5.6,17.4,4.8]],
   "ambient":[{"x":0,"energy":90,"color":[1,0.2,0.1],"size":14}]}],
 "cam":[-7.0,-52,25],"target":[0.3,0,11.8],"kscale":1.9,"rimcolor":[1,0.45,0.35]}

# ===== TARTARUS (5150 / Invective-inspired) =====
W,B=26.0,0.8; pn=Panel(W,B,0.8,5.15); C=(225,225,228,255); Rd=(220,30,30,255)
KZ=2.6; rx=[-9.3,-7.9,-6.5,-5.1]; lx=[-2.6,-1.2,0.2,1.6]; mx=[4.1,5.5,6.9]
for x,n in zip(rx,['PRE','LOW','MID','HIGH']): ticks(pn,x,KZ,92,110,11,C,3); label(pn,x,KZ+0.85,n,C,'mich',22,3)
for x,n in zip(lx,['PRE','LOW','MID','HIGH']): ticks(pn,x,KZ,92,110,11,Rd,3); label(pn,x,KZ+0.85,n,Rd,'mich',22,3)
for x,n in zip(mx,['POST','RESO','PRES']): ticks(pn,x,KZ,92,110,11,C,3); label(pn,x,KZ+0.85,n,C,'mich',22,3)
label(pn,-7.2,4.55,'RHYTHM',C,'mich',26,10); label(pn,-0.5,4.55,'LEAD',Rd,'mich',26,10); label(pn,5.5,4.55,'MASTER',C,'mich',26,10)
for a,b,col in ((-9.6,-4.8,C),(-2.9,1.9,Rd),(3.8,7.2,C)):
    y=pn.P(0,4.25)[1]; pn.d.line([(pn.P(a,0)[0],y),(pn.P(b,0)[0],y)],fill=col,width=2)
for x,n in ((-10.9,'CRUNCH'),(8.6,'CHANNEL'),(9.6,'STBY'),(10.5,'POWER')): label(pn,x,KZ-0.75,n,C,'mich',16,2)
y=pn.P(0,1.15)[1]; pn.d.line([(pn.P(-11.5,0)[0],y),(pn.P(11.5,0)[0],y)],fill=Rd,width=5)
logo=canvas(1300,340); ld=ImageDraw.Draw(logo)
text(ld,(650,165),'Tartarus','frak',250,(225,25,30,255))
logo.save(R+'tartarus_logo.png')
specs['tartarus']={"out":R+"out/tartarus.png","size":[26,11,11],"border":B,"recess":0.5,"corners":"chrome",
 "zones":[{"type":"cloth","z":[5.15,10.2],"piping":[0.62,0.62,0.64],"weave":70,
   "logos":[{"img":R+'tartarus_logo.png',"x":0,"z":7.7,"w":6.2,"h":1.62,"plate":[0.03,0.03,0.032],"metal":0.0,"rough":0.35,"emis":0.4}]},
  {"type":"panel","z":[0.8,5.15],"art":pn.save('tartarus_panel.png'),"color":[0.09,0.09,0.095],"metal":0.9,"rough":0.35,"bump":["brush",60,0.04],"artemis":0.15,
   "knobs":[kn(x,KZ,0.38,v) for x,v in zip(rx+lx+mx,[.4,.6,.5,.6,.9,.55,.4,.7,.5,.6,.55])],
   "toggles":[[-10.9,KZ],[8.6,KZ],[9.6,KZ],[10.5,KZ]],"jewels":[{"x":11.4,"z":KZ,"color":[1,0.1,0.06],"r":0.3}]}],
 "rimcolor":[1,0.5,0.45]}
specs['tartarus_cab']={"out":R+"out/tartarus_cab.png","size":[24,13,24],"border":0.8,"recess":0.5,"corners":"chrome","res":[1000,1000],"samples":32,
 "zones":[{"type":"cloth","z":[0.8,23.2],"piping":[0.62,0.62,0.64],"weave":70,
   "logos":[{"img":R+'tartarus_logo.png',"x":0,"z":20.6,"w":6.2,"h":1.62,"plate":[0.03,0.03,0.032],"metal":0.0,"rough":0.35,"emis":0.4}]}],
 "cam":[-7.0,-52,25],"target":[0.3,0,11.8],"kscale":1.9,"rimcolor":[1,0.5,0.45]}

# ===== NYX (perforated sheet, ambient tubes) =====
def slot_mask(n,wu,hu,slot=(240,44),gap=(56,76),margin=50,sc=100):
    w,h=int(wu*sc),int(hu*sc); im=Image.new('RGBA',(w,h),(0,0,0,0)); d=ImageDraw.Draw(im)
    sw,sh=slot; row=0; y=margin
    while y+sh<h-margin:
        x=margin+(0 if row%2==0 else (sw+gap[0])//2)
        while x+sw<w-margin:
            d.rounded_rectangle((x,y,x+sw,y+sh),sh//2,fill=(255,255,255,255)); x+=sw+gap[0]
        y+=gap[1]; row+=1
    im.save(R+n); return R+n
def hex_mask(n,wu,hu,r=55,margin=50,sc=100):
    w,h=int(wu*sc),int(hu*sc); im=Image.new('RGBA',(w,h),(0,0,0,0)); d=ImageDraw.Draw(im)
    dx=r*1.85; dy=r*1.6; row=0; y=margin+r
    while y<h-margin-r:
        x=margin+r+(dx/2 if row%2 else 0)
        while x<w-margin-r:
            d.regular_polygon((x,y,r*0.86),6,fill=(255,255,255,255)); x+=dx
        y+=dy; row+=1
    im.save(R+n); return R+n
W,B=24.0,0.5; IW=W-2*B
cols=[[0.15,0.35,1.0],[1.0,0.22,0.6],[0.6,0.22,1.0]]
pn=Panel(W,B,0.5,3.4); C=(215,215,225,255); Pu=(170,120,255,255)
xs=[-8.1,-5.7,-3.3,-0.9,1.5,3.9,6.3]; KZ=1.75
for x,n in zip(xs,['GAIN','BASS','MID','TREBLE','PRESENCE','DEPTH','VOLUME']): ticks(pn,x,KZ,92,106,11,C,3); label(pn,x,KZ+0.8,n,C,'mich',20,4)
label(pn,-10.3,KZ,'NYX','mich',44,Pu,) if False else label(pn,-10.4,KZ,'NYX',Pu,'mich',44,12)
label(pn,8.6,KZ-0.72,'MODE','mich',15,C) if False else label(pn,8.6,KZ-0.72,'MODE',C,'mich',15,2)
specs['nyx']={"out":R+"out/nyx.png","size":[24,10,10],"border":B,"recess":1.6,"body":{"color":[0.02,0.02,0.024],"metal":0.6,"rough":0.42,"bump":["noise",200,0.05]},"bevel":0.2,
 "zones":[{"type":"perf","z":[3.45,9.5],"mask":slot_mask('nyx_mask.png',IW-0.04,9.5-3.45),"color":[0.018,0.018,0.022],"metal":0.75,"rough":0.38,
   "tubes":[{"x":x,"h":2.4,"r":0.38,"color":cols[i%3],"strength":10,"light":70} for i,x in enumerate((-8.4,-5.6,-2.8,0,2.8,5.6,8.4))],
   "ambient":[{"x":-6,"energy":60,"color":cols[0],"size":6},{"x":0,"energy":60,"color":cols[2],"size":6},{"x":6,"energy":60,"color":cols[1],"size":6}]},
  {"type":"panel","z":[0.5,3.45],"art":pn.save('nyx_panel.png'),"color":[0.02,0.02,0.024],"metal":0.7,"rough":0.4,"bump":["brush",60,0.03],
   "knobs":[kn(x,KZ,0.38,v,'alu') for x,v in zip(xs,[.7,.5,.6,.55,.45,.6,.4])],"toggles":[[8.6,KZ]],
   "jewels":[{"x":10.2,"z":KZ,"color":[0.65,0.3,1.0],"r":0.28}]}],
 "rimcolor":[0.75,0.5,1.0],"world":[0.01,0.008,0.016]}
specs['nyx_cab']={"out":R+"out/nyx_cab.png","size":[24,13,24],"border":0.5,"recess":1.4,"res":[1000,1000],"samples":32,"body":{"color":[0.02,0.02,0.024],"metal":0.6,"rough":0.42,"bump":["noise",200,0.05]},"bevel":0.2,
 "zones":[{"type":"perf","z":[0.5,23.5],"mask":hex_mask('nyx_cab_mask.png',23-0.04,23),"color":[0.018,0.018,0.022],"metal":0.75,"rough":0.38,
   "speakers":[[-5.6,6.3,4.8],[5.6,6.3,4.8],[-5.6,17.6,4.8],[5.6,17.6,4.8]],
   "ambient":[{"x":-6,"energy":120,"color":cols[0],"size":8},{"x":0,"energy":90,"color":cols[2],"size":8},{"x":6,"energy":120,"color":cols[1],"size":8}]}],
 "cam":[-7.0,-52,25],"target":[0.3,0,11.8],"kscale":1.9,"rimcolor":[0.75,0.5,1.0],"world":[0.01,0.008,0.016]}

# ===== EREBUS (Omega Granophyre-inspired) =====
W,B=23.0,0.6; pn=Panel(W,B,0.6,8.4); C=(150,150,152,255); Cb=(195,195,198,255)
xs=[-7.2,-4.8,-2.4,0,2.4,4.8,7.2]; KZ=2.0
for x,n in zip(xs,['GAIN','BASS','MID','TREBLE','PRESENCE','DEPTH','MASTER']):
    ticks(pn,x,KZ,96,112,11,Cb,3); label(pn,x,KZ-0.95,n,Cb,'mich',20,5)
# engraved traces
for i in range(9):
    y=4.0+i*0.12; a=pn.P(-10.3,y); b=pn.P(-5.2+i*0.15,y); pn.d.line([a,b],fill=(60,60,62,255),width=2)
    a2=pn.P(10.3,y); b2=pn.P(5.2-i*0.15,y); pn.d.line([a2,b2],fill=(60,60,62,255),width=2)
for x in xs:
    pn.d.line([pn.P(x,KZ+0.62),pn.P(x,3.5)],fill=(60,60,62,255),width=2)
pn.d.line([pn.P(-7.2,3.5),pn.P(7.2,3.5)],fill=(60,60,62,255),width=2)
label(pn,9.6,KZ-0.95,'POWER',Cb,'mich',16,3)
lg=canvas(1600,480); ld=ImageDraw.Draw(lg); S_=(205,205,210,255)
cx,cy=230,240
pts=[(cx+150*math.cos(math.radians(a)),cy+150*math.sin(math.radians(a))) for a in range(-90,270,60)]
ld.polygon(pts,outline=S_,width=10)
for a in range(-90,270,60):
    ld.line([(cx,cy),(cx+150*math.cos(math.radians(a)),cy+150*math.sin(math.radians(a)))],fill=S_,width=5)
ld.polygon([(cx,cy-80),(cx+70,cy+40),(cx-70,cy+40)],outline=S_,width=6)
text(ld,(960,215),'EREBUS','mich',150,S_,spacing=30)
ld.line([(470,330),(1450,330)],fill=S_,width=3)
text(ld,(960,385),'GRANITE  HIGH GAIN','mich',34,(150,150,155,255),spacing=14)
lg.save(R+'erebus_logo.png')
wires=[{"color":[0.45,0.02,0.02],"r":0.06,"pts":[[-10.0,4.05,0],[-7.5,4.6,0.35],[-4.2,3.95,0.15],[-2.4,2.9,0.3],[-1.2,2.75,0.1]]},
       {"color":[0.65,0.6,0.5],"r":0.055,"pts":[[10.0,4.2,0],[7.6,4.75,0.35],[4.0,4.0,0.2],[2.4,2.95,0.28],[1.2,2.8,0.1]]},
       {"color":[0.03,0.03,0.03],"r":0.06,"pts":[[-10.0,4.5,0],[-6,5.05,0.4],[0,4.35,0.25],[6,5.05,0.4],[10.0,4.5,0]]}]
specs['erebus']={"out":R+"out/erebus.png","size":[23,9.5,9.0],"border":B,"recess":0.5,"body":{"color":[0.03,0.03,0.032],"rough":0.85,"bump":["leather",30,0.45]},
 "zones":[{"type":"plate","z":[0.6,8.4],"art":pn.save('erebus_panel.png'),"color":[0.012,0.012,0.013],"rough":0.95,"bump":0,"artemis":0.12,
   "knobs":[kn(x,KZ,0.42,v,'alu') for x,v in zip(xs,[.75,.55,.45,.6,.5,.6,.35])],
   "posts":[[-10.0,4.05],[10.0,4.2],[-10.0,4.5],[10.0,4.5],[-1.2,2.75],[1.2,2.8]],"wires":wires,
   "jewels":[{"x":9.6,"z":KZ,"color":[0.9,0.92,1.0],"r":0.28,"strength":12,"button":True}],
   "logos":[{"img":R+'erebus_logo.png',"x":0,"z":6.55,"w":8.0,"h":2.4,"metal":0.95,"rough":0.3,"emis":0.08}]}],
 "rimcolor":[0.8,0.85,1.0]}
specs['erebus_cab']={"out":R+"out/erebus_cab.png","size":[23,13,24],"border":0.6,"recess":0.5,"res":[1000,1000],"samples":32,"body":{"color":[0.03,0.03,0.032],"rough":0.85,"bump":["leather",30,0.45]},
 "zones":[{"type":"cloth","z":[3.0,23.4],"color":[0.02,0.02,0.021],"weave":90},
  {"type":"plate","z":[0.6,3.0],"color":[0.012,0.012,0.013],"rough":0.95,"bump":0,
   "logos":[{"img":R+'erebus_logo.png',"x":0,"z":1.8,"w":5.0,"h":1.5,"metal":0.95,"rough":0.3,"emis":0.08}]}],
 "cam":[-7.0,-52,25],"target":[0.3,0,11.8],"kscale":1.9}

for k,v in specs.items(): json.dump(v,open(R+k+'.json','w'))
print('ok',list(specs))
