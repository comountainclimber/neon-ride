"""Import the film models on the first editor open of a complete repository checkout."""
import importlib.util
import os
from pathlib import Path
import unreal


def schedule_film_import():
    command_line = unreal.SystemLibrary.get_command_line().lower()
    if "-run=" in command_line or "-game" in command_line or "-unattended" in command_line:
        return
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    local_pack = project / "SourceArt" / "Models"
    default_source = local_pack if local_pack.is_dir() else project.parent.parent / "models"
    source = Path(os.getenv("NEON_RIDE_MODELS", str(default_source)))
    if not (source / "bike.glb").is_file():
        return
    registry = unreal.AssetRegistryHelpers.get_asset_registry()

    def on_tick(_delta):
        if registry.is_loading_assets():
            return
        unreal.unregister_slate_post_tick_callback(handle)
        module_spec = importlib.util.spec_from_file_location("neon_asset_import", project / "Tools" / "import_assets.py")
        importer = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(importer)
        importer.import_all()

    handle = unreal.register_slate_post_tick_callback(on_tick)


schedule_film_import()
