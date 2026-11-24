#!/usr/bin/env python3
"""Verify and execute the published cross-built app on an actual matching Mac."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import stat
import struct
import subprocess
import sys
import time
import urllib.parse
import urllib.request
import zipfile

REPOSITORY = "rohmer/LVGL_UI_Creator"
APP_NAME = "LVGL UI Creator.app"
ROOT = Path(__file__).resolve().parents[2]


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def version_tuple(value):
    parts = tuple(int(part) for part in value.split("."))
    return (parts + (0, 0, 0))[:3]


def deployment_version(text):
    for block in text.split("Load command"):
        if "cmd LC_BUILD_VERSION" in block:
            target = re.search(r"\bplatform\s+(\S+)", block)
            match = re.search(r"\bminos\s+([\d.]+)", block)
            if not target or target.group(1).lower() not in ("1", "macos"):
                raise ValueError("Native binary targets another Apple platform")
        elif "cmd LC_VERSION_MIN_MACOSX" in block:
            match = re.search(r"\bversion\s+([\d.]+)", block)
        else:
            continue
        if match:
            return match.group(1)
    raise ValueError("Native binary has no macOS deployment version")


def member_path(value):
    path = PurePosixPath(value)
    if (not value or path.is_absolute() or ".." in path.parts or "\\" in value
            or "\0" in value or path.parts[0] != APP_NAME):
        raise ValueError("Unsafe release archive path: " + value)
    return path


def validate_zip(path):
    """Validate members and link destinations before asking ditto to extract."""
    names = set()
    symlinks = set()
    with zipfile.ZipFile(path) as archive:
        if len(archive.infolist()) > 100000:
            raise ValueError("Release archive contains too many entries")
        for entry in archive.infolist():
            member = member_path(entry.filename.rstrip("/"))
            if member in names:
                raise ValueError("Duplicate release archive member")
            names.add(member)
            mode = entry.external_attr >> 16
            if stat.S_ISLNK(mode):
                symlinks.add(member)
                if entry.file_size > 4096:
                    raise ValueError("Oversized archive symlink")
                target = PurePosixPath(archive.read(entry).decode("utf-8"))
                if target.is_absolute() or "\\" in str(target) or "\0" in str(target):
                    raise ValueError("Unsafe release symlink")
                parts = list(member.parent.parts)
                for part in target.parts:
                    if part == "..":
                        if len(parts) <= 1:
                            raise ValueError("Release symlink escapes application")
                        parts.pop()
                    elif part != ".":
                        parts.append(part)
            elif not (stat.S_ISREG(mode) or stat.S_ISDIR(mode)):
                raise ValueError("Special file in release archive")
    if any(parent in symlinks for member in names for parent in member.parents):
        raise ValueError("Archive member is nested inside a symlink")


def verify_payload(parent, manifest):
    bundle = parent / APP_NAME
    expected = manifest["files"]
    actual = {p.relative_to(parent).as_posix(): p for p in bundle.rglob("*")
              if p.is_file() or p.is_symlink()}
    if set(actual) != set(expected):
        raise ValueError("Extracted payload entries differ from release manifest")
    for name, record in expected.items():
        member_path(name)
        path = actual[name]
        if not path.resolve().is_relative_to(bundle.resolve()):
            raise ValueError("Extracted symlink escapes application")
        if "symlink" in record:
            if not path.is_symlink() or os.readlink(path) != record["symlink"]:
                raise ValueError("Release symlink differs: " + name)
        elif (path.is_symlink() or path.stat().st_size != record["bytes"]
              or sha256(path) != record["sha256"]
              or stat.S_IMODE(path.stat().st_mode) != int(record["mode"], 8)):
            raise ValueError("Release file differs: " + name)


class Smoke:
    def __init__(self, output, source=ROOT):
        self.output = output.resolve()
        self.source = source.resolve()
        self.output.mkdir(parents=True, exist_ok=True)
        self.records = []
        self.env = {key: value for key, value in os.environ.items()
                    if key not in ("PYTHONHOME", "PYTHONPATH", "NODE_PATH", "NODE_OPTIONS")
                    and not key.startswith("DYLD_")}
        self.env.update(PATH="/usr/bin:/bin:/usr/sbin:/sbin", SDL_VIDEO_DRIVER="dummy",
                        SDL_AUDIO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
                        UICREATOR_SESSION_DIR=str(self.output / "session"))
        (self.output / "session").mkdir(exist_ok=True)

    def run(self, name, command, timeout=90):
        command = [str(item) for item in command]
        started = time.monotonic()
        record = {"name": name, "command": command, "pathPolicy": "macOS system-only"}
        self.records.append(record)
        print("Running " + name, flush=True)
        with (self.output / (name + ".log")).open("w") as stream:
            try:
                result = subprocess.run(command, cwd=self.output, env=self.env, stdin=subprocess.DEVNULL,
                                        stdout=stream, stderr=subprocess.STDOUT, timeout=timeout)
            except subprocess.TimeoutExpired:
                record.update(status="timeout", seconds=time.monotonic() - started)
                raise
        record.update(exitCode=result.returncode, seconds=time.monotonic() - started)
        if result.returncode:
            raise RuntimeError(f"{name} failed ({result.returncode}); see {name}.log")

    def download(self, url, path):
        request = urllib.request.Request(url, headers={"User-Agent": "LVGL-UI-Creator-release-smoke"})
        with urllib.request.urlopen(request, timeout=180) as response, path.open("wb") as stream:
            while block := response.read(1024 * 1024):
                stream.write(block)

    def request(self, folder, value):
        folder.mkdir(parents=True, exist_ok=True)
        target = folder / "request.json"
        target.write_text(json.dumps(value, ensure_ascii=False), encoding="utf-8")
        return target

    def applications(self, bundle):
        binaries = bundle / "Contents/MacOS"
        resources = bundle / "Contents/Resources"
        document = {"formatVersion": 2, "canvas": {"width": 320, "height": 240}, "variables": [],
                    "obj": [{"oid": 0, "par": -1, "base": {"name": "Installed smoke button",
                    "drag": True, "coords": {"x": 24, "y": 32, "width": 240, "height": 56}},
                    "button": {"text": "Installed application smoke"}}]}
        project = self.output / "project.json"
        project.write_text(json.dumps(document), encoding="utf-8")
        protobuf = self.output / "project.pb"
        generated = self.output / "generated-c"
        self.run("editor", [binaries / "UICreator", "--smoke-test", project])
        self.run("codegen", [binaries / "UICreatorCodeGen", project, generated])
        for name in ("ui.c", "ui.h", "ui_assets.c", "manifest.json", "CMakeLists.txt"):
            require_file(generated / name)
        self.run("compiler", [binaries / "LVGLCompiler", project, "--format", "protobuf", "--target", "9.6", "--output", protobuf])
        require_file(protobuf)
        bitmap = self.output / "viewer.bmp"
        self.run("viewer", [binaries / "UICreatorViewer", protobuf, "--frames", "4", "--screenshot", bitmap])
        raw = bitmap.read_bytes()
        if len(raw) < 54 or raw[:2] != b"BM" or struct.unpack_from("<i", raw, 18)[0] != 320 or abs(struct.unpack_from("<i", raw, 22)[0]) != 240:
            raise ValueError("Viewer did not render the expected 320x240 image")
        for frontend in ("codegen", "compiler"):
            micro = self.output / ("micropython-" + frontend)
            command = ([binaries / "UICreatorCodeGen", "--format", "micropython", protobuf, micro]
                       if frontend == "codegen" else [binaries / "LVGLCompiler", project, "--format", "micropython", "--target", "9.3", "--output", micro])
            self.run("micropython-" + frontend, command)
            for name in ("ui.py", "ui.json", "uicreator_lvgl.py", "main.py", "README.md", "lv_conf.defaults.h", "export-manifest.json"):
                require_file(micro / name)
            export = json.loads((micro / "export-manifest.json").read_text())
            if export["format"] != "micropython" or export["lvglVersion"] != "9.3.0":
                raise ValueError("Unexpected MicroPython target")
            xml = self.output / (frontend + ".xml")
            command = ([binaries / "UICreatorCodeGen", "--format", "xml", protobuf, xml]
                       if frontend == "codegen" else [binaries / "LVGLCompiler", project, "--format", "xml", "--target", "9.6", "--output", xml])
            self.run("xml-" + frontend, command)
            picture = self.output / (frontend + "-xml.bmp")
            self.run("xml-viewer-" + frontend, [binaries / "UICreatorViewer", xml, "--frames", "4", "--screenshot", picture])
            if picture.read_bytes() != raw:
                raise ValueError("XML and protobuf render differently")
        language = self.output / "language-model"
        self.run("language-model", [binaries / "UICreator", "--language-model-smoke", language])
        report = json.loads((language / "report.json").read_text())
        if report["status"] != "passed" or report["pointerBits"] != 64 or report["trieBytes"] <= 0:
            raise ValueError("Language model prediction/autocorrect did not pass")
        corpus = self.output / "french.txt"
        corpus.write_text("bonjour monde bonjour monde salut monde\n", encoding="utf-8")
        trie = self.output / "french.ngtr"
        self.run("modelgen-bundled-wordlist", [binaries / "UICreatorModelGen", "--language", "fr", "--trie-output", trie,
                 "--trie-size", "16384", "--compact", "--no-cache", "--", corpus])
        if not trie.read_bytes().startswith(b"NGTR"):
            raise ValueError("ModelGen did not create an NGTR trie using bundled defaults")
        self.converters(resources, document, project, protobuf)

    def converters(self, resources, document, project, protobuf):
        font = self.output / "font conversion 日本"
        font.mkdir()
        source = self.source / "3rdParty/LVGL/lvgl/src/libs/freetype/LiberationSans-Regular.ttf"
        request = self.request(font, {"args": ["--font", str(source), "--range", "0x20-0x7E", "--size", "16", "--bpp", "4", "--format", "lvgl", "--output", str(font / "font.c"), "--lv-font-name", "installed_font", "--lv-include", "lvgl.h"],
                    "previewArgs": ["--font", str(source), "--range", "0x20-0x7E", "--size", "16", "--bpp", "4", "--format", "bin", "--output", "preview.bin", "--no-compress"]})
        self.run("font-converter", [resources.parent / "MacOS/UICreatorConverter", "font", "--request", request])
        result = json.loads((font / "result.json").read_text())
        if not result["files"]:
            raise ValueError("Font converter produced no files")
        require_file(font / "preview.bin")
        for entry in result["files"]:
            require_file(font / entry["staged"])
        image = self.output / "image conversion 日本"
        image.mkdir()
        svg = image / "image.svg"
        svg.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="8" height="6"><rect width="8" height="6" fill="#20a0c0"/></svg>')
        request = self.request(image, {"sources": [str(svg)], "options": {"colorFormat": "ARGB8888", "format": "BIN", "compression": "LZ4", "align": 1}})
        self.run("image-converter", [resources.parent / "MacOS/UICreatorConverter", "image", "--request", request])
        result = json.loads((image / "result.json").read_text())
        if not result["files"] or result["assets"][0]["deviceFrames"][0]["w"] != 8 or result["assets"][0]["deviceFrames"][0]["h"] != 6:
            raise ValueError("SVG/LZ4 converter produced incorrect frames")
        for entry in result["files"]:
            require_file(image / entry["staged"])
        creator = self.output / "project creation 日本"
        creator.mkdir()
        ui = creator / "ui"
        ui.mkdir()
        (ui / "project.json").write_bytes(project.read_bytes())
        (ui / "project.pb").write_bytes(protobuf.read_bytes())
        catalog = json.loads((resources / "project-creator/catalog.json").read_text())
        for identity, toolkit, required in (
                ("raspberry-pi-linux", "linux-toolkit.zip", "platform/Runtime/linux/Viewer.cpp"),
                ("raspberry-pi-pico", "pico-toolkit.zip", "src/lvgl_pb_pico.cpp"),
                ("raspberry-pi-pico2", "pico-toolkit.zip", "src/lvgl_pb_pico.cpp")):
            template = next(item for item in catalog if item.get("uicreatorTemplate") == identity)
            folder = creator / identity
            destination = folder / "new project"
            value = {"operation": "create", "manifest": template, "destination": str(destination), "project": document,
                     "format": "protobuf", "lvglVersion": "template", "cache": str(creator / "cache"), "uiSourceFolder": str(ui)}
            value["linuxToolkitArchive" if "linux" in identity else "picoToolkitArchive"] = str(resources / "project-creator" / toolkit)
            request = self.request(folder, value)
            self.run("project-creator-" + identity, [resources / "project-creator/UICreatorProjectTool", "--request", request], 180)
            for name in ("CMakeLists.txt", "ui/project.pb", "embedded/features.json", "embedded/config/lv_conf.h", required):
                require_file(destination / name)
            vendor = "3rdParty/LVGL/lvgl/src/display/lv_display.c"
            if sha256(destination / "embedded/vendor" / vendor) != sha256(self.source / vendor):
                raise ValueError("Project creator bundled outdated LVGL sources")


def require_file(path):
    if not path.is_file() or path.stat().st_size == 0:
        raise ValueError("Expected nonempty output is missing: " + str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", default="0.9.2")
    parser.add_argument("--expected-source-commit", required=True)
    parser.add_argument("--architecture", choices=("x86_64", "arm64"), required=True)
    parser.add_argument("--source", type=Path, default=ROOT, help="Exact package source checkout containing font/LVGL fixtures")
    parser.add_argument("--output", type=Path, default=ROOT / "build/macos-release-smoke")
    args = parser.parse_args()
    if not re.fullmatch(r"\d+\.\d+\.\d+", args.tag) or not re.fullmatch(r"[0-9a-fA-F]{40}", args.expected_source_commit):
        parser.error("Use a numeric release version and full source commit SHA")
    if sys.platform != "darwin" or platform.machine() != args.architecture:
        parser.error("Run on a native Mac matching the requested architecture")
    smoke = Smoke(args.output, args.source)
    result = {"schema": 1, "release": args.tag, "architecture": args.architecture,
              "sourceCommit": args.expected_source_commit, "execution": "actual published cross-built binaries on native macOS",
              "gatekeeperAcceptance": "not tested", "notarization": "not requested", "status": "failed"}
    try:
        stem = f"LVGL-UI-Creator-{args.tag}-macOS-{args.architecture}"
        prefix = f"https://github.com/{REPOSITORY}/releases/download/{urllib.parse.quote(args.tag)}/"
        archive = smoke.output / (stem + ".zip")
        checksum = smoke.output / (archive.name + ".sha256")
        manifest_path = smoke.output / ("macOS-" + args.architecture + ".manifest.json")
        for path in (archive, checksum, manifest_path):
            smoke.download(prefix + urllib.parse.quote(path.name), path)
        fields = checksum.read_text().split()
        digest = sha256(archive)
        if len(fields) != 2 or fields[1] != archive.name or fields[0].lower() != digest:
            raise ValueError("Published archive SHA256 sidecar differs")
        manifest = json.loads(manifest_path.read_text())
        if (manifest["architecture"] != args.architecture or manifest["release"] != args.tag
                or manifest["buildSource"]["sourceRevision"].lower() != args.expected_source_commit.lower()
                or manifest["buildSource"]["sourceDirty"] is not False):
            raise ValueError("Release provenance does not match the requested clean source commit")
        record = next(item for item in manifest["packages"] if item["file"] == archive.name)
        if record["sha256"] != digest or record["bytes"] != archive.stat().st_size:
            raise ValueError("Archive differs from release architecture manifest")
        validate_zip(archive)
        extracted = smoke.output / "extracted"
        extracted.mkdir()
        smoke.run("extract", ["/usr/bin/ditto", "-x", "-k", archive, extracted])
        verify_payload(extracted, manifest)
        bundle = extracted / APP_NAME
        smoke.run("apple-codesign", ["/usr/bin/codesign", "--verify", "--deep", "--strict", "--verbose=2", bundle])
        for index, native in enumerate(manifest["nativeFiles"]):
            binary = bundle / native["path"]
            member_path(APP_NAME + "/" + native["path"])
            smoke.run(f"architecture-{index:03}", ["/usr/bin/lipo", "-archs", binary])
            if (smoke.output / f"architecture-{index:03}.log").read_text().strip().split() != [args.architecture]:
                raise ValueError("Native file has unexpected architecture: " + native["path"])
            smoke.run(f"deployment-{index:03}", ["/usr/bin/otool", "-l", binary])
            minimum = deployment_version((smoke.output / f"deployment-{index:03}.log").read_text())
            if version_tuple(minimum) > version_tuple(manifest["minimumMacOS"]):
                raise ValueError("Native file requires a newer macOS version: " + native["path"])
        smoke.applications(bundle)
        # The tests must not modify the signed application or its resource files.
        smoke.run("apple-codesign-after-execution", ["/usr/bin/codesign", "--verify", "--deep", "--strict", "--verbose=2", bundle])
        verify_payload(extracted, manifest)
        result.update(status="passed", archiveSha256=digest, nativeFiles=len(manifest["nativeFiles"]),
                      appleCodeSign="--verify --deep --strict passed before and after execution")
    except Exception as error:
        result["error"] = str(error)
        raise
    finally:
        result["checks"] = smoke.records
        (smoke.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
