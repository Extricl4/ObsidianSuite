import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel
KX=[-8.5,-6.0,-3.5,-1.0,1.5,4.0,6.5]; LAB=['Gain','Bass','Middle','Treble','Presence','Depth','Master']; V=[0.65,0.55,0.45,0.5,0.5,0.6,0.4]
AMB=[1.0,0.42,0.1]
spec={"size":[24,10,10.5],"border":0.7,"recess":0.75,"bevel":0.15,"rimcolor":[1,0.75,0.5],
 "body":{"color":[0.03,0.026,0.024],"rough":0.85,"bump":["leather",30,0.6]},
 "handle":{"w":8.5,"h":1.2,"color":[0.06,0.04,0.025]},
 "glow_strips":[{"z":4.25,"color":AMB,"strength":4,"w":22.54}],
 "zones":[
  {"type":"wood","z":[4.25,9.8],"vertical":True,"planks":9,"c1":[0.03,0.016,0.008],"c2":[0.19,0.1,0.045],"coat":0.0,"rough":0.82,
   "straps":[{"z":5.0,"h":0.46,"pitch":1.25},{"z":9.05,"h":0.46,"pitch":1.25}],
   "rings":[[-5.4,7.35],[5.4,7.35]],"lockplates":[[0,7.0,1.5,2.0]],"iron_color":[0.03,0.028,0.027]},
  {"type":"panel","z":[0.7,4.25],"art":"art/rampart_panel.png","artemis":0.15,"color":[0.05,0.047,0.045],"metal":0.8,"rough":0.62,"bump":["noise",14,0.35],
   "knobs":[{"x":x,"z":2.2975,"r":0.42,"v":v,"style":"iron"} for x,v in zip(KX,V)],
   "toggles":[[8.4,2.2975]],"jewels":[{"x":9.5,"z":2.2975,"color":AMB,"r":0.28,"strength":30}],
   "jacks":[[-10.3,2.2975,"jack"]],
   "posts":[[-11.0,0.98],[11.0,0.98],[-11.0,3.97],[11.0,3.97]]}
 ]}
json.dump(spec,open('amps/rampart.json','w'),indent=1)
panel('art/rampart_panel.png',spec,(0.7,4.25),[{"x":x,"z":2.2975,"r":0.42,"label":l} for x,l in zip(KX,LAB)],
      toggles=[(8.4,2.2975,'Standby')],jewels=[(9.5,2.15,'Power')],face='frak',col=(196,160,112,255),size=40,
      lines=[(-10.6,0.98,10.6,0.98,3)])
print('ok')
