"""Render the canonical application SVG into the checked-in PNG and Windows ICO.

Run with the image-converter packaging environment (Pillow and resvg_py).
No network access, fonts or external assets are used.
"""
from __future__ import annotations

import io
from pathlib import Path

from PIL import Image
import resvg_py


def main() -> None:
    packaging = Path(__file__).resolve().parent
    source = packaging / "Linux" / "lvgl-ui-creator.svg"
    rendered = resvg_py.svg_to_bytes(
        svg_path=str(source), width=512, height=512, skip_system_fonts=True
    )
    icon = Image.open(io.BytesIO(rendered)).convert("RGBA")
    icon.save(packaging / "UICreator.png", optimize=True)
    sizes = [(size, size) for size in (16, 20, 24, 32, 40, 48, 64, 128, 256)]
    # DIB frames work in Explorer, the resource compiler and the NSIS backend.
    icon.save(packaging / "UICreator.ico", format="ICO", sizes=sizes, bitmap_format="bmp")
    with Image.open(packaging / "UICreator.ico") as check:
        if check.ico.sizes() != set(sizes):
            raise RuntimeError("The generated ICO is missing required size variants")
        for size in sizes:
            if check.ico.getimage(size).getextrema()[3] != (0, 255):
                raise RuntimeError(f"The {size} icon did not preserve its transparent edge")
    print(f"Rendered {source.name}: PNG512 and {len(sizes)} Windows icon sizes")


if __name__ == "__main__":
    main()
