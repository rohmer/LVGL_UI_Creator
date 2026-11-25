"""Audit the checked-in and deployed source catalog without optional packages."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def names(data):
    count = struct.unpack_from(">H", data, 4)[0]
    tables = {}
    for index in range(count):
        tag, _, offset, length = struct.unpack_from(">4sIII", data, 12 + index * 16)
        assert offset <= len(data) and length <= len(data) - offset
        tables[tag] = (offset, length)
    offset, length = tables[b"name"]
    _, count, strings = struct.unpack_from(">HHH", data, offset)
    result = {}
    for index in range(count):
        platform, encoding, language, identity, size, relative = struct.unpack_from(">HHHHHH", data, offset + 6 + index * 12)
        start = offset + strings + relative
        assert start >= offset and start + size <= offset + length
        value = data[start:start + size].decode("utf-16-be" if platform in (0, 3) else "mac-roman")
        result.setdefault(str(identity), set()).add(value)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--deployed", type=Path, required=True)
    args = parser.parse_args()
    catalog = json.loads((args.source / "catalog.json").read_text(encoding="utf-8"))
    assert catalog["formatVersion"] == 1
    entries = catalog["fonts"]
    assert len(entries) == 12
    assert [entry["id"] for entry in entries] == sorted(entry["id"] for entry in entries)
    assert len({entry["id"] for entry in entries}) == len({entry["name"] for entry in entries}) == 12
    expected = {"badh-regular": "Badh Regular", "caliper-regular": "Caliper Regular",
                "microgramma-d-medium-extended": "MicrogrammaDMedExt", "ocr-a-extended": "OCR A Extended",
                "inter-regular": "Inter Regular", "jetbrains-mono-regular": "JetBrains Mono Regular",
                "lato-regular": "Lato Regular", "montserrat-regular": "Montserrat Regular",
                "noto-sans-regular": "Noto Sans Regular", "open-sans-regular": "Open Sans Regular",
                "roboto-regular": "Roboto Regular", "source-sans-3-regular": "Source Sans 3 Regular"}
    assert {entry["id"]: entry["name"] for entry in entries} == expected
    assert sum(entry["bytes"] for entry in entries) < 4 * 1024 * 1024
    assert sum(entry.get("origin") == "user-provided" for entry in entries) == 4
    assert sum(entry.get("origin") == "upstream" for entry in entries) == 8
    notices = (args.source / "NOTICES.txt").read_text(encoding="utf-8")
    for entry in entries:
        filename = Path(entry["file"])
        assert filename.name == entry["file"] and filename.suffix == ".ttf"
        data = (args.source / filename).read_bytes()
        assert data[:4] == b"\0\1\0\0"
        assert len(data) == entry["bytes"]
        assert hashlib.sha256(data).hexdigest() == entry["sha256"]
        actual_names = names(data)
        for identity, values in entry["nameRecords"].items():
            assert set(values) == actual_names.get(identity, set()), (filename, identity)
        for identity in ("0", "13", "14"):
            for value in entry["nameRecords"][identity]:
                assert value in notices
        assert (args.deployed / filename).read_bytes() == data
        if entry["origin"] == "upstream":
            assert entry["license"] == "OFL-1.1"
            assert entry["sourceUrl"].startswith("https://") and entry["licenseUrl"].startswith("https://")
            assert entry["revision"] and entry["style"] == "Regular"
            license_file = Path(entry["licenseFile"])
            assert license_file.name == entry["licenseFile"] and license_file.suffix == ".txt"
            license_data = (args.source / license_file).read_bytes()
            assert b"SIL OPEN FONT LICENSE Version 1.1" in license_data
            assert hashlib.sha256(license_data).hexdigest() == entry["licenseSHA256"]
            assert (args.deployed / license_file).read_bytes() == license_data
            assert entry["licenseFile"] in notices
    # Catalog and notices are deployed as well as the actual TrueType bytes.
    for filename in ("catalog.json", "NOTICES.txt", "README.md"):
        assert (args.deployed / filename).read_bytes() == (args.source / filename).read_bytes()
    assert {path.name for path in args.source.glob("*.ttf")} == {entry["file"] for entry in entries}
    print("Twelve built-in fonts verified: source/deployed bytes, names, provenance, notices and eight OFL licenses")


if __name__ == "__main__":
    main()
