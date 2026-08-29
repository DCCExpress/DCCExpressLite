from pathlib import Path
from shutil import copy2

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
source_dir = project_dir / "default-data"
data_dir = project_dir / "data"

starter_files = (
    "layout.json",
    "locos.json",
    "devices.json",
)

starter_images = (
    "bobo.png",
    "csorgo.png",
    "szergej.png",
    "traxx.png",
)

data_dir.mkdir(parents=True, exist_ok=True)

for name in starter_files:
    source = source_dir / name
    destination = data_dir / name
    if not source.is_file():
        raise RuntimeError(f"Missing required starter data: {source}")
    if not destination.exists():
        copy2(source, destination)
        print(f"Staged default data: {name}")

source_images = source_dir / "images"
data_images = data_dir / "images"
data_images.mkdir(parents=True, exist_ok=True)

for name in starter_images:
    source = source_images / name
    destination = data_images / name
    if not source.is_file():
        raise RuntimeError(f"Missing required starter image: {source}")
    if not destination.exists():
        copy2(source, destination)
        print(f"Staged default image: images/{name}")
