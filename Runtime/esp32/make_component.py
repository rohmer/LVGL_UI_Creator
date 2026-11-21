#!/usr/bin/env python3
"""Create a self-contained, project-specific ESP-IDF component from LVSerial projects."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys
import uuid

from feature_plan import (MAX_INPUT, analyze_projects, decode_project, encode_project,
                          lvgl_config, optimize_document, repository_root, source_plan)


def text_file(path: Path, text: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def copy_file(source: Path, destination: Path):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def cpp_string(value: str) -> str:
    # Restrict generated table names to ASCII JSON string literals (valid C++ too).
    return json.dumps(value, ensure_ascii=True)


def embedded_source(projects: list[dict], embed: bool) -> str:
    lines = ['#include "lvgl_pb_esp32.h"', ""]
    if embed:
        for index, project in enumerate(projects):
            data = project["bytes"]
            lines += [f"static const uint8_t project_{index}[] = {{"]
            for offset in range(0, len(data), 24):
                lines.append("    " + ",".join(str(byte) for byte in data[offset:offset + 24]) + ",")
            lines += ["};", ""]
    lines += ["const lvgl_pb_embedded_project_t lvgl_pb_embedded_projects[] = {"]
    if embed:
        for index, project in enumerate(projects):
            lines.append(f"    {{{cpp_string(project['name'])}, project_{index}, sizeof project_{index}}},")
    lines += ["    {NULL, NULL, 0}", "};", f"const size_t lvgl_pb_embedded_project_count = {len(projects) if embed else 0};", ""]
    return "\n".join(lines)


def component_cmake() -> str:
    return r'''# Generated standalone component. Do not add another LVGL component/library.
cmake_minimum_required(VERSION 3.24)
if(CMAKE_BUILD_EARLY_EXPANSION)
    idf_component_register(REQUIRES esp_timer)
    return()
endif()
if(NOT COMMAND idf_component_register)
    project(LVGLProtobufEmbedded LANGUAGES C CXX)
endif()
if(COMMAND idf_component_register AND PYTHON)
    set(pb_python "${PYTHON}")
else()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(pb_python "${Python3_EXECUTABLE}")
endif()
execute_process(COMMAND "${pb_python}" "${CMAKE_CURRENT_LIST_DIR}/tools/verify_inputs.py"
    "${CMAKE_CURRENT_LIST_DIR}" RESULT_VARIABLE profile_result ERROR_VARIABLE profile_error)
if(NOT profile_result EQUAL 0)
    message(FATAL_ERROR "The project-specific LVGL profile is stale: ${profile_error}")
endif()
file(GLOB profile_projects "${CMAKE_CURRENT_LIST_DIR}/projects/*.pb")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${profile_projects}
    "${CMAKE_CURRENT_LIST_DIR}/features.json")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/project_sources.cmake")
set(pb_includes include config vendor/Runtime/include vendor/Runtime/compat
    vendor/LVSerial vendor/LVSerial/Device vendor/LVSerial/Proto vendor/3rdParty/nanopb
    vendor/LVSerial/Widgets/AdvancedKeyboard/include
    vendor/LVSerial/Widgets/AdvancedKeyboard
    vendor/3rdParty/LVGL/lvgl vendor/3rdParty/LVGL/lvgl/include)
set(pb_sources ${LVGL_PB_PROJECT_SOURCES} src/lvgl_pb_esp32.cpp src/lvgl_pb_projects.c)
if(COMMAND idf_component_register)
    idf_component_register(SRCS ${pb_sources} INCLUDE_DIRS ${pb_includes} REQUIRES esp_timer)
    if(NOT CONFIG_COMPILER_CXX_EXCEPTIONS)
        message(FATAL_ERROR "Enable CONFIG_COMPILER_CXX_EXCEPTIONS=y (see sdkconfig.defaults); the Protobuf loader uses exceptions for safe rollback.")
    endif()
    set(pb_target ${COMPONENT_LIB})
else()
    add_library(lvgl_pb_esp32 STATIC ${pb_sources})
    add_library(LVGLProtobuf::Embedded ALIAS lvgl_pb_esp32)
    set(pb_target lvgl_pb_esp32)
    target_include_directories(${pb_target} PUBLIC ${pb_includes})
endif()
target_compile_features(${pb_target} PUBLIC cxx_std_17)
target_compile_definitions(${pb_target} PUBLIC LV_CONF_INCLUDE_SIMPLE=1 LV_KCONFIG_IGNORE=1
    LVSERIAL_MINIMAL_RUNTIME=1 LVSERIAL_EMBEDDED_RUNTIME=1
    LVGL_RUNTIME_VERSION="9.6" LVGL_RUNTIME_MAJOR=9 LVGL_RUNTIME_MINOR=6)
set_target_properties(${pb_target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(${pb_target} PRIVATE /utf-8 /Zc:preprocessor /Zc:__cplusplus /Gy /Gw)
else()
    # IDF can append its own -std flag after the CMake target standard.
    # Keep this component's required C++17 dialect last in its private flags.
    target_compile_options(${pb_target} PRIVATE -ffunction-sections -fdata-sections
        $<$<COMPILE_LANGUAGE:CXX>:-fexceptions> $<$<COMPILE_LANGUAGE:CXX>:-frtti>
        $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)
    target_link_options(${pb_target} INTERFACE -Wl,--gc-sections)
endif()
'''


def verify_script() -> str:
    return '''"""Reject changed .pb inputs until their feature closure is regenerated."""
import hashlib,json,pathlib,sys
root=pathlib.Path(sys.argv[1]).resolve()
plan=json.loads((root/'features.json').read_text(encoding='utf-8'))
for project in plan['projects']:
    path=root/'projects'/project['file']
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=project['sha256']:
        raise SystemExit('Project '+project['file']+' changed. Re-run Runtime/esp32/make_component.py to regenerate the component.')
'''


def make_component(inputs, output: Path, *, color_depth=16, default_font=14, embed=True,
                   retain_editor_assets=False, replace=False, analyze_only=False, archive=False):
    repo = repository_root()
    projects, documents, names = [], [], set()
    for input_path in inputs:
        path = Path(input_path).resolve(strict=True)
        data = path.read_bytes()
        if not data or len(data) > MAX_INPUT:
            raise ValueError(f"Input must be between 1 byte and {MAX_INPUT} bytes: {path}")
        document = json.loads(data.decode("utf-8-sig")) if path.suffix.lower() == ".json" else decode_project(data, repo)
        optimized = optimize_document(document, retain_editor_assets)
        # Preserve compact version-2 input when no optimization was necessary.
        compiled = data if path.suffix.lower() != ".json" and optimized == document else encode_project(optimized)
        if len(compiled) > MAX_INPUT:
            raise ValueError("Compiled project exceeds the 16 MiB loader limit: " + str(path))
        name = path.stem
        if name.casefold() in names:
            raise ValueError("Project names must be unique: " + name)
        names.add(name.casefold())
        projects.append({"name": name, "source": path.name, "originalBytes": len(data),
                         "file": name + ".pb", "sha256": hashlib.sha256(compiled).hexdigest(),
                         "inputSha256": hashlib.sha256(data).hexdigest(), "bytes": compiled})
        documents.append(optimized)
    plan = analyze_projects(documents, color_depth=color_depth, default_font=default_font)
    sources = source_plan(plan, repo)
    lvgl_sources = [source for source in sources if source.startswith("3rdParty/LVGL/")]
    all_lvgl = list((repo / "3rdParty/LVGL/lvgl/src").rglob("*.c"))
    plan.update({"projects": [{key: value for key, value in project.items() if key != "bytes"} for project in projects],
                 "sourceFiles": sources, "lvglSourceFileCount": len(lvgl_sources), "fullLvglSourceFileCount": len(all_lvgl),
                 "selectedLvglSourceBytes": sum((repo / source).stat().st_size for source in lvgl_sources),
                 "embeddedInputBytes": sum(len(project["bytes"]) for project in projects) if embed else 0,
                 "profilePolicy": "safe feature dependency closure; common LVGL core, software renderer, inherited default theme and one fallback font remain"})
    if analyze_only:
        return plan
    output = output.resolve()
    if output == repo or output in repo.parents:
        raise ValueError("Output must not replace the source repository or one of its parent directories")
    for directory in ("3rdParty", "LVSerial", "ImgLib", "CodeGeneration", "Runtime/common", "Runtime/include", "Runtime/compat"):
        source_directory = repo / directory
        if output == source_directory or source_directory in output.parents:
            raise ValueError("Choose an output outside library source directories, for example build/lvgl_pb_esp32")
    if output.exists() and any(output.iterdir()):
        if not replace or not (output / "lvgl-pb-component.json").is_file():
            raise ValueError("Output is not empty. Use a new folder, or --replace for a previously generated component")
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = output.parent / ("." + output.name + "-stage-" + uuid.uuid4().hex)
    stage.mkdir()
    try:
        # Preserve upstream include topology. Copy all headers, only selected C/C++
        # translation units, and required licenses. No editor binaries/tools ship.
        lvgl = repo / "3rdParty/LVGL/lvgl"
        for directory in (lvgl / "include", lvgl / "src"):
            for path in directory.rglob("*"):
                if path.is_file() and path.suffix.lower() in (".h", ".hpp", ".inc"):
                    copy_file(path, stage / "vendor" / path.relative_to(repo))
        for name in ("lvgl.h", "lv_version.h", "lv_conf_template.h", "LICENCE.txt"):
            if (lvgl / name).is_file(): copy_file(lvgl / name, stage / "vendor/3rdParty/LVGL/lvgl" / name)
        for directory in (repo / "LVSerial", repo / "Runtime/common", repo / "Runtime/include", repo / "Runtime/compat"):
            for path in directory.rglob("*"):
                if path.is_file() and path.suffix.lower() in (".h", ".hpp", ".inc", ".inl"):
                    copy_file(path, stage / "vendor" / path.relative_to(repo))
        custom_directories = {str(Path(source).parent) for source in sources if source.startswith("LVSerial/Widgets/")}
        for directory in custom_directories:
            for path in (repo / directory).rglob("*"):
                if not path.is_file(): continue
                notice = any(marker in path.name.upper() for marker in ("LICENSE", "LICENCE", "COPYING", "NOTICE"))
                if path.suffix.lower() in (".md", ".txt") or notice:
                    copy_file(path, stage / "vendor" / path.relative_to(repo))
                if notice:
                    copy_file(path, stage / "licenses" / path.relative_to(repo))
        for source in sources:
            copy_file(repo / source, stage / "vendor" / source)
        for path in lvgl.rglob("*"):
            if path.is_file() and path.name.upper().startswith(("LICENSE", "LICENCE", "COPYING")):
                copy_file(path, stage / "licenses/LVGL_dependencies" / path.relative_to(lvgl))
        for name in ("3rdParty/JSON/json.hpp", "ImgLib/stb_image.h", "CodeGeneration/Targets/Target.h"):
            copy_file(repo / name, stage / "vendor" / name)
        for path in (repo / "3rdParty/nanopb").glob("*.h"):
            copy_file(path, stage / "vendor/3rdParty/nanopb" / path.name)
        for path in (repo / "3rdParty/nanopb").glob("*LICENSE*"):
            copy_file(path, stage / "licenses/nanopb" / path.name)
        copy_file(repo / "LICENSE", stage / "licenses/LVGL_UI_Creator_LICENSE")
        copy_file(lvgl / "LICENCE.txt", stage / "licenses/LVGL_LICENSE")
        for project in projects:
            destination = stage / "projects" / project["file"]
            destination.parent.mkdir(exist_ok=True)
            destination.write_bytes(project["bytes"])
        own = Path(__file__).parent
        for name in ("include/lvgl_pb_esp32.h", "src/lvgl_pb_esp32.cpp", "README.md", "sdkconfig.defaults"):
            copy_file(own / name, stage / name)
        for name in ("CMakeLists.txt", "sdkconfig.defaults", "main/CMakeLists.txt", "main/Kconfig.projbuild", "main/main.c"):
            copy_file(own / "example" / name, stage / "example" / name)
        text_file(stage / "src/lvgl_pb_projects.c", embedded_source(projects, embed))
        text_file(stage / "config/lv_conf.h", lvgl_config(plan, repo))
        text_file(stage / "CMakeLists.txt", component_cmake())
        text_file(stage / "tools/verify_inputs.py", verify_script())
        text_file(stage / "cmake/project_sources.cmake", "set(LVGL_PB_PROJECT_SOURCES\n" + "".join('    "vendor/' + source + '"\n' for source in sources) + ")\n")
        text_file(stage / "features.json", json.dumps(plan, indent=2, ensure_ascii=False) + "\n")
        text_file(stage / "lvgl-pb-component.json", json.dumps({"generator": "LVGL UI Creator ESP32 component", "formatVersion": 1, "lvgl": "9.6.0"}, indent=2) + "\n")
        text_file(stage / "idf_component.yml", 'description: "Project-specific LVGL Protobuf runtime with bundled LVGL"\nversion: "0.9.0"\ndependencies:\n  idf: ">=5.3"\n')
        backup = None
        if output.exists():
            backup = output.parent / ("." + output.name + "-previous-" + uuid.uuid4().hex)
            output.rename(backup)
        try:
            stage.rename(output)
        except BaseException:
            if backup: backup.rename(output)
            raise
        if backup:
            if backup.resolve().parent != output.parent or not backup.name.startswith("." + output.name + "-previous-"):
                raise ValueError("Generated backup path escaped the intended output parent")
            shutil.rmtree(backup)
        if archive:
            shutil.make_archive(str(output), "zip", output.parent, output.name)
        return plan
    finally:
        if stage.exists():
            if stage.resolve().parent != output.parent or not stage.name.startswith("." + output.name + "-stage-"):
                raise ValueError("Generated staging path escaped the intended output parent")
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("projects", nargs="+", type=Path, help="Saved .pb projects or self-contained editor .json projects")
    parser.add_argument("--output", "-o", type=Path, default=Path("lvgl_pb_esp32"))
    parser.add_argument("--color-depth", type=int, choices=(16, 32), default=16)
    parser.add_argument("--default-font", type=int, choices=(8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48), default=14)
    parser.add_argument("--no-embed", action="store_true", help="Load files through ESP-IDF VFS instead of retaining flash arrays")
    parser.add_argument("--retain-editor-assets", action="store_true", help="Keep redundant original/preview data when device frames exist")
    parser.add_argument("--replace", action="store_true", help="Replace a previously generated component, discarding edits inside it")
    parser.add_argument("--analyze-only", action="store_true", help="Print the feature/source plan without copying sources")
    parser.add_argument("--zip", action="store_true", help="Also create a self-contained source ZIP beside the output folder")
    arguments = parser.parse_args()
    try:
        plan = make_component(arguments.projects, arguments.output, color_depth=arguments.color_depth,
                              default_font=arguments.default_font, embed=not arguments.no_embed,
                              retain_editor_assets=arguments.retain_editor_assets, replace=arguments.replace,
                              analyze_only=arguments.analyze_only, archive=arguments.zip)
        if arguments.analyze_only:
            print(json.dumps(plan, indent=2, ensure_ascii=False))
        else:
            print(f"Created {arguments.output.resolve()}")
            print(f"LVGL sources: {plan['lvglSourceFileCount']} / {plan['fullLvglSourceFileCount']}; widgets: {', '.join(plan['widgetSections']) or 'core objects'}")
            print(f"Embedded Protobuf bytes: {plan['embeddedInputBytes']}; details: features.json")
    except (OSError, ValueError, KeyError, TypeError, struct.error, RecursionError) as error:
        print("Component generation failed: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
