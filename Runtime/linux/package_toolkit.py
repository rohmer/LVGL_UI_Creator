#!/usr/bin/env python3
"""Package the complete offline Linux/Raspberry Pi LVGL Protobuf source SDK."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import uuid
import zipfile

OWN = Path(__file__).resolve().parent
sys.path.insert(0, str(OWN.parent / "esp32"))
from feature_plan import repository_root
from make_component import copy_file, text_file
import package_toolkit as esp_toolkit


def copy_platform(repo: Path, destination: Path, *, bundle_sdl=True, profile=False):
    def tree(source, target):
        shutil.copytree(source, target, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns(".git", "__pycache__", "build", "node_modules"))
    tree(repo / "Runtime/linux", destination / "Runtime/linux")
    if profile:
        for name in ("make_sdk.py", "package_toolkit.py", "tests/make_fixture.py"):
            (destination / "Runtime/linux" / name).unlink(missing_ok=True)
    for name in ("Display.cpp", "Viewer.cpp", "include/lvgl_pb_display.h", "example.c"):
        copy_file(repo / "Runtime/windows" / name, destination / "Runtime/windows" / name)
    tree(repo / "UICreator/Drivers/SDL3", destination / "UICreator/Drivers/SDL3")
    if bundle_sdl:
        source = repo / "3rdParty/SDL3"
        # SDL's upstream examples/test media are not needed to build this SDK.
        for path in source.iterdir():
            target = destination / "3rdParty/SDL3" / path.name
            if path.is_file(): copy_file(path, target)
            elif path.name in ("build-scripts", "cmake", "include", "src", "wayland-protocols"): tree(path, target)


def finish_archive(output: Path, marker: str):
    files = {path.relative_to(output).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
             for path in sorted(output.rglob("*")) if path.is_file() and path.name != marker}
    text_file(output / marker, json.dumps({"formatVersion": 1, "lvglVersion": "9.6.0", "files": files}, indent=2) + "\n")
    archive = Path(shutil.make_archive(str(output), "zip", output.parent, output.name))
    with zipfile.ZipFile(archive) as zipped:
        if zipped.testzip(): raise ValueError("SDK archive integrity check failed")
    text_file(archive.with_suffix(archive.suffix + ".sha256"), hashlib.sha256(archive.read_bytes()).hexdigest() + "  " + archive.name + "\n")
    return archive


def package(output: Path, examples=(), replace=False, bundle_sdl=True):
    repo = repository_root(); output = output.resolve()
    if output == repo or output in repo.parents or any((repo / name) == output or (repo / name) in output.parents
        for name in ("Runtime", "LVSerial", "3rdParty", "UICreator", "ImgLib", "CodeGeneration")):
        raise ValueError("SDK output must be outside source directories")
    marker = "lvgl-pb-linux-toolkit.json"
    if output.exists() and any(output.iterdir()) and (not replace or not (output / marker).is_file()):
        raise ValueError("Choose a new output directory, or --replace for an existing Linux toolkit")
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = output.parent / ("." + output.name + "-stage-" + uuid.uuid4().hex)
    try:
        base_archive = esp_toolkit.package(stage, examples)
        base_archive.unlink(); base_archive.with_suffix(base_archive.suffix + ".sha256").unlink()
        (stage / "lvgl-pb-toolkit.json").unlink()
        copy_platform(repo, stage, bundle_sdl=bundle_sdl)
        for name in ("LVSerial/CMakeLists.txt", "Runtime/CMakeLists.txt", "3rdParty/LVGL/lv_conf_v9.h"):
            copy_file(repo / name, stage / name)
        readme = (OWN / "README.md").read_text(encoding="utf-8")
        if examples: readme = readme.replace("examples/project.pb", "examples/" + Path(examples[0]).name)
        text_file(stage / "README.md", readme)
        text_file(stage / "CMakeLists.txt", 'cmake_minimum_required(VERSION 3.24)\nproject(LVGLProtobufLinuxSDK LANGUAGES C CXX)\ninclude(CTest)\nadd_subdirectory(Runtime/linux)\n')
        if output.exists(): shutil.rmtree(output)
        stage.rename(output)
        return finish_archive(output, marker)
    finally:
        if stage.exists() and stage.parent == output.parent and stage.name.startswith("." + output.name + "-stage-"):
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", "-o", type=Path, required=True)
    parser.add_argument("--example-project", type=Path, action="append", default=[])
    parser.add_argument("--replace", action="store_true")
    parser.add_argument("--system-sdl", action="store_true", help="Omit bundled SDL3; require a system SDL3 development package")
    args = parser.parse_args()
    try: print(package(args.output, args.example_project, args.replace, not args.system_sdl))
    except (OSError, ValueError) as error: print("Linux toolkit: " + str(error), file=sys.stderr); return 1
    return 0

if __name__ == "__main__": sys.exit(main())
