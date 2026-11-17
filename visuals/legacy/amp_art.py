from art_common import *
import math
AW,AH=4080,674
def X(x): return (x+10.2)*200
KY=(4.12-1.9)*200
C=(205,205,210,255)
im=canvas(AW,AH); d=ImageDraw.Draw(im)
def ticks(x,r0,r1,n,w):
    cx=X(x)
    for i in range(n):
        a=math.radians(135+270*i/(n-1)); d.line([(cx+r0*math.cos(a),KY+r0*math.sin(a)),(cx+r1*math.cos(a),KY+r1*math.sin(a))],fill=C,width=w)
for x in (-4.37,4.43):
    ticks(x,140,178,21,4); d.arc((X(x)-182,KY-182,X(x)+182,KY+182),135,405,fill=C,width=3)
    d.line([(X(x),KY+200),(X(x),KY+290)],fill=C,width=3)
for x in (-2.71,-1.54,-0.37,1.38,2.57): ticks(x,92,112,11,3)
for x,n in ((-4.37,'GAIN'),(-2.71,'BASS'),(-1.54,'MID'),(-0.37,'TREBLE'),(1.38,'PRESENCE'),(2.57,'DEPTH'),(4.43,'OUTPUT')):
    text(d,(X(x),KY-(250 if n in('GAIN','OUTPUT') else 215)),n,'cinzel',36,C,spacing=5)
y=KY-170; d.line([(X(-2.71),y),(X(-0.37),y)],fill=C,width=3)
for x in (-2.71,-1.54,-0.37): d.line([(X(x),y),(X(x),y+22)],fill=C,width=3)
y=KY-170; d.line([(X(1.38),y),(X(2.57),y)],fill=C,width=3)
for x in (1.38,2.57): d.line([(X(x),y),(X(x),y+22)],fill=C,width=3)
yb=KY+150; d.line([(X(1.38),yb),(X(2.57),yb)],fill=C,width=3)
for x in (1.38,2.57): d.line([(X(x),yb),(X(x),yb-22)],fill=C,width=3)
for a,b in ((-4.37+0.75,-2.71-0.5),(-2.71+0.5,-1.54-0.5),(-1.54+0.5,-0.37-0.5),(-0.37+0.5,1.38-0.5),(1.38+0.5,2.57-0.5),(2.57+0.5,4.43-0.75)): d.line([(X(a),KY),(X(b),KY)],fill=C,width=3)
d.line([(X(6.9),KY),(X(9.0),KY)],fill=C,width=3)
text(d,(X(7.49),KY+120),'STANDBY','cinzel',24,C,spacing=3); text(d,(X(8.37),KY+120),'POWER','cinzel',24,C,spacing=3)
im.save('/home/claude/r3d/amp_art.png'); print('ok')
