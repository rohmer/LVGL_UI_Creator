#!/usr/bin/env python3
"""Package an actually built Linux shared-library C SDK and optional viewer."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile


def package(build: Path, output: Path):
    output = output.resolve(); output.parent.mkdir(parents=True, exist_ok=True)
    # Keep the many small install operations on the native temporary filesystem
    # when the final archive is written to a mounted Windows release directory.
    with tempfile.TemporaryDirectory(prefix="lvgl-linux-package-") as temporary:
        stage = Path(temporary) / "LVGL-Protobuf-Linux-SDK"; stage.mkdir()
        subprocess.run(["cmake", "--install", str(build.resolve()), "--prefix", str(stage)], check=True)
        libraries = list(stage.rglob("libLVGLProtobufDisplay.so"))
        if len(libraries) != 1: raise ValueError("Expected one installed Protobuf shared library")
        library = libraries[0]; raw = library.read_bytes()
        if raw[:4] != b"\x7fELF": raise ValueError("SDK library is not ELF")
        machine = struct.unpack_from("<H" if raw[5] == 1 else ">H", raw, 18)[0]
        architecture = {62: "x86_64", 183: "aarch64"}.get(machine, "elf-" + str(machine))
        dynamic = subprocess.check_output(["readelf", "-d", str(library)], text=True)
        if "libSDL3.so.0" in dynamic and not (library.parent / "libSDL3.so.0").exists():
            linked = subprocess.check_output(["ldd", str(library)], text=True)
            match = re.search(r"libSDL3\.so\.0\s+=>\s+(\S+)", linked)
            if not match: raise ValueError("Cannot locate the built library's SDL3 runtime")
            shutil.copy2(Path(match.group(1)).resolve(), library.parent / "libSDL3.so.0")
        own = Path(__file__).resolve().parent
        (stage / "examples").mkdir()
        shutil.copy2(own / "examples/board.c", stage / "examples/board.c")
        viewer = list(stage.rglob("lvgl-pb-viewer"))
        if viewer: shutil.copy2(own.parent / "windows/example.c", stage / "examples/window.c")
        config = stage / "cmake"; config.mkdir()
        relative = library.relative_to(stage).as_posix()
        (config / "LVGLProtobufDisplayConfig.cmake").write_text('''if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "This SDK contains Linux binaries")
endif()
get_filename_component(pb_sdk "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_library(LVGLProtobuf::Display SHARED IMPORTED GLOBAL)
set_target_properties(LVGLProtobuf::Display PROPERTIES IMPORTED_LOCATION "${pb_sdk}/''' + relative + '''" INTERFACE_INCLUDE_DIRECTORIES "${pb_sdk}/include")
''', encoding="utf-8")
        (stage / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.24)
project(LVGLProtobufLinuxConsumer LANGUAGES C)
include(cmake/LVGLProtobufDisplayConfig.cmake)
add_executable(LVGLProtobufBoardExample examples/board.c)
target_link_libraries(LVGLProtobufBoardExample PRIVATE LVGLProtobuf::Display)
''' + ('''add_executable(LVGLProtobufWindowExample examples/window.c)
target_link_libraries(LVGLProtobufWindowExample PRIVATE LVGLProtobuf::Display)
''' if viewer else ""), encoding="utf-8")
        versions = []
        inspected = set()
        for candidate in sorted(stage.rglob("*")):
            if not candidate.is_file() or candidate.resolve() in inspected: continue
            with candidate.open("rb") as stream: header = stream.read(20)
            if header[:4] != b"\x7fELF": continue
            inspected.add(candidate.resolve())
            binary_machine = struct.unpack_from("<H" if header[5] == 1 else ">H", header, 18)[0]
            if binary_machine != machine: raise ValueError("Mixed ELF architectures in SDK: " + str(candidate))
            versions.append(subprocess.check_output(["readelf", "--version-info", str(candidate)], text=True))
        version_text = "\n".join(versions)
        requirements = {kind: sorted(set(re.findall(kind + r"_[0-9.]+", version_text)),
            key=lambda value: tuple(int(part) for part in value.split("_", 1)[1].split(".")))
            for kind in ("GLIBC", "GLIBCXX", "CXXABI")}
        minimums = ", ".join(values[-1] for values in requirements.values() if values)
        (stage / "README.md").write_text("# Linux Protobuf display C SDK\n\nBuilt architecture: " + architecture + ". Required symbol versions: **" + minimums + "**. These are measured across every bundled ELF, including SDL3. The ELF loader and C/C++ runtime must satisfy sdk.json's complete requirements. This binary is not a universal Raspberry Pi OS build; build the source toolkit on the target distribution when runtime versions differ.\n\nUse `cmake -S . -B build && cmake --build build`; the board example renders through an application callback. " + ("`bin/lvgl-pb-viewer project.pb --watch` opens the SDL3 viewer. SDL3 is bundled; its OS graphics/input libraries remain platform dependencies. " if viewer else "This build uses the board callback backend without SDL. ") + "\nAll calls run on one LVGL owner thread. No physical panel is configured by this SDK.\n", encoding="utf-8")
        manifest = {"formatVersion": 1, "sdkVersion": "0.9.4", "architecture": architecture, "symbolVersions": requirements,
                    "backend": "sdl3-and-callback" if viewer else "callback", "files": {}}
        for path in sorted(stage.rglob("*")):
            if path.is_file(): manifest["files"][path.relative_to(stage).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
        (stage / "sdk.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        with tarfile.open(output, "w:gz") as archive: archive.add(stage, arcname=stage.name)
    output.with_suffix(output.suffix + ".sha256").write_text(hashlib.sha256(output.read_bytes()).hexdigest() + "  " + output.name + "\n", encoding="utf-8")
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try: print(package(args.build_dir, args.output))
    except (OSError, ValueError, subprocess.CalledProcessError) as error: print(error, file=sys.stderr); return 1
    return 0

if __name__ == "__main__": sys.exit(main())
