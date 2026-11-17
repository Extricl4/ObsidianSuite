from PIL import Image, ImageDraw, ImageFont, ImageFilter
import math
FD='/home/claude/r3d/fonts/'
F={'cinzel':FD+'Cinzel.ttf','cdec':FD+'CinzelDecorative-Bold.ttf.ttf','frak':FD+'UnifrakturCook-Bold.ttf.ttf','mich':FD+'Michroma-Regular.ttf.ttf','saira':FD+'Saira.ttf'}
W,H=1320,2400
def canvas(w=None,h=None): return Image.new('RGBA',(w or W,h or H),(0,0,0,0))
def font(k,s):
    f=ImageFont.truetype(F[k],s)
    try:
        if k in('cinzel','saira'): f.set_variation_by_axes([700] if k=='cinzel' else [100,600])
    except Exception: pass
    return f
def text(d,xy,t,k,s,fill,spacing=0,anchor='mm'):
    f=font(k,s)
    if spacing:
        tot=sum(d.textlength(c,font=f) for c in t)+spacing*(len(t)-1); x=xy[0]-tot/2
        for c in t:
            d.text((x,xy[1]),c,font=f,fill=fill,anchor='lm'); x+=d.textlength(c,font=f)+spacing
    else: d.text(xy,t,font=f,fill=fill,anchor=anchor)
def frame(d,col,inset=46,r=60,w=5,WW=None,HH=None):
    WW=WW or W; HH=HH or H
    d.rounded_rectangle((inset,inset,WW-inset,HH-inset),r,outline=col,width=w)
    d.rounded_rectangle((inset+18,inset+18,WW-inset-18,HH-inset-18),r-14,outline=col,width=2)
