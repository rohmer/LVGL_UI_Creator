#!/usr/bin/env python3
"""Package the offline Arduino source generator and all required library sources."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile

ESP_TOOLS = Path(__file__).resolve().parents[1] / "esp32"
sys.path.insert(0, str(ESP_TOOLS))
from feature_plan import repository_root
from make_component import text_file
spec = importlib.util.spec_from_file_location("esp32_source_toolkit", ESP_TOOLS / "package_toolkit.py")
shared = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shared)


def package(output: Path, example_projects=(), replace=False):
    output = output.resolve()
    repo = repository_root()
    if output == repo or output in repo.parents:
        raise ValueError("Toolkit output cannot replace its repository")
    for name in ("Runtime", "LVSerial", "3rdParty", "ImgLib", "CodeGeneration", "UICreator"):
        source = repo / name
        if output == source or source in output.parents: raise ValueError("Choose an output outside source directories")
    if output.exists() and any(output.iterdir()) and (not replace or not (output / "lvgl-pb-arduino-toolkit.json").is_file()):
        raise ValueError("Output is not empty; --replace requires a generated Arduino toolkit")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".arduino-toolkit-", dir=output.parent) as temporary:
        stage = Path(temporary).resolve()
        if stage.parent != output.parent or not stage.name.startswith(".arduino-toolkit-"):
            raise ValueError("Toolkit staging escaped its intended parent")
        toolkit = stage / "toolkit"
        shared.package(toolkit, example_projects)
        (toolkit / "lvgl-pb-toolkit.json").unlink()
        shutil.copytree(Path(__file__).parent, toolkit / "Runtime/arduino",
                        ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        examples = sorted(path.name for path in (toolkit / "examples").glob("*") if path.is_file())
        first = '"examples/' + examples[0] + '"' if examples else '"your-project.pb"'
        text_file(toolkit / "README.md", f'''# LVGL Protobuf Arduino Nano ESP32 toolkit

This standalone source toolkit generates an Arduino library containing only
the LVGL modules needed by your saved Protobuf projects. It includes the
runtime, serializers, LVGL9.6, assets, advanced widgets, trie prediction engine,
source licenses and offline generator. The editor checkout is unnecessary.

```powershell
python Runtime/arduino/make_library.py {first} --output generated/LVGLProtobuf --zip
arduino-cli core install arduino:esp32
arduino-cli compile --fqbn arduino:esp32:nano_nora --library generated/LVGLProtobuf generated/LVGLProtobuf/examples/ProtobufDisplay
```

Python3.10+ and Arduino IDE/CLI plus the Arduino ESP32 Boards package are build
tools, rather than firmware dependencies. The supplied sketch's build_opt.h
enables C++17, exceptions and RTTI. Install the generated library ZIP in
Arduino IDE, or supply its folder to Arduino CLI. Include all projects that
the firmware may load when generating its minimum feature profile.

Nano ESP32 has no built-in display. Supply your display/touch driver callbacks
and hardware pins in the example. Embedded Protobuf stays in flash; files can
be loaded through Arduino FS. When ESP32 PSRAM is enabled and initialized,
compact tries and indices automatically prefer PSRAM, with one shared copy
per model. If PSRAM is unavailable or cannot fit the model, embedded models
remain in flash and file loads retain owned model bytes. No second LVGL
library or ESP-IDF CMake project is required. The complete source toolkit is
retained so a future UI can select a different feature closure.

See [Arduino integration and memory details](Runtime/arduino/README.md).
''')
        manifest = {"formatVersion": 1, "toolkitVersion": "0.9.2", "lvglVersion": "9.6.0",
                    "platform": "Arduino Nano ESP32", "examples": examples,
                    "files": {path.relative_to(toolkit).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                              for path in sorted(toolkit.rglob("*")) if path.is_file() and path.name not in ("lvgl-pb-toolkit.json", "lvgl-pb-arduino-toolkit.json")}}
        text_file(toolkit / "lvgl-pb-arduino-toolkit.json", json.dumps(manifest, indent=2) + "\n")
        if output.exists():
            if output.parent != stage.parent: raise ValueError("Toolkit target escaped its intended parent")
            shutil.rmtree(output)
        toolkit.rename(output)
    archive = Path(shutil.make_archive(str(output), "zip", output.parent, output.name))
    with zipfile.ZipFile(archive) as zipped:
        if zipped.testzip(): raise ValueError("Toolkit archive integrity check failed")
    text_file(archive.with_suffix(".zip.sha256"), hashlib.sha256(archive.read_bytes()).hexdigest()+"  "+archive.name+"\n")
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", "-o", type=Path, required=True)
    parser.add_argument("--example-project", type=Path, action="append", default=[])
    parser.add_argument("--replace", action="store_true")
    args = parser.parse_args()
    try: print("Created Arduino toolkit: " + str(package(args.output, args.example_project, args.replace)))
    except (OSError, ValueError) as error: print("Toolkit packaging failed: " + str(error), file=sys.stderr);return 1
    return 0

if __name__ == "__main__": sys.exit(main())
