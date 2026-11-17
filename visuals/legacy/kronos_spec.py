from amp_helpers import *
exec(open(R+'amps2_spec.py').read().split('# ===== PLVTO v2 =====')[0])
specs={}
Wt=(232,232,235,255); Gy=(150,150,155,255); Rd=(215,40,40,255)
EQF=['80','240','750','2200','6600']
def eq_block(pn,x0,KZ,sp=0.62,h=1.0):
    a=pn.P(x0-0.45,KZ+h+0.45); b=pn.P(x0+4*sp+0.45,KZ-h-0.55)
    pn.d.rounded_rectangle((a[0],a[1],b[0],b[1]),14,outline=Gy,width=3)
    label(pn,x0+2*sp,KZ+h+0.22,'GRAPHIC EQ',Wt,'mich',15,6)
    for i,f in enumerate(EQF):
        label(pn,x0+i*sp,KZ-h-0.25,f,Wt,'mich',13,0)
        for t in range(5):
            y=pn.P(0,KZ-h+t*(2*h/4))[1]; x=pn.P(x0+i*sp,0)[0]; pn.d.line([(x-22,y),(x-12,y)],fill=Gy,width=2)
    for t,lab in ((0,'+12'),(2,'0'),(4,'-12')):
        y=KZ+h-t*(2*h/4); label(pn,x0-0.32,y,lab,Gy,'mich',11,0)
def knob_marks(pn,x,KZ,r,lab,col=Wt,fs=15):
    ticks(pn,x,KZ,int(r*PXU)+12,int(r*PXU)+26,11,Gy,3); numbers(pn,x,KZ,r,Gy,'mich',13,rr=int(r*PXU)+46)
    label(pn,x,KZ+r+0.58,lab,col,'mich',fs,4)
def sliders(x0,KZ,vals,sp=0.62,h=1.0): return [{"x":x0+i*sp,"z0":KZ-h,"z1":KZ+h,"v":v,"color":[0.75,0.75,0.78],"metal":1.0} for i,v in enumerate(vals)]

# ---------- KRONOS IIC ----------
W,B=22.0,0.85; za,zb=B,3.75; KZ=2.15
pn=Panel(W,B,za,zb)
xs=[-9.1+i*1.52 for i in range(8)]
names=['VOLUME','TREBLE','BASS','MIDDLE','MASTER','DRIVE','LEAD','PRESENCE']
for x,n in zip(xs,names): knob_marks(pn,x,KZ,0.36,n,Rd if n in ('DRIVE','LEAD') else Wt)
for x in (xs[0],xs[1],xs[2]): label(pn,x,KZ-0.95,'PULL','mich',11,Gy) if False else label(pn,x,KZ-0.95,'PULL',Gy,'mich',11,3)
eq_block(pn,2.9,KZ+0.1,0.62,0.95)
label(pn,6.55,KZ+1.15,'EQ',Wt,'mich',14,3); label(pn,6.55,KZ-0.75,'IN  OUT',Gy,'mich',11,2)
label(pn,7.6,KZ-0.75,'STBY',Wt,'mich',13,2); label(pn,8.65,KZ-0.75,'POWER',Wt,'mich',13,2)
art=pn.save('kronos2_panel.png')
cane=canvas(256,256); cd=ImageDraw.Draw(cane); cd.rectangle((0,0,256,256),fill=(196,158,96,255))
for k in range(-256,512,32):
    cd.line([(k,0),(k+256,256)],fill=(160,120,65,255),width=9); cd.line([(k,256),(k+256,0)],fill=(160,120,65,255),width=9)
    cd.line([(k,0),(k,256)],fill=(215,180,120,255),width=5)
for x in range(16,256,32):
    for y in range(16,256,32): cd.regular_polygon((x,y,9),8,fill=(30,20,10,255))
cane.save(R+'cane2.png')
lgo=canvas(1000,360); ld=ImageDraw.Draw(lgo); ld.rounded_rectangle((0,0,1000,360),40,fill=(14,12,10,255),outline=(200,200,205,255),width=10)
text(ld,(500,150),'Kronos','cdec',170,(235,235,238,255)); text(ld,(500,285),'MARK  II C+','mich',40,(215,40,40,255),spacing=14)
lgo.save(R+'kronos2_logo.png')
specs['kronos2']={"out":R+"out/kronos2.png","size":[W,10,8.6],"border":B,"recess":0.5,"bevel":0.35,
 "body":{"wood":True,"c1":[0.09,0.045,0.018],"c2":[0.42,0.22,0.09],"coat":0.25,"rough":0.45},
 "zones":[{"type":"cloth","z":[zb,8.6-B],"color":[0.6,0.45,0.25],"weave":60,"img":R+'cane2.png',"imgscale":2.4,"sheen":0.05,
   "logos":[{"img":R+'kronos2_logo.png',"x":6.4,"z":4.75,"w":3.4,"h":1.22,"metal":0,"rough":0.35,"emis":0.15}]},
  {"type":"panel","z":[za,zb],"art":art,"color":[0.012,0.012,0.013],"rough":0.45,"metal":0.2,"artemis":0.3,
   "knobs":[kn(x,KZ,0.36,v,'black') for x,v in zip(xs,[.6,.7,.3,.4,.55,.8,.5,.6])],
   "sliders":sliders(2.9,KZ+0.1,[.8,.55,.25,.55,.75],0.62,0.95),"toggles":[[6.55,KZ+0.3],[7.6,KZ]],
   "jewels":[{"x":8.65,"z":KZ,"color":[1,0.1,0.06],"r":0.3}]}],
 "rimcolor":[1,0.85,0.65]}

# ---------- KRONOS IV ----------
W,B=24.0,0.8; za,zb=B,5.15; K1,K2=4.05,1.95
pn=Panel(W,B,za,zb)
r1=[-10.0+i*1.55 for i in range(8)]; n1=['VOLUME','TREBLE','BASS','MID','GAIN','R2 TREBLE','R2 BASS','LEAD DRIVE']
r2=[-10.0+i*1.55 for i in range(7)]; n2=['LEAD TREBLE','LEAD BASS','LEAD MASTER','R1 MASTER','PRESENCE','REVERB','OUTPUT']
for x,n in zip(r1,n1): knob_marks(pn,x,K1-0.15,0.34,n,Rd if 'LEAD' in n else Wt,13)
for x,n in zip(r2,n2): knob_marks(pn,x,K2-0.15,0.34,n,Rd if 'LEAD' in n else Wt,13)
eq_block(pn,3.6,3.05,0.62,1.1)
tx=[7.6,8.4,9.2,10.0]; tl=['FAT','BRIGHT','TIGHT','EQ']
for x,l in zip(tx,tl): label(pn,x,4.55,l,Gy,'mich',11,2)
label(pn,8.4,1.25,'STBY',Wt,'mich',13,2); label(pn,9.6,1.25,'POWER',Wt,'mich',13,2)
y=pn.P(0,3.0)[1]; pn.d.line([(pn.P(-10.8,0)[0],y),(pn.P(1.6,0)[0],y)],fill=(60,60,64,255),width=2)
art=pn.save('kronos4_panel.png')
l4=canvas(1100,360); ld=ImageDraw.Draw(l4); ld.rounded_rectangle((0,0,1100,360),30,fill=(200,200,205,255))
ld.rounded_rectangle((18,18,1082,342),24,fill=(12,12,13,255)); text(ld,(550,150),'Kronos','cdec',160,(235,235,238,255)); text(ld,(550,280),'MARK  IV','mich',44,(215,40,40,255),spacing=18)
l4.save(R+'kronos4_logo.png')
specs['kronos4']={"out":R+"out/kronos4.png","size":[W,10.5,10.4],"border":B,"recess":0.5,"corners":"chrome",
 "body":{"color":[0.02,0.02,0.022],"rough":0.75,"bump":["leather",42,0.6]},
 "zones":[{"type":"cloth","z":[zb,10.4-B],"color":[0.03,0.03,0.032],"weave":55,"sheen":0.35,"piping":[0.8,0.8,0.82],
   "logos":[{"img":R+'kronos4_logo.png',"x":7.6,"z":6.1,"w":3.6,"h":1.18,"metal":0.9,"rough":0.25,"emis":0.1}]},
  {"type":"panel","z":[za,zb],"art":art,"color":[0.012,0.012,0.013],"rough":0.45,"metal":0.2,"artemis":0.3,
   "knobs":[kn(x,K1-0.15,0.34,v,'black') for x,v in zip(r1,[.5,.6,.35,.45,.7,.55,.4,.8])]+[kn(x,K2-0.15,0.34,v,'black') for x,v in zip(r2,[.6,.4,.5,.55,.6,.3,.5])],
   "sliders":sliders(3.6,3.05,[.78,.5,.28,.52,.72],0.62,1.1),"toggles":[[x,4.05] for x in tx]+[[8.4,1.95]],
   "jewels":[{"x":9.6,"z":1.95,"color":[1,0.1,0.06],"r":0.3}]}],
 "rimcolor":[0.9,0.9,1]}
for k,v in specs.items(): json.dump(v,open(R+k+'.json','w'))
print('ok')
