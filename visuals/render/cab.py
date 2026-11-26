"""Real-world speaker cabs (slant or straight, 1x12 to 8x10), built to measure in centimetres.

    python3 render/cab.py -- cabs/atlas_cab.json front          # straight-on plugin view, transparent background
    python3 render/cab.py -- cabs/atlas_cab.json front prev     # quick low-sample preview -> out/preview/
    python3 render/cab.py -- cabs/atlas_cab.json angle          # 3/4 view on a dark floor (for checking the shape)
    extra args: samples=N  res=W,H  cam=x,y,z tgt=x,y,z lens=mm tag=name  blend=out/blend/<key>.blend  norender

head.py builds the stylised cabs that match each amp; this one builds cabs that look like the real boxes the
IRs were recorded from. The front view also writes out/front/<key>_map.json: the picture size and, if the cab
has a display or a voice button, where they are in pixels (the plugin draws the voice name on the display).

Spec (cabs/<key>.json, "builder": "cab"):
  size      [width, depth at the bottom, height]
  slant     {"z": height where the front starts to lean back, "back": how far back the top-front edge sits}
  border    {"side", "bottom", "top"}  covered frame around the grille;  recess: grille depth behind the frame
  edge      rounding of the box edges
  tolex     {"kind": bronco|levant|taurus|carpet|duratex|metal, "color", "rough", "sheen", "grain", "bump"}
            {"kind": "tweed", "tile_cm", "tint", "coat", "varnish": [r,g,b]}  lacquered tweed (art/weave_tweed.png, box-projected)
  grille    cloth: {"cloth": jute|basket|checker|diamond|wheat|silver|stripe, "tile_cm", "tint", "sheen", "bump"}
            metal: {"kind": "perf", "holes": round|hex|square|slot, "tile_cm", "color", "metal", "rough", "scrim": cloth}
  piping    {"color", "r", "inset", "depth"}            cord round the grille (optional)
  reveal    {"w", "d"}                                   a shadow gap round the grille instead of piping (optional)
  corners   {"kind": metal|plastic|chrome|none, "size", "out", "color"}
  handles   {"z"}   casters {"h", "inset", "metal"}   feet: height (when there are no casters)
  speakers  [[x, along, r], ...]  x, and distance along the grille from its bottom edge, radius of the hole
  emblem    {"kind": "tesseract", "x", "along", "size", "bar", "mat": [r,g,b], "metal", "rough", "rot": [a,b,c,d]}
  display   {"w", "h", "x", "z"}  dark glass screen on the front frame;  button {"x", "z", "r", "led": [r,g,b]}
  displays  [{"id", "w", "h", "x", "z"}]  several screens; buttons [{"id", "x", "z", "r"} or {"id", "shape": "rect", "w", "h", ...}]
            (the map file lists every screen and button by id: display* -> the plugin's screen*, button* -> button*)
  leds      [{"z", "w", "color", "strength"}]   thin light lines on the frame;  screws {"inset", "r"}  rack screws
  top_handle  {"w", "h", "b", "color", "ends"}  a strap handle arching over the top (combos)
"""
import bpy, bmesh, math, sys, json, os, itertools
from mathutils import Vector, Matrix

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = sys.argv[sys.argv.index('--') + 1]
ARGS = sys.argv[sys.argv.index('--') + 2:]
A = {}
for a in ARGS:
    if '=' in a: k, v = a.split('=', 1); A[k] = v
    else: A[a] = True
S = json.load(open(SPEC))
KEY = os.path.splitext(os.path.basename(SPEC))[0]
VIEW = 'front' if 'front' in A else 'angle'
PREV = bool(A.get('prev'))

W, D, H = S['size']
SL = S.get('slant')
HS, SB = (SL['z'], SL['back']) if SL else (H, 0.0)
BD = S.get('border', {}); BS, BB, BT = BD.get('side', 4.0), BD.get('bottom', 4.0), BD.get('top', 4.0)
RD = S.get('recess', 1.0)
CAS = S.get('casters'); CH = CAS.get('h', 8.0) if CAS else 0.0

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'
sc.cycles.use_adaptive_sampling = True; sc.cycles.adaptive_threshold = 0.02; sc.cycles.use_denoising = True
sc.view_settings.view_transform = 'AgX'; sc.view_settings.look = 'AgX - Medium High Contrast'
sc.cycles.transparent_max_bounces = 24; sc.cycles.max_bounces = 8


# ------------------------------------------------------------------------------------------------ helpers
def link(ob):
    sc.collection.objects.link(ob); return ob


def mesh_obj(name, verts, faces, m=None):
    me = bpy.data.meshes.new(name); me.from_pydata(verts, [], faces); me.update()
    ob = link(bpy.data.objects.new(name, me))
    if m: ob.data.materials.append(m)
    bm = bmesh.new(); bm.from_mesh(me); bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me); bm.free()
    return ob


def add(op, **kw):
    op(**kw); return bpy.context.active_object


def setmat(o, m):
    o.data.materials.clear(); o.data.materials.append(m); return o


def smooth(o, angle=40):
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    try: bpy.ops.object.shade_smooth_by_angle(angle=math.radians(angle))
    except Exception: bpy.ops.object.shade_smooth()
    o.select_set(False); return o


def bevel(o, w, seg=3, angle=30, limit='ANGLE'):
    m = o.modifiers.new('bev', 'BEVEL'); m.width = w; m.segments = seg; m.limit_method = limit
    if limit == 'ANGLE': m.angle_limit = math.radians(angle)
    return smooth(o, 60)


def rbox(loc, size, m, r=0.2, seg=3):
    c = add(bpy.ops.mesh.primitive_cube_add, size=1, location=loc); c.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if r: bevel(c, r, seg)
    return setmat(c, m)


def nodes(m):
    return m.node_tree.nodes, m.node_tree.links


def mat(name, base, metal=0.0, rough=0.5, coat=0.0, sheen=0.0, emit=None, estr=0.0, aniso=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = tuple(base) + (1,)
    b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    b.inputs['Coat Weight'].default_value = coat; b.inputs['Sheen Weight'].default_value = sheen
    if aniso: b.inputs['Anisotropic'].default_value = aniso
    if emit: b.inputs['Emission Color'].default_value = tuple(emit) + (1,); b.inputs['Emission Strength'].default_value = estr
    return m


def add_bump(m, src, strength, distance=0.1):
    nt, ln = nodes(m); b = nt['Principled BSDF']
    bn = nt.new('ShaderNodeBump'); bn.inputs['Strength'].default_value = strength; bn.inputs['Distance'].default_value = distance
    ln.new(src, bn.inputs['Height'])
    if b.inputs['Normal'].is_linked:
        ln.new(b.inputs['Normal'].links[0].from_socket, bn.inputs['Normal'])
    ln.new(bn.outputs['Normal'], b.inputs['Normal'])
    return bn


def color_variation(m, color, lo=0.85, hi=1.15, scale=0.05):
    """multiply the base colour by a big, faint noise so flat black doesn't look like plastic"""
    nt, ln = nodes(m); tc = nt.new('ShaderNodeTexCoord')
    big = nt.new('ShaderNodeTexNoise'); big.inputs['Scale'].default_value = scale; big.inputs['Detail'].default_value = 2
    ln.new(tc.outputs['Object'], big.inputs['Vector'])
    cr = nt.new('ShaderNodeMapRange'); cr.inputs['To Min'].default_value = lo; cr.inputs['To Max'].default_value = hi
    ln.new(big.outputs['Fac'], cr.inputs['Value'])
    mul = nt.new('ShaderNodeMix'); mul.data_type = 'RGBA'; mul.blend_type = 'MULTIPLY'; mul.inputs[0].default_value = 1.0
    mul.inputs[6].default_value = tuple(color) + (1,)
    ln.new(cr.outputs['Result'], mul.inputs[7]); ln.new(mul.outputs[2], nt['Principled BSDF'].inputs['Base Color'])


def tolex_mat(t):
    kind = t.get('kind', 'bronco'); col = t.get('color', [0.012, 0.012, 0.013]); rough = t.get('rough', 0.6)
    if kind == 'tweed':      # lacquered tweed: a twill-weave image box-projected over the box, under an amber varnish
        m = bpy.data.materials.new('tolex'); m.use_nodes = True; nt, ln = nodes(m); b = nt['Principled BSDF']
        tc = nt.new('ShaderNodeTexCoord'); mp = nt.new('ShaderNodeMapping'); tile = t.get('tile_cm', 4.8)
        mp.inputs['Scale'].default_value = (1 / tile, 1 / tile, 1 / tile); mp.inputs['Rotation'].default_value = (0.3, 0.2, 0.1)
        ln.new(tc.outputs['Object'], mp.inputs['Vector'])
        ci = nt.new('ShaderNodeTexImage'); ci.image = bpy.data.images.load(os.path.join(ROOT, 'art', 'weave_tweed.png'))
        hi = nt.new('ShaderNodeTexImage'); hi.image = bpy.data.images.load(os.path.join(ROOT, 'art', 'weave_tweed_h.png')); hi.image.colorspace_settings.name = 'Non-Color'
        for im in (ci, hi): im.projection = 'BOX'; im.projection_blend = 0.25; im.interpolation = 'Cubic'; ln.new(mp.outputs['Vector'], im.inputs['Vector'])
        tn = nt.new('ShaderNodeMix'); tn.data_type = 'RGBA'; tn.blend_type = 'MULTIPLY'; tn.inputs[0].default_value = 1.0
        tn.inputs[7].default_value = tuple(t.get('tint', [1, 1, 1])) + (1,); ln.new(ci.outputs['Color'], tn.inputs[6])
        ln.new(tn.outputs[2], b.inputs['Base Color'])
        b.inputs['Roughness'].default_value = rough; b.inputs['Coat Weight'].default_value = t.get('coat', 0.55)
        b.inputs['Coat Roughness'].default_value = t.get('coat_rough', 0.22)
        if 'Coat Tint' in b.inputs: b.inputs['Coat Tint'].default_value = tuple(t.get('varnish', [1.0, 0.86, 0.62])) + (1,)
        add_bump(m, hi.outputs['Color'], t.get('bump', 0.5), 0.03)
        # age: big soft blotches of darker varnish
        big = nt.new('ShaderNodeTexNoise'); big.inputs['Scale'].default_value = 0.07; big.inputs['Detail'].default_value = 3
        ln.new(tc.outputs['Object'], big.inputs['Vector'])
        cr = nt.new('ShaderNodeMapRange'); cr.inputs['To Min'].default_value = 0.86; cr.inputs['To Max'].default_value = 1.1; ln.new(big.outputs['Fac'], cr.inputs['Value'])
        mul = nt.new('ShaderNodeMix'); mul.data_type = 'RGBA'; mul.blend_type = 'MULTIPLY'; mul.inputs[0].default_value = 1.0
        ln.new(tn.outputs[2], mul.inputs[6]); ln.new(cr.outputs['Result'], mul.inputs[7]); ln.new(mul.outputs[2], b.inputs['Base Color'])
        return m
    if kind == 'metal':      # anodised brushed aluminium panels
        m = mat('tolex', col, 1.0, rough, aniso=0.6)
        nt, ln = nodes(m); tc = nt.new('ShaderNodeTexCoord')
        n = nt.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = 3; n.inputs['Detail'].default_value = 8
        mp = nt.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (0.4, 40, 40)
        ln.new(tc.outputs['Object'], mp.inputs['Vector']); ln.new(mp.outputs['Vector'], n.inputs['Vector'])
        add_bump(m, n.outputs['Fac'], 0.08, 0.01)
        color_variation(m, col, 0.9, 1.1)
        return m
    m = mat('tolex', col, 0, rough, coat=t.get('coat', 0.0), sheen=t.get('sheen', 0.1))
    nt, ln = nodes(m); tc = nt.new('ShaderNodeTexCoord'); b = nt['Principled BSDF']
    if kind == 'levant':     # big soft pebbles (Marshall-style)
        v = nt.new('ShaderNodeTexVoronoi'); v.feature = 'SMOOTH_F1'; v.inputs['Scale'].default_value = t.get('grain', 2.6)
        nz = nt.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value = 1.5; nz.inputs['Detail'].default_value = 3
        mix = nt.new('ShaderNodeVectorMath'); mix.operation = 'MULTIPLY_ADD'; mix.inputs[1].default_value = (0.25, 0.25, 0.25)
        ln.new(nz.outputs['Color'], mix.inputs[0]); ln.new(tc.outputs['Object'], mix.inputs[2]); ln.new(mix.outputs[0], v.inputs['Vector'])
        add_bump(m, v.outputs['Distance'], t.get('bump', 0.6), 0.12)
        fine = nt.new('ShaderNodeTexNoise'); fine.inputs['Scale'].default_value = 40; fine.inputs['Detail'].default_value = 6
        ln.new(tc.outputs['Object'], fine.inputs['Vector']); add_bump(m, fine.outputs['Fac'], 0.12, 0.02)
        rr = nt.new('ShaderNodeMapRange'); rr.inputs['From Max'].default_value = 0.6
        rr.inputs['To Min'].default_value = rough - 0.14; rr.inputs['To Max'].default_value = rough + 0.1
        ln.new(v.outputs['Distance'], rr.inputs['Value']); ln.new(rr.outputs['Result'], b.inputs['Roughness'])
    elif kind == 'carpet':   # bass-cab carpet: fuzzy, matte, sheen at grazing angles
        b.inputs['Sheen Weight'].default_value = t.get('sheen', 1.0); b.inputs['Sheen Roughness'].default_value = 0.6
        n1 = nt.new('ShaderNodeTexNoise'); n1.inputs['Scale'].default_value = 25; n1.inputs['Detail'].default_value = 15; n1.inputs['Roughness'].default_value = 0.8
        ln.new(tc.outputs['Object'], n1.inputs['Vector']); add_bump(m, n1.outputs['Fac'], 0.9, 0.08)
        n2 = nt.new('ShaderNodeTexNoise'); n2.inputs['Scale'].default_value = 1.2; n2.inputs['Detail'].default_value = 3
        ln.new(tc.outputs['Object'], n2.inputs['Vector']); add_bump(m, n2.outputs['Fac'], 0.25, 0.3)
    elif kind == 'duratex':  # sprayed textured paint: orange-peel bumps, very matte
        v = nt.new('ShaderNodeTexVoronoi'); v.feature = 'SMOOTH_F1'; v.inputs['Scale'].default_value = t.get('grain', 7.0)
        ln.new(tc.outputs['Object'], v.inputs['Vector']); add_bump(m, v.outputs['Distance'], t.get('bump', 0.55), 0.06)
        n1 = nt.new('ShaderNodeTexNoise'); n1.inputs['Scale'].default_value = 30; n1.inputs['Detail'].default_value = 6
        ln.new(tc.outputs['Object'], n1.inputs['Vector']); add_bump(m, n1.outputs['Fac'], 0.2, 0.02)
    else:                    # bronco / taurus: leather grain; taurus is finer and smoother
        g = t.get('grain', 14.0 if kind == 'bronco' else 22.0)
        v = nt.new('ShaderNodeTexVoronoi'); v.inputs['Scale'].default_value = g; v.feature = 'SMOOTH_F1'
        wn = nt.new('ShaderNodeTexNoise'); wn.inputs['Scale'].default_value = 3.0; wn.inputs['Detail'].default_value = 2
        mix = nt.new('ShaderNodeVectorMath'); mix.operation = 'MULTIPLY_ADD'; mix.inputs[1].default_value = (0.05, 0.05, 0.05)
        ln.new(wn.outputs['Color'], mix.inputs[0]); ln.new(tc.outputs['Object'], mix.inputs[2]); ln.new(mix.outputs[0], v.inputs['Vector'])
        add_bump(m, v.outputs['Distance'], t.get('bump', 0.7 if kind == 'bronco' else 0.4), 0.05)
        n1 = nt.new('ShaderNodeTexNoise'); n1.inputs['Scale'].default_value = 60; n1.inputs['Detail'].default_value = 8
        ln.new(tc.outputs['Object'], n1.inputs['Vector']); add_bump(m, n1.outputs['Fac'], 0.25, 0.015)
        rr = nt.new('ShaderNodeMapRange'); rr.inputs['From Max'].default_value = 0.5
        rr.inputs['To Min'].default_value = rough - 0.1; rr.inputs['To Max'].default_value = rough + 0.12
        ln.new(v.outputs['Distance'], rr.inputs['Value']); ln.new(rr.outputs['Result'], b.inputs['Roughness'])
    color_variation(m, col)
    return m


def tiled_mat(name, img, h_img, tile, tint=(1, 1, 1), rough=0.8, sheen=0.3, bump=0.9, metal=0.0, color=None):
    """Material from a tileable RGBA + height pair (cloth or perforated metal). UVs are in cm."""
    m = bpy.data.materials.new(name); m.use_nodes = True; nt, ln = nodes(m); b = nt['Principled BSDF']
    uv = nt.new('ShaderNodeTexCoord'); mp = nt.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / tile, 1 / tile, 1)
    ln.new(uv.outputs['UV'], mp.inputs['Vector'])
    ci = nt.new('ShaderNodeTexImage'); ci.image = bpy.data.images.load(os.path.join(ROOT, 'art', img)); ci.interpolation = 'Cubic'
    hi = nt.new('ShaderNodeTexImage'); hi.image = bpy.data.images.load(os.path.join(ROOT, 'art', h_img)); hi.image.colorspace_settings.name = 'Non-Color'
    for t in (ci, hi): ln.new(mp.outputs['Vector'], t.inputs['Vector'])
    if color is not None:     # solid colour (metal): the image only gives the holes
        b.inputs['Base Color'].default_value = tuple(color) + (1,)
        color_variation(m, color, 0.9, 1.1, 0.08)
    else:
        tn = nt.new('ShaderNodeMix'); tn.data_type = 'RGBA'; tn.blend_type = 'MULTIPLY'; tn.inputs[0].default_value = 1.0
        tn.inputs[7].default_value = tuple(tint) + (1,); ln.new(ci.outputs['Color'], tn.inputs[6])
        big = nt.new('ShaderNodeTexNoise'); big.inputs['Scale'].default_value = 0.06; big.inputs['Detail'].default_value = 2
        ob = nt.new('ShaderNodeTexCoord'); ln.new(ob.outputs['Object'], big.inputs['Vector'])
        cr = nt.new('ShaderNodeMapRange'); cr.inputs['To Min'].default_value = 0.82; cr.inputs['To Max'].default_value = 1.18; ln.new(big.outputs['Fac'], cr.inputs['Value'])
        mul = nt.new('ShaderNodeMix'); mul.data_type = 'RGBA'; mul.blend_type = 'MULTIPLY'; mul.inputs[0].default_value = 1.0
        ln.new(tn.outputs[2], mul.inputs[6]); ln.new(cr.outputs['Result'], mul.inputs[7]); ln.new(mul.outputs[2], b.inputs['Base Color'])
    ln.new(ci.outputs['Alpha'], b.inputs['Alpha'])
    b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    b.inputs['Sheen Weight'].default_value = sheen; b.inputs['Sheen Roughness'].default_value = 0.35
    add_bump(m, hi.outputs['Color'], bump, 0.06)
    return m


def cloth_mat(g):
    cl = g.get('cloth', 'jute')
    return tiled_mat('cloth_' + cl, f'weave_{cl}.png', f'weave_{cl}_h.png', g.get('tile_cm', 8.8), g.get('tint', [1, 1, 1]),
                     g.get('rough', 0.82), g.get('sheen', 0.4), g.get('bump', 0.9))


# ------------------------------------------------------------------------------------------- the box
# side profile (y, z): y = 0 is the bottom-front edge and +y goes back; z = 0 is the bottom of the box
PROF = [(0.0, 0.0), (0.0, HS), (SB, H), (D, H), (D, 0.0)] if SL else [(0.0, 0.0), (0.0, H), (D, H), (D, 0.0)]
U = Vector((SB, H - HS)).normalized() if SL else Vector((0.0, 1.0))   # up the front (slanted part)
NIN = Vector((U.y, -U.x))                                               # into the box, normal to the slanted front
TILT = math.atan2(SB, H - HS) if SL else 0.0

n = len(PROF)
body_v = [(-W / 2, y, z) for (y, z) in PROF] + [(W / 2, y, z) for (y, z) in PROF]
body_f = [tuple(range(n)), tuple(range(2 * n - 1, n - 1, -1))] + [(i, n + i, n + (i + 1) % n, (i + 1) % n) for i in range(n)]
TOLEX = tolex_mat(S.get('tolex', {}))
BAFFLE = mat('baffle', (0.005, 0.005, 0.006), 0, 0.88)
body = mesh_obj('body', body_v, body_f, TOLEX)
body.data.materials.append(BAFFLE)


def grille_path(depth, inset=0.0):
    """Outline (y, z) of the grille surface `depth` behind the front: [bottom, (fold,) top]. `inset` shrinks it."""
    lo = Vector((depth, BB + inset))
    if SL:
        p0 = Vector((0.0, HS)) + NIN * depth
        t = (depth - p0.x) / U.x if abs(U.x) > 1e-6 else 0.0
        fold = p0 + U * t
        top = Vector((SB, H)) - U * (BT + inset) + NIN * depth
        return [lo, fold, top]
    return [lo, Vector((depth, H - BT - inset))]


def path_point(path, s):
    acc = 0.0
    for i, (a, b) in enumerate(zip(path, path[1:])):
        L = (b - a).length
        if s <= acc + L or i == len(path) - 2:
            t = (s - acc) / L if L else 0.0; d = (b - a).normalized()
            return a + (b - a) * t, Vector((d.y, -d.x))
        acc += L


def path_len(path):
    return sum((b - a).length for a, b in zip(path, path[1:]))


def boolean(target, tool):
    md = target.modifiers.new('b', 'BOOLEAN'); md.object = tool; md.operation = 'DIFFERENCE'; md.solver = 'EXACT'
    md.material_mode = 'TRANSFER'
    bpy.context.view_layer.objects.active = target; bpy.ops.object.modifier_apply(modifier=md.name)
    bpy.data.objects.remove(tool)


def prism(name, pts2d, x0, x1, m):
    v = [(x0, p.x, p.y) for p in pts2d] + [(x1, p.x, p.y) for p in pts2d]; k = len(pts2d)
    f = [tuple(range(k)), tuple(range(2 * k - 1, k - 1, -1))] + [(i, k + i, k + (i + 1) % k, (i + 1) % k) for i in range(k)]
    return mesh_obj(name, v, f, m)


def front_cutter(depth, xi):
    """Prism removing everything in front of the grille surface at `depth`, inside the frame."""
    gp = grille_path(depth); far = 30.0
    if SL:
        lo, fold, top = gp
        pts = [Vector((-far, BB)), lo, fold, top, top - NIN * (depth + far), Vector((0.0, HS)) - NIN * far, Vector((-far, HS))]
    else:
        lo, top = gp
        pts = [Vector((-far, BB)), lo, top, Vector((-far, H - BT))]
    return prism('cut', pts, -xi, xi, BAFFLE)


XI = W / 2 - BS
RV = S.get('reveal'); INS = RV.get('w', 0.6) if RV else 0.0
boolean(body, front_cutter(RD + (RV.get('d', 1.2) if RV else 0.0), XI))   # with a reveal the recess is deeper; the grille sits inset
GP = grille_path(RD, INS); GL = path_len(GP)

# speaker pockets
spk = []
for (sx, along, r) in S.get('speakers', []):
    p, nin = path_point(GP, along); ang = math.atan2(nin.y, nin.x)
    h = add(bpy.ops.mesh.primitive_cylinder_add, vertices=96, radius=r, depth=14.0)
    h.rotation_euler = (math.pi / 2 + ang, 0, 0); h.location = (sx, p.x + nin.x * 7.0, p.y + nin.y * 7.0); setmat(h, BAFFLE)
    spk.append((sx, p, nin, ang, r, h))
if spk:
    bpy.ops.object.select_all(action='DESELECT')
    for q in spk: q[5].select_set(True)
    bpy.context.view_layer.objects.active = spk[0][5]; bpy.ops.object.join()
    boolean(body, bpy.context.active_object)
bevel(body, S.get('edge', 0.8), 4, 8)

CONE = mat('cone', (0.03, 0.028, 0.026), 0, 0.88); DUST = mat('dust', (0.018, 0.018, 0.018), 0.1, 0.45)
SURR = mat('surround', (0.01, 0.01, 0.01), 0, 0.7); FRAME = mat('sframe', (0.03, 0.03, 0.032), 0.8, 0.45)
for (sx, p, nin, ang, r, _) in spk:
    def at(depth, ob, flip=False):
        ob.rotation_euler = (math.pi / 2 + ang + (math.pi if flip else 0), 0, 0)
        ob.location = (sx, p.x + nin.x * depth, p.y + nin.y * depth); return ob
    rr = r * 0.97
    at(1.0, setmat(add(bpy.ops.mesh.primitive_torus_add, major_radius=rr, minor_radius=0.55, major_segments=96, minor_segments=12), SURR))
    c = add(bpy.ops.mesh.primitive_cone_add, vertices=96, radius1=rr * 0.93, radius2=r * 0.28, depth=r * 0.42, end_fill_type='NOTHING')
    at(1.0 + r * 0.21, c, True); setmat(c, CONE); smooth(c, 80)
    dc = add(bpy.ops.mesh.primitive_uv_sphere_add, radius=r * 0.3, segments=48, ring_count=24); dc.scale = (1, 1, 0.4)
    at(1.0 + r * 0.40, dc); setmat(dc, DUST); smooth(dc, 80)
    ring = add(bpy.ops.mesh.primitive_torus_add, major_radius=r * 1.06, minor_radius=0.35, major_segments=96, minor_segments=8)
    at(0.6, ring); setmat(ring, FRAME)


def strip(name, depth, m, inset=0.0):
    """A sheet following the grille surface (cloth or metal), UVs in cm."""
    path = grille_path(depth, inset); L = path_len(path)
    xs = [-XI + 0.02 + inset, XI - 0.02 - inset]
    stops = [0.0]
    for a, b in zip(path, path[1:]): stops.append(stops[-1] + (b - a).length)
    vs, uvs = [], []
    for s_ in stops:
        q, _ = path_point(path, min(s_, L))
        for x in xs: vs.append((x, q.x, q.y)); uvs.append((x + XI, s_))
    fs = [(2 * i, 2 * i + 1, 2 * i + 3, 2 * i + 2) for i in range(len(stops) - 1)]
    me = bpy.data.meshes.new(name); me.from_pydata(vs, [], fs); me.update()
    ob = link(bpy.data.objects.new(name, me)); ob.data.materials.append(m)
    uvl = me.uv_layers.new(name='UVMap')
    for poly in me.polygons:
        for li in poly.loop_indices: uvl.data[li].uv = uvs[me.loops[li].vertex_index]
    return ob


G = S.get('grille', {})
if G.get('kind') == 'perf':
    hole = G.get('holes', 'round')
    PM = tiled_mat('perf', f'perf_{hole}.png', f'perf_{hole}_h.png', G.get('tile_cm', 14.4), rough=G.get('rough', 0.42),
                   sheen=0.0, bump=G.get('bump', 0.6), metal=G.get('metal', 0.85), color=G.get('color', [0.025, 0.025, 0.027]))
    pl = strip('perf', RD - 0.35, PM, INS)
    sol = pl.modifiers.new('s', 'SOLIDIFY'); sol.thickness = 0.12; sol.offset = 1
    strip('scrim', RD - 0.05, cloth_mat(G.get('scrim', {'cloth': 'basket', 'tile_cm': 9.6})), INS)
else:
    strip('cloth', RD - 0.12, cloth_mat(G), INS)

# piping round the grille, following the fold
PP = S.get('piping')
if PP:
    pr = PP.get('r', 0.22); pin = PP.get('inset', 0.25); rad = PP.get('corner', 1.2)
    ppath = grille_path(PP.get('depth', RD * 0.45)); PL = path_len(ppath)
    u0, u1, v0, v1 = -XI + pin, XI - pin, pin, PL - pin
    loop = []
    def arc(cu, cv, a0, a1, steps=8):
        for k in range(steps + 1):
            t = a0 + (a1 - a0) * k / steps; loop.append((cu + rad * math.cos(t), cv + rad * math.sin(t)))
    arc(u1 - rad, v0 + rad, -math.pi / 2, 0); arc(u1 - rad, v1 - rad, 0, math.pi / 2)
    arc(u0 + rad, v1 - rad, math.pi / 2, math.pi); arc(u0 + rad, v0 + rad, math.pi, 1.5 * math.pi)
    fold_s = (ppath[1] - ppath[0]).length if SL else None
    full = []
    for i in range(len(loop)):
        full.append(loop[i])
        if SL:
            (ua, va), (ub, vb) = loop[i], loop[(i + 1) % len(loop)]
            if (va - fold_s) * (vb - fold_s) < 0:
                t = (fold_s - va) / (vb - va); full.append((ua + (ub - ua) * t, fold_s))
    cu = bpy.data.curves.new('pipe', 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = pr; cu.bevel_resolution = 4
    spl = cu.splines.new('POLY'); spl.points.add(len(full) - 1)
    for ptn, (u, v) in zip(spl.points, full):
        q, _ = path_point(ppath, v); ptn.co = (u, q.x, q.y, 1)
    spl.use_cyclic_u = True
    po = link(bpy.data.objects.new('piping', cu))
    po.data.materials.append(mat('piping', PP.get('color', [0.75, 0.74, 0.7]), PP.get('metal', 0.0), PP.get('rough', 0.42), sheen=0.3))

# corners
CN = S.get('corners', {})
kind = CN.get('kind', 'none')
if kind != 'none':
    cs = CN.get('size', 6.0); pro = CN.get('out', 0.35)
    ccol = CN.get('color', [0.6, 0.6, 0.62] if kind == 'chrome' else [0.02, 0.02, 0.022])
    CM = mat('corner', ccol, 1.0 if kind == 'chrome' else (0.85 if kind == 'metal' else 0.0),
             CN.get('rough', 0.12 if kind == 'chrome' else (0.36 if kind == 'metal' else 0.42)), coat=0.3 if kind == 'plastic' else 0)
    RIV = mat('rivet', (0.05, 0.05, 0.052) if kind != 'chrome' else (0.7, 0.7, 0.72), 0.9, 0.3)
    spots = []
    for sx in (-1, 1):
        spots += [(sx, (0.0, 0.0), 0.0, (-1, -1)), (sx, (D, 0.0), 0.0, (1, -1)), (sx, (D, H), 0.0, (1, 1)), (sx, (SB, H), -TILT / 2, (-1, 1))]
    for (sx, (cy, cz), rx, (dy, dz)) in spots:
        c = add(bpy.ops.mesh.primitive_cube_add, size=1); c.scale = (cs, cs, cs)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        bv = c.modifiers.new('b', 'BEVEL'); bv.width = cs * (0.4 if kind == 'plastic' else 0.24); bv.segments = 12; bv.limit_method = 'NONE'
        smooth(c, 80); setmat(c, CM)
        off = Matrix.Rotation(rx, 3, 'X') @ Vector((-sx * (cs / 2 - pro), -dy * (cs / 2 - pro), -dz * (cs / 2 - pro)))
        c.location = (sx * W / 2 + off.x, cy + off.y, cz + off.z); c.rotation_euler = (rx, 0, 0)
        if kind in ('metal', 'chrome'):
            for ax in range(3):
                nrm = Vector((sx if ax == 0 else 0, dy if ax == 1 else 0, dz if ax == 2 else 0))
                tang = Vector((-sx * cs * 0.26 if ax != 0 else 0, -dy * cs * 0.26 if ax != 1 else 0, -dz * cs * 0.26 if ax != 2 else 0))
                pos = Vector((sx * W / 2, cy, cz)) + Matrix.Rotation(rx, 3, 'X') @ (nrm * pro + tang)
                rv = add(bpy.ops.mesh.primitive_uv_sphere_add, radius=0.3, segments=20, ring_count=10, location=pos)
                rv.scale = tuple(0.45 if k == ax else 1.0 for k in range(3)); setmat(rv, RIV); smooth(rv, 80)

# handles on the sides
HD = S.get('handles')
if HD:
    HM = mat('handle', HD.get('color', [0.025, 0.025, 0.027]), 0.85, 0.35); HIN = mat('handle_in', (0.004, 0.004, 0.004), 0.5, 0.6)
    hz = HD.get('z', H * 0.72); hy = HD.get('y', (D + SB) / 2)
    for sx in (-1, 1):
        x = sx * (W / 2 + 0.15)
        rbox((x, hy, hz), (0.3, 19.0, 9.0), HM, 0.12, 2); rbox((x + sx * 0.06, hy, hz), (0.3, 16.5, 6.5), HIN, 0.5, 3)
        bar = add(bpy.ops.mesh.primitive_cylinder_add, vertices=32, radius=0.55, depth=13.0, location=(x + sx * 0.35, hy, hz - 1.0))
        bar.rotation_euler = (math.pi / 2, 0, 0); setmat(bar, HM); bevel(bar, 0.2, 2)

# casters, or feet
if CAS:
    CMM = mat('caster', CAS.get('metal', [0.04, 0.04, 0.042]), 0.9, CAS.get('rough', 0.32)); WHL = mat('wheel', (0.012, 0.012, 0.013), 0, 0.55)
    cin = CAS.get('inset', 7.0); wr = CH * 0.36
    for sx in (-1, 1):
        for cy in (cin, D - cin):
            x = sx * (W / 2 - cin)
            rbox((x, cy, -0.25), (7.0, 7.0, 0.5), CMM, 0.15, 2)
            hs = add(bpy.ops.mesh.primitive_cylinder_add, vertices=48, radius=2.2, depth=1.0, location=(x, cy, -1.0)); setmat(hs, CMM); bevel(hs, 0.25, 2)
            for fx in (-1, 1):
                rbox((x + fx * 1.55, cy + 0.9, -1.5 - (CH - 1.5) * 0.45), (0.35, 3.2, (CH - 1.5) * 0.9), CMM, 0.12, 2)
            wh = add(bpy.ops.mesh.primitive_cylinder_add, vertices=64, radius=wr, depth=2.5, location=(x, cy + 1.6, -CH + wr))
            wh.rotation_euler = (0, math.pi / 2, 0); setmat(wh, WHL); bevel(wh, 0.5, 4)
            ax_ = add(bpy.ops.mesh.primitive_cylinder_add, vertices=24, radius=0.45, depth=3.5, location=(x, cy + 1.6, -CH + wr))
            ax_.rotation_euler = (0, math.pi / 2, 0); setmat(ax_, mat('axle', (0.6, 0.6, 0.62), 1, 0.25))
elif S.get('feet'):
    FH = S['feet']; FM = mat('foot', (0.012, 0.012, 0.012), 0, 0.6)
    for sx in (-1, 1):
        for cy in (6, D - 6):
            f = add(bpy.ops.mesh.primitive_cylinder_add, vertices=48, radius=2.2, depth=FH, location=(sx * (W / 2 - 6), cy, -FH / 2)); setmat(f, FM); bevel(f, 0.4, 3)
    CH = FH

# tesseract emblem: a hypercube projected 4D -> 3D -> the front, built from bevelled metal bars
EM = S.get('emblem')
if EM:
    a1, a2, a3, a4 = [math.radians(v) for v in EM.get('rot', [24, 18, 30, -20])]
    def rot4(p):
        x, y, z, w = p
        x, w = x * math.cos(a1) - w * math.sin(a1), x * math.sin(a1) + w * math.cos(a1)       # xw
        y, w = y * math.cos(a2) - w * math.sin(a2), y * math.sin(a2) + w * math.cos(a2)       # yw
        x, z = x * math.cos(a3) - z * math.sin(a3), x * math.sin(a3) + z * math.cos(a3)       # xz
        y, z = y * math.cos(a4) - z * math.sin(a4), y * math.sin(a4) + z * math.cos(a4)       # yz
        return x, y, z, w
    P4 = list(itertools.product((-1, 1), repeat=4))
    P3 = []
    for p in P4:
        x, y, z, w = rot4(p); kk = EM.get('persp', 2.6) / (EM.get('persp', 2.6) - w * 0.9)   # the far cube shrinks inside
        P3.append((x * kk, y * kk, z * kk, w))
    E = [(i, j) for i in range(16) for j in range(i + 1, 16) if sum(a != b for a, b in zip(P4[i], P4[j])) == 1]
    ext = max(max(abs(p[0]), abs(p[2])) for p in P3); sz = EM.get('size', 11.0) / 2 / ext
    p, nin = path_point(GP, EM.get('along', GL * 0.8))
    lift = EM.get('lift', 0.7)
    base = Vector((EM.get('x', 0.0), p.x - nin.x * lift, p.y - nin.y * lift))
    right = Vector((1, 0, 0)); up = Vector((0, -nin.y, nin.x)); out = Vector((0, -nin.x, -nin.y))
    if up.z < 0: up = -up
    EMM = mat('emblem', EM.get('mat', [0.06, 0.06, 0.065]), EM.get('metal', 1.0), EM.get('rough', 0.16))
    bar = EM.get('bar', 0.42)
    def world(q):
        # depth (y) and w push the bars apart a little so they layer like real stacked metal
        return base + right * (q[0] * sz) + up * (q[2] * sz) + out * ((q[1] + 1.5) * 0.08 + (q[3] + 1) * 0.12)
    for (i, j) in E:
        a = world(P3[i]); b = world(P3[j]); d = b - a; L = d.length
        c = add(bpy.ops.mesh.primitive_cube_add, size=1); c.scale = (bar, bar * 0.8, L + bar * 0.2)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        c.location = (a + b) / 2; c.rotation_mode = 'QUATERNION'; c.rotation_quaternion = d.to_track_quat('Z', 'Y')
        bevel(c, bar * 0.28, 2, 30, 'NONE'); setmat(c, EMM)
    for q in P3:
        s_ = add(bpy.ops.mesh.primitive_uv_sphere_add, radius=bar * 0.75, segments=24, ring_count=12, location=world(q)); setmat(s_, EMM); smooth(s_, 80)

# displays, buttons, light lines and rack screws on the front frame (the digital cabs)
MAP = {}
BEZ = mat('bezel', (0.008, 0.008, 0.009), 0.6, 0.22, coat=0.6)
def display(did, dp):
    dw, dh, dx, dz = dp['w'], dp['h'], dp.get('x', 0.0), dp['z']
    rbox((dx, -0.05, dz), (dw + dp.get('bezel', 1.6), 0.5, dh + dp.get('bezel', 1.6)), BEZ, 0.25, 3)
    rbox((dx, -0.32, dz), (dw, 0.05, dh), mat('screen_' + did, (0.0, 0.0, 0.0), 0, 0.05, coat=1.0,
         emit=dp.get('glow', [0.05, 0.09, 0.12]), estr=dp.get('glow_strength', 0.6)), 0.05, 2)
    MAP[did] = (dx, -0.36, dz, dw, dh)
for dp in ([dict(S['display'], id='display')] if S.get('display') else []) + S.get('displays', []):
    display(dp.get('id', 'display'), dp)
def button(bid, b):
    bx, bz = b['x'], b['z']; led = b.get('led', [0.4, 0.75, 1.0])
    if b.get('shape') == 'rect':      # a rack-style push button: brushed cap in a dark well, a light line under it
        bw, bh = b.get('w', 5.0), b.get('h', 2.2)
        rbox((bx, -0.04, bz), (bw + 0.8, 0.3, bh + 0.8), BEZ, 0.2, 3)
        rbox((bx, -0.42, bz), (bw, 0.7, bh), mat('bcap_' + bid, (0.5, 0.5, 0.52), 1.0, 0.26, aniso=0.7), 0.22, 3)
        rbox((bx, -0.08, bz - bh / 2 - 0.95), (bw, 0.1, 0.16), mat('bled_' + bid, (0.02, 0.02, 0.02), 0, 0.3, emit=led, estr=b.get('strength', 6)), 0.03, 2)
        MAP[bid] = (bx, -0.8, bz - 0.3, bw + 1.2, bh + 2.2)
        return
    br = b.get('r', 1.6)
    ring = add(bpy.ops.mesh.primitive_torus_add, major_radius=br * 1.18, minor_radius=0.18, major_segments=96, minor_segments=12, location=(bx, -0.12, bz))
    ring.rotation_euler = (math.pi / 2, 0, 0); setmat(ring, mat('bring_' + bid, (0.02, 0.02, 0.02), 0, 0.3, emit=led, estr=b.get('strength', 6)))
    cap = add(bpy.ops.mesh.primitive_cylinder_add, vertices=96, radius=br, depth=0.9, location=(bx, -0.35, bz)); cap.rotation_euler = (math.pi / 2, 0, 0)
    bevel(cap, 0.22, 4, 30, 'NONE'); setmat(cap, mat('bcap_' + bid, (0.5, 0.5, 0.52), 1.0, 0.28, aniso=0.7))
    MAP[bid] = (bx, -0.8, bz, br * 2.5, br * 2.5)
for b in ([dict(S['button'], id='button')] if S.get('button') else []) + S.get('buttons', []):
    button(b.get('id', 'button'), b)
for ld in S.get('leds', []):
    rbox((ld.get('x', 0.0), -0.04, ld['z']), (ld['w'], 0.12, ld.get('h', 0.22)), mat('led', (0.02, 0.02, 0.02), 0, 0.3, emit=ld['color'], estr=ld.get('strength', 8)), 0.04, 2)
RH = S.get('rack_handles')
if RH:   # rack-style front handles on the frame's sides (like Eidolon's): a bar on two standoffs
    RHM = mat('rhandle', RH.get('color', [0.55, 0.55, 0.57]), 1.0, RH.get('rough', 0.2), aniso=0.5)
    z0, z1, so, rr_ = RH['z0'], RH['z1'], RH.get('standoff', 2.2), RH.get('r', 0.5)
    for sx in (-1, 1):
        x = sx * RH.get('x', W / 2 - BS / 2)
        bar = add(bpy.ops.mesh.primitive_cylinder_add, vertices=32, radius=rr_, depth=z1 - z0, location=(x, -so, (z0 + z1) / 2)); setmat(bar, RHM); smooth(bar, 60)
        for z in (z0, z1):
            st = add(bpy.ops.mesh.primitive_cylinder_add, vertices=32, radius=rr_ * 0.95, depth=so, location=(x, -so / 2, z))
            st.rotation_euler = (math.pi / 2, 0, 0); setmat(st, RHM); smooth(st, 60)
            el = add(bpy.ops.mesh.primitive_uv_sphere_add, radius=rr_ * 1.02, segments=24, ring_count=12, location=(x, -so, z)); setmat(el, RHM); smooth(el, 80)
            ft = add(bpy.ops.mesh.primitive_cylinder_add, vertices=32, radius=rr_ * 1.7, depth=0.3, location=(x, -0.15, z))
            ft.rotation_euler = (math.pi / 2, 0, 0); setmat(ft, RHM); bevel(ft, 0.1, 2)
TH = S.get('top_handle')
if TH:   # a strap handle on top (combos): a leather band in an arch, its ends in metal caps
    hw, hh, hb = TH.get('w', 16.0), TH.get('h', 4.5), TH.get('b', 3.2); yc = TH.get('y', D / 2); th = TH.get('t', 1.1); N = 24
    vs, fs = [], []
    for i in range(N + 1):
        a = math.pi * i / N; x = -hw / 2 * math.cos(a); z = H + 0.4 + hh * math.sin(a)
        nx, nz = -math.cos(a), math.sin(a)
        for (dy, dt) in ((-hb / 2, 0), (hb / 2, 0), (hb / 2, th), (-hb / 2, th)):
            vs.append((x + nx * dt, yc + dy, z + nz * dt))
    for i in range(N):
        a0, a1 = 4 * i, 4 * (i + 1)
        for k in range(4): fs.append((a0 + k, a0 + (k + 1) % 4, a1 + (k + 1) % 4, a1 + k))
    fs += [(0, 1, 2, 3), (4 * N + 3, 4 * N + 2, 4 * N + 1, 4 * N)]
    st_ = mesh_obj('strap', vs, fs, mat('strap', TH.get('color', [0.012, 0.009, 0.007]), 0, 0.42, coat=0.3, sheen=0.2)); smooth(st_, 50)
    EC = mat('strapend', TH.get('ends', [0.6, 0.6, 0.62]), 1.0, 0.2)
    for sx in (-1, 1): rbox((sx * hw / 2, yc, H + 0.5), (2.2, hb + 1.0, 1.0), EC, 0.2, 2)
SC = S.get('screws')
if SC:   # rack screws in the frame's corners: a domed head with a hex socket
    SCM = mat('screw', (0.09, 0.09, 0.095), 1.0, 0.25); HEX = mat('hex', (0.004, 0.004, 0.004), 0.5, 0.5)
    ins = SC.get('inset', 1.8); r_ = SC.get('r', 0.5)
    for sx in (-1, 1):
        for z in (ins, H - ins):
            hd = add(bpy.ops.mesh.primitive_uv_sphere_add, radius=r_, segments=32, ring_count=16, location=(sx * (W / 2 - ins), 0.0, z))
            hd.scale = (1, 0.38, 1); setmat(hd, SCM); smooth(hd, 80)
            hx = add(bpy.ops.mesh.primitive_cylinder_add, vertices=6, radius=r_ * 0.42, depth=0.2, location=(sx * (W / 2 - ins), -r_ * 0.36, z))
            hx.rotation_euler = (math.pi / 2, 0, 0); setmat(hx, HEX)

# lift the cab onto its casters / feet
root = link(bpy.data.objects.new('cab', None))
for o in list(bpy.data.objects):
    if o is not root and o.parent is None and o.type in ('MESH', 'CURVE'): o.parent = root
root.location = (0, 0, CH)
bpy.context.view_layer.update()

# ------------------------------------------------------------------------------------- floor, lights, camera
HT = H + CH; YC = D / 2
world = bpy.data.worlds.new('w'); sc.world = world; world.use_nodes = True
world.node_tree.nodes['Background'].inputs[0].default_value = tuple(S.get('world', [0.006, 0.0065, 0.008])) + (1,)
fl = add(bpy.ops.mesh.primitive_plane_add, size=200000, location=(0, 0, 0))
if VIEW == 'front':
    fl.is_shadow_catcher = True; sc.render.film_transparent = True
else:
    setmat(fl, mat('floor', (0.006, 0.006, 0.007), 0, 0.3))


def area(loc, size, energy, color=(1, 1, 1), tgt=(0, YC, HT * 0.5), sy=None):
    li = add(bpy.ops.object.light_add, type='AREA', location=loc); li.data.energy = energy; li.data.color = color
    if sy: li.data.shape = 'RECTANGLE'; li.data.size = size; li.data.size_y = sy
    else: li.data.size = size
    li.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    li.visible_camera = False
    return li


kL = S.get('light', 1.0) * (max(W, HT) / 80.0) ** 2
sc.view_settings.exposure = S.get('exposure', 0.35)
area((-130, -170, HT + 120), 120, 2.4e5 * kL, sy=90)                                     # key softbox, upper left front
area((0, -150, HT + 130), 170, 1.5e5 * kL, (0.92, 0.95, 1.0), sy=60, tgt=(0, YC, HT * 0.62))   # high front strip: lifts the slant and the top edge
area((0, -330, HT * 0.55), 90, 3.5e4 * kL * S.get('sheen_light', 1.0), sy=60)            # behind the camera: soft sheen that shows the grain
area((175, 30 if VIEW == 'front' else 90, HT * 0.62), 28, 1.2e5 * kL, tuple(S.get('rimcolor', [0.78, 0.86, 1.0])), sy=170)   # cool rim, right
area((-175, 60, HT * 0.6), 28, 9.0e4 * kL, (1.0, 0.93, 0.86), sy=170)                    # warm rim, left
area((0, YC, HT + 150), 130, 1.1e5 * kL, (0.88, 0.92, 1.0), tgt=(0, YC, 0), sy=70)        # top
area((0, -60, -8), 150, 1.0e4 * kL, (0.9, 0.9, 1.0), tgt=(0, YC, HT * 0.25), sy=20)      # low kick on the bottom edge and casters

cd = bpy.data.cameras.new('cam'); cam = link(bpy.data.objects.new('cam', cd)); sc.camera = cam
if VIEW == 'front':
    cd.lens = 85; m_ = S.get('margin', 0.03); cd.sensor_fit = 'VERTICAL'; cd.sensor_height = 36
    FT = HT + ((TH.get('h', 4.5) + TH.get('t', 1.1) + 0.8) if TH else 0.0)   # the frame takes in a strap handle on top
    fw, fh = W + 2.0 + 2 * m_ * FT, FT * (1 + 2 * m_)
    ry = int(S.get('px', 1000)); rx = int(round(ry * fw / fh))
    sc.render.resolution_x, sc.render.resolution_y = rx, ry
    dist = (fh / 2) / math.tan(math.atan(36 / 2 / 85))
    cam.location = (0, -dist, FT / 2); cam.rotation_euler = (math.pi / 2, 0, 0)
    S_out = os.path.join(ROOT, 'out', 'front', KEY + '.png')
else:
    cd.lens = float(A.get('lens', S.get('lens', 60)))
    sc.render.resolution_x, sc.render.resolution_y = S.get('res', [1600, 1200])
    cp = S.get('cam', [-125, -215, HT + 45]); tg = S.get('target', [2, YC * 0.55, HT * 0.47])
    if A.get('cam'): cp = [float(v) for v in A['cam'].split(',')]
    if A.get('tgt'): tg = [float(v) for v in A['tgt'].split(',')]
    cam.location = cp; cam.rotation_euler = (Vector(tg) - Vector(cp)).to_track_quat('-Z', 'Y').to_euler()
    S_out = os.path.join(ROOT, 'out', 'angle', KEY + '.png')

sc.cycles.samples = S.get('samples', 24)
if PREV:
    sc.cycles.samples = 10; sc.render.resolution_x //= 2; sc.render.resolution_y //= 2
    S_out = os.path.join(ROOT, 'out', 'preview', VIEW + '_' + KEY + '.png')
if A.get('samples'): sc.cycles.samples = int(A['samples'])
if A.get('tag'): S_out = S_out[:-4] + '_' + A['tag'] + '.png'
if A.get('res'): sc.render.resolution_x, sc.render.resolution_y = [int(v) for v in A['res'].split(',')]
sc.render.image_settings.file_format = 'PNG'
sc.render.image_settings.color_mode = 'RGBA' if VIEW == 'front' else 'RGB'
os.makedirs(os.path.dirname(S_out), exist_ok=True)

# where the display / button land in the picture (for the plugin)
if VIEW == 'front' and not A.get('tag'):
    from bpy_extras.object_utils import world_to_camera_view as w2c
    bpy.context.view_layer.update()
    RX, RY = sc.render.resolution_x, sc.render.resolution_y
    def px(x, y, z):
        c = w2c(sc, cam, Vector((x, y, z + CH))); return [round(c.x * RX, 1), round((1 - c.y) * RY, 1)]
    out = dict(w=RX, h=RY)
    for k_, (x, y, z, w_, h_) in MAP.items():
        a_ = px(x - w_ / 2, y, z + h_ / 2); b_ = px(x + w_ / 2, y, z - h_ / 2); out[k_] = [a_[0], a_[1], b_[0], b_[1]]
    json.dump(out, open(S_out[:-4] + '_map.json', 'w'))

if A.get('blend'):
    bp = A['blend'] if os.path.isabs(A['blend']) else os.path.join(ROOT, A['blend'])
    os.makedirs(os.path.dirname(bp), exist_ok=True)
    bpy.ops.file.pack_all(); bpy.ops.wm.save_as_mainfile(filepath=bp, compress=True); print('SAVED', bp)
if A.get('norender'): raise SystemExit
sc.render.filepath = S_out
bpy.ops.render.render(write_still=True)
print('RENDERED', S_out)
