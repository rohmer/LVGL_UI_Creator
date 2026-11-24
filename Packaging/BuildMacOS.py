#!/usr/bin/env python3
"""Cross-build the macOS editor and its native helper payloads from Linux/WSL."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
ARCHITECTURES = ("x86_64", "arm64")
TARGETS = ("UICreator", "UICreatorViewer", "LVGLCompiler", "UICreatorCodeGen",
           "UICreatorModelGen", "LVGLProtobufMacOS")
SDK_ARCHIVES = ("LVGLProtobuf-ESP32-Toolkit.zip", "LVGLProtobuf-Arduino-Toolkit.zip",
    "LVGLProtobuf-Pico-Toolkit.zip", "LVGLProtobuf-Linux-Toolkit.zip",
    "LVGLProtobuf-Linux-Keyboard-Profile.zip", "LVGLProtobuf-Linux-x86_64.tar.gz",
    "LVGLProtobuf-Linux-aarch64-keyboard.tar.gz", "LVGLProtobufDisplay-windows.zip")


def module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(data)
    return digest.hexdigest()


def project_version(source: Path) -> str:
    value = re.search(r"project\(LVGL_UI_Creator\s+VERSION\s+(\d+\.\d+\.\d+)",
                      (source / "CMakeLists.txt").read_text(encoding="utf-8"))
    if not value:
        raise ValueError("Cannot read the application version")
    return value.group(1)


def source_identity(source: Path) -> dict:
    """Keep post-release cross-build provenance tied to the mirrored checkout."""
    try:
        revision = subprocess.run(["git", "-C", str(source), "rev-parse", "HEAD"],
                                  text=True, capture_output=True, check=True).stdout.strip()
        status = subprocess.run(["git", "-C", str(source), "status", "--porcelain"],
                                text=True, capture_output=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return {"sourceRevision": None, "sourceDirty": None}
    return {"sourceRevision": revision, "sourceDirty": bool(status)}


def download_sdk_inputs(base: Path, version: str) -> Path:
    """Download the already published device SDKs; never put Apple's SDK in the app."""
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("The SDK release must have three numeric version components")
    destination = base / "sdk-inputs" / version
    if not destination.resolve().is_relative_to(base.resolve()):
        raise ValueError("SDK input cache escaped the build cache")
    destination.mkdir(parents=True, exist_ok=True)
    prefix = "https://github.com/rohmer/LVGL_UI_Creator/releases/download/" + version + "/"
    request = urllib.request.Request(prefix + "release.json", headers={"User-Agent": "LVGL-UI-Creator-macOS"})
    with urllib.request.urlopen(request, timeout=60) as response:
        manifest = json.loads(response.read(1024 * 1024).decode("utf-8-sig"))
    if manifest.get("version") != version:
        raise ValueError("Published SDK release version differs")
    records = {item["file"]: item for item in manifest["packages"]}
    for name in SDK_ARCHIVES:
        remote = "LVGLProtobufDisplay-" + version + "-windows-x64.zip" if name == "LVGLProtobufDisplay-windows.zip" else name
        record = records.get(remote)
        if not record or not re.fullmatch(r"[0-9a-fA-F]{64}", record.get("sha256", "")):
            raise ValueError("Missing valid published SDK metadata for " + remote)
        path = destination / name
        expected = record["sha256"].lower()
        if not path.is_file() or path.stat().st_size != record["bytes"] or sha256(path) != expected:
            temporary = destination / (name + ".download")
            request = urllib.request.Request(prefix + remote, headers={"User-Agent": "LVGL-UI-Creator-macOS"})
            try:
                with urllib.request.urlopen(request, timeout=120) as response, temporary.open("wb") as stream:
                    while data := response.read(1024 * 1024):
                        stream.write(data)
                if temporary.stat().st_size != record["bytes"] or sha256(temporary) != expected:
                    raise ValueError("Published SDK checksum differs: " + remote)
                temporary.replace(path)
            finally:
                temporary.unlink(missing_ok=True)
        Path(str(path) + ".sha256").write_text(expected + "  " + name + "\n", encoding="ascii")
    (destination / "release-inputs.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return destination


def toolchain_text(tools: dict, architecture: str) -> str:
    if architecture not in ARCHITECTURES or architecture not in tools["architectures"]:
        raise ValueError("Unsupported macOS architecture: " + architecture)
    info = tools["architectures"][architecture]
    if not re.fullmatch(r"\d+\.\d+(?:\.\d+)?", tools["deploymentTarget"]):
        raise ValueError("Invalid macOS deployment target")
    settings = {
        "CMAKE_SYSTEM_NAME": "Darwin", "CMAKE_SYSTEM_PROCESSOR": architecture,
        "CMAKE_SYSTEM_VERSION": tools["osxcross"]["target"].removeprefix("darwin"),
        "CMAKE_OSX_ARCHITECTURES": architecture, "CMAKE_OSX_SYSROOT": tools["sdk"]["path"],
        "CMAKE_OSX_DEPLOYMENT_TARGET": tools["deploymentTarget"],
        "CMAKE_C_COMPILER": info["cc"], "CMAKE_CXX_COMPILER": info["cxx"],
        "CMAKE_OBJC_COMPILER": info.get("objc", info["cc"]),
        "CMAKE_OBJCXX_COMPILER": info.get("objcxx", info["cxx"]),
        "CMAKE_FIND_ROOT_PATH": tools["sdk"]["path"],
        "CMAKE_FIND_ROOT_PATH_MODE_PROGRAM": "NEVER", "CMAKE_FIND_ROOT_PATH_MODE_LIBRARY": "ONLY",
        "CMAKE_FIND_ROOT_PATH_MODE_INCLUDE": "ONLY", "CMAKE_FIND_ROOT_PATH_MODE_PACKAGE": "ONLY",
    }
    for key, variable in (("ar", "CMAKE_AR"), ("ranlib", "CMAKE_RANLIB"), ("nm", "CMAKE_NM")):
        if info.get(key):
            settings[variable] = info[key]
    lines = []
    for name, value in settings.items():
        if any(character in str(value) for character in ('"', "\n", "\r", ";")):
            raise ValueError("Unsafe CMake toolchain value: " + name)
        lines.append('set(' + name + ' "' + str(value).replace("\\", "/") + '")\n')
    return "".join(lines)


def environment(tools: dict, architecture: str, base: Path, parent: dict | None = None) -> dict:
    info = tools["architectures"][architecture]
    result = dict(os.environ if parent is None else parent)
    for name in ("PYTHONHOME", "PYTHONPATH", "NODE_OPTIONS", "NODE_PATH", "PKG_CONFIG_PATH",
                 "CFLAGS", "CXXFLAGS", "LDFLAGS", "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH",
                 "LIBRARY_PATH", "COMPILER_PATH", "QEMU_LD_PREFIX", "RUSTFLAGS",
                 "CARGO_ENCODED_RUSTFLAGS", "CARGO_BUILD_TARGET", "CMAKE_TOOLCHAIN_FILE", "CMAKE_PREFIX_PATH"):
        result.pop(name, None)
    result.update(SDKROOT=str(tools["sdk"]["path"]), MACOSX_DEPLOYMENT_TARGET=tools["deploymentTarget"],
        RUSTUP_HOME=tools["rust"]["rustupHome"], CARGO_HOME=tools["rust"]["cargoHome"],
        RUSTUP_TOOLCHAIN=tools["rust"]["version"],
        CC=info["cc"], CXX=info["cxx"], OBJC=info.get("objc", info["cc"]),
        OBJCXX=info.get("objcxx", info["cxx"]))
    paths = [Path(tools["cmake"]).parent.as_posix(), (Path(tools["osxcross"]["prefix"]) / "bin").as_posix(),
             str(tools["llvm"]["bin"]), Path(tools["rust"]["cargo"]).parent.as_posix()]
    result["PATH"] = ":".join(dict.fromkeys(paths + ["/usr/local/sbin", "/usr/local/bin", "/usr/sbin", "/usr/bin", "/sbin", "/bin"]))
    empty = base / "empty-pkgconfig"
    empty.mkdir(parents=True, exist_ok=True)
    result["PKG_CONFIG_LIBDIR"] = str(empty)
    result["PKG_CONFIG_SYSROOT_DIR"] = tools["sdk"]["path"]
    target = info["rustTarget"]
    result["CARGO_TARGET_" + target.upper().replace("-", "_") + "_LINKER"] = info["cc"]
    result["CC_" + target.replace("-", "_")] = info["cc"]
    if info.get("ar"):
        result["AR_" + target.replace("-", "_")] = info["ar"]
    return result


def run(command, log: Path, env: dict):
    command = [str(item) for item in command]
    log.with_suffix(".command.json").write_text(json.dumps(command, indent=2) + "\n")
    print("Running " + log.name, flush=True)
    with log.open("w") as stream:
        subprocess.run(command, check=True, stdout=stream, stderr=subprocess.STDOUT, env=env)
    print(log.name + " PASS", flush=True)


def build(base: Path, source: Path, architecture: str, tools: dict, sdk: Path | None,
          jobs: int, configure_only: bool = False) -> Path:
    directory = base / ("build-" + architecture)
    directory.mkdir(parents=True, exist_ok=True)
    assets = json.loads((base / "assets" / architecture / "assets.json").read_text())
    toolchain = directory / "macos-toolchain.cmake"
    content = toolchain_text(tools, architecture)
    if not toolchain.is_file() or toolchain.read_text() != content:
        toolchain.write_text(content)
    env = environment(tools, architecture, base)
    command = [tools["cmake"], "-S", source, "-B", directory, "-G", "Ninja",
        "-DCMAKE_TOOLCHAIN_FILE=" + str(toolchain), "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=OFF",
        "-DUICREATOR_USE_LVGL9=ON", "-DPython3_EXECUTABLE=" + sys.executable,
        "-DIMAGEQUANT_CARGO=" + tools["rust"]["cargo"],
        "-DUICREATOR_PREBUILT_PROJECT_TOOL_DIR=" + assets["projectToolDirectory"],
        "-DUICREATOR_RUNTIME_PYTHON_DIR=" + assets["pythonRuntime"],
        "-DUICREATOR_MACOS_BUILD_BASE=" + str(base),
        "-DUICREATOR_MACOS_REQUIRE_SDKS=" + ("ON" if sdk else "OFF"),
        "-DSDL_X11=OFF", "-DSDL_WAYLAND=OFF", "-DSDL_KMSDRM=OFF", "-DSDL_COCOA=ON",
        "-DSDL_FRAMEWORK=OFF",
        "-DSDL_TESTS=OFF", "-DSDL_EXAMPLES=OFF", "-DFT_DISABLE_HARFBUZZ=ON"]
    if sdk:
        command.append("-DUICREATOR_MACOS_SDK_DIRECTORY=" + str(sdk))
    signature = hashlib.sha256(content.encode()).hexdigest()
    signature_path = directory / "toolchain-signature.txt"
    if not signature_path.is_file() or signature_path.read_text() != signature:
        command.insert(1, "--fresh")
    run(command, directory / "configure.log", env)
    signature_path.write_text(signature)
    metadata = {"base": str(base), "architecture": architecture, "buildDirectory": str(directory), "sourceDirectory": str(source),
        "cmake": tools["cmake"], "binaryDirectory": str(directory / "bin"), "targets": list(TARGETS),
        "sdkVersion": tools["sdk"]["version"], "deploymentTarget": tools["deploymentTarget"],
        "toolchainSignature": signature, "environment": {name: env[name] for name in (
            "PATH", "SDKROOT", "MACOSX_DEPLOYMENT_TARGET", "RUSTUP_HOME", "CARGO_HOME", "PKG_CONFIG_LIBDIR")}}
    identity = base / "source-identity.json"
    if identity.is_file():
        metadata.update(json.loads(identity.read_text()))
    (directory / "build-metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    if not configure_only:
        run([tools["cmake"], "--build", directory, "--parallel", max(1, jobs), "--target", *TARGETS], directory / "build.log", env)
    return directory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT)
    parser.add_argument("--base", type=Path, default=Path("/var/tmp/lvgl-ui-macos"))
    parser.add_argument("--architecture", choices=[*ARCHITECTURES, "both"], default="both")
    parser.add_argument("--prepare", action="store_true", help="Download and prepare the verified SDK, toolchain and target-native helpers")
    parser.add_argument("--install-dependencies", action="store_true", help="Allow installation of missing host build packages")
    parser.add_argument("--jobs", type=int, default=6)
    parser.add_argument("--skip-sync", action="store_true")
    parser.add_argument("--configure-only", action="store_true")
    parser.add_argument("--sdk-directory", type=Path, help="Existing release SDK archives; otherwise downloaded from the matching published release")
    parser.add_argument("--sdk-release", help="Published SDK release override for development builds")
    parser.add_argument("--without-sdks", action="store_true", help="Development-only package omitting published device SDK archives")
    parser.add_argument("--package", action="store_true", help="Create and audit app ZIP/TGZ packages after building")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not sys.platform.startswith("linux"):
        parser.error("Run the cross-build frontend on Linux or inside WSL2")
    if args.package and args.configure_only:
        parser.error("--package requires a completed build")
    source = args.source.resolve()
    base = args.base.resolve()
    if source == base or source.is_relative_to(base):
        parser.error("Keep the original source checkout outside the tool/build cache")
    base.mkdir(parents=True, exist_ok=True)
    architectures = ARCHITECTURES if args.architecture == "both" else (args.architecture,)
    assets_module = module(Path(__file__).with_name("MacOS") / "assets.py", "macos_assets")
    if args.prepare:
        tools_module = module(Path(__file__).with_name("BuildMacOSTools.py"), "macos_build_tools")
        tools_module.prepare(base, jobs=max(1, args.jobs), install_dependencies=args.install_dependencies)
        assets_module.prepare_assets(base, source, architectures=architectures)
    else:
        legacy_assets = [arch for arch in architectures
            if not json.loads((base / "assets" / arch / "assets.json").read_text()).get("nativeConverters")]
        if legacy_assets:
            assets_module.prepare_assets(base, source, architectures=legacy_assets)
    tools = json.loads((base / "tools.json").read_text())
    if tools.get("schemaVersion") != 1:
        raise ValueError("Unsupported macOS tool cache; run --prepare")
    sdk = None
    if not args.without_sdks:
        sdk = args.sdk_directory.resolve() if args.sdk_directory else download_sdk_inputs(base, args.sdk_release or project_version(source))
        missing = [name for name in SDK_ARCHIVES if not (sdk / name).is_file()]
        if missing:
            raise ValueError("Missing release SDKs: " + ", ".join(missing))
    mirror_module = module(Path(__file__).with_name("BuildLinux.py"), "macos_source_mirror")
    mirrored = base / "source" if args.skip_sync else mirror_module.mirror(source, base)
    if not (mirrored / "CMakeLists.txt").is_file():
        raise ValueError("Source mirror is missing; omit --skip-sync")
    if not args.skip_sync:
        (base / "source-identity.json").write_text(json.dumps(source_identity(source), indent=2) + "\n")
    assets_module.refresh_helpers(base, mirrored, architectures=architectures)
    for architecture in architectures:
        directory = build(base, mirrored, architecture, tools, sdk, args.jobs, args.configure_only)
        if args.package:
            command = [sys.executable, str(Path(__file__).with_name("PackageMacOS.py")), "--base", str(base),
                "--build-directory", str(directory), "--architecture", architecture, "--cmake", tools["cmake"]]
            if args.output:
                command.extend(["--output", str(args.output.resolve())])
            subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
