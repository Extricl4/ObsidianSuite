"""Copy of visuals/render/head.py for the plugin's own amp faces (plugin/tools/faces/amps), so their renders
never touch visuals/. Added: relief "emit" (heat glow in the low parts), "gloss" (a polished top layer)
and "glass_base". Run with a Python that has bpy: python head.py -- spec.json front out=on ..."""
import bpy, math, sys, json
from mathutils import Vector
import os as _os, math as _m
_ROOT=_os.path.dirname(_os.path.dirname(_os.path.abspath(__file__)))
_SPEC=sys.argv[sys.argv.index('--')+1]
_ARGS=sys.argv[sys.argv.index('--')+2:]
def _fix(o):
    if isinstance(o,dict): return {k:_fix(v) for k,v in o.items()}
    if isinstance(o,list): return [_fix(v) for v in o]
    if isinstance(o,str) and o.startswith('art/'): return _os.path.join(_ROOT,o)
    return o
S=_fix(json.load(open(_SPEC)))
_A={}
for _a in _ARGS:
    if '=' in _a: _k,_v=_a.split('=',1); _A[_k]=_v
    else: _A[_a]=True
if _A.get('layer')=='ptr':
    for _z in S['zones']:
        for _k in _z.get('knobs',[]): _k['v']=0.5
_HOT=[]
_KEY=_os.path.splitext(_os.path.basename(_SPEC))[0]
_VIEW='front' if 'front' in _ARGS else 'angle'
_os.makedirs(_os.path.join(_ROOT,'out',_VIEW),exist_ok=True)
S['out']=_os.path.join(_ROOT,'out',_VIEW,_KEY+'.png')
if _VIEW=='front':
    _W,_D,_H=S['size']; _a=_m.atan(18/85); _cab=_KEY.endswith('_cab'); _W=_W+2*S.get('rack',{}).get('ear',0)+(2.2 if S.get('rack') else 0)
    _H=_H+(S['handle'].get('h',1.1)+0.3 if S.get('handle') else 0)
    _H=_H+(0.9 if S.get('rack') else 0)
    _rx=1400 if _cab else (2400 if S.get('rack') else 1800); _ry=max(400,min(int(_rx*_H/_W),2400))
    _dist=(_W*1.1/2)/_m.tan(_a) if _rx>=_ry else (_H*1.1/2)/_m.tan(_a)
    _zc=_H*0.42 if S.get('open') else _H/2
    S.update({"res":[_rx,_ry],"samples":28,"lens":85,"cam":[0,-_dist,_zc],"target":[0,0,_zc]})

W,D,HH=S['size']; FY=-D/2; B=S.get('border',0.75); DEP=S.get('recess',0.45)
bpy.ops.wm.read_factory_settings(use_empty=True)
sc=bpy.context.scene
sc.render.engine='CYCLES'; sc.cycles.samples=S.get('samples',40); sc.cycles.use_adaptive_sampling=True; sc.cycles.use_denoising=True; sc.cycles.device='CPU'
sc.render.resolution_x,sc.render.resolution_y=S.get('res',[1560,680])
sc.view_settings.view_transform='AgX'; sc.view_settings.look='AgX - Medium High Contrast'
sc.cycles.transparent_max_bounces=16
if 'prev' in sys.argv:
    sc.cycles.samples=6; sc.render.resolution_x//=2; sc.render.resolution_y//=2; S['out']=_os.path.join(_ROOT,'out','preview',_VIEW+'_'+_KEY+'.png'); _os.makedirs(_os.path.dirname(S['out']),exist_ok=True)

def add(op,**kw): op(**kw); return bpy.context.active_object
def apply_scale(o): bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
def bevel(o,w=0.05,seg=3):
    m=o.modifiers.new('b','BEVEL'); m.width=w; m.segments=seg; m.limit_method='ANGLE'
    bpy.context.view_layer.objects.active=o; bpy.ops.object.shade_smooth(); return o
def setmat(o,m): o.data.materials.clear(); o.data.materials.append(m); return o
def box(loc,size,m,bev=0.03):
    c=add(bpy.ops.mesh.primitive_cube_add,size=1,location=loc); c.scale=size; apply_scale(c)
    if bev: bevel(c,bev,2)
    return setmat(c,m)
def mat(name,base,metal=0,rough=.5,coat=0,bump=None,emit=None,estr=0,trans=0,alpha_img=None,aniso=0):
    m=bpy.data.materials.new(name); m.use_nodes=True; nt=m.node_tree; b=nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value=tuple(base)+(1,)
    b.inputs['Metallic'].default_value=metal; b.inputs['Roughness'].default_value=rough; b.inputs['Coat Weight'].default_value=coat
    if aniso: b.inputs['Anisotropic'].default_value=aniso
    if trans: b.inputs['Transmission Weight'].default_value=trans; b.inputs['IOR'].default_value=1.45
    if emit: b.inputs['Emission Color'].default_value=tuple(emit)+(1,); b.inputs['Emission Strength'].default_value=estr
    if alpha_img:
        t=nt.nodes.new('ShaderNodeTexImage'); t.image=bpy.data.images.load(alpha_img); nt.links.new(t.outputs['Alpha'],b.inputs['Alpha'])
    if bump:
        kind,scale,st=bump; tc=nt.nodes.new('ShaderNodeTexCoord')
        if kind=='knurl':
            sep=nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(tc.outputs['Object'],sep.inputs[0])
            at=nt.nodes.new('ShaderNodeMath'); at.operation='ARCTAN2'; nt.links.new(sep.outputs[2],at.inputs[0]); nt.links.new(sep.outputs[0],at.inputs[1])
            mu=nt.nodes.new('ShaderNodeMath'); mu.operation='MULTIPLY'; mu.inputs[1].default_value=scale; nt.links.new(at.outputs[0],mu.inputs[0])
            si=nt.nodes.new('ShaderNodeMath'); si.operation='SINE'; nt.links.new(mu.outputs[0],si.inputs[0]); src=si.outputs[0]
        elif kind=='weave':
            sep=nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(tc.outputs['Object'],sep.inputs[0])
            def sn(i):
                mu=nt.nodes.new('ShaderNodeMath'); mu.operation='MULTIPLY'; mu.inputs[1].default_value=scale; nt.links.new(sep.outputs[i],mu.inputs[0])
                s=nt.nodes.new('ShaderNodeMath'); s.operation='SINE'; nt.links.new(mu.outputs[0],s.inputs[0]); return s
            a1=sn(0); a2=sn(2); mx=nt.nodes.new('ShaderNodeMath'); mx.operation='MULTIPLY'; nt.links.new(a1.outputs[0],mx.inputs[0]); nt.links.new(a2.outputs[0],mx.inputs[1]); src=mx.outputs[0]
        elif kind=='pebble':
            v=nt.nodes.new('ShaderNodeTexVoronoi'); v.inputs['Scale'].default_value=scale; v.feature='SMOOTH_F1' if hasattr(v,'feature') else v.feature
            nt.links.new(tc.outputs['Object'],v.inputs['Vector']); src=v.outputs['Distance']
        elif kind=='brush':
            n=nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value=scale; n.inputs['Detail'].default_value=4
            mp=nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value=(1,1,120)
            nt.links.new(tc.outputs['Object'],mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'],n.inputs['Vector']); src=n.outputs['Fac']
        else:
            n=nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value=scale; n.inputs['Detail'].default_value=10; n.inputs['Roughness'].default_value=0.7
            nt.links.new(tc.outputs['Object'],n.inputs['Vector']); src=n.outputs['Fac']
        bn=nt.nodes.new('ShaderNodeBump'); bn.inputs['Strength'].default_value=st
        nt.links.new(src,bn.inputs['Height']); nt.links.new(bn.outputs['Normal'],b.inputs['Normal'])
    return m
def img_plane(path,cx,cz,w,h,y,rough=.5,metal=0,emis=0.3):
    p=add(bpy.ops.mesh.primitive_plane_add,size=1,location=(cx,y,cz)); p.scale=(w,h,1); apply_scale(p); p.rotation_euler=(math.radians(90),0,0)
    m=bpy.data.materials.new('img'); m.use_nodes=True; nt=m.node_tree; b=nt.nodes['Principled BSDF']
    t=nt.nodes.new('ShaderNodeTexImage'); t.image=bpy.data.images.load(path); t.interpolation='Cubic'
    nt.links.new(t.outputs['Color'],b.inputs['Base Color']); nt.links.new(t.outputs['Alpha'],b.inputs['Alpha'])
    b.inputs['Roughness'].default_value=rough; b.inputs['Metallic'].default_value=metal
    if emis: nt.links.new(t.outputs['Color'],b.inputs['Emission Color']); b.inputs['Emission Strength'].default_value=emis
    return setmat(p,m)
def wood_mat(c1,c2,fade=None,coat=0.3,rough=0.45,ext=None,vertical=False):
    m=bpy.data.materials.new('wood'); m.use_nodes=True; nt=m.node_tree; b=nt.nodes['Principled BSDF']
    tc=nt.nodes.new('ShaderNodeTexCoord'); mp=nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value=(2.2,1,0.06) if vertical else (0.06,1,2.2); nt.links.new(tc.outputs['Object'],mp.inputs['Vector'])
    g=nt.nodes.new('ShaderNodeTexNoise'); g.inputs['Scale'].default_value=9; g.inputs['Detail'].default_value=14; g.inputs['Roughness'].default_value=0.62; nt.links.new(mp.outputs['Vector'],g.inputs['Vector'])
    ramp=nt.nodes.new('ShaderNodeValToRGB'); cr=ramp.color_ramp; cr.elements[0].position=0.3; cr.elements[0].color=tuple(c1)+(1,); cr.elements[1].position=0.62; cr.elements[1].color=tuple(c2)+(1,)
    nt.links.new(g.outputs['Fac'],ramp.inputs['Fac'])
    if fade:
        gsrc=tc.outputs['Generated']
        if ext:  # one continuous blend across the whole zone (world space), not per plank
            x0,iw,z0,zh=ext; ge=nt.nodes.new('ShaderNodeNewGeometry'); gm=nt.nodes.new('ShaderNodeMapping')
            gm.inputs['Scale'].default_value=(1/iw,0,1/zh); gm.inputs['Location'].default_value=(-x0/iw,0.5,-z0/zh)
            nt.links.new(ge.outputs['Position'],gm.inputs['Vector']); gsrc=gm.outputs['Vector']
        sp=nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(gsrc,sp.inputs[0])
        bl=nt.nodes.new('ShaderNodeTexNoise'); bl.inputs['Scale'].default_value=1.2; bl.inputs['Detail'].default_value=3; nt.links.new(gsrc,bl.inputs['Vector'])
        ad=nt.nodes.new('ShaderNodeMath'); ad.operation='MULTIPLY_ADD'; ad.inputs[1].default_value=0.35; nt.links.new(bl.outputs['Fac'],ad.inputs[0]); nt.links.new(sp.outputs[0],ad.inputs[2])
        fr=nt.nodes.new('ShaderNodeValToRGB'); fc=fr.color_ramp; fc.elements[0].position=fade[0]; fc.elements[0].color=(0,0,0,1); fc.elements[1].position=fade[1]; fc.elements[1].color=(1,1,1,1)
        nt.links.new(ad.outputs[0],fr.inputs['Fac'])
        mx=nt.nodes.new('ShaderNodeMix'); mx.data_type='RGBA'; mx.blend_type='MULTIPLY'; mx.inputs[0].default_value=1.0
        nt.links.new(ramp.outputs['Color'],mx.inputs[6]); nt.links.new(fr.outputs['Color'],mx.inputs[7]); nt.links.new(mx.outputs[2],b.inputs['Base Color'])
    else: nt.links.new(ramp.outputs['Color'],b.inputs['Base Color'])
    b.inputs['Roughness'].default_value=rough; b.inputs['Coat Weight'].default_value=coat; b.inputs['Coat Roughness'].default_value=0.08
    return m

def swirl_mat(c1,c2,sat=0.6,bright=0.8,coat=0.4,rough=0.4):
    # even colour across the board with soft lacquer swirl, no wood fibre and no side fade
    lum=0.3*c2[0]+0.59*c2[1]+0.11*c2[2]
    c2=[(lum+(c-lum)*sat)*bright for c in c2]
    lo=[c*0.55 for c in c2]; hi=[min(1,c*1.25) for c in c2]
    m=bpy.data.materials.new('swirl'); m.use_nodes=True; nt=m.node_tree; b=nt.nodes['Principled BSDF']
    tc=nt.nodes.new('ShaderNodeTexCoord')
    mp=nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value=(0.35,1,0.6); nt.links.new(tc.outputs['Object'],mp.inputs['Vector'])
    sw=nt.nodes.new('ShaderNodeTexNoise'); sw.inputs['Scale'].default_value=1.6; sw.inputs['Detail'].default_value=6; sw.inputs['Roughness'].default_value=0.5; sw.inputs['Distortion'].default_value=2.4
    nt.links.new(mp.outputs['Vector'],sw.inputs['Vector'])
    rp=nt.nodes.new('ShaderNodeValToRGB'); cr=rp.color_ramp
    cr.elements[0].position=0.32; cr.elements[0].color=tuple(lo)+(1,); cr.elements[1].position=0.7; cr.elements[1].color=tuple(hi)+(1,)
    e=cr.elements.new(0.5); e.color=tuple(c2)+(1,)
    nt.links.new(sw.outputs['Fac'],rp.inputs['Fac']); nt.links.new(rp.outputs['Color'],b.inputs['Base Color'])
    b.inputs['Roughness'].default_value=rough; b.inputs['Coat Weight'].default_value=coat; b.inputs['Coat Roughness'].default_value=0.06
    b.inputs['Metallic'].default_value=0.25
    return m

GLOWS=[]
def glow_strip(z,color,width,strength=8,y=None,light=1.0):
    y=FRONT-0.14 if y is None else y
    q=box((0,y,z),(width,0.04,0.055),mat('glow',(0,0,0),0,.5,emit=tuple(color),estr=strength),0.01)
    li=add(bpy.ops.object.light_add,type='AREA',location=(0,y-0.05,z-0.05)); li.data.shape='RECTANGLE'; li.data.size=width; li.data.size_y=0.06
    li.data.energy=60*light*width/20; li.data.color=tuple(color); li.rotation_euler=(math.radians(135),0,0); li.visible_camera=False; li.visible_glossy=False

T=S.get('body',{}); BODY=mat('body',T.get('color',[0.022,0.022,0.024]),T.get('metal',0),T.get('rough',.82),bump=tuple(T.get('bump',['leather',42,.55])))
if T.get('wood'): BODY=wood_mat(T['c1'],T['c2'],None,T.get('coat',0.2),T.get('rough',0.5))
CHROME=mat('chrome',(0.9,0.9,0.92),1,.12)
DARKM=mat('darkm',(0.03,0.03,0.032),0.85,.38)
KNOBM={'black':mat('kb',(0.012,0.012,0.014),0,.36,bump=('knurl',36,.4)),'alu':mat('ka',(0.7,0.7,0.73),1,.3,bump=('knurl',36,.3)),'chicken':mat('kc',(0.015,0.015,0.015),0,.3),'cream':mat('kcr',(0.8,0.74,0.6),0,.3),'white':mat('kw',(0.8,0.79,0.76),0,.5,bump=('knurl',36,.25)),'iron':mat('ki',(0.05,0.047,0.045),0.85,.55,bump=('noise',30,.4)),'silver':mat('ks',(0.62,0.62,0.65),1,.28,bump=('knurl',36,.3))}
KTOP={'black':mat('kt',(0.02,0.02,0.022),0.2,.28),'alu':mat('kat',(0.75,0.75,0.78),1,.25,aniso=.5),'chicken':mat('kct',(0.015,0.015,0.015),0,.3),'cream':mat('kcrt',(0.8,0.74,0.6),0,.3),'white':mat('kwt',(0.82,0.81,0.79),0,.45),'iron':mat('kit',(0.06,0.056,0.054),0.85,.5),'silver':mat('kst',(0.7,0.7,0.73),1,.22,aniso=.4)}
WHITE=mat('white',(0.95,0.95,0.95),0,.4,emit=(1,1,1),estr=0.5)
INNER=mat('inner',(0.006,0.006,0.007),0,.9)

KMAT_DEF={'gold':((1.0,0.72,0.32),1,.22),'brass':((0.85,0.62,0.3),1,.32),'rosegold':((0.95,0.6,0.5),1,.25),'silver':((0.85,0.85,0.88),1,.18),
 'pewter':((0.42,0.42,0.44),1,.38),'blackchrome':((0.06,0.06,0.065),1,.16),'iron':((0.05,0.047,0.045),0.85,.55),'copper':((0.9,0.45,0.3),1,.3),
 'ivory':((0.82,0.77,0.64),0,.32),'bone':((0.72,0.68,0.58),0,.55),'bakelite':((0.012,0.011,0.011),0,.3),'obsidian':((0.004,0.004,0.006),0,.06),
 'oxblood':((0.16,0.01,0.015),0,.25),'white':((0.82,0.81,0.78),0,.45),'jade':((0.06,0.3,0.18),0,.15),'navy':((0.01,0.02,0.07),0,.2),'bronze':((0.55,0.36,0.18),1,.4)}
_KM={}
def kmat(name):
    if isinstance(name,list): return mat('kc'+str(len(_KM)),tuple(name),0,.3)
    if name not in _KM:
        c,mt,rg=KMAT_DEF[name]; _KM[name]=mat('k_'+name,c,mt,rg,bump=('noise',300,.02) if mt else None)
    return _KM[name]
OP=S.get('open')
if OP:
    CW=OP.get('cheek',0.8); CH=OP.get('chassis_h',3.6)
    WD=wood_mat(OP.get('c1',[0.05,0.025,0.012]),OP.get('c2',[0.3,0.16,0.07]),None,0.0,0.7)
    if CW>0:
        for sx in (-1,1): box((sx*(W/2-CW/2),0,HH/2),(CW,D,HH),WD,0.15)
        box((0,0,0.2),(W-2*CW,D,0.4),WD,0.05); CWD=W-2*CW
    else:
        CWD=W-2*B
        for sx in (-1,1):
            for sy in (-1,1):
                f=add(bpy.ops.mesh.primitive_cylinder_add,vertices=48,radius=0.35,depth=0.4,location=(sx*(CWD/2-0.8),sy*(D/2-1.2),0.2))
                if OP.get('feet'): bevel(f,0.12,4); setmat(f,kmat(OP['feet']))
                else: setmat(f,mat('foot',(0.01,0.01,0.01),0,.8))
    CHM=mat('chassis',OP.get('chassis',[0.55,0.55,0.57]),OP.get('chassis_metal',1),OP.get('chassis_rough',.32),bump=('brush',60,.03),coat=OP.get('chassis_coat',0))
    box((0,FY+0.45+(D-0.9)/2,0.4+CH/2),(CWD,D-0.9,CH),CHM,OP.get('chassis_bevel',0.04))
    if OP.get('deck'):   # polished top deck plate the tubes and transformers sit on
        dk=OP['deck']; box((0,FY+0.45+(D-0.9)/2,0.4+CH+0.04),(CWD-2*dk.get('inset',0.3),D-0.9-2*dk.get('inset',0.3),0.08),kmat(dk.get('mat','silver')),0.035)
    TOPZ=0.4+CH
    GL=mat('glass2',(0.9,0.9,0.95),0,.04,trans=1)
    for t in OP.get('tubes',[]):
        x,y,h,r,col=t['x'],t['y'],t['h'],t['r'],t['color']
        g=add(bpy.ops.mesh.primitive_cylinder_add,vertices=48,radius=r,depth=h,location=(x,y,TOPZ+0.25+h/2)); bevel(g,r*0.6,6); setmat(g,GL)
        pl=add(bpy.ops.mesh.primitive_cylinder_add,vertices=24,radius=r*0.55,depth=h*0.5,location=(x,y,TOPZ+0.25+h*0.45)); setmat(pl,mat('pl2',(0.05,0.05,0.05),0.6,.5,emit=col,estr=t.get('strength',5)))
        sk=add(bpy.ops.mesh.primitive_cylinder_add,vertices=32,radius=r*1.15,depth=0.3,location=(x,y,TOPZ+0.12)); setmat(sk,mat('sk',(0.02,0.02,0.02),0,.5))
        if OP.get('collar'):
            cl=add(bpy.ops.mesh.primitive_torus_add,major_radius=r*1.2,minor_radius=0.07,location=(x,y,TOPZ+0.27)); setmat(cl,kmat(OP['collar']))
        li=add(bpy.ops.object.light_add,type='POINT',location=(x,y,TOPZ+h*0.5)); li.data.energy=t.get('light',40); li.data.color=col; li.data.shadow_soft_size=r
    TRM=mat('trans',(0.012,0.012,0.014),0.3,.5)
    for t in OP.get('trans',[]):
        if t.get('style')=='can':   # round lacquered can with a domed top and a metal band
            rr=min(t['w'],t['d'])/2
            c=add(bpy.ops.mesh.primitive_cylinder_add,vertices=96,radius=rr,depth=t['h'],location=(t['x'],t['y'],TOPZ+t['h']/2)); bevel(c,rr*0.32,8)
            setmat(c,mat('can',tuple(t.get('color',[0.012,0.012,0.014])),t.get('metal',0.2),t.get('rough',.22),coat=1))
            for bz in t.get('bands',[0.16]):
                b=add(bpy.ops.mesh.primitive_torus_add,major_radius=rr*1.005,minor_radius=0.045,location=(t['x'],t['y'],TOPZ+t['h']*bz)); setmat(b,kmat(t.get('band','gold')))
            continue
        box((t['x'],t['y'],TOPZ+t['h']/2),(t['w'],t['d'],t['h']),TRM,0.12)
        for sx in (-1,1): box((t['x']+sx*t['w']*0.38,t['y'],TOPZ+t['h']+0.08),(0.15,t['d']*0.6,0.16),CHROME,0.03)
    body=None
elif S.get('rack'):
  # rack chassis: plain box set just behind the faceplate (no recess cut)
  rb=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(0,(FY+0.08+D/2)/2,HH/2)); rb.scale=(W-0.02,D/2-FY-0.08,HH-0.02); apply_scale(rb); bevel(rb,0.04,3); setmat(rb,BODY)
  body=None
else:
  body=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(0,0,HH/2)); body.scale=(W,D,HH); apply_scale(body)
if body:
  if S.get('chamfer'):   # slanted top-front (JP-2C style): cut the box along a plane from (FY, HH-h) to (FY+run, HH)
    _ch=S['chamfer']; _h,_run=_ch['h'],_ch['run']; _X=W
    _pts=[(FY-0.002,HH-_h),(FY+_run,HH+0.002),(FY+_run,HH+10),(FY-10,HH+10),(FY-10,HH-_h)]   # (y,z) outline of the wedge to remove
    _v=[(-_X,y,z) for (y,z) in _pts]+[(_X,y,z) for (y,z) in _pts]; _n5=len(_pts)
    _f=[tuple(range(_n5))[::-1],tuple(range(_n5,2*_n5))]+[(i,(i+1)%_n5,_n5+(i+1)%_n5,_n5+i) for i in range(_n5)]
    _me=bpy.data.meshes.new('cham'); _me.from_pydata(_v,[],_f); _me.update()
    import bmesh; _bmc=bmesh.new(); _bmc.from_mesh(_me); bmesh.ops.recalc_face_normals(_bmc,faces=_bmc.faces); _bmc.to_mesh(_me); _bmc.free()
    cc=bpy.data.objects.new('cham',_me); bpy.context.scene.collection.objects.link(cc); bpy.context.view_layer.update()
    bm2=body.modifiers.new('cham','BOOLEAN'); bm2.object=cc; bm2.operation='DIFFERENCE'
    bpy.context.view_layer.objects.active=body; bpy.ops.object.modifier_apply(modifier='cham'); bpy.data.objects.remove(cc)
  _rz=S.get('recess_z',[B,HH-B])
  cut=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(0,FY,(_rz[0]+_rz[1])/2)); cut.scale=(W-2*B,DEP*2,_rz[1]-_rz[0]); apply_scale(cut)
  bm=body.modifiers.new('cut','BOOLEAN'); bm.object=cut; bm.operation='DIFFERENCE'
  bpy.context.view_layer.objects.active=body; bpy.ops.object.modifier_apply(modifier='cut'); bpy.data.objects.remove(cut)
  if _os.environ.get('HDBG'): print('DBGPRE',sorted(set((round((body.matrix_world@x.co).y,2),round((body.matrix_world@x.co).z,2)) for x in body.data.vertices)), [m.name for m in body.modifiers])
  bevel(body,S.get('bevel',min(0.28,B*0.4)),6); setmat(body,BODY)
IY=FY+DEP; FRONT=FY+S.get('front_inset',0.42); X0,X1=-(W-2*B)/2,(W-2*B)/2; IW=X1-X0
def slab_front(za,zb,m,front=FRONT,bev=0.02):
    return box((0,(IY+front)/2,(za+zb)/2),(IW-0.06,IY-front,zb-za),m,bev)

def knob(x,z,y,r,v,style):
    km,kt=KNOBM[style],KTOP[style]
    def cyl(rad,dep,yy,m,b=0.03):
        c=add(bpy.ops.mesh.primitive_cylinder_add,vertices=96,radius=rad,depth=dep,location=(x,yy,z)); c.rotation_euler=(math.radians(90),0,0); bevel(c,b,3); return setmat(c,m)
    if style in ('chicken','cream'):
        cyl(r*0.9,0.3,y-0.15,km); a=math.radians(225-270*v)
        p=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+math.cos(a)*r*0.35,y-0.45,z+math.sin(a)*r*0.35)); p.scale=(r*1.7,0.3,r*0.55); p.rotation_euler=(0,-a,0); bevel(p,0.08,4); setmat(p,km)
        l=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+math.cos(a)*r*0.8,y-0.61,z+math.sin(a)*r*0.8)); l.scale=(r*0.6,0.02,0.04); l.rotation_euler=(0,-a,0); setmat(l,WHITE if style=='chicken' else mat('blk',(0.02,0.02,0.02),0,.5)); return
    cyl(r,0.42,y-0.21,km); cyl(r*1.12,0.06,y-0.03,kt,0.02); cyl(r*0.8,0.14,y-0.48,kt,0.05)
    a=math.radians(225-270*v)
    p=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+math.cos(a)*r*0.45,y-0.56,z+math.sin(a)*r*0.45)); p.scale=(r*0.6,0.02,0.045); p.rotation_euler=(0,-a,0)
    setmat(p,mat('ptr',(0.02,0.02,0.02),0,.5) if style in ('alu','white','silver') else WHITE)
import bmesh
def ptr_mat(p):
    if isinstance(p,list): return mat('pg'+str(len(_KM)),(0,0,0),0,.4,emit=tuple(p),estr=6)
    if p in (None,'white'): return WHITE
    if p in KMAT_DEF: return kmat(p)
    return mat('pblk',(0.015,0.015,0.015),0,.5)
def lathe(x,z,y,r,prof,m,nseg=128,mod=None,flat=False):
    bm=bmesh.new(); rings=[]
    for (rf,d,um) in prof:
        ring=[]
        for j in range(nseg):
            th=2*math.pi*j/nseg; mm=mod(th) if (mod and um) else 1.0
            R=r*rf*mm; ring.append(bm.verts.new((x+R*math.cos(th),y-d,z+R*math.sin(th))))
        rings.append(ring)
    for a,b in zip(rings,rings[1:]):
        for j in range(nseg):
            bm.faces.new((a[j],a[(j+1)%nseg],b[(j+1)%nseg],b[j]))
    for ring,dd in ((rings[0],prof[0][1]),(rings[-1],prof[-1][1])):
        if prof[[0,-1][ring is rings[-1]]][0]>1e-4:
            c=bm.verts.new((x,y-dd,z))
            for j in range(nseg): bm.faces.new((ring[j],ring[(j+1)%nseg],c))
    bmesh.ops.remove_doubles(bm,verts=bm.verts,dist=1e-5); bmesh.ops.recalc_face_normals(bm,faces=bm.faces)
    me=bpy.data.meshes.new('kn'); bm.to_mesh(me); bm.free()
    ob=bpy.data.objects.new('kn',me); sc.collection.objects.link(ob)
    bpy.context.view_layer.objects.active=ob; ob.select_set(True)
    if flat: bpy.ops.object.shade_flat()
    else:
        try: bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40))
        except Exception: bpy.ops.object.shade_smooth()
    ob.select_set(False); ob.data.materials.append(m); return ob
def flutes(n,a): return lambda th: 1-a*(0.5+0.5*math.cos(n*th))**3
def scallop(n,a): return lambda th: 1+a*math.cos(n*th)
def polyg(n): return lambda th: math.cos(math.pi/n)/math.cos(((th+math.pi/n)%(2*math.pi/n))-math.pi/n)
def knurl(n=72,a=0.012): return lambda th: 1+a*math.cos(n*th)
KPROF={
 'skirt':([(1.42,0,0),(1.42,0.05,0),(1.36,0.1,0),(1.08,0.15,0),(1.0,0.17,1),(1.0,0.52,1),(0.94,0.6,0),(0.72,0.63,0),(0,0.64,0)],knurl(80,0.018),0.64),
 'witch':([(1.18,0,0),(1.18,0.07,1),(0.95,0.32,1),(0.66,0.6,1),(0.56,0.68,0),(0.3,0.72,0),(0,0.73,0)],flutes(16,0.1),0.73),
 'fluted':([(1.08,0,0),(1.08,0.05,0),(1.0,0.08,1),(1.0,0.5,1),(0.9,0.6,1),(0.62,0.64,0),(0,0.65,0)],flutes(20,0.08),0.65),
 'crown':([(1.0,0,1),(1.0,0.34,1),(0.96,0.41,1),(0.8,0.47,1),(0.62,0.5,0),(0,0.52,0)],scallop(12,0.045),0.52),
 'dome':([(1.0,0,0),(1.0,0.24,0),(0.97,0.28,0),(0.9,0.3,0),(0.9,0.33,0),(0.84,0.36,0),(0.7,0.44,0),(0.45,0.52,0),(0,0.55,0)],None,0.55),
 'hex':([(1.0,0,1),(1.0,0.38,1),(0.92,0.46,1),(0.8,0.48,1),(0,0.49,0)],polyg(6),0.49),
 'octo':([(1.0,0,1),(1.0,0.4,1),(0.9,0.48,1),(0.78,0.5,1),(0,0.51,0)],polyg(8),0.51),
 'barrel':([(1.0,0,0)]+[q for k in range(6) for q in ((1.0,0.04+k*0.1,0),(0.93,0.09+k*0.1,0))]+[(1.0,0.64,0),(0.86,0.7,0),(0,0.71,0)],None,0.71),
 'spire':([(1.18,0,1),(1.18,0.1,1),(1.0,0.13,1),(1.0,0.3,1),(0.82,0.33,1),(0.82,0.47,1),(0.6,0.5,1),(0.28,0.68,1),(0.08,0.76,1),(0,0.78,0)],polyg(8),0.78),
 'ivory':([(0.96,0,0),(0.96,0.3,0),(0.9,0.4,0),(0.75,0.5,0),(0.5,0.57,0),(0,0.6,0)],None,0.6),
 'gemcollar':([(1.06,0,0),(1.06,0.2,0),(1.0,0.24,0),(0.86,0.26,0),(0,0.26,0)],knurl(60,0.02),0.26),
 'knurl':([(1.04,0,0),(1.04,0.04,0),(1.0,0.07,1),(1.0,0.46,1),(0.96,0.52,0),(0.84,0.545,0),(0,0.55,0)],flutes(48,0.035),0.55),
 'skirt2':([(1.22,0,0),(1.22,0.05,0),(1.13,0.1,0),(1.0,0.14,1),(1.0,0.5,1),(0.95,0.555,0),(0.84,0.57,0),(0,0.575,0)],flutes(48,0.03),0.575),
 'cap':([(1.02,0,0),(1.02,0.05,0),(1.0,0.07,0),(1.0,0.48,0),(0.96,0.53,0),(0.86,0.55,0),(0,0.555,0)],None,0.555),
}
def knob2(k,y):
    x,z,r,v=k['x'],k['z'],k['r'],k.get('v',.5); sh=k['shape']; a=math.radians(225-270*v); ca,sa=math.cos(a),math.sin(a)
    m1=kmat(k.get('mat','bakelite')); m2=kmat(k.get('mat2',k.get('mat','bakelite'))); pm=ptr_mat(k.get('ptr','white'))
    if sh=='chicken':
        lathe(x,z,y,r,[(0.95,0,0),(0.95,0.28,0),(0.88,0.32,0),(0,0.32,0)],m1)
        p=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+ca*r*0.35,y-0.46,z+sa*r*0.35)); p.scale=(r*1.75,0.3,r*0.55); p.rotation_euler=(0,-a,0); bevel(p,0.09,5); setmat(p,m1)
        l=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+ca*r*0.82,y-0.615,z+sa*r*0.82)); l.scale=(r*0.6,0.02,0.035); l.rotation_euler=(0,-a,0); setmat(l,pm); p['tag']='ptr'; l['tag']='ptr'; _HOT.append(dict(kind='knob',x=x,y=y-0.62,z=z,r=r*1.3)); return
    prof,mod,depth=KPROF['gemcollar' if sh=='gem' else sh]
    if sh in ('knurl','skirt2','cap'):   # classic knob with a metal insert on the face, fine ring and pointer
        lathe(x,z,y,r,prof,m1,mod=mod)
        ins=add(bpy.ops.mesh.primitive_cylinder_add,vertices=96,radius=r*0.74,depth=0.03,location=(x,y-depth-0.006,z)); ins.rotation_euler=(math.radians(90),0,0); bevel(ins,0.008,2); setmat(ins,m2)
        rg=add(bpy.ops.mesh.primitive_torus_add,major_radius=r*0.62,minor_radius=0.008,location=(x,y-depth-0.022,z)); rg.rotation_euler=(math.radians(90),0,0); setmat(rg,kmat('bakelite') if k.get('mat2') not in ('bakelite','obsidian') else kmat('silver'))
        l=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+ca*r*0.42,y-depth-0.03,z+sa*r*0.42)); l.scale=(r*0.6,0.012,0.035); l.rotation_euler=(0,-a,0); setmat(l,pm); l['tag']='ptr'
        _HOT.append(dict(kind='knob',x=x,y=y-depth,z=z,r=r*1.06)); return
    lathe(x,z,y,r,prof,m1 if sh not in ('barrel',) else m1,mod=mod)
    if sh in ('gem',):
        g=add(bpy.ops.mesh.primitive_ico_sphere_add,subdivisions=1,radius=r*0.82,location=(x,y-0.24,z)); g.scale=(1,0.5,1)
        bpy.ops.object.shade_flat(); gc=k.get('gem',[0.8,0.05,0.08]); setmat(g,mat('gem'+str(len(_KM)),tuple(c*0.6 for c in gc),0,.0,emit=tuple(gc),estr=k.get('gem_glow',0.25),trans=0.85))
        pin=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=0.045,location=(x+ca*r*1.0,y-0.24,z+sa*r*1.0)); bpy.ops.object.shade_smooth(); setmat(pin,pm); return
    if sh=='crown':
        cb=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=r*0.3,location=(x,y-0.5,z)); cb.scale=(1,0.5,1); bpy.ops.object.shade_smooth()
        gc=k.get('gem',[0.7,0.02,0.05]); setmat(cb,mat('cab'+str(len(_KM)),tuple(gc),0,.03,emit=tuple(gc),estr=0.35))
        l=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+ca*r*0.66,y-0.49,z+sa*r*0.66)); l.scale=(r*0.34,0.03,0.045); l.rotation_euler=(0,-a,0); setmat(l,pm); return
    if sh=='barrel':
        cap=add(bpy.ops.mesh.primitive_cylinder_add,vertices=96,radius=r*0.84,depth=0.03,location=(x,y-depth-0.005,z)); cap.rotation_euler=(math.radians(90),0,0); setmat(cap,mat('capb',(0.45,0.45,0.47),1,.35,aniso=.6))
    if sh=='ivory':
        t=add(bpy.ops.mesh.primitive_torus_add,major_radius=r*0.97,minor_radius=0.06,location=(x,y-0.06,z)); t.rotation_euler=(math.radians(90),0,0); bpy.ops.object.shade_smooth(); setmat(t,m2)
        t=add(bpy.ops.mesh.primitive_torus_add,major_radius=r*0.9,minor_radius=0.035,location=(x,y-0.38,z)); t.rotation_euler=(math.radians(90),0,0); bpy.ops.object.shade_smooth(); setmat(t,m2)
    if sh=='dome':
        t=add(bpy.ops.mesh.primitive_torus_add,major_radius=r*0.9,minor_radius=0.03,location=(x,y-0.315,z)); t.rotation_euler=(math.radians(90),0,0); bpy.ops.object.shade_smooth(); setmat(t,m2)
    if sh=='skirt':   # line on the skirt as well as the cap
        l=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+ca*r*1.22,y-0.12,z+sa*r*1.22)); l.scale=(r*0.32,0.02,0.035); l.rotation_euler=(0,-a,0); setmat(l,pm); l['tag']='ptr'
    if sh in ('spire',):
        fin=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+ca*r*0.62,y-0.36,z+sa*r*0.62)); fin.scale=(r*0.55,0.3,0.05); fin.rotation_euler=(0,-a,0); bevel(fin,0.02,2); setmat(fin,pm); return
    if sh=='witch':
        l=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+ca*r*1.05,y-0.06,z+sa*r*1.05)); l.scale=(r*0.22,0.03,0.04); l.rotation_euler=(0,-a,0); setmat(l,pm); l['tag']='ptr'
    # pointer line on the top face
    top=depth+0.005; rr=r*({'witch':0.45,'ivory':0.62,'dome':0.55}.get(sh,0.7))
    l=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+ca*rr*0.55,y-top,z+sa*rr*0.55)); l.scale=(rr*0.9,0.02,0.04); l.rotation_euler=(0,-a,0); setmat(l,pm)
    l['tag']='ptr'; _HOT.append(dict(kind='knob',x=x,y=y-top,z=z,r=r*1.06))

def toggle(x,z,y):
    n=add(bpy.ops.mesh.primitive_cylinder_add,vertices=6,radius=0.17,depth=0.1,location=(x,y-0.05,z)); n.rotation_euler=(math.radians(90),0,0); setmat(n,CHROME)
    bt=add(bpy.ops.mesh.primitive_cylinder_add,vertices=24,radius=0.045,depth=0.5,location=(x,y-0.3,z+0.08)); bt.rotation_euler=(math.radians(72),0,0); setmat(bt,CHROME)
    tip=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=0.07,location=(x,y-0.54,z+0.16)); bpy.ops.object.shade_smooth(); setmat(tip,CHROME)
def jewel(x,z,y,col,r=0.26,strength=30,button=False):
    if button:
        ring=add(bpy.ops.mesh.primitive_torus_add,major_radius=r*1.25,minor_radius=0.05,location=(x,y-0.02,z)); ring.rotation_euler=(math.radians(90),0,0); setmat(ring,mat('rg',col,0,.3,emit=col,estr=strength))
        c=add(bpy.ops.mesh.primitive_cylinder_add,vertices=64,radius=r,depth=0.25,location=(x,y-0.12,z)); c.rotation_euler=(math.radians(90),0,0); bevel(c,0.06,4); setmat(c,mat('pb',(0.02,0.02,0.022),0,.7))
    else:
        bz=add(bpy.ops.mesh.primitive_torus_add,major_radius=r*1.15,minor_radius=0.08,location=(x,y-0.06,z)); bz.rotation_euler=(math.radians(90),0,0); setmat(bz,CHROME)
        j=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=r,location=(x,y-0.08,z)); j.scale=(1,0.6,1); bpy.ops.object.shade_smooth(); setmat(j,mat('jw',col,0,.05,emit=col,estr=strength))
    li=add(bpy.ops.object.light_add,type='POINT',location=(x,y-0.8,z)); li.data.energy=5; li.data.color=col; li.data.shadow_soft_size=0.2
def tubes(zone,za,zb):
    TL=zone.get('tubes',[])
    if not TL: return
    # tubes always sit behind the grille (and its mesh), on a chassis shelf, never through the plate
    th=zone.get('thick',0.08); gback=FRONT+th+(0.32 if zone.get('mesh') else 0.1)
    lo=min(t.get('z',(za+zb)/2)-t.get('h',2.0)/2 for t in TL)-0.22
    rmax=max(t.get('r',0.32) for t in TL)
    ychas=max(IY-0.5-rmax,gback+rmax+0.05)
    shelf=mat('chassis_i',(0.05,0.05,0.055),0.85,.4,bump=('brush',60,.03))
    box((0,(gback+IY)/2,lo-0.07),(IW-0.08,IY-gback-0.02,0.14),shelf,0.01)
    box((0,IY-0.06,(lo+zb)/2),(IW-0.08,0.08,zb-lo),mat('chassis_b',(0.02,0.02,0.022),0.6,.5),0)
    for t in TL:
        x,zc,h,r,col=t['x'],t.get('z',(za+zb)/2),t.get('h',2.0),t.get('r',0.32),t['color']
        y=min(ychas,IY-r-0.1)
        g=add(bpy.ops.mesh.primitive_cylinder_add,vertices=48,radius=r,depth=h,location=(x,y,zc)); bevel(g,r*0.6,6); setmat(g,mat('glass',(0.9,0.9,0.95),0,.04,trans=1))
        pl=add(bpy.ops.mesh.primitive_cylinder_add,vertices=24,radius=r*0.55,depth=h*0.55,location=(x,y,zc)); setmat(pl,mat('plate',(0.05,0.05,0.05),0.6,.5,emit=col,estr=t.get('strength',4)))
        sock=add(bpy.ops.mesh.primitive_cylinder_add,vertices=32,radius=r*1.25,depth=zc-h/2-lo+0.02,location=(x,y,(zc-h/2+lo)/2)); setmat(sock,DARKM)
        li=add(bpy.ops.object.light_add,type='POINT',location=(x,y,zc)); li.data.energy=t.get('light',60); li.data.color=col; li.data.shadow_soft_size=r
def speakers(zone,za,zb):
    CONE=mat('cone',(0.09,0.085,0.08),0,.85,bump=('noise',30,.2));
    for (x,z,r) in zone.get('speakers',[]):
        y=IY-0.15
        rim=add(bpy.ops.mesh.primitive_torus_add,major_radius=r,minor_radius=0.18,location=(x,y-0.1,z)); rim.rotation_euler=(math.radians(90),0,0); setmat(rim,mat('sur',(0.02,0.02,0.02),0,.7))
        c=add(bpy.ops.mesh.primitive_cone_add,vertices=96,radius1=r*0.95,radius2=r*0.22,depth=r*0.45,location=(x,y+r*0.2,z)); c.rotation_euler=(math.radians(90),0,0); setmat(c,CONE)
        dc=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=r*0.24,location=(x,y+r*0.32,z)); dc.scale=(1,0.45,1); bpy.ops.object.shade_smooth(); setmat(dc,mat('dc',(0.05,0.05,0.05),0.3,.4))
        bk=add(bpy.ops.mesh.primitive_torus_add,major_radius=r*1.08,minor_radius=0.09,location=(x,y-0.05,z)); bk.rotation_euler=(math.radians(90),0,0); setmat(bk,DARKM)

def img_tex(nt,path,uv=None):
    t=nt.nodes.new('ShaderNodeTexImage'); t.image=bpy.data.images.load(path); t.image.colorspace_settings.name='Non-Color'; t.extension='EXTEND'; t.interpolation='Cubic'
    if uv is not None: nt.links.new(uv,t.inputs['Vector'])
    return t
def relief(zn,za,zb,cx=0,w=None,ybase=None):
    # real 3D relief: a dense grid displaced toward the camera by a 16-bit heightmap, with optional see-through holes
    depth=zn.get('depth',0.3); res=zn.get('res',34); w=w or (IW-0.04); h=zb-za
    g=add(bpy.ops.mesh.primitive_grid_add,x_subdivisions=int(w*res),y_subdivisions=int(h*res),size=1,location=(cx,FRONT if ybase is None else ybase,(za+zb)/2))
    g.scale=(w,h,1); apply_scale(g); g.rotation_euler=(math.radians(90),0,0)
    tx=bpy.data.textures.new('hm','IMAGE'); tx.image=bpy.data.images.load(zn['height']); tx.image.colorspace_settings.name='Non-Color'; tx.extension='EXTEND'
    md=g.modifiers.new('d','DISPLACE'); md.texture=tx; md.texture_coords='UV'; md.strength=depth; md.mid_level=0; md.direction='NORMAL'
    bpy.context.view_layer.objects.active=g; bpy.ops.object.shade_smooth()
    m=bpy.data.materials.new('relief'); m.use_nodes=True; nt=m.node_tree; b=nt.nodes['Principled BSDF']
    uv=nt.nodes.new('ShaderNodeTexCoord').outputs['UV']; ht=img_tex(nt,zn['height'],uv)
    rp=nt.nodes.new('ShaderNodeValToRGB'); cr=rp.color_ramp
    lo=zn.get('low',[c*zn.get('cavity',0.45) for c in zn['color']]); hi=zn['color']
    cr.elements[0].position=zn.get('lo_at',0.0); cr.elements[0].color=tuple(lo)+(1,); cr.elements[1].position=zn.get('hi_at',0.6); cr.elements[1].color=tuple(hi)+(1,)
    nt.links.new(ht.outputs['Color'],rp.inputs['Fac']); nt.links.new(rp.outputs['Color'],b.inputs['Base Color'])
    if zn.get('metal_hi') is not None:
        mr=nt.nodes.new('ShaderNodeMapRange'); mr.inputs['To Min'].default_value=zn.get('metal',0); mr.inputs['To Max'].default_value=zn['metal_hi']
        mr.inputs['From Min'].default_value=0.15; mr.inputs['From Max'].default_value=0.5
        nt.links.new(ht.outputs['Color'],mr.inputs['Value']); nt.links.new(mr.outputs['Result'],b.inputs['Metallic'])
    else: b.inputs['Metallic'].default_value=zn.get('metal',0)
    b.inputs['Roughness'].default_value=zn.get('rough',0.7)
    if zn.get('grain'):
        nz=nt.nodes.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value=zn['grain'][0]; nz.inputs['Detail'].default_value=10
        bn=nt.nodes.new('ShaderNodeBump'); bn.inputs['Strength'].default_value=zn['grain'][1]; nt.links.new(nz.outputs['Fac'],bn.inputs['Height']); nt.links.new(bn.outputs['Normal'],b.inputs['Normal'])
    if zn.get('emit'):   # plugin: glow that rises in the low parts (magma heat at the crust's edges)
        ec,es,at=zn['emit']
        em=nt.nodes.new('ShaderNodeMapRange'); em.inputs['From Min'].default_value=0.0; em.inputs['From Max'].default_value=at
        em.inputs['To Min'].default_value=1.0; em.inputs['To Max'].default_value=0.0; nt.links.new(ht.outputs['Color'],em.inputs['Value'])
        pw=nt.nodes.new('ShaderNodeMath'); pw.operation='POWER'; pw.inputs[1].default_value=zn.get('emit_pow',2.0); nt.links.new(em.outputs['Result'],pw.inputs[0])
        ms=nt.nodes.new('ShaderNodeMath'); ms.operation='MULTIPLY'; ms.inputs[1].default_value=es; nt.links.new(pw.outputs[0],ms.inputs[0])
        b.inputs['Emission Color'].default_value=tuple(ec)+(1,); nt.links.new(ms.outputs[0],b.inputs['Emission Strength'])
    if zn.get('gloss'):  # plugin: everything above a height gets its own colour / roughness / metal (polished letters)
        gh,gc,gr,gm=zn['gloss']
        st=nt.nodes.new('ShaderNodeMapRange'); st.inputs['From Min'].default_value=gh-0.02; st.inputs['From Max'].default_value=gh+0.02; nt.links.new(ht.outputs['Color'],st.inputs['Value'])
        mx=nt.nodes.new('ShaderNodeMix'); mx.data_type='RGBA'; nt.links.new(st.outputs['Result'],mx.inputs['Factor'])
        nt.links.new(rp.outputs['Color'],mx.inputs[6]); mx.inputs[7].default_value=tuple(gc)+(1,); nt.links.new(mx.outputs[2],b.inputs['Base Color'])
        r2=nt.nodes.new('ShaderNodeMapRange'); r2.inputs['To Min'].default_value=zn.get('rough',0.7); r2.inputs['To Max'].default_value=gr
        r2.inputs['From Min'].default_value=gh-0.02; r2.inputs['From Max'].default_value=gh+0.02; nt.links.new(ht.outputs['Color'],r2.inputs['Value']); nt.links.new(r2.outputs['Result'],b.inputs['Roughness'])
        m3=nt.nodes.new('ShaderNodeMapRange'); m3.inputs['To Min'].default_value=zn.get('metal',0); m3.inputs['To Max'].default_value=gm
        m3.inputs['From Min'].default_value=gh-0.02; m3.inputs['From Max'].default_value=gh+0.02; nt.links.new(ht.outputs['Color'],m3.inputs['Value']); nt.links.new(m3.outputs['Result'],b.inputs['Metallic'])
    if zn.get('holes'):
        hl=img_tex(nt,zn['holes'],uv); inv=nt.nodes.new('ShaderNodeMath'); inv.operation='SUBTRACT'; inv.inputs[0].default_value=1
        nt.links.new(hl.outputs['Color'],inv.inputs[1]); nt.links.new(inv.outputs[0],b.inputs['Alpha'])
    setmat(g,m)
def iron_bits(zn,za,zb):
    IRON=mat('iron',zn.get('iron_color',[0.035,0.033,0.032]),0.85,.62,bump=('noise',18,.35))
    for st in zn.get('straps',[]):
        z=st['z']; hh=st.get('h',0.42); y=FRONT-0.08
        box((0,y,z),(IW-0.06,0.12,hh),IRON,0.03)
        n=int((IW-1.2)/st.get('pitch',1.4))
        for i in range(n+1):
            x=-(IW-1.2)/2+i*(IW-1.2)/n
            r=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=hh*0.22,location=(x,y-0.06,z)); r.scale=(1,0.55,1); bpy.ops.object.shade_smooth(); setmat(r,IRON)
    for (x,z) in zn.get('rings',[]):
        pl=add(bpy.ops.mesh.primitive_cylinder_add,vertices=48,radius=0.34,depth=0.08,location=(x,FRONT-0.16,z)); pl.rotation_euler=(math.radians(90),0,0); bevel(pl,0.03,2); setmat(pl,IRON)
        bo=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=0.1,location=(x,FRONT-0.24,z)); bpy.ops.object.shade_smooth(); setmat(bo,IRON)
        rg=add(bpy.ops.mesh.primitive_torus_add,major_radius=0.42,minor_radius=0.065,location=(x,FRONT-0.26,z-0.4)); rg.rotation_euler=(math.radians(90),0,0); rg.rotation_euler=(math.radians(78),0,0); bpy.ops.object.shade_smooth(); setmat(rg,IRON)
    for lp in zn.get('lockplates',[]):
        x,z,w_,h_=lp
        box((x,FRONT-0.12,z),(w_,0.1,h_),IRON,0.06)
        for dx in (-1,1):
            for dz in (-1,1):
                r=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=0.08,location=(x+dx*(w_/2-0.18),FRONT-0.18,z+dz*(h_/2-0.18))); r.scale=(1,0.6,1); bpy.ops.object.shade_smooth(); setmat(r,IRON)
        kh=box((x,FRONT-0.175,z+0.08),(0.1,0.02,0.1),mat('kh',(0,0,0),0,.9),0.04); kh2=box((x,FRONT-0.175,z-0.1),(0.06,0.02,0.24),mat('kh',(0,0,0),0,.9),0.02)
    pc=zn.get('portcullis')
    if pc:
        x0,x1,z0,z1=pc['box']; yb=pc.get('y',FRONT+0.25); nv=pc.get('nv',9); nh=pc.get('nh',5)
        for i in range(nv):
            x=x0+(x1-x0)*(i+0.5)/nv
            box((x,yb,(z0+z1)/2+0.15),(0.16,0.16,z1-z0-0.3),IRON,0.02)
            sp=add(bpy.ops.mesh.primitive_cone_add,vertices=4,radius1=0.13,radius2=0,depth=0.42,location=(x,yb,z0+0.05)); sp.rotation_euler=(math.pi,0,math.radians(45)); setmat(sp,IRON)
        for j in range(nh):
            z=z0+0.5+(z1-z0-0.8)*j/(nh-1)
            box(((x0+x1)/2,yb-0.1,z),(x1-x0,0.12,0.15),IRON,0.02)
            for i in range(nv):
                x=x0+(x1-x0)*(i+0.5)/nv
                r=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=0.07,location=(x,yb-0.17,z)); bpy.ops.object.shade_smooth(); setmat(r,IRON)
    for tc in zn.get('torches',[]):
        x,z=tc['x'],tc['z']; col=tc.get('color',[1,0.45,0.12]); F=FRONT-tc.get('dy',0)
        box((x,F-0.05,z-0.55),(0.24,0.12,0.8),IRON,0.04)
        for dz in (-0.85,-0.25):
            r_=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=0.05,location=(x,F-0.12,z+dz)); bpy.ops.object.shade_smooth(); setmat(r_,IRON)
        arm=box((x,F-0.32,z-0.3),(0.11,0.5,0.1),IRON,0.02)
        cup=add(bpy.ops.mesh.primitive_cone_add,vertices=24,radius1=0.1,radius2=0.25,depth=0.38,location=(x,F-0.56,z-0.12)); setmat(cup,IRON)
        fl=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=0.16,location=(x,F-0.56,z+0.22)); fl.scale=(1,1,2.0); bpy.ops.object.shade_smooth()
        setmat(fl,mat('flame',(0,0,0),0,.5,emit=tuple(col),estr=tc.get('strength',30)))
        li=add(bpy.ops.object.light_add,type='POINT',location=(x,F-0.8,z+0.3)); li.data.energy=tc.get('light',35); li.data.color=tuple(col); li.data.shadow_soft_size=0.3
    for gr in zn.get('graze',[]):
        li=add(bpy.ops.object.light_add,type='AREA',location=(gr.get('x',0),FRONT-gr.get('out',1.2),za-0.1)); li.data.shape='RECTANGLE'; li.data.size=gr.get('w',IW*0.9); li.data.size_y=0.3
        li.data.energy=gr.get('energy',120); li.data.color=tuple(gr.get('color',[1,0.9,0.8])); li.rotation_euler=(math.radians(gr.get('tilt',-20)),0,0); li.visible_camera=False; li.visible_glossy=False
        li2=add(bpy.ops.object.light_add,type='AREA',location=(gr.get('x',0),FRONT-gr.get('out',1.2),zb+0.1)); li2.data.shape='RECTANGLE'; li2.data.size=gr.get('w',IW*0.9); li2.data.size_y=0.3
        li2.data.energy=gr.get('energy',120)*gr.get('top',0.5); li2.data.color=tuple(gr.get('color',[1,0.9,0.8])); li2.rotation_euler=(math.radians(180+gr.get('tilt',-20)*-1),0,0); li2.visible_camera=False; li2.visible_glossy=False

_ZMARK=[]

for zn in S['zones']:
    _ZMARK.append((zn,set(o.name for o in bpy.data.objects)))
    za,zb=zn['z']; t=zn['type']
    if t in ('panel','plate'):
        m=mat('pnl',zn.get('color',[0.016,0.016,0.018]),zn.get('metal',0),zn.get('rough',.62),bump=tuple(zn.get('bump',['noise',300,.04])) if zn.get('bump',1) else None,aniso=zn.get('aniso',0))
        if zn.get('window'):
            wx0,wx1=zn['window']; ins=zn.get('inset',0.35)
            box(((wx0+wx1)/2,(IY+FRONT+ins)/2,(za+zb)/2),(wx1-wx0,IY-FRONT-ins,zb-za),m,0.02); PY=FRONT+ins-0.005
        else:
            slab_front(za,zb,m,bev=zn.get('slab_bevel',0.02)); PY=FRONT-0.005
            for rl in zn.get('rails',[]):   # rounded metal trim rod along the panel
                rc=add(bpy.ops.mesh.primitive_cylinder_add,vertices=48,radius=rl.get('r',0.07),depth=rl.get('len',IW-0.6),location=(0,FRONT+rl.get('dy',0.0),rl['z'])); rc.rotation_euler=(0,math.radians(90),0); bevel(rc,rl.get('r',0.07)*0.9,6); setmat(rc,kmat(rl.get('mat','gold')))
        if zn.get('art'): img_plane(zn['art'],0,(za+zb)/2,IW-0.1,zb-za,PY-0.002,emis=zn.get('artemis',0.3),metal=zn.get('artmetal',0),rough=zn.get('artrough',.5))
        for k in zn.get('knobs',[]):
            if k.get('shape'): knob2(k,PY)
            else: knob(k['x'],k['z'],PY,k['r'],k.get('v',.5),k.get('style','black'))
        for tg in zn.get('toggles',[]): toggle(tg[0],tg[1],PY)
        for rk in zn.get('rockers',[]):     # rocker power/standby switch with a lit window
            x,z=rk[0],rk[1]; col=rk[2] if len(rk)>2 else [1,0.1,0.05]
            box((x,PY-0.05,z),(0.62,0.1,0.82),mat('rkb',(0.01,0.01,0.011),0.3,.4),0.06)
            rr=box((x,PY-0.16,z+0.06),(0.46,0.14,0.62),mat('rkr',(0.015,0.015,0.016),0,.35),0.05); rr.rotation_euler=(math.radians(-8),0,0)
            box((x,PY-0.24,z+0.2),(0.1,0.02,0.16),mat('rkl',(0,0,0),0,.4,emit=tuple(col),estr=8),0.01)
            box((x,PY-0.235,z-0.12),(0.14,0.02,0.04),WHITE,0.005)
            _HOT.append(dict(kind='jewel',id='power',x=x,y=PY-0.2,z=z,r=0.45))
        for sl in zn.get('sliders',[]):
            box((sl['x'],PY+0.0,sl['z0']+(sl['z1']-sl['z0'])/2),(0.12,0.04,sl['z1']-sl['z0']),mat('slot',(0.002,0.002,0.002),0,.9),0)
            zc=sl['z0']+(sl['z1']-sl['z0'])*sl['v']
            box((sl['x'],PY-0.17,zc),(0.42,0.34,0.2),mat('cap',tuple(sl.get('color',[0.02,0.02,0.02])),sl.get('metal',0),.35),0.05)
            box((sl['x'],PY-0.345,zc),(0.42,0.005,0.03),WHITE,0)
        for jk in zn.get('jacks',[]):
            x,z,kind=jk
            if kind=='jack':
                nut=add(bpy.ops.mesh.primitive_cylinder_add,vertices=6,radius=0.34,depth=0.12,location=(x,PY-0.06,z)); nut.rotation_euler=(math.radians(90),0,0); setmat(nut,CHROME)
                rg=add(bpy.ops.mesh.primitive_cylinder_add,vertices=48,radius=0.24,depth=0.2,location=(x,PY-0.14,z)); rg.rotation_euler=(math.radians(90),0,0); bevel(rg,0.03,2); setmat(rg,CHROME)
                hl=add(bpy.ops.mesh.primitive_cylinder_add,vertices=32,radius=0.13,depth=0.05,location=(x,PY-0.235,z)); hl.rotation_euler=(math.radians(90),0,0); setmat(hl,mat('hole',(0.0,0.0,0.0),0,.9))
            else:
                box((x,PY-0.04,z),(0.62,0.08,0.26),CHROME,0.1); box((x,PY-0.085,z),(0.48,0.02,0.14),mat('hole2',(0.0,0.0,0.0),0,.9),0.06)
        for pp in zn.get('posts',[]):
            c=add(bpy.ops.mesh.primitive_cylinder_add,vertices=32,radius=0.14,depth=0.14,location=(pp[0],PY-0.07,pp[1])); c.rotation_euler=(math.radians(90),0,0); bevel(c,0.03,2); setmat(c,CHROME)
        for j in zn.get('jewels',[]):
            jewel(j['x'],j['z'],PY,tuple(j['color']),j.get('r',0.26),j.get('strength',30),j.get('button',False)); _HOT.append(dict(kind='jewel',id=j.get('id','power'),x=j['x'],y=PY-0.1,z=j['z'],r=j.get('r',0.26)*1.5))
        for sc_ in zn.get('screens',[]):
            x,z,w,h=sc_['x'],sc_['z'],sc_['w'],sc_['h']; bz=sc_.get('bezel',0.18)
            # bezel: gloss black frame proud of the panel, screen set into it, glass on top
            box((x,PY-0.04,z),(w+2*bz,0.12,h+2*bz),mat('bez',(0.004,0.004,0.005),0.2,.08,coat=1),0.03)
            box((x,PY-0.1,z),(w+0.04,0.02,h+0.04),mat('bezin',(0.0,0.0,0.0),0,.6),0.0)
            img_plane(sc_['img'],x,z,w,h,PY-0.112,rough=0.35,metal=0,emis=sc_.get('emis',2.2))
            _HOT.append(dict(kind='screen',id='screen',x=x,y=PY-0.13,z=z,w=w,h=h))
            gl=add(bpy.ops.mesh.primitive_plane_add,size=1,location=(x,PY-0.13,z)); gl.scale=(w+0.02,h+0.02,1); apply_scale(gl); gl.rotation_euler=(math.radians(90),0,0)
            gm=bpy.data.materials.new('scrglass'); gm.use_nodes=True; gb=gm.node_tree.nodes['Principled BSDF']
            gb.inputs['Base Color'].default_value=(0,0,0,1); gb.inputs['Roughness'].default_value=0.04; gb.inputs['Alpha'].default_value=sc_.get('glass',0.12); setmat(gl,gm)
            if sc_.get('light'):
                li=add(bpy.ops.object.light_add,type='AREA',location=(x,PY-0.3,z)); li.data.size=w; li.data.shape='RECTANGLE'; li.data.size_y=h
                li.data.energy=sc_['light']; li.data.color=tuple(sc_.get('light_color',[0.6,0.8,1.0])); li.rotation_euler=(math.radians(90),0,0); li.visible_camera=False; li.visible_glossy=False
        for bt in zn.get('buttons',[]):
            x,z=bt['x'],bt['z']; w,h=bt.get('w',0.5),bt.get('h',0.34)
            box((x,PY+0.0,z),(w+0.08,0.05,h+0.08),mat('bwell',(0.0,0.0,0.0),0,.85),0.03)
            cap=bt.get('cap',[0.02,0.02,0.022])
            box((x,PY-0.09,z),(w,0.2,h),mat('bcap',tuple(cap),bt.get('metal',0.6),bt.get('rough',0.35),bump=('brush',80,.015)),min(w,h)*0.18)
            col=bt.get('led')
            if col:
                on=bt.get('on',False)
                if bt.get('ring'):   # light pipe along the bottom edge of the cap
                    box((x,PY-0.185,z-h/2+0.05),(w*0.62,0.02,0.035),mat('bled',(0.02,0.02,0.02),0,.3,emit=tuple(col),estr=(4 if on else 0.12)),0.01)
                else:
                    d=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=bt.get('ledr',0.045),location=(x,PY-0.02,z+h/2+0.13)); d.scale=(1,0.5,1); bpy.ops.object.shade_smooth()
                    ls_=bt.get('led_str',14.0); setmat(d,mat('dled',(0.03,0.03,0.03),0,.2,emit=tuple(col),estr=(ls_ if on else 0.2))); d['tag']='led'; d['on_str']=float(ls_)
                    _HOT.append(dict(kind='led',id=bt.get('id','b')+'_led',x=x,y=PY-0.04,z=z+h/2+0.13,w=0.2,h=0.2))
            _HOT.append(dict(kind='button',id=bt.get('id','b'),x=x,y=PY-0.19,z=z,w=w,h=h))
        for ld in zn.get('leds',[]):
            lid=None
            if isinstance(ld,dict): lid=ld['id']; ld=[ld['x'],ld['z'],ld['w'],ld['h'],ld['color'],ld.get('st',10)]
            x,z,w,h,col,st=ld
            box((x,PY-0.01,z),(w+0.03,0.03,h+0.03),mat('lw',(0,0,0),0,.6),0)
            sg=box((x,PY-0.035,z),(w,0.03,h),mat('lseg',tuple(c*0.15 for c in col),0,.25,emit=tuple(col),estr=st),0.005)
            if lid: sg['tag']='led'; sg['on_str']=float(st); _HOT.append(dict(kind='led',id=lid,x=x,y=PY-0.05,z=z,w=w+0.12,h=h+0.12))
        for enc in zn.get('encoders',[]):
            x,z,r=enc['x'],enc['z'],enc['r']
            ring=add(bpy.ops.mesh.primitive_torus_add,major_radius=r*1.16,minor_radius=0.035,location=(x,PY-0.02,z)); ring.rotation_euler=(math.radians(90),0,0)
            setmat(ring,mat('encr',(0.02,0.02,0.02),0,.3,emit=tuple(enc.get('ring',[0.4,0.8,1.0])),estr=enc.get('ring_strength',4)))
            lathe(x,z,PY,r,[(1.05,0,0),(1.05,0.05,0),(1.0,0.08,1),(1.0,0.62,1),(0.96,0.68,0),(0.88,0.7,0),(0,0.705,0)],kmat(enc.get('mat','blackchrome')),mod=flutes(64,0.03))
            dimple=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=r*0.16,location=(x+r*0.52*math.cos(math.radians(90)),PY-0.71,z+r*0.52*math.sin(math.radians(90)))); dimple.scale=(1,0.3,1); bpy.ops.object.shade_smooth(); setmat(dimple,mat('dim',(0.0,0.0,0.0),0,.5)); dimple['tag']='ptr'; _HOT.append(dict(kind='knob',x=x,y=PY-0.71,z=z,r=r*1.05))
            cap=add(bpy.ops.mesh.primitive_cylinder_add,vertices=96,radius=r*0.86,depth=0.02,location=(x,PY-0.715,z)); cap.rotation_euler=(math.radians(90),0,0); setmat(cap,mat('encc',(0.5,0.5,0.52),1,.3,aniso=.7))
        for rs in zn.get('screws',[]):
            c=add(bpy.ops.mesh.primitive_cylinder_add,vertices=32,radius=0.1,depth=0.05,location=(rs[0],PY-0.03,rs[1])); c.rotation_euler=(math.radians(90),0,0); bevel(c,0.02,2); setmat(c,DARKM)
            sl=box((rs[0],PY-0.058,rs[1]),(0.11,0.01,0.02),mat('hx',(0,0,0),0,.9),0); sl.rotation_euler=(0,math.radians(rs[2] if len(rs)>2 else 30),0)
        for w in zn.get('wires',[]):
            cu=bpy.data.curves.new('w','CURVE'); cu.dimensions='3D'; cu.bevel_depth=w.get('r',0.05); cu.bevel_resolution=4
            sp=cu.splines.new('BEZIER'); pts=w['pts']; sp.bezier_points.add(len(pts)-1)
            for bp,(x,z,o) in zip(sp.bezier_points,pts):
                bp.co=(x,PY-0.05-o,z); bp.handle_left_type=bp.handle_right_type='AUTO'
            ob=bpy.data.objects.new('wire',cu); sc.collection.objects.link(ob); ob.data.materials.append(mat('wm',w['color'],0,.55,bump=('noise',80,.3)))
    elif t=='wood':
        if zn.get('fade'):
            WM=swirl_mat(zn.get('c1',[0.004,0.01,0.02]),zn.get('c2',[0.1,0.34,0.58]),zn.get('sat',0.6),zn.get('bright',0.8),zn.get('coat',0.4),zn.get('rough',0.4))
            lf=zn.get('lift',0.02); box((0,(IY+FRONT-lf)/2,(za+zb)/2),(IW-0.06,IY-FRONT+lf,zb-za),WM,0.005)  # same width and plane as the control panel below
            fw=zn.get('frame_w',0.13); ins=zn.get('frame_inset',0.26); FR=mat('frame',zn.get('frame_color',[0.03,0.03,0.033]),0.85,.25)
            fx0,fx1,fz0,fz1=X0+ins,X1-ins,za+ins,zb-ins; fy=FRONT-zn.get('frame_d',0.1); fd=IY-fy
            if zn.get('frame'):
              for (cx,cz,sx,sz) in (((fx0+fx1)/2,fz1-fw/2,fx1-fx0,fw),((fx0+fx1)/2,fz0+fw/2,fx1-fx0,fw),(fx0+fw/2,(fz0+fz1)/2,fw,fz1-fz0),(fx1-fw/2,(fz0+fz1)/2,fw,fz1-fz0)):
                box((cx,fy+fd/2,cz),(sx,fd,sz),FR,0.05)
            gc=zn.get('glow',zn.get('c2'))
            if gc: GLOWS.append((za-0.015,[c/max(gc) for c in gc],IW-0.075,zn.get('glow_strength',4)))
        else:
            n=zn.get('planks',2)
            if zn.get('vertical'):   # door-style vertical boards, each with its own grain offset
                pw=(IW-0.06-0.07*(n-1))/n
                for i in range(n):
                    WM=wood_mat(zn.get('c1',[0.004,0.01,0.02]),zn.get('c2',[0.1,0.34,0.58]),None,zn.get('coat',0.3),zn.get('rough',0.45),vertical=True)
                    WM.node_tree.nodes['Mapping'].inputs['Location'].default_value=(i*3.7,0,i*1.3)
                    box((-(IW-0.06)/2+pw/2+i*(pw+0.07),(IY+FRONT)/2,(za+zb)/2),(pw,IY-FRONT,zb-za-0.04),WM,0.04)
            else:
                ph=(zb-za-0.06*(n-1))/n; WM=wood_mat(zn.get('c1',[0.004,0.01,0.02]),zn.get('c2',[0.1,0.34,0.58]),None,zn.get('coat',0.3),zn.get('rough',0.45))
                for i in range(n): box((0,(IY+FRONT)/2,za+ph/2+i*(ph+0.06)),(IW-0.06,IY-FRONT,ph),WM)
    elif t=='cloth':
        CL=mat('cloth',zn.get('color',[0.016,0.016,0.017]),0,.8,bump=('weave',zn.get('weave',60),.6)); CL.node_tree.nodes['Principled BSDF'].inputs['Sheen Weight'].default_value=zn.get('sheen',0.2)
        if zn.get('img'):
            nt=CL.node_tree; b_=nt.nodes['Principled BSDF']; tc_=nt.nodes.new('ShaderNodeTexCoord'); sp_=nt.nodes.new('ShaderNodeSeparateXYZ'); cb_=nt.nodes.new('ShaderNodeCombineXYZ')
            nt.links.new(tc_.outputs['Object'],sp_.inputs[0]); nt.links.new(sp_.outputs[0],cb_.inputs[0]); nt.links.new(sp_.outputs[2],cb_.inputs[1])
            mp_=nt.nodes.new('ShaderNodeMapping'); mp_.inputs['Scale'].default_value=(zn.get('imgscale',0.5),zn.get('imgscale',0.5),1); nt.links.new(cb_.outputs[0],mp_.inputs['Vector'])
            ti=nt.nodes.new('ShaderNodeTexImage'); ti.image=bpy.data.images.load(zn['img']); nt.links.new(mp_.outputs['Vector'],ti.inputs['Vector']); nt.links.new(ti.outputs['Color'],b_.inputs['Base Color'])
        slab_front(za,zb,CL,FRONT+0.02,0.01)
        pc=zn.get('piping')
        if pc:
            PM=mat('pipe',pc,0.3,.35); inset=0.25; yy=FRONT+0.05
            for zz in (za+inset,zb-inset):
                c=add(bpy.ops.mesh.primitive_cylinder_add,vertices=16,radius=0.05,depth=IW-2*inset,location=(0,yy,zz)); c.rotation_euler=(0,math.radians(90),0); setmat(c,PM)
            for xx in (X0+inset,X1-inset):
                c=add(bpy.ops.mesh.primitive_cylinder_add,vertices=16,radius=0.05,depth=zb-za-2*inset,location=(xx,yy,(za+zb)/2)); setmat(c,PM)
        if zn.get('speakers'):
            pass
    elif t=='bars':
        box((0,IY-0.02,(za+zb)/2),(IW-0.02,0.04,zb-za),INNER,0)
        zn.setdefault('thick',0.0); tubes(zn,za,zb)
        for amb in zn.get('ambient',[]):
            li=add(bpy.ops.object.light_add,type='AREA',location=(amb['x'],(IY+FRONT)/2+0.2,(za+zb)/2)); li.data.size=amb.get('size',3); li.data.energy=amb['energy']; li.data.color=amb['color']; li.rotation_euler=(math.radians(90),0,0); li.visible_camera=False; li.visible_glossy=False
        if zn.get('backglow'):
            bg=add(bpy.ops.mesh.primitive_plane_add,size=1,location=(0,IY-0.06,(za+zb)/2)); bg.scale=(IW-0.1,zb-za,1); apply_scale(bg); bg.rotation_euler=(math.radians(90),0,0)
            setmat(bg,mat('bglow',(0,0,0),0,.6,emit=tuple(zn['backglow']),estr=zn.get('backglow_strength',1.5)))
        BM=mat('barm',zn.get('color',[0.02,0.02,0.022]),zn.get('metal',0.6),zn.get('rough',0.4),bump=('brush',60,.02))
        nb=zn.get('n',9); pad=zn.get('pad',0.25); bh=zn.get('bh',0.3); bd=zn.get('bd',0.32)
        for i in range(nb):
            z=za+pad+bh/2+(zb-za-2*pad-bh)*i/(nb-1)
            segs=[(X0+0.12,X1-0.12)]
            for g0,g1 in zn.get('gaps',[]):
                if g1[0]<=z<=g1[1]:
                    segs=[q for sg in segs for q in ((sg[0],min(sg[1],g0[0])),(max(sg[0],g0[1]),sg[1])) if q[1]-q[0]>0.05]
            for (a,b) in segs:
                box(((a+b)/2,FRONT-bd/2+0.15,z),(b-a,bd,bh),BM,min(bh,bd)*0.48)
        for sx in zn.get('posts_x',[]):
            box((sx,FRONT+0.25,(za+zb)/2),(0.12,0.2,zb-za-0.1),BM,0.03)
    elif t=='relief':
        if zn.get('glass'):   # stained glass lit from behind, seen through the holes
            gp=add(bpy.ops.mesh.primitive_plane_add,size=1,location=(0,FRONT+zn.get('glass_y',0.15),(za+zb)/2)); gp.scale=(IW-0.04,zb-za,1); apply_scale(gp); gp.rotation_euler=(math.radians(90),0,0)
            gm=bpy.data.materials.new('glass_sg'); gm.use_nodes=True; gt=gm.node_tree; gb=gt.nodes['Principled BSDF']
            ti=gt.nodes.new('ShaderNodeTexImage'); ti.image=bpy.data.images.load(zn['glass']); ti.extension='EXTEND'
            gt.links.new(gt.nodes.new('ShaderNodeTexCoord').outputs['UV'],ti.inputs['Vector'])
            if zn.get('glass_base'): gb.inputs['Base Color'].default_value=tuple(zn['glass_base'])+(1,)
            else: gt.links.new(ti.outputs['Color'],gb.inputs['Base Color'])
            gt.links.new(ti.outputs['Color'],gb.inputs['Emission Color']); gb.inputs['Emission Strength'].default_value=zn.get('glass_strength',4); gb.inputs['Roughness'].default_value=0.4
            setmat(gp,gm)
            box((0,IY-0.02,(za+zb)/2),(IW-0.02,0.04,zb-za),INNER,0)
            for gl in zn.get('glass_lights',[]):
                li=add(bpy.ops.object.light_add,type='AREA',location=(gl['x'],FRONT-1.2,gl.get('z',(za+zb)/2))); li.data.size=gl.get('size',3); li.data.energy=gl.get('energy',30); li.data.color=gl['color']; li.rotation_euler=(math.radians(90),0,0); li.visible_camera=False; li.visible_glossy=False
        elif zn.get('holes'):
            box((0,IY-0.02,(za+zb)/2),(IW-0.02,0.04,zb-za),INNER,0)
            zn.setdefault('thick',0.0); tubes(zn,za,zb)
            for amb in zn.get('ambient',[]):
                li=add(bpy.ops.object.light_add,type='AREA',location=(amb['x'],(IY+FRONT)/2+0.2,(za+zb)/2)); li.data.size=amb.get('size',3); li.data.energy=amb['energy']; li.data.color=amb['color']; li.rotation_euler=(math.radians(90),0,0); li.visible_camera=False; li.visible_glossy=False
        else:
            slab_front(za,zb,mat('rback',[c*0.5 for c in zn['color']],0,.8))
        relief(zn,za,zb)
    elif t in ('louver','perf'):
        box((0,IY-0.02,(za+zb)/2),(IW-0.02,0.04,zb-za),INNER,0)
        tubes(zn,za,zb); speakers(zn,za,zb)
        for amb in zn.get('ambient',[]):
            li=add(bpy.ops.object.light_add,type='AREA',location=(amb['x'],(IY+FRONT)/2+0.2,(za+zb)/2)); li.data.size=amb.get('size',3); li.data.energy=amb['energy']; li.data.color=amb['color']; li.rotation_euler=(math.radians(90),0,0); li.visible_camera=False; li.visible_glossy=False
        if t=='louver':
            SL=mat('slat',zn.get('color',[0.02,0.02,0.022]),zn.get('metal',0.5),zn.get('rough',.45))
            pitch=zn.get('pitch',0.36); n=int((zb-za)/pitch)
            for i in range(n):
                zz=za+pitch*(i+0.5); s=box((0,FRONT+0.2,zz),(IW-0.06,0.08,0.42),SL,0.01); s.rotation_euler=(math.radians(zn.get('angle',55)),0,0)
            for xx in [X0+0.04]+[X0+IW*f for f in zn.get('mullions',[])]+[X1-0.04]:
                box((xx,FRONT+0.2,(za+zb)/2),(0.16,0.5,zb-za),SL,0.01)
        else:
            th=zn.get('thick',0.08)
            p=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(0,FRONT+th/2,(za+zb)/2)); p.scale=(IW-0.04,th,zb-za); apply_scale(p)
            bpy.ops.object.mode_set(mode='OBJECT')
            m=mat('perf',zn.get('color',[0.02,0.02,0.022]),zn.get('metal',0.7),zn.get('rough',.4),bump=('noise',400,.05),alpha_img=None)
            # alpha mask via projected image
            nt=m.node_tree; b=nt.nodes['Principled BSDF']; tc=nt.nodes.new('ShaderNodeTexCoord')
            tx=nt.nodes.new('ShaderNodeTexImage'); tx.image=bpy.data.images.load(zn['mask']); tx.extension='CLIP'
            mp=nt.nodes.new('ShaderNodeMapping'); mp.inputs['Location'].default_value=(0.5,0.5,0); mp.inputs['Scale'].default_value=(1/(IW-0.04),1/(zb-za),1)
            sp=nt.nodes.new('ShaderNodeSeparateXYZ'); cb=nt.nodes.new('ShaderNodeCombineXYZ')
            nt.links.new(tc.outputs['Object'],sp.inputs[0]); nt.links.new(sp.outputs[0],cb.inputs[0]); nt.links.new(sp.outputs[2],cb.inputs[1])
            nt.links.new(cb.outputs[0],mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'],tx.inputs['Vector'])
            inv=nt.nodes.new('ShaderNodeMath'); inv.operation='SUBTRACT'; inv.inputs[0].default_value=1; nt.links.new(tx.outputs['Alpha'],inv.inputs[1]); nt.links.new(inv.outputs[0],b.inputs['Alpha'])
            setmat(p,m); bevel(p,0.02,2)
            if zn.get('backglow'):
                bg=zn['backglow']; q=add(bpy.ops.mesh.primitive_plane_add,size=1,location=(0,FRONT+th+0.06,(za+zb)/2)); q.scale=(IW-0.04,zb-za,1); apply_scale(q); q.rotation_euler=(math.radians(90),0,0)
                gm=bpy.data.materials.new('lava'); gm.use_nodes=True; nt=gm.node_tree; b_=nt.nodes['Principled BSDF']; b_.inputs['Base Color'].default_value=(0,0,0,1)
                tc_=nt.nodes.new('ShaderNodeTexCoord'); nz=nt.nodes.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value=bg.get('scale',0.6); nz.inputs['Detail'].default_value=8
                nt.links.new(tc_.outputs['Object'],nz.inputs['Vector']); rp=nt.nodes.new('ShaderNodeValToRGB'); cr=rp.color_ramp
                cs_=bg.get('colors',[[0.5,0.04,0.0],[1.0,0.28,0.02],[1.0,0.75,0.25]])
                cr.elements[0].position=0.35; cr.elements[0].color=tuple(cs_[0])+(1,); cr.elements[1].position=0.68; cr.elements[1].color=tuple(cs_[2])+(1,)
                e=cr.elements.new(0.5); e.color=tuple(cs_[1])+(1,)
                nt.links.new(nz.outputs['Fac'],rp.inputs['Fac']); nt.links.new(rp.outputs['Color'],b_.inputs['Emission Color']); b_.inputs['Emission Strength'].default_value=bg.get('strength',6)
                setmat(q,gm)
            if zn.get('mesh'):
                mm=mat('mesh',(0.02,0.02,0.02),0.8,.4); nt=mm.node_tree; b_=nt.nodes['Principled BSDF']; tc_=nt.nodes.new('ShaderNodeTexCoord')
                sp2=nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(tc_.outputs['Object'],sp2.inputs[0])
                outs=[]
                for ax in (0,1):
                    mu=nt.nodes.new('ShaderNodeMath'); mu.operation='MULTIPLY'; mu.inputs[1].default_value=zn['mesh']*6.283; nt.links.new(sp2.outputs[ax],mu.inputs[0])
                    si=nt.nodes.new('ShaderNodeMath'); si.operation='SINE'; nt.links.new(mu.outputs[0],si.inputs[0])
                    ab=nt.nodes.new('ShaderNodeMath'); ab.operation='ABSOLUTE'; nt.links.new(si.outputs[0],ab.inputs[0])
                    lt=nt.nodes.new('ShaderNodeMath'); lt.operation='LESS_THAN'; lt.inputs[1].default_value=0.3; nt.links.new(ab.outputs[0],lt.inputs[0]); outs.append(lt)
                mx=nt.nodes.new('ShaderNodeMath'); mx.operation='MAXIMUM'; nt.links.new(outs[0].outputs[0],mx.inputs[0]); nt.links.new(outs[1].outputs[0],mx.inputs[1]); nt.links.new(mx.outputs[0],b_.inputs['Alpha'])
                q=add(bpy.ops.mesh.primitive_plane_add,size=1,location=(0,FRONT+th+0.25,(za+zb)/2)); q.scale=(IW-0.04,zb-za,1); apply_scale(q); q.rotation_euler=(math.radians(90),0,0); setmat(q,mm)
    iron_bits(zn,za,zb)
    for pl in zn.get('plates',[]):
        relief(pl,pl['z']-pl['h']/2,pl['z']+pl['h']/2,cx=pl['x'],w=pl['w'],ybase=FRONT-pl.get('lift',0.2))
    for lg in zn.get('logos',[]):
        if lg.get('plate'):
            box((lg['x'],FRONT-0.03 if t!='cloth' else FRONT+0.0,lg['z']),(lg['w']+0.15,0.1,lg['h']+0.15),mat('lp',lg['plate'],lg.get('pm',0.9),lg.get('pr',.25)),0.04)
        img_plane(lg['img'],lg['x'],lg['z'],lg['w'],lg['h'],(FRONT-0.09 if lg.get('plate') else FRONT-0.01),metal=lg.get('metal',0.9),rough=lg.get('rough',.25),emis=lg.get('emis',0.15))

# slanted zones: rotate everything a zone built about its bottom-front edge (top leans back)
_ZEND=set(o.name for o in bpy.data.objects)
for _i,(_zn,_before) in enumerate(_ZMARK):
    if not _zn.get('tilt'): continue
    _after=_ZMARK[_i+1][1] if _i+1<len(_ZMARK) else _ZEND
    _pv=bpy.data.objects.new('tilt%d'%_i,None); sc.collection.objects.link(_pv) if False else bpy.context.scene.collection.objects.link(_pv)
    _pv.location=(0,FRONT,_zn['z'][0]); bpy.context.view_layer.update()
    for _n in _after-_before:
        _o=bpy.data.objects[_n]; _mw=_o.matrix_world.copy(); _o.parent=_pv; _o.matrix_parent_inverse=_pv.matrix_world.inverted(); _o.matrix_world=_mw
    _pv.rotation_euler=(math.radians(-_zn['tilt']),0,0); _pv.location.y+=_zn.get('shift_y',0)
    bpy.context.view_layer.update()

for g in S.get('glow_strips',[]): GLOWS.append((g['z'],g['color'],g.get('w',IW-0.04),g.get('strength',7)))
for (gz,gcol,gw,gs) in GLOWS: glow_strip(gz,gcol,gw,gs,y=FRONT-0.06)
HD=S.get('handle')
if HD:
    hw=HD.get('w',8); hh=HD.get('h',1.1); HM=mat('strap',HD.get('color',[0.015,0.015,0.016]),0,.55,bump=('pebble',60,.3))
    for sx in (-1,1):
        box((sx*hw/2,0,HH+0.12),(0.9,1.1,0.24),DARKM,0.08)
        c=add(bpy.ops.mesh.primitive_cylinder_add,vertices=24,radius=0.2,depth=0.12,location=(sx*hw/2,FY+D/2-0.0,HH+0.3)); setmat(c,DARKM)
    cu=bpy.data.curves.new('h','CURVE'); cu.dimensions='3D'; cu.bevel_depth=0.2; cu.bevel_resolution=6
    sp=cu.splines.new('BEZIER'); pts=[(-hw/2+0.2,HH+0.2),(-hw/2+0.9,HH+hh),(hw/2-0.9,HH+hh),(hw/2-0.2,HH+0.2)]; sp.bezier_points.add(len(pts)-1)
    for bp,(x,z) in zip(sp.bezier_points,pts): bp.co=(x,0,z); bp.handle_left_type=bp.handle_right_type='AUTO'
    ob=bpy.data.objects.new('handle',cu); sc.collection.objects.link(ob); ob.scale=(1,2.2,1); ob.data.materials.append(HM)
cs=S.get('corners')
if cs:
    CM=CHROME if cs=='chrome' else DARKM
    for sx in (-1,1):
        for sz in (0,1):
            cx=sx*(W/2-0.02); cz=HH-0.02 if sz else 0.02
            if cs=='chrome':
                q_=S.get('chrome_size',1.25); o_=q_/2-0.075
                box((sx*(W/2-o_),FY+o_,(HH-o_) if sz else o_),(q_,q_,q_),CM,0.45*q_/1.25)
                continue
            cs_=S.get('corner_size',B+0.35)  # smooth rounded protector wrapping the corner; bottom ones sit on the floor
            c=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(sx*(W/2+0.04-cs_/2),FY-0.04+cs_/2,(HH+0.04-cs_/2) if sz else cs_/2)); c.scale=(cs_,cs_,cs_); apply_scale(c)
            bv=c.modifiers.new('b','BEVEL'); bv.width=cs_*0.3; bv.segments=10; bv.limit_method='NONE'
            bpy.context.view_layer.objects.active=c; bpy.ops.object.shade_smooth(); setmat(c,mat('cap',(0.022,0.022,0.024),0.75,.3))
RK=S.get('rack')
if RK:
    ew=RK.get('ear',0.9); EM=mat('ear',tuple(RK.get('color',[0.02,0.02,0.022])),0.75,.4,bump=('brush',60,.02))
    for sx in (-1,1):
        ex=sx*(W/2+ew/2-0.02)
        box((ex,FY+0.06,HH/2),(ew,0.12,HH),EM,0.03)
        for zz in RK.get('holes',[0.95,HH-0.95]):
            box((ex+sx*0.08,FY-0.005,zz),(0.42,0.02,0.24),mat('slot',(0,0,0),0,.9),0.1)
            c=add(bpy.ops.mesh.primitive_cylinder_add,vertices=32,radius=0.17,depth=0.1,location=(ex+sx*0.08,FY-0.04,zz)); c.rotation_euler=(math.radians(90),0,0); bevel(c,0.03,3); setmat(c,DARKM)
            sl=box((ex+sx*0.08,FY-0.093,zz),(0.2,0.01,0.03),mat('hx2',(0,0,0),0,.9),0)
        # rack handle: bar standing off the ear, bent back at both ends
        hx=ex-sx*0.05; hz0,hz1=0.75,HH-0.75; off=RK.get('handle_off',0.85)
        cu=bpy.data.curves.new('rh','CURVE'); cu.dimensions='3D'; cu.bevel_depth=0.13; cu.bevel_resolution=6
        sp=cu.splines.new('POLY'); P=[(hx,FY,hz0),(hx,FY-off*0.75,hz0),(hx,FY-off,hz0+0.25),(hx,FY-off,hz1-0.25),(hx,FY-off*0.75,hz1),(hx,FY,hz1)]
        sp.points.add(len(P)-1)
        for pt,c3 in zip(sp.points,P): pt.co=(c3[0],c3[1],c3[2],1)
        ob=bpy.data.objects.new('rackh',cu); sc.collection.objects.link(ob); ob.data.materials.append(mat('rhm',tuple(RK.get('handle_color',[0.03,0.03,0.032])),0.9,.28))
        for zz in (hz0,hz1):
            c=add(bpy.ops.mesh.primitive_cylinder_add,vertices=32,radius=0.22,depth=0.08,location=(hx,FY-0.04,zz)); c.rotation_euler=(math.radians(90),0,0); bevel(c,0.03,2); setmat(c,DARKM)
fl=add(bpy.ops.mesh.primitive_plane_add,size=400,location=(0,0,0)); setmat(fl,mat('floor',(0.01,0.01,0.012),0,.35))
w=bpy.data.worlds.new('w'); sc.world=w; w.use_nodes=True; w.node_tree.nodes['Background'].inputs[0].default_value=tuple(S.get('world',[0.008,0.009,0.012]))+(1,)
_PRE_STUDIO=set(o.name for o in bpy.data.objects)
def area(loc,size,energy,color=(1,1,1),tgt=(0,0,HH/2)):
    l=add(bpy.ops.object.light_add,type='AREA',location=loc); l.data.size=size; l.data.energy=energy; l.data.color=color
    l.rotation_euler=(Vector(tgt)-Vector(loc)).to_track_quat('-Z','Y').to_euler()
k=S.get('kscale',1.0)*(max(W,HH)/22)**2
area((-14*W/22,-22*W/22,HH+14),14,4200*k); area((16*W/22,-14*W/22,HH+2),8,1300*k,tuple(S.get('rimcolor',[0.65,0.8,1.0]))); area((0,-30*W/22,3),20,1400*k*S.get('fill',1)); area((0,10,HH+12),12,900*k,(0.7,0.8,1))
cd=bpy.data.cameras.new('c'); cd.lens=S.get('lens',52); cam=bpy.data.objects.new('c',cd); sc.collection.objects.link(cam); sc.camera=cam
dist=S.get('dist',max(W*1.82,HH*1.82*(sc.render.resolution_x/sc.render.resolution_y)/1.0*0.0+0)+0)
cam.location=tuple(S.get('cam',[-0.3*W,-1.97*max(W,HH*sc.render.resolution_x/sc.render.resolution_y*0.0+W)*(1 if sc.render.resolution_x>sc.render.resolution_y else 1.15),HH+2]))
cam.rotation_euler=(Vector(tuple(S.get('target',[0.3,0,HH*0.47])))-cam.location).to_track_quat('-Z','Y').to_euler()
# ---- interactive layers: base (on/off, no pointers, LEDs dim), pointer layer, LED layer ----
if _A.get('out'):
    S['out']=_os.path.join(_ROOT,'out','layers',_KEY,_A['out']+'.png'); _os.makedirs(_os.path.dirname(S['out']),exist_ok=True)
    _LAYER=_A.get('layer'); _OFF=bool(_A.get('off'))
    if _A.get('samples'): sc.cycles.samples=int(_A['samples'])
    if _OFF:
        for o in bpy.data.objects:
            if o.type=='LIGHT' and o.name in _PRE_STUDIO: o.data.energy=0
        for m in bpy.data.materials:
            if not m.use_nodes: continue
            b=m.node_tree.nodes.get('Principled BSDF')
            if m.name.startswith('img') and not (b and b.inputs['Emission Strength'].default_value>1.0): continue   # keep printed art, kill glowing overlays
            if b and b.inputs['Emission Strength'].default_value>0: b.inputs['Emission Strength'].default_value=0
    for o in bpy.data.objects:
        if o.get('tag')=='led':
            o.active_material.node_tree.nodes['Principled BSDF'].inputs['Emission Strength'].default_value=(o['on_str'] if _LAYER=='led' else (0.0 if _OFF else 0.12))
    if _LAYER:
        sc.render.film_transparent=True; sc.render.image_settings.color_mode='RGBA'
        for o in bpy.data.objects:
            if o.type in ('MESH','CURVE') and o.get('tag')!=_LAYER: o.is_holdout=True
    else:
        for o in bpy.data.objects:
            if o.get('tag')=='ptr': o.hide_render=True
    from bpy_extras.object_utils import world_to_camera_view as _w2c
    bpy.context.view_layer.update()
    RX,RY=sc.render.resolution_x,sc.render.resolution_y
    def _px(x,y,z):
        c=_w2c(sc,cam,Vector((x,y,z))); return [round(c.x*RX,2),round((1-c.y)*RY,2)]
    _M=[]
    for h_ in _HOT:
        e=dict(h_); c=_px(h_['x'],h_['y'],h_['z']); e['px']=c
        if 'r' in h_: e['rpx']=round(abs(_px(h_['x']+h_['r'],h_['y'],h_['z'])[0]-c[0]),2)
        if 'w' in h_:
            a_=_px(h_['x']-h_['w']/2,h_['y'],h_['z']+h_['h']/2); b_=_px(h_['x']+h_['w']/2,h_['y'],h_['z']-h_['h']/2); e['box']=[a_[0],a_[1],b_[0],b_[1]]
        _M.append(e)
    json.dump(dict(w=RX,h=RY,hot=_M),open(_os.path.join(_os.path.dirname(S['out']),'map.json'),'w'),indent=1)
if _os.environ.get('HIDEBODY'):
    for o in bpy.data.objects:
        if o.type=='MESH' and o.active_material and o.active_material.name.startswith('body'): o.hide_render=True; print('HID',o.name)
if _os.environ.get('HDBG'):
    for o in bpy.data.objects:
        if o.type=='MESH' and o.active_material and o.active_material.name.startswith('perf'):
            pass
        if o.type=='MESH' and o.active_material and o.active_material.name.startswith('perf'):
            bb=[o.matrix_world@Vector(c) for c in o.bound_box]; print('DBG',o.name,'y',round(min(v.y for v in bb),2),round(max(v.y for v in bb),2),'z',round(min(v.z for v in bb),2),round(max(v.z for v in bb),2),'FY',FY,'FRONT',FRONT,'IY',IY)
    for o in bpy.data.objects:
        if o.type=='MESH' and len(o.data.vertices)>8:
            vs=[o.matrix_world@v.co for v in o.data.vertices]
            hi=[v for v in vs if v.z>6.3]
            if o.active_material and o.active_material.name.startswith('body'):
                for zz in (4.2,4.8,5.4,6.0,6.5):
                    sl=[v for v in vs if abs(v.z-zz)<0.3]
                    if sl: print('DBGB z',zz,'minY',round(min(v.y for v in sl),2))
            if hi and min(v.y for v in hi)<-2.5: print('DBGBODY',o.name,o.active_material.name if o.active_material else None,'minY@z>6.3',round(min(v.y for v in hi),2),len(vs))
    raise SystemExit
sc.render.filepath=S['out']; bpy.ops.render.render(write_still=True); print('RENDERED',S['out'])
