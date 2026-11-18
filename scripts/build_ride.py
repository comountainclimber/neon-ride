"""Neon ride: 20 s first-person bike ride along a skyway above a far-future city, into a glass-walled
left curve, past a giant hologram jellyfish advert.

  blender -b --python scripts/build_ride.py -- --frames 1,97,193 --res 50 --samples 16
  blender -b --python scripts/build_ride.py -- --anim --res 50 --samples 16 --out renders/preview1
Options: --res, --samples, --out, --step (keyframe every n frames), --save file.blend
"""
import bpy, bmesh, math, os, random, sys
from mathutils import Vector, Matrix, Euler

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def arg(name, default=None, cast=str):
    if name in argv:
        i = argv.index(name)
        return cast(argv[i + 1]) if i + 1 < len(argv) and not argv[i + 1].startswith("--") else True
    return default


RES, SAMPLES, STEP = arg("--res", 50, int), arg("--samples", 16, int), arg("--step", 1, int)
OUT = os.path.join(ROOT, arg("--out", "renders/test"))
FPS, DUR = 24, 20.0
TOTAL = int(DUR * FPS)
SPEED = 44.0                       # m/s, average for the rain; the real speed varies, see rider_speed()
G = 9.81
S1, TRANS, RADIUS = 300.0, 90.0, 340.0      # straight, curvature ramp, curve radius
GLASS_S = 372.0                    # glass walls begin
DECK_W, DECK_Z = 10.0, 0.0
LANE = 1.8                          # right lane; the passing lane is LEFT_LANE
LEFT_LANE = -2.6
GROUND_Z = -160.0
random.seed(21)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
col = scene.collection


# ---------------------------------------------------------------- helpers
def link(ob, parent=None):
    col.objects.link(ob)
    if parent:
        ob.parent = parent
    return ob


def empty(name, loc=(0, 0, 0), parent=None, rot=(0, 0, 0)):
    ob = bpy.data.objects.new(name, None)
    ob.location, ob.rotation_euler = loc, rot
    ob.empty_display_size = 0.2
    return link(ob, parent)


def mesh_from_bm(name, bm, mat=None, parent=None):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    if mat:
        me.materials.append(mat)
    return link(bpy.data.objects.new(name, me), parent)


def box(name, size, loc, mat, parent=None, rot=(0, 0, 0), mesh=None):
    if mesh is None:
        mesh = bpy.data.meshes.new(name)
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        bm.to_mesh(mesh)
        bm.free()
        mesh.materials.append(mat)
    ob = bpy.data.objects.new(name, mesh)
    ob.location, ob.rotation_euler, ob.scale = loc, rot, size
    return link(ob, parent)


def principled(name, color=(0.1, 0.1, 0.1), rough=0.5, metal=0.0, emit=None, strength=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit:
        b.inputs["Emission Color"].default_value = (*emit, 1)
        b.inputs["Emission Strength"].default_value = strength
    return m


def emissive(name, color, strength):
    return principled(name, (0, 0, 0), rough=0.4, emit=color, strength=strength)


def mnode(nt, op, a, b=None):
    n = nt.nodes.new("ShaderNodeMath")
    n.operation = op
    for i, x in enumerate((a, b)):
        if x is None:
            continue
        if isinstance(x, (int, float)):
            n.inputs[i].default_value = x
        else:
            nt.links.new(x, n.inputs[i])
    return n.outputs[0]


def fcurves_of(idblock):
    ad = idblock.animation_data
    out = []
    if ad and ad.action:
        for layer in ad.action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    out += list(bag.fcurves)
    return out


def set_interp(idblock, mode="LINEAR"):
    for fc in fcurves_of(idblock):
        for kp in fc.keyframe_points:
            kp.interpolation = mode


def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def strip_glow(mat, strength=12.0):
    """Tripo bakes light strips into the base colour; make strongly saturated, bright pixels emit."""
    nt = mat.node_tree
    b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    if not b.inputs["Base Color"].links:
        return
    tex = b.inputs["Base Color"].links[0].from_socket
    hsv = nt.nodes.new("ShaderNodeSeparateColor")
    hsv.mode = "HSV"
    nt.links.new(tex, hsv.inputs[0])
    mask = mnode(nt, "MULTIPLY", mnode(nt, "GREATER_THAN", hsv.outputs[1], 0.42), mnode(nt, "GREATER_THAN", hsv.outputs[2], 0.32))
    nt.links.new(tex, b.inputs["Emission Color"])
    nt.links.new(mnode(nt, "MULTIPLY", mask, strength), b.inputs["Emission Strength"])


_GLB_CACHE = {}


def glb_mesh(name, ratio=None):
    """Import models/<name>.glb once and return its mesh, baked to model space; ratio<1 decimates it."""
    key = (name, ratio)
    if key in _GLB_CACHE:
        return _GLB_CACHE[key]
    path = os.path.join(ROOT, "models", name + ".glb")
    if not os.path.exists(path):
        _GLB_CACHE[key] = None
        return None
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = set(bpy.data.objects) - before
    mesh_ob = next(o for o in new if o.type == "MESH")
    if ratio:
        mod = mesh_ob.modifiers.new("dec", "DECIMATE")
        mod.ratio = ratio
        dg = bpy.context.evaluated_depsgraph_get()
        me = bpy.data.meshes.new_from_object(mesh_ob.evaluated_get(dg))
    else:
        me = mesh_ob.data.copy()
    me.transform(mesh_ob.matrix_world)
    for o in new:
        bpy.data.objects.remove(o, do_unlink=True)
    _GLB_CACHE[key] = me
    return me


def lum_glow(mat, lo=0.3, hi=0.75, strength=6.0):
    """Bright texture pixels (lit windows, light lines) emit their own colour."""
    nt = mat.node_tree
    b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    if not b.inputs["Base Color"].links:
        return
    tex = b.inputs["Base Color"].links[0].from_socket
    bw = nt.nodes.new("ShaderNodeRGBToBW")
    nt.links.new(tex, bw.inputs[0])
    rng = nt.nodes.new("ShaderNodeMapRange")
    rng.inputs["From Min"].default_value, rng.inputs["From Max"].default_value = lo, hi
    rng.inputs["To Min"].default_value, rng.inputs["To Max"].default_value = 0.0, strength
    nt.links.new(bw.outputs[0], rng.inputs["Value"])
    nt.links.new(tex, b.inputs["Emission Color"])
    nt.links.new(rng.outputs["Result"], b.inputs["Emission Strength"])


def model_at(name, kind, loc, rz, scale, ratio=None, glow=None):
    """Place a Tripo model (shared mesh). glow = ("strip", k) or ("lum", lo, hi, k), applied once per mesh."""
    me = glb_mesh(kind, ratio)
    if me is None:
        return None
    if glow and not me.get("glow"):
        for m in me.materials:
            strip_glow(m, glow[1]) if glow[0] == "strip" else lum_glow(m, *glow[1:])
        me["glow"] = 1
    ob = link(bpy.data.objects.new(name, me))
    ob.location, ob.rotation_euler, ob.scale = loc, (0, 0, rz), (scale,) * 3
    return ob


# ---------------------------------------------------------------- the throttle
# speed in m/s through the film: rolls on hard for each pass, eases off to watch the jellyfish, then a final charge
SPEED_KEYS = [(0.0, 34.0), (1.2, 34.0), (4.4, 50.0), (6.2, 44.0), (8.4, 52.0), (10.4, 46.0), (12.5, 38.0),
              (15.4, 38.0), (18.6, 55.0), (21.0, 56.0)]


def rider_speed(t):
    if t <= SPEED_KEYS[0][0]:
        return SPEED_KEYS[0][1]
    for (t0, v0), (t1, v1) in zip(SPEED_KEYS, SPEED_KEYS[1:]):
        if t <= t1:
            return v0 + (v1 - v0) * smooth((t - t0) / (t1 - t0))
    return SPEED_KEYS[-1][1]


_DT = 1.0 / 240
_S_TAB = [0.0]
for _i in range(int(23.0 / _DT)):
    _S_TAB.append(_S_TAB[-1] + rider_speed((_i + 0.5) * _DT) * _DT)


def rider_s(t):
    """Distance travelled along the skyway at time t."""
    if t <= 0:
        return rider_speed(0.0) * t
    i = min(t / _DT, len(_S_TAB) - 1.001)
    i0 = int(i)
    return _S_TAB[i0] + (_S_TAB[i0 + 1] - _S_TAB[i0]) * (i - i0)


# ---------------------------------------------------------------- the route
DS = 0.5
S_END = rider_s(DUR) + 200
_path = []                          # (s, x, y, heading, curvature)
x = y = h = 0.0
s = -60.0
while s <= S_END:
    k = (1 / RADIUS) * smooth((s - S1) / TRANS)
    _path.append((s, x, y, h, k))
    h += k * DS
    x += -math.sin(h) * DS
    y += math.cos(h) * DS
    s += DS
_path_s0 = _path[0][0]
# shift so that s=0 is at the origin
_off = next(p for p in _path if abs(p[0]) < 1e-6)
_path = [(p[0], p[1] - _off[1], p[2] - _off[2], p[3], p[4]) for p in _path]


def route(s):
    """Position, heading, curvature at distance s along the skyway centre line."""
    i = (s - _path_s0) / DS
    i0 = max(0, min(len(_path) - 2, int(math.floor(i))))
    f = i - i0
    a, b = _path[i0], _path[i0 + 1]
    lerp = lambda u, v: u + (v - u) * f
    return Vector((lerp(a[1], b[1]), lerp(a[2], b[2]), DECK_Z)), lerp(a[3], b[3]), lerp(a[4], b[4])


def frame_at(s, lateral=0.0, up=0.0):
    """World point offset sideways (right +) and up from the centre line."""
    p, hd, _ = route(s)
    right = Vector((math.cos(hd), math.sin(hd), 0))
    return p + right * lateral + Vector((0, 0, up)), hd


def bank(s):
    return math.radians(5) * route(s)[2] * RADIUS      # deck banks gently into the curve


# ---------------------------------------------------------------- materials
def road_mat():
    m = bpy.data.materials.new("deck")
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    uv = nt.nodes.new("ShaderNodeUVMap")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(uv.outputs["UV"], sep.inputs[0])
    u, v = sep.outputs[0], sep.outputs[1]
    # lane dashes at u = 1/3, 2/3; solid glowing edges
    lane = mnode(nt, "LESS_THAN", mnode(nt, "ABSOLUTE", mnode(nt, "SUBTRACT", u, 0.5)), 0.008)
    lane = mnode(nt, "MULTIPLY", lane, mnode(nt, "LESS_THAN", mnode(nt, "FRACT", mnode(nt, "DIVIDE", v, 1.0)), 0.45))
    edge = mnode(nt, "MAXIMUM", mnode(nt, "LESS_THAN", u, 0.012), mnode(nt, "GREATER_THAN", u, 0.988))
    glow = nt.nodes.new("ShaderNodeAttribute")
    glow.attribute_name = "glow"                      # per-vertex colour: district palette along the route
    nt.links.new(glow.outputs["Color"], b.inputs["Emission Color"])
    nt.links.new(mnode(nt, "ADD", mnode(nt, "MULTIPLY", edge, 9.0), mnode(nt, "MULTIPLY", lane, 3.0)),
                 b.inputs["Emission Strength"])
    puddle = nt.nodes.new("ShaderNodeTexNoise")
    puddle.inputs["Scale"].default_value, puddle.inputs["Detail"].default_value = 0.12, 4.0
    nt.links.new(tc.outputs["Object"], puddle.inputs["Vector"])
    pr = nt.nodes.new("ShaderNodeMapRange")
    pr.inputs["From Min"].default_value, pr.inputs["From Max"].default_value = 0.47, 0.56
    pr.inputs["To Min"].default_value, pr.inputs["To Max"].default_value = 0.45, 0.0
    nt.links.new(puddle.outputs["Fac"], pr.inputs["Value"])
    nt.links.new(pr.outputs["Result"], b.inputs["Roughness"])
    b.inputs["Base Color"].default_value = (0.006, 0.007, 0.01, 1)
    b.inputs["Metallic"].default_value = 0.0
    b.inputs["Coat Weight"].default_value = 1.0                 # the wet film: sharp neon reflections
    b.inputs["Coat Roughness"].default_value = 0.025
    b.inputs["Coat IOR"].default_value = 1.33
    rip = nt.nodes.new("ShaderNodeTexNoise")
    rip.noise_dimensions = "4D"
    rip.inputs["Scale"].default_value, rip.inputs["Detail"].default_value = 30.0, 0.0
    nt.links.new(tc.outputs["Object"], rip.inputs["Vector"])
    rip.inputs["W"].default_value = 0.0
    rip.inputs["W"].keyframe_insert("default_value", frame=1)
    rip.inputs["W"].default_value = 80.0
    rip.inputs["W"].keyframe_insert("default_value", frame=TOTAL)
    set_interp(m.node_tree)
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.05
    nt.links.new(rip.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    nt.links.new(bump.outputs["Normal"], b.inputs["Coat Normal"])
    return m


def windows_mat(name, base, warm_frac, lit_frac, strength, cell=(3.2, 3.6)):
    """Facades with a grid of windows; a random fraction lit."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*base, 1)
    b.inputs["Roughness"].default_value = 0.45
    b.inputs["Metallic"].default_value = 0.3
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep.inputs[0])
    obj = nt.nodes.new("ShaderNodeObjectInfo")
    u = mnode(nt, "DIVIDE", mnode(nt, "ADD", sep.outputs[0], sep.outputs[1]), cell[0])
    v = mnode(nt, "DIVIDE", sep.outputs[2], cell[1])
    fu, fv = mnode(nt, "FRACT", u), mnode(nt, "FRACT", v)
    mask = mnode(nt, "MULTIPLY",
                 mnode(nt, "MULTIPLY", mnode(nt, "GREATER_THAN", fu, 0.22), mnode(nt, "LESS_THAN", fu, 0.78)),
                 mnode(nt, "MULTIPLY", mnode(nt, "GREATER_THAN", fv, 0.3), mnode(nt, "LESS_THAN", fv, 0.8)))
    cellv = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(mnode(nt, "FLOOR", u), cellv.inputs[0])
    nt.links.new(mnode(nt, "FLOOR", v), cellv.inputs[1])
    nt.links.new(mnode(nt, "MULTIPLY", obj.outputs["Random"], 991.0), cellv.inputs[2])
    wn = nt.nodes.new("ShaderNodeTexWhiteNoise")
    wn.noise_dimensions = "3D"
    nt.links.new(cellv.outputs[0], wn.inputs["Vector"])
    lit = mnode(nt, "MULTIPLY", mask, mnode(nt, "GREATER_THAN", wn.outputs["Value"], 1.0 - lit_frac))
    sepc = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(wn.outputs["Color"], sepc.inputs[0])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.inputs["A"].default_value, mix.inputs["B"].default_value = (0.55, 0.75, 1.0, 1), (1.0, 0.62, 0.3, 1)
    nt.links.new(mnode(nt, "LESS_THAN", sepc.outputs[0], warm_frac), mix.inputs["Factor"])
    nt.links.new(mix.outputs["Result"], b.inputs["Emission Color"])
    nt.links.new(mnode(nt, "MULTIPLY", lit, strength), b.inputs["Emission Strength"])
    return m


def tower_mat():
    """Colossal future towers: dark glass, a fine grid of cool lit windows, accent bands every ~120 m."""
    m = windows_mat("tower", (0.008, 0.01, 0.016), warm_frac=0.12, lit_frac=0.32, strength=2.0, cell=(2.2, 4.2))
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Roughness"].default_value = 0.1
    b.inputs["Metallic"].default_value = 0.8
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep.inputs[0])
    obj = nt.nodes.new("ShaderNodeObjectInfo")
    band = mnode(nt, "LESS_THAN", mnode(nt, "FRACT", mnode(nt, "DIVIDE", sep.outputs[2], 120.0)), 0.006)
    old = b.inputs["Emission Strength"].links[0].from_socket
    oldc = b.inputs["Emission Color"].links[0].from_socket
    mixc = nt.nodes.new("ShaderNodeMix")
    mixc.data_type = "RGBA"
    nt.links.new(band, mixc.inputs["Factor"])
    nt.links.new(oldc, mixc.inputs["A"])
    nt.links.new(obj.outputs["Color"], mixc.inputs["B"])
    nt.links.new(mixc.outputs["Result"], b.inputs["Emission Color"])
    nt.links.new(mnode(nt, "ADD", old, mnode(nt, "MULTIPLY", band, 9.0)), b.inputs["Emission Strength"])
    return m


def ground_mat():
    """The street grid far below: dark blocks, glowing avenues and streets, warm and cool."""
    m = bpy.data.materials.new("ground")
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.01, 0.01, 0.012, 1)
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep.inputs[0])
    ax = mnode(nt, "FRACT", mnode(nt, "DIVIDE", sep.outputs[0], 90.0))
    st = mnode(nt, "FRACT", mnode(nt, "DIVIDE", sep.outputs[1], 34.0))
    avenue = mnode(nt, "LESS_THAN", ax, 0.16)
    street = mnode(nt, "LESS_THAN", st, 0.3)
    nt.links.new(mnode(nt, "ADD", mnode(nt, "MULTIPLY", avenue, 9.0), mnode(nt, "MULTIPLY", street, 3.5)),
                 b.inputs["Emission Strength"])
    b.inputs["Emission Color"].default_value = (1.0, 0.68, 0.35, 1)
    return m


def glass_mat():
    m = bpy.data.materials.new("glass")
    m.use_nodes = True
    m.surface_render_method = "BLENDED"
    m.use_backface_culling = False
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.3, 0.35, 0.4, 1)
    b.inputs["Alpha"].default_value = 0.07
    b.inputs["Roughness"].default_value = 0.04
    b.inputs["Metallic"].default_value = 0.0
    tc = nt.nodes.new("ShaderNodeTexCoord")
    # rain running sideways across the glass in the wind
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (1.0, 0.25, 1.0)
    nt.links.new(tc.outputs["UV"], mp.inputs["Vector"])
    mp.inputs["Location"].default_value = (0, 0, 0)
    mp.inputs["Location"].keyframe_insert("default_value", frame=1)
    mp.inputs["Location"].default_value = (0, -60.0, 0)
    mp.inputs["Location"].keyframe_insert("default_value", frame=TOTAL)
    set_interp(m.node_tree)
    vo = nt.nodes.new("ShaderNodeTexVoronoi")
    vo.inputs["Scale"].default_value = 40.0
    nt.links.new(mp.outputs["Vector"], vo.inputs["Vector"])
    drop = mnode(nt, "SUBTRACT", 1.0, mnode(nt, "MINIMUM", mnode(nt, "DIVIDE", vo.outputs["Distance"], 0.35), 1.0))
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.6
    nt.links.new(drop, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m


def facade_mat(name, file, tile_w, tile_h, glow, threshold, accent=False):
    """Image facade projected in world space on vertical faces; bright pixels (lit windows) glow."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Roughness"].default_value = 0.35
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep.inputs[0])
    nsep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Normal"], nsep.inputs[0])
    # horizontal coordinate runs along the face: y on faces facing x, x on faces facing y
    fx = mnode(nt, "GREATER_THAN", mnode(nt, "ABSOLUTE", nsep.outputs[0]), 0.5)
    u = mnode(nt, "ADD", mnode(nt, "MULTIPLY", fx, sep.outputs[1]), mnode(nt, "MULTIPLY", mnode(nt, "SUBTRACT", 1.0, fx), sep.outputs[0]))
    obj = nt.nodes.new("ShaderNodeObjectInfo")
    comb = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(mnode(nt, "DIVIDE", u, tile_w), comb.inputs[0])
    nt.links.new(mnode(nt, "DIVIDE", mnode(nt, "ADD", sep.outputs[2], mnode(nt, "MULTIPLY", obj.outputs["Random"], 97.0)), tile_h), comb.inputs[1])
    img = nt.nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(os.path.join(ROOT, "textures", file), check_existing=True)
    img.interpolation = "Cubic"
    nt.links.new(comb.outputs[0], img.inputs["Vector"])
    nt.links.new(img.outputs["Color"], b.inputs["Base Color"])
    bw = nt.nodes.new("ShaderNodeRGBToBW")
    nt.links.new(img.outputs["Color"], bw.inputs[0])
    lit = mnode(nt, "MAXIMUM", mnode(nt, "SUBTRACT", bw.outputs[0], threshold), 0.0)
    nt.links.new(img.outputs["Color"], b.inputs["Emission Color"])
    strength = mnode(nt, "MULTIPLY", lit, glow)
    if accent:
        band = mnode(nt, "LESS_THAN", mnode(nt, "FRACT", mnode(nt, "DIVIDE", sep.outputs[2], 120.0)), 0.006)
        mixc = nt.nodes.new("ShaderNodeMix")
        mixc.data_type = "RGBA"
        nt.links.new(band, mixc.inputs["Factor"])
        nt.links.new(img.outputs["Color"], mixc.inputs["A"])
        nt.links.new(obj.outputs["Color"], mixc.inputs["B"])
        nt.links.new(mixc.outputs["Result"], b.inputs["Emission Color"])
        strength = mnode(nt, "ADD", strength, mnode(nt, "MULTIPLY", band, 10.0))
        b.inputs["Metallic"].default_value = 0.6
        b.inputs["Roughness"].default_value = 0.15
    nt.links.new(strength, b.inputs["Emission Strength"])
    return m


M_DECK = road_mat()
M_UNDER = principled("deck_under", (0.02, 0.022, 0.03), rough=0.4, metal=0.6)
M_RAIL = principled("rail", (0.05, 0.055, 0.07), rough=0.25, metal=0.8)
M_GLASS = glass_mat()
M_OLD = facade_mat("old", "facade-old.png", 13.0, 15.0, glow=9.0, threshold=0.28)
M_LOW = windows_mat("low", (0.01, 0.011, 0.014), warm_frac=0.6, lit_frac=0.25, strength=2.2)
M_TOWER = facade_mat("tower", "facade-tower.png", 26.0, 30.0, glow=6.0, threshold=0.2, accent=True)
M_GROUND = ground_mat()
M_WOOD = principled("water_tower", (0.09, 0.06, 0.04), rough=0.8)
M_IRON = principled("iron", (0.02, 0.02, 0.02), rough=0.6, metal=0.8)

# district palette along the route: blue on the open skyway, teal and amber through the curve
PAL = [(0, (0.25, 0.55, 1.0)), (S1, (0.3, 0.6, 1.0)), (GLASS_S + 40, (0.1, 0.95, 0.85)), (S_END, (0.15, 1.0, 0.8))]
AMBER = (1.0, 0.55, 0.18)


def palette(s):
    for (s0, c0), (s1, c1) in zip(PAL, PAL[1:]):
        if s <= s1:
            k = smooth((s - s0) / max(1, s1 - s0))
            return tuple(a + (b - a) * k for a, b in zip(c0, c1))
    return PAL[-1][1]


# ---------------------------------------------------------------- skyway
def sweep(name, profile, s0, s1, step, mat, uv_scale=14.0, colour=True):
    """Sweep a cross-section [(lateral, up), ...] along the route."""
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    cl = bm.loops.layers.color.new("glow") if colour else None
    rows, ss = [], []
    s = s0
    while s <= s1 + 1e-6:
        p, hd, _ = route(s)
        rl = bank(s)
        right = Vector((math.cos(hd), math.sin(hd), 0))
        up = Vector((0, 0, 1))
        rr = right * math.cos(rl) + up * math.sin(rl)
        uu = up * math.cos(rl) - right * math.sin(rl)
        rows.append([bm.verts.new(p + rr * a + uu * b) for a, b in profile])
        ss.append(s)
        s += step
    n = len(profile)
    for i in range(len(rows) - 1):
        for j in range(n - 1):
            f = bm.faces.new((rows[i][j], rows[i][j + 1], rows[i + 1][j + 1], rows[i + 1][j]))
            for lp, (ii, jj) in zip(f.loops, ((i, j), (i, j + 1), (i + 1, j + 1), (i + 1, j))):
                lp[uvl].uv = (jj / (n - 1), ss[ii] / uv_scale)
                if cl:
                    lp[cl] = (*palette(ss[ii]), 1)
    return mesh_from_bm(name, bm, mat)


half = DECK_W / 2
sweep("deck", [(-half, 0), (half, 0)], -40, S_END, 2.0, M_DECK, uv_scale=6.0)
sweep("deck_under", [(half, 0), (half, -1.2), (-half, -1.2), (-half, 0)], -40, S_END, 4.0, M_UNDER, colour=False)
M_STRIP = bpy.data.materials.new("strip")
M_STRIP.use_nodes = True
_nt = M_STRIP.node_tree
_b = _nt.nodes["Principled BSDF"]
_a = _nt.nodes.new("ShaderNodeAttribute")
_a.attribute_name = "glow"
_nt.links.new(_a.outputs["Color"], _b.inputs["Emission Color"])
_b.inputs["Emission Strength"].default_value = 14.0
_b.inputs["Base Color"].default_value = (0, 0, 0, 1)
for sd in (-1, 1):
    x0 = sd * half
    sweep(f"rail{sd}", [(x0 + sd * 0.25, 0.12), (x0, 0.12), (x0, 0.0), (x0 + sd * 0.25, -1.2)], -40, S_END, 4.0, M_RAIL, colour=False)
    sweep(f"balustrade{sd}", [(x0 + sd * 0.1, 0.12), (x0 + sd * 0.1, 1.0)], -40, GLASS_S, 2.0, M_GLASS, uv_scale=12.0, colour=False)
    sweep(f"toprail{sd}", [(x0 + sd * 0.04, 1.0), (x0 + sd * 0.16, 1.0), (x0 + sd * 0.16, 1.06), (x0 + sd * 0.04, 1.06)],
          -40, S_END, 2.0, M_RAIL, colour=False)
    sweep(f"railstrip{sd}", [(x0 - sd * 0.02, 0.92), (x0 - sd * 0.02, 1.0)], -40, S_END, 2.0, M_STRIP)
    sweep(f"kerbstrip{sd}", [(x0 - sd * 0.05, 0.02), (x0 - sd * 0.25, 0.02)], -40, S_END, 2.0, M_STRIP)
    sweep(f"glasswall{sd}", [(x0 + sd * 0.1, 0.12), (x0 + sd * 0.1, 4.6), (x0 - sd * 0.9, 6.0)], GLASS_S, S_END, 2.0,
          M_GLASS, uv_scale=12.0, colour=False)

# lampposts (Tripo): obelisks along the open skyway, swan-neck lamps arcing over the glass section
HAVE_LAMPS = os.path.exists(os.path.join(ROOT, "models", "lamp-a.glb"))
s = 10.0
k = 0
while s < S_END:
    for sd in (-1, 1):
        if s >= GLASS_S and HAVE_LAMPS:
            SC = 8.2                                           # 8 m tall; the arm (model +X) reaches over the road
            pos, hd = frame_at(s, sd * (half + 0.3 - 0.25 * SC), 0.0)
            model_at("lamp_a", "lamp-a", pos, hd + (math.pi if sd > 0 else 0.0), SC, ratio=0.1, glow=("strip", 10.0))
            lp, _ = frame_at(s, sd * 1.6, 7.4)
            ld = bpy.data.lights.new("lamp_l", "POINT")
            ld.energy, ld.color, ld.shadow_soft_size, ld.use_shadow = 500.0, (0.2, 1.0, 0.85), 0.6, False
            link(bpy.data.objects.new("lamp_l", ld)).location = lp
        elif HAVE_LAMPS:
            SC = 7.2
            pos, hd = frame_at(s, sd * (half + 0.95), -0.9)
            model_at("lamp_b", "lamp-b", pos, hd, SC, ratio=0.1, glow=("lum", 0.55, 0.9, 6.0))
            ld = bpy.data.lights.new("lamp_l", "POINT")
            ld.energy, ld.color, ld.shadow_soft_size, ld.use_shadow = 260.0, (1.0, 0.85, 0.6), 0.5, False
            link(bpy.data.objects.new("lamp_l", ld)).location = pos + Vector((0, 0, 5.0))
        else:
            pos, hd = frame_at(s, sd * (half + 0.15), 3.0)
            box("rib", (0.18, 0.18, 6.2), pos, M_RAIL, rot=(0, 0, hd))
    s += 16.0 if s >= GLASS_S else 26.0
    k += 1

# support pylons down to the street
s = -30.0
while s < S_END:
    pos, hd = frame_at(s, 0, -80)
    box("pylon", (5.0, 5.0, 160.0), pos, M_UNDER, rot=(0, 0, hd))
    s += 70.0

# glyph road markings, glowing in the lane ahead
GLYPHS = [os.path.join(ROOT, "signs", f"word-{i}.png") for i in (1, 2, 4, 5, 6)]


def glyph_mat(path, colour, strength):
    m = bpy.data.materials.new("glyph")
    m.use_nodes = True
    nt = m.node_tree
    out = nt.nodes["Material Output"]
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    img = nt.nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(path, check_existing=True)
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (*colour, 1)
    nt.links.new(mnode(nt, "MULTIPLY", img.outputs["Color"], strength), em.inputs["Strength"])
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    mix = nt.nodes.new("ShaderNodeMixShader")
    sepc = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(img.outputs["Color"], sepc.inputs[0])
    nt.links.new(sepc.outputs[0], mix.inputs[0])
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(em.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return m


def decal(name, path, size, pos, rot, colour, strength=6.0):
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    w, hgt = size
    vs = [bm.verts.new(Vector(c)) for c in ((-w / 2, -hgt / 2, 0), (w / 2, -hgt / 2, 0), (w / 2, hgt / 2, 0), (-w / 2, hgt / 2, 0))]
    f = bm.faces.new(vs)
    for lp, uv in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
        lp[uvl].uv = uv
    ob = mesh_from_bm(name, bm, glyph_mat(path, colour, strength))
    ob.location, ob.rotation_euler = pos, rot
    return ob


for i, s in enumerate((70, 190, 330, 470, 610, 750)):
    lane = random.choice((-1, 1)) * DECK_W / 6 if i % 2 else 0.0
    pos, hd = frame_at(s, lane, 0.03)
    img = GLYPHS[i % len(GLYPHS)]
    decal("roadglyph", img, (4.2, 4.2 * 90 / 320 * 1.9), pos, (0, -bank(s), hd + math.pi), palette(s), 5.0)

# holographic street signs (Tripo): floating rings, totem pylons, projector gantries
HOLO = [(1.0, 0.1, 0.65), (0.1, 0.9, 1.0), (1.0, 0.55, 0.1), (0.6, 0.25, 1.0)]


def holo_light(pos, rot, colour, size, energy):
    ld = bpy.data.lights.new("holo_l", "AREA")
    ld.energy, ld.color, ld.size, ld.use_shadow = energy, colour, size, False
    lo = link(bpy.data.objects.new("holo_l", ld))
    lo.location, lo.rotation_euler = pos, rot


s = 40.0
k = 0
while s < S_END - 40:
    sd = 1 if k % 2 else -1
    c = HOLO[k % len(HOLO)]
    if k % 2 == 0:                                             # floating ring with a glyph word inside it
        D = random.uniform(7.0, 10.0)
        pos, hd = frame_at(s, sd * (half + random.uniform(9.0, 16.0)), random.uniform(8.0, 15.0))
        yaw = hd - sd * math.radians(22)
        model_at("sign_ring", "sign-a", pos - Vector((0, 0, D / 2)), yaw, D / 0.98, ratio=0.15, glow=("strip", 12.0))
        decal("ring_glyph", GLYPHS[k % len(GLYPHS)], (D * 0.72, D * 0.2), pos, (math.pi / 2, 0, yaw + math.pi), c, 9.0)
        holo_light(pos, (math.pi / 2, 0, yaw + math.pi), c, D * 0.6, 700.0)
    else:                                                      # totem pylon standing off the deck edge
        H = random.uniform(7.5, 9.5)
        pos, hd = frame_at(s, sd * (half + 1.3), -1.0)
        model_at("sign_totem", "sign-b", pos, hd + (math.pi if sd < 0 else 0.0), H / 0.98, ratio=0.15, glow=("strip", 12.0))
        holo_light(pos + Vector((0, 0, H * 0.6)), (math.pi / 2, 0, hd + math.pi), (1.0, 0.15, 0.8), 2.5, 500.0)
    s += random.uniform(34, 60)
    k += 1

# projector gantries spanning the skyway
for i, s in enumerate((140, 290, 440, 590, 740)):
    SC = (DECK_W + 1.6) / 0.98
    pos, hd = frame_at(s, 0, 0.0)
    if model_at("sign_gantry", "sign-c", pos, hd, SC, ratio=0.15, glow=("strip", 12.0)) is None:
        box("gantry", (DECK_W + 0.6, 0.4, 0.35), pos + Vector((0, 0, 6.7)), M_RAIL, rot=(0, 0, hd))
    gp, _ = frame_at(s - 0.6, 0, 0.42 * SC)
    decal("gantry_glyph", GLYPHS[(i + 2) % len(GLYPHS)], (6.4, 1.8), gp, (math.pi / 2, 0, hd + math.pi), (1.0, 0.7, 0.3), 7.0)
    holo_light(gp, (math.pi / 2, 0, hd + math.pi), (1.0, 0.7, 0.3), 5.0, 600.0)

# ---------------------------------------------------------------- the city
def near_route(pt, margin):
    """Distance from a ground point to the route polyline (coarse)."""
    best = 1e9
    for s in range(-60, int(S_END) + 40, 10):
        p, _, _ = route(s)
        best = min(best, (Vector((pt[0], pt[1], 0)) - Vector((p.x, p.y, 0))).length)
        if best < margin:
            return best
    return best


cube = bpy.data.meshes.new("unit")
_bm = bmesh.new()
bmesh.ops.create_cube(_bm, size=1.0)
_bm.to_mesh(cube)
_bm.free()


def mesh_box(mat):
    me = cube.copy()
    me.materials.clear()
    me.materials.append(mat)
    return me


ME_LOW, ME_OLD, ME_TOWER = mesh_box(M_LOW), mesh_box(M_OLD), mesh_box(M_TOWER)
ground = box("ground", (12000, 12000, 1), (-200, 1500, GROUND_Z - 0.5), M_GROUND)

# low city: the glowing grid's blocks, mostly far below the skyway
n_low = 0
for gx in range(-2000, 1700, 90):
    for gy in range(-700, 2600, 34):
        if random.random() < 0.45:
            continue
        cx, cy = gx + 45 + random.uniform(-8, 8), gy + 17
        hgt = random.choice((random.uniform(5, 16), random.uniform(12, 30)))
        if near_route((cx, cy), 40) < 40 and hgt > 120:
            continue
        box("low", (random.uniform(28, 58), random.uniform(10, 20), hgt), (cx, cy, GROUND_Z + hgt / 2), None, mesh=ME_LOW)
        n_low += 1

# preserved old buildings right beside the skyway, roofs near deck level, water towers on top
WT = None


def water_tower(at, scale=1.0):
    me = glb_mesh("water-tower")
    if me is not None:
        ob = link(bpy.data.objects.new("water_tower", me))
        ob.location, ob.scale = at, (12.0 * scale,) * 3
        ob.rotation_euler = (0, 0, random.uniform(0, 6.28))
        return ob
    root = empty("water_tower", at)
    root.scale = (scale,) * 3
    for i in range(4):
        a = i * math.pi / 2 + math.pi / 4
        box("wt_leg", (0.25, 0.25, 4.0), (math.cos(a) * 2.0, math.sin(a) * 2.0, 2.0), M_IRON, root)
    me = bpy.data.meshes.new("wt_tank")
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=20, radius1=2.6, radius2=2.6, depth=4.6)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(M_WOOD)
    tank = link(bpy.data.objects.new("wt_tank", me), root)
    tank.location = (0, 0, 6.3)
    me2 = bpy.data.meshes.new("wt_roof")
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=20, radius1=2.9, radius2=0.1, depth=2.0)
    bm.to_mesh(me2)
    bm.free()
    me2.materials.append(M_IRON)
    roof = link(bpy.data.objects.new("wt_roof", me2), root)
    roof.location = (0, 0, 9.6)
    return root


n_old = 0
for sd in (-1, 1):
    s = -20.0
    while s < S_END:
        w = random.uniform(22, 40)
        gap = random.uniform(70, 190)
        dist = half + random.uniform(6, 30)
        top = random.uniform(-10, 16)
        pos, hd = frame_at(s + w / 2, sd * (dist + 12), 0)
        hgt = top - GROUND_Z
        depth = random.uniform(22, 34)
        if not (GLASS_S - 30 < s < GLASS_S + 30 and sd == -1 and random.random() < 0.5):
            crown = glb_mesh("building-crown")
            CH = 26.0                                    # crown height in metres
            if crown is not None:
                box("old", (depth, w, hgt - CH * 0.9), (pos.x, pos.y, GROUND_Z + (hgt - CH * 0.9) / 2), None, rot=(0, 0, hd), mesh=ME_OLD)
                c = link(bpy.data.objects.new("crown", crown))
                c.location = (pos.x, pos.y, top - CH)
                c.rotation_euler = (0, 0, hd + (math.pi / 2 if sd > 0 else -math.pi / 2))
                c.scale = (w / 0.81, depth / 0.89, CH / 0.98)
            else:
                box("old", (depth, w, hgt), (pos.x, pos.y, GROUND_Z + hgt / 2), None, rot=(0, 0, hd), mesh=ME_OLD)
                box("cornice", (depth + 1.2, w + 1.2, 1.0), (pos.x, pos.y, top + 0.5), M_IRON, rot=(0, 0, hd))
            n_old += 1
            if random.random() < 0.75:
                off = Vector((random.uniform(-depth / 4, depth / 4), random.uniform(-w / 4, w / 4), 0))
                water_tower(Vector((pos.x, pos.y, top + 1.0)) + Matrix.Rotation(hd, 3, "Z") @ off, random.uniform(0.9, 1.3))
            for _ in range(random.randint(0, 3)):                  # chimneys of steam
                pass
        s += w + gap

# colossal towers rising far above the skyway
n_tower = 0
TOWER_COLS = [(0.4, 0.65, 1.0), (0.6, 0.45, 1.0), (0.2, 0.9, 0.85), AMBER]
for _ in range(0):
    cx, cy = random.uniform(-4200, 3600), random.uniform(-1200, 4800)
    d = near_route((cx, cy), 950)
    if d < 950:
        continue
    hgt = random.uniform(400, 1300)
    wdt = random.uniform(35, 80)
    t = box("tower", (wdt, wdt * random.uniform(0.6, 1.0), hgt), (cx, cy, GROUND_Z + hgt / 2), None,
            rot=(0, 0, random.uniform(0, math.pi)), mesh=ME_TOWER)
    t.color = (*random.choice(TOWER_COLS), 1)
    n_tower += 1

def skyline(name, file, centre, facing, width):
    path = os.path.join(ROOT, "textures", file)
    if not os.path.exists(path):
        return
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    m.surface_render_method = "BLENDED"
    nt = m.node_tree
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    img = nt.nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(path)
    img.extension = "EXTEND"
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Strength"].default_value = arg("--skyline", 7.0, float)
    nt.links.new(img.outputs["Color"], em.inputs["Color"])
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    mix = nt.nodes.new("ShaderNodeMixShader")
    uv = nt.nodes.new("ShaderNodeUVMap")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(uv.outputs["UV"], sep.inputs[0])
    edge = mnode(nt, "MINIMUM", mnode(nt, "MINIMUM", mnode(nt, "MULTIPLY", sep.outputs[0], 6.0),
                                       mnode(nt, "MULTIPLY", mnode(nt, "SUBTRACT", 1.0, sep.outputs[0]), 6.0)), 1.0)
    nt.links.new(edge, mix.inputs[0])
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(em.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], nt.nodes["Material Output"].inputs["Surface"])
    h = width / 1.5
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    vs = [bm.verts.new(Vector(c)) for c in ((-width / 2, 0, 0), (width / 2, 0, 0), (width / 2, 0, h), (-width / 2, 0, h))]
    f = bm.faces.new(vs)
    for lp, uvv in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
        lp[uvl].uv = uvv
    ob = mesh_from_bm(name, bm, m)
    ob.location = (centre[0], centre[1], -0.15 * h)
    ob.rotation_euler = (0, 0, facing)


skyline("skyline_a", "skyline-a.png", (200, 3800), 0.0, 8200)                   # ahead on the straight
skyline("skyline_b", "skyline-b.png", (-3600, 700), -math.pi / 2, 8200)        # ahead through the curve


# towers (Tripo): three designs fill the midground and skyline, lit windows glowing
TOWER_KINDS = [k for k in ("tower-a", "tower-b", "tower-c") if os.path.exists(os.path.join(ROOT, "models", k + ".glb"))]
n_mid = 0
for _ in range(1500):
    if n_mid >= 120 or not TOWER_KINDS:
        break
    cx, cy = random.uniform(-2600, 2200), random.uniform(-300, 3000)
    d = near_route((cx, cy), 300)
    if d < 300 or d > 2300:
        continue
    hgt = random.uniform(280, 520) if d < 700 else random.uniform(380, 900)
    kind = TOWER_KINDS[n_mid % len(TOWER_KINDS)]
    model_at("tower", kind, (cx, cy, GROUND_Z), random.uniform(0, 2 * math.pi), hgt / 0.98, ratio=0.12,
             glow=("lum", 0.22, 0.7, 7.0))
    n_mid += 1
print("MIDTOWERS", n_mid)


# ---------------------------------------------------------------- flying traffic
M_CAR = principled("car", (0.02, 0.022, 0.03), rough=0.15, metal=0.9)
M_HEAD = emissive("headlight", (0.85, 0.92, 1.0), 120.0)
M_TAIL = emissive("taillight", (1.0, 0.05, 0.08), 80.0)


# model name, length in metres, yaw to face +Y, light colour under the car
CAR_KINDS = [("car", 5.0, math.pi, (0.35, 0.75, 1.0)),          # sleek black (storyboard)
             ("car-a", 5.6, math.pi, (1.0, 0.75, 0.4)),          # pearl luxury coupe
             ("car-b", 9.0, math.pi, (0.4, 0.9, 1.0)),           # heavy hauler
             ("car-c", 6.5, math.pi / 2, (1.0, 0.2, 0.75))]      # interceptor
_car_i = [0]


def flying_car(name, kind=None):
    root = empty(name)
    avail = [k for k in CAR_KINDS if os.path.exists(os.path.join(ROOT, "models", k[0] + ".glb"))]
    if kind is None and avail:
        kind = avail[_car_i[0] % len(avail)]
        _car_i[0] += 1
    me = glb_mesh(kind[0]) if kind else None
    under = kind[3] if kind else (0.3, 0.6, 1.0)
    if me is not None:
        if not me.get("glow"):
            for m in me.materials:
                strip_glow(m, 22.0)
            me["glow"] = 1
        body = link(bpy.data.objects.new(name + "_body", me), root)
        body.scale = (kind[1],) * 3
        body.rotation_euler = (0, 0, kind[2])
        body.location = (0, 0, -0.5)
    else:
        box("body", (2.0, 4.6, 0.9), (0, 0, 0), M_CAR, root)
        box("canopy", (1.5, 2.0, 0.55), (0, -0.3, 0.6), M_CAR, root)
    for sx in (-0.75, 0.75):
        box("head", (0.35, 0.08, 0.12), (sx, 2.32, 0.05), M_HEAD, root)
        box("tail", (0.5, 0.08, 0.1), (sx, -2.32, 0.1), M_TAIL, root)
    box("underglow", (1.6, 3.8, 0.05), (0, 0, -0.47), emissive("under", (0.3, 0.6, 1.0), 14.0), root)
    glow = bpy.data.lights.new(name + "_glow", "POINT")              # each car lights the wet deck as it passes
    glow.energy, glow.color, glow.shadow_soft_size = 900.0, under, 1.5
    glow.use_shadow = False
    gl = link(bpy.data.objects.new(name + "_glow", glow), root)
    gl.location = (0, 0, -1.2)
    return root


TRAFFIC = []                        # (root, lane lateral, height, s0, speed)
lanes = [(11.0, 3.0, 29.0), (-11.5, 4.5, 26.0), (16.0, 9.0, 32.0), (-18.0, 12.0, 24.0), (24.0, 18.0, 30.0),
         (-14.0, 7.0, -34.0), (-30.0, 26.0, 33.0), (34.0, 32.0, 22.0)]          # negative speed = oncoming
for li, (lat, up, vel) in enumerate(lanes):
    for k in range(7):
        # spread each lane so cars are in view across the ride: start where they will meet the rider
        meet = random.uniform(0.5, DUR - 1.0)
        s0 = rider_s(meet) - vel * meet + random.uniform(-15, 15)
        TRAFFIC.append((flying_car(f"car{li}_{k}"), lat, up, s0, vel * random.uniform(0.9, 1.1)))
# road vehicles (Tripo): slower trucks and cybercabs in both lanes; the rider weaves through and passes them all
ROAD_KINDS = {"cab": (4.8, arg("--cab_yaw", 0.0, float)), "truck": (13.0, arg("--truck_yaw", 0.0, float))}
ROAD = []


def road_vehicle(name, kind):
    length, yaw = ROAD_KINDS[kind]
    me = glb_mesh(kind, 0.5)
    if me is None:
        return None
    if not me.get("glow"):
        for m in me.materials:
            strip_glow(m, 14.0)
        me["glow"] = 1
    root = empty(name)
    body = link(bpy.data.objects.new(name + "_body", me), root)
    body.scale = (length / 0.98,) * 3
    body.rotation_euler = (0, 0, math.radians(yaw))
    for nm, y, colr, en in (("tail", -length / 2 - 0.4, (1.0, 0.05, 0.05), 350.0), ("head", length / 2 + 0.6, (0.9, 0.95, 1.0), 500.0)):
        ld = bpy.data.lights.new(name + "_" + nm, "POINT")
        ld.energy, ld.color, ld.shadow_soft_size, ld.use_shadow = en, colr, 0.5, False
        link(bpy.data.objects.new(name + "_" + nm, ld), root).location = (0, y, 0.9)
    return root


# the lane changes: (time at mid-change, target lane). Each is a minimum-jerk flick, so the lean rolls in,
# snaps through upright and rolls out smoothly, the way a countersteered bike moves.
LANE_CHANGES = [(2.0, LEFT_LANE, 2.2), (6.5, LANE, 2.2), (16.2, LEFT_LANE, 2.6), (18.2, LANE, 2.6)]


def rider_lat(t):
    # 7th-order ease: lateral acceleration (and so the lean) starts and ends with zero rate, so the bike
    # rolls in, snaps through upright and stands back up without a kink
    lat = LANE
    for tc, target, dur in LANE_CHANGES:
        u = (t - (tc - dur / 2)) / dur
        k = 0.0 if u <= 0 else 1.0 if u >= 1 else u ** 4 * (35 - 84 * u + 70 * u * u - 20 * u ** 3)
        lat += (target - lat) * k
    return lat


# name, kind, lane, time the rider draws level and passes it, its speed (the rider does 40 m/s)
for nm, kind, lane, t_pass, vel in (("truck1", "truck", LANE, 3.2, 24.0), ("cab1", "cab", LANE, 5.0, 28.0),
                                    ("truck2", "truck", LEFT_LANE, 7.9, 24.0),
                                    ("cab3", "cab", LANE, 17.0, 28.0), ("truck3", "truck", LEFT_LANE, 19.1, 24.0)):
    rv = road_vehicle(nm, kind)
    if rv is not None:
        ROAD.append((rv, kind, lane, t_pass, vel))


# ---------------------------------------------------------------- the jellyfish (scripts/jelly.py)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jelly as jelly_mod
JELLY_R = 30.0
JF = jelly_mod.build(col, JELLY_R)
jelly = JF.root
JF.set_projector(52.0)


# ---------------------------------------------------------------- rider and bike stand-in
rider = empty("rider")
lean = empty("lean", parent=rider)
M_BIKE = principled("bike", (0.025, 0.025, 0.03), rough=0.3, metal=0.6)
M_GRIP = principled("grip", (0.01, 0.01, 0.01), rough=0.7)
M_GLOVE = principled("glove", (0.04, 0.04, 0.05), rough=0.35, metal=0.4)
bike = empty("bike", parent=lean)
box("bar", (0.78, 0.04, 0.04), (0, 0.62, 0.98), M_BIKE, bike)
for sd in (-1, 1):
    box("grip", (0.14, 0.06, 0.06), (sd * 0.36, 0.6, 0.98), M_GRIP, bike)
    box("glove", (0.11, 0.13, 0.1), (sd * 0.34, 0.58, 1.03), M_GLOVE, bike)
    box("mirror", (0.16, 0.02, 0.08), (sd * 0.42, 0.72, 1.18), M_BIKE, bike)
box("tank", (0.36, 0.55, 0.22), (0, 0.32, 0.82), M_BIKE, bike)
box("dash", (0.26, 0.02, 0.11), (0, 0.72, 1.04), emissive("hud", (0.15, 0.85, 1.0), 3.0), bike,
    rot=(math.radians(-55), 0, 0))
HUD = None
BIKE_GLB = os.path.join(ROOT, "models", "bike.glb")
BIKE_SCALE = 2.15                                   # Tripo model is ~0.98 m long; a real bike is ~2.1 m
GRIP_Y, GRIP_Z = -0.152, 0.463                      # handlebar grips in model space (model faces -Y)
if os.path.exists(BIKE_GLB) and not arg("--standin"):
    for ob in list(bike.children):
        if not ob.name.startswith("glove"):
            bpy.data.objects.remove(ob, do_unlink=True)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=BIKE_GLB)
    holder = empty("bike_model", parent=bike)
    holder.rotation_euler = (0, 0, math.pi)          # face +Y, the direction of travel
    holder.scale = (BIKE_SCALE,) * 3
    holder.location = (0, 0.58 + GRIP_Y * BIKE_SCALE, 0.0)   # grips 0.58 m ahead of the eye
    for ob in set(bpy.data.objects) - before:
        if ob.parent is None or ob.parent not in bpy.data.objects.values() or ob.parent.name.startswith("tripo"):
            pass
        if ob.type == "MESH":
            mw = ob.matrix_world.copy()
            ob.parent = holder
            ob.matrix_parent_inverse.identity()
            ob.matrix_basis = mw
    for ob in set(bpy.data.objects) - before:
        if ob.type == "MESH":
            for m in ob.data.materials:
                strip_glow(m, 6.0)
    for ob in set(bpy.data.objects) - before:
        if ob.type == "EMPTY" and not ob.children:
            bpy.data.objects.remove(ob, do_unlink=True)
    gme = glb_mesh("glove")
    if gme is not None:
        for m in gme.materials:
            strip_glow(m, 5.0)
        for o in [o for o in bike.children if o.name.startswith("glove")]:
            bpy.data.objects.remove(o, do_unlink=True)
        GS = 0.43                                       # gauntlet ~0.42 m from forearm to knuckles
        grip_z = GRIP_Z * BIKE_SCALE
        for sd in (-1, 1):
            g = link(bpy.data.objects.new(f"glove{sd}", gme), bike)
            g.scale = (GS * (1 if sd > 0 else -1), GS, GS)          # mirrored for the left hand
            g.rotation_euler = (math.radians(-84), 0, sd * math.radians(-9))
            g.location = (sd * 0.33, 0.58 - 0.82 * GS, grip_z - 0.02)
    # live instrument panel (scripts/hud.py), driven by the ride in animate()
    import hud as hud_mod
    HUD = hud_mod.build(link, bike, (0, 0.74, grip_z + 0.13), (math.radians(-35), 0, 0),
                        os.path.join(ROOT, "signs", "glyph-sheet.png"), [r[1] for r in ROAD])
hl = bpy.data.lights.new("headlight", "SPOT")
hl.energy, hl.spot_size, hl.spot_blend, hl.color = 3500, math.radians(38), 0.45, (0.85, 0.92, 1.0)
hlo = link(bpy.data.objects.new("headlight", hl), bike)
hlo.location, hlo.rotation_euler = (0, 0.9, 0.85), (math.radians(82), 0, 0)
wash = bpy.data.lights.new("cockpit_wash", "AREA")
wash.energy, wash.size, wash.color = 18.0, 1.4, (0.55, 0.75, 1.0)
wash.use_shadow = False
wo = link(bpy.data.objects.new("cockpit_wash", wash), lean)
wo.location, wo.rotation_euler = (0, 1.1, 2.2), (math.radians(-35), 0, 0)
head = empty("head", (0, 0.0, 1.5), lean)
cd = bpy.data.cameras.new("pov")
cd.lens, cd.clip_start, cd.clip_end = 17, 0.05, 6000
cam = link(bpy.data.objects.new("pov", cd), head)
scene.camera = cam

# ---------------------------------------------------------------- rain (loops seamlessly, see spike.py)
RAIN_FALL = 9.0
REL = Vector((0, -SPEED, -RAIN_FALL))
L = REL.length
alpha = math.atan2(RAIN_FALL, SPEED)
rain_axis = empty("rain_axis", (0, 0, 1.3), rider, rot=(alpha, 0, 0))
M_RAIN = principled("rain", (0.8, 0.85, 0.9), rough=0.05, emit=(0.6, 0.72, 0.9), strength=0.5)
bm = bmesh.new()
rot_w = rain_axis.rotation_euler.to_matrix()
for _ in range(9000):
    p = Vector((random.uniform(-9, 9), random.uniform(-2, L - 2), random.uniform(-8, 10)))
    for k in (0, 1):
        q = p + Vector((0, k * L, 0))
        w = Vector((0, -1, 0)).cross(q).normalized() * 0.005
        ln = Vector((0, -0.07, 0))
        bm.faces.new([bm.verts.new(q + a) for a in (-w - ln, w - ln, w + ln, -w + ln)])
drops = mesh_from_bm("drops", bm, M_RAIN, rain_axis)
for fr, yv in ((0.5, 0.0), (0.5 + FPS * L / L, -L)):
    drops.location = (0, yv, 0)
    drops.keyframe_insert("location", frame=fr)
fc = [f for f in fcurves_of(drops) if f.array_index == 1][0]
for kp in fc.keyframe_points:
    kp.interpolation = "LINEAR"
fc.modifiers.new("CYCLES")

# ---------------------------------------------------------------- animation
LOOK_T0, LOOK_T1 = 10.2, 15.6          # the rider turns to watch the jellyfish ad, then back to the road
_jp, _jh = frame_at(rider_s(15.3), 50.0, 0.0)
JELLY_XY = _jp                          # the hologram ad rises from the streets beside the curve
JELLY_Z0, JELLY_Z1, JELLY_T0, JELLY_T1 = -175.0, 34.0, 5.0, 16.0
EMITTER_Z = GROUND_Z + 30.0             # its projector sits on a rooftop far below


def jelly_z(t):
    """Climbs in surges from far below the deck to above it, crossing road level as the rider passes."""
    u = (t - JELLY_T0) / (JELLY_T1 - JELLY_T0)
    return JELLY_Z0 + (JELLY_Z1 - JELLY_Z0) * smooth(u) + 6.0 * max(0.0, t - JELLY_T1)


JELLY_POS = JELLY_XY + Vector((0, 0, 12.0))


def look_weight(t):
    return smooth((t - LOOK_T0) / 1.4) * (1 - smooth((t - LOOK_T1) / 1.6))


# the advert's glyph headline, floating under the bell and angled toward the approaching rider
for i, (dz, w, colr, word) in enumerate(((-16.0, 34.0, (1.0, 0.12, 0.7), 0), (-25.0, 22.0, (0.1, 0.9, 1.0), 2))):
    decal("ad_glyph", GLYPHS[word], (w, w * 0.28), JELLY_POS + Vector((0, 0, dz)),
          (math.pi / 2, 0, _jh + math.pi - 0.55), colr, 7.0)


def animate():
    for f in range(1, TOTAL + 1):
        t = (f - 1) / FPS
        s = rider_s(t)
        p, hd, k = route(s)
        lat_r = rider_lat(t)
        v_lat = (rider_lat(t + 0.05) - rider_lat(t - 0.05)) / 0.1
        a_lat = (rider_lat(t + 0.05) - 2 * lat_r + rider_lat(t - 0.05)) / 0.0025
        v = rider_speed(t)
        a_long = (rider_speed(t + 0.05) - rider_speed(t - 0.05)) / 0.1
        phi = math.atan((v * v * k - a_lat) / G)                  # lean into the curve and into each flick
        phi = max(-0.95, min(0.95, phi))
        if (f - 1) % STEP == 0 or f == TOTAL:
            rider.location = p + Vector((math.cos(hd), math.sin(hd), 0)) * lat_r
            rider.rotation_euler = (0, 0, hd - math.atan2(v_lat, v))
            rider.keyframe_insert("location", frame=f)
            rider.keyframe_insert("rotation_euler", frame=f)
            lean.rotation_euler = (0, -phi + bank(s), 0)
            lean.keyframe_insert("rotation_euler", frame=f)
            vec = JELLY_XY + Vector((0, 0, jelly_z(t) + 6.0)) - (p + Vector((0, 0, 1.5)))
            fwd_v = Vector((-math.sin(hd), math.cos(hd), 0))
            rgt_v = Vector((math.cos(hd), math.sin(hd), 0))
            ang = math.atan2(vec.dot(rgt_v), vec.dot(fwd_v))
            elev = math.atan2(vec.z, math.hypot(vec.dot(rgt_v), vec.dot(fwd_v)))
            w_ = look_weight(t)
            yaw = max(-1.25, min(1.25, ang)) * 0.9 * w_
            head.rotation_euler = (max(-0.1, min(0.45, elev)) * 0.8 * w_, phi * 0.42, -yaw)
            head.keyframe_insert("rotation_euler", frame=f)
            look = math.radians(-6) - 0.25 * phi                    # glance into the curve
            # the view: looks into each turn, tips back and is pressed into the seat under throttle, widens with speed
            cam.rotation_euler = (math.radians(78 + 0.12 * math.sin(t * 0.7) + 0.28 * a_long),
                                  0, math.radians(0.25 * math.sin(t * 0.35)) + 0.22 * phi)
            cam.location = (0, -0.014 * a_long, -0.004 * a_long)
            cam.keyframe_insert("rotation_euler", frame=f)
            cam.keyframe_insert("location", frame=f)
            cd.lens = 17.0 - 3.0 * smooth((v - 36.0) / 22.0)
            cd.keyframe_insert("lens", frame=f)
            if HUD:
                HUD.animate(f, t, v * 3.6, phi, lat_r,
                            [(rider_s(tp) + vv * (t - tp) - s, ln) for _r, _k, ln, tp, vv in ROAD])
            for root, _kind, lat, t_pass, vel in ROAD:
                s_v = rider_s(t_pass) + vel * (t - t_pass)
                pos, vhd = frame_at(s_v, lat, 0.0)
                root.location = pos
                root.rotation_euler = (0, -bank(s_v), vhd)
                root.keyframe_insert("location", frame=f)
                root.keyframe_insert("rotation_euler", frame=f)
            for root, lat, up, s0, vel in TRAFFIC:
                pos, chd = frame_at(s0 + vel * t, lat, up + 0.4 * math.sin(t * 0.9 + s0))
                root.location = pos
                root.rotation_euler = (0, 0, chd + (math.pi if vel < 0 else 0.0))
                root.keyframe_insert("location", frame=f)
                root.keyframe_insert("rotation_euler", frame=f)
            jz = jelly_z(t)
            jelly.location = JELLY_XY + Vector((0, 0, jz))
            jelly.keyframe_insert("location", frame=f)
            JF.set_projector(max(2.0, jz - EMITTER_Z))
            JF.key_projector(f)
            if (f - 1) % (2 * STEP) == 0 or f == TOTAL:
                JF.animate(f, t, rise=(jelly_z(t + 0.1) - jelly_z(t - 0.1)) / 0.2)
    set_interp(rider)
    if HUD:
        HUD.finish()
    for ob in (lean, head):
        set_interp(ob, "BEZIER")


animate()

# ---------------------------------------------------------------- atmosphere and light
world = bpy.data.worlds.new("world")
world.use_nodes = True
wnt = world.node_tree
bg = wnt.nodes["Background"]
grad = wnt.nodes.new("ShaderNodeTexGradient")
wtc = wnt.nodes.new("ShaderNodeTexCoord")
sepw = wnt.nodes.new("ShaderNodeSeparateXYZ")
wnt.links.new(wtc.outputs["Generated"], sepw.inputs[0])
ramp = wnt.nodes.new("ShaderNodeValToRGB")
ramp.color_ramp.elements[0].position, ramp.color_ramp.elements[1].position = 0.45, 0.62
ramp.color_ramp.elements[0].color, ramp.color_ramp.elements[1].color = (0.05, 0.035, 0.03, 1), (0.004, 0.006, 0.014, 1)
wnt.links.new(sepw.outputs[2], ramp.inputs[0])
clouds = wnt.nodes.new("ShaderNodeTexNoise")
clouds.inputs["Scale"].default_value, clouds.inputs["Detail"].default_value = 3.0, 8.0
clouds.inputs["Roughness"].default_value = 0.6
wnt.links.new(wtc.outputs["Generated"], clouds.inputs["Vector"])
cr = wnt.nodes.new("ShaderNodeMapRange")
cr.inputs["From Min"].default_value, cr.inputs["From Max"].default_value = 0.45, 0.75
nt_up = wnt.nodes.new("ShaderNodeMapRange")
nt_up.inputs["From Min"].default_value, nt_up.inputs["From Max"].default_value = 0.5, 0.58
wnt.links.new(sepw.outputs[2], nt_up.inputs["Value"])
wnt.links.new(clouds.outputs["Fac"], cr.inputs["Value"])
cmul = wnt.nodes.new("ShaderNodeMath")
cmul.operation = "MULTIPLY"
wnt.links.new(cr.outputs["Result"], cmul.inputs[0])
wnt.links.new(nt_up.outputs["Result"], cmul.inputs[1])
cmix = wnt.nodes.new("ShaderNodeMix")
cmix.data_type = "RGBA"
cmix.inputs["B"].default_value = (0.03, 0.045, 0.09, 1)
wnt.links.new(cmul.outputs[0], cmix.inputs["Factor"])
wnt.links.new(ramp.outputs[0], cmix.inputs["A"])
wnt.links.new(cmix.outputs["Result"], bg.inputs["Color"])
scene.world = world

hz = bpy.data.materials.new("haze")
hz.use_nodes = True
hnt = hz.node_tree
hnt.nodes.remove(hnt.nodes["Principled BSDF"])
vol = hnt.nodes.new("ShaderNodeVolumePrincipled")
vol.inputs["Density"].default_value = arg("--haze", 0.0008, float)
vol.inputs["Anisotropy"].default_value = 0.5
vol.inputs["Color"].default_value = (0.7, 0.75, 0.9, 1)
hnt.links.new(vol.outputs["Volume"], hnt.nodes["Material Output"].inputs["Volume"])
_tr = hnt.nodes.new("ShaderNodeBsdfTransparent")                  # the box walls themselves must be invisible
hnt.links.new(_tr.outputs[0], hnt.nodes["Material Output"].inputs["Surface"])
hz.surface_render_method = "BLENDED"
hzb = box("haze", (9000, 9000, 1800), (-200, 1500, GROUND_Z + 900), hz)
hzb.display_type = "WIRE"

# light spill from the deck strips onto the wet deck and the rider: soft lights every 30 m
s = 0.0
while s < S_END:
    for sd in (-1, 1):
        pos, _ = frame_at(s, sd * (half - 0.4), 1.4)
        ld = bpy.data.lights.new("strip_l", "POINT")
        ld.energy, ld.color, ld.shadow_soft_size, ld.use_shadow = 140, palette(s), 0.8, False
        ld.specular_factor = 0.0                       # light the deck, but no floating highlight blobs
        lo = link(bpy.data.objects.new("strip_l", ld))
        lo.location = pos
    s += 30.0

# warm uplight from the city below, cold moonlight from above
sun = bpy.data.lights.new("moon", "SUN")
sun.energy, sun.color, sun.angle = 0.06, (0.6, 0.7, 1.0), math.radians(3)
sun.use_shadow = False
link(bpy.data.objects.new("moon", sun)).rotation_euler = (math.radians(30), 0, math.radians(200))
up = bpy.data.lights.new("cityglow", "SUN")
up.energy, up.color = 0.05, (1.0, 0.6, 0.3)
up.use_shadow = False
link(bpy.data.objects.new("cityglow", up)).rotation_euler = (math.radians(180), 0, 0)

# ---------------------------------------------------------------- render
r = scene.render
r.engine = "BLENDER_EEVEE"
r.resolution_x, r.resolution_y, r.resolution_percentage = 1920, 1080, RES
r.fps = FPS
scene.frame_start, scene.frame_end = 1, TOTAL
ee = scene.eevee
ee.taa_render_samples = SAMPLES
ee.use_raytracing = True
ee.ray_tracing_method = "SCREEN"
try:
    rto = ee.ray_tracing_options
    rto.resolution_scale = "1"
    rto.trace_max_roughness = 0.4
    rto.screen_trace_quality = 1.0
    rto.screen_trace_thickness = 1.0
except Exception as e:
    print("raytrace opts:", e)
ee.volumetric_tile_size = "8" if RES < 100 else "4"
ee.volumetric_samples = 64
ee.volumetric_end = 1600.0
ee.use_volumetric_shadows = False
ee.use_shadows = True
r.use_motion_blur = True
r.motion_blur_shutter = 0.5
jelly_mod.add_bloom(scene, strength=arg("--bloom", 0.45, float), threshold=1.0, size=7)
try:
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Punchy"
except Exception as e:
    print("view:", e)
scene.view_settings.exposure = arg("--exposure", 0.6, float)
print(f"BUILT low={n_low} old={n_old} towers={n_tower} cars={len(TRAFFIC)} frames={TOTAL}")

if arg("--save"):
    r.filepath = "//frames/f_"                       # relative to the .blend, so it works on any machine
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, arg("--save")), compress=True)
    print("SAVED", arg("--save"))
os.makedirs(OUT, exist_ok=True)
if arg("--frames"):
    for fr in str(arg("--frames")).split(","):
        scene.frame_set(int(fr))
        r.filepath = os.path.join(OUT, f"f_{int(fr):04d}.png")
        bpy.ops.render.render(write_still=True)
if arg("--anim"):
    r.filepath = os.path.join(OUT, "f_")
    bpy.ops.render.render(animation=True)
