from amp_helpers import *
import sys
exec(open(R+'amps2_spec.py').read().split('# ===== PLVTO v2 =====')[0])
specs={}
random.seed(44)
def newmask(wu,hu,sc=100):
    w,h=int(wu*sc),int(hu*sc); im=Image.new('RGBA',(w,h),(0,0,0,0)); return im,ImageDraw.Draw(im),w,h
CUT=(255,255,255,255); NONE=(0,0,0,0)
RUNES={'f':[[(0,0),(0,1)],[(0,.15),(.6,0)],[(0,.4),(.6,.25)]],'u':[[(0,1),(0,0),(.6,.3),(.6,1)]],'th':[[(0,0),(0,1)],[(0,.3),(.5,.5),(0,.7)]],
 'a':[[(0,0),(0,1)],[(0,0),(.55,.25)],[(0,.3),(.55,.55)]],'r':[[(0,0),(0,1)],[(0,0),(.5,.25),(0,.5),(.5,1)]],'k':[[(.5,0),(0,.3),(.5,.6)]],
 'g':[[(0,0),(.7,1)],[(.7,0),(0,1)]],'w':[[(0,0),(0,1)],[(0,0),(.5,.25),(0,.5)]],'h':[[(0,0),(0,1)],[(.6,0),(.6,1)],[(0,.35),(.6,.6)]],
 'n':[[(.3,0),(.3,1)],[(0,.35),(.6,.6)]],'i':[[(.3,0),(.3,1)]],'ei':[[(.3,0),(.3,1)],[(.3,0),(.6,.2)],[(.3,1),(0,.8)]],
 'z':[[(.3,0),(.3,1)],[(0,0),(.3,.4),(.6,0)]],'s':[[(.5,0),(0,.4),(.6,.6),(.1,1)]],'t':[[(.3,0),(.3,1)],[(0,.3),(.3,0),(.6,.3)]],
 'b':[[(0,0),(0,1)],[(0,0),(.5,.25),(0,.5),(.5,.75),(0,1)]],'e':[[(0,1),(0,0),(.3,.35),(.6,0),(.6,1)]],'m':[[(0,1),(0,0),(.6,.5)],[(.6,1),(.6,0),(0,.5)]],
 'l':[[(0,0),(0,1)],[(0,0),(.5,.3)]],'d':[[(0,0),(0,1),(.8,0),(.8,1),(0,0)]]}
def rune(d,key,x,y,s,w,fill):
    for st in RUNES[key]: d.line([(x+p[0]*s,y+p[1]*s) for p in st],fill=fill,width=w,joint='curve')
def rune_mask(n,wu,hu,word,s=None,w=40):
    im,d,W_,H_=newmask(wu,hu); s=s or int(H_*0.62); s=min(s,int((W_-160)/(len(word)*0.95+0.1))); gap=int(s*0.95); tot=gap*len(word); x=(W_-tot)/2+gap*0.2
    for k in word: rune(d,k,x,(H_-s)/2,s,w,CUT); x+=gap
    im.save(R+n); return R+n
def tree_mask(n,wu,hu):
    im,d,W_,H_=newmask(wu,hu); cx=W_/2; cy=H_*0.55
    def br(x,y,a,L,w,depth):
        if depth==0: return
        x2=x+math.cos(a)*L; y2=y+math.sin(a)*L*0.55; d.line([(x,y),(x2,y2)],fill=CUT,width=max(8,int(w)),joint='curve')
        for da in (-0.42,0.38): br(x2,y2,a+da+random.uniform(-.12,.12),L*0.7,w*0.68,depth-1)
    d.line([(cx,cy-H_*0.15),(cx,cy+H_*0.2)],fill=CUT,width=70)
    for a in (-1.57-1.0,-1.57-0.5,-1.57,-1.57+0.5,-1.57+1.0): br(cx,cy-H_*0.12,a,W_*0.13,36,6)
    for a in (1.57-1.1,1.57-0.45,1.57+0.45,1.57+1.1): br(cx,cy+H_*0.18,a,W_*0.09,30,4)
    d.rectangle((0,0,W_,50),fill=NONE); d.rectangle((0,H_-50,W_,H_),fill=NONE); d.rectangle((0,0,50,H_),fill=NONE); d.rectangle((W_-50,0,W_,H_),fill=NONE)
    im.save(R+n); return R+n
def sunburst_mask(n,wu,hu):
    im,d,W_,H_=newmask(wu,hu); cx,cy=W_/2,H_-60
    for i in range(23):
        a0=math.radians(180+i*180/23+1.6); a1=math.radians(180+(i+1)*180/23-1.6); r0,r1=H_*0.28,min(H_-110,W_/2-80)
        d.polygon([(cx+r0*math.cos(a0),cy+r0*math.sin(a0)),(cx+r1*math.cos(a0),cy+r1*math.sin(a0)),(cx+r1*math.cos(a1),cy+r1*math.sin(a1)),(cx+r0*math.cos(a1),cy+r0*math.sin(a1))],fill=CUT)
    for s in (-1,1):
        for k in range(6):
            x=cx+s*(W_/2-90-k*55); d.rectangle((x-14,80+k*40,x+14,H_-80),fill=CUT)
    im.save(R+n); return R+n
def fan_mask(n,wu,hu):
    im,d,W_,H_=newmask(wu,hu); r=int(H_*0.32); row=0
    for y in range(int(H_*0.42),H_+r,int(r*0.95)):
        for x in range(-r+(r if row%2 else 0),W_+r,2*r):
            for i in range(9):
                a0=math.radians(180+i*20+2); a1=math.radians(180+(i+1)*20-2)
                d.polygon([(x+r*0.25*math.cos(a0),y+r*0.25*math.sin(a0)),(x+r*0.9*math.cos(a0),y+r*0.9*math.sin(a0)),(x+r*0.9*math.cos(a1),y+r*0.9*math.sin(a1)),(x+r*0.25*math.cos(a1),y+r*0.25*math.sin(a1))],fill=CUT)
        row+=1
    d.rectangle((0,0,W_,45),fill=NONE); d.rectangle((0,H_-45,W_,H_),fill=NONE); d.rectangle((0,0,45,H_),fill=NONE); d.rectangle((W_-45,0,W_,H_),fill=NONE)
    im.save(R+n); return R+n
def tentacle_mask(n,wu,hu):
    im,d,W_,H_=newmask(wu,hu)
    for k in range(7):
        x0=120+k*(W_-240)/6; pts=[]
        for t in range(0,101):
            f=t/100; pts.append((x0+90*math.sin(f*6+k)*f, H_-50-f*(H_-120)))
        for i in range(len(pts)-1):
            w=int(70*(1-i/len(pts))+14); d.line([pts[i],pts[i+1]],fill=CUT,width=w)
    for y in (40,):
        d.rectangle((0,0,W_,40),fill=NONE)
    im.save(R+n); return R+n
def sakura_mask(n,wu,hu):
    im,d,W_,H_=newmask(wu,hu)
    def flower(x,y,r):
        for i in range(5):
            a=math.radians(i*72-90); px,py=x+r*0.55*math.cos(a),y+r*0.55*math.sin(a); d.ellipse((px-r*0.45,py-r*0.45,px+r*0.45,py+r*0.45),fill=CUT)
        d.ellipse((x-r*0.18,y-r*0.18,x+r*0.18,y+r*0.18),fill=NONE)
    pts=[(80+t*(W_-160)/60, H_*0.8-math.sin(t/60*3.1)*H_*0.5+random.uniform(-10,10)) for t in range(61)]
    for i,(x,y) in enumerate(pts):
        if i%3==0: flower(x+random.uniform(-90,90),y+random.uniform(-110,110),random.uniform(55,95))
    for k in range(18): flower(random.uniform(100,W_-100),random.uniform(90,H_-90),random.uniform(30,55))
    d.rectangle((0,0,W_,45),fill=NONE); d.rectangle((0,H_-45,W_,H_),fill=NONE)
    im.save(R+n); return R+n
def lancet_mask(n,wu,hu,count=7):
    im,d,W_,H_=newmask(wu,hu); m=60; cw=(W_-2*m)/count
    for i in range(count):
        x0=m+i*cw+22; x1=x0+cw-44; xc=(x0+x1)/2; r=x1-x0; spring=70+r*0.866
        pts=[(x0,H_-60),(x0,spring)]
        for t in range(0,31): a=math.radians(180+60*t/30); pts.append((x1+r*math.cos(a),spring+r*math.sin(a)))
        for t in range(0,31): a=math.radians(300+60*t/30); pts.append((x0+r*math.cos(a),spring+r*math.sin(a)))
        pts+=[(x1,spring),(x1,H_-60)]
        d.polygon(pts,fill=CUT)
        d.line([(xc,spring),(xc,H_-60)],fill=NONE,width=18)
        rr=r*0.17; cy=spring-r*0.3; d.ellipse((xc-rr,cy-rr,xc+rr,cy+rr),outline=NONE,width=12)
        d.line([(x0,spring+20),(x1,spring+20)],fill=NONE,width=12)
    im.save(R+n); return R+n
def rose_mask(n,wu,hu):
    im,d,W_,H_=newmask(wu,hu); cx,cy=W_/2,H_/2; R0=H_/2-60
    d.ellipse((cx-R0,cy-R0,cx+R0,cy+R0),fill=CUT)
    for i in range(12):
        a=math.radians(i*30); d.line([(cx,cy),(cx+R0*math.cos(a),cy+R0*math.sin(a))],fill=NONE,width=22)
    for rr,w in ((R0*0.35,20),(R0*0.7,16)): d.ellipse((cx-rr,cy-rr,cx+rr,cy+rr),outline=NONE,width=w)
    for i in range(12):
        a=math.radians(i*30+15); px,py=cx+R0*0.53*math.cos(a),cy+R0*0.53*math.sin(a); d.ellipse((px-R0*0.12,py-R0*0.12,px+R0*0.12,py+R0*0.12),outline=NONE,width=10)
    for s in (-1,1):
        for k in range(4):
            x=cx+s*(R0+130+k*170); top=110
            pts=[(x-55,H_-70),(x-55,top+70),(x,top),(x+55,top+70),(x+55,H_-70)]
            if 60<x<W_-60: d.polygon(pts,fill=CUT)
    im.save(R+n); return R+n
def crack_mask(n,wu,hu):
    im,d,W_,H_=newmask(wu,hu)
    def crack(x,y,ang,steps,w):
        pts=[(x,y)]
        for k in range(steps):
            ang+=random.uniform(-0.55,0.55); L=random.uniform(14,30); x+=math.cos(ang)*L; y+=math.sin(ang)*L; pts.append((x,y))
            if random.random()<0.07 and w>8: crack(x,y,ang+random.choice((-1,1))*random.uniform(0.6,1.2),int(steps*0.5),w*0.55)
        for k in range(len(pts)-1):
            ww=max(3,int(w*(1-k/len(pts))**0.7)); d.line([pts[k],pts[k+1]],fill=CUT,width=ww)
    for k in range(7): crack(random.uniform(60,W_-60),random.uniform(60,H_-60),random.uniform(0,6.28),random.randint(35,60),random.uniform(22,38))
    for k in range(12): crack(random.uniform(60,W_-60),random.uniform(60,H_-60),random.uniform(0,6.28),random.randint(6,14),random.uniform(3,7))
    d.rectangle((0,0,W_,40),fill=NONE); d.rectangle((0,H_-40,W_,H_),fill=NONE); d.rectangle((0,0,40,H_),fill=NONE); d.rectangle((W_-40,0,W_,H_),fill=NONE)
    im.save(R+n); return R+n
def lava_lines(n,w,h,count=10):
    im=canvas(w,h); d=ImageDraw.Draw(im)
    def crack(x,y,ang,steps,wd):
        pts=[(x,y)]
        for k in range(steps):
            ang+=random.uniform(-0.5,0.5); x+=math.cos(ang)*random.uniform(12,26); y+=math.sin(ang)*random.uniform(12,26); pts.append((x,y))
            if random.random()<0.12 and wd>3: crack(x,y,ang+random.choice((-1,1))*0.9,steps//2,wd*0.6)
        for k in range(len(pts)-1): d.line([pts[k],pts[k+1]],fill=(255,int(120+100*random.random()),30,255),width=max(2,int(wd*(1-k/len(pts)))))
    for k in range(count):
        side=random.choice(('l','r','b'))
        if side=='l': crack(0,random.uniform(0,h),random.uniform(-0.4,0.4),30,9)
        elif side=='r': crack(w,random.uniform(0,h),math.pi+random.uniform(-0.4,0.4),30,9)
        else: crack(random.uniform(0,w),h,-math.pi/2+random.uniform(-0.5,0.5),18,8)
    im.save(R+n); return R+n
def portcullis_mask(n,wu,hu):
    im,d,W_,H_=newmask(wu,hu); bar=34; cell=110
    for y in range(60,H_-60,cell):
        for x in range(60,W_-60,cell):
            if x+cell-bar<W_-60 and y+cell-bar<H_-60: d.rectangle((x,y,x+cell-bar,y+cell-bar),fill=CUT)
    for x in range(60,W_-60,cell):
        d.polygon([(x,H_-60),(x+cell-bar,H_-60),(x+(cell-bar)/2,H_-20)],fill=CUT)
    im.save(R+n); return R+n
def bar_mask_(n,wu,hu,sc=100,bh=70,gap=60,m=60):
    im,d,W_,H_=newmask(wu,hu); y=m
    while y+bh<H_-m: d.rounded_rectangle((m,y,W_-m,y+bh),bh//2,fill=CUT); y+=bh+gap
    im.save(R+n); return R+n
def img_ornament(n,w,h,fn):
    im=canvas(w,h); d=ImageDraw.Draw(im); fn(d,w,h); im.save(R+n); return R+n

def themed(key,W=24,H=10.5,D=10,B=0.7,body=None,top=None,panel=None,logos=None,corners=None,rim=[1,1,1],world=None,recess=None,extra=None):
    z0,z1=B,H-B; pz=panel.get('h',3.55); zs=z0+pz
    names=panel['names']; n=len(names); sp=min(2.5,16.5/max(1,n-1)); off=panel.get('off',-1.0); xs=[off-(n-1)*sp/2+i*sp for i in range(n)]; KZ=z0+pz*0.45
    def ex(pn):
        label(pn,W/2-B-2.9,KZ-0.75,'STBY',panel['lab'],panel.get('font','cinzel'),16,2); label(pn,W/2-B-1.8,KZ-0.75,'POWER',panel['lab'],panel.get('font','cinzel'),16,2)
        if panel.get('ex'): panel['ex'](pn,KZ)
    art=std_panel(f'{key}_panel.png',W,B,z0,zs,xs,names,KZ,panel['lab'],panel.get('font','cinzel'),panel.get('fs',20),panel.get('r',0.4),panel.get('nums',False),ex,panel.get('line'))
    tz=dict(top); tz['z']=[zs,z1]
    if tz['type']=='perf': tz['mask']=tz['mask'](f'{key}_mask.png',W-2*B-0.04,z1-zs)
    if tz.get('tubes')=='row': tz['tubes']=[{"x":x,"h":min(2.6,(z1-zs)*0.5),"r":0.4,"color":tz['tc'],"strength":tz.get('ts',8),"light":tz.get('tl',55)} for x in (-8.1,-5.4,-2.7,0,2.7,5.4,8.1)]
    if tz.get('amb'): tz['ambient']=[{"x":x,"energy":tz.get('ae',60),"color":tz['amb'],"size":7} for x in (-6,0,6)]
    if logos: tz['logos']=logos
    pzone={"type":"panel","z":[z0,zs],"art":art,"color":panel['color'],"metal":panel.get('metal',0),"rough":panel.get('rough',0.55),"bump":panel.get('bump',["noise",300,0.04]),"artemis":panel.get('artemis',0.2),
       "knobs":KN(xs,KZ,panel.get('r',0.4),[random.uniform(.3,.8) for _ in xs],panel.get('knob','black')),"toggles":[[W/2-B-2.9,KZ]],
       "jewels":[{"x":W/2-B-1.8,"z":KZ,"color":panel['jewel'],"r":0.3,"button":panel.get('button',False),"strength":25}]}
    if panel.get('sliders'): pzone['sliders']=panel['sliders'](KZ)
    if panel.get('more'): panel['more'](pzone,KZ)
    sp_={"out":R+f"out/{key}.png","size":[W,D,H],"border":B,"recess":recess or (2.4 if tz['type']=='perf' and tz.get('tubes') else 0.5),"zones":[tz,pzone],"rimcolor":rim}
    if body: sp_['body']=body
    if corners: sp_['corners']=corners
    if world: sp_['world']=world
    if extra: sp_.update(extra)
    specs[key]=sp_

# ===== MESA-INSPIRED =====
cane=canvas(256,256); cd=ImageDraw.Draw(cane); cd.rectangle((0,0,256,256),fill=(200,160,95,255))
for cx_ in (0,128,256):
    for cy_ in (0,128,256): cd.regular_polygon((cx_,cy_,46),8,fill=(25,18,10,255))
for cx_ in (64,192):
    for cy_ in (64,192): cd.ellipse((cx_-12,cy_-12,cx_+12,cy_+12),fill=(25,18,10,255))
cane.save(R+'cane.png')
EQF=['80','240','750','2200','6600']
def eq_ex(xs0):
    def f(pn,KZ):
        for i,fq in enumerate(EQF): label(pn,xs0+i*0.62,KZ-1.25,fq,(215,215,215,255),'mich',12,0)
    return f
def eq_sl(xs0,vals):
    return lambda KZ:[{"x":xs0+i*0.62,"z0":KZ-0.95,"z1":KZ+0.95,"v":v,"color":[0.02,0.02,0.02]} for i,v in enumerate(vals)]
themed('kronos2',W=22,H=8.5,D=10,B=0.75,body={"color":[0.16,0.08,0.035],"rough":0.5,"bump":["noise",6,0.12]},
  top={"type":"cloth","color":[0.2,0.15,0.09],"weave":60,"img":R+'cane.png',"imgscale":1.6},
  logos=[{"img":logo_img('kronos_logo.png','KRONOS','cinzel',170,(230,230,232,255),spacing=26),"x":-6.5,"z":6.6,"w":4.4,"h":1.15,"metal":1.0,"rough":0.2,"emis":0.05}],
  panel={"h":3.3,"names":['VOLUME','TREBLE','BASS','MIDDLE','MASTER','DRIVE','LEAD','PRES'],"lab":(225,225,228,255),"font":'mich',"fs":14,"r":0.34,"off":-3.8,
         "color":[0.012,0.012,0.013],"rough":0.5,"jewel":[1,0.1,0.06],"knob":'black',"sliders":eq_sl(4.6,[.75,.45,.25,.5,.7]),"ex":eq_ex(4.6)},rim=[1,0.85,0.7])
themed('kronos4',W=23,H=9.5,D=10,B=0.75,body={"color":[0.02,0.02,0.022],"rough":0.8,"bump":["leather",42,0.55]},corners='L',
  top={"type":"cloth","color":[0.014,0.014,0.015],"weave":70,"piping":[0.75,0.75,0.75]},
  logos=[{"img":logo_img('kronos4_logo.png','KRONOS IV','cinzel',150,(230,230,232,255),spacing=22),"x":-6.2,"z":7.5,"w":4.6,"h":1.2,"metal":1.0,"rough":0.2,"emis":0.05}],
  panel={"h":4.6,"names":['GAIN','TREBLE','BASS','MID','LEAD','L.TREB','L.BASS','MASTER'],"lab":(225,225,228,255),"font":'mich',"fs":13,"r":0.32,"off":-3.6,
         "color":[0.012,0.012,0.013],"rough":0.5,"jewel":[1,0.1,0.06],"knob":'black',"sliders":eq_sl(5.0,[.7,.4,.3,.55,.72]),"ex":eq_ex(5.0),
         "more":lambda z,KZ:z.update({"toggles":z['toggles']+[[x,KZ+1.6] for x in (-9.0,-8.2,-7.4,-6.6)]})},rim=[0.9,0.9,1])

# ===== MAGMA-INSPIRED =====
themed('vulcan',body={"color":[0.025,0.022,0.02],"rough":0.95,"bump":["noise",12,0.9]},
  top={"type":"perf","mask":crack_mask,"thick":0.3,"color":[0.03,0.028,0.026],"metal":0.0,"rough":0.95,"backglow":{"scale":0.7,"strength":4},"amb":[1,0.25,0.03],"ae":60},
  panel={"names":['GAIN','LOW','MID','HIGH','PRESENCE','DEPTH','MASTER'],"lab":(240,130,40,255),"font":'mich',"fs":18,"color":[0.05,0.048,0.046],"rough":0.95,"bump":["noise",30,0.5],"jewel":[1,0.3,0.03],"button":True,"line":(1.05,(230,90,20,255)),
    "more":lambda z,KZ: z.update({"logos":[{"img":lava_lines('vulcan_lava.png',4500,700,4),"x":0,"z":0.7+3.55/2,"w":22.5,"h":3.5,"metal":0,"rough":0.9,"emis":5.0}]})},
  rim=[1,0.5,0.25],world=[0.012,0.008,0.006],recess=1.0)
themed('hephaestus',body={"color":[0.05,0.05,0.052],"metal":0.85,"rough":0.5,"bump":["noise",3,0.6]},
  top={"type":"perf","mask":lambda n,w,h: bar_mask_(n,w,h),"thick":0.35,"mesh":5,"color":[0.08,0.08,0.085],"metal":0.9,"rough":0.45,"tubes":'row',"tc":[1,0.42,0.08],"ts":10,"amb":[1,0.35,0.05],"ae":110},
  logos=[{"img":logo_img('heph_logo.png','HEPHAESTUS','mich',110,(240,150,60,255),spacing=20,plate=(14,13,12,255)),"x":0,"z":7.05,"w":5.6,"h":1.0,"metal":0,"rough":0.4,"emis":0.4}],
  panel={"names":['GAIN','BASS','MID','TREBLE','FORGE','PRESENCE','MASTER'],"lab":(225,220,215,255),"font":'mich',"fs":17,"color":[0.12,0.12,0.125],"metal":0.9,"rough":0.4,"bump":["noise",4,0.4],"jewel":[1,0.4,0.05],"knob":'alu'},
  rim=[1,0.6,0.3])

# ===== NORSE =====
def knot_border(d,w,h,col):
    for k in range(0,w,60):
        d.arc((k,0,k+60,40),0,180,fill=col,width=6); d.arc((k+30,10,k+90,50),180,360,fill=col,width=6)
themed('yggdrasil',recess=1.0,body={"color":[0.05,0.035,0.022],"rough":0.75,"bump":["noise",5,0.3]},
  top={"type":"perf","mask":tree_mask,"thick":0.3,"mesh":4,"color":[0.08,0.055,0.035],"metal":0.0,"rough":0.8,"backglow":{"scale":0.9,"strength":3.5,"colors":[[0.02,0.25,0.12],[0.1,0.85,0.45],[0.6,1,0.75]]}},
  panel={"names":['GAIN','URD','VERDANDI','SKULD','PRESENCE','DEPTH','MASTER'],"lab":(200,225,200,255),"font":'cinzel',"fs":18,"color":[0.035,0.03,0.025],"rough":0.8,"jewel":[0.3,1,0.5],"knob":'black',"line":(1.05,(80,170,90,255))},
  rim=[0.6,1,0.75])
themed('mjolnir',body={"color":[0.06,0.065,0.075],"metal":0.8,"rough":0.45,"bump":["noise",3,0.5]},corners='L',
  top={"type":"perf","mask":lambda n,w,h: rune_mask(n,w,h,['t','h','u','r','s','m','f']),"thick":0.3,"mesh":5,"color":[0.1,0.105,0.115],"metal":0.9,"rough":0.35,"tubes":'row',"tc":[0.45,0.75,1],"ts":10,"amb":[0.4,0.7,1],"ae":100},
  panel={"names":['GAIN','BASS','MID','TREBLE','THUNDER','PRESENCE','MASTER'],"lab":(210,225,240,255),"font":'cinzel',"fs":18,"color":[0.08,0.085,0.095],"metal":0.9,"rough":0.35,"bump":["brush",60,0.04],"jewel":[0.5,0.8,1],"knob":'alu'},
  rim=[0.7,0.85,1])
themed('valkyrie',body={"color":[0.75,0.76,0.78],"metal":0.0,"rough":0.6,"bump":["leather",30,0.3]},
  top={"type":"perf","mask":lambda n,w,h: rune_mask(n,w,h,['f','u','th','a','r','k','g','w'],w=34),"thick":0.25,"mesh":5,"color":[0.02,0.02,0.022],"metal":0.5,"rough":0.4,"tubes":'row',"tc":[1,0.12,0.08],"ts":10,"amb":[1,0.1,0.05],"ae":90},
  panel={"names":['GAIN','BASS','MID','TREBLE','VALOR','PRESENCE','MASTER'],"lab":(30,30,34,255),"font":'cinzel',"fs":18,"color":[0.7,0.71,0.73],"metal":1.0,"rough":0.3,"bump":["brush",60,0.03],"artemis":0,"jewel":[1,0.1,0.06],"knob":'black',"line":(1.05,(170,20,20,255))},
  rim=[1,0.9,0.9])

# ===== ART DECO / UNDERSEA =====
BR=[0.72,0.52,0.24]
def deco_ex(pn,KZ):
    y=pn.P(0,KZ+1.3)[1]
    for k in range(5): pn.d.line([(pn.P(-10.5+k*0.15,0)[0],y-k*8),(pn.P(10.5-k*0.15,0)[0],y-k*8)],fill=(30,22,10,255),width=2)
themed('leviathan',body={"color":[0.012,0.03,0.035],"metal":0.3,"rough":0.5,"bump":["leather",30,0.3]},corners='chrome',
  top={"type":"perf","mask":sunburst_mask,"thick":0.25,"mesh":5,"color":BR,"metal":1.0,"rough":0.25,"tubes":'row',"tc":[0.2,1,0.85],"ts":8,"amb":[0.1,0.9,0.8],"ae":90},
  logos=[{"img":logo_img('lev_logo.png','LEVIATHAN','cinzel',130,(20,15,8,255),plate=(185,140,70,255),spacing=24),"x":0,"z":5.15,"w":5.4,"h":1.05,"metal":0.9,"rough":0.25,"emis":0.0}],
  panel={"names":['GAIN','BASS','MID','TREBLE','DEPTH','PRESSURE','MASTER'],"lab":(30,22,10,255),"font":'cinzel',"fs":17,"color":BR,"metal":1.0,"rough":0.28,"bump":["brush",60,0.03],"artemis":0,"jewel":[0.2,1,0.85],"knob":'black',"ex":deco_ex},
  rim=[0.5,1,0.9],world=[0.004,0.012,0.014])
themed('kraken',body={"color":[0.01,0.02,0.018],"metal":0.2,"rough":0.6,"bump":["leather",30,0.35]},
  top={"type":"perf","mask":tentacle_mask,"thick":0.25,"mesh":5,"color":[0.05,0.11,0.09],"metal":1.0,"rough":0.3,"tubes":'row',"tc":[0.3,1,0.5],"ts":8,"amb":[0.2,0.9,0.5],"ae":90},
  logos=[{"img":logo_img('kraken_logo.png','KRAKEN','cdec',140,(220,190,120,255),plate=(10,22,18,255),spacing=10),"x":0,"z":8.95,"w":4.2,"h":0.95,"metal":0,"rough":0.4,"emis":0.2}],
  panel={"names":['GAIN','BASS','MID','TREBLE','INK','TIDE','MASTER'],"lab":(220,190,120,255),"font":'cinzel',"fs":17,"color":[0.03,0.07,0.06],"metal":1.0,"rough":0.35,"bump":["brush",60,0.03],"jewel":[0.3,1,0.5],"knob":'alu',"line":(1.05,(190,150,80,255))},
  rim=[0.6,1,0.75],world=[0.004,0.012,0.01])
themed('atlantis',body={"color":[0.02,0.02,0.022],"rough":0.7,"bump":["leather",36,0.45]},corners='chrome',
  top={"type":"perf","mask":fan_mask,"thick":0.22,"mesh":5,"color":[0.85,0.68,0.3],"metal":1.0,"rough":0.22,"tubes":'row',"tc":[0.25,0.75,1],"ts":8,"amb":[0.2,0.6,1],"ae":90},
  panel={"names":['GAIN','BASS','MID','TREBLE','CURRENT','PRESENCE','MASTER'],"lab":(225,190,110,255),"font":'mich',"fs":15,"color":[0.015,0.015,0.017],"rough":0.4,"metal":0.5,"jewel":[0.3,0.75,1],"knob":'alu',"line":(1.05,(210,170,90,255))},
  rim=[0.6,0.8,1])

# ===== SAKURA =====
themed('hanami',body={"color":[0.03,0.008,0.01],"metal":0.0,"rough":0.35,"bump":["noise",400,0.02]},
  top={"type":"perf","mask":sakura_mask,"thick":0.22,"mesh":5,"color":[0.015,0.012,0.012],"metal":0.3,"rough":0.35,"tubes":'row',"tc":[1,0.45,0.65],"ts":9,"amb":[1,0.4,0.6],"ae":110},
  panel={"names":['GAIN','BASS','MID','TREBLE','BLOSSOM','PRESENCE','MASTER'],"lab":(235,200,140,255),"font":'cinzel',"fs":17,"color":[0.4,0.03,0.04],"rough":0.35,"jewel":[1,0.5,0.7],"knob":'black',"line":(1.05,(235,200,140,255)),
    "ex":lambda pn,KZ: label(pn,-10.3,KZ+0.3,'HANAMI',(235,200,140,255),'cinzel',26,6)},
  rim=[1,0.7,0.8])

# ===== GOTHIC =====
themed('requiem',body={"color":[0.012,0.012,0.014],"rough":0.75,"bump":["leather",42,0.55]},corners='chrome',
  top={"type":"perf","mask":lancet_mask,"thick":0.3,"mesh":5,"color":[0.55,0.55,0.58],"metal":1.0,"rough":0.25,"tubes":'row',"tc":[1,0.06,0.08],"ts":10,"amb":[1,0.05,0.08],"ae":110},
  logos=[{"img":logo_img('requiem_logo.png','Requiem','frak',220,(220,220,225,255),plate=(8,8,9,255)),"x":0,"z":5.05,"w":4.6,"h":1.2,"metal":0.0,"rough":0.35,"emis":0.25}],
  panel={"names":['GAIN','BASS','MID','TREBLE','PRESENCE','DEPTH','MASTER'],"lab":(225,225,230,255),"font":'cinzel',"fs":18,"color":[0.015,0.015,0.017],"rough":0.4,"metal":0.5,"jewel":[1,0.05,0.08],"knob":'alu',"line":(1.05,(170,15,25,255))},
  rim=[1,0.4,0.45])
themed('sanctum',body={"color":[0.13,0.01,0.02],"rough":0.6,"bump":["leather",36,0.5]},corners='chrome',
  top={"type":"perf","mask":rose_mask,"thick":0.3,"mesh":5,"color":[0.02,0.02,0.022],"metal":0.6,"rough":0.3,"tubes":'row',"tc":[0.65,0.2,1],"ts":10,"amb":[0.6,0.15,1],"ae":110},
  panel={"names":['GAIN','BASS','MID','TREBLE','PRESENCE','DEPTH','MASTER'],"lab":(215,205,225,255),"font":'cinzel',"fs":18,"color":[0.015,0.012,0.017],"rough":0.45,"metal":0.4,"jewel":[0.7,0.25,1],"knob":'black',"line":(1.05,(130,60,180,255)),
    "ex":lambda pn,KZ: label(pn,-10.2,KZ+0.3,'Sanctum',(215,205,225,255),'frak',40,0)},
  rim=[0.8,0.5,1])

# ===== MEDIEVAL =====
def strap_img(n):
    im=canvas(2400,120); d=ImageDraw.Draw(im); d.rectangle((0,0,2400,120),fill=(30,28,26,255))
    for x in range(60,2400,240): d.ellipse((x-22,38,x+22,82),fill=(90,85,80,255))
    im.save(R+n); return R+n
crest=canvas(600,700); cd=ImageDraw.Draw(crest)
cd.polygon([(40,40),(560,40),(560,360),(300,660),(40,360)],fill=(110,15,18,255),outline=(200,170,90,255),width=18)
cd.line([(300,60),(300,620)],fill=(200,170,90,255),width=18); cd.line([(60,240),(540,240)],fill=(200,170,90,255),width=18)
crest.save(R+'crest.png')
themed('bastion',body={"color":[0.2,0.19,0.18],"rough":0.95,"bump":["noise",3,1.0]},
  top={"type":"wood","planks":4,"c1":[0.03,0.018,0.01],"c2":[0.22,0.12,0.06],"coat":0.0,"rough":0.85},
  logos=[{"img":strap_img('strap.png'),"x":0,"z":6.0,"w":22.6,"h":0.5,"metal":0.8,"rough":0.5,"emis":0},{"img":R+'strap.png',"x":0,"z":8.8,"w":22.6,"h":0.5,"metal":0.8,"rough":0.5,"emis":0},
         {"img":R+'crest.png',"x":0,"z":7.3,"w":2.4,"h":2.8,"metal":0.4,"rough":0.5,"emis":0}],
  panel={"names":['GAIN','BASS','MID','TREBLE','SIEGE','PRESENCE','MASTER'],"lab":(200,175,110,255),"font":'cinzel',"fs":18,"color":[0.05,0.045,0.04],"rough":0.7,"metal":0.6,"bump":["noise",8,0.3],"jewel":[1,0.45,0.1],"knob":'black',"line":(1.05,(140,110,60,255))},
  rim=[1,0.75,0.5])
themed('citadel',body={"color":[0.03,0.028,0.026],"metal":0.6,"rough":0.6,"bump":["noise",4,0.5]},corners='L',
  top={"type":"perf","mask":portcullis_mask,"thick":0.35,"mesh":0,"color":[0.06,0.055,0.05],"metal":0.8,"rough":0.55,"tubes":'row',"tc":[1,0.5,0.12],"ts":9,"amb":[1,0.45,0.1],"ae":110},
  logos=[{"img":R+'crest.png',"x":0,"z":7.0,"w":2.1,"h":2.45,"metal":0.4,"rough":0.5,"emis":0.05}],
  panel={"names":['GAIN','BASS','MID','TREBLE','KEEP','PRESENCE','MASTER'],"lab":(200,175,110,255),"font":'cinzel',"fs":18,"color":[0.035,0.032,0.03],"rough":0.6,"metal":0.7,"bump":["noise",8,0.25],"jewel":[1,0.5,0.12],"knob":'black',"line":(1.05,(140,110,60,255))},
  rim=[1,0.7,0.45])

BG={'mjolnir':[[0.02,0.08,0.25],[0.3,0.6,1.0],[0.8,0.9,1.0]],'valkyrie':[[0.3,0.0,0.0],[1.0,0.1,0.05],[1.0,0.55,0.4]],'kraken':[[0.0,0.15,0.08],[0.15,0.85,0.45],[0.7,1.0,0.8]],
    'hanami':[[0.35,0.05,0.15],[1.0,0.4,0.62],[1.0,0.85,0.9]],'sanctum':[[0.15,0.0,0.3],[0.6,0.15,1.0],[1.0,0.4,0.7]]}
for k,cols in BG.items():
    z=specs[k]['zones'][0]; z.pop('tubes',None); z.pop('mesh',None); z['backglow']={"scale":1.1,"strength":3.0,"colors":cols}; specs[k]['recess']=1.0
for k in ('kronos2','kronos4'): specs.pop(k,None)
for k,v in specs.items():
    for z in v['zones']:
        if z.get('mesh')==0: z.pop('mesh')
    json.dump(v,open(R+k+'.json','w'))
print('ok',list(specs))
