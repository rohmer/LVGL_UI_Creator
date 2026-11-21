#!/usr/bin/env python3
"""Create the Linux callback integration fixture, including binary trie/index assets."""
from pathlib import Path
import json
import sys

root, output = Path(sys.argv[1]), Path(sys.argv[2])
sys.path.insert(0, str(root / "Runtime/esp32"))
from feature_plan import encode_project

models = root / "LVSerial/Widgets/AdvancedKeyboard/models"
document = {
    "formatVersion": 2, "lvglVersion": "9.6.0", "runtimeTarget": "9.6",
    "canvas": {"width": 640, "height": 400},
    "obj": [
        {"oid": 7, "par": -1, "base": {"name": "Mobile keyboard", "coords": {"x": 10, "y": 120, "width": 600, "height": 240}},
         "advancedKeyboard": {"targetOid": -1, "autoFocus": True, "hideOnDefocus": False,
             "trieId": "linux-test", "autocorrect": True, "autocorrectThreshold": 0.3}},
        {"oid": 8, "par": -1, "base": {"name": "Message", "coords": {"x": 10, "y": 10, "width": 600, "height": 70}},
         "textarea": {"text": "Hello", "placeholder": "Type a message", "maxLength": 64}}],
    "variables": [{"var": {"name": "message", "dataType": 0, "default": "Hello", "history": 4,
                               "oid": 8, "property": "/textarea/text"}}],
    "tries": [{"id": "linux-test", "name": "Linux integration model", "format": "ngtr",
               "data": (models / "enUS16.ngtr").read_bytes(), "indexData": (models / "enUS16.ngix").read_bytes()}]
}
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes(encode_project(document))
print(output)
