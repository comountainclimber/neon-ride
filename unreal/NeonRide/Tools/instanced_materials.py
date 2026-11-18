"""Save runtime instancing permutations without changing the engine's glTF materials."""
import unreal as ue


def prepare_instancing():
    assets = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    lib = ue.MaterialEditingLibrary
    parents = {}

    def instanced_parent(parent):
        path = parent.get_path_name().split(".")[0]
        if path in parents:
            return parents[path]
        local = parent
        if not path.startswith("/Game/"):
            copy_path = "/Game/Environment/FilmParents/" + parent.get_name()
            if assets.does_asset_exist(copy_path):
                local = assets.load_asset(copy_path)
            else:
                local = assets.duplicate_asset(path,copy_path)
            if not local:
                raise RuntimeError("Could not copy glTF parent " + path)
        dirty = False
        if isinstance(local, ue.Material):
            if not local.get_editor_property("used_with_instanced_static_meshes"):
                local.set_editor_property("used_with_instanced_static_meshes",True)
                lib.recompile_material(local)
                dirty = True
        elif isinstance(local, ue.MaterialInstanceConstant):
            old = local.get_editor_property("parent")
            new = instanced_parent(old)
            if new != old:
                lib.set_material_instance_parent(local,new)
                lib.update_material_instance(local)
                dirty = True
        if dirty:
            assert assets.save_loaded_asset(local,False), path
        parents[path] = local
        return local

    for key in ("M_Neon","M_RainAsphalt","M_Facade","M_Glass"):
        instanced_parent(assets.load_asset("/Game/Environment/"+key))
    for item in assets.list_assets("/Game/Film",recursive=True,include_folder=False):
        if "/Materials/" in item:
            material = assets.load_asset(item)
            if isinstance(material,(ue.MaterialInstanceConstant,ue.Material)):
                instanced_parent(material)
    ue.log("NEON_INSTANCED_SHADERS_READY")
