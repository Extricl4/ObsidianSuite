import bpy, math, sys
from mathutils import Vector
sys.path.insert(0,'/home/claude/r3d')
OUT=sys.argv[sys.argv.index('--')+1]; ART=sys.argv[sys.argv.index('--')+2]
W,D,HH=22.0,9.0,8.6
FY=-D/2
bpy.ops.wm.read_factory_settings(use_empty=True)
sc=bpy.context.scene
sc.render.engine='CYCLES'; sc.cycles.samples=40; sc.cycles.use_adaptive_sampling=True; sc.cycles.use_denoising=True; sc.cycles.device='CPU'
sc.render.resolution_x,sc.render.resolution_y=1560,680
sc.view_settings.view_transform='AgX'; sc.view_settings.look='AgX - Medium High Contrast'

def add(op,**kw): op(**kw); return bpy.context.active_object
def bevel(o,w=0.05,seg=3):
    m=o.modifiers.new('b','BEVEL'); m.width=w; m.segments=seg; m.limit_method='ANGLE'
    bpy.context.view_layer.objects.active=o; bpy.ops.object.shade_smooth(); return o
def setmat(o,m): o.data.materials.clear(); o.data.materials.append(m); return o
def mat(name,base,metal=0,rough=.5,coat=0,bump=None,emit=None,estr=0):
    m=bpy.data.materials.new(name); m.use_nodes=True; nt=m.node_tree; b=nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value=base+(1,) if len(base)==3 else base
    b.inputs['Metallic'].default_value=metal; b.inputs['Roughness'].default_value=rough; b.inputs['Coat Weight'].default_value=coat
    if emit: b.inputs['Emission Color'].default_value=emit+(1,); b.inputs['Emission Strength'].default_value=estr
    if bump:
        kind,scale,st=bump; tc=nt.nodes.new('ShaderNodeTexCoord')
        if kind=='knurl':
            sep=nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(tc.outputs['Object'],sep.inputs[0])
            at=nt.nodes.new('ShaderNodeMath'); at.operation='ARCTAN2'; nt.links.new(sep.outputs[1],at.inputs[0]); nt.links.new(sep.outputs[0],at.inputs[1])
            mu=nt.nodes.new('ShaderNodeMath'); mu.operation='MULTIPLY'; mu.inputs[1].default_value=scale; nt.links.new(at.outputs[0],mu.inputs[0])
            si=nt.nodes.new('ShaderNodeMath'); si.operation='SINE'; nt.links.new(mu.outputs[0],si.inputs[0]); src=si.outputs[0]
        else:
            n=nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value=scale; n.inputs['Detail'].default_value=10
            if kind=='leather': n.inputs['Roughness'].default_value=0.7
            nt.links.new(tc.outputs['Object'],n.inputs['Vector']); src=n.outputs['Fac']
        bn=nt.nodes.new('ShaderNodeBump'); bn.inputs['Strength'].default_value=st
        nt.links.new(src,bn.inputs['Height']); nt.links.new(bn.outputs['Normal'],b.inputs['Normal'])
    return m

def wood_mat():
    m=bpy.data.materials.new('wood'); m.use_nodes=True; nt=m.node_tree; b=nt.nodes['Principled BSDF']
    tc=nt.nodes.new('ShaderNodeTexCoord')
    mp=nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value=(0.06,1,2.2); nt.links.new(tc.outputs['Object'],mp.inputs['Vector'])
    g=nt.nodes.new('ShaderNodeTexNoise'); g.inputs['Scale'].default_value=9; g.inputs['Detail'].default_value=14; g.inputs['Roughness'].default_value=0.62
    nt.links.new(mp.outputs['Vector'],g.inputs['Vector'])
    bl=nt.nodes.new('ShaderNodeTexNoise'); bl.inputs['Scale'].default_value=0.55; bl.inputs['Detail'].default_value=3
    mp2=nt.nodes.new('ShaderNodeMapping'); mp2.inputs['Scale'].default_value=(0.35,1,1.2); nt.links.new(tc.outputs['Object'],mp2.inputs['Vector']); nt.links.new(mp2.outputs['Vector'],bl.inputs['Vector'])
    br=nt.nodes.new('ShaderNodeMapRange'); br.inputs['From Min'].default_value=0.3; br.inputs['From Max'].default_value=0.6
    nt.links.new(bl.outputs['Fac'],br.inputs['Value'])
    mu=nt.nodes.new('ShaderNodeMath'); mu.operation='MULTIPLY'; nt.links.new(g.outputs['Fac'],mu.inputs[0]); nt.links.new(br.outputs['Result'],mu.inputs[1])
    ramp=nt.nodes.new('ShaderNodeValToRGB'); cr=ramp.color_ramp
    cr.elements[0].position=0.18; cr.elements[0].color=(0.002,0.005,0.01,1)
    cr.elements[1].position=0.55; cr.elements[1].color=(0.10,0.34,0.58,1)
    e=cr.elements.new(0.40); e.color=(0.012,0.045,0.09,1)
    nt.links.new(mu.outputs[0],ramp.inputs['Fac']); nt.links.new(ramp.outputs['Color'],b.inputs['Base Color'])
    b.inputs['Roughness'].default_value=0.8; b.inputs['Coat Weight'].default_value=0.0
    bn=nt.nodes.new('ShaderNodeBump'); bn.inputs['Strength'].default_value=0.25; nt.links.new(g.outputs['Fac'],bn.inputs['Height']); nt.links.new(bn.outputs['Normal'],b.inputs['Normal'])
    return m

TOLEX=mat('tolex',(0.022,0.022,0.024),0,.82,bump=('leather',42,.55))
PANEL=mat('panel',(0.016,0.016,0.018),0,.62,bump=('noise',300,.04))
CHROME=mat('chrome',(0.9,0.9,0.92),1,.12)
CORNER=mat('corner',(0.03,0.03,0.032),0.85,.38)
KNOB=mat('knob',(0.012,0.012,0.014),0,.36,bump=('knurl',36,.4))
KTOP=mat('ktop',(0.02,0.02,0.022),0.2,.28)
WHITE=mat('white',(0.95,0.95,0.95),0,.4,emit=(1,1,1),estr=0.5)

# body with recessed front
body=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(0,0,HH/2)); body.scale=(W,D,HH); bpy.ops.object.transform_apply(scale=True)
cut=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(0,FY,HH/2)); cut.scale=(W-1.5,0.9,HH-1.5); bpy.ops.object.transform_apply(scale=True)
bm=body.modifiers.new('cut','BOOLEAN'); bm.object=cut; bm.operation='DIFFERENCE'
bpy.context.view_layer.objects.active=body; bpy.ops.object.modifier_apply(modifier='cut'); bpy.data.objects.remove(cut)
bevel(body,0.28,6); setmat(body,TOLEX)
IY=FY+0.45  # inner face
x0,x1,z0,z1=-(W-1.5)/2,(W-1.5)/2,0.75,HH-0.75
# wood planks
WOOD=wood_mat(); split=4.15
ph=(z1-split-0.06)/2
for i in range(2):
    zc=split+0.03+ph/2+i*(ph+0.06)
    p=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(0,IY-0.09,zc)); p.scale=(x1-x0-0.08,0.18,ph); bpy.ops.object.transform_apply(scale=True); bevel(p,0.03,2); setmat(p,WOOD)
for (sx,sz) in ((x0+0.35,z1-0.3),(x1-0.35,z1-0.3),(x0+0.35,split+0.35),(x1-0.35,split+0.35)):
    s=add(bpy.ops.mesh.primitive_cylinder_add,vertices=32,radius=0.09,depth=0.06,location=(sx,IY-0.2,sz)); s.rotation_euler=(math.radians(90),0,0); setmat(s,CHROME)
# control panel
pz0,pz1=z0,split-0.03
pn=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(0,IY-0.06,(pz0+pz1)/2)); pn.scale=(x1-x0-0.08,0.12,pz1-pz0); bpy.ops.object.transform_apply(scale=True); bevel(pn,0.02,2); setmat(pn,PANEL)
PY=IY-0.125
art=add(bpy.ops.mesh.primitive_plane_add,size=1,location=(0,PY-0.002,(pz0+pz1)/2)); art.scale=(x1-x0-0.1,pz1-pz0,1); bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
art.rotation_euler=(math.radians(90),0,0)
am=bpy.data.materials.new('art'); am.use_nodes=True; nt=am.node_tree; b=nt.nodes['Principled BSDF']
t=nt.nodes.new('ShaderNodeTexImage'); t.image=bpy.data.images.load(ART); t.interpolation='Cubic'
nt.links.new(t.outputs['Color'],b.inputs['Base Color']); nt.links.new(t.outputs['Alpha'],b.inputs['Alpha']); b.inputs['Roughness'].default_value=0.5
nt.links.new(t.outputs['Color'],b.inputs['Emission Color']); b.inputs['Emission Strength'].default_value=0.35
setmat(art,am)
KZ=1.9
def knob(x,r,v):
    c=add(bpy.ops.mesh.primitive_cylinder_add,vertices=96,radius=r,depth=0.42,location=(x,PY-0.21,KZ)); c.rotation_euler=(math.radians(90),0,0); bevel(c,0.03,3); setmat(c,KNOB)
    sk=add(bpy.ops.mesh.primitive_cylinder_add,vertices=96,radius=r*1.12,depth=0.06,location=(x,PY-0.03,KZ)); sk.rotation_euler=(math.radians(90),0,0); bevel(sk,0.02,2); setmat(sk,KTOP)
    tp=add(bpy.ops.mesh.primitive_cylinder_add,vertices=96,radius=r*0.8,depth=0.14,location=(x,PY-0.48,KZ)); tp.rotation_euler=(math.radians(90),0,0); bevel(tp,0.05,4); setmat(tp,KTOP)
    a=math.radians(225-270*v)
    pl=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(x+math.cos(a)*r*0.45,PY-0.56,KZ+math.sin(a)*r*0.45)); pl.scale=(r*0.6,0.02,0.045); pl.rotation_euler=(0,-a,0); setmat(pl,WHITE)
for x,r,v in ((-4.37,0.62,0.62),(-2.71,0.38,0.45),(-1.54,0.38,0.5),(-0.37,0.38,0.55),(1.38,0.38,0.4),(2.57,0.38,0.6),(4.43,0.62,0.45)): knob(x,r,v)
def toggle(x):
    n=add(bpy.ops.mesh.primitive_cylinder_add,vertices=6,radius=0.17,depth=0.1,location=(x,PY-0.05,KZ)); n.rotation_euler=(math.radians(90),0,0); setmat(n,CHROME)
    bt=add(bpy.ops.mesh.primitive_cylinder_add,vertices=24,radius=0.045,depth=0.5,location=(x,PY-0.3,KZ+0.08)); bt.rotation_euler=(math.radians(72),0,0); setmat(bt,CHROME)
    tip=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=0.07,location=(x,PY-0.54,KZ+0.16)); bpy.ops.object.shade_smooth(); setmat(tip,CHROME)
toggle(7.49); toggle(8.37)
jx=9.38
bz=add(bpy.ops.mesh.primitive_torus_add,major_radius=0.3,minor_radius=0.08,location=(jx,PY-0.06,KZ)); bz.rotation_euler=(math.radians(90),0,0); setmat(bz,CHROME)
j=add(bpy.ops.mesh.primitive_uv_sphere_add,radius=0.26,location=(jx,PY-0.08,KZ)); j.scale=(1,0.6,1); bpy.ops.object.shade_smooth()
setmat(j,mat('jewel',(0.6,0.75,1.0),0,.05,emit=(0.55,0.72,1.0),estr=30))
li=add(bpy.ops.object.light_add,type='POINT',location=(jx,PY-0.8,KZ)); li.data.energy=6; li.data.color=(0.5,0.7,1); li.data.shadow_soft_size=0.2
# L corners
for sx in (-1,1):
    for sz in (0,1):
        cx=sx*(W/2-0.02); cz=HH-0.02 if sz else 0.02
        for (w_,h_) in ((1.3,0.32),(0.32,1.3)):
            ox=cx - sx*w_/2; oz=cz - h_/2 if sz else cz + h_/2
            c=add(bpy.ops.mesh.primitive_cube_add,size=1,location=(ox,FY-0.06,oz)); c.scale=(w_,0.12,h_); bpy.ops.object.transform_apply(scale=True); bevel(c,0.04,3); setmat(c,CORNER)
        s=add(bpy.ops.mesh.primitive_cylinder_add,vertices=24,radius=0.07,depth=0.04,location=(cx-sx*0.16,FY-0.13,cz+(-0.16 if sz else 0.16))); s.rotation_euler=(math.radians(90),0,0); setmat(s,CHROME)
# floor, world, lights
fl=add(bpy.ops.mesh.primitive_plane_add,size=300,location=(0,0,0)); setmat(fl,mat('floor',(0.01,0.01,0.012),0,.35))
w=bpy.data.worlds.new('w'); sc.world=w; w.use_nodes=True; w.node_tree.nodes['Background'].inputs[0].default_value=(0.008,0.009,0.012,1)
def area(loc,size,energy,color=(1,1,1),tgt=(0,0,4.3)):
    l=add(bpy.ops.object.light_add,type='AREA',location=loc); l.data.size=size; l.data.energy=energy; l.data.color=color
    l.rotation_euler=(Vector(tgt)-Vector(loc)).to_track_quat('-Z','Y').to_euler()
area((-14,-22,22),14,4200); area((16,-14,10),8,1300,(0.65,0.8,1.0)); area((0,-30,3),20,1400); area((0,10,20),12,900,(0.7,0.8,1))
cd=bpy.data.cameras.new('c'); cd.lens=52; cam=bpy.data.objects.new('c',cd); sc.collection.objects.link(cam); sc.camera=cam
cam.location=(-6.5,-40,10.5); cam.rotation_euler=(Vector((0.3,0,4.0))-cam.location).to_track_quat('-Z','Y').to_euler()
sc.render.filepath=OUT; bpy.ops.render.render(write_still=True); print('RENDERED',OUT)
