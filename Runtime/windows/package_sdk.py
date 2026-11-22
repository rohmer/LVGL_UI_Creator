#!/usr/bin/env python3
"""Package the Windows Protobuf display DLL as a standalone C SDK."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import sys
import uuid
import zipfile


def machine(path):
    with Path(path).open("rb") as stream:
        if stream.read(2) != b"MZ": raise ValueError("Not a Windows executable: " + str(path))
        stream.seek(0x3c)
        offset = struct.unpack("<I", stream.read(4))[0]
        stream.seek(offset)
        if stream.read(4) != b"PE\0\0": raise ValueError("Invalid PE signature: " + str(path))
        value = struct.unpack("<H", stream.read(2))[0]
    architectures = {0x14c: "x86", 0x8664: "x64", 0xaa64: "arm64"}
    if value not in architectures: raise ValueError("Unsupported SDK architecture: " + hex(value))
    return architectures[value]


def runtime_directory(compiler, architecture):
    compiler = Path(compiler).resolve(strict=True)
    vc = next((parent for parent in compiler.parents if parent.name.lower() == "vc"), None)
    if not vc: raise ValueError("--compiler must identify an installed MSVC compiler under VC/Tools/MSVC")
    versions = vc / "Redist/MSVC"
    candidates = list(versions.glob("*/" + architecture + "/Microsoft.VC*.CRT"))
    exact_version = next((parent.name for parent in compiler.parents if parent.parent.name.lower() == "msvc"), "")
    def key(path):
        version = path.parents[1].name
        return (version == exact_version, tuple(int(part) for part in re.findall(r"\d+", version)))
    for path in sorted(candidates, key=key, reverse=True):
        if (path / "msvcp140.dll").is_file() and (path / "vcruntime140.dll").is_file(): return path
    raise ValueError("Matching MSVC redistributable runtime was not found for " + architecture)


def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8", newline="\n")


def cmake_config(dll_name, library_name, architecture):
    bits = 4 if architecture == "x86" else 8
    return f'''# Standalone prebuilt Windows C ABI; no caller-side LVGL/SDL headers needed.
cmake_minimum_required(VERSION 3.24)
if(NOT WIN32)
    message(FATAL_ERROR "This SDK contains Windows binaries")
endif()
if(CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL {bits})
    message(FATAL_ERROR "This SDK is built for {architecture}; choose a matching compiler platform")
endif()
get_filename_component(lvgl_pb_sdk_root "${{CMAKE_CURRENT_LIST_DIR}}/.." ABSOLUTE)
if(NOT TARGET LVGLProtobuf::Display)
    add_library(LVGLProtobuf::Display SHARED IMPORTED GLOBAL)
    set_target_properties(LVGLProtobuf::Display PROPERTIES
        IMPORTED_LOCATION "${{lvgl_pb_sdk_root}}/bin/{dll_name}"
        IMPORTED_IMPLIB "${{lvgl_pb_sdk_root}}/lib/{library_name}"
        INTERFACE_INCLUDE_DIRECTORIES "${{lvgl_pb_sdk_root}}/include")
endif()
function(lvgl_pb_copy_runtime target)
    get_filename_component(sdk_root "${{CMAKE_CURRENT_FUNCTION_LIST_DIR}}/.." ABSOLUTE)
    file(GLOB runtime_dlls "${{sdk_root}}/bin/*.dll")
    add_custom_command(TARGET ${{target}} POST_BUILD
        COMMAND "${{CMAKE_COMMAND}}" -E copy_if_different ${{runtime_dlls}} "$<TARGET_FILE_DIR:${{target}}>"
        VERBATIM)
endfunction()
'''


def package(arguments):
    own = Path(__file__).resolve().parent
    repo = own.parents[1]
    dll = arguments.dll.resolve(strict=True)
    library = arguments.import_library.resolve(strict=True)
    sdl = arguments.sdl.resolve(strict=True)
    architecture = machine(dll)
    if machine(sdl) != architecture: raise ValueError("SDL3 and the display DLL must have the same architecture")
    viewer = arguments.viewer.resolve(strict=True) if arguments.viewer else None
    if viewer and machine(viewer) != architecture: raise ValueError("Viewer and display DLL architectures differ")
    crt = runtime_directory(arguments.compiler, architecture)
    output = arguments.output.resolve()
    if output == repo or output in repo.parents: raise ValueError("SDK output must not replace the source repository")
    if output.exists() and any(output.iterdir()) and (not arguments.replace or not (output / "sdk.json").is_file()):
        raise ValueError("SDK output is not empty; use a new folder or --replace for a previously generated SDK")
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = output.parent / ("." + output.name + "-stage-" + uuid.uuid4().hex)
    stage.mkdir()
    try:
        copy(dll, stage / "bin" / dll.name)
        copy(sdl, stage / "bin" / sdl.name)
        copy(library, stage / "lib" / library.name)
        if viewer: copy(viewer, stage / "bin" / viewer.name)
        for runtime in crt.glob("*.dll"): copy(runtime, stage / "bin" / runtime.name)
        copy(own / "include/lvgl_pb_display.h", stage / "include/lvgl_pb_display.h")
        copy(own / "example.c", stage / "examples/example.c")
        copy(own / "README.md", stage / "README.md")
        config = cmake_config(dll.name, library.name, architecture)
        write(stage / "cmake/LVGLProtobufDisplayConfig.cmake", config)
        write(stage / "CMakeLists.txt", '''cmake_minimum_required(VERSION 3.24)
project(LVGLProtobufDisplaySDK LANGUAGES C)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/LVGLProtobufDisplayConfig.cmake")
add_executable(LVGLProtobufExample examples/example.c)
set_target_properties(LVGLProtobufExample PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_link_libraries(LVGLProtobufExample PRIVATE LVGLProtobuf::Display)
lvgl_pb_copy_runtime(LVGLProtobufExample)
''')
        for source, destination in (
            (repo / "LICENSE", "LVGL_UI_Creator_LICENSE"),
            (repo / "3rdParty/LVGL/lvgl/LICENCE.txt", "LVGL_LICENSE"),
            (repo / "3rdParty/SDL3/LICENSE.txt", "SDL3_LICENSE"),
            (repo / "3rdParty/nanopb/LICENSE.txt", "nanopb_LICENSE"),
            (repo / "3rdParty/JSON/json.hpp", "nlohmann_json.hpp"),
            (repo / "ImgLib/stb_image.h", "stb_image.h")):
            copy(source, stage / "licenses" / destination)
        lvgl = repo / "3rdParty/LVGL/lvgl"
        for notice in lvgl.rglob("*"):
            if notice.is_file() and notice.name.upper().startswith(("LICENSE", "LICENCE", "COPYING")):
                copy(notice, stage / "licenses/LVGL_dependencies" / notice.relative_to(lvgl))
        widgets = repo / "LVSerial/Widgets"
        for notice in widgets.rglob("*"):
            if notice.is_file() and (notice.name == "PROVENANCE.md" or any(marker in notice.name.upper() for marker in ("LICENSE", "LICENCE", "COPYING", "NOTICE"))):
                copy(notice, stage / "licenses/LVSerial_widgets" / notice.relative_to(widgets))
        write(stage / "licenses/Microsoft_RUNTIME_NOTICE.txt", "Microsoft Visual C++ runtime binaries retain their vendor redistribution terms.\n")
        manifest = {"sdkVersion": "0.9.1", "api": "lvgl_pb_display.h", "lvglVersion": "9.6.0",
                    "architecture": architecture, "displayDll": dll.name, "importLibrary": library.name,
                    "sdlDll": sdl.name, "viewer": viewer.name if viewer else None,
                    "files": {path.relative_to(stage).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                              for path in sorted(stage.rglob("*")) if path.is_file()}}
        write(stage / "sdk.json", json.dumps(manifest, indent=2) + "\n")
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
                raise ValueError("SDK backup path escaped the intended output parent")
            shutil.rmtree(backup)
        archive = Path(shutil.make_archive(str(output), "zip", output.parent, output.name))
        with zipfile.ZipFile(archive) as zipped:
            bad_file = zipped.testzip()
            if bad_file: raise ValueError("SDK archive failed integrity check: " + bad_file)
        write(archive.with_suffix(archive.suffix + ".sha256"), hashlib.sha256(archive.read_bytes()).hexdigest() + "  " + archive.name + "\n")
        return architecture, archive
    finally:
        if stage.exists():
            if stage.resolve().parent != output.parent or not stage.name.startswith("." + output.name + "-stage-"):
                raise ValueError("SDK staging path escaped the intended output parent")
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--import-library", type=Path, required=True)
    parser.add_argument("--sdl", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--viewer", type=Path)
    parser.add_argument("--output", "-o", type=Path, required=True)
    parser.add_argument("--replace", action="store_true")
    args = parser.parse_args()
    try:
        architecture, archive = package(args)
        print(f"Created {architecture} Windows SDK: {archive}")
    except (OSError, ValueError, struct.error) as error:
        print("SDK packaging failed: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__": sys.exit(main())
