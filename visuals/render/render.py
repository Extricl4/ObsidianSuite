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
spec=_fix(json.load(open(_SPEC)))
_KEY=_os.path.splitext(_os.path.basename(_SPEC))[0]
_os.makedirs(_os.path.join(_ROOT,'out','pedals'),exist_ok=True)
spec['out']=_os.path.join(_ROOT,'out','pedals',_KEY+'.png')
if 'prev' in _ARGS:
    spec['samples']=6; _r=spec.get('res',[780,1170]); spec['res']=[_r[0]//2,_r[1]//2]; spec['out']=_os.path.join(_ROOT,'out','preview','pedal_'+_KEY+'.png'); _os.makedirs(_os.path.dirname(spec['out']),exist_ok=True)

W, L, H = spec.get('size', [6.6, 12.0, 3.0])
PX = spec.get('px', 200)
def P(px, py): return ((px/PX) - W/2, L/2 - (py/PX))

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.samples = spec.get('samples', 24); sc.cycles.use_adaptive_sampling = True; sc.cycles.use_denoising = True
sc.cycles.device = 'CPU'
sc.render.resolution_x, sc.render.resolution_y = spec.get('res', [780, 1170]); sc.render.film_transparent = False
sc.view_settings.view_transform = 'AgX'; sc.view_settings.look = 'AgX - Medium High Contrast'

def mat(name, base=(0.1,0.1,0.1,1), metal=0.0, rough=0.5, emit=None, estr=0, coat=0.0, bump=None, aniso=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = base; b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    b.inputs['Coat Weight'].default_value = coat
    if aniso: b.inputs['Anisotropic'].default_value = aniso
    if emit:
        b.inputs['Emission Color'].default_value = emit; b.inputs['Emission Strength'].default_value = estr
    if bump:
        kind, scale, strength = bump
        tc = nt.nodes.new('ShaderNodeTexCoord')
        if kind == 'noise':
            n = nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = scale; n.inputs['Detail'].default_value = 8
            nt.links.new(tc.outputs['Object'], n.inputs['Vector']); src = n.outputs['Fac']
        elif kind == 'knurl':
            sep = nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(tc.outputs['Object'], sep.inputs[0])
            at = nt.nodes.new('ShaderNodeMath'); at.operation = 'ARCTAN2'
            nt.links.new(sep.outputs[1], at.inputs[0]); nt.links.new(sep.outputs[0], at.inputs[1])
            mu = nt.nodes.new('ShaderNodeMath'); mu.operation = 'MULTIPLY'; mu.inputs[1].default_value = scale
            nt.links.new(at.outputs[0], mu.inputs[0])
            si = nt.nodes.new('ShaderNodeMath'); si.operation = 'SINE'; nt.links.new(mu.outputs[0], si.inputs[0]); src = si.outputs[0]
        elif kind == 'brush':
            n = nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = scale; n.inputs['Detail'].default_value = 4
            mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1, 120, 1)
            nt.links.new(tc.outputs['Object'], mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'], n.inputs['Vector']); src = n.outputs['Fac']
        bn = nt.nodes.new('ShaderNodeBump'); bn.inputs['Strength'].default_value = strength
        nt.links.new(src, bn.inputs['Height']); nt.links.new(bn.outputs['Normal'], b.inputs['Normal'])
    return m

def img_mat(name, path, rough=0.3, metal=0.0, emission=0.0, ecolor=None):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(path); t.interpolation = 'Cubic'
    nt.links.new(t.outputs['Color'], b.inputs['Base Color']); nt.links.new(t.outputs['Alpha'], b.inputs['Alpha'])
    b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    if emission:
        nt.links.new(t.outputs['Color'], b.inputs['Emission Color']); b.inputs['Emission Strength'].default_value = emission
    m.blend_method = 'BLEND' if hasattr(m,'blend_method') else None
    return m

def add(obj_op, **kw):
    obj_op(**kw); return bpy.context.active_object
def bevel(o, w=0.08, seg=4):
    md = o.modifiers.new('bev', 'BEVEL'); md.width = w; md.segments = seg; md.limit_method = 'ANGLE'
    bpy.context.view_layer.objects.active = o; bpy.ops.object.shade_smooth()
    return o
def setmat(o, m): o.data.materials.clear(); o.data.materials.append(m); return o

TOP = H
# enclosure
enc = add(bpy.ops.mesh.primitive_cube_add, size=1, location=(0,0,H/2)); enc.scale = (W, L, H)
bpy.ops.object.transform_apply(scale=True); bevel(enc, spec.get('bevel', 0.35), 8)
e = spec['enclosure']
setmat(enc, mat('enc', tuple(e['color'])+(1,), e.get('metal',0), e.get('rough',.55), coat=e.get('coat',0), bump=tuple(e.get('bump',['noise',600,.08]))))

# art planes
def plane(path, z, m):
    p = add(bpy.ops.mesh.primitive_plane_add, size=1, location=(0,0,z)); p.scale=(W-0.02, L-0.02, 1)
    bpy.ops.object.transform_apply(scale=True); setmat(p, m); return p
def engrave_mat(path, e, ink):
    # opaque top plate: enclosure finish where the art is empty, ink finish in the lines; lines cut into the surface
    m = bpy.data.materials.new('engrave'); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(path); t.interpolation = 'Cubic'
    base = nt.nodes.new('ShaderNodeBsdfPrincipled'); base.inputs['Base Color'].default_value = tuple(e['color'])+(1,)
    base.inputs['Metallic'].default_value = e.get('metal',0); base.inputs['Roughness'].default_value = e.get('rough',.55); base.inputs['Coat Weight'].default_value = e.get('coat',0)
    ik = nt.nodes.new('ShaderNodeBsdfPrincipled'); nt.links.new(t.outputs['Color'], ik.inputs['Base Color'])
    ik.inputs['Metallic'].default_value = ink.get('metal',0); ik.inputs['Roughness'].default_value = ink.get('rough',.2)
    # shared bump: alpha = depth of the cut, plus the enclosure's grain
    tc = nt.nodes.new('ShaderNodeTexCoord'); nz = nt.nodes.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value = e.get('bump',['noise',500,.1])[1]; nz.inputs['Detail'].default_value = 8
    nt.links.new(tc.outputs['Object'], nz.inputs['Vector'])
    hm = nt.nodes.new('ShaderNodeMath'); hm.operation = 'MULTIPLY_ADD'; hm.inputs[1].default_value = -ink.get('depth', 1.0)
    nt.links.new(t.outputs['Alpha'], hm.inputs[0])
    nm = nt.nodes.new('ShaderNodeMath'); nm.operation = 'MULTIPLY'; nm.inputs[1].default_value = e.get('bump',['noise',500,.1])[2]*0.15
    nt.links.new(nz.outputs['Fac'], nm.inputs[0]); nt.links.new(nm.outputs[0], hm.inputs[2])
    bp = nt.nodes.new('ShaderNodeBump'); bp.inputs['Strength'].default_value = ink.get('strength', 0.6); bp.inputs['Distance'].default_value = ink.get('distance', 0.02)
    nt.links.new(hm.outputs[0], bp.inputs['Height'])
    for b in (base, ik): nt.links.new(bp.outputs['Normal'], b.inputs['Normal'])
    mx = nt.nodes.new('ShaderNodeMixShader'); nt.links.new(t.outputs['Alpha'], mx.inputs[0])
    tr = nt.nodes.new('ShaderNodeBsdfTransparent'); nt.links.new(tr.outputs[0], mx.inputs[1]); nt.links.new(ik.outputs[0], mx.inputs[2]); nt.links.new(mx.outputs[0], out.inputs['Surface'])
    return m
if spec.get('art') and spec.get('engrave'):
    plane(spec['art'], TOP+0.002, engrave_mat(spec['art'], e, spec['engrave']))
elif spec.get('art'):
    a = spec.get('artmat', {})
    plane(spec['art'], TOP+0.002, img_mat('art', spec['art'], a.get('rough',.3), a.get('metal',0)))
if spec.get('glow'):
    plane(spec['glow'], TOP+0.004, img_mat('glow', spec['glow'], .4, 0, spec.get('glow_strength', 6)))

M_CHROME = mat('chrome', (0.9,0.9,0.92,1), 1, .12)
M_BLACKP = mat('blackp', (0.015,0.015,0.017,1), 0, .38, bump=('knurl', 40, .35))
M_BLACKTOP = mat('blacktop', (0.02,0.02,0.022,1), 0, .3, bump=('noise', 300, .02))
M_ALU = mat('alu', (0.7,0.7,0.73,1), 1, .34, bump=('knurl', 40, .3))
M_ALUTOP = mat('alutop', (0.72,0.72,0.75,1), 1, .3, aniso=.6)
M_CREAM = mat('cream', (0.85,0.8,0.7,1), 0, .35, bump=('knurl', 24, .2))
M_WHITE = mat('white', (0.95,0.95,0.95,1), 0, .4, emit=(1,1,1,1), estr=0.4)
M_BLACKLINE = mat('bl', (0.01,0.01,0.01,1), 0, .5)
M_COPPER = mat('copper', (0.95,0.55,0.35,1), 1, .25, bump=('knurl', 40, .3))

def knob(px, py, r, style='black', v=0.5, h=1.0):
    x, y = P(px, py)
    sk = {'black':M_BLACKP,'alu':M_ALU,'cream':M_CREAM,'copper':M_COPPER}[style]
    tp = {'black':M_BLACKTOP,'alu':M_ALUTOP,'cream':M_CREAM,'copper':M_ALUTOP}[style]
    c = add(bpy.ops.mesh.primitive_cylinder_add, vertices=96, radius=r, depth=h*0.75, location=(x,y,TOP+h*0.375)); bevel(c, 0.04, 3); setmat(c, sk)
    t = add(bpy.ops.mesh.primitive_cylinder_add, vertices=96, radius=r*0.82, depth=h*0.3, location=(x,y,TOP+h*0.75+h*0.15)); bevel(t, 0.06, 4); setmat(t, tp)
    ang = math.radians(135 - 270*v)
    pl = add(bpy.ops.mesh.primitive_cube_add, size=1, location=(x + math.cos(ang)*r*0.42, y + math.sin(ang)*r*0.42, TOP+h*1.05+0.012))
    pl.scale = (r*0.62, 0.05, 0.02); pl.rotation_euler = (0,0,ang)
    setmat(pl, M_BLACKLINE if style=='cream' else M_WHITE)
def footswitch(px, py, s=1.0):
    x, y = P(px, py)
    n = add(bpy.ops.mesh.primitive_cylinder_add, vertices=6, radius=0.62*s, depth=0.22, location=(x,y,TOP+0.11)); bevel(n,0.02,2); setmat(n, M_CHROME)
    w = add(bpy.ops.mesh.primitive_cylinder_add, vertices=64, radius=0.52*s, depth=0.1, location=(x,y,TOP+0.27)); setmat(w, M_CHROME)
    pl = add(bpy.ops.mesh.primitive_cylinder_add, vertices=64, radius=0.4*s, depth=0.85, location=(x,y,TOP+0.7)); bevel(pl,0.08,4); setmat(pl, M_CHROME)
def led(px, py, color, strength=40):
    x, y = P(px, py)
    t = add(bpy.ops.mesh.primitive_torus_add, major_radius=0.2, minor_radius=0.06, location=(x,y,TOP+0.05)); setmat(t, M_CHROME)
    s = add(bpy.ops.mesh.primitive_uv_sphere_add, radius=0.15, location=(x,y,TOP+0.06)); bpy.ops.object.shade_smooth()
    setmat(s, mat('led', tuple(color)+(1,), 0, .1, emit=tuple(color)+(1,), estr=strength))
    li = add(bpy.ops.object.light_add, type='POINT', location=(x,y,TOP+0.4)); li.data.energy = 2.5; li.data.color = color; li.data.shadow_soft_size = 0.1
def toggle(px, py, tilt=-20):
    x, y = P(px, py)
    n = add(bpy.ops.mesh.primitive_cylinder_add, vertices=6, radius=0.3, depth=0.14, location=(x,y,TOP+0.07)); setmat(n, M_CHROME)
    b = add(bpy.ops.mesh.primitive_cylinder_add, vertices=32, radius=0.07, depth=0.9, location=(x,y,TOP+0.45)); setmat(b, M_CHROME)
    b.rotation_euler = (math.radians(tilt),0,0)
    tip = add(bpy.ops.mesh.primitive_uv_sphere_add, radius=0.1, location=(x, y - math.sin(math.radians(tilt))*0.45*-1, TOP+0.9)); bpy.ops.object.shade_smooth(); setmat(tip, M_CHROME)
def button(px, py, w, hgt, m):
    x, y = P(px, py)
    c = add(bpy.ops.mesh.primitive_cube_add, size=1, location=(x,y,TOP+0.15)); c.scale=(w,hgt,0.3); bpy.ops.object.transform_apply(scale=True); bevel(c,0.08,4); setmat(c,m)
def screw(px, py):
    x, y = P(px, py)
    c = add(bpy.ops.mesh.primitive_cylinder_add, vertices=32, radius=0.13, depth=0.05, location=(x,y,TOP+0.025)); bevel(c,0.02,2); setmat(c, M_CHROME)
def jacks():
    for side in (-1, 1):
        x = side*(W/2-1.3)
        c = add(bpy.ops.mesh.primitive_cylinder_add, vertices=6, radius=0.42, depth=0.2, location=(x, L/2+0.1, H*0.62)); c.rotation_euler=(math.radians(90),0,0); setmat(c, M_CHROME)
        c2 = add(bpy.ops.mesh.primitive_cylinder_add, vertices=48, radius=0.3, depth=0.3, location=(x, L/2+0.3, H*0.62)); c2.rotation_euler=(math.radians(90),0,0); setmat(c2, M_CHROME)

for k in spec.get('knobs', []): knob(*k['at'], k['r'], k.get('style','black'), k.get('v',.5), k.get('h',1.0))
for f in spec.get('footswitches', []): footswitch(*f)
for l in spec.get('leds', []): led(l['at'][0], l['at'][1], l['color'], l.get('strength',40))
for t in spec.get('toggles', []): toggle(*t)
for s in spec.get('screws', []): screw(*s)
for bt in spec.get('buttons', []): button(bt['at'][0], bt['at'][1], bt['w'], bt['h'], mat('btn', tuple(bt['color'])+(1,), bt.get('metal',0), bt.get('rough',.4), emit=tuple(bt['emit'])+(1,) if bt.get('emit') else None, estr=bt.get('estr',0)))
if spec.get('jacks', True): jacks()
# extra geometry: treadle
if spec.get('treadle'):
    tr = spec['treadle']; x, y = P(*tr['at'])
    c = add(bpy.ops.mesh.primitive_cube_add, size=1, location=(x,y,TOP+0.6)); c.scale=(tr['w'], tr['l'], 0.5); bpy.ops.object.transform_apply(scale=True)
    c.rotation_euler=(math.radians(-6),0,0); bevel(c,0.2,6); setmat(c, mat('tread', (0.03,0.03,0.035,1), 0, .8, bump=('noise', 140, .6)))
    if tr.get('art'):
        p = add(bpy.ops.mesh.primitive_plane_add, size=1, location=(x,y,TOP+0.87)); p.scale=(tr['w']-0.2, tr['l']-0.2, 1); bpy.ops.object.transform_apply(scale=True)
        p.rotation_euler=(math.radians(-6),0,0); setmat(p, img_mat('treadart', tr['art'], .35, 0, tr.get('glow',0)))

# floor + lights + camera
fl = add(bpy.ops.mesh.primitive_plane_add, size=200, location=(0,0,0)); setmat(fl, mat('floor', tuple(spec.get('floor',[0.012,0.012,0.014]))+(1,), 0, .32))
w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True; w.node_tree.nodes['Background'].inputs[0].default_value = tuple(spec.get('world',[0.01,0.01,0.012]))+(1,); w.node_tree.nodes['Background'].inputs[1].default_value = 1
def area(loc, size, energy, color=(1,1,1)):
    li = add(bpy.ops.object.light_add, type='AREA', location=loc); li.data.size = size; li.data.energy = energy; li.data.color = color
    d = Vector((0,0,1.5)) - Vector(loc); li.rotation_euler = d.to_track_quat('-Z','Y').to_euler(); return li
area((-7,-10,20), 10, spec.get('key',1800)); area((9,8,12), 6, spec.get('rim',500), tuple(spec.get('rimcolor',[0.8,0.85,1])))
area((0,-20,6), 14, 250)
cam_data = bpy.data.cameras.new('cam'); cam_data.lens = spec.get('lens', 40); cam_data.sensor_fit = 'VERTICAL'; cam_data.sensor_height = 24
cam = bpy.data.objects.new('cam', cam_data); sc.collection.objects.link(cam); sc.camera = cam
cam.location = tuple(spec.get('cam', [0, -11, 28])); d = Vector(tuple(spec.get('target',[0,-0.9,1.5]))) - cam.location
cam.rotation_euler = d.to_track_quat('-Z','Y').to_euler()
sc.render.filepath = spec['out']; bpy.ops.render.render(write_still=True)
print('RENDERED', spec['out'])
