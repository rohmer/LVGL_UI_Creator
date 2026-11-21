"""Validate/deduplicate project trie catalogs and produce flash-ready payloads."""
from __future__ import annotations

import base64
from array import array
import importlib.util
from pathlib import Path

BUILTIN_TRIE = "builtin-en-us"
PREDICTION_GATE = "LVSERIAL_USE_ADVANCED_KEYBOARD_PREDICTION"
BUILTIN_GATE = "LVSERIAL_USE_ADVANCED_KEYBOARD_BUILTIN_TRIE"
MAX_ASSET = 16 * 1024 * 1024
LANGUAGES = frozenset(("en-US", "es-ES", "fr-FR", "de-DE", "it-IT", "pt-PT"))


def selected_trie(settings: dict) -> str:
    """Match the runtime's per-language defaults without inventing dictionaries."""
    language = settings.get("language", "en-US")
    if not isinstance(language, str) or language not in LANGUAGES:
        raise ValueError("Unsupported Advanced Keyboard language")
    identity = settings.get("trieId", BUILTIN_TRIE if language == "en-US" else "")
    if not isinstance(identity, str) or "\0" in identity:
        raise ValueError("Advanced Keyboard trieId must be a string without NUL bytes")
    return identity


def text_features(model: bytes) -> tuple[bool, bool]:
    """Inspect Unicode along validated BFS paths, not arbitrary payload bytes."""
    count = int.from_bytes(model[8:12], "little")
    states = array("I", [0]) * count
    offset, next_child, bidi, arabic = 16, 2, False, False
    def integer(position):
        value = shift = 0
        while True:
            byte = model[position];position += 1
            value |= (byte & 127) << shift
            if byte < 128: return value, position
            shift += 7
    for node in range(count):
        label = model[offset];offset += 1
        flags, offset = integer(offset)
        if flags & 1: _, offset = integer(offset)
        remaining, scalar = states[node] & 3, states[node] >> 2
        if label < 128: remaining, scalar = 0, label
        elif remaining and 0x80 <= label < 0xc0:
            scalar = (scalar << 6) | (label & 63);remaining -= 1
        elif 0xc2 <= label < 0xe0: remaining, scalar = 1, label & 31
        elif 0xe0 <= label < 0xf0: remaining, scalar = 2, label & 15
        elif 0xf0 <= label < 0xf5: remaining, scalar = 3, label & 7
        else: remaining, scalar = 0, 0
        if not remaining:
            bidi |= 0x590 <= scalar <= 0x8ff
            arabic |= 0x600 <= scalar <= 0x8ff
            if bidi and arabic: return True, True
        degree = flags >> 1
        state = (scalar << 2) | remaining
        for child in range(next_child, next_child + degree): states[child] = state
        next_child += degree
    return bidi, arabic


def references(document: dict) -> set[str]:
    result = set()
    def walk(value):
        if isinstance(value, list):
            for child in value: walk(child)
        elif isinstance(value, dict):
            if "base" in value and isinstance(value.get("advancedKeyboard"), dict):
                identity = selected_trie(value["advancedKeyboard"])
                if identity: result.add(identity)
            for key, child in value.items():
                if key != "tries": walk(child)
    walk(document)
    return result


def payload(value) -> bytes:
    if isinstance(value, bytes): result = value
    elif isinstance(value, str):
        if len(value) > ((MAX_ASSET + 2) // 3) * 4: raise ValueError("Trie asset exceeds 16 MiB")
        result = base64.b64decode(value, validate=True)
        if base64.b64encode(result).decode("ascii") != value:
            raise ValueError("Trie assets require canonical base64")
    else: raise ValueError("Trie assets require embedded binary or base64 data")
    if not result or len(result) > MAX_ASSET: raise ValueError("Trie asset is empty or exceeds 16 MiB")
    return result


def index_builder(root: Path):
    path = root / "LVSerial/Widgets/AdvancedKeyboard/layout/build_ngtr_index.py"
    specification = importlib.util.spec_from_file_location("lvgl_pb_ngtr_index", path)
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module.build_index


def prepare(document: dict, root: Path, *, prune=False) -> dict:
    """Mutate only the catalog: true binary fields, checked deterministic NGIX."""
    needed = references(document)
    records = document.get("tries", [])
    if not isinstance(records, list) or len(records) > 256:
        raise ValueError("Project tries must be an array of at most 256 models")
    ids, normalized, bidi, arabic = set(), [], False, False
    build_index = None
    for item in records:
        if not isinstance(item, dict): raise ValueError("Invalid project trie record")
        identity, name = item.get("id"), item.get("name")
        if (not isinstance(identity, str) or not identity or len(identity.encode()) > 128 or identity == BUILTIN_TRIE or
                any(character in identity for character in "\0\r\n") or identity in ids or
                not isinstance(name, str) or not name or len(name.encode()) > 256 or
                any(character in name for character in "\0\r\n") or item.get("format", "ngtr") != "ngtr"):
            raise ValueError("Trie IDs must be unique and distinct from the reserved built-in model")
        ids.add(identity)
        model = payload(item.get("data"))
        if build_index is None: build_index = index_builder(root)
        expected_index = bytes(build_index(model))
        if "indexData" in item and payload(item["indexData"]) != expected_index:
            raise ValueError("Trie index does not match the embedded NGTR model")
        if identity in needed:
            needs_bidi, needs_arabic = text_features(model)
            bidi |= needs_bidi;arabic |= needs_arabic
        if not prune or identity in needed:
            normalized.append({"id": identity, "name": name, "format": "ngtr", "data": model, "indexData": expected_index})
    missing = needed - ids - {BUILTIN_TRIE}
    if missing: raise ValueError("Unknown project trie: " + ", ".join(sorted(missing)))
    if normalized: document["tries"] = normalized
    else: document.pop("tries", None)
    return {"referencedTries": sorted(needed), "trieAssetCount": len(normalized),
            "trieModelBytes": sum(len(record["data"]) for record in normalized),
            "trieIndexBytes": sum(len(record["indexData"]) for record in normalized),
            "trieNeedsBidi": bidi, "trieNeedsArabic": arabic}
