"""Fixed offline Pico SDK templates for Raspberry Pi Pico 1 and Pico 2."""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import sys

TEMPLATE_BOARDS = {"raspberry-pi-pico": "pico", "raspberry-pi-pico2": "pico2"}
TOOLKIT_MARKER = "lvgl-pb-pico-toolkit.json"
PROFILE_MARKER = "lvgl-pb-pico-sdk.json"
MODULE_NAMES = ("feature_plan", "make_component", "trie_assets", "font_assets", "package_toolkit",
                "lvgl_pb_ngtr_index", "_uicreator_pico_generator")


def manifests():
    result = []
    for identity, board in TEMPLATE_BOARDS.items():
        number, chip = ("1", "RP2040") if board == "pico" else ("2", "RP2350")
        result.append({
            "name": "Raspberry Pi Pico " + number,
            "maintainer": "LVGL UI Creator", "uicreatorTemplate": identity,
            "hostOperatingsystem": ["Windows", "Linux", "MacOS"],
            "environment": ["CMake", "Pico SDK"],
            "hardware": {"chipVendor": "Raspberry Pi", "manufacturer": "Raspberry Pi",
                         "specs": {"MCU": chip, "Board": board,
                                   "Output": "ELF, binary and UF2 firmware",
                                   "Display": "Application-supplied pixel/touch callbacks"}},
            "description": "Create a native Pico SDK firmware project for Raspberry Pi Pico " + number +
                ". C output links the generated screen directly; Protobuf output includes the "
                "project-specific display library. Both use only the selected LVGL 9.6 modules "
                "and provide UF2 firmware output. Configure display and touch callbacks for your panel.",
            "shortDescription": chip + " Pico SDK firmware with generated C or Protobuf UI and UF2 output.",
            "urlToClone": "https://github.com/raspberrypi/pico-sdk", "branches": [],
            "uiFolderPath": "ui", "settings": [], "lvglVersion": "9.6.0",
            "getStartedInstructions":
                "1. Install Raspberry Pi Pico SDK 2.3.1 or newer, its documented ARM compiler tools, CMake, "
                "Ninja and Python 3.10+. Set PICO_SDK_PATH to your SDK checkout. The UI library "
                "sources are bundled; the Pico SDK remains a build prerequisite.\n"
                "2. Configure: cmake -S . -B build -G Ninja -DPICO_BOARD=" + board + "\n"
                "3. Build: cmake --build build --parallel\n"
                "4. Hold BOOTSEL while connecting the board and copy build/*.uf2 to its USB drive.\n"
                "5. Implement the example's pixel-write and touch callbacks for your display pins, "
                "driver and resolution. The default callbacks permit a firmware build without "
                "assuming a particular panel. Use the same LVGL owner thread for all UI calls.\n"
                "C mode calls ui_init and links generated_ui; Protobuf mode loads the embedded "
                "project through the Pico display library. Regenerate after changing the design.",
        })
    return result


def merge_catalog(catalog):
    return [item for item in catalog if not isinstance(item, dict) or
            item.get("uicreatorTemplate") not in TEMPLATE_BOARDS] + manifests()


def validate_manifest(value):
    if value.get("uicreatorTemplate") not in TEMPLATE_BOARDS:
        raise ValueError("Unknown built-in Pico project template")
    if value.get("lvglVersion", "9.6.0") != "9.6.0":
        raise ValueError("The Pico templates require bundled LVGL 9.6.0")
    if value.get("uiFolderPath", "ui") != "ui" or value.get("settings", value.get("Settings", [])):
        raise ValueError("The Pico templates use their fixed UI folder and board configuration")


def validate_request(request):
    if request.get("lvglVersion", "template") not in ("template", "9.6.0"):
        raise ValueError("Pico projects support bundled LVGL 9.6.0 only")
    if request.get("sourceArchive") or request.get("lvglPath") or request.get("branch") or request.get("settings"):
        raise ValueError("Pico projects use the bundled toolkit and fixed board configuration")
    output_format = request.get("format", "protobuf")
    if output_format not in ("json", "protobuf", "c"):
        raise ValueError("Unknown Pico UI output format")
    source = Path(request.get("uiSourceFolder", ""))
    project = source / "project.pb"
    if not request.get("uiSourceFolder") or not project.is_file() or project.is_symlink():
        raise ValueError("Pico projects need the current screen as uiSourceFolder/project.pb")
    if not 0 < project.stat().st_size <= 16 * 1024 * 1024:
        raise ValueError("The Pico Protobuf must be between 1 byte and 16 MiB")
    if output_format == "c" and not all((source / name).is_file() and not (source / name).is_symlink()
                                         for name in ("ui.c", "ui.h", "CMakeLists.txt")):
        raise ValueError("Pico C mode needs a complete generated C project")
    archive = Path(request.get("picoToolkitArchive", ""))
    if not request.get("picoToolkitArchive") or not archive.is_file():
        raise ValueError("The bundled Pico toolkit is missing; rebuild project-creator assets")
    return project, source, archive


def _verify_toolkit(root, inside):
    marker = root / TOOLKIT_MARKER
    if not marker.is_file():
        raise ValueError("Choose the bundled Pico source toolkit, not a generated profile")
    data = json.loads(marker.read_text(encoding="utf-8"))
    files = data.get("files")
    if data.get("formatVersion") != 1 or data.get("lvglVersion") != "9.6.0" or not isinstance(files, dict) or not files:
        raise ValueError("The Pico toolkit has an unsupported manifest")
    for name, expected in files.items():
        path = inside(root, name)
        if not path.is_file() or not isinstance(expected, str) or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError("The Pico toolkit failed its integrity check: " + name)
    for name in ("Runtime/pico/make_sdk.py", "Runtime/esp32/feature_plan.py",
                 "Runtime/esp32/make_component.py", "Runtime/esp32/trie_assets.py"):
        if name not in files:
            raise ValueError("The Pico toolkit is missing its fixed generator: " + name)


def generate(request, stage, temporary, extract_archive, inside):
    project, generated, archive = validate_request(request)
    source = temporary / "pico-toolkit"
    source.mkdir()
    extract_archive(archive.read_bytes(), source)
    _verify_toolkit(source, inside)
    previous_path = list(sys.path)
    previous_modules = {name: sys.modules.pop(name) for name in MODULE_NAMES if name in sys.modules}
    board = TEMPLATE_BOARDS[request["manifest"]["uicreatorTemplate"]]
    output_format = "c" if request.get("format") == "c" else "protobuf"
    try:
        spec = importlib.util.spec_from_file_location("_uicreator_pico_generator", source / "Runtime/pico/make_sdk.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.generate([project], stage, board=board,
                        generated_c=generated if output_format == "c" else None,
                        archive=False, copy_ui=False)
    finally:
        sys.path[:] = previous_path
        for name in MODULE_NAMES:
            sys.modules.pop(name, None)
        sys.modules.update(previous_modules)
    if not (stage / PROFILE_MARKER).is_file() or not (stage / "CMakeLists.txt").is_file():
        raise ValueError("Pico generation did not produce a complete firmware project")
    return {"board": board, "format": output_format, "uf2Output": "build/*.uf2"}


def update_profile_manifest(root):
    marker = root / PROFILE_MARKER
    data = json.loads(marker.read_text(encoding="utf-8"))
    data["files"] = {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                     for path in sorted(root.rglob("*")) if path.is_file() and path != marker}
    marker.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
