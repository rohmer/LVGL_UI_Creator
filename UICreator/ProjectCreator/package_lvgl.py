"""Package the vendored 9.6 source as an offline project-creator dependency."""
from pathlib import Path
import sys,zipfile
source=Path(sys.argv[1]);destination=Path(sys.argv[2]);destination.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED) as archive:
    for path in sorted(source.rglob('*')):
        if path.is_file() and not any(part in ('.git','__pycache__') for part in path.relative_to(source).parts):
            archive.write(path,'lvgl/'+path.relative_to(source).as_posix())
