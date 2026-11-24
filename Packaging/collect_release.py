"""Collect the four independently verified desktop releases and their checksums."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--windows-packages", type=Path, required=True)
    parser.add_argument("--windows-x86-packages", type=Path,
                        help="Optional separate directory for a rebuilt Windows x86 installer")
    parser.add_argument("--linux-packages", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", default="0.9.2")
    args = parser.parse_args()
    if not re.fullmatch(r"\d+\.\d+\.\d+", args.version):
        parser.error("Version must contain three numeric components")
    entries = []
    for architecture in ("x64", "x86"):
        directory = (args.windows_x86_packages or args.windows_packages) if architecture == "x86" else args.windows_packages
        entries.append((directory, "Windows", architecture, "installer",
                        f"LVGL-UI-Creator-{args.version}-windows-{architecture}-setup.exe"))
    for architecture in ("x86_64", "arm64"):
        for suffix, kind in (("deb", "installer"), ("tar.gz", "archive")):
            entries.append((args.linux_packages, "Linux", architecture, kind,
                            f"LVGL-UI-Creator-{args.version}-Linux-{architecture}.{suffix}"))
    verified = []
    for directory, system, architecture, kind, filename in entries:
        source = directory / filename
        checksum = Path(str(source) + ".sha256")
        recorded = checksum.read_text(encoding="utf-8-sig").strip().split()
        digest = sha256(source)
        if not recorded or recorded[0].lower() != digest:
            raise ValueError(f"Published checksum does not match {source}")
        destination = args.output / filename
        if destination.exists() and sha256(destination) != digest:
            raise FileExistsError(f"Use a new output directory; existing release differs: {destination}")
        verified.append((source, {"os": system, "architecture": architecture,
                                  "kind": kind, "file": filename,
                                  "bytes": source.stat().st_size, "sha256": digest}))
    args.output.mkdir(parents=True, exist_ok=True)
    for source, record in verified:
        destination = args.output / record["file"]
        if not destination.exists():
            shutil.copy2(source, destination)
        if sha256(destination) != record["sha256"]:
            raise ValueError(f"Release copy does not match: {destination}")
        Path(str(destination) + ".sha256").write_text(
            f"{record['sha256']}  {record['file']}\n", encoding="ascii")
    manifest = {"version": args.version, "defaultWindowsArchitecture": "x64",
                "packages": [record for _, record in verified]}
    (args.output / "release.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Verified four platform installers and two Linux archives: {args.output.resolve()}")


if __name__ == "__main__":
    main()
