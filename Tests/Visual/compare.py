#!/usr/bin/env python3
"""Exact, dependency-free BMP/PNG fixture comparison; baseline changes are explicit."""
from __future__ import annotations

import argparse
import binascii
import json
import struct
import sys
import zlib
from pathlib import Path


class ComparisonError(ValueError):
    pass


def artifact(directory: Path, name: str) -> Path:
    path = (directory / name).resolve()
    if not path.is_relative_to(directory.resolve()):
        raise ComparisonError(f"Artifact leaves its directory: {name}")
    return path


def read_image(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:2] == b"BM":
        if len(data) < 54:
            raise ComparisonError(f"Truncated BMP: {path}")
        offset = struct.unpack_from("<I", data, 10)[0]
        header, width, signed_height, planes, bits, compression = struct.unpack_from("<IiiHHI", data, 14)
        height = abs(signed_height)
        if header < 40 or width < 1 or height < 1 or planes != 1 or bits not in (24, 32) or compression not in (0, 3):
            raise ComparisonError(f"Expected a 24/32-bit uncompressed or bitfield BMP: {path}")
        if width * height > 16_777_216:
            raise ComparisonError("Image exceeds sixteen million pixels")
        stride = ((width * bits + 31) // 32) * 4
        if offset + stride * height > len(data):
            raise ComparisonError(f"Truncated BMP pixels: {path}")
        masks = (0xFF0000, 0xFF00, 0xFF)
        if compression == 3:
            masks = struct.unpack_from("<III", data, 54)
            if any(mask == 0 for mask in masks):
                raise ComparisonError("BMP has empty RGB bit masks")
        rgba = bytearray(b"\xff") * (width * height * 4)
        for y in range(height):
            source_y = height - y - 1 if signed_height > 0 else y
            if masks == (0xFF0000, 0xFF00, 0xFF):
                row = data[offset + source_y * stride:offset + source_y * stride + width * (bits // 8)]
                start, end = y * width * 4, (y + 1) * width * 4
                rgba[start:end:4] = row[2::bits // 8]
                rgba[start + 1:end:4] = row[1::bits // 8]
                rgba[start + 2:end:4] = row[0::bits // 8]
                continue
            for x in range(width):
                start = offset + source_y * stride + x * (bits // 8)
                pixel = int.from_bytes(data[start:start + bits // 8], "little")
                channels = []
                for mask in masks:
                    shift = (mask & -mask).bit_length() - 1
                    maximum = mask >> shift
                    channels.append(((pixel & mask) >> shift) * 255 // maximum)
                target = (y * width + x) * 4
                rgba[target:target + 4] = bytes((*channels, 255))
        return width, height, bytes(rgba)
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ComparisonError(f"Expected BMP or PNG: {path}")
    position, compressed, dimensions = 8, bytearray(), None
    while position < len(data):
        if position + 12 > len(data):
            raise ComparisonError("Truncated PNG chunk")
        length = struct.unpack_from(">I", data, position)[0]
        kind, body = data[position + 4:position + 8], data[position + 8:position + 8 + length]
        if position + length + 12 > len(data):
            raise ComparisonError("Truncated PNG body")
        crc = struct.unpack_from(">I", data, position + length + 8)[0]
        if binascii.crc32(kind + body) & 0xFFFFFFFF != crc:
            raise ComparisonError("PNG checksum failed")
        if kind == b"IHDR":
            dimensions = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            compressed.extend(body)
        elif kind == b"IEND":
            break
        position += length + 12
    if not dimensions:
        raise ComparisonError("PNG dimensions are missing")
    width, height, depth, color, compression, filtering, interlace = dimensions
    channels = {0: 1, 2: 3, 4: 2, 6: 4}.get(color)
    if not channels or depth != 8 or compression or filtering or interlace or not width or not height:
        raise ComparisonError("Use a non-interlaced 8-bit grayscale/RGB/RGBA PNG")
    if width * height > 16_777_216:
        raise ComparisonError("Image exceeds sixteen million pixels")
    raw = zlib.decompress(compressed)
    stride = width * channels
    if len(raw) != (stride + 1) * height:
        raise ComparisonError("PNG pixel length is incorrect")
    result, previous = bytearray(), bytearray(stride)
    for y in range(height):
        mode = raw[y * (stride + 1)]
        row = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride) if mode else ():
            left = row[x - channels] if x >= channels else 0
            above, upper_left = previous[x], previous[x - channels] if x >= channels else 0
            if mode == 1:
                row[x] = (row[x] + left) & 255
            elif mode == 2:
                row[x] = (row[x] + above) & 255
            elif mode == 3:
                row[x] = (row[x] + (left + above) // 2) & 255
            elif mode == 4:
                p = left + above - upper_left
                distances = abs(p - left), abs(p - above), abs(p - upper_left)
                predictor = (left, above, upper_left)[distances.index(min(distances))]
                row[x] = (row[x] + predictor) & 255
            elif mode != 0:
                raise ComparisonError("Unknown PNG row filter")
        if color == 6:
            result.extend(row)
        for x in range(width) if color != 6 else ():
            pixel = row[x * channels:(x + 1) * channels]
            if color == 0:
                result.extend((pixel[0], pixel[0], pixel[0], 255))
            elif color == 4:
                result.extend((pixel[0], pixel[0], pixel[0], pixel[1]))
            else:
                result.extend(pixel if color == 6 else (*pixel, 255))
        previous = row
    return width, height, bytes(result)


def write_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF)
    rows = b"".join(b"\0" + pixels[y * width * 4:(y + 1) * width * 4] for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def manifest(directory: Path) -> dict:
    value = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    if value.get("format") != "lvgl-visual-fixtures" or value.get("version") != 1 or not value.get("cases"):
        raise ComparisonError("Expected a nonempty lvgl-visual-fixtures version 1 manifest")
    ids = [case["id"] for case in value["cases"]]
    if len(set(ids)) != len(ids):
        raise ComparisonError("Duplicate visual case IDs")
    return value


def update_baselines(actual: Path, baseline: Path) -> dict:
    value = manifest(actual)
    # Validate the entire capture before changing any baseline.
    for case in value["cases"]:
        width, height, _ = read_image(artifact(actual, case["file"]))
        if (width, height) != (case["width"], case["height"]):
            raise ComparisonError(f"Capture dimensions disagree with manifest: {case['id']}")
    baseline.mkdir(parents=True, exist_ok=True)
    for case in value["cases"]:
        width, height, pixels = read_image(artifact(actual, case["file"]))
        case["file"] = str(Path(case["file"]).with_suffix(".png"))
        target = artifact(baseline, case["file"])
        target.parent.mkdir(parents=True, exist_ok=True)
        write_png(target, width, height, pixels)
    (baseline / "manifest.json").write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    return {"updated": len(value["cases"]), "baseline": str(baseline)}


def compare(actual: Path, baseline: Path, output: Path, masks: dict | None = None, channel_threshold: int = 0, max_different_pixels: int = 0, tolerance_reason: str = "") -> dict:
    if (channel_threshold or max_different_pixels) and not tolerance_reason.strip():
        raise ComparisonError("A nonzero tolerance requires --tolerance-reason")
    if not 0 <= channel_threshold <= 255 or max_different_pixels < 0:
        raise ComparisonError("Invalid comparison tolerance")
    current, expected = manifest(actual), manifest(baseline)
    if current.get("environment") != expected.get("environment"):
        raise ComparisonError("Capture environment differs from baseline; use the same SDL driver, platform, LVGL version and built-in font configuration")
    current_cases, expected_cases = ({case["id"]: case for case in value["cases"]} for value in (current, expected))
    if current_cases.keys() != expected_cases.keys():
        raise ComparisonError(f"Visual case set changed: missing={sorted(expected_cases.keys()-current_cases.keys())}, extra={sorted(current_cases.keys()-expected_cases.keys())}; review and explicitly update baselines")
    masks = masks or {}
    if masks.keys() - current_cases.keys():
        raise ComparisonError("Mask names an unknown visual case")
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for identity, case in current_cases.items():
        width, height, pixels = read_image(artifact(actual, case["file"]))
        bw, bh, reference = read_image(artifact(baseline, expected_cases[identity]["file"]))
        if (width, height) != (bw, bh) or (width, height) != (case["width"], case["height"]):
            raise ComparisonError(f"Resolution changed: {identity}")
        if pixels == reference and not masks.get(identity):
            results.append({"id": identity, "passed": True, "differentPixels": 0, "maskedPixels": 0, "maximumChannelDelta": 0})
            continue
        excluded = bytearray(width * height)
        for rectangle in masks.get(identity, []):
            if not rectangle.get("reason", "").strip():
                raise ComparisonError(f"Every mask needs a reviewable reason: {identity}")
            x, y, w, h = (rectangle[key] for key in ("x", "y", "width", "height"))
            if not all(isinstance(value, int) for value in (x, y, w, h)) or x < 0 or y < 0 or w <= 0 or h <= 0 or x + w > width or y + h > height:
                raise ComparisonError(f"Mask leaves the framebuffer: {identity}")
            for row in range(y, y + h):
                excluded[row * width + x:row * width + x + w] = b"\1" * w
        changed, maximum, difference = 0, 0, bytearray(len(pixels))
        for index in range(width * height):
            offset = index * 4
            delta = max(abs(pixels[offset + channel] - reference[offset + channel]) for channel in range(4))
            if excluded[index]:
                difference[offset:offset + 4] = b"\0\x60\xff\xff"
            elif delta > channel_threshold:
                changed += 1
                maximum = max(maximum, delta)
                difference[offset:offset + 4] = b"\xff\0\x40\xff"
            else:
                gray = sum(pixels[offset:offset + 3]) // 6
                difference[offset:offset + 4] = bytes((gray, gray, gray, 255))
        passed = changed <= max_different_pixels
        result = {"id": identity, "passed": passed, "differentPixels": changed, "maskedPixels": sum(excluded), "maximumChannelDelta": maximum}
        if not passed:
            diff_path = artifact(output, identity + ".diff.png")
            diff_path.parent.mkdir(parents=True, exist_ok=True)
            write_png(diff_path, width, height, difference)
            result["differenceImage"] = str(diff_path)
        results.append(result)
    report = {"format": "lvgl-visual-comparison", "version": 1, "passed": all(case["passed"] for case in results), "channelThreshold": channel_threshold, "maxDifferentPixels": max_different_pixels, "toleranceReason": tolerance_reason, "cases": results}
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--actual", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("visual-differences"))
    parser.add_argument("--update", action="store_true", help="Explicitly replace reviewed baseline captures")
    parser.add_argument("--masks", type=Path, help="JSON mapping case IDs to reasoned x/y/width/height rectangles")
    parser.add_argument("--channel-threshold", type=int, default=0)
    parser.add_argument("--max-different-pixels", type=int, default=0)
    parser.add_argument("--tolerance-reason", default="")
    args = parser.parse_args()
    try:
        if args.update:
            print(json.dumps(update_baselines(args.actual, args.baseline), indent=2))
            return 0
        masks = json.loads(args.masks.read_text(encoding="utf-8")) if args.masks else None
        report = compare(args.actual, args.baseline, args.output, masks, args.channel_threshold, args.max_different_pixels, args.tolerance_reason)
        failed = [case["id"] for case in report["cases"] if not case["passed"]]
        print(f"Visual comparison: {len(report['cases']) - len(failed)}/{len(report['cases'])} passed; {args.output / 'comparison.json'}")
        if failed:
            print("Changed captures: " + ", ".join(failed), file=sys.stderr)
        return 0 if report["passed"] else 1
    except (ComparisonError, OSError, ValueError, KeyError, struct.error, zlib.error) as error:
        print(f"Visual comparison failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
