#!/usr/bin/env python3
"""Package the complete offline source toolkit for project-specific ESP32 libraries."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import uuid
import zipfile

from feature_plan import repository_root
from make_component import copy_file, text_file


def readme(examples):
    first = '"examples/' + examples[0] + '"' if examples else '"path/to/your_project.pb"'
    return f'''# LVGL Protobuf ESP32 source toolkit

This self-contained toolkit generates an ESP-IDF display component for your own
LVGL UI Creator Protobuf projects. It includes the pinned LVGL 9.6 source and
headers, runtime, serializers, nanopb, JSON and bitmap-image support, plus the
offline analyzer and SPI display example. The original editor repository is not
needed. Python 3.10+ is required on the build machine; no Python runs on the ESP32.

From this extracted directory:

```powershell
python Runtime/esp32/make_component.py {first} --output generated/lvgl_pb_esp32 --zip
```

Pass multiple project filenames to compile their union. The generated component
contains only the LVGL modules required by their safe dependency closure, an
explicit source list, a feature report and embedded Protobuf data. It retains
LVGL core/software rendering, the inherited default theme and one fallback font.
Unused widgets, drivers, GPU/OS backends, codecs and built-in fonts are disabled
and omitted from the component's source list. The complete toolkit source stays
available so a future project can require a different set of features.
Advanced Keyboard's portable ESPOS implementation, key font, prediction engine
and small English model are included in this toolkit. Generated profiles select
only their required widget/engine/model features. Custom trie catalogs remain
shared, immutable binary assets. With PSRAM enabled and initialized on ESP32,
the engine automatically prefers PSRAM for the compact trie and index and
shares one copy between keyboards using that model. If PSRAM is unavailable
or cannot fit the model, embedded loads borrow flash bytes and file loads
retain owned model bytes.

Copy the generated component into your firmware's components directory, or try
its bundled configurable SPI ST7789 example:

```powershell
idf.py -C generated/lvgl_pb_esp32/example set-target esp32
idf.py -C generated/lvgl_pb_esp32/example menuconfig
idf.py -C generated/lvgl_pb_esp32/example build
```

Configure GPIO pins, resolution and panel options for your hardware before
flashing. ESP-IDF 5.3+ and C++ exception support are required. No separate LVGL
component is needed or allowed alongside this bundled copy. The library exposes
a C API for embedded flash data, mounted VFS files, screens and variables; the
application supplies display/input drivers and the LVGL task.

See [the complete integration and memory guide](Runtime/esp32/README.md).
`features.json` audits each generated profile. Use `idf.py size` for actual
linked flash/static RAM measurements; source-byte counts do not predict runtime
heap use. Host/cross-build checks do not verify a physical display or barcode
scanner. Re-run the generator after changing projects; changed bundled inputs
are rejected instead of silently using an outdated feature configuration.
'''


def package(output: Path, example_projects=(), replace=False):
    repo = repository_root()
    output = output.resolve()
    if output == repo or output in repo.parents:
        raise ValueError("Toolkit output must not replace its source repository")
    for directory in ("Runtime", "LVSerial", "3rdParty", "ImgLib", "CodeGeneration"):
        source = repo / directory
        if output == source or source in output.parents:
            raise ValueError("Toolkit output must be outside source directories, for example build/LVGL-Protobuf-ESP32-Toolkit")
    if output.exists() and any(output.iterdir()) and (not replace or not (output / "lvgl-pb-toolkit.json").is_file()):
        raise ValueError("Output is not empty; use a new folder or --replace for a generated toolkit")
    examples = []
    names = set()
    for source in example_projects:
        source = Path(source).resolve(strict=True)
        if source.suffix.lower() not in (".pb", ".json"):
            raise ValueError("Example projects must be .pb or .json files")
        if source.name.casefold() in names: raise ValueError("Example project filenames must be unique")
        names.add(source.name.casefold());examples.append(source)
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = output.parent / ("." + output.name + "-stage-" + uuid.uuid4().hex)
    stage.mkdir()
    try:
        def tree(source, destination, predicate=None):
            for path in source.rglob("*"):
                relative = path.relative_to(source)
                if any(part in (".git", "__pycache__", "build", "node_modules") for part in relative.parts): continue
                if path.is_file() and (predicate is None or predicate(path)):
                    copy_file(path, destination / relative)
        source_file = lambda path: path.suffix.lower() in (".h", ".hpp", ".inc", ".inl", ".c", ".cpp", ".py", ".md", ".json", ".ngtr", ".ngix", ".proto", ".options", ".txt", ".cmake", ".defaults", ".projbuild") or any(
            marker in path.name.upper() for marker in ("LICENSE", "LICENCE", "COPYING", "NOTICE"))
        tree(repo / "Runtime/esp32", stage / "Runtime/esp32", source_file)
        for directory in ("Runtime/common", "Runtime/include", "Runtime/compat", "LVSerial/Serialization", "LVSerial/Device", "LVSerial/Proto", "LVSerial/Widgets"):
            tree(repo / directory, stage / directory, source_file)
        copy_file(repo / "Runtime/README.md", stage / "Runtime/README.md")
        copy_file(repo / "LVSerial/README.md", stage / "LVSerial/README.md")
        lvgl = repo / "3rdParty/LVGL/lvgl"
        for directory in ("include", "src"):
            tree(lvgl / directory, stage / "3rdParty/LVGL/lvgl" / directory)
        for name in ("lvgl.h", "lv_version.h", "lv_conf_template.h", "LICENCE.txt"):
            if (lvgl / name).is_file(): copy_file(lvgl / name, stage / "3rdParty/LVGL/lvgl" / name)
        for path in (repo / "3rdParty/nanopb").iterdir():
            if path.is_file() and (path.suffix.lower() in (".h", ".c", ".proto") or path.name.upper().startswith(("LICENSE", "LICENCE"))):
                copy_file(path, stage / "3rdParty/nanopb" / path.name)
        for name in ("3rdParty/JSON/json.hpp", "ImgLib/stb_image.h", "CodeGeneration/Targets/Target.h", "LICENSE"):
            copy_file(repo / name, stage / name)
        copy_file(lvgl / "LICENCE.txt", stage / "licenses/LVGL_LICENSE")
        for path in lvgl.rglob("*"):
            if path.is_file() and path.name.upper().startswith(("LICENSE", "LICENCE", "COPYING")):
                copy_file(path, stage / "licenses/LVGL_dependencies" / path.relative_to(lvgl))
        for source in examples: copy_file(source, stage / "examples" / source.name)
        text_file(stage / "README.md", readme([source.name for source in examples]))
        manifest = {"formatVersion": 1, "toolkitVersion": "0.9.0", "lvglVersion": "9.6.0",
                    "examples": [source.name for source in examples],
                    "files": {path.relative_to(stage).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                              for path in sorted(stage.rglob("*")) if path.is_file()}}
        text_file(stage / "lvgl-pb-toolkit.json", json.dumps(manifest, indent=2) + "\n")
        backup = None
        if output.exists():
            backup = output.parent / ("." + output.name + "-previous-" + uuid.uuid4().hex)
            output.rename(backup)
        try: stage.rename(output)
        except BaseException:
            if backup: backup.rename(output)
            raise
        if backup:
            if backup.resolve().parent != output.parent or not backup.name.startswith("." + output.name + "-previous-"):
                raise ValueError("Toolkit backup escaped the intended output parent")
            shutil.rmtree(backup)
        archive = Path(shutil.make_archive(str(output), "zip", output.parent, output.name))
        with zipfile.ZipFile(archive) as zipped:
            bad_file = zipped.testzip()
            if bad_file: raise ValueError("Toolkit archive failed integrity check: " + bad_file)
        text_file(archive.with_suffix(archive.suffix + ".sha256"), hashlib.sha256(archive.read_bytes()).hexdigest() + "  " + archive.name + "\n")
        return archive
    finally:
        if stage.exists():
            if stage.resolve().parent != output.parent or not stage.name.startswith("." + output.name + "-stage-"):
                raise ValueError("Toolkit staging escaped the intended output parent")
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", "-o", type=Path, required=True)
    parser.add_argument("--example-project", type=Path, action="append", default=[])
    parser.add_argument("--replace", action="store_true")
    args = parser.parse_args()
    try:
        print("Created ESP32 source toolkit: " + str(package(args.output, args.example_project, args.replace)))
    except (OSError, ValueError) as error:
        print("Toolkit packaging failed: " + str(error), file=sys.stderr);return 1
    return 0


if __name__ == "__main__": sys.exit(main())
