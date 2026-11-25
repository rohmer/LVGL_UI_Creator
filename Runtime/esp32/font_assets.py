"""Dependency-free codec for lvserial.proto's portable FontData resource.

Used by the offline SDK generators; no font renderer or source TTF is needed.
Unknown JSON extensions retain the legacy property encoding without data loss.
"""
import base64
import json


def prune_unused(document):
    """Keep fonts referenced by concrete screens, never authoring libraries.

    Like ProjectFonts::PruneUnused, this operates on an export copy and makes
    main fontId bindings authoritative before considering inline snapshots.
    """
    catalog = document.get("fonts", [])
    if not isinstance(catalog, list):
        raise ValueError("Project fonts must be an array")
    by_id, by_data, names, used = {}, {}, set(), set()
    signature = lambda value: json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    for entry in catalog:
        if not isinstance(entry, dict):
            raise ValueError("Invalid project font")
        identity, name, font = entry.get("id"), entry.get("name"), entry.get("font")
        if (not isinstance(identity, str) or not identity or "\0" in identity or identity in by_id or
                not isinstance(name, str) or not name or any(c in name for c in "\0\r\n") or name in names):
            raise ValueError("Font IDs and names must be unique and nonempty")
        if (not isinstance(font, dict) or not isinstance(font.get("dsc"), dict) or
                not isinstance(font["dsc"].get("bitmap"), str) or type(font.get("lineHeight")) is not int or font["lineHeight"] <= 0):
            raise ValueError("Project font has no bitmap or valid metrics")
        by_id[identity] = font
        by_data[signature(font)] = identity
        names.add(name)

    def require(identity):
        if not isinstance(identity, str) or identity not in by_id:
            raise ValueError("Unknown project font: " + str(identity))
        used.add(identity)

    def visit(value):
        if isinstance(value, list):
            for child in value: visit(child)
            return
        if not isinstance(value, dict): return
        if "fontId" in value:
            identity = value["fontId"]
            require(identity)
            styles = value.setdefault("nativeStyles", [])
            if not isinstance(styles, list): raise ValueError("Font styles must be an array")
            main = next((style for style in styles if style.get("selector", 0) == 0), None)
            if main is None:
                main = {"selector": 0, "values": {}}
                styles.append(main)
            properties = main.setdefault("values", {})
            if not isinstance(properties, dict): raise ValueError("Font style values must be an object")
            properties["text_font"] = {"projectFont": identity}
        if set(value) == {"projectFont"}:
            require(value["projectFont"])
            return
        for key, child in list(value.items()):
            if key in ("design", "fonts", "components", "componentLibrary", "sharedStyles"): continue
            if key in ("text_font", "font") and isinstance(child, dict):
                if set(child) == {"projectFont"}: require(child["projectFont"])
                else:
                    identity = by_data.get(signature(child))
                    if identity is not None:
                        used.add(identity)
                        value[key] = {"projectFont": identity}
                continue
            visit(child)

    pages = document.get("screens", [document])
    if not isinstance(pages, list): raise ValueError("Screens must be an array")
    for page in pages:
        if not isinstance(page, dict): continue
        for key in ("canvas", "screen", "obj"):
            if key in page: visit(page[key])
        if "canvas" not in page and "screen" not in page and "base" in page: visit(page["base"])
    if "fonts" in document:
        document["fonts"] = [entry for entry in catalog if entry["id"] in used]
    return document

# Field number, JSON key, scalar/table encoding. Required fields are checked
# separately; repeated tables always decode to arrays, including empty tables.
GLYPH = {1: ("bitmapIdx", "u"), 2: ("adv_w", "u"), 3: ("box_w", "u"),
         4: ("box_h", "u"), 5: ("ofs_x", "s"), 6: ("ofs_y", "s")}
CMAP = {1: ("rangeStart", "u"), 2: ("rangeLength", "u"), 3: ("glyphIDstart", "u"),
        4: ("type", "u"), 5: ("formatVersion", "u"), 6: ("unicodeList", "p"), 7: ("glyphIDOffsetList", "p")}
KERN = {1: ("glyphIDSize", "u"), 2: ("GlyphIDs", "p"), 3: ("Values", "i8"),
        4: ("leftCount", "u"), 5: ("rightCount", "u"), 6: ("leftClasses", "u8"), 7: ("rightClasses", "u8")}
FONT = {1: ("format_version", "u"), 2: ("baseLine", "s"), 3: ("lineHeight", "u"),
        4: ("capHeight", "u"), 5: ("xHeight", "u"), 6: ("subpx", "u"), 7: ("kerning", "u"),
        8: ("underlinePosition", "s"), 9: ("underlineThickness", "u"), 10: ("bitmap", "b64"),
        11: ("bitmapFmt", "u"), 12: ("bpp", "u"), 13: ("stride", "u"), 14: ("kernScale", "u"),
        15: ("kernClasses", "u"), 16: ("GlyphDsc", GLYPH), 17: ("cmaps", CMAP),
        18: ("Kern", KERN), 19: ("kern_is_null", "u"), 20: ("cmapNum", "u")}
DSC_KEYS = {"bitmap", "bitmapFmt", "bpp", "stride", "kernScale", "kernClasses", "GlyphDsc", "cmaps", "Kern", "cmapNum"}


def _keys(value, schema):
    return isinstance(value, dict) and set(value) <= {key for key, _ in schema.values()}


def matches(value):
    if not isinstance(value, dict) or "dsc" not in value:
        return False
    outer = {key for key, _ in FONT.values()} - DSC_KEYS - {"format_version", "kern_is_null"}
    dsc = value["dsc"]
    if not set(value) <= outer | {"dsc"} or "lineHeight" not in value or not isinstance(dsc, dict) or not set(dsc) <= DSC_KEYS:
        return False
    if not {"bitmap", "bitmapFmt", "bpp", "GlyphDsc", "cmaps"} <= set(dsc) or not isinstance(dsc["bitmap"], str):
        return False
    try:
        if base64.b64encode(base64.b64decode(dsc["bitmap"], validate=True)).decode("ascii") != dsc["bitmap"]:
            return False
    except ValueError:
        return False
    if not isinstance(dsc["GlyphDsc"], list) or not isinstance(dsc["cmaps"], list):
        return False
    if len(dsc["GlyphDsc"]) > 65536 or len(dsc["cmaps"]) > 511:
        return False
    if any(not _keys(g, GLYPH) or len(g) != 6 for g in dsc["GlyphDsc"]):
        return False
    required_map = {"rangeStart", "rangeLength", "glyphIDstart", "type", "unicodeList", "glyphIDOffsetList"}
    if any(not _keys(m, CMAP) or not required_map <= set(m) for m in dsc["cmaps"]):
        return False
    kern = dsc.get("Kern")
    return kern is None or (_keys(kern, KERN) and "Values" in kern and (("GlyphIDs" in kern) == ("glyphIDSize" in kern)) and
                           ({"glyphIDSize", "GlyphIDs"} <= set(kern) or
                            {"leftCount", "rightCount", "leftClasses", "rightClasses"} <= set(kern)))


def _integer(value, signed=False):
    if type(value) is not int or not (-(1 << 31) if signed else 0) <= value <= ((1 << 31) - 1 if signed else (1 << 32) - 1):
        raise ValueError("Font metric exceeds its 32-bit field")
    return (value << 1) ^ (value >> 31) if signed else value


def _encode(value, schema, wire, varint):
    result = bytearray()
    for tag, (key, kind) in schema.items():
        if key not in value:
            continue
        item = value[key]
        if isinstance(kind, dict):
            for child in (item if tag in (16, 17) and schema is FONT else [item]):
                result += wire(tag, 2, _encode(child, kind, wire, varint))
        elif kind in ("u", "s"):
            result += wire(tag, 0, _integer(item, kind == "s"))
        elif kind == "p":
            if item:
                result += wire(tag, 2, b"".join(varint(_integer(n)) for n in item))
        else:
            if kind == "b64":
                raw = base64.b64decode(item, validate=True)
            elif kind in ("i8", "u8"):
                if any(type(n) is not int or not (-128 if kind == "i8" else 0) <= n <= (127 if kind == "i8" else 255) for n in item):
                    raise ValueError("Font byte table value is out of range")
                raw = bytes(n & 255 for n in item)
            else:
                raise ValueError("Unknown font field encoding")
            result += wire(tag, 2, raw)
    return bytes(result)


def encode(font, wire, varint):
    flat = {key: value for key, value in font.items() if key != "dsc"}
    flat.update(font["dsc"])
    flat["format_version"] = 1
    if "Kern" in flat and flat["Kern"] is None:
        flat.pop("Kern")
        flat["kern_is_null"] = 1
    return _encode(flat, FONT, wire, varint)


def _decode(data, schema, fields, varint):
    result, seen = {}, set()
    for tag, wire, value in fields(data):
        if tag not in schema:
            continue
        key, kind = schema[tag]
        repeated = kind == "p" or (schema is FONT and tag in (16, 17))
        if key in seen and not repeated:
            raise ValueError("Duplicate font field")
        seen.add(key)
        if kind in ("u", "s"):
            if wire != 0 or value > (1 << 32) - 1:
                raise ValueError("Invalid font metric")
            result[key] = (value >> 1) ^ -(value & 1) if kind == "s" else value
        elif kind == "p":
            numbers = result.setdefault(key, [])
            if wire == 0:
                numbers.append(_integer(value))
            elif wire == 2:
                offset = 0
                while offset < len(value):
                    number, offset = varint(value, offset)
                    numbers.append(_integer(number))
            else:
                raise ValueError("Invalid packed font table")
        else:
            if wire != 2:
                raise ValueError("Invalid font table wire type")
            if isinstance(kind, dict):
                child = _decode(value, kind, fields, varint)
                if repeated:
                    result.setdefault(key, []).append(child)
                else:
                    result[key] = child
            elif kind == "b64":
                result[key] = base64.b64encode(value).decode("ascii")
            else:
                result[key] = [n - 256 if kind == "i8" and n >= 128 else n for n in value]
    required = ({"bitmapIdx", "adv_w", "box_w", "box_h", "ofs_x", "ofs_y"} if schema is GLYPH else
                {"rangeStart", "rangeLength", "glyphIDstart", "type"} if schema is CMAP else
                {"Values"} if schema is KERN else {"format_version", "lineHeight", "bitmap", "bitmapFmt", "bpp"})
    if not required <= set(result):
        raise ValueError("Missing required font field")
    if schema is CMAP:
        result.setdefault("unicodeList", [])
        result.setdefault("glyphIDOffsetList", [])
    if schema is KERN and "glyphIDSize" in result:
        result.setdefault("GlyphIDs", [])
    if schema is FONT:
        result.setdefault("GlyphDsc", [])
        result.setdefault("cmaps", [])
    return result


def decode(data, fields, varint):
    flat = _decode(data, FONT, fields, varint)
    if flat.pop("format_version") != 1:
        raise ValueError("Unsupported font data version")
    if "kern_is_null" in flat:
        if flat.pop("kern_is_null") != 1 or "Kern" in flat:
            raise ValueError("Invalid font kerning null marker")
        flat["Kern"] = None
    font = {key: value for key, value in flat.items() if key not in DSC_KEYS}
    font["dsc"] = {key: value for key, value in flat.items() if key in DSC_KEYS}
    return font
