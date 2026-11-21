import json, sys
sys.path.insert(0, 'tools')
from panel_art import panel
KX=[-8.5,-6.0,-3.5,-1.0,1.5,4.0,6.5]; LAB=['Gain','Bass','Middle','Treble','Presence','Depth','Master']; V=[0.7,0.55,0.4,0.55,0.5,0.6,0.42]
AMB=[1.0,0.42,0.1]
tubes=[{"x":x,"z":5.95,"h":2.3,"r":0.36,"color":AMB,"strength":9,"light":55} for x in (-2.25,-0.75,0.75,2.25)]
spec={"size":[24,10,10.5],"border":0.7,"recess":2.4,"bevel":0.15,"rimcolor":[1,0.75,0.5],
 "body":{"color":[0.035,0.032,0.03],"rough":0.85,"bump":["leather",30,0.6]},
 "handle":{"w":8.5,"h":1.2,"color":[0.06,0.04,0.025]},
 "glow_strips":[{"z":4.25,"color":AMB,"strength":4,"w":22.54}],
 "zones":[
  {"type":"relief","z":[4.25,9.8],"height":"art/portcullis_h.png","holes":"art/portcullis_holes.png","depth":0.36,"res":56,
   "color":[0.15,0.14,0.125],"cavity":0.4,"rough":0.92,"hi_at":0.55,"grain":[60,0.25],"tubes":tubes,
   "ambient":[{"x":0,"energy":120,"color":AMB,"size":5}],
   "portcullis":{"box":[-3.3,3.3,7.35,9.6],"nv":9,"nh":4},"iron_color":[0.03,0.028,0.027],
   "torches":[{"x":-5.9,"z":7.55,"dy":0.27,"strength":3.5,"light":30,"color":[1,0.24,0.02]},{"x":5.9,"z":7.55,"dy":0.27,"strength":3.5,"light":30,"color":[1,0.24,0.02]}]},
  {"type":"panel","z":[0.7,4.25],"art":"art/portcullis_panel.png","artemis":0.15,"color":[0.05,0.047,0.045],"metal":0.8,"rough":0.62,"bump":["noise",14,0.35],
   "knobs":[{"x":x,"z":2.2975,"r":0.42,"v":v,"style":"iron"} for x,v in zip(KX,V)],
   "toggles":[[8.4,2.2975]],"jewels":[{"x":9.5,"z":2.2975,"color":AMB,"r":0.28,"strength":30}],
   "jacks":[[-10.3,2.2975,"jack"]],"posts":[[-11.0,0.98],[11.0,0.98],[-11.0,3.97],[11.0,3.97]]}
 ]}
json.dump(spec,open('amps/portcullis.json','w'),indent=1)
panel('art/portcullis_panel.png',spec,(0.7,4.25),[{"x":x,"z":2.2975,"r":0.42,"label":l} for x,l in zip(KX,LAB)],
      toggles=[(8.4,2.2975,'Standby')],jewels=[(9.5,2.15,'Power')],face='frak',col=(196,160,112,255),size=40,
      lines=[(-10.6,0.98,10.6,0.98,3)])
print('ok')
