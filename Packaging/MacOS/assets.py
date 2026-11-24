#!/usr/bin/env python3
"""Prepare checksum-verified, target-native macOS project-helper runtimes on Linux."""
from __future__ import annotations

import argparse
import concurrent.futures
import functools
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request
import zipfile

PYTHON_VERSION = "3.11.18"
PYTHON_RELEASE = "20261110"
RCODESIGN_VERSION = "0.29.0"
IMAGE_REQUIREMENTS = ()
ARCHITECTURES = ("x86_64", "arm64")


def rename_directory(source: Path, destination: Path) -> None:
    for attempt in range(8):
        try:
            source.rename(destination)
            return
        except PermissionError as error:
            if os.name != 'nt' or error.winerror not in (5, 32, 33) or attempt == 7:
                raise
            time.sleep(0.01 * 2 ** attempt)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


@functools.lru_cache(maxsize=64)
def fetch(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "LVGL-UI-Creator-macOS-builder"})
    with urllib.request.urlopen(request, timeout=180) as response:
        return response.read()


def download(url: str, path: Path, digest: str) -> dict:
    if len(digest) != 64 or any(char not in "0123456789abcdef" for char in digest.lower()):
        raise ValueError("Expected a published SHA256 for " + url)
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.is_file() or sha256(path) != digest.lower():
        data = fetch(url)
        if hashlib.sha256(data).hexdigest() != digest.lower():
            raise ValueError("Download SHA256 mismatch: " + url)
        partial = path.with_name(path.name + ".partial")
        partial.write_bytes(data)
        partial.replace(path)
    return {"url": url, "file": path.name, "sha256": digest.lower(), "bytes": path.stat().st_size}


def release_asset(repository: str, tag: str, name: str) -> tuple[str, str]:
    data = json.loads(fetch(f"https://api.github.com/repos/{repository}/releases/tags/{tag}"))
    asset = next(item for item in data["assets"] if item["name"] == name)
    digest = asset.get("digest") or ""
    if digest.startswith("sha256:"):
        return asset["browser_download_url"], digest.split(":", 1)[1]
    sidecar = next(item for item in data["assets"] if item["name"] == name + ".sha256")
    return asset["browser_download_url"], fetch(sidecar["browser_download_url"]).decode().split()[0]


def safe_name(name: str) -> PurePosixPath:
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or "\\" in name or any(":" in part for part in path.parts):
        raise ValueError("Unsafe archive path: " + name)
    return path


def extract_tar(archive: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as stream:
        for item in stream.getmembers():
            path = safe_name(item.name)
            if not (item.isfile() or item.isdir() or item.issym() or item.islnk()):
                raise ValueError("Special file in runtime archive: " + item.name)
            if item.issym() or item.islnk():
                link = PurePosixPath(item.linkname)
                target = (destination / (path.parent if item.issym() else PurePosixPath()) / link).resolve()
                if link.is_absolute() or not target.is_relative_to(destination.resolve()):
                    raise ValueError("Runtime symlink escapes extraction: " + item.name)
        if hasattr(tarfile, "data_filter"):
            stream.extractall(destination, filter="data")
        else:
            stream.extractall(destination)


def python_archive(base: Path, architecture: str) -> tuple[Path, dict]:
    name = f"cpython-{PYTHON_VERSION}+{PYTHON_RELEASE}-{architecture}-install_only_stripped.tar.gz"
    url, digest = release_asset("astral-sh/python-build-standalone", PYTHON_RELEASE, name)
    archive = base / "downloads" / name
    return archive, download(url, archive, digest)


def prepare_host_python(base: Path) -> Path:
    archive, _ = python_archive(base, "x86_64-unknown-linux-gnu")
    directory = base / "assets" / "host-python"
    python = directory / "python/bin/python3"
    if not python.is_file():
        extract_tar(archive, directory)
    subprocess.run([python, "-m", "ensurepip", "--upgrade"], check=True, stdout=subprocess.DEVNULL)
    return python


def wheel_platforms(architecture: str) -> list[str]:
    versions = ([(13, 0), (12, 0), (11, 0)] + [(10, minor) for minor in range(16, 8, -1)])
    return [f"macosx_{major}_{minor}_{arch}" for arch in (architecture, "universal2")
            for major, minor in versions if architecture == "x86_64" or major >= 11 or arch == "universal2"]


def install_wheels(host: Path, base: Path, runtime: Path, architecture: str) -> list[dict]:
    if not IMAGE_REQUIREMENTS:
        return []
    directory = base / "downloads" / ("wheels-" + architecture)
    directory.mkdir(parents=True, exist_ok=True)
    command = [str(host), "-m", "pip", "download", "--disable-pip-version-check", "--only-binary=:all:",
               "--no-deps", "--dest", str(directory), "--python-version", "3.11", "--implementation", "cp",
               "--abi", "cp311", "--abi", "abi3", "--abi", "none"]
    for platform in wheel_platforms(architecture):
        command.extend(("--platform", platform))
    subprocess.run([*command, *IMAGE_REQUIREMENTS], check=True)
    records = []
    expected = {name.replace("-", "_"): version for name, version in (item.split("==") for item in IMAGE_REQUIREMENTS)}
    site = runtime / "lib/python3.11/site-packages"
    site.mkdir(parents=True, exist_ok=True)
    for wheel in sorted(directory.glob("*.whl")):
        name, version = wheel.name.split("-", 2)[:2]
        if expected.get(name.lower()) != version:
            raise ValueError("Unexpected cached converter wheel: " + wheel.name)
        data = json.loads(fetch(f"https://pypi.org/pypi/{name}/{version}/json"))
        remote = next(item for item in data["urls"] if item["filename"] == wheel.name)
        digest = remote["digests"]["sha256"]
        if sha256(wheel) != digest:
            raise ValueError("PyPI wheel checksum mismatch: " + wheel.name)
        with zipfile.ZipFile(wheel) as archive:
            if archive.testzip():
                raise ValueError("Invalid wheel: " + wheel.name)
            for info in archive.infolist():
                path = safe_name(info.filename)
                if info.is_dir():
                    continue
                parts = path.parts
                if parts and parts[0].endswith(".data"):
                    if len(parts) < 3 or parts[1] not in ("purelib", "platlib"):
                        continue
                    path = PurePosixPath(*parts[2:])
                target = site / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(info))
                target.chmod((info.external_attr >> 16) & 0o777 or 0o644)
        records.append({"url": remote["url"], "file": wheel.name, "sha256": digest, "bytes": wheel.stat().st_size})
    if len(records) != len(expected):
        raise ValueError("Incomplete target-native Python wheels")
    return records


def prune_packaging_tools(runtime: Path) -> None:
    site = runtime / "lib/python3.11/site-packages"
    for pattern in ("pip", "pip-*.dist-info", "setuptools", "setuptools-*.dist-info", "pkg_resources", "_distutils_hack"):
        for path in site.glob(pattern):
            if path.is_dir() and path.resolve().is_relative_to(runtime.resolve()):
                shutil.rmtree(path)
    for path in (runtime / "bin").glob("pip*"):
        if path.is_file():
            path.unlink()
    for name in ("distutils-precedence.pth", "easy-install.pth"):
        (site / name).unlink(missing_ok=True)
    ensurepip = runtime / "lib/python3.11/ensurepip"
    if ensurepip.is_dir():
        shutil.rmtree(ensurepip)
    # The converters execute Python but do not build Python extensions. Remove
    # development archives/objects so every packaged native file is runnable.
    headers = runtime / "include"
    if headers.is_dir():
        shutil.rmtree(headers)
    for path in (runtime / "lib").rglob("*"):
        if path.is_file() and path.suffix in (".a", ".o"):
            path.unlink()


def prepare_signer(base: Path) -> tuple[Path, dict]:
    name = f"apple-codesign-{RCODESIGN_VERSION}-x86_64-unknown-linux-musl.tar.gz"
    url, digest = release_asset("indygreg/apple-platform-rs", "apple-codesign/" + RCODESIGN_VERSION, name)
    archive = base / "downloads" / name
    record = download(url, archive, digest)
    directory = base / "assets" / "signing"
    if not list(directory.glob("**/rcodesign")):
        extract_tar(archive, directory)
    candidates = list(directory.glob("**/rcodesign"))
    if len(candidates) != 1:
        raise ValueError("Cannot locate the verified rcodesign executable")
    candidates[0].chmod(0o755)
    return candidates[0], record


def refresh_helpers(base: Path, source: Path, architectures=ARCHITECTURES) -> dict:
    """Refresh only checkout-owned scripts/wrappers; never download or touch runtimes.

    Pass the build's mirrored source snapshot, including when --skip-sync is used.
    Prepared Python runtimes, download metadata and signatures are kept.
    Converters are native C++ executables and have no script assets.
    """
    base, source = Path(base).resolve(), Path(source).resolve(strict=True)
    selected = tuple(architectures)
    if not selected or any(item not in ARCHITECTURES for item in selected):
        raise ValueError("macOS helpers support x86_64 and arm64")
    result = {}
    for architecture in selected:
        output = base / "assets" / architecture
        if output.is_symlink() or not output.resolve().is_relative_to(base):
            raise ValueError("macOS helper cache escapes its build base")
        output.mkdir(parents=True, exist_ok=True)
        project = output / "project-creator"
        for destination in (project,):
            # These are generated script directories. Guard the precise targets
            # before replacing them; symlinks must never redirect into runtimes.
            if (destination.is_symlink() or destination.resolve().parent != output.resolve() or
                    (destination.exists() and not destination.is_dir()) or
                    source.is_relative_to(destination.resolve())):
                raise ValueError("Unsafe generated macOS helper directory: " + str(destination))
        with tempfile.TemporaryDirectory(prefix=".helper-refresh-", dir=output) as temporary:
            staged_project = Path(temporary) / project.name
            staged_project.mkdir()
            for name in ("project_creator.py", "raspberry_pi_linux.py", "raspberry_pi_pico.py"):
                shutil.copy2(source / "UICreator/ProjectCreator" / name, staged_project / name)
            for folder, executable, script, relative in (
                (staged_project, "UICreatorProjectTool", "project_creator.py", 'Path(script).parent'),):
                text = '#!/bin/sh\nset -eu\nTOOL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                text += 'exec "$TOOL_DIR/../python-runtime/bin/python3" -I -B -c \'import runpy,sys;from pathlib import Path;script=sys.argv.pop(1);sys.path.insert(0,str(' + relative + '));runpy.run_path(script,run_name="__main__")\' "$TOOL_DIR/' + script + '" "$@"\n'
                (folder / executable).write_text(text)
                (folder / executable).chmod(0o755)
            # Stage all copies first so a missing source leaves previous helpers
            # available. Replacing the owned directories also drops deleted files.
            for staged, destination in ((staged_project, project),):
                if destination.exists():
                    shutil.rmtree(destination)
                rename_directory(staged, destination)
        result[architecture] = {"projectToolDirectory": str(project)}
    return result


def prepare_one(base: Path, source: Path, architecture: str, host: Path, signer: Path, signer_record: dict, tools: dict) -> dict:
    output = base / "assets" / architecture
    output.mkdir(parents=True, exist_ok=True)
    records = []
    triple = "x86_64-apple-darwin" if architecture == "x86_64" else "aarch64-apple-darwin"
    archive, record = python_archive(base, triple)
    records.append(record)
    runtime = output / "python-runtime"
    marker = output / "python-dependencies.json"
    key = {"python": PYTHON_VERSION, "release": PYTHON_RELEASE, "sha256": record["sha256"], "requirements": IMAGE_REQUIREMENTS}
    if not marker.is_file() or json.loads(marker.read_text()).get("key") != json.loads(json.dumps(key)):
        extracted = output / "python-extracted"
        if not (extracted / "python/bin/python3").is_file():
            extract_tar(archive, extracted)
        if runtime.exists():
            shutil.rmtree(runtime)
        shutil.copytree(extracted / "python", runtime, symlinks=True)
        wheel_records = install_wheels(host, base, runtime, architecture)
        prune_packaging_tools(runtime)
        marker.write_text(json.dumps({"key": key, "artifacts": wheel_records}, indent=2) + "\n")
    records.extend(json.loads(marker.read_text())["artifacts"])
    if runtime.is_symlink() or not runtime.resolve().is_relative_to(output.resolve()):
        raise ValueError("Prepared Python runtime escapes its architecture cache")
    prune_packaging_tools(runtime)
    helpers = refresh_helpers(base, source, (architecture,))[architecture]
    result = {"schema": "lvgl-ui-creator-macos-assets", "version": 1, "architecture": architecture,
              "minimumMacOS": "13.5", "nativeConverters": True,
              "pythonRuntime": str(runtime), **helpers,
              "rcodesign": str(signer), "artifacts": records + [signer_record]}
    (output / "assets.json").write_text(json.dumps(result, indent=2) + "\n")
    print("Prepared checksum-verified macOS helper assets:", architecture, flush=True)
    return result


def prepare_assets(base: Path, source: Path, architectures=ARCHITECTURES) -> dict:
    base, source = Path(base).resolve(), Path(source).resolve()
    selected = tuple(architectures)
    if not selected or any(item not in ARCHITECTURES for item in selected):
        raise ValueError("macOS assets support x86_64 and arm64")
    tools = json.loads((base / "tools.json").read_text())
    host = Path(sys.executable)
    signer, record = prepare_signer(base)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(lambda arch: prepare_one(base, source, arch, host, signer, record, tools), selected))
    return {item["architecture"]: item for item in results}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, default=Path("/var/tmp/lvgl-ui-macos"))
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--architecture", choices=(*ARCHITECTURES, "both"), default="both")
    parser.add_argument("--refresh-only", action="store_true", help="Refresh source-owned scripts only, without downloads or runtime changes")
    args = parser.parse_args()
    action = refresh_helpers if args.refresh_only else prepare_assets
    action(args.base, args.source, ARCHITECTURES if args.architecture == "both" else (args.architecture,))


if __name__ == "__main__":
    main()
