"""Bounded Mach-O inspection and ad-hoc code-page verification without macOS."""
from __future__ import annotations

import hashlib
import mmap
from pathlib import Path
import plistlib
import struct

THIN = {b"\xce\xfa\xed\xfe": ("<", False), b"\xcf\xfa\xed\xfe": ("<", True),
        b"\xfe\xed\xfa\xce": (">", False), b"\xfe\xed\xfa\xcf": (">", True)}
FAT = {b"\xca\xfe\xba\xbe": (">", False), b"\xbe\xba\xfe\xca": ("<", False),
       b"\xca\xfe\xba\xbf": (">", True), b"\xbf\xba\xfe\xca": ("<", True)}
ARCH = {0x01000007: "x86_64", 0x0100000C: "arm64", 7: "i386", 12: "arm"}


def version(value: int) -> str:
    return f"{value >> 16}.{(value >> 8) & 255}.{value & 255}"


def version_tuple(value: str) -> tuple[int, ...]:
    parts = tuple(int(item) for item in value.split("."))
    return (parts + (0, 0, 0))[:3]


def directory_header_size(revision: int) -> int:
    for introduced, size in ((0x20600, 108), (0x20500, 96), (0x20400, 88),
                             (0x20300, 64), (0x20200, 52), (0x20100, 48)):
        if revision >= introduced:
            return size
    return 44


def _thin(data, offset: int, size: int) -> dict:
    magic = data[offset:offset + 4]
    if magic not in THIN:
        raise ValueError("Invalid Mach-O slice magic")
    endian, wide = THIN[magic]
    header = 32 if wide else 28
    if size < header or offset + size > len(data):
        raise ValueError("Truncated Mach-O slice")
    _, cpu, subtype, filetype, count, command_bytes, flags = struct.unpack_from(endian + "7I", data, offset)
    if count > 65536 or command_bytes > size - header:
        raise ValueError("Invalid Mach-O load command bounds")
    result = {"architecture": ARCH.get(cpu, "cpu-" + str(cpu)), "cpu": cpu, "subtype": subtype,
              "filetype": filetype, "offset": offset, "size": size, "minimumMacOS": None,
              "platform": None, "dependencies": [], "rpaths": [], "id": None, "signature": None,
              "embeddedInfoPlist": None}
    cursor, end = offset + header, offset + header + command_bytes
    for _ in range(count):
        if cursor + 8 > end:
            raise ValueError("Truncated Mach-O load command")
        command, length = struct.unpack_from(endian + "II", data, cursor)
        kind = command & 0x7FFFFFFF
        if length < 8 or cursor + length > end:
            raise ValueError("Invalid Mach-O load command size")
        if kind in (1, 0x19):
            segment_header, section_size = (72, 80) if kind == 0x19 else (56, 68)
            if length < segment_header:
                raise ValueError("Truncated Mach-O segment command")
            sections = struct.unpack_from(endian + "I", data, cursor + (64 if kind == 0x19 else 48))[0]
            if sections > (length - segment_header) // section_size:
                raise ValueError("Mach-O sections escape segment command")
            for index in range(sections):
                section = cursor + segment_header + index * section_size
                if bytes(data[section:section + 16]).rstrip(b"\0") != b"__info_plist":
                    continue
                section_bytes = struct.unpack_from(endian + ("Q" if kind == 0x19 else "I"),
                                                   data, section + (40 if kind == 0x19 else 36))[0]
                section_offset = struct.unpack_from(endian + "I", data, section + (48 if kind == 0x19 else 40))[0]
                if section_offset > size or section_bytes > size - section_offset or result["embeddedInfoPlist"]:
                    raise ValueError("Invalid embedded Mach-O Info.plist bounds")
                result["embeddedInfoPlist"] = {"offset": section_offset, "size": section_bytes}
        elif kind in (0xC, 0xD, 0x18, 0x1F, 0x20, 0x23, 0x1C):
            if length < 12:
                raise ValueError("Invalid Mach-O string command")
            start = struct.unpack_from(endian + "I", data, cursor + 8)[0]
            if start < 12 or start >= length:
                raise ValueError("Invalid Mach-O load string offset")
            raw = data[cursor + start:cursor + length]
            name = bytes(raw).split(b"\0", 1)[0].decode("utf-8")
            if kind == 0x1C:
                result["rpaths"].append(name)
            elif kind == 0xD:
                result["id"] = name
            else:
                result["dependencies"].append(name)
        elif kind == 0x32:
            if length < 24:
                raise ValueError("Invalid Mach-O build version")
            platform, minimum, sdk = struct.unpack_from(endian + "III", data, cursor + 8)
            result.update(platform=platform, minimumMacOS=version(minimum), sdk=version(sdk))
        elif kind == 0x24:
            if length < 16:
                raise ValueError("Invalid Mach-O minimum version")
            minimum, sdk = struct.unpack_from(endian + "II", data, cursor + 8)
            result.update(platform=1, minimumMacOS=version(minimum), sdk=version(sdk))
        elif kind == 0x1D:
            if length < 16:
                raise ValueError("Invalid Mach-O code signature command")
            start, length_ = struct.unpack_from(endian + "II", data, cursor + 8)
            if start > size or length_ > size - start:
                raise ValueError("Mach-O signature escapes slice")
            result["signature"] = {"offset": start, "size": length_}
        cursor += length
    if cursor != end:
        raise ValueError("Mach-O load command length mismatch")
    return result


def _slices(data) -> list[dict]:
    magic = data[:4]
    if magic in THIN:
        return [_thin(data, 0, len(data))]
    if magic not in FAT:
        return []
    if len(data) < 8:
        raise ValueError("Truncated universal Mach-O header")
    endian, wide = FAT[magic]
    count = struct.unpack_from(endian + "I", data, 4)[0]
    stride = 32 if wide else 20
    if not count or count > 64 or 8 + count * stride > len(data):
        raise ValueError("Invalid universal Mach-O slice table")
    slices, ranges = [], []
    for index in range(count):
        cursor = 8 + index * stride
        cpu, subtype = struct.unpack_from(endian + "II", data, cursor)
        offset, size = struct.unpack_from(endian + ("QQ" if wide else "II"), data, cursor + 8)
        if not size or offset < 8 + count * stride or offset > len(data) or size > len(data) - offset:
            raise ValueError("Universal Mach-O slice escapes file")
        if any(offset < previous_end and offset + size > previous_start for previous_start, previous_end in ranges):
            raise ValueError("Overlapping universal Mach-O slices")
        item = _thin(data, offset, size)
        if item["cpu"] != cpu or item["subtype"] != subtype:
            raise ValueError("Universal Mach-O architecture does not match its slice")
        slices.append(item)
        ranges.append((offset, offset + size))
    return slices


def inspect(path: Path) -> list[dict]:
    path = Path(path)
    if not path.is_file() or path.stat().st_size < 4:
        return []
    with path.open("rb") as stream:
        magic = stream.read(4)
        if magic not in THIN and magic not in FAT:
            return []
        with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
            return _slices(data)


def signature_details(path: Path, item: dict) -> dict:
    """Read public signing capabilities before install-name edits invalidate them."""
    result = {"flags": 0, "entitlements": None, "runtimeVersion": None}
    if not item["signature"]:
        return result
    if item["signature"]["size"] < 12:
        raise ValueError("Truncated original code signature")
    with Path(path).open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        start = item["offset"] + item["signature"]["offset"]
        magic, length, count = struct.unpack_from(">III", data, start)
        if magic != 0xFADE0CC0 or length > item["signature"]["size"] or 12 + count * 8 > length:
            raise ValueError("Invalid original code signature")
        has_der = False
        for index in range(count):
            kind, relative = struct.unpack_from(">II", data, start + 12 + index * 8)
            if relative > length - 8:
                raise ValueError("Original signature blob escapes bounds")
            blob = start + relative
            tag, size = struct.unpack_from(">II", data, blob)
            if size < 8 or relative + size > length:
                raise ValueError("Original signature blob has invalid size")
            if kind == 0:
                if tag != 0xFADE0C02 or size < 44:
                    raise ValueError("Invalid original code directory")
                revision = struct.unpack_from(">I", data, blob + 8)[0]
                if size < directory_header_size(revision):
                    raise ValueError("Truncated original versioned code directory")
                result["flags"] = struct.unpack_from(">I", data, blob + 12)[0]
                if revision >= 0x20500 and result["flags"] & 0x10000:
                    result["runtimeVersion"] = version(struct.unpack_from(">I", data, blob + 88)[0])
            elif kind == 5:
                result["entitlements"] = plistlib.loads(data[blob + 8:blob + size].rstrip(b"\0"))
            elif kind == 7:
                has_der = True
        if has_der and result["entitlements"] is None:
            raise ValueError("DER-only entitlements require explicit conversion before resigning")
    return result


def verify_code_pages(path: Path, item: dict, external: dict[int, Path] | None = None) -> dict:
    """Verify all code pages and special slots, including bundle resource seals."""
    signature = item["signature"]
    if not signature:
        raise ValueError("Missing Mach-O signature: " + str(path))
    with Path(path).open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        start = item["offset"] + signature["offset"]
        finish = start + signature["size"]
        if finish - start < 12:
            raise ValueError("Truncated Mach-O signature")
        magic, length, count = struct.unpack_from(">III", data, start)
        if magic != 0xFADE0CC0 or length > finish - start or count > 1024 or 12 + count * 8 > length:
            raise ValueError("Invalid Mach-O signature superblob")
        directories, blobs = [], {}
        for index in range(count):
            kind, relative = struct.unpack_from(">II", data, start + 12 + index * 8)
            if relative > length - 8:
                raise ValueError("Embedded code signature blob escapes bounds")
            size = struct.unpack_from(">I", data, start + relative + 4)[0]
            if size < 8 or relative + size > length or kind in blobs:
                raise ValueError("Invalid or duplicate embedded code signature blob")
            blobs[kind] = data[start + relative:start + relative + size]
        for index in range(count):
            kind, relative = struct.unpack_from(">II", data, start + 12 + index * 8)
            if kind != 0 and not 0x1000 <= kind <= 0x1005:
                continue
            directory = start + relative
            if relative > length - 44:
                raise ValueError("Invalid code directory offset")
            values = struct.unpack_from(">9I4BI", data, directory)
            tag, size, revision, flags, hash_offset, identifier_offset, special, slots, code_limit = values[:9]
            hash_size, hash_type, _, page_shift = values[9:13]
            if tag != 0xFADE0C02 or size < 44 or relative + size > length or page_shift > 24:
                raise ValueError("Invalid Mach-O code directory")
            minimum_size = directory_header_size(revision)
            if size < minimum_size:
                raise ValueError("Truncated versioned Mach-O code directory")
            if revision >= 0x20100 and struct.unpack_from(">I", data, directory + 44)[0]:
                raise ValueError("Scatter code directories are not supported by this package audit")
            if revision >= 0x20300:
                code_limit = struct.unpack_from(">Q", data, directory + 56)[0] or code_limit
            hashes = {1: ("sha1", 20), 2: ("sha256", 32), 3: ("sha256", 20), 4: ("sha384", 48)}
            if hash_type not in hashes or hash_size != hashes[hash_type][1]:
                raise ValueError("Unsupported code signature digest")
            if (code_limit > signature["offset"] or hash_offset > size or special > hash_offset // hash_size or
                    slots > (size - hash_offset) // hash_size):
                raise ValueError("Code signature hash table escapes its directory")
            verified_special = []
            for slot in range(1, special + 1):
                first = directory + hash_offset - slot * hash_size
                expected = data[first:first + hash_size]
                content = blobs.get(slot)
                if content is None and external and slot in external:
                    content = external[slot].read_bytes()
                if content is None and slot == 1 and item.get("embeddedInfoPlist"):
                    embedded = item["embeddedInfoPlist"]
                    first = item["offset"] + embedded["offset"]
                    content = data[first:first + embedded["size"]]
                if expected == bytes(hash_size):
                    if slot in blobs:
                        raise ValueError("Embedded signature capability has no special-slot seal")
                    continue
                if content is None:
                    raise ValueError(f"Missing source for code signature special slot {slot}: {path}")
                if hashlib.new(hashes[hash_type][0], content).digest()[:hash_size] != expected:
                    raise ValueError(f"Code signature special slot {slot} does not match: {path}")
                verified_special.append(slot)
            page_size = 1 << page_shift if page_shift else max(code_limit, 1)
            if slots != (code_limit + page_size - 1) // page_size:
                raise ValueError("Code signature page count mismatch")
            for slot in range(slots):
                first = item["offset"] + slot * page_size
                last = item["offset"] + min((slot + 1) * page_size, code_limit)
                actual = hashlib.new(hashes[hash_type][0], data[first:last]).digest()[:hash_size]
                expected = data[directory + hash_offset + slot * hash_size:directory + hash_offset + (slot + 1) * hash_size]
                if actual != expected:
                    raise ValueError("Mach-O signed code page does not match: " + str(path))
            directories.append({"algorithm": hashes[hash_type][0], "pages": slots, "adHoc": bool(flags & 2),
                                "specialSlots": verified_special,
                                "cdHash": hashlib.new(hashes[hash_type][0], data[directory:directory + size]).digest()[:20].hex()})
        if not directories or not all(item["adHoc"] for item in directories):
            raise ValueError("Expected verified ad-hoc code directories: " + str(path))
        return {"codeDirectories": directories}
