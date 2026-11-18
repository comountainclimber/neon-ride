"""Unreal editor entry point shared by first launch and unattended builds."""
import os
from pathlib import Path
import sys
import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
from import_film import import_models
from import_environment import build_environment


def import_all():
    project = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
    local = project / "SourceArt"
    original = project.parent.parent
    models = local / "Models" if (local / "Models").is_dir() else original / "models"
    art = local if (local / "textures").is_dir() else original
    import_models(Path(os.getenv("NEON_RIDE_MODELS", str(models))))
    build_environment(Path(os.getenv("NEON_RIDE_ART", str(art))))
    ue.log("NEON_RIDE_ASSETS_READY")


if __name__ == "__main__":
    import_all()
