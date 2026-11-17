import json, math, random, sys
R='/home/claude/r3d/'
ns={}
_argv=sys.argv
exec(open(R+'amps3_spec.py').read(),ns)
ns4={}; exec(open(R+'amps4_spec.py').read(),ns4)
sys.argv=_argv
M={k:ns.get(k) for k in ('chevron_mask','diamond_mask','herring_mask','bar_mask','circle_mask','hex_mask','slot_mask','logo_img')}
M.update({k:ns4[k] for k in ('crack_mask','sunburst_mask','lancet_mask','portcullis_mask','sakura_mask','rune_mask')})
SIZES={'1x12':(16,14.5,11,[(0,0.5,4.8)]),'2x12':(27,16,12,[(-6.4,0.5,4.8),(6.4,0.5,4.8)]),'2x12v':(15,27,12,[(0,0.28,4.8),(0,0.72,4.8)]),
       '4x10':(22,22,12,[(-5.0,0.28,4.0),(5.0,0.28,4.0),(-5.0,0.72,4.0),(5.0,0.72,4.0)]),'4x12':(24,24,13,[(-5.6,0.27,4.8),(5.6,0.27,4.8),(-5.6,0.73,4.8),(5.6,0.73,4.8)])}
specs={}
def cab(key,size,front,body=None,B=0.7,corners=None,logo=None,rim=[0.8,0.85,1],world=None):
    W,H,D,spk=SIZES[size]
    z0,z1=B,H-B
    zones=[]
    if front['type']=='woodband':
        zb=z1-front.get('band',3.0)
        zones.append({"type":"wood","z":[zb,z1],"planks":1,"c1":front.get('c1',[0.004,0.003,0.003]),"c2":front['c2'],"fade":front.get('fade'),"coat":0.0,"rough":0.8})
        zones.append({"type":"cloth","z":[z0,zb-0.04],"color":front.get('cloth',[0.008,0.008,0.009]),"weave":front.get('weave',70),"sheen":0.05})
    elif front['type']=='cloth':
        z={"type":"cloth","z":[z0,z1],"color":front.get('cloth',[0.008,0.008,0.009]),"weave":front.get('weave',70),"sheen":0.05}
        for k in ('piping','img','imgscale'):
            if k in front: z[k]=front[k]
        zones.append(z)
    else:
        z={"type":"perf","z":[z0,z1],"mask":M[front['mask']](f'{key}_mask.png',W-2*B-0.04,z1-z0) if front['mask']!='rune_mask' else M['rune_mask'](f'{key}_mask.png',W-2*B-0.04,z1-z0,front['word'],s=int((z1-z0)*100*0.22),w=30),
           "thick":front.get('thick',0.12),"color":front['color'],"metal":front.get('metal',0.7),"rough":front.get('rough',0.4),
           "speakers":[[x,z0+(z1-z0)*f,r] for (x,f,r) in spk]}
        if front.get('bg'): z['backglow']={'scale':1.0,'strength':front.get('bgs',1.3),'colors':front['bg']}; z.pop('speakers')
        if front.get('amb'): z['ambient']=[{"x":x,"energy":front.get("ae",90)*(W/24)*4,"color":front['amb'],"size":W/3} for x in (-W/4,0,W/4)]
        zones.append(z)
    if logo: zones[-1].setdefault('logos',[]).append(logo)
    if front['type']=='woodband' and logo: zones[-1]['logos']=[logo]
    rx,ry=(1300,900) if W>H*1.3 else ((800,1100) if H>W*1.3 else (1000,1000))
    a=rx/ry; dist=1.95*max(W,H*a) if a>=1 else 1.95*max(H,W/a)
    s={"out":R+f"out/{key}.png","size":[W,D,H],"border":B,"recess":1.4 if front['type']=='perf' else 0.5,"res":[rx,ry],"samples":28,
       "zones":zones,"cam":[-0.28*W,-dist,H*0.5+dist*0.2],"target":[0.3,0,H*0.47],"kscale":1.3,"rimcolor":rim}
    if body: s['body']=body
    if corners: s['corners']=corners
    if world: s['world']=world
    specs[key]=s
LEATHER={"color":[0.022,0.022,0.024],"rough":0.82,"bump":["leather",42,0.55]}
lg=lambda img,z_from_top,H_,w,h,metal=0.9,emis=0.1,x=0: {"img":R+img,"x":x,"z":H_-z_from_top,"w":w,"h":h,"metal":metal,"rough":0.3,"emis":emis}
H=SIZES
cab('styx_cab','2x12',{"type":"woodband","c2":[0.1,0.34,0.58],"fade":[0.2,1.4]},LEATHER,corners='L')
cab('acheron_cab','4x12',{"type":"woodband","c2":[0.36,0.15,0.045],"fade":[0.35,1.25],"band":4.0},LEATHER,corners='L')
cab('hades_cab','2x12v',{"type":"woodband","c2":[0.42,0.025,0.02],"fade":[0.35,1.25]},LEATHER,corners='L')
cab('hecate_cab','1x12',{"type":"woodband","c2":[0.26,0.07,0.45],"fade":[0.35,1.25],"band":2.6},LEATHER,corners='L')
cab('nemesis_cab','4x10',{"type":"woodband","c2":[0.03,0.32,0.16],"fade":[0.35,1.25],"band":3.6},LEATHER,corners='L')
cab('cerberus_cab','4x12',{"type":"perf","mask":"herring_mask","color":[0.016,0.016,0.018],"metal":0.55,"amb":[1,0.3,0.05],"ae":70},rim=[1,0.6,0.35])
cab('ares_cab','2x12',{"type":"perf","mask":"bar_mask","color":[0.13,0.13,0.14],"metal":0.9,"amb":[1,0.12,0.06],"ae":60},LEATHER,rim=[1,0.5,0.45])
cab('thanatos_cab','2x12',{"type":"perf","mask":"hex_mask","color":[0.05,0.055,0.06],"metal":0.3,"rough":0.5,"bg":[[0.0,0.12,0.18],[0.05,0.6,0.75],[0.6,0.95,1.0]]},{"color":[0.018,0.02,0.024],"metal":0.6,"rough":0.42,"bump":["noise",200,0.05]},B=0.5,rim=[0.5,0.9,1])
cab('orpheus_cab','1x12',{"type":"perf","mask":"circle_mask","color":[0.3,0.22,0.12],"metal":0.9,"rough":0.3,"bg":[[0.25,0.08,0.0],[1.0,0.5,0.1],[1.0,0.85,0.5]]},{"color":[0.06,0.045,0.03],"metal":0.7,"rough":0.4,"bump":["noise",200,0.05]},B=0.5,rim=[1,0.8,0.5])
cab('icarus_cab','4x12',{"type":"cloth","piping":[0.85,0.65,0.3]},LEATHER,corners='L',logo=lg('icarus_logo.png',2.4,24,5.4,1.4,0.95,0.1))
cab('erinyes_cab','4x12',{"type":"perf","mask":"diamond_mask","color":[0.6,0.6,0.62],"metal":1.0,"rough":0.25,"amb":[1,0.2,0.1],"ae":50},{"color":[0.16,0.012,0.018],"rough":0.6,"bump":["leather",36,0.55]},corners='chrome',
    logo={"img":R+'erinyes_logo.png',"x":0,"z":21.6,"w":4.6,"h":1.2,"metal":0,"rough":0.35,"emis":0.35},rim=[1,0.4,0.4])
cab('elysium_cab','2x12',{"type":"cloth","cloth":[0.32,0.22,0.12],"weave":90,"piping":[0.8,0.75,0.62]},{"color":[0.72,0.64,0.48],"rough":0.7,"bump":["leather",36,0.45]},corners='chrome',
    logo=lg('elysium_logo.png',2.3,16,4.4,1.15,0.9,0.15,x=-7.5),rim=[1,0.9,0.75])
cab('nereid_cab','2x12',{"type":"cloth","cloth":[0.2,0.13,0.08],"img":R+'nereid_cloth.png',"imgscale":0.9,"piping":[0.85,0.8,0.68]},{"color":[0.22,0.48,0.44],"rough":0.65,"bump":["leather",36,0.45]},
    logo={"img":R+'nereid_logo.png',"x":0,"z":13.6,"w":3.8,"h":1.0,"metal":0,"rough":0.4,"emis":0.1},rim=[0.7,1,0.9])
cab('selene_cab','1x12',{"type":"woodband","c1":[0.06,0.03,0.015],"c2":[0.38,0.2,0.09],"cloth":[0.42,0.42,0.43],"band":2.6},{"color":[0.8,0.78,0.72],"rough":0.55,"bump":["leather",20,0.25]},rim=[1,0.9,0.8])
cab('calliope_cab','1x12',{"type":"cloth","cloth":[0.3,0.21,0.13],"weave":90,"piping":[0.85,0.78,0.62]},{"color":[0.42,0.24,0.1],"rough":0.45,"bump":["noise",8,0.15]},
    logo={"img":R+'calliope_logo.png',"x":0,"z":7.25,"w":4.2,"h":4.2,"metal":0.4,"rough":0.3,"emis":0.15},rim=[1,0.85,0.65])
cab('aether_cab','2x12v',{"type":"cloth","cloth":[0.42,0.42,0.44],"weave":110},{"color":[0.82,0.82,0.83],"rough":0.6,"bump":["leather",20,0.2]},B=0.6,
    logo={"img":R+'aether_logo.png',"x":0,"z":2.0,"w":4.2,"h":1.1,"metal":1.0,"rough":0.25,"emis":0.0},rim=[0.8,0.9,1])
cab('mnemosyne_cab','2x12',{"type":"perf","mask":"hex_mask","color":[0.012,0.012,0.013],"metal":0.2,"rough":0.85,"amb":[0.75,0.85,1],"ae":40},{"color":[0.02,0.02,0.022],"metal":0.3,"rough":0.8,"bump":["noise",160,0.08]},B=0.5,
    logo={"img":R+'mnemosyne_line.png',"x":0,"z":0.62,"w":25.8,"h":0.09,"metal":0,"rough":0.3,"emis":6.0},rim=[0.6,0.75,1])
cab('eos_cab','1x12',{"type":"cloth","cloth":[0.75,0.74,0.7],"weave":110},{"color":[0.26,0.13,0.06],"rough":0.55,"bump":["noise",6,0.15]},B=0.9,rim=[1,0.85,0.7])
cab('kronos_cab','1x12',{"type":"cloth","cloth":[0.2,0.15,0.09],"img":R+'cane.png',"imgscale":1.6},{"color":[0.16,0.08,0.035],"rough":0.5,"bump":["noise",6,0.12]},B=0.9,
    logo={"img":R+'kronos_logo.png',"x":-3.8,"z":12.4,"w":3.6,"h":0.95,"metal":1.0,"rough":0.2,"emis":0.05},rim=[1,0.85,0.7])
cab('vulcan_cab','4x12',{"type":"perf","mask":"crack_mask","thick":0.25,"color":[0.03,0.028,0.026],"metal":0.0,"rough":0.95,"bg":[[0.5,0.04,0.0],[1.0,0.28,0.02],[1.0,0.75,0.25]],"bgs":4},{"color":[0.025,0.022,0.02],"rough":0.95,"bump":["noise",12,0.9]},rim=[1,0.5,0.25],world=[0.012,0.008,0.006])
cab('leviathan_cab','2x12',{"type":"perf","mask":"sunburst_mask","color":[0.72,0.52,0.24],"metal":1.0,"rough":0.25,"amb":[0.1,0.9,0.8],"ae":110},{"color":[0.012,0.03,0.035],"metal":0.3,"rough":0.5,"bump":["leather",30,0.3]},corners='chrome',rim=[0.5,1,0.9])
cab('requiem_cab','4x12',{"type":"perf","mask":"lancet_mask","color":[0.55,0.55,0.58],"metal":1.0,"rough":0.25,"amb":[1,0.05,0.08],"ae":110},LEATHER,corners='chrome',rim=[1,0.4,0.45])
cab('mjolnir_cab','4x12',{"type":"perf","mask":"rune_mask","word":['t','h','u','r','s','m','f','a','r'],"color":[0.1,0.105,0.115],"metal":0.9,"rough":0.35,"bg":[[0.02,0.08,0.25],[0.3,0.6,1.0],[0.8,0.9,1.0]],"bgs":2.5},{"color":[0.06,0.065,0.075],"metal":0.8,"rough":0.45,"bump":["noise",3,0.5]},corners='L',rim=[0.7,0.85,1])
cab('hanami_cab','2x12',{"type":"perf","mask":"sakura_mask","color":[0.015,0.012,0.012],"metal":0.3,"rough":0.35,"bg":[[0.35,0.05,0.15],[1.0,0.4,0.62],[1.0,0.85,0.9]],"bgs":2.5},{"color":[0.03,0.008,0.01],"rough":0.35,"bump":["noise",400,0.02]},rim=[1,0.7,0.8])
cab('citadel_cab','4x12',{"type":"perf","mask":"portcullis_mask","thick":0.3,"color":[0.06,0.055,0.05],"metal":0.8,"rough":0.55,"bg":[[0.3,0.08,0.0],[1.0,0.45,0.1],[1.0,0.8,0.45]],"bgs":2.0},{"color":[0.03,0.028,0.026],"metal":0.6,"rough":0.6,"bump":["noise",4,0.5]},corners='L',
    logo={"img":R+'crest.png',"x":0,"z":21.0,"w":2.0,"h":2.3,"metal":0.4,"rough":0.5,"emis":0.05},rim=[1,0.7,0.45])
for k,v in specs.items(): json.dump(v,open(R+k+'.json','w'))
print('ok',len(specs),list(specs))
