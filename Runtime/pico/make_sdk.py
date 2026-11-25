#!/usr/bin/env python3
"""Generate a project-specific Pico/Pico 2 SDK project and display library."""
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
from feature_plan import decode_project, repository_root
from make_component import make_component, text_file

BOARDS = {"pico": {"name": "Raspberry Pi Pico", "platform": "rp2040", "ramBytes": 264 * 1024, "flashBytes": 2 * 1024 * 1024},
          "pico2": {"name": "Raspberry Pi Pico 2", "platform": "rp2350-arm-s", "ramBytes": 520 * 1024, "flashBytes": 4 * 1024 * 1024}}
MARKER = "lvgl-pb-pico-sdk.json"


def update_profile_manifest(root: Path):
    root = Path(root)
    marker = root / MARKER
    metadata = json.loads(marker.read_text(encoding="utf-8")) if marker.exists() else {}
    metadata["files"] = {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in sorted(root.rglob("*")) if path.is_file() and path.name != MARKER}
    text_file(marker, json.dumps(metadata, indent=2) + "\n")


def project_cmake(board: str, c_project: bool):
    return f'''cmake_minimum_required(VERSION 3.24)
set(PICO_BOARD "{board}" CACHE STRING "Pico board: pico or pico2")
if(PICO_BOARD STREQUAL "pico")
    set(pico_expected_platform "rp2040")
    set(pico_sram_bytes 270336)
elseif(PICO_BOARD STREQUAL "pico2")
    set(pico_expected_platform "rp2350-arm-s")
    set(pico_sram_bytes 532480)
else()
    message(FATAL_ERROR "This project supports PICO_BOARD=pico or pico2")
endif()
if(DEFINED PICO_PLATFORM AND NOT PICO_PLATFORM STREQUAL pico_expected_platform)
    message(FATAL_ERROR "PICO_PLATFORM must match the selected board's Arm target; configure a fresh build directory")
endif()
set(PICO_PLATFORM "${{pico_expected_platform}}" CACHE STRING "Pico SDK Arm platform")
set(PICO_CXX_ENABLE_EXCEPTIONS 1 CACHE BOOL "Runtime C++ exception support")
set(PICO_CXX_ENABLE_RTTI 1 CACHE BOOL "Runtime C++ type information")
set(PICO_STACK_SIZE 16384 CACHE STRING "LVGL owner stack bytes")
if(NOT PICO_STACK_SIZE MATCHES "^[1-9][0-9]*$" OR PICO_STACK_SIZE LESS 4096 OR PICO_STACK_SIZE GREATER 65536)
    message(FATAL_ERROR "PICO_STACK_SIZE must be from 4096 to 65536 bytes")
endif()
math(EXPR pico_stack_remainder "${{PICO_STACK_SIZE}} % 8")
if(NOT pico_stack_remainder EQUAL 0)
    message(FATAL_ERROR "PICO_STACK_SIZE must be a multiple of 8 bytes")
endif()
if(NOT PICO_CXX_ENABLE_EXCEPTIONS OR NOT PICO_CXX_ENABLE_RTTI)
    message(FATAL_ERROR "The portable runtime/keyboard requires C++ exceptions and RTTI")
endif()
if(NOT PICO_SDK_PATH AND DEFINED ENV{{PICO_SDK_PATH}})
    set(PICO_SDK_PATH "$ENV{{PICO_SDK_PATH}}")
endif()
if(NOT EXISTS "${{PICO_SDK_PATH}}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR "Set PICO_SDK_PATH to an installed Pico SDK 2.3.1 or newer checkout")
endif()
include("${{PICO_SDK_PATH}}/external/pico_sdk_import.cmake")
project(LVGLPicoDisplay VERSION 0.9.2 LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_definitions(PICO_STACK_SIZE=${{PICO_STACK_SIZE}} PICO_CORE1_STACK_SIZE=4096)
pico_sdk_init()
if(PICO_SDK_VERSION_STRING VERSION_LESS "2.3.1")
    message(FATAL_ERROR "Pico SDK 2.3.1 or newer is required for the explicit stack/heap linker layout")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${{Python3_EXECUTABLE}}" "${{CMAKE_CURRENT_SOURCE_DIR}}/embedded/tools/verify_inputs.py"
    "${{CMAKE_CURRENT_SOURCE_DIR}}/embedded" RESULT_VARIABLE profile_result ERROR_VARIABLE profile_error)
if(NOT profile_result EQUAL 0)
    message(FATAL_ERROR "The Pico source profile is stale: ${{profile_error}}")
endif()
file(GLOB profile_projects "${{CMAKE_CURRENT_SOURCE_DIR}}/embedded/projects/*.pb")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${{profile_projects}}
    "${{CMAKE_CURRENT_SOURCE_DIR}}/embedded/features.json")
add_subdirectory(embedded)
{'add_subdirectory(ui generated_ui)' if c_project else ''}
set(LVGL_PICO_BUFFER_LINES 8 CACHE STRING "Partial draw-buffer rows")
if(NOT LVGL_PICO_BUFFER_LINES MATCHES "^[1-9][0-9]*$" OR LVGL_PICO_BUFFER_LINES GREATER 64)
    message(FATAL_ERROR "LVGL_PICO_BUFFER_LINES must be from 1 to 64")
endif()
add_executable(lvgl_pico_display example/main.c)
target_link_libraries(lvgl_pico_display PRIVATE {'generated_ui' if c_project else 'LVGLProtobuf::Pico'} pico_stdlib)
target_compile_definitions(lvgl_pico_display PRIVATE LVGL_PICO_BUFFER_LINES=${{LVGL_PICO_BUFFER_LINES}} PICO_MALLOC_PANIC=0)
# The SDK's default SCRATCH_Y stack region is only 4 KiB. Reserve the chosen
# owner stack explicitly at the top of SRAM, keep 4 KiB for core 1/scratch X,
# and cap the C heap below both regions instead of letting them overlap.
math(EXPR pico_main_ram "${{pico_sram_bytes}} - 4096 - ${{PICO_STACK_SIZE}}")
math(EXPR pico_scratch_x "0x20000000 + ${{pico_main_ram}}" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR pico_scratch_y "0x20000000 + ${{pico_main_ram}} + 4096" OUTPUT_FORMAT HEXADECIMAL)
pico_set_linker_script_var(lvgl_pico_display RAM_LENGTH ${{pico_main_ram}})
pico_set_linker_script_var(lvgl_pico_display SCRATCH_X_ORIGIN ${{pico_scratch_x}})
pico_set_linker_script_var(lvgl_pico_display SCRATCH_X_LENGTH 4096)
pico_set_linker_script_var(lvgl_pico_display SCRATCH_Y_ORIGIN ${{pico_scratch_y}})
pico_set_linker_script_var(lvgl_pico_display SCRATCH_Y_LENGTH ${{PICO_STACK_SIZE}})
option(LVGL_PICO_USB_STDIO "Use USB CDC logging (requires TinyUSB SDK submodule)" OFF)
pico_enable_stdio_usb(lvgl_pico_display ${{LVGL_PICO_USB_STDIO}})
pico_enable_stdio_uart(lvgl_pico_display 1)
pico_set_program_name(lvgl_pico_display "LVGL UI Creator Pico display")
pico_set_program_version(lvgl_pico_display "0.9.2")
pico_add_extra_outputs(lvgl_pico_display)
'''


def component_cmake(c_project: bool, enable_xml=False):
    prefix = '''include("${CMAKE_CURRENT_LIST_DIR}/cmake/project_sources.cmake")
set(pico_includes "${CMAKE_CURRENT_LIST_DIR}/../include" config
    vendor/Runtime/include vendor/Runtime/compat vendor/LVSerial vendor/LVSerial/Device
    vendor/LVSerial/Proto vendor/3rdParty/nanopb vendor/3rdParty/AdvancedKeyboard/include
    vendor/3rdParty/AdvancedKeyboard/src vendor/3rdParty/LVGL/lvgl vendor/3rdParty/LVGL/lvgl/include)
'''
    if c_project:
        prefix += '''list(FILTER LVGL_PB_PROJECT_SOURCES INCLUDE REGEX "vendor/3rdParty/LVGL/")
add_library(lvgl_pico_core STATIC ${LVGL_PB_PROJECT_SOURCES} ../src/lvgl_pico.c)
add_library(lvgl::lvgl ALIAS lvgl_pico_core)
set(pico_target lvgl_pico_core)
'''
    else:
        prefix += '''option(LVGL_PB_ENABLE_XML "Enable the LVGL UI Creator XML input codec" @XML_DEFAULT@)
if(LVGL_PB_ENABLE_XML)
    list(APPEND LVGL_PB_PROJECT_SOURCES optional/xml/Xml.cpp)
endif()
add_library(lvgl_pb_pico STATIC ${LVGL_PB_PROJECT_SOURCES}
    ../src/lvgl_pico.c ../src/lvgl_pb_pico.cpp ../src/lvgl_pb_projects.c)
add_library(LVGLProtobuf::Pico ALIAS lvgl_pb_pico)
add_library(lvgl::lvgl ALIAS lvgl_pb_pico)
set(pico_target lvgl_pb_pico)
target_compile_definitions(lvgl_pb_pico PUBLIC LVSERIAL_USE_XML=$<BOOL:${LVGL_PB_ENABLE_XML}>)
target_include_directories(lvgl_pb_pico PRIVATE vendor/LVSerial/Serialization)
'''.replace("@XML_DEFAULT@", "ON" if enable_xml else "OFF")
    return prefix + '''target_include_directories(${pico_target} PUBLIC ${pico_includes})
target_compile_definitions(${pico_target} PUBLIC LV_CONF_INCLUDE_SIMPLE=1 LV_KCONFIG_IGNORE=1
    LVSERIAL_MINIMAL_RUNTIME=1 LVSERIAL_EMBEDDED_RUNTIME=1
    LVGL_RUNTIME_VERSION="9.6" LVGL_RUNTIME_MAJOR=9 LVGL_RUNTIME_MINOR=6)
target_compile_features(${pico_target} PUBLIC c_std_11 cxx_std_17)
target_compile_options(${pico_target} PRIVATE -ffunction-sections -fdata-sections
    $<$<COMPILE_LANGUAGE:CXX>:-fexceptions> $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
target_link_options(${pico_target} INTERFACE -Wl,--gc-sections)
target_link_libraries(${pico_target} PUBLIC pico_stdlib m)
'''


def _remove_tree(path: Path, stage: Path):
    if path.exists():
        if path.resolve() == stage.resolve() or not path.resolve().is_relative_to(stage.resolve()):
            raise ValueError("Generated cleanup escaped staging")
        shutil.rmtree(path)


def generate(inputs, output: Path, *, board="pico", generated_c=None, color_depth=16,
             default_font=14, replace=False, archive=True, copy_ui=True, enable_xml=False):
    if board not in BOARDS: raise ValueError("Board must be pico or pico2")
    repo = repository_root()
    output = Path(output).resolve()
    if output == repo or output in repo.parents or any(output == repo / name or (repo / name) in output.parents
        for name in ("Runtime", "LVSerial", "3rdParty", "UICreator", "ImgLib", "CodeGeneration")):
        raise ValueError("Pico output must be outside source directories")
    if output.exists() and any(output.iterdir()) and (not replace or not (output / MARKER).is_file()):
        raise ValueError("Choose a new output directory or --replace for a generated Pico SDK")
    c_project = generated_c is not None
    if c_project and enable_xml:
        raise ValueError("XML loading requires the Pico runtime project, not direct C output")
    if c_project:
        generated_c = Path(generated_c).resolve(strict=True)
        if output == generated_c or generated_c in output.parents:
            raise ValueError("Pico output must not be inside its generated C input directory")
        if not all((generated_c / name).is_file() and not (generated_c / name).is_symlink()
                   for name in ("ui.c", "ui.h", "ui_assets.c", "CMakeLists.txt")):
            raise ValueError("C mode requires an editor-generated C project directory")
        if any(path.is_symlink() for path in generated_c.rglob("*")):
            raise ValueError("Generated C input must not contain symbolic links")
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = output.parent / ("." + output.name + "-stage-" + uuid.uuid4().hex)
    stage.mkdir()
    try:
        embedded = stage / "embedded"
        plan = make_component(inputs, embedded, color_depth=color_depth, default_font=default_font, enable_xml=enable_xml)
        first = plan["projects"][0]
        document = decode_project((embedded / "projects" / first["file"]).read_bytes(), repo)
        screens = document.get("screens", [document])
        initial = document.get("activeScreen", 0)
        if type(initial) is not int or not 0 <= initial < len(screens): raise ValueError("Invalid active screen")
        canvas = screens[initial].get("canvas", screens[initial].get("screen", {}))
        width, height = canvas.get("width", 320), canvas.get("height", 240)
        if any(type(value) is not int or not 1 <= value <= 8192 for value in (width, height)):
            raise ValueError("Pico display dimensions must be integers from 1 to 8192")
        for name in ("example",): _remove_tree(embedded / name, stage)
        for name in ("include/lvgl_pb_esp32.h", "src/lvgl_pb_esp32.cpp", "src/lvgl_pb_projects.c",
                     "sdkconfig.defaults", "Kconfig", "idf_component.yml", "lvgl-pb-component.json", "README.md"):
            (embedded / name).unlink(missing_ok=True)
        for folder in ("include", "src", "example"):
            shutil.copytree(OWN / folder, stage / folder)
        if c_project:
            (stage / "include/lvgl_pb_pico.h").unlink()
            (stage / "src/lvgl_pb_pico.cpp").unlink()
            if copy_ui: shutil.copytree(generated_c, stage / "ui")
        else:
            # Use the same validated binary payload table, with Pico's C header.
            from make_component import embedded_source
            projects = [{"name": item["name"], "bytes": (embedded / "projects" / item["file"]).read_bytes()}
                        for item in plan["projects"]]
            text_file(stage / "src/lvgl_pb_projects.c", embedded_source(projects, True).replace('"lvgl_pb_esp32.h"', '"lvgl_pb_pico.h"'))
        text_file(stage / "CMakeLists.txt", project_cmake(board, c_project))
        text_file(embedded / "CMakeLists.txt", component_cmake(c_project, enable_xml))
        verify = (embedded / "tools/verify_inputs.py").read_text(encoding="utf-8")
        text_file(embedded / "tools/verify_inputs.py", verify.replace("Runtime/esp32/make_component.py", "Runtime/pico/make_sdk.py"))
        text_file(stage / "include/lvgl_pico_project_config.h", "#pragma once\n" +
                  f"#define LVGL_PICO_WIDTH {width}\n#define LVGL_PICO_HEIGHT {height}\n" +
                  f"#define LVGL_PICO_C_PROJECT {int(c_project)}\n#define LVGL_PICO_PROJECT_NAME {json.dumps(first['name'])}\n" +
                  "#ifndef LVGL_PICO_BUFFER_LINES\n#define LVGL_PICO_BUFFER_LINES 8\n#endif\n" +
                  f"#define LVGL_PICO_DRAW_BUFFER_BYTES ((((LVGL_PICO_WIDTH * {color_depth // 8}u) + 3u) & ~3u) * LVGL_PICO_BUFFER_LINES)\n")
        plan.update(platform="Raspberry Pi Pico SDK", board=board, picoPlatform=BOARDS[board]["platform"],
                    outputFormat="c" if c_project else "protobuf", boardMemory=BOARDS[board],
                    drawBufferDefaultBytes=((width * (color_depth // 8) + 3) & ~3) * 8,
                    defaultStackBytes=16384)
        if c_project:
            plan["sourceFiles"] = [source for source in plan["sourceFiles"] if source.startswith("3rdParty/LVGL/")]
            # These vendors are not compiled or needed by the pure C profile.
            for name in ("Runtime", "LVSerial", "3rdParty/JSON", "3rdParty/nanopb", "ImgLib", "CodeGeneration"):
                _remove_tree(embedded / "vendor" / name, stage)
            _remove_tree(embedded / "optional", stage)
            plan["optionalSources"] = {}
        text_file(embedded / "features.json", json.dumps(plan, indent=2) + "\n")
        text_file(embedded / "cmake/project_sources.cmake", "set(LVGL_PB_PROJECT_SOURCES\n" +
                  "".join(f'    "vendor/{source}"\n' for source in plan["sourceFiles"]) + ")\n")
        shutil.copy2(OWN / "README.md", stage / "README.md")
        text_file(stage / MARKER, json.dumps({"formatVersion": 1, "generator": "LVGL UI Creator Pico SDK",
                  "lvglVersion": "9.6.0", "board": board, "platform": BOARDS[board]["platform"],
                  "outputFormat": "c" if c_project else "protobuf"}, indent=2) + "\n")
        update_profile_manifest(stage)
        backup = None
        if output.exists():
            backup = output.parent / ("." + output.name + "-previous-" + uuid.uuid4().hex)
            output.rename(backup)
        try: stage.rename(output)
        except BaseException:
            if backup: backup.rename(output)
            raise
        if backup: _remove_tree(backup, output.parent)
        if archive:
            zipped = Path(shutil.make_archive(str(output), "zip", output.parent, output.name))
            with zipfile.ZipFile(zipped) as check:
                if check.testzip(): raise ValueError("Pico SDK ZIP failed its integrity check")
            text_file(Path(str(zipped) + ".sha256"), hashlib.sha256(zipped.read_bytes()).hexdigest() + "  " + zipped.name + "\n")
            return zipped
        return output
    finally:
        if stage.exists(): _remove_tree(stage, output.parent)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("projects", type=Path, nargs="+")
    parser.add_argument("--output", "-o", type=Path, required=True)
    parser.add_argument("--board", choices=tuple(BOARDS), default="pico")
    parser.add_argument("--generated-c", type=Path, help="Use editor-exported C instead of the Protobuf runtime")
    parser.add_argument("--color-depth", type=int, choices=(16, 32), default=16)
    parser.add_argument("--default-font", type=int, default=14)
    parser.add_argument("--replace", action="store_true")
    parser.add_argument("--zip", action="store_true")
    parser.add_argument("--enable-xml", action="store_true", help="Enable the optional LVGL UI Creator XML input codec")
    args = parser.parse_args()
    try:
        print(generate(args.projects, args.output, board=args.board, generated_c=args.generated_c,
                       color_depth=args.color_depth, default_font=args.default_font, replace=args.replace,
                       archive=args.zip, enable_xml=args.enable_xml))
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("Pico project generation failed: " + str(error), file=sys.stderr); return 1
    return 0


if __name__ == "__main__": sys.exit(main())
