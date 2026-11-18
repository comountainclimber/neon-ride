"""Hero hologram jellyfish for the neon ride.

build(scene_collection, radius) -> Jelly with .root (move it), .animate(frame, t) and its parts.
Standalone beauty render:
  blender -b --python scripts/jelly.py -- --beauty renders/jelly_beauty --res 60 --samples 32
"""
import bpy, bmesh, math, os, random, sys
from mathutils import Vector

CYAN = (0.03, 0.8, 1.0)
MAGENTA = (1.0, 0.08, 0.72)
VIOLET = (0.45, 0.2, 1.0)


def _mn(nt, op, a, b=None):
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


def _holo_base(name):
    """Additive hologram: emission over full transparency, both faces."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    m.surface_render_method = "BLENDED"
    m.use_backface_culling = False
    nt = m.node_tree
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    em = nt.nodes.new("ShaderNodeEmission")
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    add = nt.nodes.new("ShaderNodeAddShader")
    nt.links.new(tr.outputs[0], add.inputs[0])
    nt.links.new(em.outputs[0], add.inputs[1])
    nt.links.new(add.outputs[0], nt.nodes["Material Output"].inputs["Surface"])
    return m, nt, em


def _scanlines(nt, spacing, scroll_node_value):
    """Soft horizontal bands in object Z that drift slowly upward (no flicker)."""
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(tc.outputs["Object"], sep.inputs[0])
    z = _mn(nt, "ADD", _mn(nt, "DIVIDE", sep.outputs[2], spacing), scroll_node_value)
    wave = _mn(nt, "SINE", _mn(nt, "MULTIPLY", z, 2 * math.pi))
    return _mn(nt, "ADD", 0.78, _mn(nt, "MULTIPLY", wave, 0.22))


def bell_material(scroll):
    m, nt, em = _holo_base("jelly_bell")
    lw = nt.nodes.new("ShaderNodeLayerWeight")
    lw.inputs["Blend"].default_value = 0.35
    rim = _mn(nt, "POWER", _mn(nt, "SUBTRACT", 1.0, lw.outputs["Facing"]), 2.2)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    vor = nt.nodes.new("ShaderNodeTexVoronoi")
    vor.feature = "DISTANCE_TO_EDGE"
    vor.inputs["Scale"].default_value = 7.0
    nt.links.new(tc.outputs["Object"], vor.inputs["Vector"])
    cells = _mn(nt, "SUBTRACT", 1.0, _mn(nt, "MINIMUM", _mn(nt, "DIVIDE", vor.outputs["Distance"], 0.05), 1.0))
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.inputs["A"].default_value, mix.inputs["B"].default_value = (*CYAN, 1), (*VIOLET, 1)
    nt.links.new(_mn(nt, "MULTIPLY", rim, 0.6), mix.inputs["Factor"])
    nt.links.new(mix.outputs["Result"], em.inputs["Color"])
    glow = _mn(nt, "ADD", _mn(nt, "MULTIPLY", rim, 2.4), _mn(nt, "ADD", 0.06, _mn(nt, "MULTIPLY", cells, 0.35)))
    glow = _mn(nt, "MULTIPLY", glow, _scanlines(nt, 0.06, scroll))
    strength = nt.nodes.new("ShaderNodeValue")
    strength.name = "level"
    strength.outputs[0].default_value = 1.0
    nt.links.new(_mn(nt, "MULTIPLY", glow, strength.outputs[0]), em.inputs["Strength"])
    return m


def flat_glow(name, colour, strength, scroll=None, gradient=None):
    """Plain glowing hologram colour; optional object-Z gradient (z0, z1, colour_b)."""
    m, nt, em = _holo_base(name)
    if gradient:
        z0, z1, colour_b = gradient
        tc = nt.nodes.new("ShaderNodeTexCoord")
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs[0])
        rng = nt.nodes.new("ShaderNodeMapRange")
        rng.inputs["From Min"].default_value, rng.inputs["From Max"].default_value = z0, z1
        nt.links.new(sep.outputs[2], rng.inputs["Value"])
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.inputs["A"].default_value, mix.inputs["B"].default_value = (*colour_b, 1), (*colour, 1)
        nt.links.new(rng.outputs["Result"], mix.inputs["Factor"])
        nt.links.new(mix.outputs["Result"], em.inputs["Color"])
        fade = _mn(nt, "ADD", 0.25, _mn(nt, "MULTIPLY", rng.outputs["Result"], 0.75))
    else:
        em.inputs["Color"].default_value = (*colour, 1)
        fade = None
    s = strength if fade is None else _mn(nt, "MULTIPLY", fade, strength)
    if scroll is not None:
        s = _mn(nt, "MULTIPLY", s, _scanlines(nt, 0.06, scroll))
    if isinstance(s, (int, float)):
        em.inputs["Strength"].default_value = s
    else:
        nt.links.new(s, em.inputs["Strength"])
    return m


class Jelly:
    pass


def build(col, R=26.0, seed=11):
    rng = random.Random(seed)
    J = Jelly()
    J.R = R

    def link(ob, parent=None):
        col.objects.link(ob)
        if parent:
            ob.parent = parent
        return ob

    def empty(name, parent=None):
        ob = bpy.data.objects.new(name, None)
        return link(ob, parent)

    J.root = empty("jelly_root")
    J.pivot = empty("bell_pivot", J.root)                      # pulses
    J.spin = empty("jelly_spin", J.root)                       # slow turn of sparkles

    # a shared, slowly scrolling value drives every scanline
    J.scroll_mat_inputs = []
    scroll_holder = bpy.data.node_groups.new("jelly_scroll", "ShaderNodeTree")
    scroll_holder.interface.new_socket("v", in_out="OUTPUT", socket_type="NodeSocketFloat")
    gout = scroll_holder.nodes.new("NodeGroupOutput")
    val = scroll_holder.nodes.new("ShaderNodeValue")
    val.name = "scroll"
    scroll_holder.links.new(val.outputs[0], gout.inputs[0])
    J.scroll_value = val

    def scroll_socket(nt):
        g = nt.nodes.new("ShaderNodeGroup")
        g.node_tree = scroll_holder
        return g.outputs[0]

    # --- bell: ribbed dome with a frilled rim, two shells
    me = bpy.data.meshes.new("bell")
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=96, v_segments=40, radius=1.0)
    for v in list(bm.verts):
        if v.co.z < -0.2:
            bm.verts.remove(v)
    for v in bm.verts:
        z = v.co.z
        ang = math.atan2(v.co.y, v.co.x)
        low = max(0.0, 1.0 - max(z, 0.0))
        r = 1.0 + 0.16 * low ** 3 + 0.03 * math.cos(ang * 16) * low ** 1.5 + 0.05 * math.cos(ang * 48) * max(0.0, -z * 5) ** 2
        v.co.x *= r
        v.co.y *= r
        v.co.z = z * 0.68 - 0.05 * low ** 4
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True

    def add_contract_key(mesh):
        ob_tmp = bpy.data.objects.new("tmp", mesh)
        col.objects.link(ob_tmp)
        ob_tmp.shape_key_add(name="rest")
        k = ob_tmp.shape_key_add(name="contract")
        for i, v in enumerate(mesh.vertices):
            co = v.co
            z = co.z
            low = max(0.0, 1.0 - max(z / 0.68, 0.0))           # 0 at the top of the dome, 1 at the rim
            squeeze = 1.0 - 0.42 * low ** 1.5
            k.data[i].co = (co.x * squeeze, co.y * squeeze, z + 0.16 * (1 - low) - 0.16 * low ** 2)
        col.objects.unlink(ob_tmp)
        bpy.data.objects.remove(ob_tmp)
        return mesh.shape_keys

    m_bell = bell_material(None)
    # wire the shared scroll into the bell's scanline node (first ADD after DIVIDE uses a constant 0 placeholder)
    J.bell_mat = m_bell
    me.materials.append(m_bell)
    J.bell = link(bpy.data.objects.new("bell", me), J.pivot)
    J.bell.scale = (R,) * 3
    inner = link(bpy.data.objects.new("bell_inner", me), J.pivot)
    inner.scale = (R * 0.86, R * 0.86, R * 0.7)
    inner.location = (0, 0, -0.04 * R)

    # wireframe overlay: the hologram's "scan" mesh
    J.keys = [add_contract_key(me)]
    wire_me = me.copy()
    wire_me.materials.clear()
    wire_me.materials.append(flat_glow("jelly_wire", CYAN, 1.6))
    wire = link(bpy.data.objects.new("bell_wire", wire_me), J.pivot)
    wire.scale = (R * 1.004,) * 3
    dec = wire.modifiers.new("dec", "DECIMATE")
    dec.decimate_type = "UNSUBDIV"
    dec.iterations = 2
    wf = wire.modifiers.new("wf", "WIREFRAME")
    wf.thickness = 0.0035
    wf.use_replace = True
    J.keys.append(wire_me.shape_keys)

    # --- gonads: four glowing horseshoe loops inside the bell
    m_gon = flat_glow("jelly_gonad", MAGENTA, 3.0)
    for i in range(4):
        a0 = i * math.pi / 2 + math.pi / 4
        cu = bpy.data.curves.new(f"gonad{i}", "CURVE")
        cu.dimensions, cu.bevel_depth, cu.bevel_resolution = "3D", 0.035, 3
        sp = cu.splines.new("NURBS")
        sp.points.add(7)
        for j, pt in enumerate(sp.points):
            a = a0 + (j / 7 - 0.5) * 2.4
            rr = 0.32 + 0.05 * math.sin(j)
            pt.co = (math.cos(a) * rr, math.sin(a) * rr, 0.18 + 0.03 * math.cos(j * 1.3), 1)
        sp.use_endpoint_u = True
        cu.materials.append(m_gon)
        g = link(bpy.data.objects.new(f"gonad{i}", cu), J.pivot)
        g.scale = (R,) * 3

    # --- oral arms: four long frilly ribbons twisting down from the centre
    m_arm = flat_glow("jelly_arm", MAGENTA, 2.2, gradient=(-3.0 * R, 0.0, VIOLET))
    J.arms = []
    for i in range(4):
        cu = bpy.data.curves.new(f"arm{i}", "CURVE")
        cu.dimensions = "3D"
        cu.extrude = 0.9
        cu.bevel_depth = 0.0
        cu.twist_mode = "MINIMUM"
        sp = cu.splines.new("NURBS")
        n = 18
        sp.points.add(n - 1)
        sp.use_endpoint_u = True
        cu.materials.append(m_arm)
        ob = link(bpy.data.objects.new(f"arm{i}", cu), J.root)
        J.arms.append((sp, i * math.pi / 2 + 0.3, rng.uniform(0, 6.28), rng.uniform(2.2, 2.9) * R))

    # --- tentacles: many fine glowing threads, cyan fading to violet
    m_tent = flat_glow("jelly_tent", CYAN, 2.6, gradient=(-160.0, 0.0, VIOLET))
    J.tents = []
    for i in range(64):
        a = i / 64 * 2 * math.pi + rng.uniform(-0.03, 0.03)
        cu = bpy.data.curves.new(f"tent{i}", "CURVE")
        cu.dimensions, cu.bevel_depth, cu.bevel_resolution = "3D", rng.uniform(0.04, 0.11), 1
        sp = cu.splines.new("NURBS")
        sp.points.add(11)
        sp.use_endpoint_u = True
        cu.materials.append(m_tent)
        ob = link(bpy.data.objects.new(f"tent{i}", cu), J.root)
        J.tents.append((sp, a, R * rng.uniform(0.98, 1.12), rng.uniform(70, 165) if i % 4 else rng.uniform(25, 55),
                        rng.uniform(0, 6.28)))

    # --- sparkles: tiny bright motes drifting inside and around the bell
    sm = bpy.data.meshes.new("sparkles")
    bm = bmesh.new()
    for _ in range(420):
        d = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))).normalized() * rng.uniform(0.2, 1.3)
        c = Vector((d.x * R, d.y * R, (d.z * 0.7 - 0.15) * R))
        s = rng.uniform(0.08, 0.22)
        bmesh.ops.create_icosphere(bm, subdivisions=1, radius=s, matrix=__import__("mathutils").Matrix.Translation(c))
    bm.to_mesh(sm)
    bm.free()
    sm.materials.append(flat_glow("jelly_spark", (0.7, 0.95, 1.0), 9.0))
    link(bpy.data.objects.new("sparkles", sm), J.spin)

    beam_me = bpy.data.meshes.new("beam")
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=False, segments=48, radius1=1.2, radius2=R * 0.9, depth=1.0)
    bm.to_mesh(beam_me)
    bm.free()
    beam_me.materials.append(flat_glow("jelly_beam", CYAN, 0.25, gradient=(-0.5, 0.5, VIOLET)))
    J.beam = link(bpy.data.objects.new("projector_beam", beam_me), J.root)
    J.beam.visible_shadow = False
    em_me = bpy.data.meshes.new("emitter")
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=32, radius1=3.0, radius2=1.4, depth=1.2)
    bm.to_mesh(em_me)
    bm.free()
    em_me.materials.append(flat_glow("jelly_emitter", CYAN, 6.0))
    J.emitter = link(bpy.data.objects.new("projector_emitter", em_me), J.root)

    def set_projector(depth):
        """Beam from an emitter `depth` metres below the bell up to the bell's underside."""
        J.beam.location = (0, 0, -depth / 2 - 0.05 * R)
        J.beam.scale = (1, 1, depth)
        J.emitter.location = (0, 0, -depth - 0.05 * R)
    def key_projector(f):
        for ob in (J.beam, J.emitter):
            ob.keyframe_insert("location", frame=f)
        J.beam.keyframe_insert("scale", frame=f)

    J.set_projector = set_projector
    J.key_projector = key_projector
    set_projector(60.0)

    # finally hook every scanline to the shared scroll value
    for m in bpy.data.materials:
        if not m.name.startswith("jelly") or not m.use_nodes:
            continue
        nt = m.node_tree
        for n in list(nt.nodes):
            if n.type == "MATH" and n.operation == "ADD" and len(n.inputs[1].links) == 0 and n.inputs[0].links \
                    and n.inputs[0].links[0].from_node.type == "MATH" and n.inputs[0].links[0].from_node.operation == "DIVIDE" \
                    and n.inputs[1].default_value == 0.0 and n.inputs[0].links[0].from_node.inputs[1].default_value == 0.06:
                nt.links.new(scroll_socket(nt), n.inputs[1])

    PERIOD = 2.7

    def contraction(t):
        """0 relaxed .. 1 fully contracted: quick squeeze (30% of the cycle), slow relaxation."""
        ph = (t / PERIOD) % 1.0
        if ph < 0.24:
            x = ph / 0.24
            return x * x * (3 - 2 * x)
        x = (ph - 0.24) / 0.76
        return 1.0 - (1 - (1 - x) ** 3)

    def bob(t):
        """The bell surges up just after each squeeze and sinks gently back."""
        return 0.26 * R * contraction(t - 0.3) + 0.04 * R * math.sin(t * 0.4)

    def animate(f, t, level=1.0, rise=0.0):
        c = contraction(t)
        z0 = bob(t)
        J.pivot.location = (0, 0, z0)
        J.pivot.scale = (1.0 - 0.08 * c, 1.0 - 0.08 * c, 1.0 + 0.1 * c)
        J.pivot.rotation_euler = (0.09 * math.sin(t * 0.8), 0.07 * math.sin(t * 0.61 + 1), t * 0.08)
        for prop in ("location", "scale", "rotation_euler"):
            J.pivot.keyframe_insert(prop, frame=f)
        for keys in J.keys:
            kb = keys.key_blocks["contract"]
            kb.value = c
            kb.keyframe_insert("value", frame=f)
        J.spin.location = (0, 0, z0)
        J.spin.rotation_euler = (0, 0, t * 0.12)
        J.spin.keyframe_insert("location", frame=f)
        J.spin.keyframe_insert("rotation_euler", frame=f)
        J.scroll_value.outputs[0].default_value = -t * 0.35
        J.scroll_value.outputs[0].keyframe_insert("default_value", frame=f)
        lv = J.bell_mat.node_tree.nodes["level"].outputs[0]
        lv.default_value = level * (1.0 + 0.25 * c)               # the bell brightens a touch on each squeeze
        lv.keyframe_insert("default_value", frame=f)
        for sp, a, rr, length, ph in J.tents:
            for j, pt in enumerate(sp.points):
                d = j / (len(sp.points) - 1)
                lag = d * 1.1                                       # the squeeze travels down the tentacle
                cl = contraction(t - lag)
                trail = z0 - bob(t - lag * 0.9)                     # tips lag behind the bell's surge
                curl = 0.55 * R * d * d * cl                        # tips flick inward then drift out
                sway = 4.0 * d * d * math.sin(t * 0.6 + ph + d * 4.0)
                drift = 3.0 * d * math.sin(t * 0.45 + ph * 1.7 + d * 2.5)
                r0 = (rr * (1 - 0.3 * d) * (1.0 - 0.1 * cl) - curl) * (1.0 - min(0.45, 0.02 * rise) * d)
                pt.co = (math.cos(a) * r0 + sway * math.cos(a + 1.57), math.sin(a) * r0 + drift * math.sin(a + 1.57),
                         z0 - 0.08 * R - d * length * (1.0 - 0.18 * cl + 0.012 * rise) - trail * d, 1)
                pt.keyframe_insert("co", frame=f)
        for sp, a, ph, length in J.arms:
            for j, pt in enumerate(sp.points):
                d = j / (len(sp.points) - 1)
                lag = d * 1.3
                cl = contraction(t - lag)
                trail = z0 - bob(t - lag)
                curl = 0.12 * R * (0.3 + d) * (1.0 - 0.15 * cl)
                ang = a + d * 2.2 + 0.25 * math.sin(t * 0.5 + ph - d * 2)
                pt.co = (math.cos(ang) * curl + 2.5 * d * math.sin(t * 0.55 + ph + d * 3),
                         math.sin(ang) * curl + 2.0 * d * math.cos(t * 0.45 + ph),
                         z0 - 0.05 * R - d * length - trail * d, 1)
                pt.tilt = d * 7.0 + 0.6 * math.sin(t * 0.7 + ph + d * 4)
                pt.keyframe_insert("co", frame=f)
                pt.keyframe_insert("tilt", frame=f)

    J.animate = animate
    return J


def beauty(out, res, samples):
    """Close-up glamour shots of the jellyfish alone in the rain-dark sky."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    J = build(sc.collection)
    for f in range(1, 49):
        J.animate(f, f / 24.0)
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.003, 0.005, 0.012, 1)
    sc.world = world
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.data.lens = 30
    cam.data.clip_end = 5000
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = 1920, 1080, res
    sc.eevee.taa_render_samples = samples
    sc.view_settings.view_transform = "AgX"
    sc.view_settings.look = "AgX - Punchy"
    add_bloom(sc)
    os.makedirs(out, exist_ok=True)
    for i, (az, el, dist, look_z) in enumerate(((-90, 8, 120, -30), (-30, -12, 95, -15), (60, 30, 140, -45))):
        a, e = math.radians(az), math.radians(el)
        cam.location = Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e))) * dist + Vector((0, 0, look_z))
        cam.rotation_euler = (Vector((0, 0, look_z)) - cam.location).to_track_quat("-Z", "Y").to_euler()
        sc.frame_set(24)
        sc.render.filepath = os.path.join(out, f"jelly_{i}.png")
        bpy.ops.render.render(write_still=True)


def add_bloom(sc, strength=0.6, threshold=0.9, size=7):
    """Neon bloom in the compositor (Blender 5 compositor node groups)."""
    ng = bpy.data.node_groups.new("comp", "CompositorNodeTree")
    rl = ng.nodes.new("CompositorNodeRLayers")
    gl = ng.nodes.new("CompositorNodeGlare")
    out = ng.nodes.new("NodeGroupOutput")
    ng.interface.new_socket("Image", in_out="OUTPUT", socket_type="NodeSocketColor")
    for name, v in (("Type", "Bloom"), ("Strength", strength), ("Threshold", threshold), ("Size", size)):
        try:
            gl.inputs[name].default_value = v
        except Exception as e:
            print("bloom", name, e)
    ng.links.new(rl.outputs["Image"], gl.inputs["Image"])
    ng.links.new(gl.outputs[0], out.inputs[0])
    sc.compositing_node_group = ng
    sc.render.use_compositing = True


if __name__ == "__main__" and "--beauty" in sys.argv:
    argv = sys.argv[sys.argv.index("--") + 1:]
    out = argv[argv.index("--beauty") + 1]
    res = int(argv[argv.index("--res") + 1]) if "--res" in argv else 60
    samples = int(argv[argv.index("--samples") + 1]) if "--samples" in argv else 32
    beauty(os.path.abspath(out), res, samples)
