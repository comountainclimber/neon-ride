"""Import the same six courier clients used in the browser release."""
from pathlib import Path
import sys
import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
from import_film import import_models

def import_couriers(project):
    folder = project.parent.parent / "web" / "assets" / "models"
    if not folder.is_dir():
        folder = project.parent / "NeonRideCircuit" / "assets" / "models"
    clients = ("night-porter", "arcade-clerk", "pump-operator", "canal-keeper", "cinema-projectionist", "clinic-physician")
    import_models(folder, clients)

if __name__ == "__main__":
    import_couriers(Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())))
