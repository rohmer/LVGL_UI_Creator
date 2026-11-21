"""The fixed, offline Raspberry Pi/Linux project template.

The only imported generator is part of the bundled, hash-checked Linux SDK.
Remote catalog entries never choose executable paths or Python module names.
"""
from __future__ import annotations

# Keep the standard-library dependencies of the offline SDK generators visible
# to PyInstaller; those generators are loaded from their extracted source tree.
import array
import base64
import copy
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import struct
import sys
import time

TEMPLATE_ID = "raspberry-pi-linux"
TOOLKIT_MARKER = "lvgl-pb-linux-toolkit.json"
PROFILE_MARKER = "lvgl-pb-linux-sdk.json"
MODULE_NAMES = ("feature_plan", "make_component", "package_toolkit", "trie_assets",
                "linux_toolkit", "lvgl_pb_ngtr_index", "_uicreator_linux_generator")
_WINDOWS_PUBLISH = sys.platform == "win32"
_PUBLISH_DELAYS = (0.05, 0.1, 0.2, 0.4, 0.8)


def publish_directory(stage, destination):
    """Publish once, retrying only transient Windows directory rename failures.

    Scanners can briefly hold a newly written directory without delete sharing.
    Retrying the rename preserves staged content; regenerating or copying into
    the destination would expose partial projects. A destination that appears
    during the bounded retry is always a collision, even if it is empty.
    """
    stage, destination = Path(stage), Path(destination)
    for attempt in range(len(_PUBLISH_DELAYS) + 1):
        if destination.exists() or destination.is_symlink():
            raise FileExistsError(f"Project destination appeared while publishing: {destination}")
        try:
            os.rename(stage, destination)
            return
        except OSError as error:
            if not _WINDOWS_PUBLISH or getattr(error, "winerror", None) not in (5, 32, 33) or attempt == len(_PUBLISH_DELAYS):
                raise
            if destination.exists() or destination.is_symlink():
                raise FileExistsError(f"Project destination appeared while publishing: {destination}") from error
            time.sleep(_PUBLISH_DELAYS[attempt])


def manifest():
    return {
        "name": "Raspberry Pi - Linux",
        "maintainer": "LVGL UI Creator",
        "uicreatorTemplate": TEMPLATE_ID,
        "hostOperatingsystem": ["Windows", "Linux", "MacOS"],
        "environment": ["CMake", "Linux", "SDL3"],
        "hardware": {"chipVendor": "Raspberry Pi", "manufacturer": "Raspberry Pi",
                     "specs": {"Operating system": "Raspberry Pi OS 64-bit / Linux",
                               "Build": "Native CMake C/C++17",
                               "Display": "SDL3 window or application-supplied board callback"}},
        "description": "Create an offline Linux application for Raspberry Pi or a Linux desktop. "
                       "The current screen is included as Protobuf with a project-specific LVGL 9.6 "
                       "runtime, Advanced Keyboard and only its required widgets, fonts and codecs. "
                       "Build natively on the target Linux system; SDL3 sources are bundled.",
        "shortDescription": "Linux CMake build with SDL3 and a minimum LVGL 9.6 runtime for the current screen.",
        "urlToClone": "https://github.com/lvgl/lv_port_linux",
        "branches": [],
        "uiFolderPath": "ui",
        "settings": [],
        "lvglVersion": "9.6.0",
        "getStartedInstructions":
            "1. Copy the generated folder to Raspberry Pi OS 64-bit or a Linux machine with "
            "CMake 3.24+, Ninja, Python 3 and a C/C++17 compiler. Python checks the embedded "
            "profile inputs during CMake configuration. Install the graphics/input "
            "development packages for your Linux desktop; SDL3 source is included.\n"
            "2. Configure: cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release\n"
            "3. Build: cmake --build build --parallel\n"
            "4. Run: build/platform/Runtime/linux/lvgl-pb-viewer ui/project.pb\n"
            "5. For a custom display transport, configure -DLVGL_PB_WITH_SDL=OFF and use the "
            "board callback API described in platform/Runtime/linux/README.md.\n"
            "Regenerate after changing the UI so its minimum runtime includes any new widgets "
            "or assets. Building from source on the target selects its native architecture and ABI. "
            "Physical display and touch-controller drivers remain application-specific.",
    }


def merge_catalog(catalog):
    """Keep the local template available after a cache load or upstream refresh."""
    return [item for item in catalog if not isinstance(item, dict) or
            item.get("uicreatorTemplate") != TEMPLATE_ID] + [manifest()]


def validate_manifest(value):
    identity = value.get("uicreatorTemplate")
    if identity is None:
        return
    if identity != TEMPLATE_ID:
        raise ValueError("Unknown built-in project template")
    if value.get("lvglVersion", "9.6.0") != "9.6.0":
        raise ValueError("The Raspberry Pi Linux template requires bundled LVGL 9.6.0")
    if value.get("uiFolderPath", "ui") != "ui" or value.get("settings", value.get("Settings", [])):
        raise ValueError("The Raspberry Pi Linux template uses its fixed UI folder and build configuration")


def validate_request(request):
    if request.get("lvglVersion", "template") not in ("template", "9.6.0"):
        raise ValueError("The Raspberry Pi Linux build supports bundled LVGL 9.6.0 only")
    if request.get("sourceArchive") or request.get("lvglPath") or request.get("branch") or request.get("settings"):
        raise ValueError("The Raspberry Pi Linux build uses the bundled toolkit; source, branch, LVGL-folder and template-setting overrides are unavailable")
    source = Path(request.get("uiSourceFolder", ""))
    project = source / "project.pb"
    if not request.get("uiSourceFolder") or not project.is_file() or project.is_symlink():
        raise ValueError("The Raspberry Pi Linux build needs the current screen as uiSourceFolder/project.pb")
    if not 0 < project.stat().st_size <= 16 * 1024 * 1024:
        raise ValueError("The Raspberry Pi Linux Protobuf must be between 1 byte and 16 MiB")
    archive = Path(request.get("linuxToolkitArchive", ""))
    if not request.get("linuxToolkitArchive") or not archive.is_file():
        raise ValueError("The bundled Linux toolkit is missing; rebuild the project-creator assets")
    return project, archive


def _verify_toolkit(root, inside):
    marker = root / TOOLKIT_MARKER
    if not marker.is_file():
        raise ValueError("Choose the bundled Linux source toolkit, not a generated profile or binary SDK")
    data = json.loads(marker.read_text(encoding="utf-8"))
    files = data.get("files")
    if data.get("formatVersion") != 1 or data.get("lvglVersion") != "9.6.0" or not isinstance(files, dict) or not files:
        raise ValueError("The bundled Linux toolkit has an unsupported manifest")
    for name, expected in files.items():
        path = inside(root, name)
        if not path.is_file() or not isinstance(expected, str) or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"The bundled Linux toolkit failed its integrity check: {name}")
    for name in ("Runtime/linux/make_sdk.py", "Runtime/linux/package_toolkit.py", "Runtime/esp32/feature_plan.py",
                 "Runtime/esp32/make_component.py", "Runtime/esp32/package_toolkit.py", "Runtime/esp32/trie_assets.py"):
        if name not in files:
            raise ValueError(f"The bundled Linux toolkit is missing its fixed generator: {name}")


def generate(request, stage, temporary, extract_archive, inside):
    project, archive = validate_request(request)
    source = temporary / "linux-toolkit"
    source.mkdir()
    extract_archive(archive.read_bytes(), source)
    _verify_toolkit(source, inside)
    previous_path = list(sys.path)
    previous_modules = {name: sys.modules.pop(name) for name in MODULE_NAMES if name in sys.modules}
    try:
        spec = importlib.util.spec_from_file_location("_uicreator_linux_generator", source / "Runtime/linux/make_sdk.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.generate([project], stage, publish_directory=publish_directory)
    finally:
        sys.path[:] = previous_path
        for name in MODULE_NAMES:
            sys.modules.pop(name, None)
        sys.modules.update(previous_modules)
    plan = json.loads((stage / "embedded/features.json").read_text(encoding="utf-8"))
    projects = plan.get("projects", [])
    if len(projects) != 1 or not isinstance(projects[0].get("file"), str):
        raise ValueError("The Linux profile generator did not produce the current screen")
    optimized = inside(stage / "embedded/projects", projects[0]["file"])
    if not optimized.is_file():
        raise ValueError("The Linux profile is missing its optimized Protobuf screen")
    return {"optimizedProject": optimized.relative_to(stage).as_posix(),
            "lvglSourceFileCount": plan["lvglSourceFileCount"],
            "fullLvglSourceFileCount": plan["fullLvglSourceFileCount"]}


def update_profile_manifest(root):
    files = {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
             for path in sorted(root.rglob("*")) if path.is_file() and path.name != PROFILE_MARKER}
    (root / PROFILE_MARKER).write_text(json.dumps({"formatVersion": 1, "lvglVersion": "9.6.0", "files": files}, indent=2) + "\n", encoding="utf-8")
