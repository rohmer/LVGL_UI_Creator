#!/usr/bin/env python3
"""Stage, relocate, ad-hoc sign and audit Intel/Apple Silicon app archives."""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import stat
import struct
import subprocess
import tarfile
import tempfile
import zipfile

from MacOS import macho

APP_NAME = "LVGL UI Creator.app"
EXECUTABLES = ("UICreator", "UICreatorConverter", "UICreatorViewer", "LVGLCompiler", "UICreatorCodeGen", "UICreatorModelGen")
RESOURCE_FOLDERS = ("project-creator", "language-models", "python-runtime", "integrations")
SECURITY_FLAGS = {1: "host", 0x100: "hard", 0x200: "kill", 0x400: "expires", 0x2000: "library", 0x10000: "runtime"}
SECURITY_MASK = sum(SECURITY_FLAGS)


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(data)
    return value.hexdigest()


def run(command, log: Path, environment=None) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w") as stream:
        result = subprocess.run([str(value) for value in command], env=environment,
                                stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"macOS package command failed ({result.returncode}); see {log}")


def system_dependency(name: str) -> bool:
    return name.startswith(("/System/Library/", "/usr/lib/"))


def native_files(bundle: Path) -> dict[Path, list[dict]]:
    result = {}
    for path in sorted(bundle.rglob("*")):
        if path.is_symlink():
            if not path.resolve().is_relative_to(bundle.resolve()):
                raise ValueError("Bundle symlink escapes application: " + str(path))
            continue
        if not path.is_file():
            continue
        with path.open("rb") as stream:
            magic = stream.read(4)
        if magic == b"\x7fELF" or magic[:2] == b"MZ":
            raise ValueError("Foreign native executable in macOS payload: " + str(path))
        slices = macho.inspect(path)
        if slices:
            if any(item["filetype"] == 1 for item in slices):
                raise ValueError("Development object file in macOS runtime: " + str(path))
            result[path] = slices
    return result


def thin_binaries(bundle: Path, architecture: str, tool: Path, logs: Path) -> None:
    for index, (path, slices) in enumerate(native_files(bundle).items()):
        if len([item for item in slices if item["architecture"] == architecture]) != 1:
            raise ValueError("Missing requested architecture in macOS helper: " + str(path))
        if len(slices) > 1:
            temporary = path.with_name(path.name + ".thin")
            run([tool, path, "-thin", architecture, "-output", temporary], logs / f"thin-{index:03}.log")
            temporary.chmod(stat.S_IMODE(path.stat().st_mode))
            temporary.replace(path)


def dependency_target(name: str, consumer: Path, bundle: Path, files: dict, search: list[Path]) -> Path:
    candidates = []
    if name.startswith("@loader_path/"):
        candidates.append(consumer.parent / name.removeprefix("@loader_path/"))
    elif name.startswith("@executable_path/"):
        relative = name.removeprefix("@executable_path/")
        candidates.extend((bundle / "Contents/MacOS" / relative,
                           bundle / "Contents/Resources/python-runtime/bin" / relative))
    elif name.startswith("@rpath/"):
        relative = name.removeprefix("@rpath/")
        for item in files[consumer]:
            for rpath in item["rpaths"]:
                if rpath == "@loader_path" or rpath.startswith("@loader_path/"):
                    suffix = "" if rpath == "@loader_path" else rpath.removeprefix("@loader_path/")
                    candidates.append(consumer.parent / suffix / relative)
                elif rpath == "@executable_path" or rpath.startswith("@executable_path/"):
                    suffix = "" if rpath == "@executable_path" else rpath.removeprefix("@executable_path/")
                    candidates.append(bundle / "Contents/MacOS" / suffix / relative)
                elif rpath.startswith("/"):
                    candidates.append(Path(rpath) / relative)
        candidates.extend(directory / relative for directory in search)
    elif name.startswith("/"):
        candidates.append(Path(name))
    else:
        raise ValueError("Relative Mach-O dependency is not relocatable: " + name)
    for candidate in candidates:
        if candidate.is_file() and macho.inspect(candidate):
            return candidate.resolve()
    basename = Path(name).name
    matches = [path.resolve() for path in files if path.name == basename]
    matches.extend(path.resolve() for directory in search for path in directory.glob(basename)
                   if path.is_file() and macho.inspect(path))
    matches = list(dict.fromkeys(matches))
    if matches and len({digest(path) for path in matches}) == 1:
        return matches[0]
    if len(matches) == 1:
        return matches[0]
    raise ValueError(f"Cannot resolve target-native dependency {name} for {consumer}")


def relocate(bundle: Path, tool: Path, search: list[Path], logs: Path, capabilities: dict | None = None) -> list[dict]:
    frameworks = bundle / "Contents/Frameworks"
    frameworks.mkdir(exist_ok=True)
    changes = []
    for generation in range(32):
        files = native_files(bundle)
        copied = False
        for consumer, slices in files.items():
            for name in dict.fromkeys(name for item in slices for name in item["dependencies"]):
                if system_dependency(name):
                    continue
                target = dependency_target(name, consumer, bundle, files, search)
                if target.is_relative_to(bundle.resolve()):
                    continue
                destination = frameworks / target.name
                if destination.exists() and digest(destination) != digest(target):
                    raise ValueError("Conflicting bundled dylib basename: " + destination.name)
                if not destination.exists():
                    shutil.copy2(target, destination)
                    if capabilities is not None:
                        capabilities[destination.relative_to(bundle).as_posix()] = [
                            macho.signature_details(destination, item) for item in macho.inspect(destination)]
                    copied = True
                    changes.append({"copied": str(target), "destination": str(destination.relative_to(bundle))})
        if not copied:
            break
    else:
        raise ValueError("macOS dylib dependency closure exceeded 32 iterations")
    files = native_files(bundle)
    for index, (consumer, slices) in enumerate(files.items()):
        arguments = []
        for name in dict.fromkeys(name for item in slices for name in item["dependencies"]):
            if system_dependency(name):
                continue
            target = dependency_target(name, consumer, bundle, files, search)
            if not target.is_relative_to(bundle.resolve()):
                target = (frameworks / target.name).resolve()
            relative = os.path.relpath(target, consumer.parent).replace(os.sep, "/")
            replacement = "@loader_path/" + relative
            if name != replacement:
                arguments.extend(("-change", name, replacement))
                changes.append({"binary": str(consumer.relative_to(bundle)), "from": name, "to": replacement})
        if any(item["rpaths"] for item in slices):
            arguments.append("-delete_all_rpaths")
        if any(item["filetype"] == 6 for item in slices):
            arguments.extend(("-id", "@rpath/" + consumer.name))
        if arguments:
            run([tool, *arguments, consumer], logs / f"relocate-{index:03}.log")
    return changes


def icon(png: Path, target: Path) -> None:
    data = png.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Application icon is not PNG")
    width, height = struct.unpack(">II", data[16:24])
    types = {128: b"ic07", 256: b"ic08", 512: b"ic09", 1024: b"ic10"}
    if width != height or width not in types:
        raise ValueError("macOS icon requires a square 128/256/512/1024 PNG")
    item = types[width] + struct.pack(">I", len(data) + 8) + data
    target.write_bytes(b"icns" + struct.pack(">I", len(item) + 8) + item)


def prepare_bundle(bundle: Path, version: str, minimum: str) -> None:
    contents = bundle / "Contents"
    resources = contents / "Resources"
    for executable in EXECUTABLES:
        path = contents / "MacOS" / executable
        if not path.is_file():
            raise ValueError("Missing macOS application executable: " + executable)
        path.chmod(0o755)
    for folder in RESOURCE_FOLDERS:
        if not (resources / folder).is_dir():
            raise ValueError("Missing macOS helper/resource directory: " + folder)
    icon(resources / "UICreator.png", resources / "UICreator.icns")
    info = {"CFBundleDevelopmentRegion": "en", "CFBundleDisplayName": "LVGL UI Creator",
            "CFBundleExecutable": "UICreator", "CFBundleIdentifier": "org.lvgl.uicreator",
            "CFBundleInfoDictionaryVersion": "6.0", "CFBundleName": "LVGL UI Creator",
            "CFBundlePackageType": "APPL", "CFBundleShortVersionString": version, "CFBundleVersion": version,
            "CFBundleIconFile": "UICreator.icns", "LSMinimumSystemVersion": minimum,
            "LSApplicationCategoryType": "public.app-category.developer-tools", "NSHighResolutionCapable": True,
            "NSHumanReadableCopyright": "LVGL UI Creator contributors"}
    (contents / "Info.plist").write_bytes(plistlib.dumps(info, sort_keys=True))
    (contents / "PkgInfo").write_bytes(b"APPL????")


def signing_capabilities(bundle: Path) -> dict:
    return {path.relative_to(bundle).as_posix(): [macho.signature_details(path, item) for item in slices]
            for path, slices in native_files(bundle).items()}


def signing_arguments(capabilities: dict, logs: Path) -> list[str]:
    arguments = ["--timestamp-url", "none"]
    for path, slices in capabilities.items():
        for index, original in enumerate(slices):
            scope = path + ("@" + str(index) if len(slices) > 1 else "")
            unknown = original["flags"] & ~(SECURITY_MASK | 2 | 0x20000)
            if unknown:
                raise ValueError(f"Unrecognized signing flags {unknown:#x} in {path}")
            for bit, name in SECURITY_FLAGS.items():
                if original["flags"] & bit:
                    arguments.extend(("--code-signature-flags", scope + ":" + name))
            if original.get("runtimeVersion") is not None:
                arguments.extend(("--runtime-version", scope + ":" + original["runtimeVersion"]))
            if original["entitlements"] is not None:
                destination = logs / ("entitlements-" + hashlib.sha256(scope.encode()).hexdigest() + ".plist")
                destination.write_bytes(plistlib.dumps(original["entitlements"]))
                arguments.extend(("--entitlements-xml-file", scope + ":" + str(destination)))
    return arguments


def verify_resource_seal(bundle: Path, inventory: list[dict]) -> dict:
    contents = bundle / "Contents"
    seal = plistlib.loads((contents / "_CodeSignature/CodeResources").read_bytes())
    files = seal.get("files2")
    if not isinstance(files, dict):
        raise ValueError("Missing version 2 bundle resource seal")
    native = {item["path"].removeprefix("Contents/"): item for item in inventory}
    for name, entry in files.items():
        path = contents / name
        if not path.resolve().is_relative_to(contents.resolve()) or not isinstance(entry, dict):
            raise ValueError("Invalid bundle resource seal path: " + name)
        if "symlink" in entry:
            if not path.is_symlink() or os.readlink(path) != entry["symlink"]:
                raise ValueError("Bundle symlink seal does not match: " + name)
        elif "cdhash" in entry:
            hashes = [directory["cdHash"] for directory in native.get(name, {}).get("codeDirectories", [])]
            if entry["cdhash"].hex() not in hashes:
                raise ValueError("Nested code signature seal does not match: " + name)
        elif "hash2" in entry or "hash" in entry:
            if not path.is_file() or path.is_symlink():
                raise ValueError("Missing sealed bundle resource: " + name)
            for key, algorithm in (("hash2", "sha256"), ("hash", "sha1")):
                if key in entry:
                    hasher = hashlib.new(algorithm)
                    with path.open("rb") as stream:
                        for block in iter(lambda: stream.read(1024 * 1024), b""):
                            hasher.update(block)
                    if hasher.digest() != entry[key]:
                        raise ValueError("Bundle resource hash does not match: " + name)
        else:
            raise ValueError("Unrecognized bundle resource seal: " + name)
    for path in contents.rglob("*"):
        name = path.relative_to(contents).as_posix()
        if path.is_dir() and not path.is_symlink():
            continue
        if name in ("Info.plist", "PkgInfo", "MacOS/UICreator") or name.startswith("_CodeSignature/"):
            continue
        if name not in files:
            raise ValueError("Unsealed bundle resource: " + name)
    return {"sealedResources": len(files), "resourceSealSHA256": digest(contents / "_CodeSignature/CodeResources")}


def audit(bundle: Path, architecture: str, minimum: str, logs: Path, signer: Path, original: dict) -> dict:
    info = plistlib.loads((bundle / "Contents/Info.plist").read_bytes())
    if info["CFBundleExecutable"] != "UICreator" or info["LSMinimumSystemVersion"] != minimum:
        raise ValueError("macOS bundle metadata does not match the build")
    files = native_files(bundle)
    inventory = []
    for index, (path, slices) in enumerate(files.items()):
        selected = [item for item in slices if item["architecture"] == architecture]
        if len(selected) != 1 or len(slices) != 1:
            raise ValueError("Wrong Mach-O target architecture: " + str(path))
        for item in selected:
            if item["platform"] != 1 or not item["minimumMacOS"]:
                raise ValueError("Missing native macOS deployment metadata: " + str(path))
            if macho.version_tuple(item["minimumMacOS"]) > macho.version_tuple(minimum):
                raise ValueError("Bundled binary requires a newer macOS release: " + str(path))
            for name in item["dependencies"]:
                if system_dependency(name):
                    continue
                if not name.startswith("@loader_path/"):
                    raise ValueError("Unresolved Mach-O dependency: " + name)
                target = (path.parent / name.removeprefix("@loader_path/")).resolve()
                if not target.is_relative_to(bundle.resolve()) or not target.is_file():
                    raise ValueError("Bundled dependency escapes or is missing: " + name)
            if item["rpaths"]:
                raise ValueError("Unexpected runtime build rpath: " + str(path))
            external = {1: bundle / "Contents/Info.plist", 3: bundle / "Contents/_CodeSignature/CodeResources"} if path == bundle / "Contents/MacOS/UICreator" else None
            verification = macho.verify_code_pages(path, item, external)
        final_capabilities = [macho.signature_details(path, item) for item in slices]
        before = original.get(path.relative_to(bundle).as_posix(), [])
        if before and (len(before) != len(final_capabilities) or any(
                (old["flags"] & SECURITY_MASK) != (new["flags"] & SECURITY_MASK) or
                (old.get("runtimeVersion") is not None and old["runtimeVersion"] != new.get("runtimeVersion")) or
                old["entitlements"] != new["entitlements"]
                for old, new in zip(before, final_capabilities))):
            raise ValueError("Resigning changed runtime capabilities: " + str(path))
        inventory.append({"path": path.relative_to(bundle).as_posix(), "architectures": [item["architecture"] for item in slices],
                          "minimumMacOS": selected[0]["minimumMacOS"], "dependencies": selected[0]["dependencies"],
                          "sha256": digest(path), "capabilities": final_capabilities, **verification})
    for executable in EXECUTABLES:
        if (bundle / "Contents/MacOS" / executable) not in files:
            raise ValueError("Required CLI is not a native Mach-O executable: " + executable)
    resource_verification = verify_resource_seal(bundle, inventory)
    manifest = {}
    for path in sorted(bundle.rglob("*")):
        name = path.relative_to(bundle.parent).as_posix()
        if path.is_symlink():
            manifest[name] = {"symlink": os.readlink(path)}
        elif path.is_file():
            manifest[name] = {"bytes": path.stat().st_size, "sha256": digest(path), "mode": oct(stat.S_IMODE(path.stat().st_mode))}
    return {"architecture": architecture, "minimumMacOS": minimum, "signing": "ad-hoc", "notarized": False,
            "execution": "not run: cross-built on Linux", "nativeFiles": inventory, "files": manifest,
            "signatureVerification": "all code pages, special slots and bundle resource seals", **resource_verification}


def archive_inventory(bundle: Path) -> dict:
    result = {}
    for path in (bundle, *sorted(bundle.rglob("*"))):
        name = path.relative_to(bundle.parent).as_posix()
        mode = path.lstat().st_mode
        item = {"mode": stat.S_IMODE(mode)}
        if stat.S_ISLNK(mode):
            item.update(type="symlink", target=os.readlink(path))
        elif stat.S_ISDIR(mode):
            item.update(type="directory")
        elif stat.S_ISREG(mode):
            item.update(type="file", bytes=path.stat().st_size, sha256=digest(path))
        else:
            raise ValueError("Unsupported bundle archive file type: " + name)
        result[name] = item
    return result


def stream_digest(stream) -> str:
    hasher = hashlib.sha256()
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        hasher.update(block)
    return hasher.hexdigest()


def verify_archive(path: Path, expected: dict) -> None:
    actual = {}
    if path.name.endswith(".zip"):
        with zipfile.ZipFile(path) as archive:
            for entry in archive.infolist():
                name = entry.filename.rstrip("/")
                mode = entry.external_attr >> 16
                item = {"mode": stat.S_IMODE(mode)}
                if stat.S_ISLNK(mode):
                    item.update(type="symlink", target=archive.read(entry).decode())
                elif stat.S_ISDIR(mode):
                    item.update(type="directory")
                elif stat.S_ISREG(mode):
                    with archive.open(entry) as stream:
                        item.update(type="file", bytes=entry.file_size, sha256=stream_digest(stream))
                else:
                    raise ValueError("Unexpected ZIP member type: " + name)
                if name in actual:
                    raise ValueError("Duplicate ZIP member: " + name)
                actual[name] = item
    else:
        with tarfile.open(path, "r:gz") as archive:
            for entry in archive:
                name = entry.name.rstrip("/")
                item = {"mode": stat.S_IMODE(entry.mode)}
                if entry.issym():
                    item.update(type="symlink", target=entry.linkname)
                elif entry.isdir():
                    item.update(type="directory")
                elif entry.isfile():
                    with archive.extractfile(entry) as stream:
                        item.update(type="file", bytes=entry.size, sha256=stream_digest(stream))
                else:
                    raise ValueError("Unexpected TGZ member type: " + name)
                if name in actual:
                    raise ValueError("Duplicate TGZ member: " + name)
                actual[name] = item
    if actual != expected:
        names = set(actual) ^ set(expected)
        names.update(name for name in set(actual) & set(expected) if actual[name] != expected[name])
        raise ValueError("Archive differs from verified bundle: " + ", ".join(sorted(names)[:10]))


def archives(bundle: Path, output: Path, stem: str) -> list[dict]:
    records = []
    expected = archive_inventory(bundle)
    target = output / (stem + ".tar.gz")
    with target.open("wb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, filename="", compresslevel=6) as compressed:
        with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
            for path in (bundle, *sorted(bundle.rglob("*"))):
                info = archive.gettarinfo(path, arcname=path.relative_to(bundle.parent).as_posix())
                info.uid = info.gid = 0
                info.uname = info.gname = ""
                info.mtime = 0
                if info.islnk():
                    info.type, info.linkname, info.size = tarfile.REGTYPE, "", path.stat().st_size
                if info.isfile():
                    with path.open("rb") as stream:
                        archive.addfile(info, stream)
                else:
                    archive.addfile(info)
    verify_archive(target, expected)
    records.append({"file": target.name, "bytes": target.stat().st_size, "sha256": digest(target)})
    target = output / (stem + ".zip")
    with zipfile.ZipFile(target, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in [bundle, *sorted(bundle.rglob("*"))]:
            name = path.relative_to(bundle.parent).as_posix()
            mode = path.lstat().st_mode
            entry = zipfile.ZipInfo(name + ("/" if stat.S_ISDIR(mode) else ""), date_time=(2020, 1, 1, 0, 0, 0))
            entry.create_system = 3
            entry.external_attr = mode << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            if stat.S_ISLNK(mode):
                archive.writestr(entry, os.readlink(path).encode())
            elif stat.S_ISREG(mode):
                with path.open("rb") as stream, archive.open(entry, "w") as destination:
                    shutil.copyfileobj(stream, destination, 1024 * 1024)
            else:
                archive.writestr(entry, b"")
    verify_archive(target, expected)
    records.append({"file": target.name, "bytes": target.stat().st_size, "sha256": digest(target)})
    for item in records:
        (output / (item["file"] + ".sha256")).write_text(item["sha256"] + "  " + item["file"] + "\n", encoding="ascii")
    return records


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-directory", type=Path, required=True)
    parser.add_argument("--architecture", choices=("x86_64", "arm64"), required=True)
    parser.add_argument("--base", type=Path)
    parser.add_argument("--cmake")
    parser.add_argument("--configuration", default="Release")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    build = args.build_directory.resolve()
    metadata = json.loads((build / "macos-package.json").read_text())
    if metadata["architecture"] != args.architecture:
        raise ValueError("Configured macOS architecture does not match requested package")
    recorded = json.loads((build / "build-metadata.json").read_text()) if (build / "build-metadata.json").is_file() else {}
    base = (args.base or Path(recorded.get("base", "/var/tmp/lvgl-ui-macos"))).resolve()
    tools = json.loads((base / "tools.json").read_text())
    assets = json.loads((base / "assets" / args.architecture / "assets.json").read_text())
    output = (args.output or build / "packages").resolve()
    output.mkdir(parents=True, exist_ok=True)
    logs = output / ("verification-" + args.architecture)
    logs.mkdir(exist_ok=True)
    environment = dict(os.environ)
    environment.update(recorded.get("environment", {}))
    cmake = args.cmake or tools["cmake"]
    install_tool = Path(tools["llvm"]["bin"]) / "llvm-install-name-tool"
    signer = Path(assets["rcodesign"])
    with tempfile.TemporaryDirectory(prefix="lvgl-ui-macos-") as temporary:
        stage = Path(temporary)
        run([cmake, "--install", build, "--config", args.configuration, "--prefix", stage], logs / "install.log", environment)
        bundle = stage / APP_NAME
        compiler_rt = tools.get("compilerRt")
        if compiler_rt:
            notice = Path(compiler_rt["license"])
            if not notice.is_file():
                raise ValueError("Missing compiler-rt runtime license: " + str(notice))
            if digest(notice) != compiler_rt["licenseSha256"]:
                raise ValueError("Compiler-rt runtime license checksum differs: " + str(notice))
            notices = bundle / "Contents/Resources/licenses/compiler-rt"
            notices.mkdir(parents=True, exist_ok=True)
            shutil.copy2(notice, notices / "LICENSE.TXT")
        prepare_bundle(bundle, metadata["version"], metadata["minimumMacOS"])
        thin_binaries(bundle, args.architecture, Path(tools["llvm"]["bin"]) / "llvm-lipo", logs)
        original = signing_capabilities(bundle)
        search = [Path(metadata["runtime"]), build / "Runtime/macos", build / "3rdParty/SDL3"]
        relocations = relocate(bundle, install_tool, search, logs, original)
        clean_environment = {key: value for key, value in environment.items() if not key.startswith("RCODESIGN_")}
        run([signer, "-C", "/dev/null", "sign", *signing_arguments(original, logs), bundle], logs / "sign.log", clean_environment)
        report = audit(bundle, args.architecture, metadata["minimumMacOS"], logs, signer, original)
        report.update(schema="lvgl-ui-creator-macos-artifacts", version=1, release=metadata["version"],
                      relocations=relocations, sdk=tools["sdk"], assets=assets["artifacts"],
                      buildSource={name: recorded.get(name) for name in (
                          "sourceRevision", "sourceDirty", "sourceDirectory", "toolchainSignature")})
        if compiler_rt:
            report["compilerRt"] = compiler_rt
        report["packages"] = archives(bundle, output, f"LVGL-UI-Creator-{metadata['version']}-macOS-{args.architecture}")
        (output / f"macOS-{args.architecture}.manifest.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Verified ad-hoc-signed macOS {args.architecture} app ZIP/TGZ packages: {output}")


if __name__ == "__main__":
    main()
