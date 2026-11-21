"""Check actual viewer/C ABI output dimensions and background pixels, stdlib only."""
from pathlib import Path
import struct
import sys

root = Path(sys.argv[1])
for name, width, height, expected in (
    ("api_authored.bmp", 144, 88, (0, 255, 0)),
    ("api_override.bmp", 96, 64, (255, 0, 0)),
    ("viewer_authored.bmp", 144, 88, (0, 255, 0)),
    ("viewer_override.bmp", 96, 64, (255, 0, 0)),
):
    data = (root / name).read_bytes()
    assert data[:2] == b"BM", (name, "not a BMP")
    offset, dib = struct.unpack_from("<I", data, 10)[0], struct.unpack_from("<I", data, 14)[0]
    actual_width, actual_height = struct.unpack_from("<ii", data, 18)
    assert (actual_width, abs(actual_height)) == (width, height), (name, actual_width, actual_height)
    bpp, compression = struct.unpack_from("<H", data, 28)[0], struct.unpack_from("<I", data, 30)[0]
    assert bpp in (24, 32) and compression in (0, 3), (name, bpp, compression)
    stride = (width * bpp + 31) // 32 * 4
    x, y = width // 2, height // 2
    stored_y = height - 1 - y if actual_height > 0 else y
    pixel = data[offset + stored_y * stride + x * (bpp // 8):][:bpp // 8]
    if compression == 3:
        masks = struct.unpack_from("<III", data, 14 + (40 if dib >= 52 else dib))
        value = int.from_bytes(pixel, "little")
        def channel(mask):
            shift = (mask & -mask).bit_length() - 1
            return round(((value & mask) >> shift) * 255 / (mask >> shift))
        rgb = tuple(channel(mask) for mask in masks)
    else:
        rgb = (pixel[2], pixel[1], pixel[0])
    assert rgb == expected, (name, rgb, expected)
print("Viewer default/override and DLL authored-screen pixel/dimension checks passed")
