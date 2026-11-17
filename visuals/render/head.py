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
_KEY=_os.path.splitext(_os.path.basename(_SPEC))[0]
_VIEW='front' if 'front' in _ARGS else 'angle'
_os.makedirs(_os.path.join(_ROOT,'out',_VIEW),exist_ok=True)
S['out']=_os.path.join(_ROOT,'out',_VIEW,_KEY+'.png')
if _VIEW=='front':
    _W,_D,_H=S['size']; _a=_m.atan(18/85); _cab=_KEY.endswith('_cab')
    _rx=1400 if _cab else 1800; _ry=max(400,min(int(_rx*_H/_W),2400))
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
def wood_mat(c1,c2,fade=None,coat=0.3,rough=0.45):
    m=bpy.data.materials.new('wood'); m.use_nodes=True; nt=m.node_tree; b=nt.nodes['Principled BSDF']
    tc=nt.nodes.new('ShaderNodeTexCoord'); mp=nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value=(0.06,1,2.2); nt.links.new(tc.outputs['Object'],mp.inputs['Vector'])
    g=nt.nodes.new('ShaderNodeTexNoise'); g.inputs['Scale'].default_value=9; g.inputs['Detail'].default_value=14; g.inputs['Roughness'].default_value=0.62; nt.links.new(mp.outputs['Vector'],g.inputs['Vector'])
    ramp=nt.nodes.new('ShaderNodeValToRGB'); cr=ramp.color_ramp; cr.elements[0].position=0.3; cr.elements[0].color=tuple(c1)+(1,); cr.elements[1].position=0.62; cr.elements[1].color=tuple(c2)+(1,)
    nt.links.new(g.outputs['Fac'],ramp.inputs['Fac'])
    if fade:
        sp=nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(tc.outputs['Generated'],sp.inputs[0])
        bl=nt.nodes.new('ShaderNodeTexNoise'); bl.inputs['Scale'].default_value=1.2; bl.inputs['Detail'].default_value=3; nt.links.new(tc.outputs['Generated'],bl.inputs['Vector'])
        ad=nt.nodes.new('ShaderNodeMath'); ad.operation='MULTIPLY_ADD'; ad.inputs[1].default_value=0.35; nt.links.new(bl.outputs['Fac'],ad.inputs[0]); nt.links.new(sp.outputs[0],ad.inputs[2])
        fr=nt.nodes.new('ShaderNodeValToRGB'); fc=fr.color_ramp; fc.elements[0].position=fade[0]; fc.elements[0].color=(0,0,0,1); fc.elements[1].position=fade[1]; fc.elements[1].color=(1,1,1,1)
        nt.links.new(ad.outputs[0],fr.inputs['Fac'])
        mx=nt.nodes.new('ShaderNodeMix'); mx.data_type='RGBA'; mx.blend_type='MULTIPLY'; mx.inputs[0].default_value=1.0
        nt.links.new(ramp.outputs['Color'],mx.inputs[6]); nt.links.new(fr.outputs['Color'],mx.inputs[7]); nt.links.new(mx.outputs[2],b.inputs['Base Color'])
    else: nt.links.new(ramp.outputs['Color'],b.inputs['Base Color'])
    b.inputs['Roughness'].default_value=rough; b.inputs['Coat Weight'].default_value=coat; b.inputs['Coat Roughness'].default_value=0.08
    return m

T=S.get('body',{}); BODY=mat('body',T.get('color',[0.022,0.022,0.024]),T.get('metal',0),T.get('rough',.82),bump=tuple(T.get('bump',['leather',42,.55])))
if T.get('wood'): BODY=wood_mat(T['c1'],T['c2'],None,T.get('coat',0.2),T.get('rough',0.5))
CHROME=mat('chrome',(0.9,0.9,0.92),1,.12)
DARKM=mat('darkm',(0.03,0.03,0.032),0.85,.38)
KNOBM={'black':mat('kb',(0.012,0.012,0.014),0,.36,bump=('knurl',36,.4)),'alu':mat('ka',(0.7,0.7,0.73),1,.3,bump=('knurl',36,.3)),'chicken':mat('kc',(0.015,0.015,0.015),0,.3),'cream':mat('kcr',(0.8,0.74,0.6),0,.3)}
KTOP={'black':mat('kt',(0.02,0.02,0.022),0.2,.28),'alu':mat('kat',(0.75,0.75,0.78),1,.25,aniso=.5),'chicken':mat('kct',(0.015,0.015,0.015),0,.3),'cream':mat('kcrt',(0.8,0.74,0.6),0,.3)}
WHITE=mat('white',(0.95,0.95,0.95),0,.4,emit=(1,1,1),estr=0.5)
INNER=mat('inner',(0.006,0.006,0.007),0,.9)

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
                f=add(bpy.ops.mesh.primitive_cylinder_add,vertices=32,radius=0.35,depth=0.4,location=(sx*(CWD/2-0.8),sy*(D/2-1.2),0.2)); setmat(f,mat('foot',(0.01,0.01,0.01),0,.8))
    CHM=mat('chassis',OP.get('chassis',[0.55,0.55,0.57]),1,.32,bump=('brush',60,.03))
    box((0,FY+0.45+(D-0.9)/2,0.4+CH/2),(CWD,D-0.9,CH),CHM,0.04)
    TOPZ=0.4+CH
    GL=mat('glass2',(0.9,0.9,0.95),0,.04,trans=1)
    for t in OP.get('tubes',[]):
        x,y,h,r,col=t['x'],t['y'],t['h'],t['r'],t['color']
        g=add(bpy.ops.mesh.primitive_cylinder_add,vertices=48,radius=r,depth=h,location=(x,y,TOPZ+0.25+h/2)); bevel(g,r*0.6,6); setmat(g,GL)
        pl=add(bpy.ops.mesh.primitive_cylinder_add,vertices=24,radius=r*0.55,depth=h*0.5,location=(x,y,TOPZ+0.25+h*0.45)); setmat(pl,mat('pl2',(0.05,0.05,0.05),0.6,.5,emit=col,estr=t.get('strength',5)))
        sk=add(bpy.ops.mesh.primitive_cylinder_add,vertices=32,radius=r*1.15,depth=0.3,location=(x,y,TOPZ+0.12)); setmat(sk,mat('sk',(0.02,0.02,0.02),0,.5))
        li=add(bpy.ops.object.light_add,type='POINT',location=(x,y,TOPZ+h*0.5)); li.data.energy=t.get('light',40); li.data.color=col; li.data.shadow_soft_size=r
    TRM=mat('trans',(0.012,0.012,0.014),0.3,.5)
    for t in OP.get('trans',[]):
        box((t['x'],t['y'],TOPZ+t['h']/2),(t['w'],t['d'],t['h']),TRM,0.12)
        for sx in (-1,1): box((t['x']+sx*t['w']*0.38,t['y'],TOPZ+t['h']+0.08),(0.15,t['d']*0.6,0.16),CHROME,0.03)
    body=None
else:
  body=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(0,0,HH/2)); body.scale=(W,D,HH); apply_scale(body)
if body:
  cut=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(0,FY,HH/2)); cut.scale=(W-2*B,DEP*2,HH-2*B); apply_scale(cut)
  bm=body.modifiers.new('cut','BOOLEAN'); bm.object=cut; bm.operation='DIFFERENCE'
  bpy.context.view_layer.objects.active=body; bpy.ops.object.modifier_apply(modifier='cut'); bpy.data.objects.remove(cut)
  bevel(body,S.get('bevel',min(0.28,B*0.4)),6); setmat(body,BODY)
IY=FY+DEP; FRONT=FY+0.42; X0,X1=-(W-2*B)/2,(W-2*B)/2; IW=X1-X0
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
    setmat(p,mat('ptr',(0.02,0.02,0.02),0,.5) if style=='alu' else WHITE)
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
    for t in zone.get('tubes',[]):
        x,zc,h,r,col=t['x'],t.get('z',(za+zb)/2),t.get('h',2.0),t.get('r',0.32),t['color']
        y=IY-0.5-r
        g=add(bpy.ops.mesh.primitive_cylinder_add,vertices=48,radius=r,depth=h,location=(x,y,zc)); bevel(g,r*0.6,6); setmat(g,mat('glass',(0.9,0.9,0.95),0,.04,trans=1))
        pl=add(bpy.ops.mesh.primitive_cylinder_add,vertices=24,radius=r*0.55,depth=h*0.55,location=(x,y,zc)); setmat(pl,mat('plate',(0.05,0.05,0.05),0.6,.5,emit=col,estr=t.get('strength',4)))
        sock=add(bpy.ops.mesh.primitive_cylinder_add,vertices=32,radius=r*1.1,depth=0.25,location=(x,y,zc-h/2-0.1)); setmat(sock,DARKM)
        li=add(bpy.ops.object.light_add,type='POINT',location=(x,y,zc)); li.data.energy=t.get('light',60); li.data.color=col; li.data.shadow_soft_size=r
def speakers(zone,za,zb):
    CONE=mat('cone',(0.09,0.085,0.08),0,.85,bump=('noise',30,.2));
    for (x,z,r) in zone.get('speakers',[]):
        y=IY-0.15
        rim=add(bpy.ops.mesh.primitive_torus_add,major_radius=r,minor_radius=0.18,location=(x,y-0.1,z)); rim.rotation_euler=(math.radians(90),0,0); setmat(rim,mat('sur',(0.02,0.02,0.02),0,.7))
        c=add(bpy.ops.mesh.primitive_cone_add,vertices=96,radius1=r*0.95,radius2=r*0.22,depth=r*0.45,location=(x,y+r*0.2,z)); c.rotation_euler=(math.radians(90),0,0); setmat(c,CONE)
        dc=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=r*0.24,location=(x,y+r*0.32,z)); dc.scale=(1,0.45,1); bpy.ops.object.shade_smooth(); setmat(dc,mat('dc',(0.05,0.05,0.05),0.3,.4))
        bk=add(bpy.ops.mesh.primitive_torus_add,major_radius=r*1.08,minor_radius=0.09,location=(x,y-0.05,z)); bk.rotation_euler=(math.radians(90),0,0); setmat(bk,DARKM)

for zn in S['zones']:
    za,zb=zn['z']; t=zn['type']
    if t in ('panel','plate'):
        m=mat('pnl',zn.get('color',[0.016,0.016,0.018]),zn.get('metal',0),zn.get('rough',.62),bump=tuple(zn.get('bump',['noise',300,.04])) if zn.get('bump',1) else None,aniso=zn.get('aniso',0))
        slab_front(za,zb,m); PY=FRONT-0.005
        if zn.get('art'): img_plane(zn['art'],0,(za+zb)/2,IW-0.1,zb-za,PY-0.002,emis=zn.get('artemis',0.3),metal=zn.get('artmetal',0),rough=zn.get('artrough',.5))
        for k in zn.get('knobs',[]): knob(k['x'],k['z'],PY,k['r'],k.get('v',.5),k.get('style','black'))
        for tg in zn.get('toggles',[]): toggle(tg[0],tg[1],PY)
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
        for j in zn.get('jewels',[]): jewel(j['x'],j['z'],PY,tuple(j['color']),j.get('r',0.26),j.get('strength',30),j.get('button',False))
        for w in zn.get('wires',[]):
            cu=bpy.data.curves.new('w','CURVE'); cu.dimensions='3D'; cu.bevel_depth=w.get('r',0.05); cu.bevel_resolution=4
            sp=cu.splines.new('BEZIER'); pts=w['pts']; sp.bezier_points.add(len(pts)-1)
            for bp,(x,z,o) in zip(sp.bezier_points,pts):
                bp.co=(x,PY-0.05-o,z); bp.handle_left_type=bp.handle_right_type='AUTO'
            ob=bpy.data.objects.new('wire',cu); sc.collection.objects.link(ob); ob.data.materials.append(mat('wm',w['color'],0,.55,bump=('noise',80,.3)))
    elif t=='wood':
        n=zn.get('planks',2); ph=(zb-za-0.06*(n-1))/n; WM=wood_mat(zn.get('c1',[0.004,0.01,0.02]),zn.get('c2',[0.1,0.34,0.58]),zn.get('fade'),zn.get('coat',0.3),zn.get('rough',0.45))
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
    for lg in zn.get('logos',[]):
        if lg.get('plate'):
            box((lg['x'],FRONT-0.03 if t!='cloth' else FRONT+0.0,lg['z']),(lg['w']+0.15,0.1,lg['h']+0.15),mat('lp',lg['plate'],lg.get('pm',0.9),lg.get('pr',.25)),0.04)
        img_plane(lg['img'],lg['x'],lg['z'],lg['w'],lg['h'],(FRONT-0.09 if lg.get('plate') else FRONT-0.01),metal=lg.get('metal',0.9),rough=lg.get('rough',.25),emis=lg.get('emis',0.15))

cs=S.get('corners')
if cs:
    CM=CHROME if cs=='chrome' else DARKM
    for sx in (-1,1):
        for sz in (0,1):
            cx=sx*(W/2-0.02); cz=HH-0.02 if sz else 0.02
            if cs=='chrome':
                box((sx*(W/2-0.55),FY+0.55,(HH-0.55) if sz else 0.55),(1.25,1.25,1.25),CM,0.45)
                continue
            for (w_,h_) in ((1.3,0.32),(0.32,1.3)):
                ox=cx-sx*w_/2; oz=cz-h_/2 if sz else cz+h_/2
                box((ox,FY-0.06,oz),(w_,0.12,h_),CM,0.04)
fl=add(bpy.ops.mesh.primitive_plane_add,size=400,location=(0,0,0)); setmat(fl,mat('floor',(0.01,0.01,0.012),0,.35))
w=bpy.data.worlds.new('w'); sc.world=w; w.use_nodes=True; w.node_tree.nodes['Background'].inputs[0].default_value=tuple(S.get('world',[0.008,0.009,0.012]))+(1,)
def area(loc,size,energy,color=(1,1,1),tgt=(0,0,HH/2)):
    l=add(bpy.ops.object.light_add,type='AREA',location=loc); l.data.size=size; l.data.energy=energy; l.data.color=color
    l.rotation_euler=(Vector(tgt)-Vector(loc)).to_track_quat('-Z','Y').to_euler()
k=S.get('kscale',1.0)*(max(W,HH)/22)**2
area((-14*W/22,-22*W/22,HH+14),14,4200*k); area((16*W/22,-14*W/22,HH+2),8,1300*k,tuple(S.get('rimcolor',[0.65,0.8,1.0]))); area((0,-30*W/22,3),20,1400*k*S.get('fill',1)); area((0,10,HH+12),12,900*k,(0.7,0.8,1))
cd=bpy.data.cameras.new('c'); cd.lens=S.get('lens',52); cam=bpy.data.objects.new('c',cd); sc.collection.objects.link(cam); sc.camera=cam
dist=S.get('dist',max(W*1.82,HH*1.82*(sc.render.resolution_x/sc.render.resolution_y)/1.0*0.0+0)+0)
cam.location=tuple(S.get('cam',[-0.3*W,-1.97*max(W,HH*sc.render.resolution_x/sc.render.resolution_y*0.0+W)*(1 if sc.render.resolution_x>sc.render.resolution_y else 1.15),HH+2]))
cam.rotation_euler=(Vector(tuple(S.get('target',[0.3,0,HH*0.47])))-cam.location).to_track_quat('-Z','Y').to_euler()
sc.render.filepath=S['out']; bpy.ops.render.render(write_still=True); print('RENDERED',S['out'])
