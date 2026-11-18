"""Import a GLB and report its objects, sizes, materials and textures; render a turnaround sheet.

  blender -b --python scripts/inspect_glb.py -- models/drone.glb renders/inspect_drone
"""
import bpy, math, os, sys
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
path, out = os.path.abspath(argv[0]), os.path.abspath(argv[1])

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=path)
scene = bpy.context.scene
meshes = [o for o in scene.objects if o.type == "MESH"]
lo, hi = Vector((1e9,) * 3), Vector((-1e9,) * 3)
for o in meshes:
    bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
    olo = Vector((min(v[i] for v in bb) for i in range(3)))
    ohi = Vector((max(v[i] for v in bb) for i in range(3)))
    for i in range(3):
        lo[i], hi[i] = min(lo[i], olo[i]), max(hi[i], ohi[i])
    print(f"OBJ {o.name!r} verts={len(o.data.vertices)} tris={len(o.data.polygons)} "
          f"min={tuple(round(x, 3) for x in olo)} max={tuple(round(x, 3) for x in ohi)} "
          f"mats={[m.name for m in o.data.materials if m]} parent={o.parent.name if o.parent else None}")
print("TOTAL objects", len(meshes), "bounds", tuple(round(x, 3) for x in lo), tuple(round(x, 3) for x in hi))
for m in bpy.data.materials:
    if m.node_tree:
        print("MAT", m.name, [(n.type, n.image.name, tuple(n.image.size)) for n in m.node_tree.nodes
                              if n.type == "TEX_IMAGE" and n.image])

# turnaround sheet: 4 views, flat studio light
centre, size = (lo + hi) / 2, max(hi - lo)
world = bpy.data.worlds.new("w")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.35, 0.35, 0.35, 1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.2
scene.world = world
sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
sun.data.energy = 3
sun.rotation_euler = (math.radians(50), 0, math.radians(30))
scene.collection.objects.link(sun)
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.lens = 60
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = 640
scene.view_settings.view_transform = "Standard"
for name, ang, elev in (("front", -90, 5), ("left", 180, 5), ("back", 90, 5), ("threeq", -55, 20), ("top", -90, 80)):
    a, e = math.radians(ang), math.radians(elev)
    d = size * 2.6
    cam.location = centre + Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e))) * d
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = os.path.join(out, name + ".png")
    bpy.ops.render.render(write_still=True)
print("DONE", out)
