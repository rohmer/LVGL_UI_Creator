"""Prepare checksum-verified, entirely 32-bit Windows converter helpers.

Uses a private CPython NuGet package; never installs Python or Node globally.
The existing converter sources and protocols are identical to the x64 build.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import urllib.request
import zipfile

NODE_VERSION = "22.23.5"
NODE_SHA256 = "9c01245683c87e3f81eedfbad82c1665611749627cd1b0b5a42921db9361b4a6"
PYTHON_VERSION = "3.14.10"
# CPython team's NuGet catalog packageHash, SHA512, for pythonx86 3.14.10.
PYTHON_SHA512 = "Lhvpz4mysDXDB/BMIUg7XNXMZzbTlJ5lEyRw66WGmd4/KNhi/x1Pr+LdpjmcTBr93g8J20mSyOWQQibZmblQeg=="
PACKAGES = {
    "altgraph": "0.17.5", "packaging": "26.3", "pefile": "2024.8.26",
    "pillow": "12.1.1", "lz4": "4.4.5", "pypng": "0.20220715.0",
    "resvg-py": "0.2.6", "pyinstaller": "6.19.0",
    "pyinstaller-hooks-contrib": "2026.10", "pywin32-ctypes": "0.2.3",
}


def sha256(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def download(url: str, target: Path, expected: str, algorithm="sha256") -> Path:
    def matches(path):
        value = hashlib.new(algorithm, path.read_bytes()).digest()
        return value.hex() == expected if algorithm == "sha256" else base64.b64encode(value).decode() == expected
    if target.is_file() and matches(target):
        return target
    print("Download " + url, flush=True)
    target.parent.mkdir(parents=True, exist_ok=True)
    partial = target.with_name(target.name + ".partial")
    with urllib.request.urlopen(url, timeout=120) as source, partial.open("wb") as output:
        shutil.copyfileobj(source, output)
    if not matches(partial):
        raise ValueError("Official download checksum differs: " + target.name)
    partial.replace(target)
    return target


def extract(archive: Path, target: Path):
    target.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as source:
        for name in source.namelist():
            if not (target / name).resolve().is_relative_to(target.resolve()):
                raise ValueError("Archive path escapes its private tool directory")
        source.extractall(target)


def audit(directory: Path):
    result = {}
    for path in sorted(directory.rglob("*")):
        if not path.is_file():
            continue
        with path.open("rb") as stream:
            if stream.read(2) != b"MZ":
                continue
            stream.seek(0x3c)
            offset = struct.unpack("<I", stream.read(4))[0]
            stream.seek(offset)
            if stream.read(4) != b"PE\0\0" or struct.unpack("<H", stream.read(2))[0] != 0x14c:
                raise ValueError("A helper contains a non-x86 Windows executable: " + str(path))
        result[str(path.relative_to(directory))] = sha256(path)
    if not result:
        raise ValueError("No native helper executables were produced: " + str(directory))
    return result


def execute(command, environment, log):
    print("+ " + subprocess.list2cmdline(list(map(str, command))), flush=True)
    result = subprocess.run(list(map(str, command)), env=environment, stdout=log, stderr=subprocess.STDOUT)
    log.flush()
    if result.returncode:
        raise RuntimeError("Helper build failed; inspect " + str(log.name))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--architecture", choices=("x86",), default="x86")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if os.name != "nt":
        raise ValueError("Prepare Windows helpers on Windows")
    repo = Path(__file__).resolve().parent.parent
    output = (args.output or repo / "build/windows-helper-tools/x86").resolve()
    output.mkdir(parents=True, exist_ok=True)
    downloads = output / "downloads"
    downloads.mkdir(exist_ok=True)
    environment = os.environ.copy()
    for name in ("PYTHONHOME", "PYTHONPATH"):
        environment.pop(name, None)
    environment.update(PYTHONNOUSERSITE="1", PYTHONUTF8="1", PIP_DISABLE_PIP_VERSION_CHECK="1")
    node_name = f"node-v{NODE_VERSION}-win-x86"
    node_archive = download(f"https://nodejs.org/dist/v{NODE_VERSION}/{node_name}.zip", downloads / (node_name + ".zip"), NODE_SHA256)
    node_dir = output / node_name
    if not (node_dir / "node.exe").is_file():
        extract(node_archive, output)
    python_archive = download(f"https://api.nuget.org/v3-flatcontainer/pythonx86/{PYTHON_VERSION}/pythonx86.{PYTHON_VERSION}.nupkg", downloads / f"pythonx86.{PYTHON_VERSION}.nupkg", PYTHON_SHA512, "sha512")
    python_dir = output / f"pythonx86-{PYTHON_VERSION}"
    python = python_dir / "tools/python.exe"
    if not python.is_file():
        extract(python_archive, python_dir)
    node = node_dir / "node.exe"
    if subprocess.check_output([str(node), "-p", "process.arch"], env=environment, text=True).strip() != "ia32":
        raise ValueError("Font Node must execute as ia32")
    if subprocess.check_output([str(python), "-c", "import struct;print(struct.calcsize('P')*8)"], env=environment, text=True).strip() != "32":
        raise ValueError("Converter Python must execute as 32-bit")
    inputs = {str(path.relative_to(repo)): sha256(path) for path in [
        Path(__file__), repo / "UICreator/Images/image-converter-bridge.py",
        repo / "3rdParty/lvgl_image_converter/LVGLImage.py",
        repo / "UICreator/ProjectCreator/project_creator.py",
        repo / "UICreator/ProjectCreator/raspberry_pi_linux.py",
        repo / "UICreator/ProjectCreator/raspberry_pi_pico.py",
    ]}
    fingerprint = {"node": NODE_VERSION, "python": PYTHON_VERSION, "packages": PACKAGES, "inputs": inputs}
    stamp = output / "built-inputs.json"
    image = output / "dist/UICreatorImageTool"
    project = output / "dist/UICreatorProjectTool"
    rebuild = not stamp.is_file() or json.loads(stamp.read_text()) != fingerprint or not (image / "UICreatorImageTool.exe").is_file() or not (project / "UICreatorProjectTool.exe").is_file()
    artifacts = {node_archive.name: {"sha256": sha256(node_archive)}, python_archive.name: {"sha256": sha256(python_archive)}}
    if rebuild:
        with (output / "helper-build.log").open("w", encoding="utf-8") as log:
            if subprocess.run([str(python), "-m", "pip", "--version"], env=environment, stdout=log, stderr=log).returncode:
                execute([python, "-m", "ensurepip", "--default-pip"], environment, log)
            wheels = []
            for package, version in PACKAGES.items():
                with urllib.request.urlopen(f"https://pypi.org/pypi/{package}/{version}/json", timeout=60) as source:
                    metadata = json.load(source)
                candidates = [item for item in metadata["urls"] if item["filename"].endswith(("-cp314-cp314-win32.whl", "-py3-none-win32.whl", "-py3-none-any.whl", "-py2.py3-none-any.whl"))]
                if len(candidates) != 1:
                    raise ValueError(f"Expected exactly one compatible x86 wheel for {package}: {[item['filename'] for item in candidates]}")
                item = candidates[0]
                wheel = download(item["url"], downloads / item["filename"], item["digests"]["sha256"])
                wheels.append(wheel)
                artifacts[wheel.name] = {"sha256": sha256(wheel)}
            execute([python, "-m", "pip", "install", "--no-index", "--no-deps", "--upgrade", *wheels], environment, log)
            execute([python, "-c", "import PIL,png,lz4,resvg_py,PyInstaller; assert resvg_py.svg_to_bytes(svg_string='<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"2\" height=\"2\"><rect width=\"2\" height=\"2\" fill=\"red\"/></svg>')"], environment, log)
            common = [python, "-m", "PyInstaller", "--noconfirm", "--clean", "--onedir", "--distpath", output / "dist", "--specpath", output / "spec"]
            execute([*common, "--name", "UICreatorImageTool", "--workpath", output / "work/image", "--paths", repo / "3rdParty/lvgl_image_converter", "--collect-all", "resvg_py", "--collect-all", "lz4", repo / "UICreator/Images/image-converter-bridge.py"], environment, log)
            execute([*common, "--name", "UICreatorProjectTool", "--workpath", output / "work/project", repo / "UICreator/ProjectCreator/project_creator.py"], environment, log)
        stamp.write_text(json.dumps(fingerprint, indent=2) + "\n")
    audit_node = audit(node_dir)
    audit_image = audit(image)
    audit_project = audit(project)
    result = {"schema": "uicreator-windows-native-helpers", "version": 1, "architecture": "x86",
              "nodeVersion": NODE_VERSION, "pythonVersion": PYTHON_VERSION,
              "fontNode": str(node), "fontNpm": str(node_dir / "npm.cmd"), "imagePython": str(python),
              "imageToolDirectory": str(image), "projectToolDirectory": str(project),
              "nativePE": {"font": audit_node, "image": audit_image, "project": audit_project},
              "artifacts": artifacts, "sources": fingerprint}
    manifest = output / "cmake-helpers.json"
    if manifest.is_file() and not rebuild:
        result["artifacts"] = json.loads(manifest.read_text()).get("artifacts", artifacts)
    manifest.write_text(json.dumps(result, indent=2) + "\n")
    print(f"Verified entirely x86 helpers: {manifest}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
