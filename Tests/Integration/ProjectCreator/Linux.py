"""Build and exercise the Raspberry Pi Linux project created by Creator.cpp.

Run on Linux (including WSL) after UICreatorProjectCreatorFunctional, for example:
python3 Tests/Integration/ProjectCreator/Linux.py path/to/output/pi-linux-json \
    --work-dir /var/tmp/uicreator-pi-linux-check
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def run(arguments, log, env=None):
    with log.open("w", encoding="utf-8") as output:
        result = subprocess.run([str(arg) for arg in arguments], stdout=output,
                                stderr=subprocess.STDOUT, env=env)
    if result.returncode:
        raise RuntimeError(f"{log.name} failed ({result.returncode}):\n" +
                           "\n".join(log.read_text(errors="replace").splitlines()[-35:]))
    print(f"{log.name}: passed", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    project = args.project.resolve()
    for relative in ("CMakeLists.txt", "embedded/features.json", "ui/project.pb",
                     "platform/Runtime/linux/Viewer.cpp"):
        if not (project / relative).is_file():
            raise ValueError(f"Not a generated Raspberry Pi Linux project: missing {relative}")
    work = args.work_dir.resolve()
    if work == project or work in project.parents or project in work.parents:
        raise ValueError("Choose a separate work directory")
    marker = work / ".pi-linux-check.json"
    if work.exists() and any(work.iterdir()) and not marker.is_file():
        raise ValueError("Choose an empty work directory or an existing Pi Linux check workspace")
    work.mkdir(parents=True, exist_ok=True)
    marker.write_text(json.dumps({"project": str(project)}), encoding="utf-8")
    source = work / "project"
    shutil.copytree(project, source, dirs_exist_ok=True)
    build = work / "build"
    run(["cmake", "-S", source, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
         "-DBUILD_TESTING=ON"], work / "configure.log")
    run(["cmake", "--build", build, "--parallel", "4"], work / "build.log")
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software")
    run(["ctest", "--test-dir", build, "--output-on-failure"], work / "tests.log", env)
    binaries = build / "platform/Runtime/linux"
    consumer = build / "pi-linux-consumer"
    run(["cc", repo / "Tests/Integration/ProjectCreator/PiLinux.c",
         "-I" + str(source / "platform/Runtime/linux/include"),
         "-I" + str(source / "platform/Runtime/windows/include"),
         "-L" + str(binaries), "-lLVGLProtobufDisplay", "-Wl,-rpath," + str(binaries),
         "-o", consumer], work / "consumer-build.log")
    run([consumer, source / "ui/project.pb"], work / "consumer.log", env)
    run([binaries / "lvgl-pb-viewer", source / "ui/project.pb", "--frames", "12",
         "--screenshot", work / "pi-linux-screen.bmp"], work / "viewer.log", env)
    if not (work / "pi-linux-screen.bmp").is_file():
        raise RuntimeError("Viewer did not save its current-screen preview")
    print("Generated Linux project: CMake build, CTest, native C ABI keyboard/action scene and SDL viewer passed")


if __name__ == "__main__":
    main()
