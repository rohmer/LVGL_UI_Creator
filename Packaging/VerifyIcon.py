#!/usr/bin/env python3
"""Verify a multi-resolution ICO and its actual embedded Windows PE resources."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


REQUIRED_SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def part(data, offset, size):
    require(offset >= 0 and size >= 0 and offset + size <= len(data), "Truncated or invalid icon/resource offset")
    return data[offset:offset + size]


def unpack(fmt, data, offset):
    return struct.unpack(fmt, part(data, offset, struct.calcsize(fmt)))


def image_record(data, width, height):
    require(width == height and 0 < width <= 256, "Icon frame must be square and no larger than 256px")
    if data.startswith(b'\x89PNG\r\n\x1a\n'):
        position, compressed, header, ended = 8, bytearray(), None, False
        while position < len(data):
            length, = unpack('>I', data, position)
            kind = part(data, position + 4, 4)
            content = part(data, position + 8, length)
            checksum, = unpack('>I', data, position + 8 + length)
            require(zlib.crc32(kind + content) & 0xffffffff == checksum, "PNG icon chunk checksum differs")
            if kind == b'IHDR':
                require(header is None and length == 13, "Invalid PNG icon header")
                header = struct.unpack('>IIBBBBB', content)
            elif kind == b'IDAT':
                compressed.extend(content)
            elif kind == b'IEND':
                ended = True
                break
            position += length + 12
        require(header is not None and ended, "PNG icon lacks IHDR or IEND")
        w, h, bits, color, compression, filtering, interlace = header
        require((w, h) == (width, height), "PNG frame dimensions differ from icon directory")
        require(compression == filtering == interlace == 0, "Unsupported PNG icon compression/filter/interlace")
        channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(color)
        require(channels is not None and bits in (1, 2, 4, 8, 16), "Invalid PNG icon color format")
        stride = (width * channels * bits + 7) // 8 + 1
        raw = zlib.decompress(compressed)
        require(len(raw) == stride * height, "PNG icon decompressed pixel size differs")
        require(all(raw[row * stride] <= 4 for row in range(height)), "PNG icon uses an invalid row filter")
        encoding = 'PNG'
    else:
        header, w, doubled_height, planes, bits = unpack('<IiiHH', data, 0)
        require(header >= 40 and planes == 1 and bits in (1, 4, 8, 16, 24, 32), "Invalid DIB icon header")
        require(w == width and doubled_height == height * 2, "DIB frame dimensions differ from icon directory")
        palette = (1 << bits) * 4 if bits <= 8 else 0
        minimum = header + palette + ((width * bits + 31) // 32) * 4 * height
        require(len(data) >= minimum, "DIB icon pixel data is truncated")
        encoding = 'DIB'
    return {'width': width, 'height': height, 'encoding': encoding,
            'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def ico_records(path, required_sizes=REQUIRED_SIZES):
    data = Path(path).read_bytes()
    reserved, kind, count = unpack('<HHH', data, 0)
    require(reserved == 0 and kind == 1 and 0 < count <= 256, "Not a valid Windows ICO")
    records = []
    for index in range(count):
        width, height, colors, reserved, planes, bits, size, offset = unpack('<BBBBHHII', data, 6 + index * 16)
        require(reserved == 0 and offset >= 6 + count * 16, "Invalid ICO frame directory")
        records.append(image_record(part(data, offset, size), width or 256, height or 256))
    sizes = [record['width'] for record in records]
    require(len(set(sizes)) == len(sizes), "ICO contains duplicate resolution entries")
    require(set(required_sizes) <= set(sizes), "ICO lacks required sizes: " + str(sorted(set(required_sizes) - set(sizes))))
    return records


def pe_resources(path):
    data = Path(path).read_bytes()
    require(part(data, 0, 2) == b'MZ', "Executable lacks DOS header")
    pe, = unpack('<I', data, 0x3c)
    require(part(data, pe, 4) == b'PE\0\0', "Executable lacks PE signature")
    section_count, = unpack('<H', data, pe + 6)
    optional_size, = unpack('<H', data, pe + 20)
    optional = pe + 24
    magic, = unpack('<H', data, optional)
    require(magic in (0x10b, 0x20b), "Unsupported PE optional header")
    directories = optional + (96 if magic == 0x10b else 112)
    require(optional + optional_size >= directories + 24, "PE lacks resource data directory")
    resource_rva, resource_size = unpack('<II', data, directories + 16)
    require(resource_rva and resource_size, "Executable has no embedded resources")
    sections = []
    for index in range(section_count):
        offset = optional + optional_size + index * 40
        virtual_size, rva, raw_size, raw_offset = unpack('<IIII', data, offset + 8)
        sections.append((rva, raw_size, raw_offset))

    def image_bytes(rva, size):
        for start, length, offset in sections:
            if start <= rva and rva + size <= start + length:
                return part(data, offset + rva - start, size)
        raise ValueError('Resource RVA is outside the executable section data')

    tree = image_bytes(resource_rva, resource_size)
    leaves = {}
    visited = set()

    def walk(offset, keys):
        require(offset not in visited and len(keys) <= 3, "Cyclic or excessively deep PE resource directory")
        visited.add(offset)
        named, numbered = unpack('<HH', tree, offset + 12)
        require(named + numbered <= 65536, "Oversized PE resource directory")
        for index in range(named + numbered):
            name, target = unpack('<II', tree, offset + 16 + index * 8)
            if name & 0x80000000:
                text_offset = name & 0x7fffffff
                length, = unpack('<H', tree, text_offset)
                name = part(tree, text_offset + 2, length * 2).decode('utf-16-le')
            if target & 0x80000000:
                walk(target & 0x7fffffff, (*keys, name))
            else:
                rva, size, codepage, reserved = unpack('<IIII', tree, target)
                key = (*keys, name)
                require(key not in leaves, "Duplicate PE resource identifier")
                leaves[key] = image_bytes(rva, size)
    walk(0, ())
    return leaves


def verify_executable(path, expected):
    resources = pe_resources(path)
    wanted = {(item['width'], item['height'], item['sha256']) for item in expected}
    groups = []
    for key, data in resources.items():
        if len(key) != 3 or key[0] != 14:
            continue
        reserved, kind, count = unpack('<HHH', data, 0)
        require(reserved == 0 and kind == 1 and count <= 256, "Invalid RT_GROUP_ICON header")
        records = []
        for index in range(count):
            width, height, colors, reserved, planes, bits, size, icon_id = unpack('<BBBBHHIH', data, 6 + index * 14)
            candidates = [value for resource, value in resources.items()
                          if len(resource) == 3 and resource[0] == 3 and resource[1] == icon_id]
            preferred = resources.get((3, icon_id, key[2]))
            icon = preferred if preferred is not None else candidates[0] if len(candidates) == 1 else None
            require(icon is not None and len(icon) == size, "RT_GROUP_ICON references a missing/truncated RT_ICON")
            records.append(image_record(icon, width or 256, height or 256))
        if {(item['width'], item['height'], item['sha256']) for item in records} == wanted:
            groups.append({'id': key[1], 'language': key[2], 'frames': records})
    require(groups, 'Executable does not contain the canonical multi-resolution application icon: ' + str(path))
    return {'file': str(Path(path).resolve()), 'groups': groups}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--icon', required=True, type=Path)
    parser.add_argument('--executable', action='append', type=Path, default=[])
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    expected = ico_records(args.icon)
    report = {'schema': 'lvgl-ui-creator-application-icon-verification', 'version': 1,
              'icon': str(args.icon.resolve()), 'frames': expected,
              'executables': [verify_executable(path, expected) for path in args.executable]}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Verified canonical icon sizes ' + ', '.join(str(item['width']) for item in expected)
          + ' in ' + str(len(args.executable)) + ' executable(s)')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, struct.error, zlib.error, UnicodeError) as error:
        raise SystemExit(str(error))
