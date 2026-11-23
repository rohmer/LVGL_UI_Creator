"""Offline, dependency-free LVSerial Protobuf analysis for a slim LVGL 9.6 build.

This decodes the public typed-property wire schema, including version-2 default
profiles, rather than searching binary strings for widget names.
"""
from __future__ import annotations

import base64
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import trie_assets

MAX_INPUT = 16 * 1024 * 1024
SECTIONS = {
    "arc": "ARC", "bar": "BAR", "button": "BUTTON", "btnm": "BUTTONMATRIX",
    "cal": "CALENDAR", "canvas": "CANVAS", "cb": "CHECKBOX", "img": "IMAGE",
    "text": "LABEL", "dropdown": "DROPDOWN", "roller": "ROLLER", "slider": "SLIDER",
    "spinbox": "SPINBOX", "switch": "SWITCH", "textarea": "TEXTAREA", "led": "LED",
    "line": "LINE", "spinner": "SPINNER", "chart": "CHART", "table": "TABLE",
    "scale": "SCALE", "keyboard": "KEYBOARD", "tabview": "TABVIEW", "tileview": "TILEVIEW",
    "messagebox": "MSGBOX", "imagebutton": "IMAGEBUTTON", "qrcode": "QRCODE", "barcode": "BARCODE",
}
CUSTOM = {"textBox": "LVSERIAL_USE_TEXTBOX", "layerView": "LVSERIAL_USE_LAYERVIEW", "window": "LVSERIAL_USE_WINDOW",
          "advancedKeyboard": "LVSERIAL_USE_ADVANCED_KEYBOARD"}
CUSTOM_DEPENDENCIES = {
    "textBox": {"FLEX", "LABEL", "BUTTON", "TEXTAREA"},
    "layerView": {"FLEX", "LABEL", "BUTTON"},
    "window": {"FLEX", "LABEL", "BUTTON"},
    "advancedKeyboard": {"BUTTON", "LABEL", "TEXTAREA"},
}
CUSTOM_SOURCES = {"LVSERIAL_USE_ADVANCED_KEYBOARD": ["LVSerial/Widgets/AdvancedKeyboard/" + name for name in (
    "Key.cpp", "Keyboard.cpp", "TypingView.cpp", "SuggestionText.cpp", "Utf8Fold.cpp", "AdvancedKeyboard.cpp", "KeyboardFont.c")]}
CUSTOM_SOURCES[trie_assets.PREDICTION_GATE] = ["LVSerial/Widgets/AdvancedKeyboard/" + name for name in (
    "PredictionModel.cpp", "PredictionPrefix.cpp", "Prediction.cpp", "PredictionStorage.cpp")]
DEPENDENCIES = {
    "BUTTON": {"LABEL"},
    "SLIDER": {"BAR"}, "SPINBOX": {"TEXTAREA"}, "SPINNER": {"ARC"},
    "KEYBOARD": {"BUTTONMATRIX", "TEXTAREA"}, "TEXTAREA": {"LABEL"},
    "DROPDOWN": {"LABEL"}, "ROLLER": {"LABEL"},
    "CALENDAR": {"BUTTONMATRIX", "BUTTON", "LABEL", "FLEX", "CALENDAR_HEADER_ARROW"},
    "TABVIEW": {"BUTTON", "LABEL", "FLEX"}, "MSGBOX": {"BUTTON", "LABEL", "IMAGE", "FLEX"},
    "QRCODE": {"CANVAS"}, "BARCODE": {"CANVAS"}, "CANVAS": {"IMAGE"},
}
WIDGET_DIRS = {
    "ARC": "arc", "BAR": "bar", "BUTTON": "button", "BUTTONMATRIX": "buttonmatrix",
    "CALENDAR": "calendar", "CANVAS": "canvas", "CHECKBOX": "checkbox", "IMAGE": "image",
    "LABEL": "label", "DROPDOWN": "dropdown", "ROLLER": "roller", "SLIDER": "slider",
    "SPINBOX": "spinbox", "SWITCH": "switch", "TEXTAREA": "textarea", "LED": "led", "LINE": "line",
    "SPINNER": "spinner", "CHART": "chart", "TABLE": "table", "SCALE": "scale", "KEYBOARD": "keyboard",
    "TABVIEW": "tabview", "TILEVIEW": "tileview", "MSGBOX": "msgbox", "IMAGEBUTTON": "imagebutton",
    "BARCODE": "barcode", "QRCODE": "qrcode",
}
COLOR_FORMATS = {1: "RAW", 2: "RAW_ALPHA", 6: "L8", 7: "I1", 8: "I2", 9: "I4", 10: "I8", 11: "A1", 12: "A2", 13: "A4",
                 14: "A8", 18: "RGB565", 20: "RGB565A8", 21: "AL88", 27: "RGB565_SWAPPED",
                 15: "RGB888", 16: "ARGB8888", 17: "XRGB8888", 26: "ARGB8888_PREMULTIPLIED"}


def repository_root() -> Path:
    for parent in Path(__file__).resolve().parents:
        for candidate in (parent, parent / "vendor"):
            if (candidate / "LVSerial/Device/lvserial_defaults.c").is_file():
                return candidate
    raise ValueError("Cannot locate the bundled LVSerial default profiles")


def default_profiles(root: Path) -> list[list[tuple[str, object]]]:
    source = (root / "LVSerial/Device/lvserial_defaults.c").read_text(encoding="utf-8")
    tables = {}
    for name, body in re.findall(r"static const lvserial_default\s+(\w+)\[\]\s*=\s*\{(.*?)\n\};", source, re.S):
        entries = []
        pattern = r'([NBTAO])\(("(?:[^"\\]|\\.)*")(?:\s*,\s*("(?:[^"\\]|\\.)*"|-?\d+))?\)|\{("(?:[^"\\]|\\.)*"),lvserial_ValueKind_REAL,(-?\d+),NULL\}'
        for match in re.finditer(pattern, body):
            kind, key, value, real_key, real_value = match.groups()
            if real_key:
                entries.append((json.loads(real_key), float(real_value)))
            else:
                parsed = [] if kind == "A" else {} if kind == "O" else json.loads(value)
                if kind == "B":
                    parsed = bool(parsed)
                entries.append((json.loads(key), parsed))
        if not entries:
            raise ValueError("Unrecognized default profile " + name)
        if len(entries) > 64:
            raise ValueError("Default profile exceeds the wire mask size")
        tables[name] = entries
    order = re.search(r"static const profile profiles\[\]\s*=\s*\{(.*?)\};", source, re.S)
    if not order:
        raise ValueError("LVSerial default profile registry is missing")
    return [tables[name] for name in re.findall(r"PROFILE\((\w+)\)", order.group(1))]


def _varint(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    for shift in range(0, 70, 7):
        if offset >= len(data):
            raise ValueError("Truncated Protobuf varint")
        byte = data[offset]
        offset += 1
        if shift == 63 and byte > 1:
            raise ValueError("Protobuf integer exceeds 64 bits")
        value |= (byte & 127) << shift
        if byte < 128:
            return value, offset
    raise ValueError("Invalid Protobuf varint")


def _fields(data: bytes):
    offset = 0
    while offset < len(data):
        tag, offset = _varint(data, offset)
        field, wire = tag >> 3, tag & 7
        if not field:
            raise ValueError("Invalid zero Protobuf field")
        if wire == 0:
            value, offset = _varint(data, offset)
        elif wire in (1, 5):
            length = 8 if wire == 1 else 4
            value = data[offset:offset + length]
            if len(value) != length:
                raise ValueError("Truncated fixed-width Protobuf value")
            offset += length
        elif wire == 2:
            length, offset = _varint(data, offset)
            value = data[offset:offset + length]
            if len(value) != length:
                raise ValueError("Truncated length-delimited Protobuf value")
            offset += length
        else:
            raise ValueError("Unsupported Protobuf wire type")
        yield field, wire, value


def decode_project(data: bytes, root: Path | None = None) -> dict:
    if not data or len(data) > MAX_INPUT:
        raise ValueError("Project is empty or exceeds the 16 MiB offline analysis limit")
    header, properties = {}, []
    for field, wire, value in _fields(data):
        if field == 4 and wire == 2:
            properties.append(value)
        elif field in (1, 2, 3):
            if field in header:
                raise ValueError("Duplicate project header field")
            header[field] = value
    version = header.get(1, 1)
    if not all(field in header for field in (1, 2, 3)) or version not in (1, 2) or header.get(2) != 3 or header.get(3) != b"project":
        raise ValueError("Expected an LVSerial version 1 or 2 project envelope")
    profiles = default_profiles(root or repository_root())
    result, seen, containers = None, set(), []
    for raw in properties:
        values = {}
        for field, wire, value in _fields(raw):
            if field in values:
                raise ValueError("Duplicate property metadata")
            values[field] = (wire, value)
        if 1 not in values or 2 not in values:
            raise ValueError("Property path/type is missing")
        path = values[1][1].decode("utf-8")
        kind = values[2][1]
        if len(path.encode()) >= 512 or path.count("/") > 64 or path in seen or re.search(r"~(?![01])", path):
            raise ValueError("Invalid, duplicate, or excessive property path")
        seen.add(path)
        if len(seen) > 250000:
            raise ValueError("Too many project properties")
        if path:
            parent_path, _, key = path.rpartition("/")
            while containers and containers[-1][0] != parent_path:
                if containers[-1][2] != containers[-1][3]:
                    raise ValueError("Incomplete property container")
                containers.pop()
            if not containers:
                raise ValueError("Property parent is missing")
            container = containers[-1]
            if container[1] and key != str(container[3]):
                raise ValueError("Invalid array index")
            container[3] += 1
            if container[3] > container[2]:
                raise ValueError("Too many container children")
        elif result is not None:
            raise ValueError("Project has multiple roots")
        number = lambda field, default=0: values.get(field, (0, default))[1]
        if kind == 0:
            value = None
        elif kind == 1:
            value = {}
            if version == 2 and 4 in values:
                encoded = number(4)
                profile_id = (encoded >> 1) ^ -(encoded & 1)
                if profile_id < 1 or profile_id > len(profiles):
                    raise ValueError("Unknown default profile")
                mask = number(5)
                table = profiles[profile_id - 1]
                if mask >> len(table):
                    raise ValueError("Default profile mask is out of range")
                for index, (key, default) in enumerate(table):
                    if mask & (1 << index):
                        if key in value:
                            raise ValueError("Duplicate default key")
                        value[key] = copy.deepcopy(default)
        elif kind == 2:
            value = []
        elif kind == 3:
            value = bool(number(3))
        elif kind in (4, 5):
            value = number(4 if kind == 4 else 5)
            if kind == 4:
                value = (value >> 1) ^ -(value & 1)
        elif kind == 6:
            value = struct.unpack("<d", number(6, b"\0" * 8))[0]
            if not math.isfinite(value):
                raise ValueError("Nonfinite project value")
        elif kind in (7, 8):
            value = number(7, b"")
            if kind == 7:
                value = value.decode("utf-8")
        else:
            raise ValueError("Unknown property type")
        if not path:
            result = value
        else:
            parts = [part.replace("~1", "/").replace("~0", "~") for part in path[1:].split("/")]
            parent = result
            for part in parts[:-1]:
                parent = parent[int(part)] if isinstance(parent, list) else parent[part]
            if isinstance(parent, list):
                if int(parts[-1]) != len(parent):
                    raise ValueError("Sparse array index")
                parent.append(value)
            else:
                if parts[-1] in parent:
                    raise ValueError("Property duplicates a default")
                parent[parts[-1]] = value
        if kind in (1, 2):
            containers.append([path, kind == 2, number(8), 0])
    if any(expected != actual for _, _, expected, actual in containers):
        raise ValueError("Incomplete final property container")
    if not isinstance(result, dict):
        raise ValueError("Project root must be an object")
    return result


def _encode_varint(value: int) -> bytes:
    output = bytearray()
    while value >= 128:
        output.append((value & 127) | 128)
        value >>= 7
    output.append(value)
    return bytes(output)


def _wire(field: int, wire: int, value) -> bytes:
    tag = _encode_varint((field << 3) | wire)
    return tag + (_encode_varint(value) if wire == 0 else _encode_varint(len(value)) + value if wire == 2 else value)


def encode_project(document: dict, *, omit_defaults=True) -> bytes:
    """Emit the same typed wire schema, with optional version-2 scalar defaults."""
    profiles = default_profiles(repository_root()) if omit_defaults else []
    records = []
    def walk(value, path):
        if path.count("/") > 64 or len(path.encode()) >= 512:
            raise ValueError("Property path exceeds the device schema limits")
        if isinstance(value, dict): kind = 1
        elif isinstance(value, list): kind = 2
        elif value is None: kind = 0
        elif isinstance(value, bool): kind = 3
        elif isinstance(value, int): kind = 4 if value < 0 else 5
        elif isinstance(value, float): kind = 6
        elif isinstance(value, bytes): kind = 8
        elif isinstance(value, str): kind = 7
        else: raise ValueError("Unsupported project value")
        record = _wire(1, 2, path.encode()) + _wire(2, 0, kind)
        omitted = set()
        if kind == 1 and omit_defaults:
            best = (0, 0, 0)
            for profile_id, table in enumerate(profiles, 1):
                mask = saving = 0
                for index, (key, default) in enumerate(table):
                    if key in value and type(value[key]) is type(default) and value[key] == default:
                        mask |= 1 << index
                        saving += len(path.encode()) + len(key.encode()) + 8
                overhead = 3 + len(_encode_varint(mask))
                if saving > overhead and saving - overhead > best[0]: best = (saving - overhead, profile_id, mask)
            if best[1]:
                record += _wire(4, 0, best[1] << 1) + _wire(5, 0, best[2])
                omitted = {key for index, (key, _) in enumerate(profiles[best[1] - 1]) if best[2] & (1 << index)}
        if kind in (1, 2): record += _wire(8, 0, len(value) - len(omitted))
        elif kind == 3: record += _wire(3, 0, int(value))
        elif kind == 4:
            if value < -(1 << 63): raise ValueError("Signed project integer exceeds 64 bits")
            record += _wire(4, 0, (value << 1) ^ (value >> 63))
        elif kind == 5:
            if value >= 1 << 64: raise ValueError("Unsigned project integer exceeds 64 bits")
            record += _wire(5, 0, value)
        elif kind == 6:
            if not math.isfinite(value): raise ValueError("Nonfinite project value")
            record += _wire(6, 1, struct.pack("<d", value))
        elif kind in (7, 8): record += _wire(7, 2, value.encode() if kind == 7 else value)
        records.append(_wire(4, 2, record))
        if kind in (1, 2):
            entries = sorted(value.items()) if kind == 1 else enumerate(value)
            for key, child in entries:
                if key in omitted: continue
                key = str(key).replace("~", "~0").replace("/", "~1")
                walk(child, path + "/" + key)
    walk(document, "")
    return _wire(1, 0, 2 if omit_defaults else 1) + _wire(2, 0, 3) + _wire(3, 2, b"project") + b"".join(records)


def _runtime_document(document: dict) -> dict:
    """Discard authoring definitions, keeping concrete widgets and theme tokens.

    A component instance is already materialized in obj. Its editable baseline
    and unused library definitions must not require widgets, codecs or tries on
    the device. Work on a copy so inspecting a saved editor project is read-only.
    """
    result = copy.deepcopy(document)
    def page(value):
        if not isinstance(value, dict): return
        for field in ("components", "sharedStyles"): value.pop(field, None)
        records = value.get("obj", [])
        if isinstance(records, list):
            for record in records:
                if not isinstance(record, dict): continue
                base = record.get("base")
                design = base.get("design") if isinstance(base, dict) else None
                if isinstance(design, dict):
                    design.pop("component", None)
                    if not design: base.pop("design", None)
        screens = value.get("screens", [])
        if isinstance(screens, list):
            for screen in screens: page(screen)
    page(result)
    return result


def optimize_document(document: dict, retain_editor_assets=False) -> dict:
    result = _runtime_document(document)
    trie_assets.prepare(result, repository_root(), prune=True)
    def visit(value):
        if isinstance(value, dict):
            if not retain_editor_assets and value.get("deviceFrames"):
                for field in ("sourceData", "previewFrames", "conversionOptions", "options", "sourcePath", "originalPath"):
                    value.pop(field, None)
            for child in value.values(): visit(child)
        elif isinstance(value, list):
            for child in value: visit(child)
    visit(result)
    return result


def analyze_projects(documents: list[dict], *, color_depth=16, default_font=14) -> dict:
    if color_depth not in (16, 32):
        raise ValueError("ESP32 software display color depth must be 16 or 32")
    enabled, custom, kinds, legacy_formats, reasons = set(), set(), set(), set(), {}
    sw_formats = {"RGB565" if color_depth == 16 else "ARGB8888", "ARGB8888", "A8"}
    compressed_fonts = large_fonts = complex_gradients = False
    font_count = image_count = 0
    keyboard_languages = set()
    trie_summaries = []
    def need(feature, reason):
        enabled.add(feature)
        reasons.setdefault("LV_USE_" + feature, set()).add(reason)
    def image_format(cf):
        if cf not in COLOR_FORMATS:
            raise ValueError(f"Image color format {cf} is unsupported by the ESP32 software renderer; reconvert to RGB565/ARGB8888/A8/indexed")
        name = COLOR_FORMATS[cf]
        if name not in ("RAW", "RAW_ALPHA"):
            if name in ("I1", "I2", "I4", "I8"):
                sw_formats.add("ARGB8888")
            elif name in ("A1", "A2", "A4"):
                sw_formats.add("A8")  # Native bin decoder expands packed alpha.
            else:
                sw_formats.add(name)
            if name == "RGB565A8": sw_formats.update(("RGB565", "A8"))
    def raw_image(data):
        decoded = base64.b64decode(data, validate=True)
        if decoded.startswith(b"\x89PNG"):
            need("LODEPNG", "RAW PNG image"); sw_formats.add("ARGB8888")
        elif decoded.startswith(b"\xff\xd8"):
            need("TJPGD", "RAW JPEG image"); sw_formats.add("RGB888")
        else: raise ValueError("RAW image requires a supported PNG/JPEG payload")
    def walk(value, path=""):
        nonlocal compressed_fonts, large_fonts, complex_gradients, font_count, image_count
        if isinstance(value, list):
            for index, child in enumerate(value): walk(child, f"{path}/{index}")
        elif isinstance(value, dict):
            record_path = bool(re.search(r"/obj/\d+$", path))
            widget_record = "base" in value and isinstance(value["base"], dict) and not path.endswith("/canvas") and (
                record_path or any(key in value for key in SECTIONS) or any(key in value for key in CUSTOM))
            if widget_record:
                sections = [key for key in SECTIONS if key in value]
                special = [key for key in CUSTOM if key in value]
                if len(sections) + len(special) > 1:
                    raise ValueError("Widget has multiple type sections at " + path)
                if sections:
                    kinds.add(sections[0]); need(SECTIONS[sections[0]], path or "widget")
                    if sections[0] == "scale" and value["scale"].get("variant") == "gauge": need("LINE", "scale gauge needle")
                    if sections[0] == "button" and value["button"].get("layout", 0): need("FLEX", "legacy button layout")
                    if sections[0] == "img" and value["img"].get("srcType") == 1 and "asset" not in value["img"]:
                        raise ValueError("Import/embed external image files in the editor before creating a self-contained ESP32 component")
                elif special:
                    kind = special[0]; kinds.add(kind); custom.add(CUSTOM[kind])
                    reasons.setdefault(CUSTOM[kind], set()).add(path or "widget")
                    for dependency in CUSTOM_DEPENDENCIES[kind]: need(dependency, kind)
                    if kind == "advancedKeyboard":
                        settings = value[kind]
                        language = settings.get("language", "en-US")
                        keyboard_languages.add(language)
                        if "rightToLeft" in settings and not isinstance(settings["rightToLeft"], bool):
                            raise ValueError("Advanced Keyboard rightToLeft must be a boolean")
                        if language in ("ar-SA", "ur-PK") or settings.get("rightToLeft", False):
                            need("BIDI", "Advanced Keyboard right-to-left input")
                elif any(key not in {"base", "oid", "par", "hostPath", "hostIndex", "contentChildren", "page"} for key in value):
                    raise ValueError("Unrecognized widget type section at " + path)
                else: kinds.add("object")
            image_source = "/imagebutton/" in path or "/nativeStyles/" in path or path.endswith("/encodedSource")
            if image_source and value.get("kind") == "text" and isinstance(value.get("source"), str) and value["source"] and ord(value["source"][0]) < 128:
                raise ValueError("Import/embed external image sources before creating a self-contained ESP32 component")
            if "data" in value and "cf" in value:
                cf = int(value["cf"])
                if value.get("formatVersion", 6) == 6:
                    cf = {4: 17, 5: 16, 6: 16, 7: 7, 8: 8, 9: 9, 10: 10, 11: 11, 12: 12, 13: 13, 14: 14}.get(cf, cf)
                image_format(cf)
                if cf in (1, 2): raw_image(value["data"])
            if "deviceFrames" in value:
                image_count += 1
                frames = value["deviceFrames"]
                if not isinstance(frames, list) or not frames:
                    raise ValueError("Device image has no frames")
                for frame in frames:
                    cf = int(frame.get("cf", -1)); image_format(cf)
                    if cf in (1, 2):
                        raw_image(frame["data"])
                    if frame.get("flags", 0) & 8:
                        data = base64.b64decode(frame["data"], validate=True)
                        if len(data) < 12: raise ValueError("Truncated compressed image header")
                        codec = struct.unpack("<I", data[:4])[0] & 15
                        if codec not in (1, 2): raise ValueError("Unsupported image compression codec")
                        need("RLE" if codec == 1 else "LZ4", "compressed device image")
            elif "sourceData" in value or "previewFrames" in value:
                image_count += 1; sw_formats.add("ARGB8888")
                if "previewFrames" in value: legacy_formats.add("PNG")
                else:
                    name = str(value.get("name", "")).lower()
                    extension = name.rsplit(".", 1)[-1]
                    if extension not in ("png", "jpg", "jpeg", "gif"):
                        raise ValueError("Legacy image needs PNG/JPEG/GIF embedded source data")
                    legacy_formats.add("JPEG" if extension in ("jpg", "jpeg") else extension.upper())
            if "dsc" in value and isinstance(value["dsc"], dict) and "bitmap" in value["dsc"]:
                font_count += 1; descriptor = value["dsc"]
                compressed_fonts |= bool(descriptor.get("bitmapFmt", 0))
                large_fonts |= len(descriptor.get("GlyphDsc", [])) > 511 or any(
                    glyph.get("box_w", 0) > 255 or glyph.get("box_h", 0) > 255 or glyph.get("adv_w", 0) > 4095 or
                    glyph.get("bitmapIdx", 0) > 1048575 for glyph in descriptor.get("GlyphDsc", []))
            if "nativeStyles" in value:
                for style in value["nativeStyles"]:
                    properties = style.get("values", {})
                    if properties.get("layout") == 1 or any(key.startswith("flex_") for key in properties): need("FLEX", "saved flex layout")
                    if properties.get("layout") == 2 or any(key.startswith("grid_") for key in properties): need("GRID", "saved grid layout")
                    gradient = properties.get("bg_grad")
                    if isinstance(gradient, dict) and gradient.get("dir", 0) >= 3: complex_gradients = True
                    for key in ("transform_scale_x", "transform_scale_y", "transform_rotation"):
                        if properties.get(key, 256 if "scale" in key else 0) != (256 if "scale" in key else 0): sw_formats.add("ARGB8888")
            if "canvas" in value and isinstance(value["canvas"], dict) and "base" in value and "obj" not in value:
                canvas = value["canvas"]
                if any(key in canvas for key in ("format", "cf", "header.cf")):
                    image_format(int(canvas.get("format", canvas.get("cf", canvas.get("header.cf")))))
            for key, child in value.items(): walk(child, path + "/" + key)
        elif isinstance(value, str):
            # Payload bytes, asset base64 and editor names are not rendered text.
            # Variable strings may later flow into a text binding.
            text_value = path.rsplit("/", 1)[-1] in ("text", "title", "caption", "placeholder", "options") or "/variables/" in path or ("/tabs/" in path and path.endswith("/name"))
            if text_value and "/qrcode/" not in path and "/barcode/" not in path:
                if any(0x590 <= ord(character) <= 0x8ff for character in value): need("BIDI", "RTL project text")
                if any(0x600 <= ord(character) <= 0x8ff for character in value): need("ARABIC_PERSIAN_CHARS", "Arabic/Persian project text")
    for source_document in documents:
        document = _runtime_document(source_document)
        if not isinstance(document, dict) or document.get("runtimeTarget", "9.6") != "9.6":
            raise ValueError("ESP32 component requires an LVGL 9.6 project; re-export with --target 9.6")
        if document.get("formatVersion", 1) not in (1, 2): raise ValueError("Unsupported project format version")
        if not re.fullmatch(r"9\.6(?:\.\d+)?", str(document.get("lvglVersion", "9.6.0"))):
            raise ValueError("ESP32 component requires LVGL 9.6 authoring data")
        screens = document.get("screens", [document])
        if not isinstance(screens, list) or not screens or any(not isinstance(screen.get("obj"), list) for screen in screens):
            raise ValueError("Project requires one or more screen obj arrays")
        trie_summary = trie_assets.prepare(copy.deepcopy(document), repository_root())
        trie_summaries.append(trie_summary)
        if trie_summary["referencedTries"]:
            custom.add(trie_assets.PREDICTION_GATE)
            reasons.setdefault(trie_assets.PREDICTION_GATE, set()).add("Advanced Keyboard trie selection")
        if trie_assets.BUILTIN_TRIE in trie_summary["referencedTries"]:
            custom.add(trie_assets.BUILTIN_GATE)
            reasons.setdefault(trie_assets.BUILTIN_GATE, set()).add("Built-in English trie selection")
        if trie_summary["trieNeedsBidi"]: need("BIDI", "Selected trie vocabulary")
        if trie_summary["trieNeedsArabic"]: need("ARABIC_PERSIAN_CHARS", "Selected trie vocabulary")
        walk(document)
    for feature in tuple(enabled):
        if feature in ("QRCODE", "BARCODE"): sw_formats.update(("ARGB8888", "I1"))
    changed = True
    while changed:
        old = set(enabled)
        for feature in tuple(enabled):
            for dependency in DEPENDENCIES.get(feature, set()): need(dependency, feature + " dependency")
        changed = old != enabled
    # Automatic Arabic label shaping changes textarea's logical text storage.
    # Keyboard projects retain Unicode input and use BiDi for visual direction.
    if "LVSERIAL_USE_ADVANCED_KEYBOARD" in custom:
        enabled.discard("ARABIC_PERSIAN_CHARS")
        reasons.pop("LV_USE_ARABIC_PERSIAN_CHARS", None)
    return {"schemaVersion": 1, "lvglVersion": "9.6.0", "runtimeTarget": "9.6", "colorDepth": color_depth,
            "defaultFont": default_font, "widgetSections": sorted(kinds), "enabledFeatures": sorted(enabled),
            "customFeatures": sorted(custom), "softwareFormats": sorted(sw_formats),
            "keyboardLanguages": sorted(keyboard_languages),
            "legacyImageFormats": sorted(legacy_formats), "fontCompressed": compressed_fonts,
            "fontLargeDescriptors": large_fonts, "complexGradients": complex_gradients,
            "fontAssetCount": font_count, "imageAssetCount": image_count,
            "trieAssetCount": sum(item["trieAssetCount"] for item in trie_summaries),
            "trieModelBytes": sum(item["trieModelBytes"] for item in trie_summaries),
            "trieIndexBytes": sum(item["trieIndexBytes"] for item in trie_summaries),
            "referencedTries": sorted({identity for item in trie_summaries for identity in item["referencedTries"]}),
            "featureReasons": {key: sorted(value) for key, value in sorted(reasons.items())}}


def lvgl_config(plan: dict, root: Path) -> str:
    internal = (root / "3rdParty/LVGL/lvgl/include/lvgl/config/lv_conf_internal.h").read_text(encoding="utf-8")
    boolean_names = set(re.findall(r"^\s*#define\s+(LV_USE_[A-Z0-9_]+)\s+[01](?:\s|$)", internal, re.M))
    # Enum-valued configuration settings are set explicitly below, never inferred as booleans.
    boolean_names -= {"LV_USE_OS", "LV_USE_STDLIB_MALLOC", "LV_USE_STDLIB_STRING", "LV_USE_STDLIB_SPRINTF", "LV_USE_DRAW_SW_ASM"}
    enabled = {"LV_USE_" + feature for feature in plan["enabledFeatures"]}
    enabled |= {"LV_USE_DRAW_SW", "LV_USE_THEME_DEFAULT", "LV_USE_FLOAT", "LV_USE_ASSERT", "LV_USE_ASSERT_MALLOC"}
    if "LZ4" in plan["enabledFeatures"]: enabled.add("LV_USE_LZ4_INTERNAL")
    if plan["fontCompressed"]: enabled.add("LV_USE_FONT_COMPRESSED")
    if plan["complexGradients"]: enabled.add("LV_USE_DRAW_SW_COMPLEX_GRADIENTS")
    boolean_names |= enabled
    definitions = {name: int(name in enabled) for name in boolean_names}
    definitions.update({"LV_USE_OS": "LV_OS_NONE", "LV_USE_STDLIB_MALLOC": "LV_STDLIB_CLIB",
                        "LV_USE_STDLIB_STRING": "LV_STDLIB_CLIB", "LV_USE_STDLIB_SPRINTF": "LV_STDLIB_CLIB",
                        "LV_COLOR_FORMAT_DEFAULT": "LV_COLOR_FORMAT_RGB565" if plan["colorDepth"] == 16 else "LV_COLOR_FORMAT_ARGB8888", "LV_DEF_REFR_PERIOD": 16,
                        "LV_DRAW_SW_DRAW_UNIT_CNT": 1, "LV_DRAW_SW_COMPLEX": 1,
                        "LV_USE_DRAW_SW_ASM": "LV_DRAW_SW_ASM_NONE", "LV_BIN_DECODER_RAM_LOAD": 1,
                        "LV_FONT_DEFAULT": "&lv_font_montserrat_" + str(plan["defaultFont"]),
                        "LV_FONT_FMT_TXT_LARGE": int(plan["fontLargeDescriptors"]),
                        "LV_THEME_DEFAULT_GROW": 0, "LV_THEME_DEFAULT_DARK": 0,
                        "LV_BUILD_EXAMPLES": 0, "LV_BUILD_DEMOS": 0,
                        "LV_CACHE_DEF_SIZE": 0, "LV_IMAGE_HEADER_CACHE_DEF_CNT": 0})
    fonts = set(re.findall(r"^\s*#define\s+(LV_FONT_(?:MONTSERRAT|UNSCII|DEJAVU|SOURCE_HAN)[A-Z0-9_]+)\s+[01](?:\s|$)", internal, re.M))
    for name in fonts: definitions[name] = int(name == "LV_FONT_MONTSERRAT_" + str(plan["defaultFont"]))
    definitions["LV_FONT_MONTSERRAT_" + str(plan["defaultFont"])] = 1
    languages = set(plan.get("keyboardLanguages", []))
    definitions["LV_FONT_SOURCE_HAN_SANS_SC_16_CJK"] = int("zh-CN" in languages)
    definitions["LV_FONT_DEJAVU_16_PERSIAN_HEBREW"] = int("ar-SA" in languages)
    definitions["LV_ADVANCED_KEYBOARD_USE_PINYIN"] = int("zh-CN" in languages)
    for language, name in (("hi-IN", "HINDI"), ("bn-BD", "BENGALI"), ("ur-PK", "URDU"), ("ru-RU", "CYRILLIC"), ("el-GR", "GREEK")):
        definitions["LV_ADVANCED_KEYBOARD_USE_" + name + "_FONT"] = int(language in languages)
    formats = set(re.findall(r"#ifndef\s+(LV_DRAW_SW_SUPPORT_[A-Z0-9_]+)", internal))
    for name in formats:
        definitions[name] = int(name.removeprefix("LV_DRAW_SW_SUPPORT_") in plan["softwareFormats"])
    for macro in set(CUSTOM.values()) | {trie_assets.PREDICTION_GATE, trie_assets.BUILTIN_GATE}:
        definitions[macro] = int(macro in plan["customFeatures"])
    definitions["LVSERIAL_MINIMAL_RUNTIME"] = 1
    definitions["LVSERIAL_ENABLE_LEGACY_IMAGE_DECODERS"] = int(bool(plan["legacyImageFormats"]))
    for kind in ("PNG", "JPEG", "GIF"):
        definitions["LVSERIAL_ENABLE_LEGACY_" + kind] = int(kind in plan["legacyImageFormats"])
    lines = ["/* Generated from the bundled project(s). Re-run make_component.py after changing them. */", "#ifndef LV_CONF_H", "#define LV_CONF_H"]
    lines += [f"#define {name} {value}" for name, value in sorted(definitions.items())]
    return "\n".join(lines + ["#endif", ""])


def source_plan(plan: dict, root: Path) -> list[str]:
    """List actual translation units; unused widget/backend files are not compiled."""
    lvgl = root / "3rdParty/LVGL/lvgl"
    selected = set()
    def add(pattern):
        for source in lvgl.glob(pattern):
            if source.is_file(): selected.add(source.relative_to(root).as_posix())
    add("src/*.c")
    for directory in ("core", "display", "indev", "misc", "logging", "tick"):
        # SVG is a separate backend and is never needed for converted device images.
        for source in (lvgl / "src" / directory).rglob("*.c"):
            if source.name == "lv_bidi.c" and "BIDI" not in plan["enabledFeatures"]: continue
            if source.name == "lv_text_ap.c" and "ARABIC_PERSIAN_CHARS" not in plan["enabledFeatures"]: continue
            if source.name in ("lv_matrix.c", "vg_lite_matrix.c", "lv_templ.c"): continue
            selected.add(source.relative_to(root).as_posix())
    for pattern in ("src/draw/*.c", "src/draw/convert/*.c", "src/draw/sw/*.c", "src/draw/sw/blend/*.c",
                    "src/stdlib/lv_mem.c", "src/stdlib/clib/*.c", "src/osal/lv_os.c", "src/osal/lv_os_none.c",
                    "src/fs/lv_fs.c", "src/layouts/lv_layout.c", "src/font/lv_font.c", "src/font/fmt_txt/*.c",
                    "src/themes/lv_theme.c", "src/themes/default/*.c", "src/image/lv_bin_decoder.c", "src/image/lv_image_decoder.c"):
        add(pattern)
    add(f"src/font/lv_font_montserrat_{plan['defaultFont']}.c")
    languages = set(plan.get("keyboardLanguages", []))
    if "zh-CN" in languages: add("src/font/lv_font_source_han_sans_sc_16_cjk.c")
    if "ar-SA" in languages: add("src/font/lv_font_dejavu_16_persian_hebrew.c")
    for language, name in (("hi-IN", "Hindi"), ("bn-BD", "Bengali"), ("ur-PK", "Urdu"), ("ru-RU", "Cyrillic"), ("el-GR", "Greek")):
        if language in languages:
            selected.add("LVSerial/Widgets/AdvancedKeyboard/Keyboard" + name + "Font.c")
    enabled = set(plan["enabledFeatures"])
    for feature, directory in WIDGET_DIRS.items():
        if feature in enabled:
            for source in (lvgl / "src/widgets" / directory).glob("*.c"):
                if source.name == "lv_calendar_header_dropdown.c" or source.name == "lv_calendar_chinese.c": continue
                selected.add(source.relative_to(root).as_posix())
    for feature in ("FLEX", "GRID"):
        if feature in enabled: add("src/layouts/" + feature.lower() + "/*.c")
    for feature in ("QRCODE", "BARCODE", "RLE", "LZ4", "LODEPNG", "TJPGD"):
        if feature in enabled: add("src/libs/" + feature.lower() + "/*.c")
    if "LODEPNG" in enabled: add("src/image/lv_lodepng.c")
    if "TJPGD" in enabled: add("src/image/lv_tjpgd.c")
    serializer_widget = {"LVArc.cpp": "ARC", "LVBar.cpp": "BAR", "LVButton.cpp": "BUTTON", "LVButtonMatrix.cpp": "BUTTONMATRIX",
                         "LVCalendar.cpp": "CALENDAR", "LVCanvas.cpp": "CANVAS", "LVCheckBox.cpp": "CHECKBOX", "LVImage.cpp": "IMAGE", "LVLabel.cpp": "LABEL"}
    for source in (root / "LVSerial/Serialization").glob("*.cpp"):
        # XML is an optional input codec, outside a Protobuf-only source closure.
        # The generator bundles it separately for an explicit build-time opt-in.
        if source.name == "Xml.cpp": continue
        if source.name in ("lodepng.cpp", "PNGToArray.cpp"): continue
        if source.name in serializer_widget and serializer_widget[source.name] not in enabled: continue
        selected.add(source.relative_to(root).as_posix())
    for gate, sources in CUSTOM_SOURCES.items():
        if gate in plan["customFeatures"]:
            for source in sources:
                if not (root / source).is_file():
                    raise ValueError("Missing required custom widget source: " + source)
                selected.add(source)
    for name in ("LVSerial/Device/lvserial_pb.c", "LVSerial/Device/lvserial_defaults.c", "LVSerial/Proto/lvserial.pb.c",
                 "3rdParty/nanopb/pb_common.c", "3rdParty/nanopb/pb_decode.c", "3rdParty/nanopb/pb_encode.c",
                 "Runtime/common/Runtime.cpp", "Runtime/common/Validation.cpp"):
        selected.add(name)
    return sorted(selected)
