#!/usr/bin/env python3
"""Generate an Arduino Nano ESP32 library with a project-specific LVGL closure."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

ESP_TOOLS = Path(__file__).resolve().parents[1] / "esp32"
sys.path.insert(0, str(ESP_TOOLS))
from make_component import make_component, rename_generated_directory, text_file
from feature_plan import decode_project, repository_root


def forwarding_headers(source: Path, destination: Path):
    for header in source.rglob("*"):
        if not header.is_file() or header.suffix.lower() not in (".h", ".hpp"): continue
        target = destination / header.relative_to(source)
        if target.exists(): continue
        relative = os.path.relpath(header, target.parent).replace("\\", "/")
        text_file(target, '#pragma once\n#include "' + relative + '"\n')


def make_library(inputs, output: Path, *, color_depth=16, default_font=14,
                 embed=True, retain_editor_assets=False, replace=False, archive=False, enable_xml=False):
    repo = repository_root()
    if default_font not in range(8, 49, 2):
        raise ValueError("Default font must be an available even Montserrat size from 8 to 48")
    output = output.resolve()
    if output == repo or output in repo.parents:
        raise ValueError("Library output must not replace its repository")
    for name in ("Runtime", "LVSerial", "3rdParty", "ImgLib", "CodeGeneration", "UICreator"):
        source = repo / name
        if output == source or source in output.parents:
            raise ValueError("Choose an output outside source directories, such as build/LVGLProtobuf")
    marker = output / "lvgl-pb-arduino.json"
    if output.exists() and any(output.iterdir()) and (not replace or not marker.is_file()):
        raise ValueError("Output is not empty; use a new folder or --replace for a generated Arduino library")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".lvgl-arduino-", dir=output.parent) as temporary:
        stage = Path(temporary).resolve()
        if stage.parent != output.parent or not stage.name.startswith(".lvgl-arduino-"):
            raise ValueError("Staging escaped the intended output parent")
        component = stage / "component"
        plan = make_component(inputs, component, color_depth=color_depth, default_font=default_font,
                              embed=embed, retain_editor_assets=retain_editor_assets, enable_xml=enable_xml)
        library = stage / "library"
        src = library / "src"
        src.mkdir(parents=True)
        (component / "vendor").rename(src / "vendor")
        if enable_xml:
            shutil.copy2(component / "optional/xml/Xml.cpp", src / "vendor/LVSerial/Serialization/Xml.cpp")
        shutil.copytree(component / "projects", library / "projects")
        shutil.copytree(component / "licenses", library / "licenses")
        shutil.copytree(Path(__file__).parent / "examples", library / "examples")
        shutil.copy2(Path(__file__).parent / "README.md", library / "README.md")
        shutil.copy2(Path(__file__).parent / "include/lvgl_pb_arduino.h", src / "lvgl_pb_arduino.h")
        shutil.copy2(Path(__file__).parent / "src/lvgl_pb_arduino.cpp", src / "lvgl_pb_arduino.cpp")
        projects = (component / "src/lvgl_pb_projects.c").read_text(encoding="utf-8").replace('"lvgl_pb_esp32.h"', '"lvgl_pb_arduino.h"')
        text_file(src / "lvgl_pb_projects.c", projects)
        config = (component / "config/lv_conf.h").read_text(encoding="utf-8")
        config = config.replace("#define LV_CONF_H", "#define LV_CONF_H\n#define LVSERIAL_EMBEDDED_RUNTIME 1\n#define LVGL_RUNTIME_MAJOR 9\n#define LVGL_RUNTIME_MINOR 6")
        config += "\n#ifndef LVSERIAL_USE_XML\n#define LVSERIAL_USE_XML " + str(int(enable_xml)) + "\n#endif\n"
        text_file(src / "lv_conf.h", config)
        text_file(src / "vendor/3rdParty/LVGL/lv_conf.h", config)
        include_roots = [src / "vendor" / name for name in (
            "Runtime/include", "Runtime/compat", "LVSerial", "LVSerial/Device", "LVSerial/Proto", "3rdParty/nanopb",
            "3rdParty/AdvancedKeyboard/include", "3rdParty/AdvancedKeyboard/src",
            "3rdParty/LVGL/lvgl", "3rdParty/LVGL/lvgl/include")]
        for directory in include_roots: forwarding_headers(directory, src)
        text_file(src / "lvgl/lvgl.h", '#pragma once\n#include "../vendor/3rdParty/LVGL/lvgl/lvgl.h"\n')
        text_file(src / "LVGLProtobuf.h", '#pragma once\n#include "lvgl_pb_arduino.h"\n')
        first_project = plan["projects"][0]["file"]
        document = decode_project((library / "projects" / first_project).read_bytes(), repo)
        canvas = document.get("canvas", {})
        width = canvas.get("width", 320); height = canvas.get("height", 240)
        if any(type(value) is not int or not 0 < value <= 65535 for value in (width, height)):
            raise ValueError("The example display requires integer canvas dimensions from 1 to 65535")
        text_file(src / "lvgl_pb_project_config.h", "#pragma once\n" +
                  f"#define LVGL_PB_DEFAULT_WIDTH {width}\n#define LVGL_PB_DEFAULT_HEIGHT {height}\n" +
                  f"#define LVGL_PB_COLOR_DEPTH {color_depth}\n")
        text_file(library / "library.properties", '''name=LVGLProtobuf
version=0.9.4
author=LVGL UI Creator contributors
maintainer=LVGL UI Creator contributors
sentence=Project-specific LVGL 9.6 Protobuf UI runtime for Arduino Nano ESP32.
paragraph=Bundles the required LVGL, serializers, assets, advanced keyboard and optional prediction modules selected from saved projects.
category=Display
url=https://github.com/rohmer/LVGL_UI_Creator
architectures=esp32
includes=lvgl_pb_arduino.h
dot_a_linkage=true
''')
        text_file(library / "features.json", json.dumps(plan, indent=2, ensure_ascii=False) + "\n")
        text_file(library / "lvgl-pb-arduino.json", json.dumps({"formatVersion": 1, "generator": "LVGL UI Creator Arduino library",
                  "board": "arduino:esp32:nano_nora", "lvgl": "9.6.0",
                  "files": {path.relative_to(library).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                            for path in sorted(library.rglob("*")) if path.is_file()}}, indent=2) + "\n")
        if output.exists():
            # A previously generated marker and explicit --replace authorize the
            # exact checked output. Never enumerate source paths for deletion.
            if output.resolve() != output or output.parent != stage.parent:
                raise ValueError("Output escaped the intended target directory")
            shutil.rmtree(output)
        rename_generated_directory(library, output)
    if archive:
        archived = Path(shutil.make_archive(str(output), "zip", output.parent, output.name))
        text_file(archived.with_suffix(".zip.sha256"), hashlib.sha256(archived.read_bytes()).hexdigest()+"  "+archived.name+"\n")
    return plan


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("projects", nargs="+", type=Path)
    parser.add_argument("--output", "-o", type=Path, default=Path("LVGLProtobuf"))
    parser.add_argument("--color-depth", type=int, choices=(16, 32), default=16)
    parser.add_argument("--default-font", type=int, choices=range(8, 49, 2), default=14)
    parser.add_argument("--no-embed", action="store_true")
    parser.add_argument("--enable-xml", action="store_true", help="Compile the optional LVGL UI Creator XML input codec")
    parser.add_argument("--retain-editor-assets", action="store_true")
    parser.add_argument("--replace", action="store_true")
    parser.add_argument("--zip", action="store_true")
    args = parser.parse_args()
    try:
        plan = make_library(args.projects, args.output, color_depth=args.color_depth, default_font=args.default_font,
                            embed=not args.no_embed, retain_editor_assets=args.retain_editor_assets,
                            replace=args.replace, archive=args.zip, enable_xml=args.enable_xml)
        print(f"Created {args.output.resolve()}; {plan['lvglSourceFileCount']} LVGL sources, {plan['embeddedInputBytes']} Protobuf bytes")
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("Arduino library generation failed: " + str(error), file=sys.stderr); return 1
    return 0

if __name__ == "__main__": sys.exit(main())
