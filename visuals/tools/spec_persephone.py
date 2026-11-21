import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel
KX=[-8.5,-6.0,-3.5,-1.0,1.5,4.0,6.5]; LAB=['GAIN','BASS','MIDDLE','TREBLE','PRESENCE','DEPTH','MASTER']; V=[0.62,0.5,0.55,0.48,0.52,0.45,0.4]
WHITE=[0.78,0.77,0.74]
spec={"size":[24,10,10.5],"border":0.7,"recess":1.0,"bevel":0.2,"rimcolor":[1,0.9,0.85],"world":[0.012,0.011,0.012],
 "body":{"color":[0.72,0.71,0.68],"rough":0.78,"bump":["leather",40,0.35]},
 "handle":{"w":8.5,"h":1.2,"color":[0.66,0.65,0.62]},
 "glow_strips":[{"z":4.25,"color":[1.0,0.62,0.6],"strength":5,"w":22.54}],
 "zones":[
  {"type":"relief","z":[4.25,9.8],"height":"art/persephone_h.png","depth":0.42,"res":56,"color":WHITE,"cavity":0.72,"rough":0.78,"hi_at":0.5,
   "grain":[180,0.04],"graze":[{"color":[1,0.88,0.8],"energy":70,"tilt":-25,"out":1.0,"top":0.35}]},
  {"type":"panel","z":[0.7,4.25],"art":"art/persephone_panel.png","artemis":0.0,"artrough":0.6,"color":[0.74,0.73,0.7],"rough":0.72,"bump":["noise",300,0.02],
   "knobs":[{"x":x,"z":2.2975,"r":0.42,"v":v,"style":"white"} for x,v in zip(KX,V)],
   "toggles":[[8.4,2.2975]],"jewels":[{"x":9.5,"z":2.2975,"color":[1,0.5,0.6],"r":0.28,"strength":28}],
   "jacks":[[-10.3,2.2975,"jack"]]}
 ]}
json.dump(spec,open('amps/persephone.json','w'),indent=1)
panel('art/persephone_panel.png',spec,(0.7,4.25),[{"x":x,"z":2.2975,"r":0.42,"label":l} for x,l in zip(KX,LAB)],
      toggles=[(8.4,2.2975,'STANDBY')],jewels=[(9.5,2.15,'POWER')],col=(110,100,94,255),size=30,
      lines=[(-10.9,0.95,10.9,0.95,2),(-10.9,3.95,10.9,3.95,2)])
print('ok')
