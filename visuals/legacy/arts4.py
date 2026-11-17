from art_common import *
import math, json, random
from PIL import ImageFilter
random.seed(9)
R='/home/claude/r3d/'
GW,GH=4000,2400
SX0,SY0,SX1,SY1=1150,170,2850,1090
def big_unit(key,A,name,sub,knobs,btnL,btnR,fs,screen,enc,kstyle,ledcols,rim):
    Wg=(225,222,218,255)
    im=canvas(GW,GH); d=ImageDraw.Draw(im); frame(d,(A[0],A[1],A[2],140),inset=46,WW=GW,HH=GH)
    d.rounded_rectangle((SX0-40,SY0-40,SX1+40,SY1+40),30,outline=Wg,width=4)
    text(d,(650,240),name,'mich',64,Wg,spacing=16); text(d,(650,320),sub,'mich',20,(160,155,150,255),spacing=8)
    text(d,(650,760),'SELECT  /  PUSH','mich',22,Wg,spacing=6)
    for (x,y),n in zip(((480,920),(820,920)),btnL): text(d,(x,y+115),n,'mich',22,Wg,spacing=5)
    for (x,y),n in zip(((3180,420),(3520,420),(3180,800),(3520,800)),btnR): text(d,(x,y+115),n,'mich',22,Wg,spacing=5)
    kx=[500+i*428 for i in range(8)]
    for x,n in zip(kx,knobs): text(d,(x,1490),n,'mich',24,Wg,spacing=5)
    d.line([(120,1590),(3880,1590)],fill=(A[0],A[1],A[2],160),width=3)
    fx=[500+i*750 for i in range(5)]
    for x,n in zip(fx,fs): text(d,(x,2200),n,'mich',28,A if n not in ('BYPASS',) else Wg,spacing=6)
    im.save(R+f'{key}_art.png')
    g=canvas(GW,GH); gd=ImageDraw.Draw(g); gd.rectangle((SX0,SY0,SX1,SY1),fill=(6,7,10,255)); screen(gd,A)
    g.save(R+f'{key}_glow.png')
    return {"out":R+f"out/{key}.png","size":[20.0,12.0,3.0],"art":R+f"{key}_art.png","glow":R+f"{key}_glow.png","glow_strength":2.2,"artmat":{"rough":0.35,"metal":0.0},
     "enclosure":enc,
     "knobs":[{"at":[650,540],"r":0.78,"style":"alu","v":0.5}]+[{"at":[x,1300],"r":0.52,"style":kstyle,"v":random.uniform(.25,.8)} for x in kx],
     "buttons":[{"at":[x,y],"w":0.95,"h":0.62,"color":[0.05,0.05,0.055]} for (x,y) in ((480,920),(820,920),(3180,420),(3520,420),(3180,800),(3520,800))],
     "leds":[{"at":[x,1730],"color":ledcols[i],"strength":38 if i in (0,2,4) else 0.4} for i,x in enumerate(fx)],
     "footswitches":[[x,1960,1.0] for x in fx],
     "screws":[[110,110],[3890,110],[110,2290],[3890,2290]],"res":[1170,780],"lens":34,"cam":[0,-14.2,30.5],"target":[0,-0.7,1.5],"rimcolor":rim}

def F(s): return font('mich',s)
# ---------- SHATTER screen ----------
def shatter_screen(d,A):
    W_=(235,235,240,255); G_=(120,120,130,255); Rr=(255,70,70,255); Bl=(90,170,255,255)
    d.text((SX0+40,SY0+30),'PRESET 07   STUTTER CASCADE',font=F(40),fill=W_)
    d.text((SX1-40,SY0+30),'120 BPM   1/16',font=F(36),fill=G_,anchor='ra')
    d.line([(SX0+30,SY0+100),(SX1-30,SY0+100)],fill=(50,50,60,255),width=3)
    x0,x1,yc=SX0+60,SX1-60,SY0+330; n=16; sw=(x1-x0)/n
    for i in range(n):
        xa=x0+i*sw
        if i in (4,5,6,7): d.rectangle((xa+3,yc-180,xa+sw-3,yc+180),fill=(90,20,25,255))
        if i in (10,11): d.rectangle((xa+3,yc-180,xa+sw-3,yc+180),fill=(20,45,85,255))
        if i==13: d.rectangle((xa+3,yc-180,xa+sw-3,yc+180),fill=(60,60,70,255))
        d.line([(xa,yc-185),(xa,yc+185)],fill=(55,55,65,255),width=2)
    pts=[]
    for k in range(int(x1-x0)):
        t=k/(x1-x0); env=0.25+0.75*abs(math.sin(t*math.pi*4))
        pts.append((x0+k,yc+env*160*math.sin(k*0.37)*random.uniform(0.4,1)))
    d.line(pts,fill=W_,width=2)
    for i,lab,c in ((5.5,'STUTTER x4',Rr),(10.5,'REVERSE',Bl),(13,'FREEZE',W_)):
        d.text((x0+i*sw+sw/2,yc+215),lab,font=F(26),fill=c,anchor='mm')
    names=['RATE','LENGTH','CHAOS','PITCH','REVERSE','CRUSH','FILTER','MIX']; vals=[.5,.35,.72,.5,.2,.4,.6,.55]
    by=SY1-150; bw=(SX1-SX0-120)/8
    for i,(n_,v) in enumerate(zip(names,vals)):
        bx=SX0+60+i*bw
        d.text((bx+bw/2,by),n_,font=F(22),fill=G_,anchor='mm')
        d.rectangle((bx+20,by+30,bx+bw-20,by+50),fill=(40,40,48,255)); d.rectangle((bx+20,by+30,bx+20+(bw-40)*v,by+50),fill=Rr if i==2 else W_)
        d.text((bx+bw/2,by+85),('%d%%'%(v*100)) if i not in (3,) else '+7 ST',font=F(24),fill=W_,anchor='mm')
# ---------- LEGION screen ----------
def legion_screen(d,A):
    W_=(240,236,228,255); G_=(140,130,115,255); Am=(255,180,80,255)
    d.text((SX0+40,SY0+30),'PRESET 03   HARMONY WALL',font=F(40),fill=W_)
    d.text((SX1-40,SY0+30),'E MINOR   AUTO FOLLOW',font=F(34),fill=Am,anchor='ra')
    d.line([(SX0+30,SY0+100),(SX1-30,SY0+100)],fill=(60,55,50,255),width=3)
    d.text((SX0+60,SY0+150),'INPUT',font=F(24),fill=G_); d.text((SX0+60,SY0+185),'E4',font=F(80),fill=W_)
    lanes=[('VOICE 1','-OCT','E3',.7,-.4),('VOICE 2','+3RD','G4',.85,-.15),('VOICE 3','+5TH','B4',.75,.15),('VOICE 4','+OCT','E5',.5,.4)]
    for i,(vn,iv,nt,lv,pn_) in enumerate(lanes):
        y=SY0+150+i*120; x=SX0+380
        d.text((x,y+10),vn,font=F(24),fill=G_)
        d.text((x+200,y),iv,font=F(44),fill=Am)
        d.text((x+420,y+5),nt,font=F(38),fill=W_)
        d.rectangle((x+560,y+18,x+1060,y+42),fill=(45,40,35,255)); d.rectangle((x+560,y+18,x+560+500*lv,y+42),fill=W_)
        cx=x+1180; d.line([(cx-80,y+30),(cx+80,y+30)],fill=(80,70,60,255),width=3); d.ellipse((cx+pn_*80-10,y+20,cx+pn_*80+10,y+40),fill=Am)
    ky=SY1-140; kx0=SX0+60; kw=(SX1-SX0-120)/29
    lit={4,7,11,16,19,23}
    for i in range(29):
        d.rectangle((kx0+i*kw+2,ky,kx0+(i+1)*kw-2,ky+110),fill=Am if i in lit else (210,205,195,255))
    for i in range(28):
        if i%7 in (0,1,3,4,5): d.rectangle((kx0+(i+1)*kw-kw*0.3,ky,kx0+(i+1)*kw+kw*0.3,ky+65),fill=(20,18,16,255))

specs={}
specs['shatter']=big_unit('shatter',(230,45,50,255),'SHATTER','GLITCH WORKSTATION',['RATE','LENGTH','CHAOS','PITCH','REVERSE','CRUSH','FILTER','MIX'],
    ['PRESET','EDIT'],['HOLD','RANDOM','SYNC','SAVE'],['STUTTER','REVERSE','FREEZE','TAP','BYPASS'],shatter_screen,
    {"color":[0.014,0.014,0.016],"metal":0.0,"rough":0.62,"bump":["noise",700,0.07]},'black',[[1,.12,.12],[1,.12,.12],[1,.12,.12],[1,.12,.12],[1,1,1]],[1,0.5,0.5])
specs['legion']=big_unit('legion',(222,170,80,255),'LEGION','POLYPHONIC HARMONY ENGINE',['KEY','SCALE','VOICE 1','VOICE 2','VOICE 3','VOICE 4','HUMANIZE','MIX'],
    ['VOICE','MODE'],['FOLLOW','FIXED','CHORD','LEARN'],['VOICES','FOLLOW','HOLD','PRESET','BYPASS'],legion_screen,
    {"color":[0.06,0.058,0.062],"metal":0.85,"rough":0.35,"bump":["brush",500,0.05]},'black',[[1,.65,.2]]*4+[[1,1,1]],[1,0.75,0.4])
for k,v in specs.items(): json.dump(v,open(R+k+'.json','w'))
print('arts4 ok')
