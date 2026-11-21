import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel
KX=[-8.5,-6.0,-3.5,-1.0,1.5,4.0,6.5]; LAB=['GAIN','BASS','MIDDLE','TREBLE','PRESENCE','RESONANCE','MASTER']; V=[0.7,0.45,0.4,0.55,0.5,0.6,0.42]
RED=[1,0.07,0.05]
tubes=[{"x":x,"z":6.85,"h":2.7,"r":0.36,"color":RED,"strength":9,"light":55} for x in (-9.55,-6.95,-4.35,4.35,6.95,9.55)]+[{"x":x,"z":7.1,"h":2.3,"r":0.34,"color":RED,"strength":9,"light":45} for x in (-1.25,1.25)]
spec={"size":[24,10,10.5],"border":0.7,"recess":2.4,"rimcolor":[1,0.45,0.5],"corners":"chrome","chrome_size":1.6,
 "body":{"color":[0.012,0.012,0.014],"rough":0.62,"bump":["leather",42,0.55]},
 "handle":{"w":8.5,"h":1.2},
 "glow_strips":[{"z":4.25,"color":RED,"strength":4,"w":22.54}],
 "zones":[
  {"type":"relief","z":[4.25,9.8],"height":"art/lamia_h.png","holes":"art/lamia_holes.png","depth":0.42,"res":56,
   "color":[0.16,0.155,0.15],"cavity":0.3,"metal":0.85,"rough":0.36,"hi_at":0.75,"grain":[120,0.05],"tubes":tubes,
   "ambient":[{"x":x,"energy":90,"color":RED,"size":5} for x in (-7,0,7)]},
  {"type":"panel","z":[0.7,4.25],"art":"art/lamia_panel.png","artemis":0.15,"color":[0.014,0.014,0.016],"metal":0.5,"rough":0.42,"bump":["noise",300,0.03],
   "knobs":[{"x":x,"z":2.2975,"r":0.42,"v":v,"style":"silver"} for x,v in zip(KX,V)],
   "toggles":[[8.4,2.2975]],"jewels":[{"x":9.5,"z":2.2975,"color":RED,"r":0.28,"strength":30}],
   "jacks":[[-10.3,2.2975,"jack"]]}
 ]}
json.dump(spec,open('amps/lamia.json','w'),indent=1)
def gothic(d,P,col):   # small pointed-arch bracket over the EQ and a fleur at each end
    import math
    x0,x1=P(-6.6,3.25)[0],P(1.9,3.25)[0]; y=P(0,3.25)[1]
    d.line([(x0,y),(x1,y)],fill=col,width=2)
    for x in (x0,x1): d.line([(x,y),(x,y+14)],fill=col,width=2)
panel('art/lamia_panel.png',spec,(0.7,4.25),[{"x":x,"z":2.2975,"r":0.42,"label":l} for x,l in zip(KX,LAB)],
      toggles=[(8.4,2.2975,'STANDBY')],jewels=[(9.5,2.15,'POWER')],face='cinzel',col=(185,182,186,255),size=28,
      lines=[(-10.9,0.95,10.9,0.95,2)])
print('ok')
