import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel
KX=[-8.5,-6.0,-3.5,-1.0,1.5,4.0,6.5]; LAB=['GAIN','BASS','MIDDLE','TREBLE','PRESENCE','DEPTH','MASTER']; V=[0.6,0.5,0.45,0.55,0.5,0.55,0.4]
VIO=[0.62,0.25,1.0]
spec={"size":[24,10,10.5],"border":0.7,"recess":1.0,"bevel":0.2,"rimcolor":[0.75,0.6,1.0],"corners":"chrome","chrome_size":1.3,
 "body":{"color":[0.012,0.011,0.014],"rough":0.6,"bump":["leather",42,0.5]},
 "handle":{"w":8.5,"h":1.2},
 "glow_strips":[{"z":4.25,"color":VIO,"strength":4,"w":22.54}],
 "zones":[
  {"type":"relief","z":[4.25,9.8],"height":"art/melinoe_h.png","depth":0.4,"res":56,
   "color":[0.62,0.62,0.66],"low":[0.008,0.008,0.01],"lo_at":0.16,"hi_at":0.34,"metal":0.2,"metal_hi":1.0,"rough":0.3,"grain":[160,0.03],
   "graze":[{"color":VIO,"energy":60,"tilt":-25,"out":1.0,"top":0.4}]},
  {"type":"panel","z":[0.7,4.25],"art":"art/melinoe_panel.png","artemis":0.15,"color":[0.012,0.012,0.014],"metal":0.4,"rough":0.35,"bump":["noise",300,0.03],
   "knobs":[{"x":x,"z":2.2975,"r":0.42,"v":v,"style":"silver"} for x,v in zip(KX,V)],
   "toggles":[[8.4,2.2975]],"jewels":[{"x":9.5,"z":2.2975,"color":VIO,"r":0.28,"strength":30}],
   "jacks":[[-10.3,2.2975,"jack"]]}
 ]}
json.dump(spec,open('amps/melinoe.json','w'),indent=1)
panel('art/melinoe_panel.png',spec,(0.7,4.25),[{"x":x,"z":2.2975,"r":0.42,"label":l} for x,l in zip(KX,LAB)],
      toggles=[(8.4,2.2975,'STANDBY')],jewels=[(9.5,2.15,'POWER')],face='cinzel',col=(190,186,196,255),size=28,
      lines=[(-10.9,0.95,10.9,0.95,2),(-10.9,3.95,10.9,3.95,2)])
print('ok')
