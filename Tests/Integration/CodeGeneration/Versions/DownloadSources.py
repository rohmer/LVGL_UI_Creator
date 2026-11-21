"""Download pinned official LVGL sources for the optional cross-version matrix.
Usage: python DownloadSources.py [destination, default build/compat-lvgl]
"""
import io,sys,urllib.request,zipfile
from pathlib import Path
root=Path(sys.argv[1] if len(sys.argv)>1 else "build/compat-lvgl").resolve()
for version in ("9.5.0","9.3.0","9.2.2","9.1.0","8.3.11"):
    target=root/("v"+version.replace(".","_"))
    print("Downloading LVGL "+version,flush=True)
    content=urllib.request.urlopen("https://codeload.github.com/lvgl/lvgl/zip/refs/tags/v"+version,timeout=180).read()
    with zipfile.ZipFile(io.BytesIO(content)) as archive:
        for item in archive.infolist():
            relative="/".join(item.filename.split("/")[1:]);parts=Path(relative).parts
            if item.is_dir() or not relative:continue
            if any(p==".." for p in parts) or Path(relative).is_absolute():raise RuntimeError("Unsafe archive path")
            if relative.startswith(("src/","include/","env_support/cmake/")) or "/" not in relative and relative.endswith((".h",".txt")):
                path=target/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(archive.read(item))
    (target/"UPSTREAM_VERSION").write_text(version+"\nhttps://github.com/lvgl/lvgl/tree/v"+version+"\n")
