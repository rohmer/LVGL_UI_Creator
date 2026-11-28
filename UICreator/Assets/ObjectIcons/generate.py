"""Build embedded LVGL geometry and standalone SVGs from the object icon catalog.

Requires only the Python standard library. Run with --check to verify checked-in
outputs without writing; --preview <path.svg> also creates a labeled contact sheet.
"""
from __future__ import annotations

import argparse
import html
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent
COLORS = {"none": "none", "primary": "#D6E5F5", "accent": "#45D6B3", "blue": "#65AFFF", "muted": "#8FA8C4"}
TONES = {name: name.title() for name in COLORS}


def number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError(f"Invalid geometric coordinate: {value!r}")
    return format(value, ".6g")


def color(value):
    if value not in COLORS:
        raise ValueError(f"Unknown color role: {value}")
    return COLORS[value]


def literal(value):
    text = number(value)
    return text + ("f" if any(character in text for character in ".eE") else ".0f")


def primitives(command):
    kind, *args = command
    if kind == "polyline":
        points, stroke, width = args
        if len(points) < 2:
            raise ValueError("A polyline must contain at least two points")
        return [("Line", stroke, "none", width, [*first, *second]) for first, second in zip(points, points[1:])]
    if kind == "rect":
        x, y, w, h, r, stroke, fill, width = args
        if w <= 0 or h <= 0 or r < 0:
            raise ValueError("Invalid rectangle dimensions")
        return [("Rect", stroke, fill, width, [x, y, w, h, r])]
    if kind == "line":
        x1, y1, x2, y2, stroke, width = args
        return [("Line", stroke, "none", width, [x1, y1, x2, y2])]
    if kind == "circle":
        x, y, radius, stroke, fill, width = args
        if radius <= 0:
            raise ValueError("Invalid circle radius")
        return [("Circle", stroke, fill, width, [x, y, radius])]
    if kind == "arc":
        x, y, radius, start, end, stroke, width = args
        if radius <= 0 or end <= start or end - start > 360:
            raise ValueError("Invalid arc radius or clockwise angle range")
        return [("Arc", stroke, "none", width, [x, y, radius, start, end])]
    if kind == "triangle":
        points, fill = args
        if len(points) != 3 or any(len(point) != 2 for point in points):
            raise ValueError("A triangle must contain three points")
        return [("Triangle", "none", fill, 0, [item for point in points for item in point])]
    raise ValueError(f"Unknown icon primitive: {kind}")


def svg(command):
    kind, *args = command
    if kind in ("line", "polyline", "arc"):
        stroke, width = args[-2:]
        style = f'fill="none" stroke="{color(stroke)}" stroke-width="{number(width)}"'
    if kind in ("rect", "circle"):
        stroke, fill, width = args[-3:]
        style = f'fill="{color(fill)}" stroke="{color(stroke)}" stroke-width="{number(width)}"'
    if kind == "rect":
        x, y, w, h, r = args[:5]
        return f'<rect x="{number(x)}" y="{number(y)}" width="{number(w)}" height="{number(h)}" rx="{number(r)}" {style}/>'
    if kind == "circle":
        x, y, r = args[:3]
        return f'<circle cx="{number(x)}" cy="{number(y)}" r="{number(r)}" {style}/>'
    if kind == "line":
        x1, y1, x2, y2 = args[:4]
        return f'<path d="M{number(x1)} {number(y1)}L{number(x2)} {number(y2)}" {style}/>'
    if kind in ("polyline", "triangle"):
        points = " ".join(f"{number(x)},{number(y)}" for x, y in args[0])
        if kind == "triangle":
            return f'<polygon points="{points}" fill="{color(args[1])}"/>'
        return f'<polyline points="{points}" {style}/>'
    if kind == "arc":
        x, y, r, start, end = args[:5]
        if end - start == 360:
            return f'<circle cx="{number(x)}" cy="{number(y)}" r="{number(r)}" {style}/>'
        radians = [math.radians(value) for value in (start, end)]
        x1, y1 = x + r * math.cos(radians[0]), y + r * math.sin(radians[0])
        x2, y2 = x + r * math.cos(radians[1]), y + r * math.sin(radians[1])
        path = f"M{number(x1)} {number(y1)}A{number(r)} {number(r)} 0 {int(end-start>180)} 1 {number(x2)} {number(y2)}"
        return f'<path d="{path}" {style}/>'
    raise ValueError(kind)


def outputs():
    records = []
    keys = set()
    for family, first, count in (("widgets", 0, 40), ("bootstrap", 0, 36), ("templates", 36, 13)):
        entries = json.loads((ROOT / f"{family}.json").read_text(encoding="utf-8"))
        if [entry["id"] for entry in entries] != list(range(first, first + count)):
            raise ValueError(f"{family} must contain exactly {count} canonical ordered IDs")
        for entry in entries:
            key = entry["key"]
            if key in keys or not key or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789._-" for c in key):
                raise ValueError(f"Invalid or repeated icon key: {key}")
            if not key.startswith({"widgets": "widget.", "bootstrap": "bootstrap.", "templates": "template."}[family]):
                raise ValueError(f"Wrong icon namespace: {key}")
            keys.add(key)
            if not entry["name"] or not entry["commands"]:
                raise ValueError(f"Incomplete icon: {key}")
            commands = [primitive for command in entry["commands"] for primitive in primitives(command)]
            for kind, stroke, fill, width, points in commands:
                color(stroke); color(fill); number(width)
                if stroke == fill == "none" or (stroke != "none" and width <= 0):
                    raise ValueError(f"Invisible icon command: {key}")
                for value in points:
                    number(value)
                positions = points[:4] if kind == "Line" else points if kind == "Triangle" else points[:2]
                if any(value < 0 or value > 32 for value in positions):
                    raise ValueError(f"Icon coordinate outside 32-unit viewbox: {key}")
            records.append((family, entry, commands))
    result = {}
    native = ["// Generated by Assets/ObjectIcons/generate.py. Edit the JSON sources, then regenerate."]
    manifest = []
    for index, (family, entry, commands) in enumerate(records):
        native.append(f"static const Primitive geometry{index}[] = {{")
        for kind, stroke, fill, width, points in commands:
            coordinates = ", ".join(literal(value) for value in points + [0] * (6-len(points)))
            native.append(f"    {{Shape::{kind}, Tone::{TONES[stroke]}, Tone::{TONES[fill]}, {literal(width)}, {{{coordinates}}}}},")
        native.append("};")
        body = '\n'.join("  " + svg(command) for command in entry["commands"])
        svg_name = entry["key"] + ".svg"
        result[ROOT / "svg" / svg_name] = f'<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32">\n<title>{html.escape(entry["name"])}</title>\n<g stroke-linecap="round" stroke-linejoin="round">\n{body}\n</g>\n</svg>\n'
        manifest.append({"family": family, "id": entry["id"], "key": entry["key"], "name": entry["name"], "primitives": len(commands), "svg": "svg/" + svg_name})
    native.append("static const Icon catalog[] = {")
    for index, (_, entry, commands) in enumerate(records):
        native.append(f"    {{{json.dumps(entry['key'])}, {json.dumps(entry['name'])}, geometry{index}, {len(commands)}}},")
    native.append("};\n")
    result[ROOT / "ObjectIconData.inc"] = "\n".join(native)
    result[ROOT / "catalog.json"] = json.dumps({"viewBox": 32, "colors": COLORS, "icons": manifest}, indent=2) + "\n"
    return records, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    records, generated = outputs()
    for path, text in generated.items():
        if args.check:
            if not path.is_file() or path.read_text(encoding="utf-8") != text:
                raise ValueError(f"Generated icon asset is stale: {path}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8", newline="\n")
    if args.preview:
        cards = []
        for index, (_, entry, _) in enumerate(records):
            x, y = (index % 7) * 160, (index // 7) * 104
            body = ''.join(svg(command) for command in entry["commands"])
            cards.append(f'<g transform="translate({x} {y})"><rect x="5" y="5" width="150" height="94" rx="8" fill="#293647"/><svg x="56" y="13" width="48" height="48" viewBox="0 0 32 32"><g stroke-linecap="round" stroke-linejoin="round">{body}</g></svg><text x="80" y="82" fill="#E4EAF2" text-anchor="middle" font-family="sans-serif" font-size="10">{html.escape(entry["name"])}</text></g>')
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        args.preview.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="1120" height="1248" viewBox="0 0 1120 1248"><rect width="1120" height="1248" fill="#152131"/>' + ''.join(cards) + '</svg>\n', encoding="utf-8")
    print(f"{'Verified' if args.check else 'Generated'} {len(records)} explicit icons, embedded geometry and standalone SVGs")


if __name__ == "__main__":
    main()
