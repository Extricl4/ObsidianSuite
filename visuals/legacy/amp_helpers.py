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
