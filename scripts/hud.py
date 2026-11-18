"""Live instrument panel for the bike: every element is driven by the ride (speed, lean, nearby traffic).

build(link, parent, loc, rot, glyph_sheet, kinds) -> Hud with .animate(f, t, kmh, lean, lat, blips) and .finish()
Panel is 0.34 x 0.11 m in its local XZ plane; the rider sees its -Y side.
"""
import bpy, bmesh, math
from mathutils import Vector

TEAL = (0.08, 0.9, 1.0)
MAGENTA = (1.0, 0.12, 0.7)
AMBER = (1.0, 0.6, 0.15)
WHITE = (0.85, 0.95, 1.0)
W, H = 0.34, 0.11
DIGIT_CELLS = [0, 1, 2, 3, 5, 6, 7, 8, 9, 10]          # glyph-sheet cells used as digits 0-9
CELL_W, CELL_H, GRID_X0, GRID_Y0 = 182.0 / 1536, 125.0 / 1024, 41.0 / 1536, 22.0 / 1024


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


def _emit(name, blended=False):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    em = nt.nodes.new("ShaderNodeEmission")
    out = nt.nodes["Material Output"]
    if blended:
        m.surface_render_method = "BLENDED"
        tr = nt.nodes.new("ShaderNodeBsdfTransparent")
        mix = nt.nodes.new("ShaderNodeMixShader")
        mix.name = "mix"
        nt.links.new(tr.outputs[0], mix.inputs[1])
        nt.links.new(em.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs["Surface"])
    else:
        nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m, nt, em


def solid(name, colour, strength):
    m, nt, em = _emit(name)
    em.inputs["Color"].default_value = (*colour, 1)
    em.inputs["Strength"].default_value = strength
    return m


class Hud:
    pass


def build(link, parent, loc, rot, glyph_sheet, kinds):
    hud = Hud()
    root = bpy.data.objects.new("hud_root", None)
    root.location, root.rotation_euler = loc, rot
    link(root, parent)
    hud.root = root

    def quad(name, cx, cz, w, h, mat, y=-0.002, uv=((0, 0), (1, 0), (1, 1), (0, 1))):
        bm = bmesh.new()
        uvl = bm.loops.layers.uv.new("UVMap")
        vs = [bm.verts.new(Vector(c)) for c in ((-w / 2, 0, -h / 2), (w / 2, 0, -h / 2), (w / 2, 0, h / 2), (-w / 2, 0, h / 2))]
        f = bm.faces.new(vs)
        for lp, u in zip(f.loops, uv):
            lp[uvl].uv = u
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mat)
        ob = bpy.data.objects.new(name, me)
        ob.location = (cx, y, cz)
        link(ob, root)
        return ob

    m_teal = solid("hud_teal", TEAL, 4.0)
    m_dim = solid("hud_dim", TEAL, 0.5)
    m_white = solid("hud_white", WHITE, 5.0)
    m_mag = solid("hud_magenta", MAGENTA, 6.0)
    m_amber = solid("hud_amber", AMBER, 5.0)

    # glass panel and frame
    quad("hud_bg", 0, 0, W, H, solid("hud_bg", (0.0, 0.01, 0.015), 1.0), y=0.0)
    for cx, cz, w, h in ((0, H / 2 - 0.0015, W, 0.0012), (0, -H / 2 + 0.0015, W, 0.0012),
                         (-W / 2 + 0.0015, 0, 0.0012, H), (W / 2 - 0.0015, 0, 0.0012, H),
                         (-0.035, 0, 0.0008, H * 0.8), (0.06, 0, 0.0008, H * 0.8)):
        quad("hud_frame", cx, cz, w, h, m_dim)

    # --- speed arc: segmented semicircle that fills with speed
    m_arc, nt, em = _emit("hud_arc")
    uvn = nt.nodes.new("ShaderNodeUVMap")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(uvn.outputs["UV"], sep.inputs[0])
    fill = nt.nodes.new("ShaderNodeValue")
    fill.name = "fill"
    fill.outputs[0].default_value = 0.6
    lit = _mn(nt, "LESS_THAN", sep.outputs[0], fill.outputs[0])
    seg = _mn(nt, "LESS_THAN", _mn(nt, "FRACT", _mn(nt, "MULTIPLY", sep.outputs[0], 26.0)), 0.72)
    hot = _mn(nt, "GREATER_THAN", sep.outputs[0], 0.8)          # the top of the range burns magenta
    mixc = nt.nodes.new("ShaderNodeMix")
    mixc.data_type = "RGBA"
    mixc.inputs["A"].default_value, mixc.inputs["B"].default_value = (*TEAL, 1), (*MAGENTA, 1)
    nt.links.new(hot, mixc.inputs["Factor"])
    nt.links.new(mixc.outputs["Result"], em.inputs["Color"])
    nt.links.new(_mn(nt, "MULTIPLY", seg, _mn(nt, "ADD", 0.35, _mn(nt, "MULTIPLY", lit, 5.5))), em.inputs["Strength"])
    hud.arc_mat = m_arc
    ACX, ACZ, R0, R1, N = -0.105, -0.036, 0.058, 0.074, 52
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    for i in range(N):
        a0 = math.radians(172 - 164 * i / N)
        a1 = math.radians(172 - 164 * (i + 1) / N)
        pts = [(R0, a0), (R1, a0), (R1, a1), (R0, a1)]
        vs = [bm.verts.new(Vector((ACX + r * math.cos(a), -0.002, ACZ + r * math.sin(a)))) for r, a in pts]
        f = bm.faces.new(vs)
        for lp, u in zip(f.loops, (i / N, i / N, (i + 1) / N, (i + 1) / N)):
            lp[uvl].uv = (u, 0.5)
    me = bpy.data.meshes.new("hud_arc")
    bm.to_mesh(me)
    bm.free()
    me.materials.append(m_arc)
    link(bpy.data.objects.new("hud_arc", me), root)

    # --- speed readout: three glyph digits cut from the glyph sheet, switched by moving their UVs
    img = bpy.data.images.load(glyph_sheet, check_existing=True)
    hud.digits = []
    for i, dx in enumerate((-0.131, -0.105, -0.079)):
        m, nt, em = _emit(f"hud_digit{i}", blended=True)
        uvn = nt.nodes.new("ShaderNodeUVMap")
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.name = "map"
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image = img
        tex.extension = "CLIP"
        nt.links.new(uvn.outputs["UV"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], tex.inputs["Vector"])
        em.inputs["Color"].default_value = (*WHITE, 1)
        em.inputs["Strength"].default_value = 6.0
        bw = nt.nodes.new("ShaderNodeRGBToBW")
        nt.links.new(tex.outputs["Color"], bw.inputs[0])
        nt.links.new(bw.outputs[0], nt.nodes["mix"].inputs[0])
        u0, v1 = GRID_X0, 1.0 - GRID_Y0
        uv = ((u0, v1 - CELL_H), (u0 + CELL_W, v1 - CELL_H), (u0 + CELL_W, v1), (u0, v1))
        quad(f"hud_digit{i}", dx, -0.024, 0.025, 0.0172, m, uv=uv)
        hud.digits.append(m)

    # --- artificial horizon: a bar that rolls with the bike's lean, over fixed reference ticks
    quad("hud_tick_l", -0.022 + 0.0, 0.012, 0.006, 0.0014, m_dim)
    quad("hud_tick_r", 0.047, 0.012, 0.006, 0.0014, m_dim)
    quad("hud_center", 0.0125, 0.012, 0.003, 0.003, m_white)
    hud.horizon = quad("hud_horizon", 0.0125, 0.012, 0.052, 0.0022, m_teal)
    quad("hud_h_wing_l", -0.008, 0.012 - 0.0, 0.0018, 0.008, m_dim)
    quad("hud_h_wing_r", 0.033, 0.012, 0.0018, 0.008, m_dim)

    # --- data bars: a row of bars whose levels change several times a second
    m_bars, nt, em = _emit("hud_bars")
    uvn = nt.nodes.new("ShaderNodeUVMap")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(uvn.outputs["UV"], sep.inputs[0])
    clock = nt.nodes.new("ShaderNodeValue")
    clock.name = "clock"
    cell = nt.nodes.new("ShaderNodeCombineXYZ")
    nt.links.new(_mn(nt, "FLOOR", _mn(nt, "MULTIPLY", sep.outputs[0], 18.0)), cell.inputs[0])
    nt.links.new(_mn(nt, "FLOOR", clock.outputs[0]), cell.inputs[1])
    wn = nt.nodes.new("ShaderNodeTexWhiteNoise")
    wn.noise_dimensions = "2D"
    nt.links.new(cell.outputs[0], wn.inputs["Vector"])
    level = _mn(nt, "ADD", 0.2, _mn(nt, "MULTIPLY", wn.outputs["Value"], 0.8))
    on = _mn(nt, "MULTIPLY", _mn(nt, "LESS_THAN", sep.outputs[1], level),
             _mn(nt, "LESS_THAN", _mn(nt, "FRACT", _mn(nt, "MULTIPLY", sep.outputs[0], 18.0)), 0.6))
    em.inputs["Color"].default_value = (*TEAL, 1)
    nt.links.new(_mn(nt, "MULTIPLY", on, 3.5), em.inputs["Strength"])
    quad("hud_bars", 0.0125, -0.031, 0.08, 0.022, m_bars)
    hud.bars_mat = m_bars

    # --- lane radar: two lanes, the bike's own marker, and a blip for every vehicle near us
    RCX, RCZ, SX, SZ = 0.115, -0.012, 0.0045, 0.00075
    hud.radar = (RCX, RCZ, SX, SZ)
    for x in (-5.0, 5.0):
        quad("hud_lane_edge", RCX + x * SX, 0.0, 0.0012, H * 0.86, m_dim)
    for k in range(7):
        quad("hud_lane_dash", RCX - 0.4 * SX, -0.042 + k * 0.014, 0.001, 0.006, m_dim)
    hud.me = quad("hud_me", RCX, RCZ, 0.007, 0.011, m_white, y=-0.003)
    hud.blips = []
    for kind in kinds:
        w, h = (0.012, 0.03) if kind == "truck" else (0.008, 0.012)
        hud.blips.append(quad("hud_blip", RCX, RCZ, w, h, m_amber if kind == "truck" else m_teal, y=-0.0025))
    hud.warn = [quad("hud_warn_l", RCX - 0.04, RCZ, 0.006, 0.03, m_mag), quad("hud_warn_r", RCX + 0.04, RCZ, 0.006, 0.03, m_mag)]

    def animate(f, t, kmh, lean, lat, blips):
        fl = m_arc.node_tree.nodes["fill"].outputs[0]
        fl.default_value = max(0.05, min(1.0, kmh / 190.0))
        fl.keyframe_insert("default_value", frame=f)
        n = int(round(kmh))
        for m, d in zip(hud.digits, (n // 100 % 10, n // 10 % 10, n % 10)):
            c = DIGIT_CELLS[d]
            loc_in = m.node_tree.nodes["map"].inputs["Location"]
            loc_in.default_value = ((c % 8) * CELL_W, -(c // 8) * CELL_H, 0)
            loc_in.keyframe_insert("default_value", frame=f)
        hud.horizon.rotation_euler = (0, -lean * 0.9, 0)
        hud.horizon.keyframe_insert("rotation_euler", frame=f)
        ck = m_bars.node_tree.nodes["clock"].outputs[0]
        ck.default_value = t * 7.0
        ck.keyframe_insert("default_value", frame=f)
        hud.me.location = (RCX + lat * SX, -0.003, RCZ)
        hud.me.keyframe_insert("location", frame=f)
        near = [False, False]
        for ob, (d, vlat) in zip(hud.blips, blips):
            z = RCZ + d * SZ
            vis = -0.046 < z - 0.0 < 0.046
            ob.location = (RCX + vlat * SX, -0.0025, max(-0.05, min(0.05, z)))
            ob.scale = (1, 1, 1) if vis else (0, 0, 0)
            ob.keyframe_insert("location", frame=f)
            ob.keyframe_insert("scale", frame=f)
            if abs(d) < 16.0:
                near[0 if vlat < lat else 1] = True
        blink = (int(t * 6) % 2) == 0
        for ob, on in zip(hud.warn, near):
            ob.scale = (1, 1, 1) if (on and blink) else (0, 0, 0)
            ob.keyframe_insert("scale", frame=f)

    def finish():
        """Digits, blips appearing and warning blinks switch instantly rather than blending."""
        def const(idblock, paths=None):
            ad = idblock.animation_data
            if not ad or not ad.action:
                return
            for layer in ad.action.layers:
                for strip in layer.strips:
                    for bag in strip.channelbags:
                        for fc in bag.fcurves:
                            if paths is None or any(p in fc.data_path for p in paths):
                                for kp in fc.keyframe_points:
                                    kp.interpolation = "CONSTANT"
        for m in hud.digits:
            const(m.node_tree)
        for ob in hud.blips + hud.warn:
            const(ob, ("scale",))

    hud.animate, hud.finish = animate, finish
    return hud
