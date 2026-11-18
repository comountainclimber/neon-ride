"""Feasibility spike: wet neon street, rain at speed, first-person bike, all grey-box.

  blender -b --python scripts/spike.py -- --frames 40,120 --res 50 --samples 16
"""
import bpy, bmesh, math, os, random, sys
from mathutils import Vector

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def arg(name, default=None, cast=str):
    if name in argv:
        i = argv.index(name)
        return cast(argv[i + 1]) if i + 1 < len(argv) and not argv[i + 1].startswith("--") else True
    return default


RES, SAMPLES = arg("--res", 50, int), arg("--samples", 16, int)
OUT = os.path.join(ROOT, arg("--out", "renders/spike"))
FPS, DUR, SPEED = 24, 10.0, 20.0                 # 20 m/s = 72 km/h
RAIN_FALL = 9.0
TOTAL = int(DUR * FPS)
STREET = SPEED * DUR + 120
random.seed(3)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
col = scene.collection


def link(ob, parent=None):
    col.objects.link(ob)
    if parent:
        ob.parent = parent
    return ob


def empty(name, loc=(0, 0, 0), parent=None):
    ob = bpy.data.objects.new(name, None)
    ob.location = loc
    return link(ob, parent)


def box(name, size, loc, mat, parent=None, rot=(0, 0, 0)):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    ob.location, ob.rotation_euler = loc, rot
    return link(ob, parent)


def mat_basic(name, color, rough=0.5, metal=0.0, emit=None, strength=0.0):
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


def node(nt, kind, **inputs):
    n = nt.nodes.new(kind)
    for k, v in inputs.items():
        if k.startswith("op_"):
            setattr(n, k[3:], v)
        else:
            n.inputs[k].default_value = v
    return n


def math_node(nt, op, a, b=None):
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


# ---------------------------------------------------------------- materials
def building_mat():
    """Dark facades with a grid of windows, a random fraction lit warm or cool."""
    m = bpy.data.materials.new("facade")
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.012, 0.012, 0.015, 1)
    b.inputs["Roughness"].default_value = 0.55
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(geo.outputs["Position"], sep.inputs[0])
    obj = nt.nodes.new("ShaderNodeObjectInfo")
    u = math_node(nt, "DIVIDE", math_node(nt, "ADD", sep.outputs[0], sep.outputs[1]), 3.2)
    v = math_node(nt, "DIVIDE", sep.outputs[2], 3.6)
    fu, fv = math_node(nt, "FRACT", u), math_node(nt, "FRACT", v)
    mask = math_node(nt, "MULTIPLY",
                     math_node(nt, "MULTIPLY", math_node(nt, "GREATER_THAN", fu, 0.2), math_node(nt, "LESS_THAN", fu, 0.8)),
                     math_node(nt, "MULTIPLY", math_node(nt, "GREATER_THAN", fv, 0.3), math_node(nt, "LESS_THAN", fv, 0.78)))
    cell = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(math_node(nt, "FLOOR", u), cell.inputs[0])
    nt.links.new(math_node(nt, "FLOOR", v), cell.inputs[1])
    nt.links.new(math_node(nt, "MULTIPLY", obj.outputs["Random"], 997.0), cell.inputs[2])
    wn = nt.nodes.new("ShaderNodeTexWhiteNoise")
    wn.noise_dimensions = "3D"
    nt.links.new(cell.outputs[0], wn.inputs["Vector"])
    lit = math_node(nt, "MULTIPLY", mask, math_node(nt, "GREATER_THAN", wn.outputs["Value"], 0.74))
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    cr = ramp.color_ramp
    cr.elements[0].color, cr.elements[1].color = (1.0, 0.55, 0.25, 1), (0.35, 0.7, 1.0, 1)
    nt.links.new(wn.outputs["Color"], ramp.inputs[0])
    nt.links.new(ramp.outputs[0], b.inputs["Emission Color"])
    nt.links.new(math_node(nt, "MULTIPLY", lit, 2.5), b.inputs["Emission Strength"])
    return m


def road_mat():
    """Near-black asphalt; large puddle blobs are mirror-smooth, the rest semi-gloss; dashed centre line."""
    m = bpy.data.materials.new("road")
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    puddle = node(nt, "ShaderNodeTexNoise", Scale=0.18, Detail=4.0, Roughness=0.6)
    nt.links.new(tc.outputs["Object"], puddle.inputs["Vector"])
    pr = nt.nodes.new("ShaderNodeMapRange")
    pr.inputs["From Min"].default_value, pr.inputs["From Max"].default_value = 0.48, 0.56
    pr.inputs["To Min"].default_value, pr.inputs["To Max"].default_value = 0.32, 0.02
    nt.links.new(puddle.outputs["Fac"], pr.inputs["Value"])
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(tc.outputs["Object"], sep.inputs[0])
    dash = math_node(nt, "MULTIPLY", math_node(nt, "LESS_THAN", math_node(nt, "ABSOLUTE", sep.outputs[0]), 0.08),
                     math_node(nt, "LESS_THAN", math_node(nt, "FRACT", math_node(nt, "DIVIDE", sep.outputs[1], 6.0)), 0.5))
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.inputs["A"].default_value, mix.inputs["B"].default_value = (0.018, 0.018, 0.02, 1), (0.35, 0.35, 0.3, 1)
    nt.links.new(dash, mix.inputs["Factor"])
    nt.links.new(mix.outputs["Result"], b.inputs["Base Color"])
    nt.links.new(pr.outputs["Result"], b.inputs["Roughness"])
    ripple = node(nt, "ShaderNodeTexNoise", Scale=9.0, Detail=2.0)
    nt.links.new(tc.outputs["Object"], ripple.inputs["Vector"])
    drops = nt.nodes.new("ShaderNodeTexNoise")
    drops.noise_dimensions = "4D"
    drops.inputs["Scale"].default_value, drops.inputs["Detail"].default_value = 38.0, 0.0
    nt.links.new(tc.outputs["Object"], drops.inputs["Vector"])
    drops.inputs["W"].default_value = 0.0
    drops.inputs["W"].keyframe_insert("default_value", frame=1)
    drops.inputs["W"].default_value = 60.0
    drops.inputs["W"].keyframe_insert("default_value", frame=TOTAL)
    for fc in m.node_tree.animation_data.action.layers[0].strips[0].channelbags[0].fcurves:
        for kp in fc.keyframe_points:
            kp.interpolation = "LINEAR"
    both = math_node(nt, "ADD", ripple.outputs["Fac"], math_node(nt, "MULTIPLY", drops.outputs["Fac"], 0.6))
    bump = node(nt, "ShaderNodeBump", Strength=0.05)
    nt.links.new(both, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m


M_FACADE = building_mat()
M_ROAD = road_mat()
M_KERB = mat_basic("kerb", (0.04, 0.04, 0.045), rough=0.25)
NEON = [(1.0, 0.08, 0.55), (0.05, 0.85, 1.0), (0.55, 0.15, 1.0), (1.0, 0.45, 0.05)]

# ---------------------------------------------------------------- street
box("road", (14, STREET, 0.1), (0, STREET / 2 - 20, -0.05), M_ROAD)
for s in (-1, 1):
    box("walk", (3.5, STREET, 0.25), (s * 8.75, STREET / 2 - 20, 0.0), M_KERB)

n_lights = 0
for s in (-1, 1):
    y = -20.0
    while y < STREET - 20:
        w = random.uniform(10, 28)
        h = random.choice([random.uniform(18, 45), random.uniform(45, 140)])
        d = random.uniform(14, 30)
        box("bldg", (d, w - 1.0, h), (s * (10.5 + d / 2), y + w / 2, h / 2), M_FACADE)
        # blade signs sticking out over the pavement, with a light to spill colour onto the wet road
        for _ in range(random.choice([0, 1, 1, 2])):
            c = random.choice(NEON)
            sh, sz = random.uniform(2.5, 7.0), random.uniform(3.5, 12.0)
            sy = y + random.uniform(1, w - 1)
            mat = mat_basic("neon", (0, 0, 0), emit=c, strength=random.uniform(9, 16))
            box("sign", (1.4, 0.12, sh), (s * 9.6, sy, sz + sh / 2), mat)
            ld = bpy.data.lights.new("neon_l", "POINT")
            ld.energy, ld.color, ld.shadow_soft_size = 900 * sh / 5, c, 1.2
            ld.use_shadow = False
            lo = bpy.data.objects.new("neon_l", ld)
            lo.location = (s * 8.4, sy, sz + sh / 2)
            link(lo)
            n_lights += 1
        y += w

# ---------------------------------------------------------------- haze
fog = box("fog", (80, STREET + 40, 60), (0, STREET / 2 - 20, 30), mat_basic("x", (0, 0, 0)))
fm = bpy.data.materials.new("haze")
fm.use_nodes = True
fnt = fm.node_tree
fnt.nodes.remove(fnt.nodes["Principled BSDF"])
vol = node(fnt, "ShaderNodeVolumePrincipled", Density=arg("--fog", 0.012, float), Anisotropy=0.45)
vol.inputs["Color"].default_value = (0.75, 0.8, 0.95, 1)
fnt.links.new(vol.outputs["Volume"], fnt.nodes["Material Output"].inputs["Volume"])
fog.data.materials[0] = fm
fog.display_type = "WIRE"

world = bpy.data.worlds.new("world")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.004, 0.005, 0.012, 1)
scene.world = world

# ---------------------------------------------------------------- rider, bike stand-in, camera
rider = empty("rider")
for f, y in ((1, 0.0), (TOTAL, SPEED * DUR)):
    rider.location = (0, y, 0)
    rider.keyframe_insert("location", frame=f)
for fc in rider.animation_data.action.layers[0].strips[0].channelbags[0].fcurves:
    for kp in fc.keyframe_points:
        kp.interpolation = "LINEAR"

M_BIKE = mat_basic("bike", (0.03, 0.03, 0.035), rough=0.3, metal=0.6)
M_GRIP = mat_basic("grip", (0.01, 0.01, 0.01), rough=0.7)
bike = empty("bike", parent=rider)
box("bar", (0.78, 0.04, 0.04), (0, 0.62, 0.98), M_BIKE, bike)
for s in (-1, 1):
    box("grip", (0.14, 0.06, 0.06), (s * 0.36, 0.6, 0.98), M_GRIP, bike)
    box("glove", (0.11, 0.13, 0.1), (s * 0.34, 0.58, 1.03), mat_basic("glove", (0.05, 0.05, 0.06), rough=0.4), bike)
box("tank", (0.36, 0.55, 0.22), (0, 0.32, 0.82), M_BIKE, bike)
box("dash", (0.24, 0.02, 0.1), (0, 0.72, 1.05), mat_basic("hud", (0, 0, 0), emit=(0.1, 0.9, 1.0), strength=3.0),
    bike, rot=(math.radians(-55), 0, 0))
def windscreen_mat():
    m = bpy.data.materials.new("screen")
    m.use_nodes = True
    m.use_raytrace_refraction = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.75, 0.8, 0.85, 1)
    b.inputs["Transmission Weight"].default_value = 1.0
    b.inputs["Roughness"].default_value = 0.02
    b.inputs["IOR"].default_value = 1.33
    tc = nt.nodes.new("ShaderNodeTexCoord")
    height = None
    for scale, speed, size in ((55.0, 0.9, 0.42), (120.0, 1.6, 0.35)):     # big slow beads, small fast ones
        mp = nt.nodes.new("ShaderNodeMapping")
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        mp.inputs["Location"].default_value = (0, 0, 0)
        mp.inputs["Location"].keyframe_insert("default_value", frame=1)
        mp.inputs["Location"].default_value = (0, 0, -speed * DUR)          # wind pushes drops up the glass
        mp.inputs["Location"].keyframe_insert("default_value", frame=TOTAL)
        vo = nt.nodes.new("ShaderNodeTexVoronoi")
        vo.feature = "F1"
        vo.inputs["Scale"].default_value = scale
        vo.inputs["Randomness"].default_value = 1.0
        nt.links.new(mp.outputs["Vector"], vo.inputs["Vector"])
        r = math_node(nt, "DIVIDE", vo.outputs["Distance"], size)
        dome = math_node(nt, "SQRT", math_node(nt, "MAXIMUM", math_node(nt, "SUBTRACT", 1.0, math_node(nt, "MULTIPLY", r, r)), 0.0))
        height = dome if height is None else math_node(nt, "MAXIMUM", height, dome)
    for fc in m.node_tree.animation_data.action.layers[0].strips[0].channelbags[0].fcurves:
        for kp in fc.keyframe_points:
            kp.interpolation = "LINEAR"
    bump = node(nt, "ShaderNodeBump", Strength=0.9, Distance=0.004)
    nt.links.new(height, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m


me_ws = bpy.data.meshes.new("windscreen")
bm = bmesh.new()
bmesh.ops.create_grid(bm, x_segments=12, y_segments=6, size=0.5)
for v in bm.verts:                                   # 0.5 m wide, 0.22 m tall, curved around the dash
    x, y = v.co.x * 0.5, v.co.y * 0.22
    v.co = Vector((x, -1.6 * x * x, y))
bm.to_mesh(me_ws)
bm.free()
me_ws.materials.append(windscreen_mat())
ws = link(bpy.data.objects.new("windscreen", me_ws), bike)
ws.location, ws.rotation_euler = (0, 0.82, 1.12), (math.radians(-35), 0, 0)
head = light_ob = None
hl = bpy.data.lights.new("headlight", "SPOT")
hl.energy, hl.spot_size, hl.spot_blend, hl.color = 2500, math.radians(40), 0.4, (0.85, 0.92, 1.0)
light_ob = link(bpy.data.objects.new("headlight", hl), bike)
light_ob.location, light_ob.rotation_euler = (0, 0.9, 0.85), (math.radians(80), 0, 0)

cd = bpy.data.cameras.new("pov")
cd.lens, cd.clip_start = 18, 0.05
cam = link(bpy.data.objects.new("pov", cd), rider)
cam.location = (0, 0.05, 1.32)
for f in range(1, TOTAL + 1):                    # road buzz + slow weave
    t = f / FPS
    cam.rotation_euler = (math.radians(84 + 0.25 * math.sin(t * 37) + 0.15 * math.sin(t * 23)),
                          math.radians(2.0 * math.sin(t * 0.7)), math.radians(0.6 * math.sin(t * 0.45)))
    cam.keyframe_insert("rotation_euler", frame=f)
scene.camera = cam

# ---------------------------------------------------------------- rain
# Drops move relative to the rider along d (fall speed + forward speed). The rain object slides along d
# and loops every second; geometry repeats every L along d so the loop is seamless. Loop points sit
# on half frames so motion blur never straddles the jump.
REL = Vector((0, -SPEED, -RAIN_FALL))
L = REL.length                                   # one second of relative travel
alpha = math.atan2(RAIN_FALL, SPEED)
rain = empty("rain_axis", (0, 0, 1.3), rider)
rain.rotation_euler = (alpha, 0, 0)              # local +Y points up-forward, opposite to REL
M_RAIN = mat_basic("rain", (0.8, 0.85, 0.9), rough=0.05, emit=(0.6, 0.7, 0.85), strength=arg("--rain_glow", 0.25, float))
me = bpy.data.meshes.new("drops")
bm = bmesh.new()
dvec = Vector((0, -1, 0))                       # streak direction in rain-local space
rot_world = rain.rotation_euler.to_matrix()
n_drops = int(arg("--drops", 9000, int))
for _ in range(n_drops):
    p = Vector((random.uniform(-7, 7), random.uniform(-2, L - 2), random.uniform(-7, 9)))
    for k in (0, 1):                             # two copies, one period apart
        q = p + Vector((0, k * L, 0))
        view = rot_world @ q                     # roughly camera-relative
        w = dvec.cross(rot_world.inverted() @ view).normalized() * 0.004
        ln = dvec * 0.05
        vs = [bm.verts.new(q + a) for a in (-w - ln, w - ln, w + ln, -w + ln)]
        bm.faces.new(vs)
bm.to_mesh(me)
bm.free()
me.materials.append(M_RAIN)
drops = link(bpy.data.objects.new("drops", me), rain)
for fr, yv in ((0.5, 0.0), (0.5 + FPS, -L)):
    drops.location = (0, yv, 0)
    drops.keyframe_insert("location", frame=fr)
fc = drops.animation_data.action.layers[0].strips[0].channelbags[0].fcurves.find("location", index=1)
for kp in fc.keyframe_points:
    kp.interpolation = "LINEAR"
fc.modifiers.new("CYCLES")

# ---------------------------------------------------------------- render
r = scene.render
r.engine = "BLENDER_EEVEE"
r.resolution_x, r.resolution_y, r.resolution_percentage = 1920, 1080, RES
r.fps = FPS
scene.frame_start, scene.frame_end = 1, TOTAL
ee = scene.eevee
ee.taa_render_samples = SAMPLES
ee.use_raytracing = True
ee.volumetric_tile_size = "8" if RES < 100 else "4"
ee.volumetric_samples = 64
ee.volumetric_end = 250.0
ee.use_volumetric_shadows = False
r.use_motion_blur = True
r.motion_blur_shutter = 0.5
try:
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Punchy"
except Exception as e:
    print("view:", e)
scene.view_settings.exposure = arg("--exposure", 0.0, float)
print(f"BUILT lights={n_lights} drops={n_drops * 2}")

os.makedirs(OUT, exist_ok=True)
for fr in str(arg("--frames", "40")).split(","):
    scene.frame_set(int(fr))
    r.filepath = os.path.join(OUT, f"f_{int(fr):04d}.png")
    bpy.ops.render.render(write_still=True)
