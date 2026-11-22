#!/usr/bin/env python3
"""Create the offline Pico/Pico 2 minimum-LVGL source toolkit."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import uuid
import zipfile

OWN = Path(__file__).resolve().parent
sys.path.insert(0, str(OWN.parent / "esp32"))
from feature_plan import repository_root
from make_component import text_file
spec = importlib.util.spec_from_file_location("pico_esp_toolkit", OWN.parent / "esp32/package_toolkit.py")
shared = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shared)
MARKER = "lvgl-pb-pico-toolkit.json"


def package(output: Path, examples=(), replace=False):
    repo = repository_root()
    output = Path(output).resolve()
    if output == repo or output in repo.parents or any(output == repo / name or (repo / name) in output.parents
        for name in ("Runtime", "LVSerial", "3rdParty", "UICreator", "ImgLib", "CodeGeneration")):
        raise ValueError("Pico toolkit output must be outside source directories")
    if output.exists() and any(output.iterdir()) and (not replace or not (output / MARKER).is_file()):
        raise ValueError("Choose a new folder or --replace for a generated Pico toolkit")
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = output.parent / ("." + output.name + "-stage-" + uuid.uuid4().hex)
    try:
        archive = shared.package(stage, examples)
        archive.unlink(); Path(str(archive) + ".sha256").unlink()
        (stage / "lvgl-pb-toolkit.json").unlink()
        shutil.copytree(OWN, stage / "Runtime/pico", ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        shutil.copy2(OWN / "README.md", stage / "README.md")
        text_file(stage / MARKER, json.dumps({
            "formatVersion": 1, "toolkitVersion": "0.9.1", "lvglVersion": "9.6.0",
            "boards": ["pico", "pico2"], "platform": "Raspberry Pi Pico SDK",
            "files": {path.relative_to(stage).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                      for path in sorted(stage.rglob("*")) if path.is_file()},
        }, indent=2) + "\n")
        backup = None
        if output.exists():
            backup = output.parent / ("." + output.name + "-previous-" + uuid.uuid4().hex)
            output.rename(backup)
        try: stage.rename(output)
        except BaseException:
            if backup: backup.rename(output)
            raise
        if backup:
            if backup.resolve().parent != output.parent: raise ValueError("Toolkit backup escaped output parent")
            shutil.rmtree(backup)
        archive = Path(shutil.make_archive(str(output), "zip", output.parent, output.name))
        with zipfile.ZipFile(archive) as check:
            if check.testzip(): raise ValueError("Pico toolkit ZIP integrity check failed")
        text_file(Path(str(archive) + ".sha256"), hashlib.sha256(archive.read_bytes()).hexdigest() + "  " + archive.name + "\n")
        return archive
    finally:
        if stage.exists():
            if stage.resolve().parent != output.parent or not stage.name.startswith("." + output.name + "-stage-"):
                raise ValueError("Toolkit staging escaped output parent")
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", "-o", type=Path, required=True)
    parser.add_argument("--example-project", type=Path, action="append", default=[])
    parser.add_argument("--replace", action="store_true")
    args = parser.parse_args()
    try: print(package(args.output, args.example_project, args.replace))
    except (OSError, ValueError) as error:
        print("Pico toolkit generation failed: " + str(error), file=sys.stderr); return 1
    return 0


if __name__ == "__main__": sys.exit(main())
