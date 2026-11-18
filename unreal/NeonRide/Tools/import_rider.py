"""Import the Tripo rider, bind skeleton and crash animation with stable game paths."""
from pathlib import Path
import unreal as ue


def import_rider(source):
    assets = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    folder = "/Game/Rider"
    def import_file(path, target):
        if assets.does_asset_exist(folder + "/" + target):
            return []
        if not path.is_file():
            raise FileNotFoundError(path)
        task = ue.AssetImportTask()
        task.filename = str(path)
        task.destination_path = folder + "/" + path.stem.replace("-", "_")
        task.automated = True
        task.save = True
        ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        return [assets.load_asset(name) for name in assets.list_assets(task.destination_path, recursive=True, include_folder=False)]
    def rename(asset, target):
        if not assets.rename_loaded_asset(asset, folder + "/" + target):
            raise RuntimeError("Could not name rider asset " + target)
        assets.save_loaded_asset(asset, False)
    static = import_file(source / "vesper-brace.glb", "SM_Vesper")
    for mesh in [a for a in static if isinstance(a, ue.StaticMesh)]:
        ue.log(f"VESPER_STATIC {mesh.get_bounds()}")
        rename(mesh, "SM_Vesper")
    animated = import_file(source / "vesper-animated.glb", "SK_Vesper")
    for asset in animated:
        ue.log(f"VESPER_IMPORT {asset.get_class().get_name()} {asset.get_name()}")
        if isinstance(asset, ue.SkeletalMesh):
            ue.log(f"VESPER_BOUNDS {asset.get_bounds()}")
            rename(asset, "SK_Vesper")
        elif isinstance(asset, ue.AnimSequence) and "crash" in asset.get_name().lower():
            rename(asset, "A_CrashEject")
    for target in ("SK_Vesper", "A_CrashEject", "SM_Vesper"):
        if not assets.does_asset_exist(folder + "/" + target):
            raise RuntimeError("Imported rider missing " + target)
    assets.save_directory(folder, only_if_is_dirty=True, recursive=True)

if __name__ == "__main__":
    project = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
    import_rider(project / "SourceArt" / "Rider")
