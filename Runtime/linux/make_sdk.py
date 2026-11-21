#!/usr/bin/env python3
"""Create a self-contained project-specific Linux/Pi display SDK with minimum LVGL source closure."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import shutil
import sys
import uuid

OWN = Path(__file__).resolve().parent
sys.path.insert(0, str(OWN.parent / "esp32"))
from feature_plan import repository_root
from make_component import make_component, text_file
# This local module has a unique name when imported, avoiding the ESP32
# package_toolkit module which the shared offline toolkit builder also uses.
import importlib.util
spec = importlib.util.spec_from_file_location("linux_toolkit", OWN / "package_toolkit.py")
linux = importlib.util.module_from_spec(spec); spec.loader.exec_module(linux)


def generate(inputs, output: Path, *, color_depth=32, default_font=14, replace=False, bundle_sdl=True, publish_directory=None):
    repo = repository_root(); output = output.resolve()
    if output == repo or output in repo.parents or any((repo / name) == output or (repo / name) in output.parents
        for name in ("Runtime", "LVSerial", "3rdParty", "UICreator", "ImgLib", "CodeGeneration")):
        raise ValueError("SDK output must be outside source directories")
    marker = "lvgl-pb-linux-sdk.json"
    if output.exists() and any(output.iterdir()) and (not replace or not (output / marker).is_file()):
        raise ValueError("Choose a new output directory, or --replace for a generated Linux SDK")
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = output.parent / ("." + output.name + "-stage-" + uuid.uuid4().hex)
    try:
        plan = make_component(inputs, stage / "embedded", color_depth=color_depth, default_font=default_font)
        linux.copy_platform(repo, stage / "platform", bundle_sdl=bundle_sdl, profile=True)
        config = stage / "embedded/config/lv_conf.h"
        content = config.read_text(encoding="utf-8")
        content = content.rsplit("#endif", 1)[0] + '''#ifdef LVGL_PB_ENABLE_SDL
#undef LV_USE_SDL
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH "SDL3/SDL.h"
#define LV_SDL_DIRECT_EXIT 0
#endif
#endif
'''
        text_file(config, content)
        text_file(stage / "linux-features.json", json.dumps({"runtimeProfile": "embedded/features.json",
            "lvglSourceFileCount": plan["lvglSourceFileCount"], "fullLvglSourceFileCount": plan["fullLvglSourceFileCount"],
            "defaultTransport": "SDL3", "boardCallbackBackend": True,
            "transportConfiguration": "LVGL_PB_ENABLE_SDL overrides only LV_USE_SDL for the optional Linux transport",
            "additionalTransportSources": ["platform/UICreator/Drivers/SDL3/lv_sdl_" + name + ".c" for name in ("window", "keyboard", "mouse", "mousewheel", "sw")]}, indent=2) + "\n")
        text_file(stage / "CMakeLists.txt", '''cmake_minimum_required(VERSION 3.24)
project(LVGLProtobufLinuxProfile LANGUAGES C CXX)
include(CTest)
set(LVGL_PB_PROFILE_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}/embedded")
set(LVGL_PB_TEST_PROJECT "${CMAKE_CURRENT_LIST_DIR}/embedded/projects/''' + plan["projects"][0]["file"] + '''")
add_subdirectory(platform/Runtime/linux)
''')
        text_file(stage / "README.md", "# Project-specific Linux / Raspberry Pi SDK\n\nBuild with `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`, then `cmake --build build`.\nRun `build/platform/Runtime/linux/lvgl-pb-viewer embedded/projects/" + plan["projects"][0]["file"] + "`.\nSee [the integration guide](platform/Runtime/linux/README.md), [the runtime feature/source report](embedded/features.json), and [the Linux transport overlay](linux-features.json).\nUse `-DLVGL_PB_WITH_SDL=OFF` for the board callback backend. For cross compilation, pass `-DCMAKE_TOOLCHAIN_FILE=platform/Runtime/linux/cmake/aarch64-linux-gnu.cmake`.\nRegenerate changed projects using the complete offline source toolkit; this profile contains only its required translation units.\n")
        if publish_directory is not None:
            # Creator supplies its bounded Windows rename retry. Its output is
            # a fresh private directory: never remove a late-arriving collision.
            publish_directory(stage, output)
        else:
            if output.exists(): shutil.rmtree(output)
            stage.rename(output)
        return linux.finish_archive(output, marker)
    finally:
        if stage.exists() and stage.parent == output.parent and stage.name.startswith("." + output.name + "-stage-"):
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("projects", type=Path, nargs="+")
    parser.add_argument("--output", "-o", type=Path, required=True)
    parser.add_argument("--color-depth", type=int, choices=(16, 32), default=32)
    parser.add_argument("--default-font", type=int, default=14)
    parser.add_argument("--replace", action="store_true")
    parser.add_argument("--system-sdl", action="store_true")
    args = parser.parse_args()
    try: print(generate(args.projects, args.output, color_depth=args.color_depth, default_font=args.default_font,
                        replace=args.replace, bundle_sdl=not args.system_sdl))
    except (OSError, ValueError) as error: print("Linux profile: " + str(error), file=sys.stderr); return 1
    return 0

if __name__ == "__main__": sys.exit(main())
