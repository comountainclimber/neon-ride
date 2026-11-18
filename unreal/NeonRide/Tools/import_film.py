"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>."""
import os
from pathlib import Path
import unreal as ue


def import_models(source):
    assets = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    tools = ue.AssetToolsHelpers.get_asset_tools()
    files = sorted(source.glob("*.glb"))
    if len(files) != 18:
        raise RuntimeError(f"Expected the 18 film models in {source}, found {len(files)}")
    def retain_glass_geometry(key, mesh):
        if key == "bike" and not mesh.get_editor_property("allow_cpu_access"):
            mesh.set_editor_property("allow_cpu_access", True)
            assets.save_loaded_asset(mesh, False)

    for file in files:
        key = file.stem.replace("-", "_")
        folder = "/Game/Film/" + key
        target = folder + "/SM_" + key
        if assets.does_asset_exist(target):
            if key == "bike":
                retain_glass_geometry(key, assets.load_asset(target))
            continue
        task = ue.AssetImportTask()
        task.filename = str(file)
        task.destination_path = folder
        task.automated = True
        task.save = True
        task.replace_existing = True
        tools.import_asset_tasks([task])
        meshes = [o for o in task.get_objects() if isinstance(o, ue.StaticMesh)]
        if len(meshes) != 1:
            entries = assets.list_assets(folder, recursive=True, include_folder=False)
            meshes = [obj for name in entries if isinstance(obj := assets.load_asset(name), ue.StaticMesh)]
        if len(meshes) != 1:
            raise RuntimeError(f"{file.name} imported {len(meshes)} static meshes")
        mesh = meshes[0]
        retain_glass_geometry(key, mesh)
        if mesh.get_path_name().split(".")[0] != target:
            if not assets.rename_loaded_asset(mesh, target):
                raise RuntimeError(f"Could not give {file.name} a stable Unreal asset path")
        if not assets.save_loaded_asset(mesh, False):
            raise RuntimeError(f"Could not save {target}")
        assets.save_directory(folder, only_if_is_dirty=True, recursive=True)
        ue.log(f"NEON_FILM_MODEL {key} {mesh.get_bounds()}")
        ue.SystemLibrary.collect_garbage()
    ue.log("NEON_FILM_IMPORT_COMPLETE")


if __name__ == "__main__":
    project = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
    local_pack = project / "SourceArt" / "Models"
    default_source = local_pack if local_pack.is_dir() else project.parent.parent / "models"
    import_models(Path(os.getenv("NEON_RIDE_MODELS", str(default_source))))
